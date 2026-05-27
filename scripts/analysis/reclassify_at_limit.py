"""Bulk reclassification of AT_LIMIT functions.

Scans AT_LIMIT functions, runs objdiff + diagnosis, classifies fixable vs
unfixable patterns, and updates DB verdicts so query_functions surfaces
fixable ones as workable targets.

Usage:
    python -m scripts.analysis.reclassify_at_limit                         # Dry run, all AT_LIMIT
    python -m scripts.analysis.reclassify_at_limit --apply                 # Actually update DB
    python -m scripts.analysis.reclassify_at_limit --unit 'system/char/*'  # Filter by unit
    python -m scripts.analysis.reclassify_at_limit --min-pct 90            # Filter by match %
    python -m scripts.analysis.reclassify_at_limit --json -o report.json   # JSON output

NOTE: scripts/permuter is a symlink to dc3-decomp/scripts/permuter, so all
module-level state in batch_triage (REPO_ROOT, _project, DECOMP_DB) points
to DC3's repo. This script overrides build_object, run_objdiff, and
load_unit_source_map to use the correct RB3 paths.
"""

from __future__ import annotations

import argparse
import json
import sqlite3
import subprocess
import sys
import time
from dataclasses import asdict, dataclass
from pathlib import Path

from scripts.permuter.diagnosis import diagnose_baseline, is_all_noise
from scripts.permuter.batch_triage import classify
from scripts.permuter.types import extract_qualified_name

# ---------------------------------------------------------------------------
# RB3-specific paths (REPO_ROOT must be computed from THIS file's real path,
# not from batch_triage.__file__ which resolves through the symlink to DC3)
# ---------------------------------------------------------------------------

# This file lives at rb3/scripts/analysis/reclassify_at_limit.py
# __file__ -> rb3/scripts/analysis/reclassify_at_limit.py  (real path, not symlink)
REPO_ROOT = Path(__file__).resolve().parent.parent.parent
DECOMP_DB = REPO_ROOT / "decomp.db"
OBJDIFF_JSON = REPO_ROOT / "objdiff.json"
OBJDIFF_CLI = REPO_ROOT / "bin" / "objdiff-cli"
BUILD_ID = "SZBE69_B8"

# RB3 units are stored in decomp.db with prefix "main/" (e.g. "main/system/char/Char")
# objdiff.json also uses "main/" as the unit prefix.
UNIT_PREFIX = "main/"

# Columns present in the RB3 functions table.
# verdict_reason was added in DC3 but not yet migrated to RB3's schema;
# apply_reclassification skips it gracefully.
_HAS_VERDICT_REASON: bool | None = None  # lazy-checked on first DB write


def _has_verdict_reason_column() -> bool:
    global _HAS_VERDICT_REASON
    if _HAS_VERDICT_REASON is None:
        conn = sqlite3.connect(str(DECOMP_DB))
        cols = {row[1] for row in conn.execute("PRAGMA table_info(functions)")}
        conn.close()
        _HAS_VERDICT_REASON = "verdict_reason" in cols
    return _HAS_VERDICT_REASON


# ---------------------------------------------------------------------------
# RB3-local overrides of batch_triage helpers that use the wrong REPO_ROOT
# ---------------------------------------------------------------------------

def load_unit_source_map() -> dict[str, str]:
    """Load objdiff.json and return unit name -> source_path mapping (RB3)."""
    with open(OBJDIFF_JSON) as f:
        data = json.load(f)
    mapping = {}
    for unit in data.get("units", []):
        name = unit.get("name", "")
        source_path = unit.get("metadata", {}).get("source_path")
        if name and source_path:
            mapping[name] = source_path
    return mapping


def build_object(source_path: str) -> bool:
    """Build the object file for a source path (RB3 via tools/ninja-locked)."""
    from pathlib import PurePosixPath
    obj_path = str(Path(source_path).with_suffix(".o"))
    obj_target = f"build/{BUILD_ID}/{obj_path}"
    result = subprocess.run(
        [str(REPO_ROOT / "tools" / "ninja-locked"), obj_target],
        capture_output=True,
        text=True,
        cwd=str(REPO_ROOT),
    )
    return result.returncode == 0


