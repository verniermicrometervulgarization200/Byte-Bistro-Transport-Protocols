/// ========================================================================================================================================
// Module       : include/bb_channel.h
// Description  : Unreliable channel emulator with controllable loss/dup/reorder/latency/rate for Byte-Bistro
// Topics       : Computer Networks, Network Emulation, Sockets, Systems Testing, Reproducibility
// Project      : Byte Bistro (Copyright 2025)
// Purpose      : Defines the configuration and API for a userspace channel that intentionally perturbs datagrams
//                (drop, duplicate, reorder, delay, throttle) to stress-test higher layers (bb_proto, GBN/SR, bb_app).
// Author       : Rizky Johan Saputra (Independent Project)
// Note         : This abstraction is intentionally transport-agnostic and payload-opaque. It does not interpret frames;
//                it only delivers raw bytes with specified impairments for deterministic experiments.
/// ========================================================================================================================================

// ======================================================================================================
// SPECIFICATIONS
// ======================================================================================================
// - Behavior Controls (bb_channel_cfg_t):
//     - loss_pct        : Probability (0..100] of dropping a datagram silently.
//     - dup_pct         : Probability of enqueuing a second copy with slight delay (~1ms).
//     - reorder_pct     : Chance to reorder queued frames (simple adjacent swap).
//     - delay_mean_ms   : Base one-way delay added before send; applied per enqueued frame.
//     - delay_jitter_ms : Symmetric jitter amplitude (±jitter) added to the base delay.
//     - rate_mbps       : Token-bucket rate limit (0 => unlimited); enforced at send time.
//     - seed            : RNG seed for reproducibility; 0 selects a default nonzero seed.
// - API:
//     - bb_channel_create() : Binds a socket + peer to a new channel with the given impairment profile.
//     - bb_channel_send()   : Enqueues datagrams, applies impairments, and transmits when ready.
//     - bb_channel_recv()   : Blocking receive with timeout; returns bytes received or 0/-1 for timeout/error.
//     - bb_channel_destroy(): Frees all resources and pending queued frames.
// - Safety & Guarantees:
//     - Never mutates payload bytes; treats data as opaque.
//     - Bounded internal waits (send path) to avoid indefinite blocking; receive uses select() with timeout.
//     - Logs observable SEND/RECV events to stderr for experiment traceability.
// - Portability:
//     - Uses POSIX sockets (sendto/recvfrom/select). Windows would require minor adaptations.
// - Intended Use:
//     - Sits below transport (GBN/SR) and above OS/socket to emulate lossy networks in a reproducible way.

// ======================================================================================================
// SETUP (ADJUST IF NECESSARY)
// ======================================================================================================
#ifndef BB_CHANNEL_H
#define BB_CHANNEL_H
#include "bb_common.h"

// ======================================================================================================
// IMPLEMENTATIONS
// ======================================================================================================
// Transport the configurations (Drop probability, Jitter, Tokens)
typedef struct {
double loss_pct;        // Drop probability (0.100)
double dup_pct;         // Chance to duplicate a frame
double reorder_pct;     // Chance to reorder queued frames
double delay_mean_ms;   // Base one-way delay
double delay_jitter_ms; // Jitter added to base delay
double rate_mbps;       // Token-bucket sender rate (0 => unlimited)
uint64_t seed;          // RNG seed (0 => default)
} bb_channel_cfg_t;

// Declare the channel instances
typedef struct bb_channel_s bb_channel_t;

// Declare a channel bound to 'sockfd' and initial peer
bb_channel_t* bb_channel_create(int sockfd, const struct sockaddr_in* peer, bb_channel_cfg_t cfg);

// Destroy the channel and free all its pending frames/resources (NULL inclusion)
void bb_channel_destroy(bb_channel_t*);

// Send the datagrams through the channel with enqueue, impairment and transmittance
ssize_t bb_channel_send(bb_channel_t*, const void* buf, size_t n);

// Receive the datagram with a bounded timeout
ssize_t bb_channel_recv(bb_channel_t*, void* buf, size_t cap, int timeout_ms);
#endif

/// ========================================================================================================================================
// END (ADD IMPLEMENTATIONS IF NECESSARY)
/// ========================================================================================================================================