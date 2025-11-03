/// ========================================================================================================================================
// Module       : src/bb_channel.c
// Description  : Userspace unreliable channel with loss/dup/reorder/latency/rate controls + observable socket I/O logs
// Topics       : Network Emulation, POSIX Sockets, Deterministic Testing, Timing, Systems Safety
// Project      : Byte Bistro (Copyright 2025)
// Purpose      : Provides a controllable impairment layer beneath transports (GBN/SR). It enqueues outgoing datagrams,
//                applies jittered delays, optional drop/dup/reorder, and rate limiting before sendto(). Receives via select().
// Author       : Rizky Johan Saputra (Independent Project)
// Note         : Designed to be payload-opaque and thread-friendly when used from a single I/O thread. Logging to stderr
//                documents actual SEND/RECV events for reproducible experiments and debugging.
/// ========================================================================================================================================

// ======================================================================================================
// SPECIFICATIONS
// ======================================================================================================
// - Impairments:
//     - Loss   : Probabilistic silent drop at enqueue time (logical success to upper layer).
//     - Dup    : Optional second enqueue with slight offset (~1ms).
//     - Reorder: Adjacent swap on head pair to simulate mild reordering.
//     - Delay  : Base + jitter applied per enqueued frame before eligibility to send.
//     - Rate   : Token-bucket in time domain (ns/byte) to space out actual sendto() calls.
// - Waiting Policy (send path):
//     - Bounded sleep chunks (<=5ms) until first head frame is ready, with a hard cap (~150ms).
//     - Prevents indefinite stalls if no other activity wakes the loop.
// - Receive Path:
//     - select() with caller-specified timeout, logs source and size on success.
// - Safety:
//     - No payload mutation and heap copies exist only to delay/reorder safely.
//     - All allocations checked implicitly by use whilst queued frames fully freed in destroy.
// - Time Base:
//     - Uses bb_now_ns() from bb_common.h for monotonic-ish timestamps (implementation-defined).

// ======================================================================================================
// SETUP (ADJUST IF NECESSARY)
// ======================================================================================================
#include "bb_channel.h"
#include <math.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <arpa/inet.h> 

// ======================================================================================================
// IMPLEMENTATIONS
// ======================================================================================================
// Transport the configurations (Length, Buffer, Signals)
typedef struct bb_frame {
    size_t n;                 // Length
    uint8_t *buf;             // Heap copy (so we can delay/reorder)
    uint64_t ready_ts;        // When it becomes available for send/recv
    struct bb_frame* next;
} bb_frame_t;

// Declare the channel instances
struct bb_channel_s {
    int sockfd;
    struct sockaddr_in peer;
    bb_channel_cfg_t cfg;
    uint64_t now;
    uint64_t token_ns_per_byte;     // For rate limiting
    uint64_t next_tx_ts;            // Next time when tokens allow send
    bb_frame_t *q_head, *q_tail;    // Reorder queue for outgoing frames
    uint64_t rng;                   // RNG state (xorshift64*)
};

// Declare the necessary RNG utilities
static uint64_t xorshift64(uint64_t* s){ uint64_t x=*s; x^=x<<13; x^=x>>7; x^=x<<17; return *s=x; }
static double frand01(uint64_t* s){ return (xorshift64(s) >> 11) * (1.0/9007199254740992.0); }

// Decalre the time helpers
static uint64_t ms2ns(double ms){ return (uint64_t)(ms*1e6); }

// Declare a channel instance bound to 'sockfd' and initial 'peer' with its rate, jitter and RNG
bb_channel_t* bb_channel_create(int sockfd, const struct sockaddr_in* peer, bb_channel_cfg_t cfg){
    bb_channel_t* ch = calloc(1,sizeof(*ch));
    ch->sockfd = sockfd; ch->peer = *peer; ch->cfg = cfg; ch->rng = cfg.seed?cfg.seed:0xC0FFEE1234ull;
    ch->token_ns_per_byte = (cfg.rate_mbps>0)? (uint64_t)((8.0/cfg.rate_mbps)*1e3) : 0; // ns per byte
    ch->next_tx_ts = 0; ch->now = 0;
    return ch;
}

// Free all the queued frames and destroy its channels
void bb_channel_destroy(bb_channel_t* ch){
    for(bb_frame_t* p=ch->q_head;p;){ bb_frame_t* n=p->next; free(p->buf); free(p); p=n; }
    free(ch);
}

// Compute the jittered one-way delay
static uint64_t jittered_delay_ns(bb_channel_t* ch){
    double base = ch->cfg.delay_mean_ms;
    double jitter = (frand01(&ch->rng)*2.0 - 1.0) * ch->cfg.delay_jitter_ms; // ±jitter
    double ms = fmax(0.0, base + jitter);
    return ms2ns(ms);
}

// Compute the Bernoulli decision as a percentage
static bool decide(double pct, uint64_t* rng){ return frand01(rng) < (pct/100.0); }

