/// ========================================================================================================================================
// Module     : include/bb_log.h
// Description: Lightweight logging facade with levels and file:line tagging for Byte-Bistro
// Topics     : Observability, Diagnostics, Systems Debugging, API Design
// Project    : Byte Bistro (Copyright 2025)
// Purpose    : Declares a tiny logging API with compile-time-friendly macro dispatch, level filtering,
//              and consistent prefixes for reproducible experiments and debugging.
// Author     : Rizky Johan Saputra (Independent Project)
// Note       : Keep this header minimal. The BBLOG() macro routes to bb_log_do() only when level<=bb_log_level.
//              Default global level is set in bb_log.c; callers may change it at runtime.
/// ========================================================================================================================================

// ======================================================================================================
// SPECIFICATIONS
// ======================================================================================================
// - Levels (enum bb_loglvl):
//     - BB_ERR (0)  : Fatal and critical (Prints to stderr)
//     - BB_WARN (1) : Warnings (Prints to stderr)
//     - BB_INFO (2) : High-level progress (Prints to stdout)
//     - BB_DBG (3)  : Verbose debugging (Prints to stdout)
// - Global Control:
//     - Extern int bb_log_level and set at runtime (e.g., BB_INFO or BB_DBG) with a default within BB_INFO.
// - API:
//     - BBLOG(level, fmt, ...) : Emits "[TAG] HH:MM:SS file:line: message" if level <= bb_log_level
//     - bb_log_do(...)         : Internal backend (Do not call directly).
// - Threading:
//     - Logging uses stdio; outputs are line-buffered with fflush(). For heavy concurrency, consider a mutex or
//       redirecting to per-thread buffers (not required for current single-threaded Byte-Bistro demos).
// - Performance:
//     - When filtered out by level, BBLOG() is a cheap branch and avoids formatting cost.
// - Portability:
//     - Uses standard C/POSIX time functions for timestamps and can be simplified if needed.

// ======================================================================================================
// SETUP (ADJUST IF NECESSARY)
// ======================================================================================================
#ifndef BB_LOG_H
#define BB_LOG_H
#include "bb_common.h"
#include <stdarg.h>
#include <stdio.h>

// ======================================================================================================
// IMPLEMENTATIONS
// ======================================================================================================
#ifdef __cplusplus
extern "C" {
#endif

// Log levels
enum bb_loglvl { BB_ERR = 0, BB_WARN = 1, BB_INFO = 2, BB_DBG = 3 };

// Global log level (defined in bb_log.c)
extern int bb_log_level;

// Internal logging system
void bb_log_do(int lvl, const char* file, int line, const char* fmt, ...);

// Macros logging system
#define BBLOG(lvl, ...) \
  do { \
    if ((lvl) <= bb_log_level) { \
      bb_log_do((lvl), __FILE__, __LINE__, __VA_ARGS__); \
    } \
  } while (0)

#ifdef __cplusplus
} // extern "C"
#endif
#endif // BB_LOG_H

/// ========================================================================================================================================
// END (ADD IMPLEMENTATIONS IF NECESSARY)
/// ========================================================================================================================================