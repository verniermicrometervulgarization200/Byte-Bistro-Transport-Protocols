#!/usr/bin/env bash
set -euo pipefail
mkdir -p logs results
PORT=7779
N=200
echo "Running GBN uniform-cook client to :$PORT for $N orders"
./byte-bistro-client-gbn --addr 127.0.0.1:$PORT --proto gbn -c 1 -n $N -v \
  > logs/gbn_uniform_client.log 2>&1

echo "id,rtt_ms" > results/rtt_gbn_uniform_client.csv
awk '/ok id=/ {for(i=1;i<=NF;i++){if($i~/^id=/){gsub(/^id=/,"",$i);id=$i} if($i~/^rtt=/){gsub(/^rtt=/,"",$i);gsub(/ms$/,"",$i);rtt=$i}} if(id!=""&&rtt!="") print id","rtt}' \
  logs/gbn_uniform_client.log >> results/rtt_gbn_uniform_client.csv

echo "id,cook_ms" > results/cook_gbn_uniform_server.csv
awk '/served id=/ {for(i=1;i<=NF;i++){if($i~/^id=/){gsub(/^id=/,"",$i);gsub(/[^0-9]/,"",$i);id=$i} if($i~/^t=/){gsub(/^t=/,"",$i);gsub(/ms$/,"",$i);t=$i}} if(id!=""&&t!="") print id","t}' \
  logs/gbn_uniform_server.log >> results/cook_gbn_uniform_server.csv

echo "Done: logs/gbn_uniform_*.log and CSVs in results/"
