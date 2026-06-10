#!/usr/bin/env bash
# Reproducible end-to-end benchmark of the Ghidra Version-Tracking *Reference* correlator
# (VTAbstractReferenceProgramCorrelator.findDestinations -- the Tier-2 parallel target).
#
# Drives VTRefCorrelatorBenchmarkScript.java headless against ONE Ghidra install
# (whichever GHIDRA_HOME points at), so running it twice -- once against the serial
# baseline /opt/ghidra and once against the reflink copy whose VersionTracking.jar is the
# parallel build -- yields an A/B that is byte-comparable on the produced match tuples and
# timed on the scoring phase.
#
# Usage:
#   GHIDRA_HOME=/opt/ghidra                       scripts/ghidra/vt_ref_benchmark.sh OUT.txt
#   GHIDRA_HOME=/home/.../ghidra-parallel-opt     scripts/ghidra/vt_ref_benchmark.sh OUT.txt
#
# Tunables (env): VT_REF_BENCH_N (callers/program), VT_REF_BENCH_ANCHORS,
#                 VT_REF_BENCH_CALLS (anchor calls per caller).
#
# Output: writes the full headless log to OUT.txt and echoes the VT-REF-BENCH-RESULT lines.
set -euo pipefail

OUT="${1:-/tmp/vt-ref-bench.out}"

GHIDRA_HOME="${GHIDRA_HOME:-/opt/ghidra}"
HEADLESS="${GHIDRA_HOME}/support/analyzeHeadless"
export JAVA_HOME="${JAVA_HOME:-/usr/lib/jvm/java-26-openjdk}"
# analyzeHeadless defaults to a 2G heap. The parallel scorer materializes ALL
# destination->neighbor results (with their VectorCompare objects) before the serial
# commit, so at large N its peak heap exceeds the serial scorer's (which streams one
# destination at a time). Give BOTH runs the same generous heap so the A/B measures the
# scoring speedup, not the memory ceiling. (The 2G-OOM at large N is itself a finding,
# noted in the report -- it argues for chunked/streaming results in the upstream PR.)
export GHIDRA_HEADLESS_MAXMEM="${GHIDRA_HEADLESS_MAXMEM:-8G}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Throwaway project, recreated every run. NEVER the RB3 project, never :8001.
PROJ_DIR="${VT_REF_BENCH_PROJ_DIR:-/tmp/vt-ref-bench-proj}"
PROJ_NAME="vtrefbench"
rm -rf "${PROJ_DIR}"
mkdir -p "${PROJ_DIR}"

JARV="$(stat -c '%s bytes  %y' "${GHIDRA_HOME}/Ghidra/Features/VersionTracking/lib/VersionTracking.jar")"
echo "VT-REF-BENCH: GHIDRA_HOME=${GHIDRA_HOME}"
echo "VT-REF-BENCH: VersionTracking.jar = ${JARV}"
echo "VT-REF-BENCH: N=${VT_REF_BENCH_N:-default} ANCHORS=${VT_REF_BENCH_ANCHORS:-default} CALLS=${VT_REF_BENCH_CALLS:-default}"

DUMMY="${PROJ_DIR}/dummy.bin"
printf '\x00\x00\x00\x00' > "${DUMMY}"

# Pin the Ghidra install used by analyzeHeadless. analyzeHeadless resolves its install from
# its own location, so launching the copy's analyzeHeadless is what selects the parallel jar.
"${HEADLESS}" \
  "${PROJ_DIR}" "${PROJ_NAME}" \
  -import "${DUMMY}" \
  -processor "PowerPC:BE:32:default" \
  -scriptPath "${SCRIPT_DIR}" \
  -preScript VTRefCorrelatorBenchmarkScript.java \
  -noanalysis \
  -deleteProject \
  2>&1 | tee "${OUT}"

echo
echo "=== VT-REF-BENCH summary (${OUT}) ==="
grep "VT-REF-BENCH-RESULT" "${OUT}" || {
  echo "VT-REF-BENCH: ERROR no result lines -- inspect ${OUT}"
  exit 1
}
