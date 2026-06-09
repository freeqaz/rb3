#!/usr/bin/env bash
# export_xenon_gzf.sh — Export rb3-xenon's ALREADY-ANALYZED default.xex program
# as a .gzf (packed Ghidra program DB, analysis included) for the seeded
# Wii<->Xenon ghidriff experiment (tools/ghidra/run_ghidriff_xenon.sh).
#
# *** GATED STEP — DO NOT RUN until BOTH are true: ***
#   1. The concurrent BSim agent that owns rb3-xenon's Ghidra project
#      (ghidra_projects/ + pyghidra service on port 8002) has FINISHED.
#      Ghidra projects are single-writer; even a -readOnly headless -process
#      needs the project lock and would collide with the live service.
#   2. No other heavy Ghidra/Java job is running (the 48G VT re-run etc.).
# This script self-checks (port 8002 down, no project lock) but those checks
# are best-effort — confirm with the human/agents first.
#
# WHY GZF EXPORT (decision record, 2026-06-09 — see run_ghidriff_xenon.sh for
# the full version):
#   - rb3-xenon's canonical program was imported with the XEXLoaderWV
#     extension + language PowerPC:BE:64:Xenon (the VMX128 fork language).
#     Every Xenon address in seeds/holdout/bindiff_match.json refers to THIS
#     program's function set. A .gzf carries the analyzed DB verbatim:
#     zero re-import risk, zero re-analysis cost, identical addresses.
#   - A fresh XEX import into the ghidriff project would instead need a
#     XEXLoaderWV zip rebuilt for the exact Ghidra version (none exists for
#     /opt 12.1.2; rebuilding = gradle/Java, also gated), would default to the
#     A2ALT-32addr language (VMX128 instructions disassemble wrong), and would
#     cost hours of auto-analysis with function-boundary drift risk.
#
# VERSION GOTCHA: the project is normally served with the local Ghidra fork
# build (../ghidra/build/ghidra — currently 12.2 DEV), while /opt/ghidra is
# 12.1.2 DEV. Export with the SAME install that owns the project (default
# below). If the program DB was last written by 12.2, the resulting gzf will
# NOT open under /opt 12.1.2 — in that case run run_ghidriff_xenon.sh with
# GHIDRA_INSTALL_DIR pointed at the fork build too (both Ghidras carry the
# PowerPC:BE:64:Xenon language; they are the same fork lineage).
#
# BEFORE FIRST RUN: verify the project/program names (we could not inspect
# ghidra_projects/ while the BSim agent owned it):
#   ls /home/free/code/milohax/rb3-xenon/ghidra_projects/*.gpr
# and adjust XENON_PROJ_LOC / XENON_PROJ_NAME / XENON_PROGRAM below (env
# overrides) if they differ. rb3-xenon/tools/ghidra/import-xex.sh created
# location=ghidra_projects name=RB3Xenon program=default.xex; the pyghidra
# service script's --project-path suggests ghidra_projects/RB3Xenon may be
# the location instead.
#
# USAGE
#   ./tools/ghidra/export_xenon_gzf.sh            # export to the ghidriff-xenon dir
#   ./tools/ghidra/export_xenon_gzf.sh --dry-run  # print the command only
#
set -euo pipefail

RB3_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
XENON_ROOT="${XENON_ROOT:-$(cd "${RB3_ROOT}/.." && pwd)/rb3-xenon}"

# Default to the install that owns the project (pyghidra-service.sh uses the
# fork build). Override: GHIDRA_INSTALL_DIR=/opt/ghidra ./export_xenon_gzf.sh
GHIDRA_INSTALL_DIR="${GHIDRA_INSTALL_DIR:-$(cd "${RB3_ROOT}/.." && pwd)/ghidra/build/ghidra}"
JAVA_HOME="${JAVA_HOME:-/usr/lib/jvm/java-17-openjdk}"

