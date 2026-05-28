#!/usr/bin/env python3
"""Audit C++ struct/class layouts against the target binary's DWARF.

A wrong struct layout is one of the highest-value PORT bugs there is: it silently
corrupts memory in a native port (every member access past the first wrong field
reads/writes the wrong bytes) AND it caps every function in the translation unit
from matching, so one bad offset can hold dozens of functions below 100%. Fixing
it therefore pays off twice — port correctness and match%.

This tool compares our header layouts against the *authoritative* layout recorded
in the debug ELF's DWARF. Unlike `/struct-check` (which goes through a running
Ghidra service), it parses the ELF's `.debug_info` directly with pyelftools, so
it is deterministic, dependency-light, and runnable in any sweep.

  Target side  : DW_TAG_structure_type / DW_TAG_class_type DIEs in the Bank-5
                 debug ELF -> {class: byte_size + {member_name: offset}}.
  Our side     : `// 0xHEX` member-offset annotations parsed from src/**/*.h
                 (reuses tools/struct_db.parse_header).

Finding kinds (highest signal first):
  OFFSET_MISMATCH  a member we annotate at one offset sits at a different offset
                   in the target -> wrong member order / wrong base-class size /
                   missing padding. This is a real layout bug.
  SIZE_MISMATCH    our largest annotated member implies a size that exceeds the
                   target's recorded byte_size (extra/oversized member), or the
                   target is materially larger than our last annotated member
                   (likely a missing trailing member). Informational-to-strong.

Caveat: the only DWARF available is the Bank-5 debug ELF (the Bank-8 target DOL is
stripped). Struct layouts are overwhelmingly stable across banks (same source +
compiler), but treat a lone 4-byte tail delta with mild suspicion. This is the
same source `/struct-check` already trusts via Ghidra.

Default scope is the native-port surface (band3/ + platform-agnostic system/).
Use --all to include sdk / network / rndwii / os / lib headers too.

Examples:
    scripts/analysis/struct_layout_audit.py                       # port surface
    scripts/analysis/struct_layout_audit.py --filter Character    # one class
    scripts/analysis/struct_layout_audit.py --unit system/char    # one subtree
    scripts/analysis/struct_layout_audit.py --all --json out.json
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
SRC = REPO / "src"
DEBUG_ELF = Path(
    "/home/free/code/milohax/milo-executable-library/rb3/"
    "Wii Proto (Bank 5) (Debug)/band_r_wii.elf"
)
CACHE = Path("/tmp/claude/struct_dwarf/target_layouts.json")

# Port surface: band3/ + system/ minus the Wii-only subdirs the port replaces.
# (mirrors the scope filter used across the decomp tooling)
OUT_OF_SCOPE_PREFIXES = (
    "system/rndwii/",
    "system/os/",
    "sdk/",
    "network/",
    "lib/",
)


# ── target DWARF extraction ──────────────────────────────────────────────────
#
# MWCC's DWARF-2 desyncs pyelftools' abbrev parser, but `readelf --debug-dump`
# handles it cleanly, so we parse readelf's regular text output. Each DIE is a
#   <DEPTH><off>: Abbrev Number: N (DW_TAG_xxx)
# header followed by `<off>  DW_AT_xxx : value` attribute lines. A class/struct
# type's direct members are DW_TAG_member DIEs one depth deeper.

_DIE_RE = re.compile(r"^\s*<(\d+)><[0-9a-f]+>:\s*Abbrev Number:\s*\d+(?:\s*\((DW_TAG_\w+)\))?")
_ATTR_RE = re.compile(r"^\s*<[0-9a-f]+>\s+(DW_AT_\w+)\s*:\s*(.*?)\s*$")
_UCONST_RE = re.compile(r"DW_OP_plus_uconst:\s*(\d+)")


def _parse_int(val: str) -> int | None:
    val = val.strip()
    try:
        return int(val, 16) if val.lower().startswith("0x") else int(val)
    except ValueError:
        return None


def _member_loc(val: str) -> int | None:
    m = _UCONST_RE.search(val)
    if m:
        return int(m.group(1))
    return _parse_int(val)


@dataclass
class _Ctx:
    depth: int
    name: str | None = None
    byte_size: int | None = None
    members: dict | None = None


def _extract_target_layouts(max_lines: int | None = None) -> dict[str, dict]:
    """Parse class/struct layouts from `readelf --debug-dump=info`.

    Returns {class_name: {"byte_size": int|None, "members": {name: offset}}},
    merged across CUs (first complete definition with members wins).
    """
    layouts: dict[str, dict] = {}
    proc = subprocess.Popen(
        ["readelf", "--debug-dump=info", str(DEBUG_ELF)],
        stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True, bufsize=1,
    )
    assert proc.stdout is not None

    stack: list[_Ctx] = []
    pending_kind: str | None = None        # "class" | "member" | None
    pending_member: dict | None = None     # {"name", "off", "ctx"}
    t0 = time.time()
    n = 0

    def finalize(ctx: _Ctx) -> None:
        if not ctx.name or ctx.name.startswith("@"):
            return
        prev = layouts.get(ctx.name)
        if prev and prev.get("members"):
            return  # keep first complete definition
        layouts[ctx.name] = {"byte_size": ctx.byte_size, "members": ctx.members or {}}

    def commit_member() -> None:
        if pending_member and pending_member.get("name") and pending_member.get("off") is not None:
            pending_member["ctx"].members[pending_member["name"]] = pending_member["off"]

    for line in proc.stdout:
        n += 1
        if max_lines and n > max_lines:
            proc.terminate()
            break
        if n % 500000 == 0:
            print(f"  [dwarf] {n} lines, {len(layouts)} classes "
                  f"({time.time() - t0:.0f}s)", file=sys.stderr)

        dm = _DIE_RE.match(line)
        if dm:
            commit_member()
            pending_member = None
            depth = int(dm.group(1))
            tag = dm.group(2)  # None for "Abbrev Number: 0" terminators
            while stack and stack[-1].depth >= depth:
                finalize(stack.pop())
            if tag in ("DW_TAG_structure_type", "DW_TAG_class_type"):
                ctx = _Ctx(depth=depth, members={})
                stack.append(ctx)
                pending_kind = "class"
            elif tag == "DW_TAG_member" and stack and depth == stack[-1].depth + 1:
                pending_member = {"name": None, "off": None, "ctx": stack[-1]}
                pending_kind = "member"
            else:
                pending_kind = None
            continue

        am = _ATTR_RE.match(line)
        if not am:
            continue
        attr, val = am.group(1), am.group(2)
        if pending_kind == "class" and stack:
            if attr == "DW_AT_name":
                stack[-1].name = val
            elif attr == "DW_AT_byte_size":
                stack[-1].byte_size = _parse_int(val)
        elif pending_kind == "member" and pending_member is not None:
            if attr == "DW_AT_name":
                pending_member["name"] = val
            elif attr == "DW_AT_data_member_location":
                pending_member["off"] = _member_loc(val)

    commit_member()
    while stack:
        finalize(stack.pop())
    proc.wait()
    return layouts


def load_target_layouts(refresh: bool, max_lines: int | None) -> dict[str, dict]:
    if not DEBUG_ELF.exists():
        print(f"missing debug ELF: {DEBUG_ELF}", file=sys.stderr)
        sys.exit(2)
    elf_mtime = DEBUG_ELF.stat().st_mtime
    if not refresh and not max_lines and CACHE.exists():
        try:
            cached = json.loads(CACHE.read_text())
            if cached.get("_elf_mtime") == elf_mtime:
                return cached["layouts"]
        except (json.JSONDecodeError, KeyError):
            pass
    print("[dwarf] parsing debug ELF via readelf (cached afterwards)...",
          file=sys.stderr)
    layouts = _extract_target_layouts(max_lines)
    if not max_lines:
        CACHE.parent.mkdir(parents=True, exist_ok=True)
        CACHE.write_text(json.dumps({"_elf_mtime": elf_mtime, "layouts": layouts}))
    print(f"[dwarf] {len(layouts)} target classes", file=sys.stderr)
    return layouts


# ── our header layouts ────────────────────────────────────────────────────────


def rel_unit(path: Path) -> str:
    try:
        return str(path.resolve().relative_to(SRC.resolve()))
    except ValueError:
        return str(path)


def in_scope(rel: str, include_all: bool) -> bool:
    if include_all:
        return True
    return not any(rel.startswith(p) for p in OUT_OF_SCOPE_PREFIXES)


def collect_our_classes(include_all: bool, filt: str | None, unit: str | None):
    """Yield (ClassInfo, rel_path) for in-scope headers that carry annotations."""
    sys.path.insert(0, str(REPO / "tools"))
    from struct_db import parse_header  # type: ignore

    for header in SRC.rglob("*.h"):
        rel = rel_unit(header)
        if unit and not rel.startswith(unit):
            continue
        if not in_scope(rel, include_all):
            continue
        for ci in parse_header(header):
            if not ci.members:
                continue
            if filt and filt not in ci.name:
                continue
            yield ci, rel


# ── comparison ────────────────────────────────────────────────────────────────


@dataclass
class Finding:
    cls: str
    unit: str
    kind: str           # OFFSET_MISMATCH | SIZE_MISMATCH
    member: str
    ours: int
    target: int
    detail: str = ""
    owner_pct: float | None = None


SEVERITY = {"OFFSET_MISMATCH": 0, "SIZE_MISMATCH": 1}


def load_owner_pcts() -> dict[str, float]:
    """Map header-relative unit (e.g. 'band3/game/BandUser') -> the defining
    TU's fuzzy match%. A layout finding is only actionable when this TU is below
    100%: a fully-matched TU proves our compiled layout already equals the
    target's, so any Bank-5 DWARF difference there is divergence noise, not a bug."""
    report = REPO / "build" / "SZBE69_B8" / "report.json"
    if not report.exists():
        return {}
    data = json.loads(report.read_text())
    out: dict[str, float] = {}
    for u in data.get("units", []):
        name = u.get("name", "")
        for prefix in ("main/", "default/"):
            if name.startswith(prefix):
                name = name[len(prefix):]
        pct = (u.get("measures", {}) or {}).get("fuzzy_match_percent")
        if pct is not None:
            out[name] = float(pct)
    return out