// Enqueue a heap-backed frame with a given ready timestamp
static void enqueue_out(bb_channel_t* ch, const uint8_t* buf, size_t n, uint64_t ready_ts){
    bb_frame_t* f = malloc(sizeof(*f));
    f->buf = malloc(n); memcpy(f->buf, buf, n); f->n = n; f->ready_ts = ready_ts; f->next = NULL;
    if(ch->q_tail) ch->q_tail->next = f; else ch->q_head = f; ch->q_tail = f;
}

// Declare an adjacent swap to simulate simple reordering
static void maybe_reorder(bb_channel_t* ch){
    if(!ch->q_head || !ch->q_head->next) return;
    if(!decide(ch->cfg.reorder_pct, &ch->rng)) return;
    // Swap head with next to simulate simple reordering
    bb_frame_t* a = ch->q_head; bb_frame_t* b = a->next; a->next = b->next; b->next = a; ch->q_head = b; if(ch->q_tail==b) ch->q_tail=a;
}

// Enqueue and drain the ready frames to recall its delay rate
ssize_t bb_channel_send(bb_channel_t* ch, const void* buf, size_t n){
    ch->now = bb_now_ns();
    // Simulated drop
    if (decide(ch->cfg.loss_pct, &ch->rng)) {
         // Logical success even if dropped
        return (ssize_t)n;
    }

    // Schedule the frames with a jittered delay
    uint64_t ready = ch->now + jittered_delay_ns(ch);
    enqueue_out(ch, buf, n, ready);
    if (decide(ch->cfg.dup_pct, &ch->rng)) {
        enqueue_out(ch, buf, n, ready + ms2ns(1.0));
    }
    maybe_reorder(ch);

    // Declare a robust waiting time until the head is ready
    const uint64_t ABSOLUTE_CAP_NS = ms2ns(150.0); // hard cap ~150ms
    uint64_t waited = 0;

    for (;;) {
        bb_frame_t* f = ch->q_head;
        if (!f) break;
        ch->now = bb_now_ns();
        // Ready to drain now
        if (f->ready_ts <= ch->now) break;
        uint64_t remain = f->ready_ts - ch->now;
        // Ensure the blockage is not too long
        if (waited >= ABSOLUTE_CAP_NS) break;
        // Partition sleep in small chunks
        if (remain > ms2ns(5.0)) remain = ms2ns(5.0);
        struct timespec ts = { .tv_sec = 0, .tv_nsec = (long)remain };
        nanosleep(&ts, NULL);
        waited += remain;
    }

    // Drain the ready frames to recall its rate limits
    ssize_t sent_total = 0;
    for (;;) {
        bb_frame_t* f = ch->q_head;
        if (!f) break;
        ch->now = bb_now_ns();
        // Recall that the preceeding state is not ready
        if (f->ready_ts > ch->now) break;
        if (ch->token_ns_per_byte) {
            if (ch->now < ch->next_tx_ts) {
                // Prioritize its token buckets
                break;
            }
            ch->next_tx_ts = ch->now + ch->token_ns_per_byte * f->n;
        }
        ssize_t s = sendto(ch->sockfd, f->buf, f->n, 0,
                           (struct sockaddr*)&ch->peer, sizeof(ch->peer));
        if (s < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            return -1;
        }
        // Declare the log send
        fprintf(stderr, "[CHAN SEND] %zd bytes -> %s:%d\n",
                (ssize_t)f->n,
                inet_ntoa(ch->peer.sin_addr),
                ntohs(ch->peer.sin_port));

        sent_total += (ssize_t)f->n;
        ch->q_head = f->next;
        if (!ch->q_head) ch->q_tail = NULL;
        free(f->buf); free(f);
    }

    // Report the past success if nothing is flushed and still queued
    return sent_total > 0 ? sent_total : (ssize_t)n;
}

// Blocking the receive with select()-based timeout and observable logging
ssize_t bb_channel_recv(bb_channel_t* ch, void* buf, size_t cap, int timeout_ms){
    struct timeval tv={ .tv_sec=timeout_ms/1000, .tv_usec=(timeout_ms%1000)*1000 };
    fd_set rf; FD_ZERO(&rf); FD_SET(ch->sockfd,&rf);
    int r = select(ch->sockfd+1,&rf,NULL,NULL,&tv);
    // Declare a 0 timeout or -1 err
    if(r<=0) return r;
    struct sockaddr_in from; socklen_t slen = sizeof(from);
    ssize_t n = recvfrom(ch->sockfd, buf, cap, 0, (struct sockaddr*)&from, &slen);
    if(n > 0){
        // Track the latest peer
        ch->peer = from;

        // Declare the log recv
        fprintf(stderr, "[CHAN RECV] %zd bytes <- %s:%d\n",
                n,
                inet_ntoa(from.sin_addr),
                ntohs(from.sin_port));
    }
    return n;
}

/// ========================================================================================================================================
// END (ADD IMPLEMENTATIONS IF NECESSARY)
/// ========================================================================================================================================