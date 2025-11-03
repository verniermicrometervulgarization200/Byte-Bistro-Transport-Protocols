#!/usr/bin/env bash
set -euo pipefail
PORT=7777
PROTO=${PROTO:-gbn}
THREADS=${THREADS:-5}
ORDERS=${ORDERS:-50}
CFG=${CFG:-examples/configs/med-loss.toml}

./byte-bistro-server --port "$PORT" --proto "$PROTO" --kitchens 8 --channel "$CFG" > server.log 2>&1 &
SRV_PID=$!
trap 'kill $SRV_PID 2>/dev/null || true' EXIT
sleep 0.2

./byte-bistro-client --addr 127.0.0.1:$PORT --proto "$PROTO" -c "$THREADS" -n "$ORDERS" | tee client.log

echo -e "\n== Summary =="
grep -E "cli#.*(ok|timeout)" client.log | tail -n 10 || true
