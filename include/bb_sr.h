/// ========================================================================================================================================
// Module       : include/bb_sr.h
// Description  : Selective Repeat (SR) transport constructor for Byte-Bistro
// Topics       : Transport Protocols, Sliding Window, Per-Packet Timers, Reliability
// Project      : Byte Bistro (Copyright 2025)
// Purpose      : Declares the factory for an SR-based bb_proto_t instance, which provides selective ACK/NAK-style
//                reliability with per-sequence buffering and retransmission timers.
// Author       : Rizky Johan Saputra (Independent Project)
// Note         : The returned bb_proto_t obeys the bb_proto.h API (send/recv/close). GBN uses a separate constructor.
/// ========================================================================================================================================

// ======================================================================================================
// SPECIFICATIONS
// ======================================================================================================
// - Construction:
//     - bb_proto_sr_create(ch, cfg) binds the transport to a bb_channel_t and applies bb_proto_cfg_t.
//     - Zero cfg fields get sane defaults (wnd, mss, rto_ms), clamped to internal caps.
// - Semantics (via bb_proto.h):
//     - Send may fragment into <= MSS frames and keep up to 'wnd' inflight with each frame having its own timer.
//     - Receive buffers are set as its out-of-order frames in the windows and deliver prefix and piggybacks ACK(rcv_nxt).
// - Safety:
//     - Opaque bb_proto_t instance owns per-slot buffers/timers where caller releases via bb_proto_close().
//     - Channel payload is treated as opaque bytes as SR never rewrites application encoding.

// ======================================================================================================
// SETUP (ADJUST IF NECESSARY)
// ======================================================================================================
#ifndef BB_SR_H
#define BB_SR_H
#include "bb_proto.h"

// ======================================================================================================
// IMPLEMENTATIONS
// ======================================================================================================
bb_proto_t* bb_proto_sr_create(bb_channel_t* ch, bb_proto_cfg_t cfg);
#endif

/// ========================================================================================================================================
// END (ADD IMPLEMENTATIONS IF NECESSARY)
/// ========================================================================================================================================