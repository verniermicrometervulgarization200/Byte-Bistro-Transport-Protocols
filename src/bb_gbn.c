/// ========================================================================================================================================
// Module       : src/bb_gbn.c
// Description  : Go-Back-N (GBN) reliable transport over Byte-Bistro channel (nonblocking send, liveness in recv)
// Topics       : Sliding Window, ARQ, Timers, Cumulative ACKs, Systems Reliability
// Project      : Byte Bistro (Copyright 2025)
// Purpose      : Implements a GBN sender/receiver with a fixed window, single RTO timer, batch retransmit on timeout,
//                and cumulative ACK advancement. Send path is nonblocking; progress/liveness are driven from recv().
// Author       : Rizky Johan Saputra (Independent Project)
// Note         : This backend depends on bb_wire.* for header pack/parse and bb_channel.* for lossy delivery.
//                It conforms to the bb_proto.h interface so callers remain transport-agnostic.
/// ========================================================================================================================================

// ======================================================================================================
// SPECIFICATIONS
// ======================================================================================================
// - Window & Sequence:
//     - Sender window size = cfg.wnd; outstanding = snd_nxt - snd_una (mod 2^32).
//     - Sequence arithmetic uses uint32_t wrap semantics.
// - ACK Policy:
//     - Cumulative ACK (GBN). On ack>=snd_una and ack<=snd_nxt: slide window and stop timer if window empty.
// - Timer / Retransmit:
//     - Single base timer (cfg.rto_ms). On expiry: retransmit all frames in [snd_una .. snd_nxt-1] from snapshot.
// - Fragmentation:
//     - send() fragments payload into chunks of size<=cfg.mss and sends while window has space; nonblocking.
//     - recv() delivers only in-order DATA (seq==rcv_nxt) and sends ACK(rcv_nxt+1). Out-of-order as an immediate ACK(rcv_nxt).
// - Liveness Model:
//     - No busy waits in send() whilst retransmission and window advancement are driven by recv() calls and timer checks.
// - Buffers:
//     - outbuf snapshot retains last application message for window retransmission where inbuf is a single-slot latch for delivery.
// - Safety & Limits:
//     - MSS/Window defaults applied if 0 (wnd=32, mss=512, rto=120ms). Copy lengths are bounded and parse guards are strict.
// - Observability:
//     - Emits concise header traces to stderr for debugging; channel logs actual wire sends/receives.

// ======================================================================================================
// SETUP (ADJUST IF NECESSARY)
// ======================================================================================================
#include "bb_gbn.h"
#include "bb_proto.h"
#include "bb_log.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

// ======================================================================================================
// IMPLEMENTATIONS
// ======================================================================================================
// Transport the configurations with the GBN states
typedef struct {
    bb_channel_t*   ch;
    bb_proto_cfg_t  cfg;
    uint32_t snd_una;   // First unacked
    uint32_t snd_nxt;   // Next seq to send
    uint32_t rcv_nxt;   // Next expected seq
    uint64_t timer_ts;
    bool timer_running;
    uint8_t *outbuf; size_t outcap; size_t outlen;
    uint8_t *inbuf;  size_t incap;  size_t inlen; bool have_in;
} gbn_t;

// Increment the sequence number modulo 2^32 (pure and branch-free)
static inline uint32_t bb_next_seq(uint32_t x) { return x + 1; }

// Signed comparison with the results in modulo-2^32 space (returns <0, 0, >0)
static inline int32_t  seq_cmp(uint32_t a, uint32_t b) { return (int32_t)(a - b); }

// Declare the Arm and restart state to the single base retransmission timer
static inline void start_timer(gbn_t* s) {
    s->timer_ts = bb_now_ns() + (uint64_t)s->cfg.rto_ms * 1000000ull;
    s->timer_running = true;
}

// Declare a validation on the retransmission timer (Check for expiration)
static inline bool timer_expired(const gbn_t* s) {
    return s->timer_running && (bb_now_ns() >= s->timer_ts);
}

// Declare the encoder and send a single data frame with the given seq and payload
static int send_frame(gbn_t* s, uint32_t seq, const uint8_t* p, size_t n) {
    uint8_t buf[1500];
    size_t m = bb_wire_pack(buf, sizeof(buf), BB_FLAG_DATA, seq, s->rcv_nxt, p, (uint16_t)n);
    if (!m) return -1;
    if (bb_channel_send(s->ch, buf, m) < 0) return -1;
    if (!s->timer_running) start_timer(s);
    return 0;
}

// Send an ACK frame with the acknowledging 'ackn' (Preceeding state is expected)
static void send_ack(bb_channel_t* ch, uint32_t ackn) {
    uint8_t abuf[64];
    size_t an = bb_wire_pack(abuf, sizeof(abuf), BB_FLAG_ACK, 0, ackn, NULL, 0);
    (void)bb_channel_send(ch, abuf, an);
}

// Retransmit all the frames in current outstanding window using the snapshot buffer (Restart timer)
static void retransmit_window_from_snapshot(gbn_t* s) {
    size_t len = s->outlen;
    if (seq_cmp(s->snd_nxt, s->snd_una) <= 0 || len == 0) {
        s->timer_running = false;
        return;
    }
    for (uint32_t q = s->snd_una; seq_cmp(s->snd_nxt, q) > 0; ++q) {
        size_t off2 = (size_t)(q - s->snd_una) * s->cfg.mss;
        if (off2 >= len) break;
        size_t c2 = len - off2; if (c2 > s->cfg.mss) c2 = s->cfg.mss;
        (void)send_frame(s, q, s->outbuf + off2, c2);
    }
    start_timer(s);
}

