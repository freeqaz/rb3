#!/usr/bin/env bash
# Sequential matrix runner. Conditions x 2 runs each. One server (8441), cold
# context per run. Emits a line per completed run for monitoring.
set -u
cd /home/free/code/milohax/rb3/scripts/web
PORT=8441
ROOT=/tmp/rb3perf-netmatrix

# condition: label mbps rtt
run_cond() {
  local label=$1 mbps=$2 rtt=$3
  for r in 1 2; do
    local out="$ROOT/${label}-run${r}"
    echo "START ${label} run${r} (${mbps}Mbps/${rtt}ms)"
    timeout 420 node _netmatrix.mjs --port $PORT --out "$out" --mbps "$mbps" --rtt "$rtt" --label "$label" --run "$r" > "$out.log" 2>&1
    local rc=$?
    if [ -f "$out/result.json" ]; then
      echo "DONE ${label} run${r} rc=$rc"
    else
      echo "FAIL ${label} run${r} rc=$rc (no result.json)"
    fi
  done
}

run_cond c1 20 40
run_cond c2 20 150
run_cond c3 8 80
run_cond c4 4 150
run_cond c5 1.5 300
echo "MATRIX COMPLETE"
