#!/usr/bin/env bash
set -euo pipefail
mkdir -p logs results
PORT=7778
echo "Starting GBN impaired server on :$PORT"
./byte-bistro-server-gbn --port $PORT --proto gbn -v \
  --loss 10 --dup 5 --reorder 10 --dmean 20 --djitter 10 --rate 50 --seed 123 \
  > logs/gbn_impaired_server.log 2>&1