XENON_PROJ_LOC="${XENON_PROJ_LOC:-${XENON_ROOT}/ghidra_projects}"
XENON_PROJ_NAME="${XENON_PROJ_NAME:-RB3Xenon}"
XENON_PROGRAM="${XENON_PROGRAM:-default.xex}"

OUT_GZF="${OUT_GZF:-${RB3_ROOT}/build/SZBE69_B8/ghidra/ghidriff-xenon/rb3_xenon_default_xex.gzf}"
SCRIPT_DIR="${RB3_ROOT}/tools/ghidra"

DRY_RUN=0
[[ "${1:-}" == "--dry-run" ]] && DRY_RUN=1

echo "[export_xenon_gzf] project: ${XENON_PROJ_LOC} / ${XENON_PROJ_NAME} program: ${XENON_PROGRAM}"
echo "[export_xenon_gzf] ghidra:  ${GHIDRA_INSTALL_DIR}"
echo "[export_xenon_gzf] out:     ${OUT_GZF}"

ANALYZE_HEADLESS="${GHIDRA_INSTALL_DIR}/support/analyzeHeadless"
[[ -x "${ANALYZE_HEADLESS}" ]] || { echo "ERROR: analyzeHeadless not found at ${ANALYZE_HEADLESS}"; exit 1; }

if [[ "${DRY_RUN}" == "0" ]]; then
    # Gate 1: pyghidra service on :8002 must be down (BSim agent owns it).
    if command -v ss >/dev/null 2>&1 && ss -ltn 2>/dev/null | grep -q ':8002 '; then
        echo "ERROR: something is listening on :8002 (rb3-xenon pyghidra service?)."
        echo "       The BSim agent likely still owns the project. ABORTING."
        exit 1
    fi
    # Gate 2: no Ghidra project lock.
    if compgen -G "${XENON_PROJ_LOC}/${XENON_PROJ_NAME}.lock" >/dev/null || \
       compgen -G "${XENON_PROJ_LOC}/${XENON_PROJ_NAME}.rep/*.lock" >/dev/null; then
        echo "ERROR: project lock present under ${XENON_PROJ_LOC} — project is in use. ABORTING."
        exit 1
    fi
    [[ -f "${XENON_PROJ_LOC}/${XENON_PROJ_NAME}.gpr" ]] || {
        echo "ERROR: ${XENON_PROJ_LOC}/${XENON_PROJ_NAME}.gpr not found."
        echo "       Check the real project location/name (see header) and override"
        echo "       XENON_PROJ_LOC / XENON_PROJ_NAME."
        exit 1
    }
fi

mkdir -p "$(dirname "${OUT_GZF}")"
export GHIDRA_USER_HOME="${GHIDRA_USER_HOME:-/tmp/claude/ghidra_user_xenon_export}"
mkdir -p "${GHIDRA_USER_HOME}"

CMD=(env JAVA_HOME="${JAVA_HOME}" GHIDRA_USER_HOME="${GHIDRA_USER_HOME}"
     "${ANALYZE_HEADLESS}" "${XENON_PROJ_LOC}" "${XENON_PROJ_NAME}"
     -process "${XENON_PROGRAM}"
     -noanalysis -readOnly
     -scriptPath "${SCRIPT_DIR}"
     -postScript ExportProgramGzf.java "${OUT_GZF}")

if [[ "${DRY_RUN}" == "1" ]]; then
    printf '[export_xenon_gzf] DRY RUN. Would execute:\n  %q' "${CMD[0]}"
    printf ' %q' "${CMD[@]:1}"
    printf '\n'
    exit 0
fi

"${CMD[@]}"

[[ -s "${OUT_GZF}" ]] || { echo "ERROR: export produced no file at ${OUT_GZF}"; exit 1; }
echo "[export_xenon_gzf] OK: ${OUT_GZF} ($(du -h "${OUT_GZF}" | cut -f1))"
echo "Next: ./tools/ghidra/run_ghidriff_xenon.sh"
