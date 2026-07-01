#!/usr/bin/env bash
# Extended-timeout runs for the slow conditions (c4, c5) — the standard 420s cap
# cannot contain the journey at <=4 Mbps. Usage: _netmatrix_slow.sh [c4] [c5]
set -u
cd /home/free/code/milohax/rb3/scripts/web
PORT=8441
ROOT=/tmp/rb3perf-netmatrix
run_cond() {
  local label=$1 mbps=$2 rtt=$3 cap=$4
  for r in 1 2; do
    local out="$ROOT/${label}-run${r}"
    [ -f "$out/result.json" ] && { echo "SKIP ${label} run${r} (already has result)"; continue; }
    echo "START ${label} run${r} (${mbps}Mbps/${rtt}ms, ${cap}s cap)"
    timeout "$cap" node _netmatrix.mjs --port $PORT --out "$out" --mbps "$mbps" --rtt "$rtt" --label "$label" --run "$r" > "$out.log" 2>&1
    local rc=$?
    if [ -f "$out/result.json" ]; then echo "DONE ${label} run${r} rc=$rc"; else echo "FAIL ${label} run${r} rc=$rc"; fi
  done
}
for c in "$@"; do
  case "$c" in
    c4) run_cond c4 4 150 900 ;;
    c5) run_cond c5 1.5 300 1800 ;;
  esac
done
echo "SLOW RUNS COMPLETE"
