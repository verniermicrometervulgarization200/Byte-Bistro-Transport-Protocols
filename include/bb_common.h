/// ========================================================================================================================================
// Module       : include/bb_common.h
// Description  : Common headers, helpers, timing utilities, and safety macros for Byte-Bistro
// Topics       : Systems Programming, POSIX, Timing, Error Handling, Portability
// Project      : Byte Bistro (Copyright 2025)
// Purpose      : Centralizes lightweight utilities (monotonic clock, array len, nonblocking I/O toggles) and
//                defensive macros used across the codebase to keep other modules focused and consistent.
// Author       : Rizky Johan Saputra (Independent Project)
// Note         : Keep this header minimal and dependency-light. Functions here should be inline/leaf-level and
//                free of heavy transitive includes to avoid compile-time bloat and dependency cycles.
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
#ifndef BB_COMMON_H
#define BB_COMMON_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define BB_CHECK(x) do { if(!(x)) { fprintf(stderr, "[FATAL] %s:%d %s\n", __FILE__, __LINE__, #x); exit(EXIT_FAILURE);} } while(0)
#define BB_TRY(expr,msg) do { if((expr) < 0){ perror(msg); exit(EXIT_FAILURE);} } while(0)
#define BB_ARRAY_LEN(a) ((int)(sizeof(a)/sizeof((a)[0])))

// ======================================================================================================
// IMPLEMENTATIONS
// ======================================================================================================
// Declare a monotonic time feature in nanoseconds. (Strictly relies on CLOCK_MONOTONIC)
static inline uint64_t bb_now_ns(void){ struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return (uint64_t)ts.tv_sec*1000000000ull+ts.tv_nsec; }

// Convert nanoseconds into milliseconds
static inline uint64_t bb_ms(uint64_t ns){ return ns/1000000ull; }

// Toggle the nonblocking mode on a file descriptor (0 on success, -1 on error; errno set)
int bb_set_nonblock(int fd, bool enable);
#endif

/// ========================================================================================================================================
// END (ADD IMPLEMENTATIONS IF NECESSARY)
/// ========================================================================================================================================