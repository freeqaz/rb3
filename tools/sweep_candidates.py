"""Rank decomp promotion candidates by tier.

Combines per-unit `fuzzy_match_percent` from `build/SZBE69_B8/report.json`
with per-unit promotion status from `config/SZBE69_B8/objects.json` to
produce promotable target lists.

Tiers (--tier <name>):

    99-100   :  unit fuzzy >= 99.0%,  unmatched <= --max-fns (default 1)
    95-99    :  unit fuzzy >= 95.0%,  unmatched <= --max-fns (default 2)
    80-95    :  unit fuzzy >= 80.0%,  unmatched <= --max-fns (default 1)
    all      :  no fuzzy floor

The unit is filtered out if it is already Matching or Equivalent, or
sits in an out-of-scope subtree per CLAUDE.md (src/sdk, src/lib,
src/network, src/system/{os,rndwii,stlport}, and third-party libs).

Composes with the link-issue linter: when `--skip-units-with-link-issues`
is passed, each candidate's source file is fed to `bin/lint-link-issues`
and filtered out if any issue is detected. (Slower; the no-link-check
default is fine for an initial pass.)

CLI:
    bin/sweep-candidates --tier 99-100
    bin/sweep-candidates --tier 95-99 --max-fns 1
    bin/sweep-candidates --tier all --json
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Optional

REPO_ROOT = Path(__file__).resolve().parent.parent
REPORT_JSON = REPO_ROOT / "build" / "SZBE69_B8" / "report.json"
OBJECTS_JSON = REPO_ROOT / "config" / "SZBE69_B8" / "objects.json"
LINT_BIN = REPO_ROOT / "bin" / "lint-link-issues"

# Same as the linter — out-of-scope per CLAUDE.md.
_OUT_OF_SCOPE_PREFIXES = (
    "src/sdk/",
    "src/lib/",
    "src/network/",
    "src/system/rndwii/",
    "src/system/os/",
    "src/system/stlport/",
    "src/system/oggvorbis/",
    "src/system/speex/",
    "src/system/zlib/",
    "src/system/synthwii/soundtouch/",
)

TIERS = {
    "99-100": (99.0, 100.01, 1),
    "95-99": (95.0, 99.0, 2),
    "80-95": (80.0, 95.0, 1),
    "all": (0.0, 100.01, 4),
}

# objects.json statuses that disqualify a unit from being a candidate
# (already promoted to source link).
ALREADY_PROMOTED = {"Matching", "Equivalent"}


@dataclass
class Candidate:
    unit: str           # e.g. "main/App"
    source: str         # e.g. "src/App.cpp"
    fuzzy_pct: float    # 0..100
    matched_pct: float  # asm match%
    total_funcs: int
    unmatched_funcs: int
    worst_pct: float
    status: str         # objects.json status

    @property
    def headroom(self) -> float:
        # How far this unit is from a clean promotion.
        return 100.0 - self.fuzzy_pct


def _load_report() -> dict:
    if not REPORT_JSON.exists():
        sys.exit(f"report.json not found: {REPORT_JSON}\nrun `ninja` first")
    with REPORT_JSON.open("r") as f:
        return json.load(f)


def _load_object_statuses() -> dict[str, str]:
    """Return {source_path: status} for every object in objects.json."""
    if not OBJECTS_JSON.exists():
        return {}
    with OBJECTS_JSON.open("r") as f:
        cfg = json.load(f)
    out: dict[str, str] = {}
    for group_name, group in cfg.items():
        objs = group.get("objects", {})
        for path, val in objs.items():
            # Status may be a string ("Matching") or dict ({"status": ...}).
            status = val if isinstance(val, str) else val.get("status", "")
            # Resolve to a `src/...` path. The objects.json key is relative
            # to either the project root or includes `src/`. We normalize:
            if not path.startswith("src/"):
                # Most entries use bare paths like "App.cpp"; the actual
                # source path lives in report.json metadata.
                pass
            out[path] = status
    return out


# Unit-name prefixes (used when source_path metadata is absent) that map
# to out-of-scope subtrees.
_OUT_OF_SCOPE_UNIT_PREFIXES = (
    "main/network/",
    "main/sdk/",
    "main/lib/",
    "main/system/rndwii/",
    "main/system/os/",
    "main/system/stlport/",
    "main/system/oggvorbis/",
    "main/system/speex/",
    "main/system/zlib/",
    "main/system/synthwii/soundtouch/",
)


def _in_scope(source_path: str, unit: str = "") -> bool:
    if source_path:
        return not source_path.startswith(_OUT_OF_SCOPE_PREFIXES)
    if unit:
        return not unit.startswith(_OUT_OF_SCOPE_UNIT_PREFIXES)
    return True


def _has_link_issues(source_path: str) -> bool:
    if not LINT_BIN.exists():
        return False
    p = REPO_ROOT / source_path
    if not p.exists():
        return False
    r = subprocess.run(
        [str(LINT_BIN), "--json", str(p)],
        capture_output=True,
        text=True,
    )
    if r.returncode == 0:
        return False
    try:
        issues = json.loads(r.stdout)
    except json.JSONDecodeError:
        return False
    return bool(issues)


def collect_candidates(
    *,
    fuzzy_min: float,
    fuzzy_max: float,
    max_fns: int,
    in_scope_only: bool,
    skip_already_promoted: bool,
) -> list[Candidate]:
    report = _load_report()
    statuses_by_path = _load_object_statuses()

    # Build a lookup by source filename. objects.json keys are like
    # "App.cpp" while report metadata is "src/App.cpp"; we accept either.
    def lookup_status(source_path: str) -> str:
        if source_path in statuses_by_path:
            return statuses_by_path[source_path]
        bare = source_path.split("/", 1)[1] if "/" in source_path else source_path
        return statuses_by_path.get(bare, "")

    out: list[Candidate] = []
    for u in report["units"]:
        meas = u.get("measures", {})
        fmp = meas.get("fuzzy_match_percent", 0.0)
        if fmp < fuzzy_min or fmp > fuzzy_max:
            continue
        md = u.get("metadata", {}) or {}
        source = md.get("source_path") or ""
        unit_name = u.get("name", "")
        if in_scope_only and not _in_scope(source, unit_name):
            continue
        status = lookup_status(source) if source else ""
        if skip_already_promoted and status in ALREADY_PROMOTED:
            continue
        fns = u.get("functions", []) or []
        unmatched = [
            f for f in fns
            if f.get("fuzzy_match_percent", 100.0) < 100.0
        ]
        if len(unmatched) > max_fns:
            continue
        worst = min(
            (f.get("fuzzy_match_percent", 100.0) for f in fns),
            default=100.0,
        )
        out.append(
            Candidate(
                unit=u.get("name", ""),
                source=source,
                fuzzy_pct=fmp,
                matched_pct=meas.get("matched_code_percent", 0.0),
                total_funcs=len(fns),
                unmatched_funcs=len(unmatched),
                worst_pct=worst,
                status=status,
            )
        )
    return out


def _format_human(cands: list[Candidate]) -> str:
    if not cands:
        return "(no candidates)"
    lines: list[str] = []
    lines.append(
        f"{'fuzzy%':>7}  {'unmatched':>9}  {'worst%':>6}  "
        f"{'status':<11}  unit"
    )
    lines.append("-" * 80)
    for c in sorted(cands, key=lambda x: (-x.fuzzy_pct, x.unmatched_funcs)):
        lines.append(
            f"{c.fuzzy_pct:7.2f}  {c.unmatched_funcs:>9}  "
            f"{c.worst_pct:6.2f}  {c.status:<11}  {c.unit}"
        )
    lines.append("")
    lines.append(f"{len(cands)} candidate(s)")
    return "\n".join(lines)


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description="Rank promotion candidates by tier.")
    ap.add_argument(
        "--tier",
        choices=list(TIERS.keys()),
        default="95-99",
    )
    ap.add_argument(
        "--max-fns",
        type=int,
        default=None,
        help="Override the tier's default max-unmatched-function count.",
    )
    ap.add_argument(
        "--in-scope",
        action="store_true",
        default=True,
        help="Filter out out-of-scope subtrees (default: on).",
    )
    ap.add_argument(
        "--all-scopes",
        action="store_true",
        help="Include out-of-scope paths.",
    )
    ap.add_argument(
        "--include-promoted",
        action="store_true",
        help="Don't filter out Matching/Equivalent units.",
    )
    ap.add_argument(
        "--skip-units-with-link-issues",
        action="store_true",
        help="Run bin/lint-link-issues on each candidate and drop the unit "
             "if any issue is detected. Slower.",
    )
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args(argv)

    in_scope = not args.all_scopes
    fuzzy_min, fuzzy_max, default_max_fns = TIERS[args.tier]
    max_fns = args.max_fns if args.max_fns is not None else default_max_fns

    cands = collect_candidates(
        fuzzy_min=fuzzy_min,
        fuzzy_max=fuzzy_max,
        max_fns=max_fns,
        in_scope_only=in_scope,
        skip_already_promoted=not args.include_promoted,
    )

    if args.skip_units_with_link_issues:
        cands = [c for c in cands if c.source and not _has_link_issues(c.source)]

    if args.json:
        json.dump([asdict(c) for c in cands], sys.stdout, indent=2)
        sys.stdout.write("\n")
    else:
        print(_format_human(cands))
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
