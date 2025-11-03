/// ========================================================================================================================================
// Module       : src/bb_common.c
// Description  : Minimal POSIX utility implementations for Byte-Bistro (nonblocking toggle, etc.)
// Topics       : POSIX I/O, File Descriptors, Error Handling
// Project      : Byte Bistro (Copyright 2025)
// Purpose      : Implements small cross-module helpers declared in bb_common.h. Kept tiny to avoid link bloat.
// Author       : Rizky Johan Saputra (Independent Project)
// Note         : Only place non-inline and non-templated helpers here. Prefer header-inline for hot-path utilities.
/// ========================================================================================================================================

// ======================================================================================================
// SPECIFICATIONS
// ======================================================================================================
// - Macros:
//     - BB_CHECK(x)     : Aborts the process with file:line on failed invariant (Fatal precondition).
//     - BB_TRY(expr,msg): Ff (expr) < 0, then return perror(msg) and exit (Intended for unrecoverable syscalls).
//     - BB_ARRAY_LEN(a) : Compile-time-safe array length for C arrays (not pointers).
// - Time Utilities (monotonic base):
//     - bb_now_ns() : Returns CLOCK_MONOTONIC nanoseconds as uint64_t.
//     - bb_ms(ns)   : Converts nanoseconds into milliseconds (Truncating).
// - I/O Utilities:
//     - bb_set_nonblock(fd, enable): Toggles O_NONBLOCK, where it returns 0 on success, -1 on error.
// - Portability:
//     - Requires POSIX (clock_gettime, fcntl, sockets). On non-POSIX platforms, provide shims or guards.
// - Safety:
//     - Inline utilities are side-effect free, where macros are single-evaluation style in guarded blocks.
//     - Retain the macros constrained to debugging/guarding use. Thus, do not abuse for control flow.

// ======================================================================================================
// SETUP (ADJUST IF NECESSARY)
// ======================================================================================================
#include "bb_common.h"
#include <fcntl.h>

// ======================================================================================================
// IMPLEMENTATIONS
// ======================================================================================================
// Toggle the O_NONBLOCK flag for a file descriptor with 0 on success and -1 on error
int bb_set_nonblock(int fd, bool enable){
  int flags = fcntl(fd, F_GETFL, 0); if(flags<0) return -1;
  if(enable) flags |= O_NONBLOCK; else flags &= ~O_NONBLOCK;
  return fcntl(fd, F_SETFL, flags);
}

/// ========================================================================================================================================
// END (ADD IMPLEMENTATIONS IF NECESSARY)
/// ========================================================================================================================================