#!/usr/bin/env bash
# run_ghidriff_xenon.sh — EXPERIMENT 1: seeded, scored ghidriff diff of
# RB3 Wii Bank 8 (MWCC/Gekko) vs RB3 Xbox 360 (MSVC/Xenon).
#
# GOAL: port function IDENTITY (not names) Wii -> Xenon. SUCCESS BAR: precision
# > 0.32 (the existing Wii->Xenon BinDiff oracle) on game-code (band3/)
# identities at comparable recall — scored by tools/ghidra/eval_xenon_matches.py
# against the 146-pair holdout + DC3-name agreement. See
# docs/decomp/ghidriff-improvement-plan-2026-06-09.md (Track 2) and
# docs/decomp/ghidriff-calibration-2026-06-09.md (per-correlator precision).
#
# ---------------------------------------------------------------------------
# HOW THE XENON PROGRAM GETS IN (decision record, 2026-06-09)
# ---------------------------------------------------------------------------
# CHOSEN: .gzf export of rb3-xenon's already-analyzed default.xex program
# (tools/ghidra/export_xenon_gzf.sh — a GATED step, see below). Evidence:
#   - Upstream Ghidra has no XEX loader. rb3-xenon imported default.xex with
#     the XEXLoaderWV extension (~/code/milohax/XEXLoaderWV) using language
#     PowerPC:BE:64:Xenon — the VMX128 language from the local Ghidra fork
#     (rb3-xenon/docs/tools/XEXLOADERWV.md, docs/vmx128/, pyghidra-mcp
#     server.py:_detect_binary_language).
#   - /opt/ghidra 12.1.2 DEV is a build of that same fork lineage: it SHIPS
#     PowerPC:BE:64:Xenon (ppc.ldefs:335), so a gzf of the canonical program
#     is language-compatible with /opt.
#   - ghidriff accepts .gzf inputs directly: binaries passed on the CLI go
#     through GhidraProject.importProgram, which handles packed DBs (see
#     ../ghidriff/tests/test_ghidra_zip_format_import.py), and
#     analyze_program() skips re-analysis when the analyzed flag is set.
#   - Direct fresh XEX import was REJECTED for now: no XEXLoaderWV dist zip
#     matches /opt 12.1.2 (only 12.0.x/12.1 zips exist; rebuilding is a
#     gradle/Java job — also gated), ghidriff has no extension-install or
#     per-binary language mechanism, the loader would default to
#     A2ALT-32addr (VMX128 instructions break), and hours of re-analysis
#     would risk function-boundary drift vs the canonical addresses that
#     seeds/holdout/bindiff_match.json refer to. If a 12.1.2-compatible
#     XEXLoaderWV is ever installed into the runtime Ghidra, direct import
#     into this fresh project becomes acceptable (set RB3_GHIDRIFF_XENON_P2
#     to the .xex), but the gzf stays preferred.
#
# ---------------------------------------------------------------------------
# PREREQUISITES (ALL GATED — confirm before running)
# ---------------------------------------------------------------------------
# 1. The heavy VT re-run (48G JVM, session RB3_b5_to_b8_opt) has FINISHED and
#    the machine is otherwise free. This run starts its own large JVM.
# 2. The concurrent BSim agent owning rb3-xenon's project has FINISHED, and
#    tools/ghidra/export_xenon_gzf.sh has been run (it self-gates on the
#    project lock / port 8002). Until then the default P2 below won't exist.
# 3. Seeds exist: build/SZBE69_B8/ghidra/xenon-seeds/seeds.json
#    (tools/ghidra/build_xenon_seeds.py — 1,189 high-confidence pairs;
#    p1 = Wii Bank8 0x80xxxxxx vaddrs, p2 = Xenon xex vaddrs).
# 4. ghidriff venv installed EDITABLE from /home/free/code/milohax/ghidriff
#    with branch rb3-improvements checked out (provides --seed-matches,
#    --skip-correlators, --implied-min-ratio, --vt-ref-correlators,
#    --vt-ref-min-score, and the flat function_matches list in
#    json/<name>.matches.json).
# 5. /opt/ghidra's VersionTracking.jar is the Tier-1+Tier-2 patched build
#    (build/SZBE69_B8/ghidra/VersionTracking-opt1212.jar — verified identical
#    2026-06-09). --vt-ref-correlators leans on the parallelized LSH
#    reference correlator; re-deploy the jar after any Ghidra update.
#
# RUNTIME UNKNOWNS: p1 analysis is reused (gzf) or ~30-60 min (ELF import);
# p2 comes analyzed via gzf. The diff itself is unknown territory: the
# Bank5<->Bank8 run took ~3h20m dominated by implied matches on 12.6k funcs;
# here p2 has 65,548 funcs but the exact stages will mostly no-op
# cross-compiler, BSim + VT-ref + implied dominate. Budget hours, not minutes.
#
# ---------------------------------------------------------------------------
# FLAG RATIONALE (cross-compiler calibration-informed; see
# docs/decomp/ghidriff-calibration-2026-06-09.md)
# ---------------------------------------------------------------------------
#   --force-diff             REQUIRED: language mismatch (Gekko 32BE vs Xenon
#                            64BE) otherwise raises (ghidra_diff_engine.py:1462).
#   --seed-matches           1,189 externally-validated identity pairs
#                            pre-accepted before the cascade: shrinks the
#                            pool, seeds BSim, seeds VT-ref features, roots
#                            implied propagation.
#   --bsim                   the only similarity stage designed for
#                            cross-compiler "same source, different bytes".
#   --vt-ref-correlators     scored VT CombinedFunctionAndDataReference stage:
#                            call/data-ref-graph features survive compilers
#                            when bytes/mnemonics/CFG don't (our Tier-2 work).
#   --vt-ref-min-score 9.5   accept gate on similarity*confidence. STORED
#                            UNITS: VT multiplies confidence (the dot-product
#                            information content) by 10 before storing, so
#                            9.5 ~= similarity 0.95 x raw-confidence 1.0(x10)
#                            — the same conservative cut headless AutoVT uses
#                            internally (sim 0.95 / raw conf 10.0 precedent,
#                            see ghidriff/vt_ref.py score-semantics note).
#   --min-func-len 16        keeps the 8-byte stub ocean (and most Xenon
#                            import thunks) out of every hash stage
#                            (calibration: ExactBytes <16B = 63% precision).
#   --implied-min-ratio 0.9  calibrated: 0.9 -> 63.9% exact (90.4% on the 1:1
#                            subset) vs 26.5% ungated. Eval should still
#                            1:1-dedupe implied pairs.
#   --skip-correlators       BulkBasicBlockMnemonicHash: 11.1M-tag many-to-many
#                            explosion, ~0% precision, zero accepted matches.
#                            SigCallingCalledHasher: 1.4% precision = garbage.
#                            StructuralGraphExactHash: 46.7% = coin flip even
#                            at 256B+, and CFG shape is the LEAST portable
#                            signal cross-compiler. The exact byte/instruction/
#                            mnemonic hashers stay: they near-no-op across
#                            MWCC->MSVC (harmless) and any hit they do land
#                            (data-only or extern-C identical bodies) is signal.
#   --no-symbols NOT USED    we WANT Wii names in the output (they are the
#                            payload being ported) and the Xenon side is
#                            stripped anyway. CAVEAT — SymbolsHash: the only
#                            named Xenon functions are XEXLoaderWV's import
#                            thunks (__imp__*/Xam*/Nt*/Ke*...). Those mostly
#                            cannot collide with Wii MSL/game names, BUT a few
#                            CRT-ish kernel-export names could (sprintf-class).
#                            SymbolsHash has NO length gate, so thunks slip
#                            past --min-func-len. MITIGATION: any cross-run
#                            match whose only type is SymbolsHash is treated
#                            as suspect — eval_xenon_matches.py flags them and
#                            excludes them from new-coverage counts.
#
# GHIDRA INSTALL: MUST be the fork dist (/home/free/code/milohax/ghidra/build/ghidra,
# Ghidra 12.2_DEV) — the Xenon gzf and Bank8 gzf are 12.2-format project DBs that
# ONLY open under the fork build.  /opt/ghidra (12.1.2) cannot read them.
# The fork ships PowerPC:BE:64:Xenon (ppc.ldefs:335), VT Tier-1+Tier-2 natively,
# and the BSim patches (bsim-xenon-patches branch, jar-swapped into the dist —
# see docs/decomp/xenon-hardening/task-T3-impl.md).
# Default is now the fork path; override with GHIDRA_INSTALL_DIR=/opt/ghidra only
# if you know the gzfs have been migrated to 12.1.2 format.
#
# USAGE
#   ./tools/ghidra/run_ghidriff_xenon.sh             # the real run (GATED, hours)
#   ./tools/ghidra/run_ghidriff_xenon.sh --dry-run   # print the command, run nothing
#   ./tools/ghidra/run_ghidriff_xenon.sh --help      # ghidriff help
#
# POST-PROCESS / SCORING
#   build/SZBE69_B8/ghidra/ghidriff-venv/bin/python \
#       tools/ghidra/eval_xenon_matches.py \
#       --run-dir build/SZBE69_B8/ghidra/ghidriff-xenon
#   (holdout recovery, DC3-name agreement, new band3/ coverage, per-match-type
#    precision proxy; report JSON next to the run.)

