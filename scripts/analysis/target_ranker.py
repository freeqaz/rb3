#!/usr/bin/env python3
"""Fixability-ranked target list — fuses three permuter scanners into one rank.

Future permuter sweeps want to spend their (expensive) builds on functions that
a *source* edit can actually move, not on IPA-locked or FPR-cascade-locked ones
that look tantalizingly close to 100% but are immovable from C++. This tool
fuses three existing scanners into one fixability score and emits a ranked list.

The three fused signals (all read from already-cached data — NO per-function
objdiff is run):

  1. ``scripts.permuter.pattern_scan`` — AST pattern hits gated by asm signal.
     A hit with ``confidence == asm_signal_match`` means the pattern's
     ``relevant(diagnosis)`` confirmed the asm mismatch is the kind the pattern
     fixes. Structural patterns (switch_case_reorder, variable_extraction,
     decl reorder/movement, scope widening/narrowing, demorgan/branch invert,
     signed_unsigned, statement_reorder, store_then_compound_add) weigh most —
     those are the shapes that won this session. Micro patterns weigh less.

  2. ``scripts.analysis.regswap_classify`` — callee-saved register-swap
     classifier with ``ipa_penalty_class``. ``ipa_locked`` (>=99% in an
     ``-ipa file`` unit) is a whole-TU scheduling decision that intra-function
     source edits cannot move → HARD EXCLUDE. ``ipa_partial`` → penalty.

  3. ``scripts.analysis.function_health`` — its per-function diagnosis is reused
     *cheaply*: we do NOT call ``analyze_function`` (it shells out to objdiff
     per fn, ~1s each — far too slow for a fleet-wide rank). Instead we replay
     its mismatch classifier on the cached diff JSON to get a no-objdiff ceiling
     estimate (see ``_cheap_ceiling``), and we additionally veto functions whose
     mismatch is dominated by a floating-point register cascade
     (``classifier.is_fpr_cascade_dominated``) — the confirmed dead-end class.

Population: in-progress functions in ``decomp.db`` (verdict not COMPLETE / not
AT_LIMIT) within the pct window and in scope. AT_LIMIT and COMPLETE are dropped.

Usage:
    python3 scripts/analysis/target_ranker.py                       # top in-scope
    python3 scripts/analysis/target_ranker.py --filter system/synth
    python3 scripts/analysis/target_ranker.py --top 40 --json
    python3 scripts/analysis/target_ranker.py --min-pct 85 --max-pct 96
    python3 scripts/analysis/target_ranker.py --include-out-of-scope
"""

from __future__ import annotations

import argparse
import json
import sqlite3
import sys
import time
from dataclasses import dataclass, field
from pathlib import Path

# scripts/analysis/target_ranker.py → repo root is two levels up.
REPO_ROOT = Path(__file__).resolve().parent.parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

# --- Reuse library entry points from the three scanners (no shelling out). ---
from scripts.permuter import pattern_scan as ps_mod
from scripts.permuter.classifier import (
    count_multi_instruction_fpr_swap_pairs,
    is_fpr_cascade_dominated,
)
from scripts.permuter.diagnosis import diagnose_baseline
from scripts.permuter.patterns import get_pattern
from scripts.analysis import regswap_classify as rs_mod
# function_health: reuse its mismatch classifier (cheap, no objdiff) for ceiling.
from scripts.analysis.function_health import _classify_mismatches as fh_classify

DECOMP_DB = REPO_ROOT / "decomp.db"
DIFF_CACHE_DIR = Path("/tmp/claude")

# ---------------------------------------------------------------------------
# Scope: directories the native port replaces wholesale — matching teaches
# nothing about porting, so exclude by default (overridable).
# ---------------------------------------------------------------------------
OUT_OF_SCOPE_SUBSTR = (
    "sdk/", "network/", "rndwii/", "synthwii/", "/lib/", "system/lib",
    "system/os/", "usbwii/", "_Wii", "_wii",
)

