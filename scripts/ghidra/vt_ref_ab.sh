#!/usr/bin/env bash
# A/B + determinism driver for the Tier-2 parallel reference correlator.
#
# Runs the VTRefCorrelatorBenchmarkScript end-to-end under BOTH a serial-baseline Ghidra
# runtime and the parallel runtime, back-to-back in one machine-load window (other agents
# may be consuming cores; A/B-in-one-window keeps the comparison fair). Proves:
#   (a) IDENTICAL output: the parallel run's sorted match-tuple digest == the serial run's.
#   (b) DETERMINISM: >=3 parallel runs produce the same digest.
# and reports serial vs parallel correlate() wall-clock + speedup.
#
# Both runtimes are built from the SAME fork tree (same SoftwareModeling/Generic/etc), and
# differ ONLY in VersionTracking.jar (serial findDestinations vs the ConcurrentQ one), so
# any output or timing delta is attributable to the parallelization alone.
#
# Env:
#   SER_HOME   serial-baseline GHIDRA_HOME   (default /home/free/code/milohax/ghidra-rt-serial)
#   PAR_HOME   parallel GHIDRA_HOME          (default .../ghidra-rt-parallel/ghidra_12.2_DEV)
#   VT_REF_BENCH_N / _ANCHORS / _CALLS       benchmark size knobs
#   REPS       number of parallel determinism reps (default 3)
set -euo pipefail

SER_HOME="${SER_HOME:-/home/free/code/milohax/ghidra-rt-serial}"
PAR_HOME="${PAR_HOME:-/home/free/code/milohax/ghidra-rt-parallel/ghidra_12.2_DEV}"
REPS="${REPS:-3}"
export JAVA_HOME="${JAVA_HOME:-/usr/lib/jvm/java-26-openjdk}"
export VT_REF_BENCH_N="${VT_REF_BENCH_N:-8000}"
export VT_REF_BENCH_ANCHORS="${VT_REF_BENCH_ANCHORS:-256}"
export VT_REF_BENCH_CALLS="${VT_REF_BENCH_CALLS:-10}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUTDIR="${VT_REF_AB_OUTDIR:-/tmp/vt-ref-ab}"
mkdir -p "${OUTDIR}"

field() { grep -m1 "VT-REF-BENCH-RESULT $1=" "$2" | sed "s/.*$1=//; s/ .*//"; }

run_one() {
  local home="$1" label="$2" out="$3"
  GHIDRA_HOME="$home" VT_REF_BENCH_PROJ_DIR="${OUTDIR}/proj-${label}" \
    "${SCRIPT_DIR}/vt_ref_benchmark.sh" "${out}" >/dev/null 2>&1 || true
}

echo "VT-REF-AB: N=${VT_REF_BENCH_N} ANCHORS=${VT_REF_BENCH_ANCHORS} CALLS=${VT_REF_BENCH_CALLS} REPS=${REPS}"
echo "VT-REF-AB: SER_HOME=${SER_HOME}"
echo "VT-REF-AB: PAR_HOME=${PAR_HOME}"
echo "VT-REF-AB: load at start: $(uptime | sed 's/.*load average/load/')"

# --- serial baseline (single authoritative run for the digest + timing) ---
SER_OUT="${OUTDIR}/serial.out"
run_one "${SER_HOME}" "serial" "${SER_OUT}"
SER_MS=$(field correlateMs "${SER_OUT}")
SER_DIGEST=$(field digest "${SER_OUT}")
SER_MATCHES=$(field matches "${SER_OUT}")
echo "VT-REF-AB: SERIAL   correlateMs=${SER_MS} matches=${SER_MATCHES} digest=${SER_DIGEST}"

# --- parallel: REPS runs (first one is the timing+digest of record; all check determinism) ---
declare -a PAR_DIGESTS PAR_MS
for r in $(seq 1 "${REPS}"); do
  POUT="${OUTDIR}/parallel-${r}.out"
  run_one "${PAR_HOME}" "parallel-${r}" "${POUT}"
  d=$(field digest "${POUT}"); m=$(field correlateMs "${POUT}"); mc=$(field matches "${POUT}")
  PAR_DIGESTS[$r]="$d"; PAR_MS[$r]="$m"
  echo "VT-REF-AB: PARALLEL[${r}] correlateMs=${m} matches=${mc} digest=${d}"
done

# --- determinism across parallel reps ---
DET="true"
for r in $(seq 2 "${REPS}"); do
  [ "${PAR_DIGESTS[$r]}" = "${PAR_DIGESTS[1]}" ] || DET="false"
done

# --- serial == parallel ---
EQ="true"
[ "${PAR_DIGESTS[1]}" = "${SER_DIGEST}" ] || EQ="false"

PAR1_MS="${PAR_MS[1]}"
SPEEDUP=$(awk "BEGIN{ if (${PAR1_MS}>0) printf \"%.2f\", ${SER_MS}/${PAR1_MS}; else print \"NA\" }")

echo "VT-REF-AB-SUMMARY serialMs=${SER_MS} parallelMs=${PAR1_MS} speedup=${SPEEDUP}x"
echo "VT-REF-AB-SUMMARY deterministic=${DET} matchesSerialEqualsParallel=${EQ}"
echo "VT-REF-AB-SUMMARY serialMatches=${SER_MATCHES} serialDigest=${SER_DIGEST} parallelDigest=${PAR_DIGESTS[1]}"
