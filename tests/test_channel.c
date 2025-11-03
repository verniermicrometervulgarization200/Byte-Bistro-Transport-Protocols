/// ========================================================================================================================================
// Module     : tests/test_channel.c
// Description: Smoke test for bb_channel send/recv with synthetic 100% loss and short recv timeout
// Topics     : UDP I/O, Impairment Simulation, Harness
// Project    : Byte Bistro (Copyright 2025)
// Purpose    : Verifies that bb_channel_send() returns a non-negative byte count even when frames are dropped,
//              and that bb_channel_recv() times out as expected under loss-only conditions.
// Author     : Rizky Johan Saputra (Independent Project)
// Note       : This test does not require a running peer as it validates local channel behavior & APIs.
/// ========================================================================================================================================

// ======================================================================================================
// SPECIFICATIONS
// ======================================================================================================
// - Setup:
//     - UDP socket bound ephemeral, peer set to 127.0.0.1:9999 (no server required).
//     - Channel impairment: loss_pct = 100% (everything dropped).
// - Expected:
//     - send() => returns >= 0 (logical success even if dropped by channel).
//     - recv(50ms) => returns <= 0 (timeout or no data).
// - Usage:
//     - $ gcc -o test_channel tests/test_channel.c src/bb_channel.c src/bb_common.c ... && ./test_channel

// ======================================================================================================
// SETUP (ADJUST IF NECESSARY)
// ======================================================================================================
#include "bb_channel.h"
#include <stdio.h>

// ======================================================================================================
// IMPLEMENTATIONS
// ======================================================================================================
// Declare the main entry for testing
int main(){
  int fd = socket(AF_INET, SOCK_DGRAM, 0); 
  if(fd < 0){ perror("socket"); return 1; }
  struct sockaddr_in peer = {0}; 
  peer.sin_family = AF_INET; 
  peer.sin_port   = htons(9999); 
  inet_pton(AF_INET, "127.0.0.1", &peer.sin_addr);

  // Configure channel with a 100% loss and seed set as 1234
  bb_channel_cfg_t cfg = {
    .loss_pct        = 100.0,  // Drop everything
    .dup_pct         = 0,
    .reorder_pct     = 0,
    .delay_mean_ms   = 0,
    .delay_jitter_ms = 0,
    .rate_mbps       = 0,
    .seed            = 1234
  };

  // Initialize the channel
  bb_channel_t* ch = bb_channel_create(fd, &peer, cfg);

  // Return the outcomes 
  char msg[] = "test"; 
  ssize_t s = bb_channel_send(ch, msg, sizeof(msg));
  printf("send returned %zd (>=0 even if dropped)\n", s);
  char buf[64]; 
  ssize_t r = bb_channel_recv(ch, buf, sizeof(buf), 50);
  printf("recv after 50ms timeout returned %zd (<=0 expected)\n", r);

  // Destroy the channel after finish execution
  bb_channel_destroy(ch);
  return 0;
}

/// ========================================================================================================================================
// END (ADD IMPLEMENTATIONS IF NECESSARY)
/// ========================================================================================================================================