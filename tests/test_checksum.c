/// ========================================================================================================================================
// Module     : tests/test_checksum.c
// Description: Sanity test for Fletcher-32 and optional CRC32C(HW) implementations
// Topics     : Checksums, CRC32C, SSE4.2 Features
// Project    : Byte Bistro (Copyright 2025)
// Purpose    : Prints Fletcher-32 for a known string and attempts CRC32C(HW) if available; otherwise notes absence.
// Author     : Rizky Johan Saputra (Independent Project)
// Note       : CRC32C(HW) requires SSE4.2. For non-x86_64 or CPUs without SSE4.2, function returns 0 by design.
/// ========================================================================================================================================

// ======================================================================================================
// SPECIFICATIONS
// ======================================================================================================
// - Inputs:
//     - Literal test string "hello world".
// - Expected Output (example):
//     - fletcher32(hello world)=0xXXXXXXXX  (Deterministic across platforms)
//     - crc32c_hw not available
//     - crc32c_hw(hello world)=0xYYYYYYYY
// - Usage:
//     - $ gcc -msse4.2 -o test_checksum tests/test_checksum.c src/bb_checksum.c src/bb_common.c && ./test_checksum
//     - Omit the -msse4.2 if only Fletcher-32 is utilized (CRC path may still work if compiler/CPU support it).

// ======================================================================================================
// SETUP (ADJUST IF NECESSARY)
// ======================================================================================================
#include "bb_checksum.h"
#include <stdio.h>
#include <string.h>

// ======================================================================================================
// IMPLEMENTATIONS
// ======================================================================================================
// Declare the main entry for testing
int main(){
  const char* s = "hello world";

  // Print Fletcher32
  uint32_t f = bb_fletcher32((const uint8_t*)s, strlen(s));
  printf("fletcher32(hello world)=0x%08x\n", f);

  // Print CRC32C(HW)
  uint32_t c = bb_crc32c_hw((const uint8_t*)s, strlen(s));
  if (c == 0) 
    printf("crc32c_hw not available (ok)\n");
  else 
    printf("crc32c_hw(hello world)=0x%08x\n", c);
  return 0;
}

/// ========================================================================================================================================
// END (ADD IMPLEMENTATIONS IF NECESSARY)
/// ========================================================================================================================================