def run_objdiff(symbol: str) -> tuple[float, dict | None]:
    """Run objdiff-cli and return (match%, json) (RB3)."""
    cmd = [
        str(OBJDIFF_CLI), "diff", "-p", ".", symbol,
        "-c", "functionRelocDiffs=none",
        "-f", "json", "--include-instructions",
    ]
    result = subprocess.run(
        cmd, capture_output=True, text=True, cwd=str(REPO_ROOT),
    )
    try:
        data = json.loads(result.stdout)
        match_pct = data.get("fuzzy_match_percent", 0.0)
        return match_pct, data
    except (json.JSONDecodeError, KeyError):
        return 0.0, None


# ---------------------------------------------------------------------------
# Data model
# ---------------------------------------------------------------------------

@dataclass
class ReclassifyResult:
    """Result of reclassifying a single function."""

    symbol: str
    demangled: str
    unit: str
    current_percent: float
    category: str  # NOISE_ONLY, REGSWAP_ONLY, REGSWAP_PLUS, STRUCTURAL, UNFIXABLE, MIXED, ERROR
    action: str  # REOPEN, KEEP, ERROR
    verdict_reason: str
    diff_op_count: int = 0
    cluster_count: int = 0
    gpr_swap_count: int = 0
    total_instructions: int = 0
    error: str | None = None


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Reclassify AT_LIMIT functions: find fixable ones and reopen them.",
    )
    parser.add_argument(
        "--apply", action="store_true",
        help="Actually update the database (default: dry run)",
    )
    parser.add_argument(
        "--unit", type=str, default=None,
        help="Filter by unit glob pattern (e.g. 'system/char/*')",
    )
    parser.add_argument(
        "--min-pct", type=float, default=0,
        help="Minimum match percentage (default: 0)",
    )
    parser.add_argument(
        "--max-pct", type=float, default=99.9,
        help="Maximum match percentage (default: 99.9)",
    )
    parser.add_argument(
        "--limit", type=int, default=0,
        help="Max functions to process (0 = unlimited)",
    )
    parser.add_argument(
        "-o", "--output", type=Path, default=None,
        help="Output JSON file (default: none)",
    )
    parser.add_argument(
        "--json", action="store_true", dest="json_output",
        help="Output JSON to stdout",
    )
    return parser.parse_args()


# ---------------------------------------------------------------------------
# Query
# ---------------------------------------------------------------------------

def query_at_limit_functions(
    unit_source_map: dict[str, str],
    unit_pattern: str | None,
    min_pct: float,
    max_pct: float,
    limit: int,
) -> list[dict]:
    """Query decomp.db for AT_LIMIT functions matching filters."""
    conn = sqlite3.connect(str(DECOMP_DB))
    conn.row_factory = sqlite3.Row

    query = """
        SELECT symbol, demangled, unit, current_percent, verdict
        FROM functions
        WHERE verdict = 'AT_LIMIT'
          AND current_percent >= ? AND current_percent <= ?
          AND symbol NOT LIKE 'merged_%'
          AND symbol NOT LIKE 'fn_%'
          AND demangled NOT LIKE '%stlpmtx_std::%'
    """
    params: list = [min_pct, max_pct]

    if unit_pattern:
        # Convert glob to SQL LIKE pattern.
        # RB3 units have "main/" prefix in decomp.db (e.g. "main/system/char/Char").
        like_pattern = unit_pattern.replace("*", "%")
        if not like_pattern.startswith(UNIT_PREFIX):
            # Match both bare and "main/"-prefixed patterns
            query += " AND (unit LIKE ? OR unit LIKE ?)"
            params.append(like_pattern)
            params.append(UNIT_PREFIX + like_pattern)
        else:
            query += " AND unit LIKE ?"
            params.append(like_pattern)

    query += " ORDER BY current_percent DESC"

    rows = conn.execute(query, params).fetchall()
    conn.close()

    candidates = []
    for row in rows:
        row_dict = dict(row)
        unit = row_dict["unit"]
        demangled = row_dict.get("demangled", "")

        source_path = unit_source_map.get(unit)
        if not source_path:
            continue

        if not Path(REPO_ROOT / source_path).exists():
            continue

        qualified_name = extract_qualified_name(demangled or "")
        if not qualified_name:
            continue

        row_dict["source_path"] = source_path
        row_dict["qualified_name"] = qualified_name
        candidates.append(row_dict)

    if limit > 0:
        candidates = candidates[:limit]

    return candidates


