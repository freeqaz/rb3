#!/usr/bin/env bash
# run_ghidriff.sh — Run ghidriff to produce instruction-level Bank5 vs Bank8 divergence data.
#
# PREREQUISITES
# -------------
# 1. pyghidra-mcp service MUST be stopped (this needs the project lock).
#    Do NOT run while the service is running.
# 2. bank8_target.elf must exist:
#      cd /home/free/code/milohax/pyghidra-mcp && uv run --python 3.10 --project . \
#        python /home/free/code/milohax/rb3/tools/ghidra/bank8_decompile.py --help
#    (That imports the ELF on first run.)
# 3. ghidriff venv must be installed (already done):
#      /home/free/code/milohax/rb3/build/SZBE69_B8/ghidra/ghidriff-venv/bin/python \
#        -c "import ghidriff; print(ghidriff.__version__)"
#
# USAGE
# -----
#   ./tools/ghidra/run_ghidriff.sh          # full run (30-90 min first time)
#   ./tools/ghidra/run_ghidriff.sh --help   # show ghidriff help
#
# POST-PROCESS
# ------------
# After completion, use bank_divergence_ghidriff.py to query results:
#   python3 scripts/analysis/bank_divergence_ghidriff.py --report
#   python3 scripts/analysis/bank_divergence_ghidriff.py Init__14BandHeadShaperFv

set -euo pipefail

RB3_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
VENV_PYTHON="${RB3_ROOT}/build/SZBE69_B8/ghidra/ghidriff-venv/bin/python"
B5_ELF="/home/free/code/milohax/milo-executable-library/rb3/Wii Proto (Bank 5) (Debug)/band_r_wii.elf"
B8_ELF="${RB3_ROOT}/build/SZBE69_B8/ghidra/bank8_target.elf"
OUT_DIR="${RB3_ROOT}/build/SZBE69_B8/ghidra/ghidriff"
PROJ_DIR="${OUT_DIR}/proj"

if [[ "${1:-}" == "--help" ]]; then
    JAVA_HOME=/usr/lib/jvm/java-17-openjdk \
    GHIDRA_INSTALL_DIR=/opt/ghidra \
    "${VENV_PYTHON}" -m ghidriff --help
    exit 0
fi

echo "[run_ghidriff] Checking prerequisites..."
[[ -f "${VENV_PYTHON}" ]] || { echo "ERROR: ghidriff venv not found at ${VENV_PYTHON}"; exit 1; }
[[ -f "${B5_ELF}" ]]      || { echo "ERROR: Bank 5 ELF not found: ${B5_ELF}"; exit 1; }
[[ -f "${B8_ELF}" ]]      || { echo "ERROR: Bank 8 ELF not found: ${B8_ELF} — run bank8_decompile.py first"; exit 1; }

mkdir -p "${OUT_DIR}" "${PROJ_DIR}"

echo "[run_ghidriff] Starting ghidriff diff..."
echo "  old: ${B5_ELF}"
echo "  new: ${B8_ELF}"
echo "  out: ${OUT_DIR}"
echo "  proj: ${PROJ_DIR}"
echo ""
echo "NOTE: First run takes 30-90 minutes (Ghidra analysis of two large ELFs)."
echo "      Subsequent runs reuse the analyzed project and take ~5-15 minutes."
echo ""

JAVA_HOME=/usr/lib/jvm/java-17-openjdk \
GHIDRA_INSTALL_DIR=/opt/ghidra \
GHIDRA_USER_HOME=/tmp/claude/ghidra_user_bank8 \
"${VENV_PYTHON}" -m ghidriff \
    "${B5_ELF}" \
    "${B8_ELF}" \
    --engine VersionTrackingDiff \
    --output-path "${OUT_DIR}" \
    --project-location "${PROJ_DIR}" \
    --project-name "rb3-b5-b8-diff" \
    --force-diff \
    --no-bsim \
    --min-func-len 4 \
    --no-symbols \
    --log-level INFO \
    --log-path "${OUT_DIR}/ghidriff.log"

echo ""
echo "[run_ghidriff] Done. Output in: ${OUT_DIR}"
echo "JSON files: ${OUT_DIR}/json/"
echo ""
echo "To query results:"
echo "  python3 ${RB3_ROOT}/scripts/analysis/bank_divergence_ghidriff.py --report"
