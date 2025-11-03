#!/usr/bin/env bash
set -euo pipefail
mkdir -p logs results
PORT=7777
N=200
echo "Running GBN baseline client to :$PORT for $N orders"
./byte-bistro-client-gbn --addr 127.0.0.1:$PORT --proto gbn -c 1 -n $N -v \
  > logs/gbn_baseline_client.log 2>&1

# Extract client RTTs -> results/rtt_gbn_baseline_client.csv (id,rtt_ms)
echo "id,rtt_ms" > results/rtt_gbn_baseline_client.csv
awk '/ok id=/ {
  for (i=1;i<=NF;i++){
    if ($i ~ /^id=/)  {gsub(/^id=/,"",$i); id=$i}
    if ($i ~ /^rtt=/) {gsub(/^rtt=/,"",$i); gsub(/ms$/,"",$i); rtt=$i}
  }
  if (id!="" && rtt!="") print id "," rtt
}' logs/gbn_baseline_client.log >> results/rtt_gbn_baseline_client.csv

# Extract server cook times -> results/cook_gbn_baseline_server.csv (id,cook_ms)
echo "id,cook_ms" > results/cook_gbn_baseline_server.csv
awk '/served id=/ {
  # e.g. served id=3 items="..." t=40ms
  for (i=1;i<=NF;i++){
    if ($i ~ /^id=/) {gsub(/^id=/,"",$i); gsub(/[^0-9]/,"",$i); id=$i}
    if ($i ~ /^t=/)  {gsub(/^t=/,"",$i);  gsub(/ms$/,"",$i);      t=$i}
  }
  if (id!="" && t!="") print id "," t
}' logs/gbn_baseline_server.log >> results/cook_gbn_baseline_server.csv

echo "Done: logs/gbn_baseline_*.log, results/rtt_gbn_baseline_client.csv, results/cook_gbn_baseline_server.csv"
