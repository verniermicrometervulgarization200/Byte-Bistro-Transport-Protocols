#!/usr/bin/env bash
set -euo pipefail
mkdir -p logs results
PORT=7782
echo "Starting SR impaired server on :$PORT"
./byte-bistro-server-sr --port $PORT --proto sr -v \
  --loss 10 --dup 5 --reorder 10 --dmean 20 --djitter 10 --rate 50 --seed 123 \
  > logs/sr_impaired_server.log 2>&1
