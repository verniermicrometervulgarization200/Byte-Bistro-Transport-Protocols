#!/usr/bin/env bash
set -euo pipefail
mkdir -p logs results
PORT=7780
echo "Starting GBN exp-cook server on :$PORT"
./byte-bistro-server-gbn --port $PORT --proto gbn -v --seed 123 \
  --cook-dist exp --cook-mean 60 \
  > logs/gbn_exp_server.log 2>&1