def compare(ci, rel: str, tgt: dict) -> list[Finding]:
    out: list[Finding] = []
    tmembers: dict[str, int] = tgt.get("members", {}) or {}
    # Strip the qualified prefix used for nested classes (Foo::Bar -> Bar) when
    # the DWARF only has the leaf name. We already looked the class up by leaf.
    for m in ci.members:
        if m.name in tmembers:
            tof = tmembers[m.name]
            if tof != m.offset:
                out.append(Finding(ci.name, rel, "OFFSET_MISMATCH", m.name,
                                   m.offset, tof,
                                   f"ours=0x{m.offset:x} target=0x{tof:x} "
                                   f"({m.type_str})"))
    # Size sanity: our largest annotated offset vs target byte_size.
    byte_size = tgt.get("byte_size")
    if byte_size and ci.members:
        max_off = max(m.offset for m in ci.members)
        if max_off >= byte_size:
            out.append(Finding(ci.name, rel, "SIZE_MISMATCH", "(size)",
                               max_off, byte_size,
                               f"annotated member at 0x{max_off:x} >= target "
                               f"byte_size 0x{byte_size:x}"))
    return out


def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--all", action="store_true",
                    help="include sdk/network/rndwii/os/lib headers")
    ap.add_argument("--filter", help="substring match on class name")
    ap.add_argument("--unit", help="restrict to a src/ subtree, e.g. system/char")
    ap.add_argument("--refresh", action="store_true",
                    help="rebuild the cached target-DWARF layout table")
    ap.add_argument("--max-lines", type=int,
                    help="parse only the first N readelf lines (fast validation; "
                         "result is not cached)")
    ap.add_argument("--max-pct", type=float, default=99.0,
                    help="only treat findings as actionable when the defining TU "
                         "is below this fuzzy%% (default 99.0; a near-matched TU "
                         "has a provably-correct layout, so its DWARF deltas are "
                         "Bank-5 proto divergence)")
    ap.add_argument("--show-uniform", action="store_true",
                    help="also show classes whose members are ALL shifted by the "
                         "same delta — a uniform shift means the class's own "
                         "layout is internally consistent and only its base "
                         "differs, which is almost always Bank-5 divergence")
    ap.add_argument("--show-matched", action="store_true",
                    help="also show findings in TUs at/above --max-pct")
    ap.add_argument("--show-unattributed", action="store_true",
                    help="also show findings whose defining TU isn't in report.json")
    ap.add_argument("--json", metavar="FILE", help="write ALL findings to FILE")
    args = ap.parse_args()

    target = load_target_layouts(args.refresh, args.max_lines)
    owner_pcts = load_owner_pcts()

    findings: list[Finding] = []
    classes_checked = 0
    classes_matched = 0
    for ci, rel in collect_our_classes(args.all, args.filter, args.unit):
        classes_checked += 1
        tgt = target.get(ci.name) or target.get(ci.name.split("::")[-1])
        if not tgt:
            continue
        # Guard against leaf-name collisions (e.g. several nested "Element"
        # classes): require at least one shared member name before trusting
        # the match, otherwise we'd diff two unrelated classes.
        tmembers = tgt.get("members", {}) or {}
        if not any(m.name in tmembers for m in ci.members):
            continue
        classes_matched += 1
        owner = owner_pcts.get(rel[:-2] if rel.endswith(".h") else rel)
        for f in compare(ci, rel, tgt):
            f.owner_pct = owner
            findings.append(f)

    # Group by class so we can classify the shift shape, then bucket. A finding
    # is only worth a human's time when (a) the defining TU is below --max-pct
    # (a near-matched TU has a provably-correct layout) AND (b) the shift is NOT
    # uniform (a uniform shift means the class's own members are internally
    # consistent and only its base offset differs — the BandUser signature of
    # Bank-5 proto divergence, not a real Bank-8 bug).
    from collections import defaultdict
    by_class: dict[tuple, list[Finding]] = defaultdict(list)
    for f in findings:
        by_class[(f.cls, f.unit, f.owner_pct)].append(f)

    internal: list[list[Finding]] = []   # MIXED / SIZE — real-bug candidates
    uniform: list[list[Finding]] = []    # all members shift by one delta
    matched: list[list[Finding]] = []    # TU at/above --max-pct
    unattributed: list[list[Finding]] = []

    for (cls, unit, pct), items in by_class.items():
        if pct is None:
            unattributed.append(items)
            continue
        if pct >= args.max_pct:
            matched.append(items)
            continue
        deltas = {f.target - f.ours for f in items if f.kind == "OFFSET_MISMATCH"}
        if len(deltas) == 1 and all(f.kind == "OFFSET_MISMATCH" for f in items):
            uniform.append(items)
        else:
            internal.append(items)

    def render(groups: list[list[Finding]], title: str) -> None:
        if not groups:
            return
        groups.sort(key=lambda g: (g[0].owner_pct if g[0].owner_pct is not None else 999,
                                    g[0].cls))
        total = sum(len(g) for g in groups)
        print(f"\n########## {title} — {len(groups)} classes, {total} findings ##########")
        for g in groups:
            f0 = g[0]
            pct = f"{f0.owner_pct:.2f}%" if f0.owner_pct is not None else "??"
            deltas = sorted({f.target - f.ours for f in g if f.kind == 'OFFSET_MISMATCH'})
            shape = f"deltas={deltas}" if deltas else "size-only"
            print(f"\n  {f0.cls}  TU={f0.unit} ({pct})  {shape}")
            for f in sorted(g, key=lambda x: x.ours):
                print(f"      [{f.kind}] {f.member}: {f.detail}")

    render(internal, "CANDIDATES — non-uniform shift in sub-%g%% TU (most likely real)" % args.max_pct)
    if args.show_uniform:
        render(uniform, "UNIFORM SHIFT — base/proto divergence, usually Bank-5 noise")
    if args.show_matched:
        render(matched, "MATCHED-TU — layout already correct for Bank-8")
    if args.show_unattributed:
        render(unattributed, "UNATTRIBUTED — no defining TU in report.json")

    print("\n=== summary ===")
    print(f"  classes with annotations checked : {classes_checked}")
    print(f"  matched to target DWARF          : {classes_matched}")
    print(f"  CANDIDATES (non-uniform, TU<{args.max_pct:g}%) : {len(internal)} classes")
    print(f"  uniform-shift (likely Bank-5)    : {len(uniform)} classes"
          f"{'' if args.show_uniform else '  (--show-uniform)'}")
    print(f"  matched-TU (>= {args.max_pct:g}%)          : {len(matched)} classes"
          f"{'' if args.show_matched else '  (--show-matched)'}")
    print(f"  unattributed (no TU in report)   : {len(unattributed)} classes"
          f"{'' if args.show_unattributed else '  (--show-unattributed)'}")
    print("\n  NOTE: the only DWARF is the Bank-5 proto; confirm any candidate "
          "against\n  the Bank-8 target asm (objdiff/Ghidra) before editing a header.")
    if not internal:
        print("\n  no non-uniform candidates")

    if args.json:
        Path(args.json).write_text(json.dumps([f.__dict__ for f in findings], indent=2))
        print(f"\nwrote {args.json} ({len(findings)} total findings)", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
