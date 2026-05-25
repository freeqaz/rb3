#!/usr/bin/env python3
"""
Detect inlining-gap candidates between target asm and our compiled .o.

Modes:
  qualified-call    Target asm references @STRING@MangledMethod inside a
                    partial-match function, but our compiled function does
                    NOT. Suggests the original called the method via
                    qualified syntax (obj->Class::Method(args)) to bypass
                    virtual dispatch and force MWCC to inline.
                    See: feedback_qualified_inline_call.md

  missing-inline    (Phase 2) Target inlines a helper our function calls
                    out-of-line. Suggests adding `inline` to the helper.
                    See: feedback_inline_container_methods.md

  missing-noinline  (Phase 2) Target calls a helper out-of-line that our
                    function inlines. Suggests __declspec(noinline).
                    See: feedback_cw_noinline_pattern.md

  all               Run all modes, tag each row by mode.

Per-function accuracy:
  Strict mode in qualified-call uses objdiff-cli per function to read the
  base side's @STRING@ references — not a TU-level .o scan. Cost is
  ~18ms per candidate via subprocess; cached in memory for the run.
"""
from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
REPORT = ROOT / "build" / "SZBE69_B8" / "report.json"
ASM_DIR = ROOT / "build" / "SZBE69_B8" / "asm"
OBJDIFF_CONFIG = ROOT / "objdiff.json"
OBJDIFF_CLI = ROOT / "build" / "tools" / "objdiff-cli"

# .fn header in dtk-emitted asm: ".fn MangledName, global"
FN_HEADER_RE = re.compile(r'^\.fn\s+([^,]+),')
END_FN_RE = re.compile(r'^\.endfn\b')
# String references: "@STRING@MangledName" or "@STRING@MangledName@N"
STRING_REF_RE = re.compile(r'"@STRING@([^"@]+)(?:@\d+)?"')

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


def scan_target_unit(asm_file: Path) -> dict[str, set[str]]:
    """Return {fn_mangled: {referenced_method_mangled, ...}} for target asm.

    Only collects refs of the form @STRING@MangledMethod (the assert-string
    symbols MWCC emits per-function). Excludes self-references."""
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


_OBJDIFF_UNIT_MAP: dict[str, str] | None = None


def load_objdiff_unit_map() -> dict[str, str]:
    """Map src/foo/bar/Baz.cpp -> main/foo/bar/Baz (objdiff unit name)."""
    global _OBJDIFF_UNIT_MAP
    if _OBJDIFF_UNIT_MAP is not None:
        return _OBJDIFF_UNIT_MAP
    cfg = json.loads(OBJDIFF_CONFIG.read_text())
    m: dict[str, str] = {}
    for u in cfg.get("units", []):
        sp = u.get("metadata", {}).get("source_path")
        if sp:
            m[sp] = u["name"]
    _OBJDIFF_UNIT_MAP = m
    return m


_PER_FN_CACHE: dict[tuple[str, str], dict | None] = {}


def per_fn_objdiff(unit: str, symbol: str) -> dict | None:
    """Run `objdiff-cli diff` once per (unit, symbol). Returns parsed JSON
    or None on failure. Results cached in-process."""
    key = (unit, symbol)
    if key in _PER_FN_CACHE:
        return _PER_FN_CACHE[key]
    cmd = [str(OBJDIFF_CLI), "diff", "-u", unit, symbol,
           "--include-instructions", "--format", "json"]
    try:
        out = subprocess.run(cmd, capture_output=True, text=True,
                             timeout=30, cwd=str(ROOT))
    except subprocess.TimeoutExpired:
        _PER_FN_CACHE[key] = None
        return None
    if out.returncode != 0 or not out.stdout.strip():
        _PER_FN_CACHE[key] = None
        return None
    try:
        d = json.loads(out.stdout)
    except json.JSONDecodeError:
        _PER_FN_CACHE[key] = None
        return None
    _PER_FN_CACHE[key] = d
    return d


def extract_side_symbols(diff: dict, side: str) -> tuple[set[str], list[str]]:
    """Return (set of Symbol typed_args on this side,
               list of bl-target symbols in order on this side)."""
    syms: set[str] = set()
    bls: list[str] = []
    for insn in diff.get("instructions", []):
        s = insn.get(side)
        if not s:
            continue
        opcode = s.get("opcode", "")
        for ta in s.get("typed_args", []):
            if ta.get("type") == "Symbol":
                v = ta.get("value")
                if not v:
                    continue
                syms.add(v)
                if opcode == "bl":
                    bls.append(v)
    return syms, bls


def our_strings_for_fn(unit: str, symbol: str) -> set[str] | None:
    """Per-function set of @STRING@MangledName refs on our compiled side.
    Returns None if objdiff couldn't probe (treat as unknown)."""
    d = per_fn_objdiff(unit, symbol)
    if d is None:
        return None
    base_syms, _ = extract_side_symbols(d, "base")
    return {s for s in base_syms if s.startswith("@STRING@")}


