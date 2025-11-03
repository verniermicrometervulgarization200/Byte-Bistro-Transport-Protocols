/// ========================================================================================================================================
// Module:      src/bb_proto.c
// Description: Binary frame helpers and transport-agnostic utilities for Byte-Bistro
// Topics:      Framing, Sequence Arithmetic, Checksums (future), Reliability Primitives, Systems Safety
// Project:     Byte Bistro (Copyright 2025)
// Purpose:     Hosts small, transport-shared helpers (e.g., sequence arithmetic) and, in extended versions,
//              may include frame encode/decode and checksum utilities consumed by GBN/SR backends.
// Author:      Rizky Johan Saputra (Independent Project)
// Note:        Keep this file lean and dependency-light to avoid cycles with GBN/SR. If the project evolves
//              to a dedicated frame layer, promote heavy encode/decode into a dedicated module (e.g., bb_frame.*).
/// ========================================================================================================================================

// ======================================================================================================
// SPECIFICATIONS
// ======================================================================================================
// - Sequence arithmetic is modulo 2^32. Helpers must be pure, branch-light, and inlinable.
// - For checksum or framing helpers that were added here, they MUST:
//     - Be endianness-safe
//     - Avoid dynamic allocation
//     - Return negative on malformed inputs
//     - Operate only with the provided caller based buffers (no hidden globals)
// - This module should remain transport-agnostic (Do not integrate any GBN/SR policy encodings).

// ======================================================================================================
// SETUP (ADJUST IF NECESSARY)
// ======================================================================================================
#include "bb_proto.h"

// ======================================================================================================
// IMPLEMENTATIONS
// ======================================================================================================
// Declare the preceeding sequence results in the modulo-2^32 arithmetic
uint32_t bb_next_seq(uint32_t x){ return x+1; }

// Implement more helpers for robustness (could go here and briefll annotate for clarity)

/// ========================================================================================================================================
// END (ADD IMPLEMENTATIONS IF NECESSARY)
/// ========================================================================================================================================