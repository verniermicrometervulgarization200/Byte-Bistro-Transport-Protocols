#!/usr/bin/env bash
set -euo pipefail
mkdir -p logs results
PORT=7781
echo "Starting SR baseline server on :$PORT"
./byte-bistro-server-sr --port $PORT --proto sr -v --seed 123 \
  > logs/sr_baseline_server.log 2>&1
