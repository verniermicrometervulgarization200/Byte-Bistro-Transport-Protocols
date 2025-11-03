/// ========================================================================================================================================
// Module       : src/bb_checksum.c
// Description  : Implementations of Fletcher-32 and optional x86_64 SSE4.2 CRC32C for Byte-Bistro
// Topics       : Checksums, CRC, CPU Feature Detection, Intrinsics
// Project      : Byte Bistro (Copyright 2025)
// Purpose      : Provides portable integrity primitives with an optimized CRC32C path when hardware support is detected.
// Author       : Rizky Johan Saputra (Independent Project)
// Note         : CRC32C HW path uses compiler intrinsics when __SSE4_2__ is defined. If compiled without intrinsics
//                (or on non-x86_64), functions safely report unavailability (return 0) so callers can fall back.
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
#include "bb_checksum.h"
#ifdef __x86_64__
#include <cpuid.h>
#endif

// ======================================================================================================
// IMPLEMENTATIONS
// ======================================================================================================
// Portable Fletcher-32 checksum (RFC-style implementation)
uint32_t bb_fletcher32(const uint8_t* data, size_t len){
    // Ensure a pure C, endian-neutral and no dynamic allocation
    uint32_t sum1 = 0xffff, sum2 = 0xffff;
    while (len) {
        size_t tlen = len > 360 ? 360 : len;
        len -= tlen;
        do {
            sum1 += *data++;
            sum2 += sum1;
        } while (--tlen);
        sum1 = (sum1 & 0xffff) + (sum1 >> 16);
        sum2 = (sum2 & 0xffff) + (sum2 >> 16);
    }
    sum1 = (sum1 & 0xffff) + (sum1 >> 16);
    sum2 = (sum2 & 0xffff) + (sum2 >> 16);
    return (sum2 << 16) | sum1;
}

// Internal system to detect SSE4.2 CRC32 instruction support (x86_64).
static int have_sse42(void){
#ifdef __x86_64__
    unsigned int a,b,c,d; __get_cpuid(1,&a,&b,&c,&d); return (c & (1u<<20))!=0; // SSE4.2
#else
    return 0;
#endif
}

// Public probe for CRC32C hardware availability
bool bb_crc32c_hw_available(void){ return have_sse42(); }

// Hardware-accelerated CRC32C using SSE4.2 intrinsics when available
uint32_t bb_crc32c_hw(const uint8_t* data, size_t len){
#ifdef __x86_64__
    if(!have_sse42()) return 0;
    uint32_t crc = 0xffffffffu;
    // Implemented in asm/checksum_x86_64.S when available (Provide C fallback using builtin)
    #ifdef __SSE4_2__
    for(size_t i=0;i<len;i++){
        crc = __builtin_ia32_crc32qi(crc, data[i]);
    }
    return ~crc;
    #else
    // Compiled on x86_64 without intrinsics (The asm module will provide symbol if assembled)
    return 0;
    #endif
#else
    (void)data; (void)len; return 0;
#endif
}

/// ========================================================================================================================================
// END (ADD IMPLEMENTATIONS IF NECESSARY)
/// ========================================================================================================================================