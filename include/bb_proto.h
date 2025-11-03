/// ========================================================================================================================================
// Module       : include/bb_proto.h
// Description  : Transport-agnostic protocol interface for Byte-Bistro (GBN/SR creators + send/recv API)
// Topics       : Computer Networks, Framing, Transport Abstractions, ARQ, System Interfaces, Modularity
// Project      : Byte Bistro (Copyright 2025)
// Purpose      : Declares a stable API boundary for reliable transports (GBN / SR) running atop a generic channel.
//                Encoders/decoders for frames are implemented in the .c files; application semantics live in bb_app.c.
//                Callers use bb_proto_send/recv() to exchange opaque application bytes, independent of transport choice.
// Author       : Rizky Johan Saputra (Independent Project)
// Note         : This API is versionable and intentionally minimal. All implementations MUST preserve semantics across
//                platforms (endianness-safe) and respect bounded buffers for safety and testability.
/// ========================================================================================================================================

// ======================================================================================================
// SPECIFICATIONS
// ======================================================================================================
// - Abstractions:
//     - bb_proto_t is an opaque handle representing a transport instance bound to a bb_channel_t.
//     - bb_proto_gbn_create() and bb_proto_sr_create() construct concrete GBN/SR implementations.
// - Configurations:
//     - init_seq  : 32-bit initial sequence number (deterministic if seeded).
//     - wnd       : Sender window size (frames); MUST be >= 1 and within internal hard caps.
//     - mss       : Maximum segment size (bytes) for payload per frame (MTU-aware).
//     - rto_ms    : Base retransmission timeout in milliseconds (subject to adaptive tuning).
// - Send and Receive Semantics:
//     - bb_proto_send()  : Fragment the 'data' into multiple frames and ensure a reliable delivery.
//     - bb_proto_recv()  : Reassembles a complete application message and returns the byte count or timeouts.
//     - Both of the calls are blocking with bounded waits, where timeouts are explicit with'timeout_ms' in recv.
// - Safety and Portability:
//     - No UB: All pointers are validated, sizes checked, and interfaces remain C ABI-stable.
//     - Endianness handled internally in implementations; header users treat data as opaque.
// - Lifetime and Observability:
//     - bb_proto_close() releases resources and detaches from the underlying channel; subsequent calls are invalid.
//     - Implementations should expose counters via internal diagnostics (not part of this header API).

// ======================================================================================================
// SETUP (ADJUST IF NECESSARY)
// ======================================================================================================
#ifndef BB_PROTO_H
#define BB_PROTO_H
#include "bb_common.h"
#include "bb_wire.h"
#include "bb_channel.h"

// ======================================================================================================
// IMPLEMENTATIONS
// ======================================================================================================
// Transport the configurations (window, MSS, timers, initial sequence)
typedef struct {
uint32_t init_seq;      // Initial sequence number (mod 2^32)
uint32_t wnd;           // Send window size (frames)
uint32_t mss;           // Max segment size (payload bytes per frame)
uint32_t rto_ms;        // Baseline retransmission timeout (milliseconds)
} bb_proto_cfg_t;

// Opaque transport instance (GBN or SR behind the API)
typedef struct bb_proto_s bb_proto_t;

// Construct a Go-Back-N transport bound to 'bb_channel_t' with cfg
bb_proto_t* bb_proto_gbn_create(bb_channel_t*, bb_proto_cfg_t);

// Construct a Selective Repeat transport bound to 'bb_channel_t' with cfg
bb_proto_t* bb_proto_sr_create(bb_channel_t*, bb_proto_cfg_t);

// Send one application message (split into frames as needed)
int bb_proto_send(bb_proto_t*, const uint8_t* data, size_t len);

// Receive one complete application message (reassembles frames)
ssize_t bb_proto_recv(bb_proto_t*, uint8_t* out, size_t cap, int timeout_ms);

// Close and release transport resources. Safe to call once; no-op if NULL.
void bb_proto_close(bb_proto_t*);

#endif

/// ========================================================================================================================================
// END (ADD IMPLEMENTATIONS IF NECESSARY)
/// ========================================================================================================================================