/// ========================================================================================================================================
// Module       : src/main_server.c
// Description  : Byte-Bistro UDP server that wraps a socket with bb_channel impairments and (GBN|SR) transport,
//                parses ORDERs, simulates "cooking" latency, and returns REPLY messages.
// Topics       : UDP Servers, Reliability (GBN/SR), CLI, Randomized Latency Models, Observability
// Project      : Byte Bistro (Copyright 2025)
// Purpose      : Acts as a deterministic-yet-configurable service endpoint for client experiments. Lets you test
//                end-to-end behavior under controlled channel impairments and kitchen delay distributions.
// Author       : Rizky Johan Saputra (Independent Project)
// Note         : Start-up waits for a HELLO datagram to learn the client's 4-tuple, then serves one session.
//                Call --cook-* knobs to emulate service times and --loss/--dup/--reorder to stress transports.
/// ========================================================================================================================================

// ======================================================================================================
// SPECIFICATIONS
// ======================================================================================================
// - CLI (see usage()):
//     - port <p>                  : UDP listen port (default 7777)
//     - proto (gbn|sr)            : select transport
//     - v / -q                    : log verbosity (BB_DBG / BB_WARN)
//     - Channel impairments:
//         --loss P, --dup P, --reorder P        (percent, 0..100)
//         --dmean MS, --djitter MS              (one-way delay mean/jitter)
//         --rate Mbps                           (token-bucket rate where 0 = unlimited)
//         --seed N                              (PRNG seed where 0 => time-based)
//     - Kitchen latency:
//         --cook-min MS, --cook-max MS
//         --cook-dist (uniform|exp)
//         --cook-mean MS                        (Used for exp. If 0, mean≈(min+max)/2)
// - Session model:
//     - Single peer learned via first received datagram ("HELLO").
//     - Main loop: ORDER into cook delay into REPLY with repetitions until SIGINT is called.
// - Transport defaults:
//     - init_seq=1, wnd=32, mss=512, rto=150ms (applied for both GBN and SR).
// - Safety & Observability:
//     - BB_TRY/BBLOG for errors. ERR/WARN→stderr, INFO/DBG→stdout. -v|-q control level.
//     - Channel logs real socket I/O. Server logs session start and per-request debug lines.
// - Determinism:
//     - With a fixed --seed and fixed knobs, runs are reproducible.
// - Limits:
//     - Single client session per process instance (simple lab harness).

// ======================================================================================================
// SETUP (ADJUST IF NECESSARY)
// ======================================================================================================
#define _GNU_SOURCE
#include "bb_common.h"
#include "bb_log.h"
#include "bb_channel.h"
#include "bb_proto.h"
#include "bb_app.h"
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>

// ======================================================================================================
// IMPLEMENTATIONS
// ======================================================================================================
// Assign the global variables and sigint handler to request shutdown of the serve iteration
static volatile int g_stop = 0;
static void on_sigint(int sig){ (void)sig; g_stop = 1; }

// Transport the configurations with the server states
typedef enum {
    COOK_UNIFORM = 0,
    COOK_EXP     = 1
} cook_dist_t;

typedef struct {
    int port;
    int verbose;
    enum { MODE_GBN, MODE_SR } mode;

    // Declare the channel knobs
    int loss_pct;
    int dup_pct;
    int reorder_pct;
    int dmean_ms;
    int djitter_ms;
    int rate_mbps;
    uint64_t seed;

    // Declare the kitchen knobs
    uint32_t    cook_min_ms;
    uint32_t    cook_max_ms;
    cook_dist_t cook_dist;
    double      cook_mean_ms; // used when dist=EXP (fallbacks if 0)
} cfg_t;

// Print the short usage banner for interpretability
static void usage(const char* argv0){
    fprintf(stderr,
        "Usage: %s --port <p> --proto (gbn|sr) [-v|-q]\n"
        "       [--loss P] [--dup P] [--reorder P]\n"
        "       [--dmean MS] [--djitter MS] [--rate Mbps] [--seed N]\n"
        "       [--cook-min MS] [--cook-max MS]\n"
        "       [--cook-dist uniform|exp] [--cook-mean MS]\n",
        argv0);
}

