/// ========================================================================================================================================
// Module     : src/bb_sr.c
// Description: Selective Repeat (SR) reliable transport over Byte-Bistro channel (per-packet timers & reordering)
// Topics     : Data Communications, Sliding Window, Sequential Timers, Selective ACKs, Systems Reliability
// Project    : Byte Bistro (Copyright 2025)
// Purpose    : Implements SR with per-sequence buffering/timers. Sender retransmits only timed-out packets.
//              Receiver buffers out-of-order, delivers the in-order prefix, and advertises cumulative ACK(rcv_nxt).
// Author     : Rizky Johan Saputra (Independent Project)
// Note       : Depends on bb_wire.* (pack/parse), bb_timer.* (per-packet timers), and bb_channel.* (lossy link).
//              Conforms to bb_proto.h so callers remain transport-agnostic.
/// ========================================================================================================================================

// ======================================================================================================
// SPECIFICATIONS
// ======================================================================================================
// - Window & Sequence:
//     - Sender window = cfg.wnd (clamped to BB_MAX_WND). Sequence arithmetic uses uint32_t wrap semantics.
// - ACK Policy:
//     - Cumulative ACK on header. consume_ack_any() processes ACK in both pure-ACK and piggybacked DATA headers.
// - Timers / Retransmit:
//     - Per-packet timers (bb_timer_t). Expiry triggers targeted retransmit of that sequence only.
// - Fragmentation & Send:
//     - send() fragments payload into chunks<=cfg.mss; keeps sending while window has space. Nonblocking polls for ACKs
//       and performs timer-driven retransmissions during send and a final drain phase.
// - Receive & Reassembly:
//     - recv() buffers out-of-order frames within window, delivers in-order prefix, and advances rcv_nxt; ACKs next-needed.
// - Safety & Limits:
//     - Buffers are per-window-slot; all heap allocations are bounded by MSS and window size. Defaults if cfg fields are 0:
//       wnd=32 (≤BB_MAX_WND), mss=512, rto=120ms.
// - Observability:
//     - Uses bb_log if needed (optional); wire-level logging occurs in the channel layer.

// ======================================================================================================
// SETUP (ADJUST IF NECESSARY)
// ======================================================================================================
#include "bb_sr.h"
#include "bb_log.h"
#include "bb_timer.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#define BB_MAX_WND 256

// ======================================================================================================
// IMPLEMENTATIONS
// ======================================================================================================
// Transport the configurations with the SR states
typedef struct {
  bb_channel_t*  ch;
  bb_proto_cfg_t cfg;
  uint32_t snd_una, snd_nxt;  // Send window [snd_una, snd_nxt)
  uint32_t rcv_nxt;           // Next in-order seq to be expected

  // Send side state (Utilize per-packet timers and buffers)
  struct {
    uint8_t  *bufs[BB_MAX_WND];
    uint16_t  lens[BB_MAX_WND];
    bb_timer_t timers[BB_MAX_WND];
    bool      inuse[BB_MAX_WND];
  } tx;

  // Receive side buffer for out-of-order frames
  struct {
    uint8_t  *bufs[BB_MAX_WND];
    uint16_t  lens[BB_MAX_WND];
    bool      present[BB_MAX_WND];
  } rx;
} sr_t;

// Signed comparison with the results in modulo-2^32 space (returns <0, 0, >0)
static inline int32_t seq_cmp(uint32_t a, uint32_t b){ return (int32_t)(a - b); }

// Declare the absolute seq mapping into its circular index within the corresponding window
static inline uint32_t wrap_idx(uint32_t x, uint32_t wnd){ return (wnd? (x % wnd) : 0); }

// Send an ACK for the next-needed (rcv_nxt)
static void send_ack(sr_t* s){
  uint8_t abuf[64];
  size_t an = bb_wire_pack(abuf, sizeof(abuf), BB_FLAG_ACK, 0, s->rcv_nxt, NULL, 0);
  (void)bb_channel_send(s->ch, abuf, an);
}