def in_scope(unit_md: dict) -> bool:
    if not any(c in PORT_CATEGORIES for c in unit_md.get("progress_categories", [])):
        return False
    sp = unit_md.get("source_path", "")
    return not any(d in sp for d in SKIP_DIRS)


def iter_wanted(args, report: dict):
    """Yield (unit_metadata, asm_path, {fn_mangled: info}) for each in-scope TU
    that has at least one partial fn passing the size/percent filters."""
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
        if wanted:
            yield md, asm, wanted


def find_qualified_call_rows(args, report: dict, unit_map: dict[str, str]) -> list[dict]:
    """Mode: qualified-call. Per-fn accuracy on the 'ours' side via objdiff."""
    rows = []
    for md, asm, wanted in iter_wanted(args, report):
        sp = md["source_path"]
        target_refs = scan_target_unit(asm)
        objdiff_unit = unit_map.get(sp)
        for fn_mangled, info in wanted.items():
            r = target_refs.get(fn_mangled, set())
            if not r:
                continue
            ours = our_strings_for_fn(objdiff_unit, fn_mangled) if objdiff_unit else None
            annotated = []
            strong = 0
            for m in sorted(r):
                full = f"@STRING@{m}"
                if ours is None:
                    in_ours: bool | None = None
                else:
                    in_ours = full in ours
                annotated.append({"ref": m, "in_ours_fn": in_ours})
                if in_ours is False:
                    strong += 1
            if args.strict and strong == 0:
                continue
            rows.append({
                "mode": "qualified-call",
                "unit": sp,
                "fn": fn_mangled,
                "demangled": info["demangled"],
                "match": info["match"],
                "size": info["size"],
                "refs": annotated,
                "strong_count": strong,
            })
    return rows


def find_missing_inline_rows(args, report: dict, unit_map: dict[str, str]) -> list[dict]:
    """Phase 2 stub: target inlines, ours has `bl X`."""
    return []


def find_missing_noinline_rows(args, report: dict, unit_map: dict[str, str]) -> list[dict]:
    """Phase 2 stub: target has `bl X`, ours inlines."""
    return []


MODE_FUNCS = {
    "qualified-call": find_qualified_call_rows,
    "missing-inline": find_missing_inline_rows,
    "missing-noinline": find_missing_noinline_rows,
}


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--mode", default="all",
                    choices=["qualified-call", "missing-inline",
                             "missing-noinline", "all"],
                    help="Which signal(s) to scan (default: all).")
    ap.add_argument("--min-percent", type=float, default=0.0)
    ap.add_argument("--max-percent", type=float, default=99.99)
    ap.add_argument("--min-size", type=int, default=100,
                    help="Minimum function size in bytes (default: 100).")
    ap.add_argument("--filter", default="",
                    help="Substring filter on source path.")
    ap.add_argument("--limit", type=int, default=40,
                    help="Max rows to print (default: 40).")
    ap.add_argument("--json", action="store_true",
                    help="Emit JSON instead of human-readable table.")
    ap.add_argument("--strict", action="store_true",
                    help="Only show rows with at least one strong signal.")
    args = ap.parse_args(argv)

    if not REPORT.exists():
        print(f"ERROR: {REPORT} missing. Run tools/ninja-locked first.",
              file=sys.stderr)
        return 1
    if not OBJDIFF_CONFIG.exists():
        print(f"ERROR: {OBJDIFF_CONFIG} missing. Run python3 configure.py first.",
              file=sys.stderr)
        return 1

    report = json.loads(REPORT.read_text())
    unit_map = load_objdiff_unit_map()

    modes = ["qualified-call", "missing-inline", "missing-noinline"] \
        if args.mode == "all" else [args.mode]

    all_rows: list[dict] = []
    for m in modes:
        all_rows.extend(MODE_FUNCS[m](args, report, unit_map))

    all_rows.sort(key=lambda r: (-r["strong_count"], -r["match"], -r["size"]))
    all_rows = all_rows[: args.limit]

    if args.json:
        json.dump(all_rows, sys.stdout, indent=2)
        sys.stdout.write("\n")
        return 0

    if not all_rows:
        print("(no candidates)")
        return 0

    for r in all_rows:
        tag = f"[{r['mode']}: {r['strong_count']} strong]" if r["strong_count"] \
            else f"[{r['mode']}: weak only]"
        print(f"{r['match']:6.2f}%  {r['size']:>5}b  {tag}  {r['unit']}")
        print(f"        fn: {r['demangled'] or r['fn']}")
        for ann in r["refs"]:
            v = ann.get("in_ours_fn")
            mark = "MISSING" if v is False else ("in-fn" if v is True else "unknown")
            print(f"        {mark:>7}  {ann['ref']}")
        print()
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
