#!/usr/bin/env bash
# Reproducible benchmark for the Ghidra Version-Tracking Duplicate-Function O(n^2)
# apply-phase stall. Drives VTDupeBenchmarkScript.java headless against whatever
# VersionTracking.jar is installed in /opt/ghidra, so re-running this exact command
# after swapping in a patched jar measures the patched behavior for the SAME inputs.
#
# Usage:   scripts/ghidra/vt_dupe_benchmark.sh [N]
#   N  = dupe-group size per program (default 700 -> ~69s baseline hot path on this
#        host). Total associations ~= N*N. Use a smaller N (e.g. 300 ~4s) for a
#        quick sanity check; the headline baseline uses 700.
#
# Output:  lines prefixed VT-DUPE-BENCH-RESULT carrying:
#   associations            = correctness counter (k^2 associations created)
#   relatedElementsTouched  = correctness counter (HashSet build work over the group)
#   getAllRelatedAssociationsMs = the hot-path measurement (the number that drops
#                                 ~100x-1000x once the equals/hashCode patch lands)
set -euo pipefail

N="${1:-700}"

GHIDRA_HOME="${GHIDRA_HOME:-/opt/ghidra}"
HEADLESS="${GHIDRA_HOME}/support/analyzeHeadless"
# A real JDK is required (java-21 here is a JRE -> no javac for script compilation).
export JAVA_HOME="${JAVA_HOME:-/usr/lib/jvm/java-26-openjdk}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Throwaway project: recreate deterministically every run (NOT the RB3 project).
PROJ_DIR="${VT_BENCH_PROJ_DIR:-/tmp/vt-bench-proj}"
PROJ_NAME="vtbench"
rm -rf "${PROJ_DIR}"
mkdir -p "${PROJ_DIR}"

echo "VT-DUPE-BENCH: JAVA_HOME=${JAVA_HOME}"
echo "VT-DUPE-BENCH: VersionTracking.jar = $(ls -l "${GHIDRA_HOME}/Ghidra/Features/VersionTracking/lib/VersionTracking.jar" | awk '{print $5" bytes  "$6" "$7" "$8}')"
echo "VT-DUPE-BENCH: N=${N}  project=${PROJ_DIR}/${PROJ_NAME}"

# -import of nothing: we create programs inside the script. Use a tiny dummy file
# is not needed; -preScript runs before any (absent) import, so use -postScript with
# no import by passing a temp file is awkward. Instead run the script directly via
# -preScript with a throwaway import-less invocation using "noimport".
# analyzeHeadless requires either -import or -process; we satisfy it by importing a
# trivial empty file, then our preScript does all the work and we don't -noanalysis.
DUMMY="${PROJ_DIR}/dummy.bin"
printf '\x00\x00\x00\x00' > "${DUMMY}"

VT_BENCH_N="${N}" "${HEADLESS}" \
  "${PROJ_DIR}" "${PROJ_NAME}" \
  -import "${DUMMY}" \
  -processor "PowerPC:BE:32:Gekko_Broadway" \
  -scriptPath "${SCRIPT_DIR}" \
  -preScript VTDupeBenchmarkScript.java "${N}" \
  -noanalysis \
  -deleteProject \
  2>&1 | tee "${PROJ_DIR}/run.log"

echo
echo "=== VT-DUPE-BENCH summary (from ${PROJ_DIR}/run.log) ==="
grep "VT-DUPE-BENCH-RESULT" "${PROJ_DIR}/run.log" || {
  echo "VT-DUPE-BENCH: ERROR no result lines -- inspect ${PROJ_DIR}/run.log"
  exit 1
}
