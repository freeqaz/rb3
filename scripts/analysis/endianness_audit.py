#!/usr/bin/env python3
"""Audit endianness / bit-packing hazards that a little-endian native port hits.

Every RB3-era console (Wii, X360, PS3) is big-endian, so all serialized data —
save files, .milo scene blobs, packed store metadata, network packets — is stored
big-endian, and the MWCC compiler packs C bitfields MSB-first. A native port on a
little-endian host (x86 / ARM) reads those same bytes with LSB-first bitfield
packing and the opposite byte order, so any persisted bitfield struct silently
decodes to garbage unless the (de)serializer compensates.

This class of bug is INVISIBLE to match%: the Wii build is big-endian, so it
matches the target perfectly; the corruption only appears once you run on a
little-endian host. Nothing in the decomp pipeline catches it — hence this sweep.

What it reports:
  BITFIELD STRUCTS   every class/struct carrying C bitfields, grouped with their
                     bit layout. Ranked by a serialization-risk heuristic:
                       HIGH  the type co-occurs with serialization (BinStream
                             Save/Load, operator>>/<<, EndianFix, block Read) ->
                             its bits are persisted big-endian, so the port MUST
                             read/write them endian-aware.
                       LOW   no serialization signal -> looks runtime-only; the
                             bit order never leaves memory, so the port is fine.
  ENDIAN HANDLING    inventory of existing EndianFix / EndianSwap surface — the
                     code that already compensates, i.e. the port's reference for
                     what "doing it right" looks like (and what's conspicuously
                     missing for a HIGH-risk struct).

This is a heuristic source sweep, not a prover: a HIGH tag means "confirm the
serializer is endian-aware for the port," not "definitely broken."

Default scope is the port surface (band3/ + platform-agnostic system/); --all adds
sdk / network / Wii-only dirs.

Examples:
    scripts/analysis/endianness_audit.py                 # ranked hazard catalog
    scripts/analysis/endianness_audit.py --risk high     # only HIGH-risk structs
    scripts/analysis/endianness_audit.py --endian-map     # existing endian surface
    scripts/analysis/endianness_audit.py --json out.json
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

# REPO/SRC are reassigned in main() from --repo so the same sweep runs against
# any Milo-engine checkout (rb3 Wii, dc3 Xbox360 — both big-endian, both native
# port targets). Default is the repo this script lives in.
REPO = Path(__file__).resolve().parents[2]
SRC = REPO / "src"

# Platform/third-party dirs the port replaces wholesale (superset across the
# rb3 Wii and dc3 Xbox360 trees). Best-effort; use --all to bypass.
OUT_OF_SCOPE_PREFIXES = (
    "system/rndwii/",
    "system/rndxenon/",
    "system/os/",
    "sdk/",
    "xdk/",
    "network/",
    "lib/",
)

# A bitfield member: `<type> <name> : <width>;` (optionally with a trailing
# // comment). Excludes `case`/labels/ternaries handled by the caller.
BITFIELD_RE = re.compile(
    r"^\s*([A-Za-z_][\w:<>,\s\*&]*?)\s+([A-Za-z_]\w*)\s*:\s*(\d+)\s*;")
CLASS_RE = re.compile(r"^\s*(class|struct)\s+(\w+)")

# Serialization signals that imply a type's bytes are persisted big-endian.
SERIAL_TOKENS = re.compile(
    r"\bBinStream\b|\bEndianFix\b|\bSaveFixed\b|\bLoadFixed\b"
    r"|\boperator\s*>>\b|\boperator\s*<<\b|\bSyncRead\b|\bSyncWrite\b"
    r"|\bSave\s*\(|\bLoad\s*\(")


def rel(path: Path) -> str:
    try:
        return str(path.resolve().relative_to(SRC.resolve()))
    except ValueError:
        return str(path)


def in_scope(r: str, include_all: bool) -> bool:
    return include_all or not any(r.startswith(p) for p in OUT_OF_SCOPE_PREFIXES)


@dataclass
class BitField:
    name: str
    type_str: str
    width: int
    line: int


@dataclass
class BitfieldStruct:
    cls: str
    file: str
    fields: list[BitField] = field(default_factory=list)
    risk: str = "LOW"
    signals: list[str] = field(default_factory=list)


def parse_bitfields(path: Path) -> list[BitfieldStruct]:
    """Brace-tracked scan: collect bitfield members per enclosing class/struct."""
    try:
        lines = path.read_text(encoding="utf-8", errors="ignore").split("\n")
    except Exception:
        return []
    structs: list[BitfieldStruct] = []
    # stack of (class_name, brace_depth_at_open)
    stack: list[tuple[str, int]] = []
    depth = 0
    current: dict[str, BitfieldStruct] = {}  # class_name -> struct (within file)

    for i, line in enumerate(lines):
        cm = CLASS_RE.match(line)
        if cm and "{" in line.split("//")[0]:
            name = cm.group(2)
            qual = "::".join([s for s, _ in stack] + [name])
            stack.append((qual, depth))
        # opening of a class declared across lines (no brace yet) — approximate:
        elif cm and "{" not in line and ";" not in line:
            name = cm.group(2)
            qual = "::".join([s for s, _ in stack] + [name])
            stack.append((qual, depth))

        code = line.split("//")[0]
        depth += code.count("{") - code.count("}")
        while stack and depth <= stack[-1][1]:
            stack.pop()

        if not stack:
            continue
        bm = BITFIELD_RE.match(line)
        if not bm:
            continue
        type_str = bm.group(1).strip()
        # filter out non-member matches (access labels, control flow)
        if type_str in ("public", "private", "protected", "case", "default", "return"):
            continue
        qual = stack[-1][0]
        bs = current.get(qual)
        if bs is None:
            bs = BitfieldStruct(cls=qual, file=rel(path))
            current[qual] = bs
            structs.append(bs)
        bs.fields.append(BitField(bm.group(2), type_str, int(bm.group(3)), i + 1))
    return structs


def assess_risk(bs: BitfieldStruct) -> None:
    """Heuristic: does this bitfield struct's type get serialized big-endian?

    Scans the defining header and its sibling .cpp for serialization tokens that
    co-occur with the (leaf) class name."""
    leaf = bs.cls.split("::")[-1]
    candidates = [SRC / bs.file]
    if bs.file.endswith(".h"):
        candidates.append(SRC / (bs.file[:-2] + ".cpp"))
    signals: set[str] = set()
    for path in candidates:
        if not path.exists():
            continue
        text = path.read_text(encoding="utf-8", errors="ignore")
        if f"EndianFix" in text and (leaf in text):
            signals.add("EndianFix")
        # Look for serialization functions/operators that mention the class.
        for m in SERIAL_TOKENS.finditer(text):
            # cheap proximity: same 400-char window as the class name
            lo = max(0, m.start() - 400)
            hi = min(len(text), m.end() + 400)
            if leaf in text[lo:hi]:
                tok = m.group(0).split("(")[0].strip()
                signals.add(tok)
    bs.signals = sorted(signals)
    bs.risk = "HIGH" if signals else "LOW"


def collect_endian_surface(include_all: bool) -> list[tuple[str, int, str]]:
    """Inventory of EndianFix/EndianSwap declarations across the tree."""
    out: list[tuple[str, int, str]] = []
    pat = re.compile(r"\b(EndianFix\w*|EndianSwap\w*|SwapBytes)\b\s*\(")
    for path in SRC.rglob("*.h"):
        r = rel(path)
        if not in_scope(r, include_all):
            continue
        for i, line in enumerate(path.read_text(errors="ignore").split("\n")):
            if pat.search(line):
                out.append((r, i + 1, line.strip()[:90]))
    return out


def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--repo", metavar="ROOT",
                    help="repo root to sweep (default: this script's repo). Use to "
                         "run the same sweep against another Milo checkout, e.g. "
                         "--repo /home/free/code/milohax/dc3-decomp")
    ap.add_argument("--all", action="store_true",
                    help="include sdk/network/rndwii/rndxenon/os/lib")
    ap.add_argument("--risk", choices=["high", "low", "all"], default="all",
                    help="filter bitfield structs by serialization risk")
    ap.add_argument("--filter", help="substring match on class name")
    ap.add_argument("--endian-map", action="store_true",
                    help="also print the existing EndianFix/EndianSwap surface")
    ap.add_argument("--json", metavar="FILE", help="write findings to FILE")
    args = ap.parse_args()

    if args.repo:
        global REPO, SRC
        REPO = Path(args.repo).resolve()
        SRC = REPO / "src"
        if not SRC.is_dir():
            print(f"no src/ under {REPO}", file=sys.stderr)
            return 2
    print(f"[scope] sweeping {SRC}", file=sys.stderr)

    structs: list[BitfieldStruct] = []
    for path in SRC.rglob("*.h"):
        r = rel(path)
        if not in_scope(r, args.all):
            continue
        for bs in parse_bitfields(path):
            if args.filter and args.filter not in bs.cls:
                continue
            assess_risk(bs)
            structs.append(bs)

    high = [b for b in structs if b.risk == "HIGH"]
    low = [b for b in structs if b.risk == "LOW"]
    show = structs
    if args.risk == "high":
        show = high
    elif args.risk == "low":
        show = low
    show.sort(key=lambda b: (0 if b.risk == "HIGH" else 1, b.file, b.cls))

    cur_risk = None
    for b in show:
        if b.risk != cur_risk:
            cur_risk = b.risk
            label = ("HIGH RISK — serialized bitfields (port must read/write endian-aware)"
                     if b.risk == "HIGH" else
                     "LOW RISK — no serialization signal (likely runtime-only)")
            print(f"\n########## {label} ##########")
        sig = f"  signals={b.signals}" if b.signals else ""
        nbits = sum(f.width for f in b.fields)
        print(f"\n  {b.cls}  ({b.file})  {len(b.fields)} bitfields / {nbits} bits{sig}")
        for f in b.fields:
            print(f"      {f.type_str} {f.name} : {f.width}   (L{f.line})")

    if args.endian_map:
        surface = collect_endian_surface(args.all)
        print(f"\n########## EXISTING ENDIAN SURFACE ({len(surface)} decls) ##########")
        for r, ln, txt in sorted(surface):
            print(f"  {r}:{ln}  {txt}")

    print("\n=== summary ===")
    print(f"  bitfield structs (in scope) : {len(structs)}")
    print(f"  HIGH risk (serialized)      : {len(high)}")
    print(f"  LOW  risk (runtime-only)    : {len(low)}")

    if args.json:
        payload = [
            {"cls": b.cls, "file": b.file, "risk": b.risk, "signals": b.signals,
             "fields": [{"name": f.name, "type": f.type_str, "width": f.width,
                         "line": f.line} for f in b.fields]}
            for b in structs
        ]
        Path(args.json).write_text(json.dumps(payload, indent=2))
        print(f"\nwrote {args.json}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
