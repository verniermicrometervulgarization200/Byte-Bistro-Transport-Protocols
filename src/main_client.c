/// ========================================================================================================================================
// Module       : src/main_client.c
// Description  : Byte-Bistro UDP client driving application orders over (GBN|SR) transports with nonblocking recv loop
// Topics       : Client Networking, UDP, Concurrency (pthread), CLI, Observability
// Project      : Byte Bistro (Copyright 2025)
// Purpose      : Spawns N client threads that each open a UDP socket, wrap it with bb_channel (optionally lossy),
//                select a transport (GBN or SR), encode orders with bb_app, and drive a nonblocking recv loop to
//                observe liveness and end-to-end latency without blocking the transport internals.
// Author       : Rizky Johan Saputra (Independent Project)
// Note         : Build tag (for reproducibility): FIX-RECV-NONBLOCK-2025-11-02-B. The client prints concise progress
//                indicators ('.') while polling bb_proto_recv(timeout=0). Adjust -n/-c to scale load.
//                ERR/WARN go to stderr; INFO/DBG controlled via -v/-q switches.
/// ========================================================================================================================================

// ======================================================================================================
// SPECIFICATIONS
// ======================================================================================================
// - CLI:
//     - addr <ip:port>     : Server address (default 127.0.0.1:7777)
//     - proto (gbn|sr)     : Select transport (default gbn)
//     - n <orders>         : Orders per thread (default 5)
//     - c <threads>        : Number of client threads (default 1)
//     - v / -q             : Verbose (BB_DBG) / quiet (BB_WARN) logging level
// - Concurrency Model:
//     - Each worker thread owns its UDP socket and transport instance with no shared transport across threads.
//     - Threads are joinable with no detached threads.
// - Channel Configuration (default):
//     - No impairments by default (loss/dup/reorder/delay/rate all zero), but can be changed here if needed.
// - Transport Configuration (default):
//     - init_seq=1, wnd=32, mss=512, rto=150ms
// - Nonblocking Receive Loop:
//     - For each order: Poll bb_proto_recv(timeout=0) up to ~5s total (50 ticks x 100ms sleep), printing '.' beats.
//     - Purpose: Demonstrate progress/liveness without blocking inside transport and its timers running internally.
// - Safety & Error Handling:
//     - If proto constructor returns NULL (e.g., shim build), print error and exit worker cleanly.
//     - bb_app_parse_reply validates incoming reply shape before logging success.
// - Observability:
//     - BANNER identifies build. Per-order DBG lines show send attempts with final INF lines print id/items/RTT.
//     - Channel also logs wire I/O if enabled in its module.

// ======================================================================================================
// SETUP (ADJUST IF NECESSARY)
// ======================================================================================================
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <pthread.h>
#include <errno.h>
#include "bb_common.h"
#include "bb_log.h"
#include "bb_channel.h"
#include "bb_proto.h"
#include "bb_app.h"

// ======================================================================================================
// IMPLEMENTATIONS
// ======================================================================================================
// Transport the configurations with the client states
typedef struct {
    struct sockaddr_in peer;
    int  threads;
    int  count;
    bool use_sr;
} cfg_t;

typedef struct {
    int   id;
    cfg_t cfg;
} worker_arg_t;

