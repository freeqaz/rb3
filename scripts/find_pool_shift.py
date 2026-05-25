#!/usr/bin/env python3
"""Find translation units where the MILO_ASSERT pool-shift pattern likely applies.

The pool-shift signal: multiple functions in the same TU have a Signed-arg diff on
an instruction that also carries an `@stringBase0` / `@sda` / `@sda2` symbol
relocation, with the SAME byte delta across all of them. That uniform delta is
the cascade signature — typically caused by one `#cond` stringification adding or
removing a unique TU-pool entry that shifts every later string's offset.

See docs/decomp/patterns/fixable-macros.md ("MILO_ASSERT #cond Stringification
→ Pool Shift") for the underlying mechanism and the bidirectional fix recipe.

Usage:
    python3 scripts/find_pool_shift.py                       # scan whole project
    python3 scripts/find_pool_shift.py --filter band3        # only band3/*
    python3 scripts/find_pool_shift.py --filter system/meta  # one subdir
    python3 scripts/find_pool_shift.py --min-fns 5           # raise threshold
    python3 scripts/find_pool_shift.py --range 80 99.99      # widen match band
    python3 scripts/find_pool_shift.py --jobs 8              # parallel workers
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from concurrent.futures import ProcessPoolExecutor, as_completed
from collections import Counter, defaultdict
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
REPORT_JSON = PROJECT_ROOT / "build" / "SZBE69_B8" / "report.json"
OBJDIFF_CLI = PROJECT_ROOT / "build" / "tools" / "objdiff-cli"

# Symbol prefixes that indicate a pool reference (rodata strings, sdata, sdata2).
POOL_SYMBOL_PREFIXES = ("@stringBase", "@sda", "@sda2")

# Skip directories that the port replaces wholesale (per CLAUDE.md and
# feedback-scope-native-port).
SKIP_PREFIXES = (
    "main/sdk/",
    "main/system/rndwii/",
    "main/system/os/",
    "main/network/",
)


def is_pool_symbol(value: str) -> bool:
    return value.startswith(POOL_SYMBOL_PREFIXES)


def extract_pool_deltas(diff_json: dict) -> list[int]:
    """Return the list of (target - base) deltas for pool-referencing instructions."""
    deltas: list[int] = []
    for ins in diff_json.get("instructions", []):
        if ins.get("match_type") != "diff_arg":
            continue
        breakdown = ins.get("diff_breakdown") or {}
        args = breakdown.get("arguments") or []
        if not args:
            continue
        target = ins.get("target") or {}
        # Check that this instruction touches a pool symbol on either side.
        touches_pool = any(
            (a.get("type") == "Symbol" and is_pool_symbol(a.get("value", "")))
            for a in target.get("typed_args", [])
        )
        if not touches_pool:
            base = ins.get("base") or {}
            touches_pool = any(
                (a.get("type") == "Symbol" and is_pool_symbol(a.get("value", "")))
                for a in base.get("typed_args", [])
            )
        if not touches_pool:
            continue
        for arg in args:
            if arg.get("arg_type") != "immediate":
                continue
            t = arg.get("target") or {}
            b = arg.get("base") or {}
            if t.get("type") != "Signed" or b.get("type") != "Signed":
                continue
            delta = int(t.get("value", 0)) - int(b.get("value", 0))
            if delta != 0:
                deltas.append(delta)
    return deltas


def diff_function(unit: str, symbol: str) -> tuple[str, str, list[int]]:
    """Run objdiff for one function, return (unit, symbol, pool_deltas)."""
    try:
        completed = subprocess.run(
            [
                str(OBJDIFF_CLI),
                "diff",
                "-u",
                unit,
                symbol,
                "--include-instructions",
                "--format",
                "json",
                "-o",
                "-",
            ],
            capture_output=True,
            text=True,
            timeout=60,
            cwd=str(PROJECT_ROOT),
        )
    except subprocess.TimeoutExpired:
        return (unit, symbol, [])
    if completed.returncode != 0:
        return (unit, symbol, [])
    try:
        data = json.loads(completed.stdout)
    except json.JSONDecodeError:
        return (unit, symbol, [])
    return (unit, symbol, extract_pool_deltas(data))


def gather_targets(
    report: dict,
    name_filter: str | None,
    min_match: float,
    max_match: float,
) -> list[tuple[str, str]]:
    """Walk report.json for (unit, symbol) pairs to diff."""
    pairs: list[tuple[str, str]] = []
    for unit in report.get("units", []):
        name = unit.get("name", "")
        if any(name.startswith(p) for p in SKIP_PREFIXES):
            continue
        if name_filter and name_filter not in name:
            continue
        partials = [
            f
            for f in unit.get("functions", [])
            if min_match <= f.get("fuzzy_match_percent", 0) < max_match
        ]
        if len(partials) < 2:
            # need at least 2 partials for a delta to cluster
            continue
        for f in partials:
            pairs.append((name, f["name"]))
    return pairs


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--filter",
        default=None,
        help="Substring filter for unit names (e.g. 'band3', 'system/meta').",
    )
    parser.add_argument(
        "--range",
        nargs=2,
        type=float,
        metavar=("MIN", "MAX"),
        default=[90.0, 99.99],
        help="Match%% range for partial functions (default: 90 to 99.99).",
    )
    parser.add_argument(
        "--min-fns",
        type=int,
        default=3,
        help="Require this many functions sharing the same delta (default: 3).",
    )
    parser.add_argument(
        "--jobs",
        type=int,
        default=8,
        help="Parallel objdiff workers (default: 8).",
    )
    parser.add_argument(
        "--limit",
        type=int,
        default=0,
        help="Stop after diffing N functions (debugging).",
    )
    parser.add_argument(
        "--out",
        default=None,
        help="Write JSON report to this path in addition to stdout.",
    )
    args = parser.parse_args()

    if not REPORT_JSON.exists():
        print(f"missing {REPORT_JSON} — run ninja first", file=sys.stderr)
        return 1
    if not OBJDIFF_CLI.exists():
        print(f"missing {OBJDIFF_CLI}", file=sys.stderr)
        return 1

    with REPORT_JSON.open() as fh:
        report = json.load(fh)

    targets = gather_targets(report, args.filter, args.range[0], args.range[1])
    if args.limit:
        targets = targets[: args.limit]
    print(
        f"scanning {len(targets)} functions in "
        f"{len({u for u, _ in targets})} units...",
        file=sys.stderr,
    )

    # unit -> delta -> list of (symbol, count_in_function)
    per_unit: dict[str, dict[int, list[tuple[str, int]]]] = defaultdict(
        lambda: defaultdict(list)
    )

    with ProcessPoolExecutor(max_workers=args.jobs) as ex:
        futures = [ex.submit(diff_function, u, s) for u, s in targets]
        done = 0
        for fut in as_completed(futures):
            unit, symbol, deltas = fut.result()
            done += 1
            if done % 100 == 0:
                print(f"  diffed {done}/{len(targets)}", file=sys.stderr)
            if not deltas:
                continue
            # Pick dominant delta for this function (most-shifted offset).
            counter = Counter(deltas)
            dominant_delta, dominant_count = counter.most_common(1)[0]
            per_unit[unit][dominant_delta].append((symbol, dominant_count))

    # Rank: TUs where N>=min-fns functions share the SAME delta.
    findings: list[dict] = []
    for unit, deltas in per_unit.items():
        for delta, hits in deltas.items():
            if len(hits) < args.min_fns:
                continue
            findings.append(
                {
                    "unit": unit,
                    "delta": delta,
                    "function_count": len(hits),
                    "total_instruction_hits": sum(c for _, c in hits),
                    "functions": [s for s, _ in hits],
                }
            )

    findings.sort(
        key=lambda f: (f["function_count"], f["total_instruction_hits"]),
        reverse=True,
    )

    if not findings:
        print("\nNo pool-shift candidates above threshold.")
    else:
        print(
            f"\n{len(findings)} pool-shift candidates "
            f"(>= {args.min_fns} functions sharing one delta):\n"
        )
        print(f"{'Funcs':>5}  {'Delta':>6}  {'Insns':>5}  Unit")
        print("-" * 80)
        for f in findings:
            print(
                f"{f['function_count']:5d}  {f['delta']:+6d}  "
                f"{f['total_instruction_hits']:5d}  {f['unit']}"
            )

    if args.out:
        with open(args.out, "w") as fh:
            json.dump(findings, fh, indent=2)
        print(f"\nReport written to {args.out}", file=sys.stderr)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
