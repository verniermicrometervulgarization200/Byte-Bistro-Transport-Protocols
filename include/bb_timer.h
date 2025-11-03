/// ========================================================================================================================================
// Module     : include/bb_timer.h
// Description: Minimal monotonic deadline timer utilities for Byte-Bistro (inline, zero-allocation)
// Topics     : Sequential Timers, Multi-Threaded Scheduling, RTOs, Transport Protocols
// Project    : Byte Bistro (Copyright 2025)
// Purpose    : Provides tiny inline RTO / timeout helpers used by SR and GBN transports. Designed to be
//              extremely cheap and dependency-free — no threads, no events, no heap. Pure timestamp math.
// Author     : Rizky Johan Saputra (Independent Project)
// Note       : bb_timer_t is intentionally tiny — one deadline_ns + one armed bit. Caller controls drive
//              frequency. This aligns with selective retransmission controlled by recv()-driven liveness.
/// ========================================================================================================================================

// ======================================================================================================
// SPECIFICATIONS
// ======================================================================================================
// - Semantics:
//     - arm(timeout_ms) is configured by setting armed=true with deadline_ns being computed through bb_now_ns()
//     - disarm() is configured by setting armed=false
//     - expired() is true iff armed AND now>=deadline_ns
//     - remaining_ms() measures how many ms remain, whilst returning 0 if disarmed or expired
// - Performance:
//     - fully inline as there are no syscall except bb_now_ns(), cheap for high-frequency checks
// - Safety:
//     - Caller must reset or disarm before reuse. No memory allocation inside.

// ======================================================================================================
// SETUP (ADJUST IF NECESSARY)
// ======================================================================================================
#ifndef BB_TIMER_H
#define BB_TIMER_H
#include "bb_common.h"

// ======================================================================================================
// IMPLEMENTATIONS
// ======================================================================================================
// Transport the configurations (Deadline and arm)
typedef struct { uint64_t deadline_ns; bool armed; } bb_timer_t;

// Declare the arm timer and compute the deadline
static inline void bb_timer_arm(bb_timer_t* t, uint64_t timeout_ms){
  t->deadline_ns = bb_now_ns() + timeout_ms*1000000ull; t->armed=true;
}

// Disarm the timer (Treat as inactive and ensure expiration checks return false)
static inline void bb_timer_disarm(bb_timer_t* t){ t->armed=false; }

// Return true if timer is armed and monotonic time has passed the deadline
static inline bool bb_timer_expired(const bb_timer_t* t){
  return t->armed && bb_now_ns() >= t->deadline_ns;
}

// Return the remaining time until reaching its deadline (Declare 0 if disarmed or already expired)
static inline uint64_t bb_timer_remaining_ms(const bb_timer_t* t){
  if(!t->armed) return 0;
  uint64_t now=bb_now_ns();
  return (now>=t->deadline_ns)?0: (t->deadline_ns-now)/1000000ull;
}
#endif // BB_TIMER_H

/// ========================================================================================================================================
// END (ADD IMPLEMENTATIONS IF NECESSARY)
/// ========================================================================================================================================