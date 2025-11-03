#!/usr/bin/env bash
set -euo pipefail
mkdir -p logs results
PORT=7784
echo "Starting SR exp-cook server on :$PORT"
./byte-bistro-server-sr --port $PORT --proto sr -v --seed 123 \
  --cook-dist exp --cook-mean 60 \
  > logs/sr_exp_server.log 2>&1
