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
# 4. The flags --skip-correlators / --implied-min-ratio require the `rb3-improvements`
#    branch of the local ghidriff checkout. The venv installed ghidriff EDITABLE from
#    /home/free/code/milohax/ghidriff (verified: __editable__.ghidriff-1.0.0.pth), so
#    just check out the branch there — no reinstall needed:
#      git -C /home/free/code/milohax/ghidriff checkout rb3-improvements
#    (If the install ever stops being editable, reinstall with:
#      build/SZBE69_B8/ghidra/ghidriff-venv/bin/python -m pip install -e /home/free/code/milohax/ghidriff)
#
# FLAG RATIONALE (2026-06-09, see docs/decomp/ghidriff-improvement-plan-2026-06-09.md):
#   --bsim (default on; --no-bsim removed)  first scored correlator in the cascade
#   --min-func-len 16                       keeps the 8-byte stub ocean out of every stage
#   --skip-correlators BulkBasicBlockMnemonicHash
#                                           kills the 11.1M-tag one-to-many explosion
#   --implied-min-ratio 0.5                 gates implied matches on mnemonic similarity;
#                                           0.5 is PROVISIONAL pending the calibration table in
#                                           docs/decomp/ghidriff-calibration-2026-06-09.md
#
# USAGE
# -----
#   ./tools/ghidra/run_ghidriff.sh          # full run (30-90 min first time)
#   ./tools/ghidra/run_ghidriff.sh --help   # show ghidriff help
#
# POST-PROCESS
# ------------
# After completion, distill the ~3 GB pdiff into the small per-symbol index:
#   build/SZBE69_B8/ghidra/ghidriff-venv/bin/python tools/ghidra/distill_ghidriff.py
# (writes divergence_index.json + renames.json). Per-symbol verdicts are then served
# by bank_divergence.py, which prefers that index over the body-size heuristic:
#   python3 scripts/analysis/bank_divergence.py Init__14BandHeadShaperFv

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
    --min-func-len 16 \
    --skip-correlators BulkBasicBlockMnemonicHash \
    --implied-min-ratio 0.5 \
    --no-symbols \
    --log-level INFO \
    --log-path "${OUT_DIR}/ghidriff.log"

echo ""
echo "[run_ghidriff] Done. Output in: ${OUT_DIR}"
echo "JSON files: ${OUT_DIR}/json/"
echo ""
echo "To distill into the per-symbol divergence index + rename map:"
echo "  ${VENV_PYTHON} ${RB3_ROOT}/tools/ghidra/distill_ghidriff.py"
echo "Then query verdicts via:"
echo "  python3 ${RB3_ROOT}/scripts/analysis/bank_divergence.py SYMBOL"
