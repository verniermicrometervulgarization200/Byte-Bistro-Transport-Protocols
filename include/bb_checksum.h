/// ========================================================================================================================================
// Module       : include/bb_checksum.h
// Description  : Checksum/CRC utilities (Fletcher-32 and optional CRC32C hardware path) for Byte-Bistro
// Topics       : Data Integrity, CRC, CPU, Portability
// Project      : Byte Bistro (Copyright 2025)
// Purpose      : Declares lightweight integrity primitives used by framing/transport layers to detect corruption,
//                with a pure C Fletcher-32 and an optional x86_64 SSE4.2 CRC32C fast path when available.
// Author       : Rizky Johan Saputra (Independent Project)
// Note         : API keeps callers agnostic to CPU features. If CRC32C HW is unavailable, bb_crc32c_hw() returns 0,
//                allowing higher layers to fall back (e.g., to Fletcher-32) without probing intrinsics directly.
/// ========================================================================================================================================

// ======================================================================================================
// SPECIFICATIONS
// ======================================================================================================
// - Fletcher-32:
//     - bb_fletcher32(data,len) into 32-bit checksum, endian-neutral, pure C and no dynamic allocation.
//     - Suitable for fast integrity checks where CRC strength is unnecessary.
// - CRC32C (Castagnoli, polynomial 0x1EDC6F41):
//     - bb_crc32c_hw_available() is set as true iff CPU provides SSE4.2 CRC32 instructions (x86_64).
//     - bb_crc32c_hw(data,len) is set to 0 if unsupported. Otherwise, returns CRC32C over 'data' (standard ~crc finalization).
// - Safety & Portability:
//     - All functions tolerate len==0 where pointers must be non-NULL when len>0.
//     - Hardware path currently guarded for x86_64 and non-x86 platforms will report as unavailable.
// - Usage Guidance:
//     - Prefer CRC32C when available for stronger burst-error detection and speed.
//     - Use Fletcher-32 as a portable fallback or when HW CRC32C is absent.
// - Performance Notes:
//     - CRC32C HW path uses byte-wise intrinsics here to keep implementation simple; can be extended to wider ops later.

// ======================================================================================================
// SETUP (ADJUST IF NECESSARY)
// ======================================================================================================
#ifndef BB_CHECKSUM_H
#define BB_CHECKSUM_H
#include "bb_common.h"

// ======================================================================================================
// IMPLEMENTATIONS
// ======================================================================================================
// Portable Fletcher-32
uint32_t bb_fletcher32(const uint8_t* data, size_t len);

// Hardware-accelerated CRC32C (SSE4.2)
uint32_t bb_crc32c_hw(const uint8_t* data, size_t len); // returns 0 if unsupported CPU

// Runtime availability probe for the CRC32C HW path
bool bb_crc32c_hw_available(void);
#endif

/// ========================================================================================================================================
// END (ADD IMPLEMENTATIONS IF NECESSARY)
/// ========================================================================================================================================