# ---------------------------------------------------------------------------
# Structural patterns: the shapes that produced this session's wins. These get
# the heavy positive weight; everything else is a "micro" pattern (smaller
# weight). switch_case_reorder, variable_extraction and decl reorder are the
# three confirmed winners, so they get the biggest individual bumps.
# ---------------------------------------------------------------------------
STRUCTURAL_PATTERN_WEIGHTS = {
    # Confirmed session winners — top tier.
    "switch_case_reorder": 5.0,
    "variable_extraction": 5.0,
    "declaration_reorder": 4.5,
    "declaration_movement": 4.0,
    "signed_unsigned": 4.0,
    "signed_unsigned_cast_polarity": 3.5,
    # Other structural rewrites that have landed wins historically.
    "scope_widening": 3.5,
    "scope_narrowing": 3.5,
    "statement_reorder": 3.0,
    "store_then_compound_add": 3.0,
    "demorgan_guard": 3.0,
    "positive_branch_invert": 3.0,
    "early_return_merge": 3.0,
    "branch_polarity": 2.5,
    "assignment_reorder": 2.5,
    "loop_var_hoist": 2.5,
    "comparison_equivalence": 2.0,
    "comparison_flip": 2.0,
    "ternary_swap": 2.0,
}
# Default weight for any asm_signal_match hit from a non-structural pattern.
MICRO_PATTERN_WEIGHT = 1.0

# Patterns we actually scan for. We scan the structural set (high value) plus a
# handful of broadly-applicable micro patterns so a function with only a micro
# signal still surfaces above one with no signal at all.
SCAN_PATTERNS = list(STRUCTURAL_PATTERN_WEIGHTS.keys()) + [
    "bool_materialize", "signed_unsigned", "comparison_equivalence",
    "fma_reorder", "tail_call_reorder", "temp_elimination",
    "reference_elimination",
]
# Deduplicate while preserving order.
SCAN_PATTERNS = list(dict.fromkeys(SCAN_PATTERNS))


# ---------------------------------------------------------------------------
# Per-function fused record
# ---------------------------------------------------------------------------
@dataclass
class RankedFunction:
    symbol: str
    demangled: str
    unit: str
    current_percent: float
    best_percent: float | None
    verdict: str | None
    in_scope: bool

    # Fused signals
    score: float = 0.0
    excluded: bool = False
    exclude_reason: str = ""
    pattern_hits: list[str] = field(default_factory=list)  # asm_signal_match patterns
    pattern_score: float = 0.0
    ipa_penalty_class: str | None = None
    fpr_cascade_pairs: int | None = None
    ceiling: float | None = None
    headroom: float | None = None
    reasons: list[str] = field(default_factory=list)


# ---------------------------------------------------------------------------
# Data loading
# ---------------------------------------------------------------------------
def _in_scope(unit: str) -> bool:
    if not unit:
        return True  # unknown unit: don't pre-judge.
    low = unit.lower()
    return not any(s in low for s in OUT_OF_SCOPE_SUBSTR)


def _load_db_functions(filter_substr: str | None, min_pct: float,
                       max_pct: float) -> list[sqlite3.Row]:
    """In-progress functions from decomp.db within the pct window.

    'In-progress' = verdict is NULL or anything other than COMPLETE/AT_LIMIT.
    COMPLETE (done) and AT_LIMIT (this session's confirmed dead-ends) are
    dropped at the SQL level so they never enter the ranking.
    """
    if not DECOMP_DB.exists():
        print(f"Error: {DECOMP_DB} not found", file=sys.stderr)
        sys.exit(1)
    conn = sqlite3.connect(str(DECOMP_DB))
    conn.row_factory = sqlite3.Row
    q = (
        "SELECT symbol, demangled, unit, current_percent, best_percent, verdict "
        "FROM functions "
        "WHERE current_percent IS NOT NULL "
        "AND current_percent >= ? AND current_percent < ? "
        "AND (verdict IS NULL OR verdict NOT IN ('COMPLETE', 'AT_LIMIT'))"
    )
    params: list = [min_pct, max_pct]
    if filter_substr:
        q += " AND unit LIKE ?"
        params.append(f"%{filter_substr}%")
    rows = conn.execute(q, params).fetchall()
    conn.close()
    return rows


