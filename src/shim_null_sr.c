/// ========================================================================================================================================
// Module       : src/shim_null_sr.c
// Description  : Null stub implementation for SR constructor (used for build-time selective transport disabling)
// Topics       : Build Systems, Linking, Stubs, Modularity
// Project      : Byte Bistro (Copyright 2025)
// Purpose      : Provides a placeholder symbol for bb_proto_sr_create() so that code can link without shipping SR.
// Author       : Rizky Johan Saputra (Independent Project)
// Note         : Returns NULL unconditionally. Exists strictly for ablation + benchmarking “zero transport reliability”
//                scenarios and incremental module testing.
/// ========================================================================================================================================

// ======================================================================================================
// SPECIFICATIONS
// ======================================================================================================
// - bb_proto_sr_create():
//     - Always returns NULL in this shim.
// - Applicability:
//     - When SR is disabled globally for compiled variant testing.
//     - When doing clean binary reduction to isolate overhead contribution of SR vs GBN etc.
// - Caller MUST guard against NULL before usage.

// ======================================================================================================
// SETUP (ADJUST IF NECESSARY)
// ======================================================================================================
#include "bb_sr.h"

// ======================================================================================================
// IMPLEMENTATIONS
// ======================================================================================================
// Declare the NULL transport constructor for the SR disabled variant (Always returns NULL)
bb_proto_t* bb_proto_sr_create(bb_channel_t* ch, bb_proto_cfg_t cfg){
    (void)ch; (void)cfg; return NULL;
}

/// ========================================================================================================================================
// END (ADD IMPLEMENTATIONS IF NECESSARY)
/// ========================================================================================================================================