// Encode and send a single data frame with the specified seq and payload
static int send_frame(sr_t* s, uint32_t seq, const uint8_t* p, size_t n){
  uint8_t buf[1600];
  size_t m = bb_wire_pack(buf, sizeof(buf), BB_FLAG_DATA, seq, s->rcv_nxt, p, (uint16_t)n);
  if (m == 0) return -1;
  if (bb_channel_send(s->ch, buf, m) < 0) return -1;
  return 0;
}

// Consume the cumulative ACK from any header (Disarming timers and free slots).
static void consume_ack_any(sr_t* s, const bb_hdr_t* h){
  const uint32_t W = s->cfg.wnd;

  // Accept acks in the state of [snd_una .. snd_nxt]
  if (seq_cmp(h->ack, s->snd_una) >= 0 && seq_cmp(h->ack, s->snd_nxt) <= 0){
    while (seq_cmp(s->snd_una, h->ack) < 0){
      uint32_t i = wrap_idx(s->snd_una, W);
      if (s->tx.inuse[i]){
        free(s->tx.bufs[i]);
        s->tx.bufs[i] = NULL;
        s->tx.inuse[i] = false;
        bb_timer_disarm(&s->tx.timers[i]);
      }
      s->snd_una++;
    }
  }
}

// Declare and initialize an SR transport with its defaults and clamp window
bb_proto_t* bb_proto_sr_create(bb_channel_t* ch, bb_proto_cfg_t cfg){
  // Clamp the window and its defaults
  if (cfg.wnd == 0) cfg.wnd = 32;
  if (cfg.wnd > BB_MAX_WND) cfg.wnd = BB_MAX_WND;
  if (cfg.mss == 0) cfg.mss = 512;
  if (cfg.rto_ms == 0) cfg.rto_ms = 120;
  sr_t* s = (sr_t*)calloc(1, sizeof(*s));
  if (!s) return NULL;
  s->ch = ch; s->cfg = cfg;

  // Start with the empty flight and localized both at init_seq
  s->snd_una = cfg.init_seq;
  s->snd_nxt = cfg.init_seq;
  s->rcv_nxt = cfg.init_seq;
  return (bb_proto_t*)s;
}

// Declare the fragmentation and send the per-packet timers with poll acks.
int bb_proto_send(bb_proto_t* v, const uint8_t* data, size_t len){
  sr_t* s = (sr_t*)v;
  const uint32_t W = s->cfg.wnd;
  size_t off = 0;
  while (off < len){
    // Declare the non-blocking poll (Process ANY header's ack through ACK-only or piggyback datas)
    {
      uint8_t ibuf[2048];
      ssize_t rn = bb_channel_recv(s->ch, ibuf, sizeof(ibuf), 0);
      if (rn > 0){
        bb_hdr_t h; const uint8_t* pl;
        if (bb_wire_parse(ibuf, rn, &h, &pl)){
          consume_ack_any(s, &h);
        }
      }
    }

    // Retransmit any expired in-flight packets
    for (uint32_t q = s->snd_una; seq_cmp(s->snd_nxt, q) > 0; ++q){
      uint32_t i = wrap_idx(q, W);
      if (s->tx.inuse[i] && bb_timer_expired(&s->tx.timers[i])){
        (void)send_frame(s, q, s->tx.bufs[i], s->tx.lens[i]);
        bb_timer_arm(&s->tx.timers[i], s->cfg.rto_ms);
      }
    }

    // Declare a backoff call when flight window is full
    if ((uint32_t)(s->snd_nxt - s->snd_una) >= W){
      struct timespec ts = {0, 1000000}; // 1 ms
      nanosleep(&ts, NULL);
      continue;
    }

    // Send the preceeding chunk
    size_t chunk = (len - off > s->cfg.mss) ? s->cfg.mss : (len - off);
    if (send_frame(s, s->snd_nxt, data + off, chunk) < 0) return -1;

    uint32_t i = wrap_idx(s->snd_nxt, W);
    if (s->tx.inuse[i]){ // Extra measure (Shouldn't happen normally, but ensure safety)
      free(s->tx.bufs[i]);
      s->tx.inuse[i] = false;
    }
    s->tx.bufs[i] = (uint8_t*)malloc(chunk);
    memcpy(s->tx.bufs[i], data + off, chunk);
    s->tx.lens[i]   = (uint16_t)chunk;
    s->tx.inuse[i]  = true;
    bb_timer_arm(&s->tx.timers[i], s->cfg.rto_ms);

    s->snd_nxt++;
    off += chunk;
  }

  // Drain until the system is fully acked (Consume acks on any header)
  while (seq_cmp(s->snd_una, s->snd_nxt) < 0){
    uint8_t ibuf[2048];
    ssize_t rn = bb_channel_recv(s->ch, ibuf, sizeof(ibuf), s->cfg.rto_ms);
    if (rn > 0){
      bb_hdr_t h; const uint8_t* pl;
      if (bb_wire_parse(ibuf, rn, &h, &pl)){
        consume_ack_any(s, &h);
      }
    }

    // Maintain the retransmittance for any timeouts during draining
    for (uint32_t q = s->snd_una; seq_cmp(s->snd_nxt, q) > 0; ++q){
      uint32_t i = wrap_idx(q, W);
      if (s->tx.inuse[i] && bb_timer_expired(&s->tx.timers[i])){
        (void)send_frame(s, q, s->tx.bufs[i], s->tx.lens[i]);
        bb_timer_arm(&s->tx.timers[i], s->cfg.rto_ms);
      }
    }
  }
  return 0;
}

