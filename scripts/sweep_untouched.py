#!/usr/bin/env python3
"""List RB3 functions with no decomp progress, ranked by size descending.

"Untouched" means a function that has no recorded match% (current_percent
is NULL or 0) and is not already marked COMPLETE or AT_LIMIT. The result is
ranked largest-first so an agent can pick a fresh, high-value unit to start
decompilation work on.

This is an RB3-only tool: it reads decomp.db directly and does not compare
against DC3 (use scripts/dc3_compare.py for cross-project joins).

Usage:
    python3 scripts/sweep_untouched.py system/char/
    python3 scripts/sweep_untouched.py system/char/* --min-size 100
    python3 scripts/sweep_untouched.py main/system/char/* --max-attempts 1
    python3 scripts/sweep_untouched.py system/char/ --limit 10 --json
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any

PROJECT_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PROJECT_ROOT))

from scripts.orchestrator.database import (  # noqa: E402
    DEFAULT_EXCLUDE_PATTERNS,
    get_connection,
)

DEFAULT_DB_PATH = PROJECT_ROOT / "decomp.db"


def normalize_unit_glob(pattern: str) -> str:
    """Normalize a user-supplied unit pattern for a SQL GLOB match.

    decomp.db unit names are all 'main/'-prefixed (e.g. 'main/system/char/
    CharBones'). This accepts a pattern with or without that prefix, and
    turns a trailing-directory pattern ('system/char/') into a glob that
    actually matches the functions under it ('main/system/char/*').
    """
    pattern = pattern.strip()
    if not pattern.startswith("main/"):
        pattern = f"main/{pattern}"
    if pattern.endswith("/"):
        pattern = f"{pattern}*"
    return pattern


def sweep_untouched(
    unit_glob: str,
    min_size: int = 0,
    max_attempts: int = 0,
    limit: int = 40,
    db_path: str | Path = DEFAULT_DB_PATH,
) -> list[dict[str, Any]]:
    """Return untouched functions under the unit glob, largest first."""
    from scripts.orchestrator.database import _build_unit_glob_clause

    glob_clause, glob_params = _build_unit_glob_clause(
        unit_glob, DEFAULT_EXCLUDE_PATTERNS
    )

    query = f"""
        SELECT symbol, demangled, unit, size, current_percent, attempt_count
        FROM functions
        WHERE {glob_clause}
          AND (current_percent IS NULL OR current_percent = 0)
          AND (verdict IS NULL OR verdict NOT IN ('COMPLETE', 'AT_LIMIT'))
          AND COALESCE(size, 0) >= ?
          AND COALESCE(attempt_count, 0) <= ?
        ORDER BY size DESC
        LIMIT ?
    """
    params: list[Any] = glob_params + [min_size, max_attempts, limit]

    conn = get_connection(db_path)
    try:
        rows = conn.execute(query, params).fetchall()
    finally:
        conn.close()

    return [
        {
            "symbol": row["symbol"],
            "unit": row["unit"],
            "size": row["size"],
            "current_percent": row["current_percent"],
            "attempt_count": row["attempt_count"],
        }
        for row in rows
    ]


def format_text(rows: list[dict[str, Any]], unit_glob: str) -> str:
    """Render the results as a text table with a header and total line."""
    lines: list[str] = []
    lines.append(f"Unit glob: {unit_glob}")
    lines.append("")

    if not rows:
        lines.append("No untouched functions found.")
        return "\n".join(lines)

    lines.append(f"{'SIZE':>8}  {'SYMBOL':<48}  UNIT")
    lines.append(f"{'-' * 8}  {'-' * 48}  {'-' * 30}")
    for row in rows:
        size = row["size"] if row["size"] is not None else 0
        lines.append(f"{size:>8}  {row['symbol']:<48}  {row['unit']}")

    lines.append("")
    lines.append(f"Total: {len(rows)} untouched function(s)")
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="List RB3 functions with no decomp progress, ranked by size.",
    )
    parser.add_argument(
        "unit_glob",
        help="Unit glob, e.g. 'system/char/' or 'system/char/*' "
        "(a 'main/' prefix is added if missing).",
    )
    parser.add_argument(
        "--min-size", type=int, default=0,
        help="Only include functions with size >= N (default: 0)",
    )
    parser.add_argument(
        "--max-attempts", type=int, default=0,
        help="Only include functions with attempt_count <= N (default: 0)",
    )
    parser.add_argument(
        "--limit", type=int, default=40,
        help="Maximum number of rows to return (default: 40)",
    )
    parser.add_argument(
        "--db", type=Path, default=DEFAULT_DB_PATH,
        help="Path to decomp.db",
    )
    parser.add_argument(
        "--json", action="store_true",
        help="Emit machine-readable JSON instead of the text table.",
    )
    args = parser.parse_args()

    unit_glob = normalize_unit_glob(args.unit_glob)

    rows = sweep_untouched(
        unit_glob=unit_glob,
        min_size=args.min_size,
        max_attempts=args.max_attempts,
        limit=args.limit,
        db_path=args.db,
    )

    if args.json:
        print(json.dumps(rows, indent=2))
    else:
        print(format_text(rows, unit_glob))

    return 0


if __name__ == "__main__":
    sys.exit(main())
