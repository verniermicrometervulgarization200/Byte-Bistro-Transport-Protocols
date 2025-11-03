/// ========================================================================================================================================
// Module       : src/bb_timer.c
// Description  : Translation unit anchor for bb_timer_t inline API (header-only but provides compilation unit)
// Topics       : Timer linkage and Debugging
// Project      : Byte Bistro (Copyright 2025)
// Purpose      : Exists so this module has a TU presence in build graph. Logic is inline in header by design.
// Author       : Rizky Johan Saputra (Independent Project)
// Note         : This file intentionally contains no additional code. Do NOT add runtime here.
/// ========================================================================================================================================

// ======================================================================================================
// SPECIFICATIONS
// ======================================================================================================
// - Semantics:
//     - arm(timeout_ms) is configured by setting armed=true with deadline_ns being computed through bb_now_ns()
//     - disarm() is configured by setting armed=false
//     - expired() is true iff armed AND now>=deadline_ns
//     - remaining_ms() measures how many ms remain, whilst returning 0 if disarmed or expired
// - Performance:
//     - fully inline as there are no syscall except bb_now_ns(), cheap for high-frequency checks
// - Safety:
//     - Caller must reset or disarm before reuse. No memory allocation inside.

// ======================================================================================================
// SETUP (ADJUST IF NECESSARY)
// ======================================================================================================
#include "bb_timer.h"

// ======================================================================================================
// IMPLEMENTATIONS
// ======================================================================================================
// All the functions are inline in the header. This TU anchors the object file if needed.

/// ========================================================================================================================================
// END (ADD IMPLEMENTATIONS IF NECESSARY)
/// ========================================================================================================================================