set -euo pipefail

RB3_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
VENV_PYTHON="${RB3_ROOT}/build/SZBE69_B8/ghidra/ghidriff-venv/bin/python"
GHIDRA_INSTALL_DIR="${GHIDRA_INSTALL_DIR:-/home/free/code/milohax/ghidra/build/ghidra}"
# JAVA_HOME is effectively unused: pyghidra's launcher resolves the JDK via
# PATH/LaunchSupport against the dist's requirements (fork dist needs >= 21).
# Kept defined only so `env JAVA_HOME=...` below survives set -u.
JAVA_HOME="${JAVA_HOME:-/usr/lib/jvm/java-17-openjdk}"

# BSim toggle (T3 verify fix): ON by default; RB3_XENON_BSIM=0 forces OFF.
# The old `${RB3_XENON_BSIM:+--bsim} ${RB3_XENON_BSIM:---no-bsim}` expansions
# produced `--bsim 1` (argparse exit 2) for =1 and silent OFF when unset.
BSIM_FLAG="--bsim"
[[ "${RB3_XENON_BSIM:-1}" == "0" ]] && BSIM_FLAG="--no-bsim"

OUT_DIR="${RB3_ROOT}/build/SZBE69_B8/ghidra/ghidriff-xenon"
PROJ_DIR="${OUT_DIR}/proj"
SEEDS="${RB3_ROOT}/build/SZBE69_B8/ghidra/xenon-seeds/seeds.json"