# ---------------------------------------------------------------------------
# Reclassification logic
# ---------------------------------------------------------------------------

# Which categories get reopened (re-surfaced as workable)
REOPEN_CATEGORIES = {"STRUCTURAL", "REGSWAP_PLUS", "MIXED"}
KEEP_CATEGORIES = {"NOISE_ONLY", "REGSWAP_ONLY", "UNFIXABLE"}


def reclassify_function(candidate: dict) -> ReclassifyResult:
    """Diagnose and reclassify a single AT_LIMIT function."""
    symbol = candidate["symbol"]
    source_path = candidate["source_path"]

    base = dict(
        symbol=symbol,
        demangled=candidate.get("demangled", ""),
        unit=candidate["unit"],
        current_percent=candidate["current_percent"],
    )

    # Build
    if not build_object(source_path):
        return ReclassifyResult(
            **base,
            category="ERROR",
            action="ERROR",
            verdict_reason="build_failed",
            error="build failed",
        )

    # Run objdiff
    match_pct, objdiff_data = run_objdiff(symbol)

    if not objdiff_data or not objdiff_data.get("instructions"):
        return ReclassifyResult(
            **base,
            category="ERROR",
            action="ERROR",
            verdict_reason="no_objdiff_data",
            error="no instruction data from objdiff",
        )

    # Diagnose
    diagnosis = diagnose_baseline(objdiff_data)
    category = classify(diagnosis)

    # Count GPR swaps
    gpr_count = sum(
        1 for (r0, r1) in diagnosis.reg_swap_pairs
        if r0.startswith("r") or r1.startswith("r")
    )

    # Decide action
    if category in REOPEN_CATEGORIES:
        action = "REOPEN"
        verdict_reason = f"has_fixable_{category.lower()}"
    elif category in KEEP_CATEGORIES:
        action = "KEEP"
        verdict_reason = category.lower()
    else:
        action = "KEEP"
        verdict_reason = category.lower()

    return ReclassifyResult(
        **base,
        category=category,
        action=action,
        verdict_reason=verdict_reason,
        diff_op_count=len(diagnosis.diff_ops),
        cluster_count=len(diagnosis.clusters),
        gpr_swap_count=gpr_count,
        total_instructions=diagnosis.total_instructions,
    )


