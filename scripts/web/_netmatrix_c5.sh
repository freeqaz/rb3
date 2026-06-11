#!/usr/bin/env bash
# c5 (1.5 Mbps / 300 ms) with extended timeout — the standard 420s cap can't
# contain the journey at this bandwidth (85MB serial milos alone ≈ 510s).
set -u
cd /home/free/code/milohax/rb3/scripts/web
PORT=8441
ROOT=/tmp/rb3perf-netmatrix
for r in 1 2; do
  out="$ROOT/c5-run${r}"
  echo "START c5 run${r} (1.5Mbps/300ms, 1500s cap)"
  timeout 1500 node _netmatrix.mjs --port $PORT --out "$out" --mbps 1.5 --rtt 300 --label c5 --run "$r" > "$out.log" 2>&1
  rc=$?
  if [ -f "$out/result.json" ]; then echo "DONE c5 run${r} rc=$rc"; else echo "FAIL c5 run${r} rc=$rc"; fi
done
echo "C5 COMPLETE"
