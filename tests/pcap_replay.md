# PCAP Replay (Design Sketch)

Future extension to drive `bb_channel` from a TUN/TAP or libpcap source to reproduce real loss/reorder patterns.

1. Read a `.pcap` and extract timestamps/directions.
2. Convert into `bb_channel` scheduling decisions (drop/dup/delay).
3. Compare GBN vs SR under *identical* impairment traces.

This doc outlines packet timing normalization and mapping. Implementation TBD.