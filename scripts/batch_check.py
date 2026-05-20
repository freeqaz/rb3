#!/usr/bin/env python3
"""Batch-check the match% of all functions under a unit pattern.

Reads build/SZBE69_B8/report.json (no per-function objdiff invocations) and
classifies each function as complete (100%), partial, unimplemented (no
decomp object), or already-tracked.

For every function in the matched units it performs a full sync of
decomp.db: updating current_percent/best_percent/size/unit, marking
newly-100% functions COMPLETE, and downgrading any function whose verdict
is COMPLETE but has regressed below 100% back to NULL.

Use this to sweep a unit after a header or build-system change and
quickly see what's at 100%, what's partial, and what's still untouched.
A full sweep (`batch_check.py '*' --quiet`) is wired into the ninja build
so decomp.db always reflects the current report.json.

Usage:
    python3 scripts/batch_check.py 'system/char/CharBones'
    python3 scripts/batch_check.py 'system/char/*'
    python3 scripts/batch_check.py 'system/*' --dry-run
    python3 scripts/batch_check.py 'band3/*' --json
    python3 scripts/batch_check.py '*' --quiet
"""

from __future__ import annotations

import argparse
import fnmatch
import json
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PROJECT_ROOT))

from scripts.orchestrator.database import (  # noqa: E402
    DEFAULT_DB_PATH,
    get_connection,
)

DEFAULT_REPORT = PROJECT_ROOT / "build" / "SZBE69_B8" / "report.json"


def normalize_unit_pattern(pattern: str) -> str:
    """Strip 'main/' prefix if present; report.json's unit names start with it."""
    pattern = pattern.strip()
    if pattern.startswith("main/"):
        return pattern
    return f"main/{pattern}"


def unit_matches(unit_name: str, pattern: str) -> bool:
    """Case-sensitive glob match. Pattern may contain '*' / '?'."""
    return fnmatch.fnmatchcase(unit_name, pattern)


def _func_percent(func: dict) -> float:
    """Fuzzy match% — the 'how close' ranking metric. Stored as
    decomp.db.current_percent (the long-standing convention) and used to
    sort partial work. NOT the completeness signal — see _func_normalized."""
    pct = func.get("fuzzy_match_percent")
    if pct is None:
        pct = func.get("match_percent")
    return float(pct) if pct is not None else 0.0


def _func_normalized(func: dict) -> float:
    """Completeness signal. `match_percent_normalized` is objdiff's own
    matched-function metric — its per-unit `measures.matched_functions`
    count equals count(match_percent_normalized >= 100). A byte-perfect
    function reads exactly 100.0 here even when `fuzzy_match_percent` dips
    slightly below 100, so the verdict MUST key off this field, not fuzzy.
    Falls back to the fuzzy percent only if the field is absent."""
    pct = func.get("match_percent_normalized")
    if pct is None:
        return _func_percent(func)
    return float(pct)


def classify_function(func: dict) -> str:
    """Return one of: complete, partial, unimplemented.

    'complete' is decided by match_percent_normalized (objdiff's
    matched-function signal), NOT fuzzy_match_percent — a genuinely
    complete function routinely has fuzzy_match_percent a hair below 100."""
    size = int(func.get("size", 0) or 0)
    if size == 0:
        # 0-size functions shouldn't show up in practice — treat as unimplemented.
        return "unimplemented"
    if _func_normalized(func) >= 100.0 - 1e-6:
        return "complete"
    if _func_percent(func) > 0:
        return "partial"
    return "unimplemented"


