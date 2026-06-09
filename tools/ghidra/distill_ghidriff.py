#!/usr/bin/env python3
"""Distill ghidriff's giant pdiff JSON into a small per-symbol divergence index.

ghidriff (run_ghidriff.sh, --no-symbols) emits a ~3 GB pdiff JSON whose bulk is
per-function decompiled `code` blobs. This streams it ONCE (ijson, yajl2_c) and
writes a small index keyed by the **Bank 8 mangled symbol**, plus the rename map.

WHY ADDRESS, NOT NAME, IS THE JOIN KEY
--------------------------------------
ghidriff's `name`/`fullname` are Ghidra's *demangled* display names
(`AppDebugModal(bool_&,_char_*,_bool)`), not the CodeWarrior mangled symbols our
maps / report.json / analyze-function use. So we join on the `address` field:
  Bank 8 func: new.address (modified) / address (added) -> Bank 8 map -> mangled
  Bank 5 func: old.address (modified) / address (deleted) -> Bank 5 map -> mangled

WHAT GHIDRIFF PUTS WHERE (with --no-symbols, name-blind structural matching)
  functions.modified[]  matched pair whose body changed (10k). Has old{} + new{}
                        each with address/length/sig/code + ratios + match_types
                        + diff_type[]. A RENAME = old/new mangled (by addr) differ
                        (also flagged by "fullname" in diff_type).
  functions.added[]     Bank-8-only (unmatched in new) -> NO_DWARF
  functions.deleted[]   Bank-5-only (unmatched in old) -> NO_TARGET (rename src pool)
  (44k byte-identical matches are NOT in the pdiff -> TRUST by complement: any
   Bank 8 map function absent from this index was an identical match.)

VERDICT (m_ratio-led: mnemonic similarity ~= algorithm/source-intent similarity)
  byte/instr/graph-exact match_type            -> TRUST
  m_ratio >= 0.95                              -> TRUST   (same mnemonic stream)
  m_ratio >= 0.75                              -> CAUTION
  m_ratio <  0.75                              -> MISLEADING
  (i_ratio used as fallback when m_ratio absent)

USAGE (no Ghidra needed — pure JSON streaming):
  build/SZBE69_B8/ghidra/ghidriff-venv/bin/python tools/ghidra/distill_ghidriff.py
  # writes  build/SZBE69_B8/ghidra/ghidriff/divergence_index.json
  #         build/SZBE69_B8/ghidra/ghidriff/renames.json
  # and prints a corpus report.
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

RB3 = Path(__file__).resolve().parents[2]
GHIDRIFF_DIR = RB3 / "build" / "SZBE69_B8" / "ghidra" / "ghidriff"
DEFAULT_PDIFF = (GHIDRIFF_DIR / "json" /
                 "band_r_wii.elf-bank8_target.elf.ghidriff.json")
DEFAULT_INDEX_OUT = GHIDRIFF_DIR / "divergence_index.json"
DEFAULT_RENAMES_OUT = GHIDRIFF_DIR / "renames.json"
BANK8_MAP = RB3 / "orig" / "SZBE69_B8" / "files" / "band_r_wii.map"
BANK5_MAP = (RB3.parent / "milo-executable-library" / "rb3"
             / "Wii Proto (Bank 5) (Debug)" / "band_r_wii.map")

_LINE = re.compile(
    r"^  [0-9a-fA-F]{8} [0-9a-fA-F]{6} ([0-9a-fA-F]{8}) [0-9a-fA-F]{8}\s+\d+ (\S.*?) \t"
)
_BYTE_EXACT = {"ExactBytesFunctionHasher", "ExactInstructionsFunctionHasher",
               "StructuralGraphExactHash"}


def _addr_to_sym(path: Path) -> dict:
    """virtual address (int) -> mangled symbol (first occurrence wins)."""
    out: dict[int, str] = {}
    for line in path.read_text(errors="replace").splitlines():
        m = _LINE.match(line)
        if not m:
            continue
        addr, name = int(m.group(1), 16), m.group(2)
        if name.startswith("."):
            continue
        out.setdefault(addr, name)
    return out


def _hexaddr(s):
    try:
        return int(s, 16)
    except (TypeError, ValueError):
        return None


def _verdict(match_types, m_ratio, i_ratio):
    if set(match_types) & _BYTE_EXACT:
        return "TRUST"
    r = m_ratio if m_ratio is not None else None
    if r is not None:
        if r >= 0.95:
            return "TRUST"
        if r >= 0.75:
            return "CAUTION"
        return "MISLEADING"
    if i_ratio is None:
        return "UNKNOWN"
    if i_ratio >= 0.90:
        return "TRUST"
    if i_ratio >= 0.70:
        return "CAUTION"
    return "MISLEADING"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--pdiff", type=Path, default=DEFAULT_PDIFF)
    ap.add_argument("--index-out", type=Path, default=DEFAULT_INDEX_OUT)
    ap.add_argument("--renames-out", type=Path, default=DEFAULT_RENAMES_OUT)
    args = ap.parse_args()

    if not args.pdiff.exists():
        print(f"error: pdiff not found: {args.pdiff}", file=sys.stderr)
        return 1

    import ijson  # yajl2_c backend (fast, bounded memory)

    b5_a2s = _addr_to_sym(BANK5_MAP)
    b8_a2s = _addr_to_sym(BANK8_MAP)
    print(f"[distill] Bank5 addr->sym={len(b5_a2s)} Bank8 addr->sym={len(b8_a2s)}",
          file=sys.stderr)

    functions: dict[str, dict] = {}   # keyed by Bank8 mangled
    bank5_only: dict[str, dict] = {}  # keyed by Bank5 mangled (deleted)
    renames: list[dict] = []
    counts = {"modified": 0, "added": 0, "deleted": 0, "modified_no_b8sym": 0}

    # --- pass 1: modified (the changed matched pairs; renames live here) ---
    with args.pdiff.open("rb") as f:
        for e in ijson.items(f, "functions.modified.item", use_float=True):
            counts["modified"] += 1
            new = e.get("new", {}) or {}
            old = e.get("old", {}) or {}
            b8_addr = _hexaddr(new.get("address"))
            b5_addr = _hexaddr(old.get("address"))
            b8_sym = b8_a2s.get(b8_addr)
            b5_sym = b5_a2s.get(b5_addr)
            if b8_sym is None:
                counts["modified_no_b8sym"] += 1
                continue
            mt = e.get("match_types", []) or []
            i_r = e.get("i_ratio")
            m_r = e.get("m_ratio")
            b_r = e.get("b_ratio")
            dt = e.get("diff_type", []) or []
            # A real rename = matched pair with DIFFERENT mangled symbols (by address).
            # NOT ghidriff's "fullname" diff_type: its fullname is the *demangled* name,
            # which differs cosmetically between the real-DWARF Bank 5 and the synthetic
            # Bank 8 ELF even for the same function. Use mangled-by-address only.
            is_rename = (b5_sym is not None and b5_sym != b8_sym)
            # base identifier (strip the __<mangling> suffix). A genuine signature-rename
            # keeps the base (Foo__A -> Foo__B); trivial-body byte collisions across
            # unrelated functions do not. This is the safety gate for porting.
            base_match = (is_rename and b5_sym.split("__", 1)[0] == b8_sym.split("__", 1)[0]
                          and bool(b5_sym.split("__", 1)[0]))
            rec = {
                "verdict": _verdict(mt, m_r, i_r),
                "i_ratio": i_r, "m_ratio": m_r, "b_ratio": b_r,
                "match_types": mt, "diff_type": dt,
                "b5_sym": b5_sym, "b8_len": new.get("length"),
                "b5_len": old.get("length"), "b8_addr": new.get("address"),
                "is_rename": is_rename,
            }
            functions[b8_sym] = rec
            if is_rename and b5_sym is not None:
                b8_len = new.get("length") or 0
                # port-safe = same base identifier AND non-trivial body (>=16 bytes,
                # i.e. not a stub prone to byte-collision false matches)
                port_safe = bool(base_match) and (b8_len >= 16)
                renames.append({
                    "b8_sym": b8_sym, "b5_sym": b5_sym,
                    "b8_addr": new.get("address"), "b5_addr": old.get("address"),
                    "i_ratio": i_r, "m_ratio": m_r, "b_ratio": b_r,
                    "match_types": mt, "verdict": rec["verdict"],
                    "base_match": bool(base_match), "b8_len": b8_len,
                    "port_safe": port_safe,
                })

    # --- pass 2: added (Bank-8-only -> NO_DWARF) ---
    with args.pdiff.open("rb") as f:
        for e in ijson.items(f, "functions.added.item", use_float=True):
            counts["added"] += 1
            b8_sym = b8_a2s.get(_hexaddr(e.get("address")))
            if b8_sym is None:
                continue
            functions.setdefault(b8_sym, {
                "verdict": "NO_DWARF", "b8_len": e.get("length"),
                "b8_addr": e.get("address"), "is_rename": False,
                "note": "Bank 8 only (unmatched in Bank 5) — no DWARF; use m2c/objdiff",
            })

    # --- pass 3: deleted (Bank-5-only -> rename source pool) ---
    with args.pdiff.open("rb") as f:
        for e in ijson.items(f, "functions.deleted.item", use_float=True):
            counts["deleted"] += 1
            b5_sym = b5_a2s.get(_hexaddr(e.get("address")))
            if b5_sym is None:
                continue
            bank5_only[b5_sym] = {"b5_len": e.get("length"), "b5_addr": e.get("address")}

    # --- write outputs ---
    index = {
        "meta": {"pdiff": str(args.pdiff), "counts": counts,
                 "bank8_map_funcs": len(b8_a2s)},
        "functions": functions,
        "bank5_only": bank5_only,
    }
    args.index_out.write_text(json.dumps(index))
    args.renames_out.write_text(json.dumps(renames, indent=1))
    print(f"[distill] wrote {args.index_out} ({len(functions)} funcs) and "
          f"{args.renames_out} ({len(renames)} renames)", file=sys.stderr)

    # --- report ---
    buckets = {"TRUST": 0, "CAUTION": 0, "MISLEADING": 0, "NO_DWARF": 0, "UNKNOWN": 0}
    rename_typed_pool = 0
    for sym, r in functions.items():
        buckets[r["verdict"]] = buckets.get(r["verdict"], 0) + 1
    modified_classified = buckets["TRUST"] + buckets["CAUTION"] + buckets["MISLEADING"] - buckets["NO_DWARF"]
    # implicit TRUST = Bank8 map functions not present in the diff index at all
    implicit_trust = len(b8_a2s) - len(functions)

    def pct(n, d):
        return f"{100*n/max(1,d):5.1f}%"

    print()
    print("=== Bank 5 (DWARF) vs Bank 8 (target) — ghidriff instruction-level divergence ===")
    print(f"  Bank 8 map functions: {len(b8_a2s)}")
    print(f"  ghidriff: modified={counts['modified']} added={counts['added']} deleted={counts['deleted']}")
    print(f"  (modified entries with no Bank8 map symbol at new.address: {counts['modified_no_b8sym']})")
    print()
    print(f"  In-diff (changed) Bank 8 funcs classified: {len(functions)}")
    print(f"    TRUST       {buckets['TRUST']:6d}  (mnemonic-near-identical; source intent reliable)")
    print(f"    CAUTION     {buckets['CAUTION']:6d}  (some divergence; cross-check m2c/objdiff)")
    print(f"    MISLEADING  {buckets['MISLEADING']:6d}  (real rewrite; ignore Bank 5 body)")
    print(f"    NO_DWARF    {buckets['NO_DWARF']:6d}  (Bank 8 only; no Bank 5 reference)")
    print(f"  Implicit TRUST (map symbols absent from diff = identical match): <= {implicit_trust}")
    print(f"     ^ upper bound — map count includes data symbols, not just functions")
    print()
    base_match = sum(1 for r in renames if r.get("base_match"))
    port_safe = sum(1 for r in renames if r.get("port_safe"))
    print(f"  RENAMES (matched pair, DIFFERENT mangled symbol by address): {len(renames)}")
    print(f"    base-identifier match (Foo__A -> Foo__B, real signature rename): {base_match}")
    print(f"    PORT-SAFE (base match AND body >= 16 bytes): {port_safe}")
    print(f"     ^ port_safe = the set safe to inherit Bank 5 DWARF by address-pair;")
    print(f"       the rest are trivial-body byte collisions (the VT dupe-OOM hazard).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