// Declare the worker thread entry with one UDP socket, channel and transport
static void* worker(void* p) {
    worker_arg_t* wa = (worker_arg_t*)p;
    int   id = wa->id;
    cfg_t c  = wa->cfg;
    free(wa);
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    BB_TRY(fd, "socket");

    // Bind its ephemeral (Ensures debugging the client port)
    struct sockaddr_in me; memset(&me, 0, sizeof(me));
    me.sin_family = AF_INET; me.sin_addr.s_addr = htonl(INADDR_ANY); me.sin_port = 0;
    (void)bind(fd, (struct sockaddr*)&me, sizeof(me));

    // Send a HELLO to seed the 4-tuple on the server
    static const uint8_t hello[] = "HELLO\n";
    sendto(fd, hello, sizeof(hello)-1, 0, (struct sockaddr*)&c.peer, sizeof(c.peer));
    usleep(10 * 1000);

    // Declare the channel (Set to default with no impairments, adjust to test its robustness)
    bb_channel_cfg_t chcfg = {
        .loss_pct        = 0,
        .dup_pct         = 0,
        .reorder_pct     = 0,
        .delay_mean_ms   = 0,
        .delay_jitter_ms = 0,
        .rate_mbps       = 0,
        .seed            = (uint64_t)bb_now_ns() ^ (uint64_t)id
    };
    bb_channel_t* ch = bb_channel_create(fd, &c.peer, chcfg);

    // Transport the config (Applies to both the GBN and SR protocols)
    bb_proto_cfg_t pcfg = {
        .init_seq = 1,
        .wnd      = 32,
        .mss      = 512,
        .rto_ms   = 150
    };
    bb_proto_t* proto = c.use_sr ? bb_proto_sr_create(ch, pcfg)
                                 : bb_proto_gbn_create(ch, pcfg);
    if (!proto) {
        fprintf(stderr, "[ERR] proto init failed (use --proto %s)\n", c.use_sr ? "sr" : "gbn");
        return NULL;
    }

    // Build the application order payload
    for (int i = 0; i < c.count; i++) {
        bb_order_t o = {.id = ((uint64_t)id << 32) | (uint32_t)i};
        snprintf(o.itemlist, sizeof(o.itemlist),
                 (i % 3 == 0) ? "double-cheese,cola" : "fries,shake");

        uint8_t out[512];
        int n = bb_app_encode_order(&o, out, sizeof(out));
        fprintf(stderr, "[DBG] cli#%d sending order #%d len=%d\n", id, i, n);

        if (bb_proto_send(proto, out, (size_t)n) < 0) {
            fprintf(stderr, "[ERR] cli#%d send failed\n", id);
            continue;
        }

        // Declare the non-blocking receive iteration (max ~5s total)
        uint8_t in[512];
        ssize_t rn = 0;
        int ticks = 50;                                     // 50 * 100ms = 5s
        while (ticks-- > 0) {
            rn = bb_proto_recv(proto, in, sizeof(in), 0);   // **NON-BLOCK**: timeout=0
            if (rn > 0) break;                              // Return the reply
            if (rn < 0) break;                              // Return error state
            fputc('.', stderr);                             // Set heartbeat to observe progress
            fflush(stderr);
            usleep(100 * 1000);                             // 100ms
        }
        fputc('\n', stderr);

        // Parse the application based reply
        if (rn > 0) {
            uint64_t idr = 0;
            uint32_t ms  = 0;
            char items[256];
            if (bb_app_parse_reply(in, (size_t)rn, &idr, &ms, items, sizeof(items)) == 0) {
                fprintf(stderr, "[INF] cli#%d ok id=%llu items=\"%s\" rtt=%lums\n",
                        id, (unsigned long long)idr, items, (unsigned long)ms);
            } else {
                fprintf(stderr, "[WRN] cli#%d parse fail (len=%zd)\n", id, rn);
            }
        } else if (rn == 0) {
            fprintf(stderr, "[WRN] cli#%d timeout waiting for reply (~5s)\n", id);
        } else { // rn < 0
            fprintf(stderr, "[ERR] cli#%d recv error (rn=%zd, errno=%d)\n", id, rn, errno);
        }
    }
    return NULL;
}

// The main entry which parses the CLI, spawns worker threads, joins them, and exit
int main(int argc, char** argv) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    // Debugging in the client side to diagnose any latency or errors
    fprintf(stderr, "[BANNER] Byte-Bistro client build = FIX-RECV-NONBLOCK-2025-11-02-B\n");
    if (argc < 5) {
        fprintf(stderr,
            "Usage: %s --addr <ip:port> --proto (gbn|sr) -n <orders_per_thread> -c <threads> [-v|-q]\n",
            argv[0]);
        return 1;
    }
    cfg_t cfg; memset(&cfg, 0, sizeof(cfg));
    cfg.threads = 1;
    cfg.count   = 5;
    cfg.use_sr  = false;
    char ip[64] = "127.0.0.1";
    int  port   = 7777;

    // Parse the CLI switches
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--addr") && i + 1 < argc) {
            sscanf(argv[++i], "%63[^:]:%d", ip, &port);
        } else if (!strcmp(argv[i], "--proto") && i + 1 < argc) {
            const char* p = argv[++i];
            cfg.use_sr = !strcmp(p, "sr");
        } else if (!strcmp(argv[i], "-n") && i + 1 < argc) {
            cfg.count = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "-c") && i + 1 < argc) {
            cfg.threads = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "-v")) {
            bb_log_level = BB_DBG;
        } else if (!strcmp(argv[i], "-q")) {
            bb_log_level = BB_WARN;
        }
    }

    // Fill the peer sockaddr
    cfg.peer.sin_family = AF_INET;
    cfg.peer.sin_port   = htons(port);
    inet_pton(AF_INET, ip, &cfg.peer.sin_addr);

    // Launch the worker threads
    pthread_t th[cfg.threads];
    for (int t = 0; t < cfg.threads; t++) {
        worker_arg_t* w = (worker_arg_t*)malloc(sizeof(*w));
        *w = (worker_arg_t){ .id = t, .cfg = cfg };
        pthread_create(&th[t], NULL, worker, w);
    }
    for (int t = 0; t < cfg.threads; t++) {
        pthread_join(th[t], NULL);
    }
    return 0;
}

/// ========================================================================================================================================
// END (ADD IMPLEMENTATIONS IF NECESSARY)
/// ========================================================================================================================================