def _load_regswap_index() -> dict[str, dict]:
    """Index regswap_classify records by symbol (ipa-aware, default ON).

    Reuses regswap_classify's library helpers verbatim so the IPA penalty
    logic stays single-sourced. Keyed by mangled ``symbol``.
    """
    import glob

    ipa_map, _ = rs_mod._load_unit_ipa_map()
    ipa_known = rs_mod._OBJDIFF_JSON.exists()
    out: dict[str, dict] = {}
    for df in glob.glob(f"{DIFF_CACHE_DIR}/diff_*.json"):
        try:
            with open(df) as f:
                data = json.load(f)
        except Exception:
            continue
        instrs = data.get("instructions", [])
        pct = data.get("normalized_match_percent", 0)
        if not instrs:
            continue
        pairs = rs_mod.find_swap_pairs(instrs)
        if not pairs:
            continue
        sym = data.get("symbol", "")
        if not sym:
            continue
        unit = data.get("unit", "") or ""
        resolved = rs_mod._resolve_unit(unit, ipa_map)
        penalty = rs_mod._ipa_penalty_class(resolved, pct, ipa_map, ipa_known)
        # Prefer the highest-pct record if a symbol has multiple diffs.
        prev = out.get(sym)
        if prev is None or pct > prev["match_percent"]:
            out[sym] = {
                "match_percent": pct,
                "ipa_penalty_class": penalty,
                "num_pairs": len(pairs),
            }
    return out


def _load_diag_index() -> dict[str, "object"]:
    """Index a Diagnosis per symbol from the cached diffs (no objdiff run).

    Used for the FPR-cascade veto and the cheap ceiling estimate. Keyed by
    mangled ``symbol``; only diffs with real instructions are included.
    """
    import glob

    out: dict[str, object] = {}
    cache: dict[str, float] = {}
    for df in glob.glob(f"{DIFF_CACHE_DIR}/diff_*.json"):
        try:
            with open(df) as f:
                data = json.load(f)
        except Exception:
            continue
        if not data.get("instructions"):
            continue
        sym = data.get("symbol", "")
        if not sym:
            continue
        pct = data.get("normalized_match_percent", 0) or 0.0
        if sym in cache and cache[sym] >= pct:
            continue
        cache[sym] = pct
        try:
            diag = diagnose_baseline(data)
        except Exception:
            continue
        out[sym] = (diag, data)
    return out


# function_health labels regswap/scheduling as "unfixable", but those are
# exactly what a source edit / permuter MOVES — so its raw ceiling sits
# pessimistically *below* current%, which is useless as an absolute. We instead
# subtract only the categories that are genuinely immovable from C++ source:
# ICF-merged calls, address relocations, and prologue save/restore stub deltas.
# Register swaps and instruction scheduling are treated as potentially-movable,
# so the resulting ceiling sits at-or-above current% and its headroom is a
# meaningful "how much could a perfect source edit still gain" estimate.
_TRULY_IMMOVABLE_CATS = {"Merged symbol", "Relocation", "Save/Restore"}


def _cheap_ceiling(raw_objdiff_json: dict, current_pct: float) -> tuple[float, float]:
    """Estimate a ceiling WITHOUT running objdiff (reuses function_health's
    classifier on the already-cached diff). Returns (ceiling, headroom).

    Only the genuinely-immovable categories (ICF merge / relocation /
    save-restore) reduce the ceiling; regswap & scheduling are assumed movable.
    The ceiling is floored at current% (the estimator can't justify a ceiling
    below where we already are).
    """
    instrs = raw_objdiff_json.get("instructions", [])
    if not instrs:
        return 100.0, 100.0 - current_pct
    categories, total = fh_classify(instrs)
    immovable = sum(c.count for c in categories if c.name in _TRULY_IMMOVABLE_CATS)
    if total > 0:
        ceiling = min(100.0, 100.0 - (immovable / total) * 100.0)
    else:
        ceiling = 100.0
    # Never claim a ceiling below where we already match.
    ceiling = max(ceiling, current_pct)
    return ceiling, ceiling - current_pct