// Parse the CLI into cfg (Apply defaults and sanity corrections)
static int parse_args(int argc, char** argv, cfg_t* out){
    memset(out, 0, sizeof(*out));
    out->port = 7777;
    out->mode = MODE_GBN;
    bb_log_level = BB_INFO;

    // Set the defaults (Channel off, deterministic 40ms kitchen, Uniform with min=max)
    out->loss_pct = 0;
    out->dup_pct = 0;
    out->reorder_pct = 0;
    out->dmean_ms = 0;
    out->djitter_ms = 0;
    out->rate_mbps = 0;
    out->seed = 0;              // 0 => auto-seed
    out->cook_min_ms = 40;
    out->cook_max_ms = 40;
    out->cook_dist   = COOK_UNIFORM;
    out->cook_mean_ms = 0.0;    // if 0 and dist=EXP, compute the mean from min/max

    // Declare the specified CLI for the server side
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--port") && i+1 < argc) {
            out->port = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--proto") && i+1 < argc) {
            const char* p = argv[++i];
            if (!strcmp(p, "gbn")) out->mode = MODE_GBN;
            else if (!strcmp(p, "sr")) out->mode = MODE_SR;
            else return -1;
        } else if (!strcmp(argv[i], "-v")) {
            bb_log_level = BB_DBG;
        } else if (!strcmp(argv[i], "-q")) {
            bb_log_level = BB_WARN;
        } else if (!strcmp(argv[i], "--loss") && i+1 < argc) {
            out->loss_pct = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--dup") && i+1 < argc) {
            out->dup_pct = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--reorder") && i+1 < argc) {
            out->reorder_pct = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--dmean") && i+1 < argc) {
            out->dmean_ms = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--djitter") && i+1 < argc) {
            out->djitter_ms = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--rate") && i+1 < argc) {
            out->rate_mbps = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--seed") && i+1 < argc) {
            out->seed = (uint64_t)strtoull(argv[++i], NULL, 10);
        } else if (!strcmp(argv[i], "--cook-min") && i+1 < argc) {
            out->cook_min_ms = (uint32_t)atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--cook-max") && i+1 < argc) {
            out->cook_max_ms = (uint32_t)atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--cook-dist") && i+1 < argc) {
            const char* d = argv[++i];
            if (!strcmp(d, "uniform")) out->cook_dist = COOK_UNIFORM;
            else if (!strcmp(d, "exp")) out->cook_dist = COOK_EXP;
            else return -1;
        } else if (!strcmp(argv[i], "--cook-mean") && i+1 < argc) {
            out->cook_mean_ms = atof(argv[++i]);
        } else {
            return -1;
        }
    }
    if (out->cook_max_ms < out->cook_min_ms) {
        uint32_t t = out->cook_min_ms; out->cook_min_ms = out->cook_max_ms; out->cook_max_ms = t;
    }
    return 0;
}

// Declare a uniform integer
static uint32_t rand_u32_range(uint32_t a, uint32_t b_inclusive){
    if (a == b_inclusive) return a;
    uint32_t span = b_inclusive - a + 1u;
    return a + (uint32_t)(rand() % span);
}

// Pseudo-random in (0,1] (Never localized at 0)
static double rand_unit_open(){ // (0,1]
    double r = ((double)rand() + 1.0) / ((double)RAND_MAX + 1.0);
    if (r <= 0.0) r = 1e-12;
    return r;
}

// Draw a cook time with the current distribution and knobs (Clamps into [min,max])
static uint32_t draw_cook_ms(const cfg_t* cfg){
    if (cfg->cook_dist == COOK_UNIFORM){
        return rand_u32_range(cfg->cook_min_ms, cfg->cook_max_ms);
    } else {
        // Exponential with mean mu (ms) (If cook_mean_ms==0, fallback to mid of [min,max]).
        double mu = cfg->cook_mean_ms;
        if (mu <= 0.0){
            mu = 0.5 * ((double)cfg->cook_min_ms + (double)cfg->cook_max_ms);
            if (mu <= 0.0) mu = 40.0;
        }
        // Exponential draw in ms
        double u = rand_unit_open();
        double x = -mu * log(u);

        // If bounds are set (min/max), clamp into [min,max]
        double lo = (double)cfg->cook_min_ms;
        double hi = (double)cfg->cook_max_ms;
        if (hi < lo) hi = lo;
        if (lo==hi){
            x = lo;
        } else {
            if (x < lo) x = lo;
            if (x > hi) x = hi;
        }
        if (x < 0.0) x = 0.0;
        return (uint32_t)(x + 0.5);
    }
}