// Construct and initialize a GBN transport (Apply the defaults for zero cfg fields)
bb_proto_t* bb_proto_gbn_create(bb_channel_t* ch, bb_proto_cfg_t cfg) {
    if (!cfg.wnd)    cfg.wnd = 32;
    if (!cfg.mss)    cfg.mss = 512;
    if (!cfg.rto_ms) cfg.rto_ms = 120;
    gbn_t* s = (gbn_t*)calloc(1, sizeof(*s));
    if (!s) return NULL;
    s->ch = ch; s->cfg = cfg;
    s->snd_una = cfg.init_seq;
    s->snd_nxt = cfg.init_seq;
    s->rcv_nxt = cfg.init_seq;
    s->outcap = 64 * 1024; s->outbuf = (uint8_t*)malloc(s->outcap); s->outlen = 0;
    s->incap  = 64 * 1024; s->inbuf  = (uint8_t*)malloc(s->incap);  s->inlen  = 0; s->have_in = false;
    return (bb_proto_t*)s;
}

// Release all the transport resources with its buffers and state (Safe to call once)
void bb_proto_close(bb_proto_t* v) {
    gbn_t* s = (gbn_t*)v;
    free(s->outbuf);
    free(s->inbuf);
    free(s);
}

// Fragment and send up to window capacity (Snapshot its payload for potential retransmittance)
int bb_proto_send(bb_proto_t* v, const uint8_t* data, size_t len) {
    gbn_t* s = (gbn_t*)v;
    // Snapshot payload
    if (len > s->outcap) len = s->outcap;
    memcpy(s->outbuf, data, len);
    s->outlen = len;
    size_t off = 0;
    while (off < len) {
        // Follow window policy (If full, just stop sending new and return to its caller)
        if ((uint32_t)(s->snd_nxt - s->snd_una) >= s->cfg.wnd) break;
        size_t chunk = len - off; if (chunk > s->cfg.mss) chunk = s->cfg.mss;
        if (send_frame(s, s->snd_nxt, data + off, chunk) < 0) return -1;
        s->snd_nxt = bb_next_seq(s->snd_nxt);
        off += chunk;
    }
    // Ensure no blockage is declared here (Caller will drive progress with recv)
    return 0;
}

// Collect the channel with timeout, process ACKs/timeout, and deliver its only in-order datas
ssize_t bb_proto_recv(bb_proto_t* v, uint8_t* out, size_t cap, int timeout_ms) {
    gbn_t* s = (gbn_t*)v;
    // Deliver latched inbound immediately
    if (s->have_in) {
        size_t n = s->inlen; if (n > cap) n = cap;
        memcpy(out, s->inbuf, n);
        s->have_in = false; s->inlen = 0;
        return (ssize_t)n;
    }
    // Handle the retransmittance on the RTO for before and after polling
    if (seq_cmp(s->snd_nxt, s->snd_una) > 0 && timer_expired(s)) {
        retransmit_window_from_snapshot(s);
    }
    // Declare the bounded poll of its corresponding channels (Caller controls timeout)
    uint8_t ibuf[2048];
    ssize_t rn = bb_channel_recv(s->ch, ibuf, sizeof(ibuf), timeout_ms);
    if (rn <= 0) {
        // Ensure an addition in RTO valication during timeout to allow progession
        if (rn == 0 && seq_cmp(s->snd_nxt, s->snd_una) > 0 && timer_expired(s)) {
            retransmit_window_from_snapshot(s);
        }
        return rn;
    }
    // Parse and act
    bb_hdr_t h; const uint8_t* pl;
    if (!bb_wire_parse(ibuf, rn, &h, &pl)) return 0;
    // Header trace for validation and debugging
    fprintf(stderr, "[GBN HDR] flags=0x%02x seq=%u ack=%u len=%u (rcv_nxt=%u)\n",
            h.flags, h.seq, h.ack, h.len, s->rcv_nxt);

    // Pump the cumulative ACKs for our outstanding sends
    if (seq_cmp(h.ack, s->snd_una) >= 0 && seq_cmp(h.ack, s->snd_nxt) <= 0) {
        s->snd_una = h.ack;
        if (seq_cmp(s->snd_una, s->snd_nxt) == 0) s->timer_running = false;
        else start_timer(s);
    }

    // Ensure to not deliver when the data is not validated
    if (!(h.flags & BB_FLAG_DATA)) return 0;

    // Ensure that the next state is expected and only accept through the validated orders
    if (seq_cmp(h.seq, s->rcv_nxt) != 0) {
        send_ack(s->ch, s->rcv_nxt);
        return 0;
    }

    size_t L = h.len; if (L > cap) L = cap;
    memcpy(out, pl, L);
    s->rcv_nxt = bb_next_seq(s->rcv_nxt);
    send_ack(s->ch, s->rcv_nxt);
    return (ssize_t)L;
}

/// ========================================================================================================================================
// END (ADD IMPLEMENTATIONS IF NECESSARY)
/// ========================================================================================================================================