# ---------------------------------------------------------------------------
# Pattern-scan signal (asm-signal-gated AST hits), per symbol.
# ---------------------------------------------------------------------------
def _gather_pattern_hits(
    filter_substr: str | None,
    diff_path_index: dict,
) -> dict[str, list[str]]:
    """Run pattern_scan over SCAN_PATTERNS with asm-signal gating and return
    ``{symbol: [pattern, ...]}`` of patterns that fired ``asm_signal_match``.

    ``diff_path_index`` is a ``{symbol: Path}`` map (from
    ``pattern_scan._build_diff_index``) — the shape its
    ``_load_diagnosis_for_symbol`` expects (NOT the ``{symbol: (diag, raw)}``
    map used by scoring).

    Reuses pattern_scan's own library functions (``_load_source_files``,
    ``_scan_file``, ``_load_match_info_multi``, ``_load_diagnosis_for_symbol``)
    so attribution + the asm-signal gate match the standalone CLI exactly. No
    fresh objdiff: misses are simply skipped (no signal credited).
    """
    patterns = [get_pattern(n) for n in SCAN_PATTERNS]
    files = ps_mod._load_source_files(filter_substr)
    match_info_multi = ps_mod._load_match_info_multi()
    match_info = {q: (c[0][0], c[0][1]) for q, c in match_info_multi.items() if c}

    fresh_attempted: set[str] = set()
    by_symbol: dict[str, list[str]] = {}

    for unit_name, source_path in files:
        hits = ps_mod._scan_file(
            Path(source_path), patterns, unit_name,
            match_info, show_variants=False,
            match_info_multi=match_info_multi,
        )
        for hit in hits:
            if not hit.symbol or hit.ambiguous_overload:
                continue
            diag, _ = ps_mod._load_diagnosis_for_symbol(
                hit.symbol, diff_path_index, DIFF_CACHE_DIR,
                fresh_objdiff=False, fresh_attempted=fresh_attempted,
            )
            if diag is None:
                continue  # unknown — no asm signal available, skip.
            pat = get_pattern(hit.pattern_name)
            try:
                if not pat.relevant(diag):
                    continue
            except Exception:
                pass  # conservative: keep on relevance crash.
            by_symbol.setdefault(hit.symbol, [])
            if hit.pattern_name not in by_symbol[hit.symbol]:
                by_symbol[hit.symbol].append(hit.pattern_name)
    return by_symbol


# ---------------------------------------------------------------------------
# Fusion score
# ---------------------------------------------------------------------------
def _structural_room_bonus(pct: float) -> float:
    """Bonus that peaks in the 85-96% structural sweet spot.

    Session wins clustered at sub-96% with structural room. 99%+ micro funcs
    are overwhelmingly permuter-class / IPA-locked, so they get little here.
    Piecewise: 0 below 70 (too far / likely a from-scratch rewrite, not a
    permuter target), rising to a +6 plateau across 85-96, decaying to ~0 at
    99.5+.
    """
    if pct < 70.0:
        return 1.0  # some room, but not the permuter sweet spot
    if pct < 85.0:
        # 70 -> 85 ramps 2 -> 6
        return 2.0 + (pct - 70.0) / 15.0 * 4.0
    if pct <= 96.0:
        return 6.0  # the sweet spot
    if pct < 99.5:
        # 96 -> 99.5 decays 6 -> 1
        return 6.0 - (pct - 96.0) / 3.5 * 5.0
    return 0.5


