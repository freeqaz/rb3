#!/usr/bin/env python3
"""
Find functions whose target asm references @STRING@OtherMethod symbols —
a strong signal the original called OtherMethod via qualified syntax to
force MWCC to inline it (bypassing virtual dispatch).

Outputs a ranked candidate list. For each candidate, lists the referenced
methods so the fix is checklist-driven: add the method to its class,
move the inlined logic + assert into it, replace the call site with
`obj->Class::Method(args)`.

See: memory/feedback_qualified_inline_call.md
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
REPORT = ROOT / "build" / "SZBE69_B8" / "report.json"
ASM_DIR = ROOT / "build" / "SZBE69_B8" / "asm"
OBJ_DIR = ROOT / "build" / "SZBE69_B8" / "obj"

# .fn header in dtk-emitted asm: ".fn MangledName, global"
FN_HEADER_RE = re.compile(r'^\.fn\s+([^,]+),')
END_FN_RE = re.compile(r'^\.endfn\b')
# String references: "@STRING@MangledName" or "@STRING@MangledName@N"
STRING_REF_RE = re.compile(r'"@STRING@([^"@]+)(?:@\d+)?"')

# Categories we care about for the port surface.
PORT_CATEGORIES = {"game", "engine"}
SKIP_DIRS = ("/rndwii/", "/os/", "/synthwii/", "/usbwii/")


def asm_path_for_unit(source_path: str) -> Path | None:
    """Map src/foo/bar/Baz.cpp -> build/SZBE69_B8/asm/foo/bar/Baz.s."""
    if not source_path.startswith("src/"):
        return None
    rel = source_path[len("src/"):]
    if rel.endswith(".cpp"):
        rel = rel[:-4] + ".s"
    elif rel.endswith(".c"):
        rel = rel[:-2] + ".s"
    else:
        return None
    p = ASM_DIR / rel
    return p if p.exists() else None


def our_strings_for_unit(source_path: str) -> set[str]:
    """Return the set of @STRING@<mangled> symbols present in our compiled .o."""
    if not source_path.startswith("src/"):
        return set()
    rel = source_path[len("src/"):]
    if rel.endswith(".cpp"):
        rel = rel[:-4] + ".o"
    elif rel.endswith(".c"):
        rel = rel[:-2] + ".o"
    else:
        return set()
    p = OBJ_DIR / rel
    if not p.exists():
        return set()
    data = p.read_bytes()
    hits = re.findall(rb"@STRING@([A-Za-z0-9_<>,]+)", data)
    return {h.decode() for h in hits}


def scan_unit(asm_file: Path) -> dict[str, set[str]]:
    """Return {fn_mangled: {referenced_method_mangled, ...}} for one asm file."""
    refs: dict[str, set[str]] = defaultdict(set)
    cur_fn: str | None = None
    with asm_file.open() as f:
        for line in f:
            m = FN_HEADER_RE.match(line)
            if m:
                cur_fn = m.group(1)
                continue
            if END_FN_RE.match(line):
                cur_fn = None
                continue
            if cur_fn is None:
                continue
            for sm in STRING_REF_RE.findall(line):
                if sm != cur_fn:
                    refs[cur_fn].add(sm)
    return refs


def in_scope(unit_md: dict) -> bool:
    if not any(c in PORT_CATEGORIES for c in unit_md.get("progress_categories", [])):
        return False
    sp = unit_md.get("source_path", "")
    return not any(d in sp for d in SKIP_DIRS)


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--min-percent", type=float, default=0.0,
                    help="Minimum fuzzy match percent (default: 0).")
    ap.add_argument("--max-percent", type=float, default=99.99,
                    help="Maximum fuzzy match percent (default: 99.99).")
    ap.add_argument("--min-size", type=int, default=100,
                    help="Minimum function size in bytes (default: 100).")
    ap.add_argument("--filter", default="",
                    help="Substring filter on source path (e.g. 'band3/').")
    ap.add_argument("--limit", type=int, default=40,
                    help="Max rows to print (default: 40).")
    ap.add_argument("--json", action="store_true",
                    help="Emit JSON instead of human-readable table.")
    ap.add_argument("--strict", action="store_true",
                    help="Only show rows with at least one @STRING@ ref missing "
                         "from our compiled .o (strongest signal).")
    args = ap.parse_args(argv)

    if not REPORT.exists():
        print(f"ERROR: {REPORT} missing. Run tools/ninja-locked first.",
              file=sys.stderr)
        return 1

    report = json.loads(REPORT.read_text())

    rows = []
    for unit in report.get("units", []):
        md = unit.get("metadata", {})
        if not in_scope(md):
            continue
        sp = md.get("source_path", "")
        if args.filter and args.filter not in sp:
            continue
        asm = asm_path_for_unit(sp)
        if not asm:
            continue
        # Build the set of partial-match fns in this unit (mangled name -> info).
        wanted = {}
        for fn in unit.get("functions", []):
            fp = float(fn.get("fuzzy_match_percent", 0))
            if fp >= 100.0:
                continue
            if not (args.min_percent <= fp <= args.max_percent):
                continue
            sz = int(fn.get("size", 0))
            if sz < args.min_size:
                continue
            wanted[fn["name"]] = {
                "match": fp, "size": sz,
                "demangled": fn.get("metadata", {}).get("demangled_name", "")
            }
        if not wanted:
            continue
        refs = scan_unit(asm)
        ours = our_strings_for_unit(sp)
        for fn_mangled, info in wanted.items():
            r = refs.get(fn_mangled, set())
            if not r:
                continue
            # Annotate each ref: missing-in-our-TU is the strong signal,
            # present-in-our-TU is weaker (might still be a per-fn miss).
            annotated = []
            actionable_count = 0
            for m in sorted(r):
                in_ours = m in ours
                annotated.append({"ref": m, "in_ours_tu": in_ours})
                if not in_ours:
                    actionable_count += 1
            # If --strict, drop rows with zero strong signals.
            if args.strict and actionable_count == 0:
                continue
            rows.append({
                "unit": sp,
                "fn": fn_mangled,
                "demangled": info["demangled"],
                "match": info["match"],
                "size": info["size"],
                "refs": annotated,
                "strong_count": actionable_count,
            })

    # Rank: strong-signal rows first, then highest match%, then biggest size.
    rows.sort(key=lambda r: (-r["strong_count"], -r["match"], -r["size"]))
    rows = rows[: args.limit]

    if args.json:
        json.dump(rows, sys.stdout, indent=2)
        sys.stdout.write("\n")
        return 0

    if not rows:
        print("(no candidates)")
        return 0

    for r in rows:
        tag = f"[{r['strong_count']} strong]" if r["strong_count"] else "[weak only]"
        print(f"{r['match']:6.2f}%  {r['size']:>5}b  {tag}  {r['unit']}")
        print(f"        fn: {r['demangled'] or r['fn']}")
        for ann in r["refs"]:
            mark = "MISSING" if not ann["in_ours_tu"] else "in-TU"
            print(f"        {mark:>7}  {ann['ref']}")
        print()
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
