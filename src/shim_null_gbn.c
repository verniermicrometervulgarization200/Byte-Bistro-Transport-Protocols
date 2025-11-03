/// ========================================================================================================================================
// Module       : src/shim_null_gbn.c
// Description  : Null stub implementation for GBN constructor (used for build-time selective transport disabling)
// Topics       : Build Systems, Linking, Stubs, Modularity
// Project      : Byte Bistro (Copyright 2025)
// Purpose      : Provides a placeholder symbol for bb_proto_gbn_create() so that components which link against GBN
//                can still compile even if full GBN transport code is intentionally excluded in a given build.
// Author       : Rizky Johan Saputra (Independent Project)
// Note         : Returns NULL unconditionally. DO NOT USE THIS IN ACTUAL RUNTIME. This is only for controlled
//                build variants / experiments / ablation benchmarking where GBN is disabled.
/// ========================================================================================================================================

// ======================================================================================================
// SPECIFICATIONS
// ======================================================================================================
// - bb_proto_gbn_create():
//     - Always returns NULL in this shim.
//     - Allows link satisfaction without shipping full GBN implementation.
// - Safety:
//     - Any caller receiving NULL MUST check before invoking send/recv/close.
// - Applicability:
//     - If comparing baseline (no GBN) performance vs with transport reliability
//     - If disabling GBN due to dynamic modular build / selective compile
// - Restrictions:
//     - In full production / functional Byte-Bistro transport experiments.

// ======================================================================================================
// SETUP (ADJUST IF NECESSARY)
// ======================================================================================================
#include "bb_gbn.h"

// ======================================================================================================
// IMPLEMENTATIONS
// ======================================================================================================
// Declare the NULL transport constructor for the GBN disabled variant (Always returns NULL)
bb_proto_t* bb_proto_gbn_create(bb_channel_t* ch, bb_proto_cfg_t cfg){
    (void)ch; (void)cfg; return NULL;
}

/// ========================================================================================================================================
// END (ADD IMPLEMENTATIONS IF NECESSARY)
/// ========================================================================================================================================