# p1 — Wii Bank 8. Prefer the ALREADY-ANALYZED gzf from the Bank5<->Bank8 run
# (same program the calibration was measured on; skips ~30-60 min analysis),
# fall back to the raw ELF. Override: RB3_GHIDRIFF_XENON_P1.
P1_GZF="${RB3_ROOT}/build/SZBE69_B8/ghidra/ghidriff/gzfs/bank8_target.elf-42264e.gzf"
P1_ELF="${RB3_ROOT}/build/SZBE69_B8/ghidra/bank8_target.elf"
if [[ -n "${RB3_GHIDRIFF_XENON_P1:-}" ]]; then
    P1="${RB3_GHIDRIFF_XENON_P1}"
elif [[ -f "${P1_GZF}" ]]; then
    P1="${P1_GZF}"
else
    P1="${P1_ELF}"
fi

# p2 — Xenon. The gzf produced by the GATED tools/ghidra/export_xenon_gzf.sh.
# Override: RB3_GHIDRIFF_XENON_P2 (a raw default.xex is acceptable ONLY with a
# version-matched XEXLoaderWV installed into ${GHIDRA_INSTALL_DIR} — see
# decision record above).
P2="${RB3_GHIDRIFF_XENON_P2:-${OUT_DIR}/rb3_xenon_default_xex.gzf}"

if [[ "${1:-}" == "--help" ]]; then
    JAVA_HOME="${JAVA_HOME}" GHIDRA_INSTALL_DIR="${GHIDRA_INSTALL_DIR}" \
        "${VENV_PYTHON}" -m ghidriff --help
    exit 0
fi
DRY_RUN=0
[[ "${1:-}" == "--dry-run" ]] && DRY_RUN=1

echo "[run_ghidriff_xenon] Checking prerequisites..."
[[ -f "${VENV_PYTHON}" ]] || { echo "ERROR: ghidriff venv not found at ${VENV_PYTHON}"; exit 1; }
[[ -f "${SEEDS}" ]] || { echo "ERROR: seeds not found: ${SEEDS} — run tools/ghidra/build_xenon_seeds.py"; exit 1; }
if [[ ! -f "${P1}" ]]; then
    echo "ERROR: p1 not found: ${P1}"
    echo "       (gzf from the b5<->b8 run, or build bank8_target.elf via bank8_decompile.py)"
    exit 1