def batch_check(
    unit_pattern: str,
    report_path: Path = DEFAULT_REPORT,
    db_path: Path = Path(DEFAULT_DB_PATH),
    dry_run: bool = False,
) -> dict:
    if not report_path.exists():
        raise SystemExit(f"report.json not found at {report_path} — run `ninja` first.")

    with report_path.open() as f:
        report = json.load(f)

    pattern = normalize_unit_pattern(unit_pattern)
    matched_units = [u for u in report["units"] if unit_matches(u["name"], pattern)]

    if not matched_units:
        return {
            "pattern": pattern,
            "units_matched": 0,
            "checked": 0,
            "complete": [],
            "partial": [],
            "unimplemented": [],
            "newly_complete": [],
            "downgraded": [],
            "percent_updates": 0,
            "dry_run": dry_run,
        }

    complete: list[dict] = []
    partial: list[dict] = []
    unimplemented: list[dict] = []
    all_funcs: list[tuple[str, dict, str]] = []  # (unit_name, func, class)

    for unit in matched_units:
        unit_name = unit["name"]
        for func in unit.get("functions", []):
            klass = classify_function(func)
            all_funcs.append((unit_name, func, klass))

            row = {
                "name": func.get("name", "?"),
                "demangled": (func.get("metadata") or {}).get("demangled_name", ""),
                "unit": unit_name,
                "percent": _func_percent(func),
                "size": int(func.get("size", 0) or 0),
            }
            if klass == "complete":
                complete.append(row)
            elif klass == "partial":
                partial.append(row)
            elif klass == "unimplemented":
                unimplemented.append(row)

    # Full DB sync: for EVERY function in matched units, refresh
    # current_percent/best_percent/size/unit; mark newly-100% as COMPLETE;
    # downgrade regressed COMPLETE rows back to NULL.
    newly_complete: list[str] = []
    downgraded: list[str] = []
    percent_updates = 0
    if not dry_run:
        conn = get_connection(db_path)
        try:
            cur = conn.cursor()
            for unit_name, func, klass in all_funcs:
                symbol = func.get("name", "")
                if not symbol:
                    continue
                pct = _func_percent(func)
                size = int(func.get("size", 0) or 0)
                demangled = (func.get("metadata") or {}).get("demangled_name", "")
                is_complete = klass == "complete"

                existing = cur.execute(
                    "SELECT verdict, current_percent, best_percent FROM functions"
                    " WHERE symbol = ?",
                    (symbol,),
                ).fetchone()

                if existing is None:
                    cur.execute(
                        """
                        INSERT INTO functions
                            (symbol, demangled, unit, size, current_percent,
                             best_percent, verdict)
                        VALUES (?, ?, ?, ?, ?, ?, ?)
                        """,
                        (symbol, demangled or None, unit_name, size, pct, pct,
                         "COMPLETE" if is_complete else None),
                    )
                    percent_updates += 1
                    if is_complete:
                        newly_complete.append(symbol)
                    continue

                old_verdict = existing["verdict"]
                old_pct = existing["current_percent"]
                old_best = existing["best_percent"]
                new_best = max(old_best or 0.0, pct)

                # Determine the new verdict.
                new_verdict = old_verdict
                if is_complete and old_verdict != "COMPLETE":
                    new_verdict = "COMPLETE"
                    newly_complete.append(symbol)
                elif not is_complete and old_verdict == "COMPLETE":
                    # Regressed below 100% — downgrade.
                    new_verdict = None
                    downgraded.append(symbol)

                cur.execute(
                    """
                    UPDATE functions
                    SET current_percent = ?,
                        best_percent = ?,
                        size = ?,
                        unit = ?,
                        demangled = ?,
                        verdict = ?
                    WHERE symbol = ?
                    """,
                    (pct, new_best, size, unit_name, demangled or None,
                     new_verdict, symbol),
                )
                if (old_pct != pct or old_best != new_best
                        or old_verdict != new_verdict):
                    percent_updates += 1
            conn.commit()
        finally:
            conn.close()

    # Sort partial by percent descending — biggest wins first.
    partial.sort(key=lambda r: r["percent"], reverse=True)
    unimplemented.sort(key=lambda r: r["size"], reverse=True)

    return {
        "pattern": pattern,
        "units_matched": len(matched_units),
        "checked": len(all_funcs),
        "complete": complete,
        "partial": partial,
        "unimplemented": unimplemented,
        "newly_complete": newly_complete,
        "downgraded": downgraded,
        "percent_updates": percent_updates,
        "dry_run": dry_run,
    }


