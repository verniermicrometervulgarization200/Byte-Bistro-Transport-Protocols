#!/usr/bin/env bash
set -euo pipefail
mkdir -p logs results
PORT=7779
echo "Starting GBN uniform-cook server on :$PORT"
./byte-bistro-server-gbn --port $PORT --proto gbn -v --seed 123 \
  --cook-dist uniform --cook-min 10 --cook-max 120 \
  > logs/gbn_uniform_server.log 2>&1