def _score_function(
    rf: RankedFunction,
    pattern_hits: list[str],
    regswap_rec: dict | None,
    diag_entry,
) -> None:
    """Populate rf.score / rf.excluded / rf.reasons from the fused signals."""
    reasons: list[str] = []

    # ---- HARD EXCLUDES --------------------------------------------------
    # (verdict AT_LIMIT/COMPLETE already filtered at SQL level.)

    # IPA-locked: whole-TU scheduling decision; source edits can't move it.
    if regswap_rec is not None:
        rf.ipa_penalty_class = regswap_rec["ipa_penalty_class"]
        if regswap_rec["ipa_penalty_class"] == "ipa_locked":
            rf.excluded = True
            rf.exclude_reason = "ipa_locked (>=99% in -ipa file unit)"
            return

    # FPR-cascade-dominated: a wall of multi-instruction FP register swaps that
    # no source transform can fix (this session's CharIK/CharTwist/Vocal
    # dead-end class). Reuse classifier.is_fpr_cascade_dominated verbatim.
    if diag_entry is not None:
        diag, raw = diag_entry
        n_fpr = count_multi_instruction_fpr_swap_pairs(diag)
        rf.fpr_cascade_pairs = n_fpr
        if is_fpr_cascade_dominated(diag):
            rf.excluded = True
            rf.exclude_reason = f"fpr_cascade_dominated ({n_fpr} multi-instr FPR swap pairs)"
            return
        # Cheap ceiling from the cached diff (no objdiff).
        rf.ceiling, rf.headroom = _cheap_ceiling(raw, rf.current_percent)
        # No headroom at all → effectively at-limit; heavy penalty (don't hard
        # exclude, the cheap ceiling is approximate).
        if rf.ceiling is not None and rf.headroom is not None and rf.headroom <= 0.5:
            reasons.append(f"ceiling {rf.ceiling:.1f}% (headroom {rf.headroom:.1f})")

    # ---- POSITIVE SIGNALS ----------------------------------------------
    score = 0.0

    # 1. Pattern signal (asm_signal_match). Structural patterns weigh most.
    rf.pattern_hits = pattern_hits
    pat_score = 0.0
    for p in pattern_hits:
        pat_score += STRUCTURAL_PATTERN_WEIGHTS.get(p, MICRO_PATTERN_WEIGHT)
    # Diminishing returns past the first couple of hits (avoid one fn winning
    # purely by matching many overlapping patterns).
    if pat_score > 0:
        pat_score = min(pat_score, 5.0) + 0.25 * max(0.0, pat_score - 5.0)
    rf.pattern_score = pat_score
    score += pat_score
    if pattern_hits:
        struct = [p for p in pattern_hits if p in STRUCTURAL_PATTERN_WEIGHTS]
        if struct:
            reasons.append("asm_signal_match: " + ",".join(struct[:3]))
        else:
            reasons.append("asm_signal_match (micro): " + ",".join(pattern_hits[:3]))

    # 2. Structural-room bonus (peaks 85-96%).
    room = _structural_room_bonus(rf.current_percent)
    score += room
    reasons.append(f"room+{room:.1f}@{rf.current_percent:.1f}%")

    # 3. Ceiling/headroom factor: scale down when the cheap ceiling says there's
    #    little to gain; scale up modestly when there is real headroom.
    if rf.headroom is not None:
        if rf.headroom <= 0.5:
            score *= 0.25
        elif rf.headroom < 2.0:
            score *= 0.7
        elif rf.headroom >= 5.0:
            score *= 1.15

    # 4. IPA penalty (non-locked bands).
    if rf.ipa_penalty_class == "ipa_partial":
        score *= 0.6
        reasons.append("ipa_partial penalty")
    elif rf.ipa_penalty_class == "intra_function":
        # callee-saved swap present but pct<95 → still plausibly fixable.
        reasons.append("intra_function regswap")

    # 5. Out-of-scope penalty (only applies when --include-out-of-scope is on;
    #    in-scope-only runs never see these).
    if not rf.in_scope:
        score *= 0.3
        reasons.append("out-of-scope")

    # 6. No-signal floor: a function with neither a pattern hit nor a diagnosis
    #    is a weak target — it only has the room bonus. That's intentional: it
    #    keeps un-diagnosed functions from outranking asm-confirmed ones.
    if not pattern_hits and diag_entry is None:
        reasons.append("no asm diagnosis cached")

    rf.score = round(score, 3)
    rf.reasons = reasons


# ---------------------------------------------------------------------------
# Driver
# ---------------------------------------------------------------------------
def rank(
    filter_substr: str | None,
    min_pct: float,
    max_pct: float,
    include_out_of_scope: bool,
) -> list[RankedFunction]:
    rows = _load_db_functions(filter_substr, min_pct, max_pct)

    print(f"Loaded {len(rows)} in-progress functions from decomp.db",
          file=sys.stderr)
    print("Indexing cached diffs (regswap + diagnosis)…", file=sys.stderr)
    regswap_index = _load_regswap_index()
    diag_index = _load_diag_index()
    print(f"  {len(regswap_index)} regswap records, {len(diag_index)} diagnoses",
          file=sys.stderr)

    print("Scanning source for asm-signal-gated pattern hits…", file=sys.stderr)
    t0 = time.time()
    # pattern_scan's _load_diagnosis_for_symbol wants a {symbol: Path} index,
    # not our {symbol: (diag, raw)} scoring index — build it from the cache.
    diff_path_index = ps_mod._build_diff_index(DIFF_CACHE_DIR)
    pattern_hits_by_sym = _gather_pattern_hits(filter_substr, diff_path_index)
    print(f"  {len(pattern_hits_by_sym)} functions with >=1 asm_signal_match "
          f"({time.time() - t0:.1f}s)", file=sys.stderr)

    out: list[RankedFunction] = []
    for row in rows:
        unit = row["unit"] or ""
        in_scope = _in_scope(unit)
        if not in_scope and not include_out_of_scope:
            continue
        rf = RankedFunction(
            symbol=row["symbol"],
            demangled=row["demangled"] or "",
            unit=unit,
            current_percent=row["current_percent"],
            best_percent=row["best_percent"],
            verdict=row["verdict"],
            in_scope=in_scope,
        )
        _score_function(
            rf,
            pattern_hits_by_sym.get(rf.symbol, []),
            regswap_index.get(rf.symbol),
            diag_index.get(rf.symbol),
        )
        out.append(rf)

    # Drop hard-excluded, then sort by score desc, current% desc as tiebreak.
    ranked = [r for r in out if not r.excluded]
    ranked.sort(key=lambda r: (-r.score, -r.current_percent))
    return ranked


