#!/usr/bin/env python3
"""Audit data symbols for PORT-RELEVANT byte mismatches.

The objdiff report scores data all-or-nothing per section, so a unit's data sits
below 100% for two very different reasons:

  1. Pure MWCC codegen artifacts -- string/float pool ordering, jump-table
     encoding, RTTI header offsets. The host compiler regenerates all of this in
     a native port, so it carries ZERO port value.
  2. Real source bugs -- a vtable slot wired to the wrong method (the class
     header has the wrong virtual-function order/override) or a wrong literal in
     an initialized global. These DO carry into the port.

This tool isolates (2). It enumerates data symbols, diffs each against the target
with objdiff's `--include-data`, and reports only the high-signal findings,
skipping the `@`-prefixed compiler pools that make up the noise bucket.

Finding kinds (highest signal first):
  WRONG_SLOT     vtable/RTTI/pointer slot resolves to a different symbol than the
                 target  -> wrong virtual-function order or override in the header.
  SLOT_SHAPE     a pointer slot is present on only one side (insert/delete)
                 -> vtable length/order differs.
  WRONG_VALUE    an initialized global's raw bytes differ from the target
                 -> wrong literal / initializer value in source.
  POINTER_OFFSET a slot points into the right symbol at the wrong addend.

Default scope is the native-port surface (progress categories `game` + `engine`,
i.e. band3/ plus the platform-agnostic system/ dirs). Use --all to include
sdk / network / Wii-only units too.

Examples:
    scripts/analysis/data_symbol_audit.py                      # port surface
    scripts/analysis/data_symbol_audit.py --filter band3/game  # one subtree
    scripts/analysis/data_symbol_audit.py --all                # everything
    scripts/analysis/data_symbol_audit.py --json findings.json # machine output
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass, field
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
REPORT = REPO / "build" / "SZBE69_B8" / "report.json"
OBJ_DIR = REPO / "build" / "SZBE69_B8" / "obj"
OBJDIFF = REPO / "bin" / "objdiff-cli"
NINJA_LOCKED = REPO / "tools" / "ninja-locked"

PORT_CATEGORIES = {"game", "engine"}
DATA_SECTIONS = {".data", ".rodata", ".sdata", ".sdata2", ".ctors", ".dtors", ".sbss"}


@dataclass
class Finding:
    unit: str
    symbol: str
    demangled: str
    fuzzy: float
    kind: str          # WRONG_SLOT | SLOT_SHAPE | WRONG_VALUE | POINTER_OFFSET
    offset: int
    detail: str


SEVERITY = {"WRONG_SLOT": 0, "SLOT_SHAPE": 1, "WRONG_VALUE": 2, "POINTER_OFFSET": 3}


def obj_path_for(unit_name: str) -> Path:
    # unit "main/band3/bandtrack/GemTrack" -> obj/band3/bandtrack/GemTrack.o
    rel = unit_name.split("/", 1)[1] if "/" in unit_name else unit_name
    return OBJ_DIR / f"{rel}.o"


def in_scope_units(report: dict, include_all: bool, filt: str | None) -> list[tuple[str, Path]]:
    units = []
    for u in report.get("units", []):
        md = u.get("metadata", {}) or {}
        cats = set(md.get("progress_categories", []) or [])
        if not include_all and not (cats & PORT_CATEGORIES):
            continue
        name = u["name"]
        if filt and filt not in name and filt not in (md.get("source_path") or ""):
            continue
        # only units with an imperfect data section are worth diffing
        imperfect = any(
            s.get("name") in DATA_SECTIONS and float(s.get("fuzzy_match_percent", 0)) < 100.0
            for s in u.get("sections", [])
        )
        if not imperfect:
            continue
        op = obj_path_for(name)
        if not op.exists():
            continue
        units.append((name, op))
    return units


def candidate_symbols(obj: Path) -> list[str]:
    """Data (OBJECT) symbols worth diffing: vtables, RTTI, and named globals.
    Skips @-prefixed compiler pools (strings/floats/jump tables) = the noise."""
    try:
        out = subprocess.run(
            ["readelf", "-sW", str(obj)], capture_output=True, text=True, check=True
        ).stdout
    except (subprocess.CalledProcessError, FileNotFoundError):
        return []
    syms: list[str] = []
    seen: set[str] = set()
    for line in out.splitlines():
        f = line.split()
        if len(f) < 8 or not f[0].endswith(":"):
            continue
        size, typ, ndx, name = f[2], f[3], f[6], " ".join(f[7:])
        if typ != "OBJECT" or ndx in ("UND", "ABS"):
            continue
        if size in ("0", "0x0"):
            continue
        if name.startswith("@"):          # @stringBase / @floatBase / @NNNNN / @LOCAL@
            continue
        if "$" in name:                    # __FUNCTION__$N and other compiler temporaries
            continue
        if name in seen:
            continue
        seen.add(name)
        syms.append(name)
    return syms


def is_pointer_table(symbol: str) -> bool:
    return symbol.startswith("__vt__") or symbol.startswith("__RTTI")


def is_noise_symbol(name: str | None) -> bool:
    """A reloc target that is a compiler pool / renumbered temporary, not a real
    named function or global. Slot diffs against these are codegen artifacts
    (e.g. __RTTI type-name strings get renumbered between builds)."""
    if not name:
        return True
    return name.startswith("@") or "$" in name


def diff_symbol(unit: str, symbol: str) -> dict | None:
    try:
        p = subprocess.run(
            [str(OBJDIFF), "diff", "-p", str(REPO), "-u", unit, symbol,
             "-f", "json", "--include-data"],
            capture_output=True, text=True, timeout=120,
        )
    except subprocess.TimeoutExpired:
        return None
    if p.returncode != 0 or not p.stdout.strip():
        return None
    try:
        return json.loads(p.stdout)
    except json.JSONDecodeError:
        return None


def classify(unit: str, symbol: str, result: dict) -> list[Finding]:
    dd = result.get("data_diff")
    if not dd:
        return []
    fuzzy = float(result.get("fuzzy_match_percent", 0.0))
    if fuzzy >= 100.0:
        return []
    demangled = result.get("demangled") or symbol
    ptr_table = is_pointer_table(symbol)
    out: list[Finding] = []

    for rel in dd.get("relocations", []):
        kind = rel.get("kind")
        off = int(rel.get("offset", 0))
        tgt = rel.get("target_symbol") or ""
        base = rel.get("base_target_symbol")
        if kind == "replace" and base is not None and base != tgt:
            # Both sides must be real named symbols. If either is a compiler pool
            # (@N / __FUNCTION__$N), the slot just points at a renumbered string
            # -- pure codegen noise, regenerated by the host compiler.
            if is_noise_symbol(tgt) or is_noise_symbol(base):
                continue
            out.append(Finding(unit, symbol, demangled, fuzzy, "WRONG_SLOT", off,
                               f"target={tgt or '(none)'}  base={base or '(none)'}"))
        elif kind in ("insert", "delete"):
            who = base or tgt or "(none)"
            if is_noise_symbol(who):
                continue
            side = "base-only" if kind == "insert" else "target-only"
            out.append(Finding(unit, symbol, demangled, fuzzy, "SLOT_SHAPE", off,
                               f"{side} slot -> {who}"))
        elif kind == "equal":
            ba = rel.get("base_addend")
            a = rel.get("addend")
            if ba is not None and a is not None and ba != a and not is_noise_symbol(tgt):
                out.append(Finding(unit, symbol, demangled, fuzzy, "POINTER_OFFSET", off,
                                   f"{tgt} addend target={a} base={ba}"))

    # Raw-byte value diffs. For pointer tables a `replace` segment is usually the
    # RTTI/offset-to-top header, so only flag value diffs on real globals.
    if not ptr_table:
        for seg in dd.get("segments", []):
            if seg.get("kind") == "replace":
                off = int(seg.get("offset", 0))
                tb = seg.get("bytes", "")
                bb = seg.get("base_bytes", "")
                out.append(Finding(unit, symbol, demangled, fuzzy, "WRONG_VALUE", off,
                                   f"target={tb[:32]} base={bb[:32]}"))
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--all", action="store_true",
                    help="include sdk/network/Wii units (default: port surface only)")
    ap.add_argument("--filter", help="substring match on unit name or source path")
    ap.add_argument("--jobs", type=int, default=8, help="parallel diff workers")
    ap.add_argument("--no-build", action="store_true",
                    help="skip the up-front ninja build")
    ap.add_argument("--json", metavar="FILE", help="write structured findings to FILE")
    ap.add_argument("--limit", type=int, help="cap number of units (debug)")
    args = ap.parse_args()

    if not REPORT.exists():
        print(f"missing {REPORT}; run tools/ninja-locked build/SZBE69_B8/report.json", file=sys.stderr)
        return 2
    report = json.loads(REPORT.read_text())

    units = in_scope_units(report, args.all, args.filter)
    if args.limit:
        units = units[: args.limit]
    print(f"[scope] {'ALL' if args.all else 'port surface (game+engine)'}"
          f"{f' filter={args.filter!r}' if args.filter else ''}: "
          f"{len(units)} units with imperfect data sections", file=sys.stderr)
    if not units:
        return 0

    if not args.no_build:
        print("[build] tools/ninja-locked (once) ...", file=sys.stderr)
        subprocess.run([str(NINJA_LOCKED)], cwd=str(REPO))

    # (unit, symbol) work list
    work: list[tuple[str, str]] = []
    for name, obj in units:
        for sym in candidate_symbols(obj):
            work.append((name, sym))
    print(f"[work] {len(work)} data symbols to diff", file=sys.stderr)

    findings: list[Finding] = []

    def run(item: tuple[str, str]) -> list[Finding]:
        unit, sym = item
        res = diff_symbol(unit, sym)
        return classify(unit, sym, res) if res else []

    done = 0
    with ThreadPoolExecutor(max_workers=args.jobs) as ex:
        for fs in ex.map(run, work):
            findings.extend(fs)
            done += 1
            if done % 100 == 0:
                print(f"  ... {done}/{len(work)}", file=sys.stderr)

    findings.sort(key=lambda f: (SEVERITY.get(f.kind, 9), f.unit, f.symbol, f.offset))

    # human report
    counts: dict[str, int] = {}
    cur_kind = None
    for f in findings:
        counts[f.kind] = counts.get(f.kind, 0) + 1
        if f.kind != cur_kind:
            cur_kind = f.kind
            print(f"\n=== {f.kind} ===")
        print(f"  {f.unit}  +0x{f.offset:x}  [{f.fuzzy:.1f}%]  {f.demangled}")
        print(f"      {f.detail}")

    print("\n=== summary ===")
    for k in sorted(counts, key=lambda x: SEVERITY.get(x, 9)):
        print(f"  {k:14} {counts[k]}")
    if not findings:
        print("  no port-relevant data findings")

    if args.json:
        Path(args.json).write_text(json.dumps([f.__dict__ for f in findings], indent=2))
        print(f"\nwrote {args.json}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
