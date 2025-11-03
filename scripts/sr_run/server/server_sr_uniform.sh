#!/usr/bin/env bash
set -euo pipefail
mkdir -p logs results
PORT=7783
echo "Starting SR uniform-cook server on :$PORT"
./byte-bistro-server-sr --port $PORT --proto sr -v --seed 123 \
  --cook-dist uniform --cook-min 10 --cook-max 120 \
  > logs/sr_uniform_server.log 2>&1
