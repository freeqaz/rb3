#!/usr/bin/env python3
"""
Compare RB3 and DC3 decomp databases to find porting opportunities.

Finds functions in the shared system/ engine that DC3 has matched but RB3 hasn't.
"""

import sqlite3
import argparse
import sys
from pathlib import Path
from collections import defaultdict


RB3_DB = Path(__file__).parent.parent / "decomp.db"
DC3_DB = Path(__file__).parent.parent.parent / "dc3-decomp" / "decomp.db"


def connect(path: Path) -> sqlite3.Connection:
    if not path.exists():
        print(f"ERROR: database not found: {path}", file=sys.stderr)
        sys.exit(1)
    con = sqlite3.connect(path)
    con.row_factory = sqlite3.Row
    return con


def strip_prefix(unit: str, prefix: str) -> str:
    if unit and unit.startswith(prefix):
        return unit[len(prefix):]
    return unit


def main():
    parser = argparse.ArgumentParser(
        description="Compare RB3 vs DC3 decomp progress",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Show all system/ functions DC3 has at 100% but RB3 doesn't
  python3 dc3_compare.py

  # Show unit-level summary sorted by most opportunity
  python3 dc3_compare.py --units

  # Drill into a specific unit
  python3 dc3_compare.py --filter system/math/

  # Show functions missing from RB3 entirely (different splits or not yet implemented)
  python3 dc3_compare.py --show-missing --filter system/math/
""",
    )
    parser.add_argument(
        "--filter", "-f", default="system/",
        help="Unit prefix filter (default: 'system/')",
    )
    parser.add_argument(
        "--min-dc3", type=float, default=100.0,
        help="Minimum DC3 match%% to include (default: 100.0)",
    )
    parser.add_argument(
        "--max-rb3", type=float, default=99.9,
        help="Maximum RB3 match%% to include (default: 99.9)",
    )
    parser.add_argument(
        "--show-missing", "-m", action="store_true",
        help="Also show functions present in DC3 but missing from RB3",
    )
    parser.add_argument(
        "--units", "-u", action="store_true",
        help="Show unit-level summary instead of per-function listing",
    )
    parser.add_argument(
        "--sort",
        choices=["unit", "symbol", "rb3", "gap", "size"],
        default="unit",
        help="Sort per-function output (default: unit)",
    )
    parser.add_argument(
        "--limit", "-n", type=int, default=0,
        help="Limit rows (0 = unlimited)",
    )
    args = parser.parse_args()

    rb3 = connect(RB3_DB)
    dc3 = connect(DC3_DB)

    rb3_prefix = "main/"
    dc3_prefix = "default/"
    filt = args.filter

    # Load all RB3 functions in filter range
    rb3_funcs: dict[str, dict] = {}
    for row in rb3.execute(
        "SELECT symbol, demangled, unit, size, current_percent, best_percent, verdict "
        "FROM functions WHERE unit LIKE ?",
        (f"{rb3_prefix}{filt}%",),
    ):
        rb3_funcs[row["symbol"]] = dict(row)

    # Load all DC3 functions in filter range
    dc3_funcs: dict[str, dict] = {}
    for row in dc3.execute(
        "SELECT symbol, demangled, unit, size, current_percent, best_percent, verdict "
        "FROM functions WHERE unit LIKE ?",
        (f"{dc3_prefix}{filt}%",),
    ):
        dc3_funcs[row["symbol"]] = dict(row)

    print(f"RB3 '{filt}' functions : {len(rb3_funcs)}")
    print(f"DC3 '{filt}' functions : {len(dc3_funcs)}")
    print()

    # Build per-function results
    shared_fixable = []  # DC3>=min, RB3<max, symbol in both
    missing_in_rb3 = []  # DC3>=min, symbol NOT in RB3

    for sym, dc3row in dc3_funcs.items():
        dc3_pct = dc3row["current_percent"] or 0.0
        if dc3_pct < args.min_dc3:
            continue
        if sym not in rb3_funcs:
            missing_in_rb3.append({
                "symbol": sym,
                "demangled": dc3row["demangled"] or sym,
                "dc3_unit": strip_prefix(dc3row["unit"], dc3_prefix),
                "dc3_pct": dc3_pct,
                "size": dc3row["size"] or 0,
            })
            continue
        rb3row = rb3_funcs[sym]
        rb3_pct = rb3row["current_percent"] or 0.0
        if rb3_pct > args.max_rb3:
            continue
        rb3_unit = strip_prefix(rb3row["unit"], rb3_prefix)
        dc3_unit = strip_prefix(dc3row["unit"], dc3_prefix)
        shared_fixable.append({
            "symbol": sym,
            "demangled": rb3row["demangled"] or dc3row["demangled"] or sym,
            "rb3_unit": rb3_unit,
            "dc3_unit": dc3_unit,
            "rb3_pct": rb3_pct,
            "dc3_pct": dc3_pct,
            "gap": dc3_pct - rb3_pct,
            "size": rb3row["size"] or 0,
        })

    # ---- Unit-level summary mode ----
    if args.units:
        # For each RB3 unit, compute: total funcs, matched, unmatched, DC3-ported potential
        rb3_by_unit: dict[str, dict] = defaultdict(lambda: {
            "total": 0, "matched": 0, "fixable": 0, "fixable_size": 0
        })
        for sym, row in rb3_funcs.items():
            unit = strip_prefix(row["unit"], rb3_prefix)
            rb3_by_unit[unit]["total"] += 1
            pct = row["current_percent"] or 0.0
            if pct >= 100.0:
                rb3_by_unit[unit]["matched"] += 1

        for r in shared_fixable:
            rb3_by_unit[r["rb3_unit"]]["fixable"] += 1
            rb3_by_unit[r["rb3_unit"]]["fixable_size"] += r["size"]

        summary = []
        for unit, d in rb3_by_unit.items():
            if d["fixable"] == 0:
                continue
            pct_matched = d["matched"] / d["total"] * 100 if d["total"] else 0
            summary.append((unit, d["total"], d["matched"], pct_matched,
                             d["fixable"], d["fixable_size"]))

        summary.sort(key=lambda x: -x[4])  # sort by fixable count

        if args.limit:
            summary = summary[:args.limit]

        print(f"{'Unit':<50} {'Total':>7} {'Match%':>7} {'DC3Fix':>7} {'Bytes':>8}")
        print("-" * 85)
        for unit, total, matched, pct_m, fixable, fix_size in summary:
            print(f"{unit:<50} {total:>7} {pct_m:>6.1f}% {fixable:>7} {fix_size:>8}")
        print()
        print(f"Total fixable functions (DC3=100%, in RB3 but <100%): {len(shared_fixable)}")
        print(f"Total missing from RB3: {len(missing_in_rb3)}")
        return

    # ---- Per-function listing ----
    sort_key = {
        "unit":   lambda r: (r["rb3_unit"], r["symbol"]),
        "symbol": lambda r: r["symbol"],
        "rb3":    lambda r: (r["rb3_pct"], r["symbol"]),
        "gap":    lambda r: (-r["gap"], r["symbol"]),
        "size":   lambda r: (-r["size"], r["symbol"]),
    }[args.sort]
    shared_fixable.sort(key=sort_key)

    rows = shared_fixable[:args.limit] if args.limit else shared_fixable

    if rows:
        print(f"Functions DC3>={args.min_dc3:.0f}% and RB3<={args.max_rb3:.0f}% (sorted by {args.sort}):")
        print()
        print(f"{'Unit':<50} {'RB3%':>6} {'DC3%':>6} {'Size':>6}  Demangled")
        print("-" * 120)
        for r in rows:
            dem = r["demangled"]
            if len(dem) > 55:
                dem = dem[:52] + "..."
            print(f"{r['rb3_unit']:<50} {r['rb3_pct']:>6.1f} {r['dc3_pct']:>6.1f} {r['size']:>6}  {dem}")
        print()
        print(f"Total: {len(shared_fixable)} functions")

    if args.show_missing and missing_in_rb3:
        missing_in_rb3.sort(key=lambda r: (r["dc3_unit"], r["symbol"]))
        if args.limit:
            missing_in_rb3 = missing_in_rb3[:args.limit]
        print()
        print(f"Functions in DC3>={args.min_dc3:.0f}% but MISSING from RB3 (different splits or not yet ported):")
        print()
        print(f"{'DC3 Unit':<50} {'DC3%':>6} {'Size':>6}  Demangled")
        print("-" * 120)
        for r in missing_in_rb3:
            dem = r["demangled"]
            if len(dem) > 55:
                dem = dem[:52] + "..."
            print(f"{r['dc3_unit']:<50} {r['dc3_pct']:>6.1f} {r['size']:>6}  {dem}")
        print()
        print(f"Total: {len(missing_in_rb3)} missing")


if __name__ == "__main__":
    main()