// The main entry that parses the args, bind UDP, learn peer, wrap with channel and proto, serve until SIGINT.
int main(int argc, char** argv){
    cfg_t cfg;
    if (parse_args(argc, argv, &cfg) != 0){
        usage(argv[0]);
        return 1;
    }

    // Declare the seed RNGs (Prefer CLI seed. Otherwise, use a time-based system).
    uint64_t seed_use = cfg.seed ? cfg.seed : (uint64_t)bb_now_ns();
    srand((unsigned)seed_use);
    signal(SIGINT, on_sigint);

    // Bind the UDP socket on server port
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    BB_TRY(fd, "socket");
    struct sockaddr_in me = {0};
    me.sin_family = AF_INET;
    me.sin_addr.s_addr = htonl(INADDR_ANY);
    me.sin_port = htons(cfg.port);
    BB_TRY(bind(fd, (struct sockaddr*)&me, sizeof(me)), "bind");
    BBLOG(BB_INFO, "server ready :%d proto=%s",
          cfg.port, (cfg.mode==MODE_GBN) ? "GBN" : "SR");

    // Wait for a HELLO to learn its peer (First datagram)
    struct sockaddr_in peer = {0};
    uint8_t tmp[1500];
    for(;;){
        socklen_t sl = sizeof(peer);
        ssize_t rn = recvfrom(fd, tmp, sizeof(tmp), 0, (struct sockaddr*)&peer, &sl);
        if (rn < 0){
            if (errno == EINTR) continue;
            BBLOG(BB_ERR, "recvfrom error: %s", strerror(errno));
            return 2;
        }
        if (rn > 0) break;
    }
    char ipbuf[64];
    inet_ntop(AF_INET, &peer.sin_addr, ipbuf, sizeof(ipbuf));
    BBLOG(BB_INFO, "session start %s:%d via %s",
          ipbuf, ntohs(peer.sin_port),
          (cfg.mode==MODE_GBN) ? "GBN" : "SR");

    // Wrap the socket with channel and proto
    bb_channel_cfg_t chcfg = {
        .loss_pct = cfg.loss_pct,
        .dup_pct = cfg.dup_pct,
        .reorder_pct = cfg.reorder_pct,
        .delay_mean_ms = cfg.dmean_ms,
        .delay_jitter_ms = cfg.djitter_ms,
        .rate_mbps = cfg.rate_mbps,
        .seed = seed_use
    };
    bb_channel_t* ch = bb_channel_create(fd, &peer, chcfg);
    bb_proto_cfg_t pcfg = {
        .init_seq = 1,
        .wnd = 32,
        .mss = 512,
        .rto_ms = 150
    };
    bb_proto_t* proto = (cfg.mode==MODE_GBN)
                      ? bb_proto_gbn_create(ch, pcfg)
                      : bb_proto_sr_create(ch, pcfg);
    if (!proto){
        BBLOG(BB_ERR, "proto init failed");
        return 3;
    }

    // Declare the main serve loop
    uint64_t served_id = 0;
    while (!g_stop){
        // Receive a single order frame
        uint8_t in[1500];
        ssize_t rn = bb_proto_recv(proto, in, sizeof(in), 1000);
        if (rn == 0) continue;              // Timeout (Poll again)
        if (rn < 0) {                       // Socket and proto error
            BBLOG(BB_ERR, "proto recv error");
            break;
        }

        // Parse the orders
        uint64_t oid = 0;
        char items[256];
        if (bb_app_parse_order(in, (size_t)rn, &oid, items, sizeof(items)) != 0){
            // Ignore if it is not an order payload
            continue;
        }

        // Set cook with the specified distribution
        uint32_t t_ms = draw_cook_ms(&cfg);
        usleep(t_ms * 1000);

        // Encode the reply and send through proto
        uint8_t out[1500];
        int wn = bb_app_encode_reply(oid, items, t_ms, out, sizeof(out));
        if (wn < 0){
            BBLOG(BB_ERR, "encode reply failed");
            continue;
        }
        int rc = bb_proto_send(proto, out, (size_t)wn);
        if (rc < 0){
            BBLOG(BB_ERR, "proto send failed");
            continue;
        }

        // Return the served id, items and time
        BBLOG(BB_DBG, "served id=%lu items=\"%s\" t=%ums",
              (unsigned long)served_id, items, t_ms);
        served_id++;
    }
    bb_proto_close(proto);
    bb_channel_destroy(ch);
    close(fd);
    return 0;
}

/// ========================================================================================================================================
// END (ADD IMPLEMENTATIONS IF NECESSARY)
/// ========================================================================================================================================