def apply_reclassification(result: ReclassifyResult) -> None:
    """Update the DB verdict for a reclassified function."""
    conn = sqlite3.connect(str(DECOMP_DB))
    has_reason = _has_verdict_reason_column()

    if result.action == "REOPEN":
        # Clear verdict to NULL so query_functions surfaces it as workable
        if has_reason:
            conn.execute(
                "UPDATE functions SET verdict = NULL, verdict_reason = ?, updated_at = CURRENT_TIMESTAMP WHERE symbol = ?",
                (result.verdict_reason, result.symbol),
            )
        else:
            conn.execute(
                "UPDATE functions SET verdict = NULL, updated_at = CURRENT_TIMESTAMP WHERE symbol = ?",
                (result.symbol,),
            )
    elif result.action == "KEEP":
        # Confirm AT_LIMIT; record reason if the column exists in this schema
        if has_reason:
            conn.execute(
                "UPDATE functions SET verdict_reason = ?, updated_at = CURRENT_TIMESTAMP WHERE symbol = ?",
                (result.verdict_reason, result.symbol),
            )
        # If no verdict_reason column, nothing to update for KEEP (verdict already AT_LIMIT)

    conn.commit()
    conn.close()


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    args = parse_args()

    mode = "APPLY" if args.apply else "DRY RUN"
    print(f"=== Reclassify AT_LIMIT Functions [{mode}] ===", file=sys.stderr)
    print(f"  REPO_ROOT: {REPO_ROOT}", file=sys.stderr)
    print(f"  DECOMP_DB: {DECOMP_DB}", file=sys.stderr)

    print("Loading objdiff.json...", file=sys.stderr)
    unit_source_map = load_unit_source_map()
    print(f"  {len(unit_source_map)} units with source paths", file=sys.stderr)

    print(
        f"Querying AT_LIMIT functions "
        f"({args.min_pct}-{args.max_pct}%, unit={args.unit or '*'})...",
        file=sys.stderr,
    )
    candidates = query_at_limit_functions(
        unit_source_map, args.unit, args.min_pct, args.max_pct, args.limit,
    )
    print(f"  {len(candidates)} candidates", file=sys.stderr)

    if not candidates:
        print("No candidates found.", file=sys.stderr)
        sys.exit(0)

    # Process each function
    results: list[ReclassifyResult] = []
    start_time = time.time()
    reopen_count = 0
    keep_count = 0
    error_count = 0

    for i, candidate in enumerate(candidates):
        func_name = candidate["qualified_name"]
        pct = candidate["current_percent"]
        print(
            f"[{i + 1}/{len(candidates)}] {func_name} ({pct:.1f}%) ... ",
            end="", flush=True, file=sys.stderr,
        )

        result = reclassify_function(candidate)
        results.append(result)

        if result.error:
            error_count += 1
            print(f"ERROR: {result.error}", file=sys.stderr)
        elif result.action == "REOPEN":
            reopen_count += 1
            print(
                f"REOPEN [{result.category}] "
                f"(diff_ops={result.diff_op_count}, clusters={result.cluster_count}, gpr_swaps={result.gpr_swap_count})",
                file=sys.stderr,
            )
            if args.apply:
                apply_reclassification(result)
        else:
            keep_count += 1
            print(f"KEEP [{result.category}]", file=sys.stderr)
            if args.apply:
                apply_reclassification(result)

    elapsed = time.time() - start_time

    # Summary
    print(f"\n{'=' * 60}", file=sys.stderr)
    print("RECLASSIFICATION SUMMARY", file=sys.stderr)
    print(f"{'=' * 60}", file=sys.stderr)
    print(f"  Total processed: {len(results)}", file=sys.stderr)
    print(f"  REOPEN (fixable): {reopen_count}", file=sys.stderr)
    print(f"  KEEP (unfixable): {keep_count}", file=sys.stderr)
    print(f"  ERROR:            {error_count}", file=sys.stderr)
    print(f"  Elapsed:          {elapsed:.1f}s", file=sys.stderr)
    if args.apply:
        print(f"\n  Database updated: {reopen_count} functions reopened.", file=sys.stderr)
    else:
        print(f"\n  DRY RUN — no database changes. Use --apply to update.", file=sys.stderr)

    # Category breakdown
    from collections import Counter
    cats = Counter(r.category for r in results if not r.error)
    if cats:
        print(f"\n  Category breakdown:", file=sys.stderr)
        for cat, count in cats.most_common():
            pct = 100 * count / len(results)
            print(f"    {cat:20s}: {count:4d} ({pct:.1f}%)", file=sys.stderr)

    # JSON output
    if args.output or args.json_output:
        report = {
            "metadata": {
                "mode": mode,
                "unit_pattern": args.unit,
                "min_pct": args.min_pct,
                "max_pct": args.max_pct,
                "total_candidates": len(candidates),
                "elapsed_seconds": round(elapsed, 1),
            },
            "summary": {
                "total": len(results),
                "reopen": reopen_count,
                "keep": keep_count,
                "error": error_count,
                "by_category": {
                    cat: count for cat, count in cats.most_common()
                },
            },
            "results": [asdict(r) for r in results],
        }
        output_text = json.dumps(report, indent=2)

        if args.output:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_text(output_text)
            print(f"\nReport written to: {args.output}", file=sys.stderr)
        else:
            print(output_text)


if __name__ == "__main__":
    main()