// Receive the buffer in out-of-order through the window and deliver in-order prefix for the preceeding acks
ssize_t bb_proto_recv(bb_proto_t* v, uint8_t* out, size_t cap, int timeout_ms){
  sr_t* s = (sr_t*)v;
  const uint32_t W = s->cfg.wnd;
  uint8_t ibuf[2048];
  ssize_t rn = bb_channel_recv(s->ch, ibuf, sizeof(ibuf), timeout_ms);
  if (rn <= 0) return rn;
  bb_hdr_t h; const uint8_t* pl;
  if (!bb_wire_parse(ibuf, rn, &h, &pl)) return 0;

  // Always consume the peer's cumulative ack first (Piggyback or pure)
  consume_ack_any(s, &h);

  if (h.flags & BB_FLAG_DATA){
    // Declare a dropping measure if the outside receive any window
    if (seq_cmp(h.seq, s->rcv_nxt + W) >= 0){ send_ack(s); return 0; }
    if (seq_cmp(h.seq, s->rcv_nxt) <  0){ send_ack(s); return 0; }
    uint32_t i = wrap_idx(h.seq, W);
    if (!s->rx.present[i]){
      s->rx.bufs[i] = (uint8_t*)malloc(h.len);
      memcpy(s->rx.bufs[i], pl, h.len);
      s->rx.lens[i]    = (uint16_t)h.len;
      s->rx.present[i] = true;
    }

    // Deliver the head-of-line
    if (h.seq == s->rcv_nxt){
      size_t n = s->rx.lens[i];
      if (n > cap) n = cap;
      memcpy(out, s->rx.bufs[i], n);
      free(s->rx.bufs[i]); s->rx.bufs[i] = NULL; s->rx.present[i] = false; s->rcv_nxt++;

      // Drain the consecutive buffered segments
      while (s->rx.present[wrap_idx(s->rcv_nxt, W)]){
        uint32_t j = wrap_idx(s->rcv_nxt, W);
        free(s->rx.bufs[j]); s->rx.bufs[j] = NULL; s->rx.present[j] = false; s->rcv_nxt++;
      }
      send_ack(s);
      return (ssize_t)n;
    }

    // Ensure that the gap remains and still ACK is sent accordingly
    send_ack(s);
  }
  return 0;
}

// Release the SR resources (per-slot tx/rx buffers) and its corresponding instance
void bb_proto_close(bb_proto_t* v){
  sr_t* s = (sr_t*)v;
  const uint32_t W = s->cfg.wnd;
  for (uint32_t i = 0; i < W; i++){
    if (s->tx.inuse[i])   free(s->tx.bufs[i]);
    if (s->rx.present[i]) free(s->rx.bufs[i]);
  }
  free(s);
}

/// ========================================================================================================================================
// END (ADD IMPLEMENTATIONS IF NECESSARY)
/// ========================================================================================================================================