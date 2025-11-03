#!/usr/bin/env bash
set -euo pipefail
mkdir -p logs results
PORT=7777
echo "Starting GBN baseline server on :$PORT"
./byte-bistro-server-gbn --port $PORT --proto gbn -v --seed 123 \
  > logs/gbn_baseline_server.log 2>&1
