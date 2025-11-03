/// ========================================================================================================================================
// Module       : include/bb_gbn.h
// Description  : Go-Back-N (GBN) transport constructor for Byte-Bistro
// Topics       : Transport Protocols, Sliding Window ARQ, Timers, Reliability
// Project      : Byte Bistro (Copyright 2025)
// Purpose      : Declares the factory for a GBN-based bb_proto_t instance which implements cumulative-ACK ARQ
//                with a fixed-size sender window and single retransmission timer.
// Author       : Rizky Johan Saputra (Independent Project)
// Note         : The returned bb_proto_t obeys the bb_proto.h API (send/recv/close). GBN is selected by calling
//                bb_proto_gbn_create(); Selective Repeat (SR) has a separate constructor.
/// ========================================================================================================================================

// ======================================================================================================
// SPECIFICATIONS
// ======================================================================================================
// - Construction:
//     - bb_proto_gbn_create(ch, cfg) binds the transport to a bb_channel_t and applies bb_proto_cfg_t.
//     - cfg defaults are applied internally if zero (e.g., wnd, mss, rto_ms).
// - Semantics (as used via bb_proto.h):
//     - Send may fragment into the MSS frames and enqueue up to 'wnd' outstanding frames.
//     - Receive reassembles a single in-order message; cumulative ACK behavior (GBN).
// - Safety:
//     - Opaque bb_proto_t instance owns its buffers where caller releases via bb_proto_close().
//     - Channel payloads are treated as opaque bytes and GBN never rewrites application encoding.

// ======================================================================================================
// SETUP (ADJUST IF NECESSARY)
// ======================================================================================================
#ifndef BB_GBN_H
#define BB_GBN_H
#include "bb_proto.h"

// ======================================================================================================
// IMPLEMENTATIONS
// ======================================================================================================
// Construct a Go-Back-N transport instance implementing bb_proto_t
bb_proto_t* bb_proto_gbn_create(bb_channel_t* ch, bb_proto_cfg_t cfg);
#endif

/// ========================================================================================================================================
// END (ADD IMPLEMENTATIONS IF NECESSARY)
/// ========================================================================================================================================