#!/usr/bin/env bash
# Portable (macOS Bash 3.2 OK) runner for all scenarios.

set -euo pipefail

# ================================
# Config (you can tweak these)
# ================================
ORDERS="${1:-200}"
SEED=123

# Impaired channel profile
LOSS=10
DUP=5
REORDER=10
DMEAN=20
DJITTER=10
RATE=50

# Cook distributions (server-side)
COOK_FIXED="fixed:40"
COOK_UNI="uniform:20:120"
COOK_EXP="exp:60:10:250"

mkdir -p logs results

# ================================
# Helpers (no associative arrays)
# ================================
cook_kind() {
  # echo fixed|uniform|exp from a full cook string
  case "$1" in
    fixed:*)   echo "fixed" ;;
    uniform:*) echo "uniform" ;;
    exp:*)     echo "exp" ;;
    *)         echo "fixed" ;;
  esac
}

get_port() {
  # Map (proto, mode, cookkind) -> distinct port
  # GBN: 7777..7780, SR: 7787..7790
  proto="$1"; mode="$2"; cookkind="$3"
  if [ "$proto" = "gbn" ]; then
    case "${mode}_${cookkind}" in
      baseline_fixed)  echo 7777 ;;
      impaired_fixed)  echo 7778 ;;
      impaired_uniform) echo 7779 ;;
      impaired_exp)    echo 7780 ;;
      *)               echo 7777 ;;
    esac
  else
    case "${mode}_${cookkind}" in
      baseline_fixed)  echo 7787 ;;
      impaired_fixed)  echo 7788 ;;
      impaired_uniform) echo 7789 ;;
      impaired_exp)    echo 7790 ;;
      *)               echo 7787 ;;
    esac
  fi
}

sanitize_label() {
  # turn "exp:60:10:250" -> "exp_60_10_250"
  echo "$1" | sed 's/:/_/g' | sed 's/,/_/g'
}

now_ms() {
  # portable-ish timestamp (milliseconds) via Python or date fallback
  python - "$@" <<'PY'
import time,sys
print(int(time.time()*1000))
PY
}

run_case() {
  proto="$1"       # gbn|sr
  mode="$2"        # baseline|impaired
  cook="$3"        # fixed:40 | uniform:... | exp:...

  cookkind="$(cook_kind "$cook")"
  port="$(get_port "$proto" "$mode" "$cookkind")"
  cook_lbl="$(sanitize_label "$cook")"
  label="${proto}_${mode}_${cook_lbl}"

  if [ "$proto" = "gbn" ]; then
    server_bin=./byte-bistro-server-gbn
    client_bin=./byte-bistro-client-gbn
  else
    server_bin=./byte-bistro-server-sr
    client_bin=./byte-bistro-client-sr
  fi

  slog="logs/${label}_server.log"
  clog="logs/${label}_client.log"
  client_csv="results/rtt_client_${label}.csv"
  server_csv="results/cook_server_${label}.csv"
  client_csv_fromlog="results/rtt_client_fromlog_${label}.csv"

  echo
  echo "=== Running $proto | $mode | cook=$cook | port=$port | orders=$ORDERS ==="

  # Build server command
  cmd=( "$server_bin" --port "$port" --proto "$proto" -v --seed "$SEED" --cook-dist "$cook" )
  if [ "$mode" = "impaired" ]; then
    cmd+=( --loss "$LOSS" --dup "$DUP" --reorder "$REORDER" --dmean "$DMEAN" --djitter "$DJITTER" --rate "$RATE" )
  fi

  # Start server (background)
  "${cmd[@]}" >"$slog" 2>&1 & echo $! > .srvpid
  sleep 0.4

  # Run client (its own CSV + log)
  "$client_bin" --addr "127.0.0.1:$port" --proto "$proto" -c 1 -n "$ORDERS" -v \
    --rtt-csv "$client_csv" --log "$clog" || true

  # Stop server
  if [ -f .srvpid ]; then
    kill "$(cat .srvpid)" 2>/dev/null || true
    rm -f .srvpid
  fi

  # Extract server cook times -> CSV (order_id,cook_ms)
  {
    echo "order_id,cook_ms"
    awk '/served id=/ {
      id=""; t="";
      for (i=1;i<=NF;i++) {
        if ($i ~ /^id=/) { gsub("id=","",$i); id=$i }
        if ($i ~ /^t=/)  { gsub("t=","",$i); gsub("ms","",$i); t=$i }
      }
      if (id != "" && t != "") { print id "," t }
    }' "$slog"
  } > "$server_csv"

  # Also extract client RTTs from client log (fallback)
  {
    echo "order_id,rtt_ms"
    awk '/ ok id=/ {
      id=""; rtt="";
      for (i=1;i<=NF;i++) {
        if ($i ~ /^id=/)  { gsub("id=","",$i); id=$i }
        if ($i ~ /^rtt=/) { gsub("rtt=","",$i); gsub("ms","",$i); rtt=$i }
      }
      if (id != "" && rtt != "") { print id "," rtt }
    }' "$clog"
  } > "$client_csv_fromlog"

  echo "→ Server log:   $slog"
  echo "→ Client log:   $clog"
  echo "→ Client RTT:   $client_csv"
  echo "→ Client RTT†:  $client_csv_fromlog (parsed from log)"
  echo "→ Server cook:  $server_csv"
}

# ================================
# Matrix of runs
# ================================
# GBN
run_case gbn baseline "$COOK_FIXED"
run_case gbn impaired "$COOK_FIXED"
run_case gbn impaired "$COOK_UNI"
run_case gbn impaired "$COOK_EXP"

# SR
run_case sr baseline "$COOK_FIXED"
run_case sr impaired "$COOK_FIXED"
run_case sr impaired "$COOK_UNI"
run_case sr impaired "$COOK_EXP"

echo
echo "All done. CSVs in results/, logs in logs/."
echo "Example plots:"
echo "  python3 scripts/plot_rtt.py results/rtt_client_gbn_baseline_fixed_40.csv results/rtt_client_sr_baseline_fixed_40.csv --out-prefix results/rtt_baseline_fixed"
echo "  python3 scripts/plot_rtt.py results/rtt_client_gbn_impaired_exp_60_10_250.csv results/rtt_client_sr_impaired_exp_60_10_250.csv --out-prefix results/rtt_impaired_exp"