def format_text(result: dict, partial_limit: int = 30, unimpl_limit: int = 20) -> str:
    lines: list[str] = []
    p = result["pattern"]
    lines.append(f"Pattern: {p}")
    lines.append(f"Units matched: {result['units_matched']}")
    lines.append(f"Functions checked: {result['checked']}")
    lines.append("")
    lines.append(f"  Complete (100%):  {len(result['complete'])}")
    lines.append(f"  Partial:          {len(result['partial'])}")
    lines.append(f"  Unimplemented:    {len(result['unimplemented'])}")

    nc = result.get("newly_complete", [])
    dg = result.get("downgraded", [])
    if result.get("dry_run"):
        lines.append("")
        lines.append(f"(dry-run — no decomp.db writes; would sync {result['checked']} functions)")
    else:
        lines.append("")
        lines.append(f"decomp.db synced: {result.get('percent_updates', 0)} rows updated, "
                     f"{len(nc)} marked COMPLETE, {len(dg)} downgraded")

    if result["partial"]:
        lines.append("")
        lines.append(f"Partial matches (showing top {min(len(result['partial']), partial_limit)}):")
        for row in result["partial"][:partial_limit]:
            lines.append(f"  {row['percent']:6.2f}%  {row['name']}  ({row['unit']})")
        if len(result["partial"]) > partial_limit:
            lines.append(f"  ... and {len(result['partial']) - partial_limit} more")

    if result["unimplemented"]:
        lines.append("")
        lines.append(f"Unimplemented (showing top {min(len(result['unimplemented']), unimpl_limit)} by size):")
        for row in result["unimplemented"][:unimpl_limit]:
            lines.append(f"  {row['size']:>7}  {row['name']}  ({row['unit']})")
        if len(result["unimplemented"]) > unimpl_limit:
            lines.append(f"  ... and {len(result['unimplemented']) - unimpl_limit} more")

    return "\n".join(lines)


def format_summary(result: dict) -> str:
    """Single-line summary for --quiet (e.g. build logs)."""
    prefix = "batch_check (dry-run)" if result.get("dry_run") else "batch_check"
    return (
        f"{prefix}: {result['units_matched']} units, {result['checked']} functions, "
        f"{len(result.get('newly_complete', []))} marked COMPLETE, "
        f"{len(result.get('downgraded', []))} downgraded, "
        f"{result.get('percent_updates', 0)} percentages updated"
    )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Sweep a unit pattern for 100% matches; mark them COMPLETE in decomp.db.",
    )
    parser.add_argument("pattern", help="Unit name or glob, e.g. 'system/char/CharBones' or 'system/char/*'.")
    parser.add_argument("--report", type=Path, default=DEFAULT_REPORT,
                        help="Path to report.json (default: build/SZBE69_B8/report.json)")
    parser.add_argument("--db", type=Path, default=Path(DEFAULT_DB_PATH),
                        help="Path to decomp.db")
    parser.add_argument("--dry-run", action="store_true",
                        help="Don't update the database — show what would change.")
    parser.add_argument("--json", action="store_true",
                        help="Emit machine-readable JSON instead of text.")
    parser.add_argument("--quiet", action="store_true",
                        help="Suppress the partial/unimplemented tables; print one summary line.")
    parser.add_argument("--partial-limit", type=int, default=30,
                        help="Max partial-match rows to display (default: 30)")
    parser.add_argument("--unimpl-limit", type=int, default=20,
                        help="Max unimplemented-function rows to display (default: 20)")
    args = parser.parse_args()

    result = batch_check(
        unit_pattern=args.pattern,
        report_path=args.report,
        db_path=args.db,
        dry_run=args.dry_run,
    )

    if args.json:
        print(json.dumps(result, indent=2))
    elif args.quiet:
        print(format_summary(result))
    else:
        print(format_text(result, partial_limit=args.partial_limit, unimpl_limit=args.unimpl_limit))

    return 0


if __name__ == "__main__":
    sys.exit(main())
