/// ========================================================================================================================================
// Module     : src/bb_log.c
// Description: Logging backend for Byte-Bistro (level filtering, timestamps, file:line tagging)
// Topics     : Diagnostics, Runtime Configuration, I/O Management
// Project    : Byte Bistro (Copyright 2025)
// Purpose    : Implements bb_log_do() and defines the default global log level. Produces uniform log lines
//              with level tags, wall-clock timestamps, and source locations for reproducibility.
// Author     : Rizky Johan Saputra (Independent Project)
// Note       : ERR/WARN go to stderr and INFO/DBG go to stdout (Adjust the timestamp or formatting here if needed).
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
#include "bb_log.h"
#include <time.h>
#include <string.h>

// ======================================================================================================
// IMPLEMENTATIONS
// ======================================================================================================
// Default global log level
int bb_log_level = BB_INFO;

// Map the enum level to short tags
static const char* lvl_tag(int lvl) {
  switch (lvl) {
    case BB_ERR:  return "ERR";
    case BB_WARN: return "WRN";
    case BB_INFO: return "INF";
    default:      return "DBG";
  }
}

// Emit the single formatted log line with "[TAG] HH:MM:SS file:line: message\n"
void bb_log_do(int lvl, const char* file, int line, const char* fmt, ...) {
  FILE* out = (lvl <= BB_WARN) ? stderr : stdout;

  // Timestamp (Optional)
  struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts);
  struct tm tm; localtime_r(&ts.tv_sec, &tm);
  char tbuf[32];
  strftime(tbuf, sizeof tbuf, "%H:%M:%S", &tm);

  fprintf(out, "[%s] %s %s:%d: ", lvl_tag(lvl), tbuf, file, line);

  va_list ap; va_start(ap, fmt);
  vfprintf(out, fmt, ap);
  va_end(ap);

  fputc('\n', out);
  fflush(out);
}

/// ========================================================================================================================================
// END (ADD IMPLEMENTATIONS IF NECESSARY)
/// ========================================================================================================================================