fi
if [[ ! -f "${P2}" ]]; then
    echo "WARN: p2 not found: ${P2}"
    echo "      GATED STEP: run tools/ghidra/export_xenon_gzf.sh once the BSim agent"
    echo "      has released rb3-xenon's Ghidra project (see that script's header)."
    if [[ "${DRY_RUN}" != "1" ]]; then
        echo "ERROR: refusing the real run without p2."
        exit 1
    fi
fi
# branch sanity: the flags below only exist on rb3-improvements
if ! "${VENV_PYTHON}" -c "import ghidriff" 2>/dev/null; then
    echo "ERROR: ghidriff not importable from ${VENV_PYTHON}"; exit 1
fi

mkdir -p "${OUT_DIR}" "${PROJ_DIR}"
export GHIDRA_USER_HOME="${GHIDRA_USER_HOME:-/tmp/claude/ghidra_user_bank8}"
mkdir -p "${GHIDRA_USER_HOME}"

echo "  p1 (Wii Bank8):  ${P1}"
echo "  p2 (Xenon):      ${P2}"
echo "  seeds:           ${SEEDS}"
echo "  ghidra:          ${GHIDRA_INSTALL_DIR}"
echo "  out:             ${OUT_DIR}"
echo ""

CMD=(env JAVA_HOME="${JAVA_HOME}"
     GHIDRA_INSTALL_DIR="${GHIDRA_INSTALL_DIR}"
     GHIDRA_USER_HOME="${GHIDRA_USER_HOME}"
     "${VENV_PYTHON}" -m ghidriff
     "${P1}"
     "${P2}"
     --engine VersionTrackingDiff
     --output-path "${OUT_DIR}"
     --project-location "${PROJ_DIR}"
     --project-name "rb3-wii-xenon-diff"
     --force-diff
     --seed-matches "${SEEDS}"
     # BSim: RECOMMENDED ON for the next run (default ON, toggle off with
     # RB3_XENON_BSIM=0 / unset RB3_XENON_BSIM is treated as ON).
     # STATUS: BSim completed in 152s on the 2026-06-10 seeded run (1,186 accepted
     # pairs fed as seeds → effective unmatched pool ~54k, well below the O(n×m)
     # stall threshold).  Patches bsim-topk-cap + bsim-parallel-aggregation applied
     # on ghidra fork branch bsim-xenon-patches and jar-swapped into the dist
     # (see docs/decomp/xenon-hardening/task-T3-impl.md); the patches add top-K
     # truncation + parallel Phase-B aggregation for correctness/perf on near-dup
     # families and observability Msg.info after aggregation.  BSim precision is
     # UNMEASURED (first run was killed before eval; ~20-50% expected cross-compiler).
     # The old stall was caused by decomp_correlate (see --no-decomp-correlate below),
     # NOT BSim.  Toggle (BSIM_FLAG above): leave unset or RB3_XENON_BSIM=1 -> --bsim
     # (default ON); RB3_XENON_BSIM=0 -> --no-bsim.
     # Recommended full invocation (fork GHIDRA_INSTALL_DIR + BSim ON are the defaults):
     #   ./tools/ghidra/run_ghidriff_xenon.sh
     "${BSIM_FLAG}"
     --vt-ref-correlators
     --vt-ref-min-score 9.5
     --min-func-len 16
     --implied-min-ratio 0.9
     --skip-correlators "BulkBasicBlockMnemonicHash,SigCallingCalledHasher,StructuralGraphExactHash"
     --no-decomp-correlate
     --decompiler-timeout "${RB3_XENON_DECOMP_TIMEOUT:-20}"
     --log-level INFO
     --log-path "${OUT_DIR}/ghidriff.log")

if [[ "${DRY_RUN}" == "1" ]]; then
    printf '[run_ghidriff_xenon] DRY RUN. Would execute:\n  %q' "${CMD[0]}"
    printf ' %q' "${CMD[@]:1}"
    printf '\n'
    exit 0
fi

"${CMD[@]}"

echo ""
echo "[run_ghidriff_xenon] Done. Output in: ${OUT_DIR}"
echo "Matches: ${OUT_DIR}/json/*.matches.json (function_matches list)"
echo ""
echo "Score it (EXPERIMENT 1 success bar: precision > 0.32 on band3/ identities):"
echo "  ${VENV_PYTHON} ${RB3_ROOT}/tools/ghidra/eval_xenon_matches.py --run-dir ${OUT_DIR}"