# ---------------------------------------------------------------------------
# Output
# ---------------------------------------------------------------------------
def _short(name: str, n: int) -> str:
    return name if len(name) <= n else name[: n - 1] + "…"


def _print_table(ranked: list[RankedFunction], top: int) -> None:
    show = ranked[:top]
    print(f"\n{'=' * 110}")
    print(f"FIXABILITY-RANKED TARGETS (top {len(show)} of {len(ranked)} "
          f"non-excluded)")
    print(f"{'=' * 110}")
    hdr = (f"{'#':>3} {'Score':>6} {'Cur%':>6} {'Ceil':>5} {'IPA':>9} "
           f"{'FPRp':>4}  {'Function':40} Reasons")
    print(hdr)
    print("─" * 110)
    for i, r in enumerate(show, 1):
        name = r.demangled or r.symbol
        ipa = (r.ipa_penalty_class or "-")[:9]
        ceil = f"{r.ceiling:.0f}" if r.ceiling is not None else "-"
        fprp = str(r.fpr_cascade_pairs) if r.fpr_cascade_pairs is not None else "-"
        reasons = "; ".join(r.reasons[:3])
        print(f"{i:>3} {r.score:6.2f} {r.current_percent:5.1f}% {ceil:>5} "
              f"{ipa:>9} {fprp:>4}  {_short(name, 40):40} {_short(reasons, 60)}")


def _to_dict(r: RankedFunction) -> dict:
    return {
        "symbol": r.symbol,
        "demangled": r.demangled,
        "unit": r.unit,
        "current_percent": r.current_percent,
        "best_percent": r.best_percent,
        "verdict": r.verdict,
        "in_scope": r.in_scope,
        "score": r.score,
        "pattern_hits": r.pattern_hits,
        "pattern_score": round(r.pattern_score, 3),
        "ipa_penalty_class": r.ipa_penalty_class,
        "fpr_cascade_pairs": r.fpr_cascade_pairs,
        "ceiling": r.ceiling,
        "headroom": r.headroom,
        "reasons": r.reasons,
    }


def main() -> None:
    ap = argparse.ArgumentParser(
        prog="python3 scripts/analysis/target_ranker.py",
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    ap.add_argument("--filter", dest="filter_substr", default=None,
                    help="Unit substring filter (e.g. 'system/synth', 'band3/game').")
    ap.add_argument("--top", type=int, default=20,
                    help="Show top N ranked functions (default: 20).")
    ap.add_argument("--min-pct", type=float, default=0.0,
                    help="Minimum current match%% (default: 0).")
    ap.add_argument("--max-pct", type=float, default=100.0,
                    help="Maximum current match%% — exclusive (default: 100).")
    ap.add_argument("--include-out-of-scope", action="store_true",
                    help="Include sdk/network/rndwii/synthwii/lib/os units "
                         "(penalized, not excluded). Default: in-scope only.")
    ap.add_argument("--json", dest="json_output", action="store_true",
                    help="Emit ranked list as JSON to stdout.")
    args = ap.parse_args()

    ranked = rank(
        args.filter_substr, args.min_pct, args.max_pct,
        args.include_out_of_scope,
    )

    if args.json_output:
        out = {
            "metadata": {
                "filter": args.filter_substr,
                "min_pct": args.min_pct,
                "max_pct": args.max_pct,
                "include_out_of_scope": args.include_out_of_scope,
                "total_ranked": len(ranked),
                "scan_patterns": SCAN_PATTERNS,
            },
            "ranked": [_to_dict(r) for r in ranked[: args.top if args.top else len(ranked)]],
        }
        print(json.dumps(out, indent=2))
    else:
        _print_table(ranked, args.top)


if __name__ == "__main__":
    main()
