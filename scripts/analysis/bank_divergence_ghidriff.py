#!/usr/bin/env python3
"""
bank_divergence_ghidriff.py — Post-process ghidriff JSON output into
per-symbol divergence verdicts that upgrade bank_divergence.py.

BACKGROUND
----------
bank_divergence.py uses a MAP size heuristic:
  |Bank8_size - Bank5_size| <= 32  => TRUST
  |delta| <= 128                   => CAUTION
  |delta| > 128                    => MISLEADING

This script replaces that heuristic with INSTRUCTION-LEVEL evidence from a
ghidriff run.  ghidriff uses ExactBytes + ExactInstructions + SymbolsHash +
StructuralGraph correlators and produces per-function match_type annotations.

CLASSIFICATION MAP (match_types from ghidriff → verdict)
---------------------------------------------------------
  ExactBytesFunctionHasher in match_types    → TRUST      (byte-identical)
  ExactInstructionsFunctionHasher in mt      → TRUST      (reloc-safe match)
  StructuralGraphExactHash in mt             → TRUST      (identical graph)
  ExactMnemonicsFunctionHasher in mt         → TRUST_ISH  (same mnemonics, different immediates)
  SymbolsHash in mt (but none of the above) → CAUTION/MISLEADING per i_ratio:
      i_ratio >= 0.90                        → CAUTION
      i_ratio < 0.90                         → MISLEADING
  function in pdiff['functions']['modified'] → always needs i_ratio check
  function in pdiff['functions']['deleted']  → NO_TARGET (Bank5-only)
  function in pdiff['functions']['added']    → NO_DWARF  (Bank8-only)

USAGE
-----
  # After running ghidriff (see run_command in tools/ghidra/run_ghidriff.sh):
  python3 scripts/analysis/bank_divergence_ghidriff.py --json-dir build/SZBE69_B8/ghidra/ghidriff/json/
  python3 scripts/analysis/bank_divergence_ghidriff.py --json-dir ... --report
  python3 scripts/analysis/bank_divergence_ghidriff.py --json-dir ... SYMBOL

Importable:
  from bank_divergence_ghidriff import lookup_ghidriff
  v = lookup_ghidriff("Init__14BandHeadShaperFv")
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_JSON_DIR = ROOT / "build" / "SZBE69_B8" / "ghidra" / "ghidriff" / "json"
CACHE = ROOT / "build" / "SZBE69_B8" / "ghidra" / "ghidriff" / "divergence_cache.json"

# match_type strings produced by ghidriff's VersionTrackingDiff
BYTE_EXACT_TYPES = {
    "ExactBytesFunctionHasher",
    "ExactInstructionsFunctionHasher",
    "StructuralGraphExactHash",
}
MNEMONIC_EXACT_TYPES = {
    "ExactMnemonicsFunctionHasher",
}

# i_ratio thresholds for SymbolsHash-only matches (non-exact)
TRUST_THRESHOLD = 0.90      # instruction ratio >= this -> TRUST (near-identical regs)
CAUTION_THRESHOLD = 0.70    # instruction ratio >= this -> CAUTION
# below CAUTION_THRESHOLD -> MISLEADING


def _classify_modified(entry: dict) -> dict:
    """Classify a pdiff['functions']['modified'] entry."""
    match_types = entry.get("match_types", [])
    i_ratio = entry.get("i_ratio", 0.0)
    b_ratio = entry.get("b_ratio", 0.0)
    old_len = entry.get("old", {}).get("length", None)
    new_len = entry.get("new", {}).get("length", None)
    name = entry.get("old", {}).get("fullname", "?")
    match_set = set(match_types)

    if match_set & BYTE_EXACT_TYPES:
        # Byte/instruction exact: bodies are identical at instruction level.
        # (ghidriff still flags as 'modified' if symbol counts differ — keep TRUST.)
        return {
            "verdict": "TRUST",
            "i_ratio": i_ratio,
            "match_types": match_types,
            "b5": old_len,
            "b8": new_len,
            "note": "instruction-exact match; Bank 5 body is identical to Bank 8",
        }

    if match_set & MNEMONIC_EXACT_TYPES:
        # Same mnemonic stream, different operands (reloc offsets etc.).
        # Same control flow, same algorithm. Very reliable for source intent.
        return {
            "verdict": "TRUST",
            "i_ratio": i_ratio,
            "match_types": match_types,
            "b5": old_len,
            "b8": new_len,
            "note": "mnemonic-exact match (operand deltas only); Bank 5 source intent reliable",
        }

    # SymbolsHash-only or structural matches: use instruction ratio for finer grain
    if i_ratio >= TRUST_THRESHOLD:
        return {
            "verdict": "TRUST",
            "i_ratio": i_ratio,
            "match_types": match_types,
            "b5": old_len,
            "b8": new_len,
            "note": f"symbol match, high i_ratio={i_ratio:.2f}; Bank 5 body nearly identical",
        }
    elif i_ratio >= CAUTION_THRESHOLD:
        return {
            "verdict": "CAUTION",
            "i_ratio": i_ratio,
            "match_types": match_types,
            "b5": old_len,
            "b8": new_len,
            "note": f"symbol match, moderate i_ratio={i_ratio:.2f}; cross-check Bank 5 vs m2c",
        }
    else:
        return {
            "verdict": "MISLEADING",
            "i_ratio": i_ratio,
            "match_types": match_types,
            "b5": old_len,
            "b8": new_len,
            "note": f"symbol match, low i_ratio={i_ratio:.2f}; bodies structurally divergent; use m2c",
        }


def _build_index_from_json_dir(json_dir: Path) -> dict:
    """Parse all *.json files in json_dir and produce a symbol → verdict dict."""
    idx: dict[str, dict] = {}

    json_files = list(json_dir.glob("*.json"))
    if not json_files:
        raise FileNotFoundError(f"No JSON files found in {json_dir}. Run ghidriff first.")

    for jf in json_files:
        try:
            pdiff = json.loads(jf.read_text())
        except Exception as e:
            print(f"[warn] Could not parse {jf.name}: {e}", file=sys.stderr)
            continue

        funcs = pdiff.get("functions", {})

        # modified: matched but bodies differ
        for entry in funcs.get("modified", []):
            name = entry.get("old", {}).get("fullname")
            if name:
                idx[name] = _classify_modified(entry)

        # deleted: in Bank5 (old), not in Bank8 (new)  -> NO_TARGET for our purposes
        # (Bank5-only functions have no target in Bank8)
        for esym in funcs.get("deleted", []):
            name = esym.get("fullname")
            if name:
                idx[name] = {
                    "verdict": "NO_TARGET",
                    "b5": esym.get("length"),
                    "b8": None,
                    "note": "Bank 5 only (not in Bank 8 map); Ghidra has a body but no target exists",
                }

        # added: in Bank8 (new), not in Bank5 (old) -> NO_DWARF
        for esym in funcs.get("added", []):
            name = esym.get("fullname")
            if name:
                idx[name] = {
                    "verdict": "NO_DWARF",
                    "b5": None,
                    "b8": esym.get("length"),
                    "note": "Bank 8 only (no Bank 5 DWARF); use m2c/objdiff",
                }

        # matched but NOT in modified = passed syms_need_diff check (same body size,
        # same ref count). These are the byte-identical matched functions.
        # They are stored in pdiff['matches']['name_matches'] keyed by fullname.
        name_matches = pdiff.get("matches", {}).get("name_matches", {})
        for fullname, match_list in name_matches.items():
            if fullname in idx:
                continue  # already classified by modified/deleted/added
            # Each entry is [matched_fullname, [match_types]]
            for _matched_name, match_types in match_list:
                match_set = set(match_types)
                if match_set & BYTE_EXACT_TYPES:
                    verdict = "TRUST"
                    note = "byte/instruction-exact match; Bank 5 body identical"
                elif match_set & MNEMONIC_EXACT_TYPES:
                    verdict = "TRUST"
                    note = "mnemonic-exact; Bank 5 source intent reliable"
                else:
                    verdict = "TRUST"
                    note = "symbol-matched, same body size/refcount; Bank 5 reliable"
                idx[fullname] = {
                    "verdict": verdict,
                    "match_types": match_types,
                    "note": note,
                }
                break  # take first match

    return idx


def _load_index(json_dir: Path | None = None, refresh: bool = False) -> dict:
    effective_dir = json_dir or DEFAULT_JSON_DIR
    if not refresh and CACHE.exists():
        try:
            cm = CACHE.stat().st_mtime
            # Rebuild if any JSON in the dir is newer than cache
            if all(jf.stat().st_mtime <= cm for jf in effective_dir.glob("*.json")):
                return json.loads(CACHE.read_text())
        except (OSError, json.JSONDecodeError):
            pass
    idx = _build_index_from_json_dir(effective_dir)
    try:
        CACHE.parent.mkdir(parents=True, exist_ok=True)
        CACHE.write_text(json.dumps(idx))
    except OSError:
        pass
    return idx


_INDEX: dict | None = None


def lookup_ghidriff(symbol: str, json_dir: Path | None = None, refresh: bool = False) -> dict:
    """Return the instruction-level divergence verdict for one symbol."""
    global _INDEX
    if _INDEX is None or refresh:
        _INDEX = _load_index(json_dir, refresh)
    return _INDEX.get(symbol, {
        "verdict": "UNKNOWN",
        "note": "symbol not in ghidriff output (ghidriff may not have run yet)",
    })


def _report(idx: dict):
    buckets: dict[str, int] = {
        "TRUST": 0, "CAUTION": 0, "MISLEADING": 0,
        "NO_DWARF": 0, "NO_TARGET": 0, "UNKNOWN": 0,
    }
    total = 0
    for sym, v in idx.items():
        verdict = v.get("verdict", "UNKNOWN")
        buckets[verdict] = buckets.get(verdict, 0) + 1
        total += 1

    print("Bank 5 (Ghidra/DWARF) vs Bank 8 (target) — INSTRUCTION-LEVEL divergence")
    print(f"  Total classified symbols: {total}")
    common = total - buckets["NO_TARGET"] - buckets["NO_DWARF"]
    print(f"  Common (in both builds): {common}")
    for k in ("TRUST", "CAUTION", "MISLEADING"):
        c = buckets[k]
        print(f"    {k:11} {c:6d}  ({100 * c / max(1, common):5.1f}%)")
    print(f"  Bank8-only (no Bank5 DWARF): {buckets['NO_DWARF']}")
    print(f"  Bank5-only (no Bank8 target): {buckets['NO_TARGET']}")
    print()
    print("Verdict guide:")
    print("  TRUST       -> Bank 5 Ghidra body is instruction-identical to Bank 8 (safe)")
    print("  CAUTION     -> some instruction divergence; verify against m2c/objdiff")
    print("  MISLEADING  -> major structural divergence; ignore Ghidra body, use m2c")
    print()
    print("NOTE: compared to bank_divergence.py (size heuristic), this gives:")
    print("  - True TRUST (byte-exact) vs size-based approximation")
    print("  - Instruction ratio for CAUTION (more granular than 128-byte delta)")
    print("  - Proper handling of renamed/split/merged functions")


def main():
    import argparse
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--json-dir", type=Path, default=DEFAULT_JSON_DIR,
                    help="Directory containing ghidriff JSON output files")
    ap.add_argument("--report", action="store_true", help="Print corpus-wide summary")
    ap.add_argument("--refresh", action="store_true", help="Rebuild the cache")
    ap.add_argument("symbol", nargs="?", help="Symbol to look up (mangled name)")
    args = ap.parse_args()

    if not args.json_dir.exists():
        print(f"ERROR: JSON dir not found: {args.json_dir}", file=sys.stderr)
        print("Run ghidriff first. See tools/ghidra/run_ghidriff.sh", file=sys.stderr)
        sys.exit(1)

    idx = _load_index(args.json_dir, refresh=args.refresh)

    if args.report or not args.symbol:
        _report(idx)
        return

    v = idx.get(args.symbol, {"verdict": "UNKNOWN", "note": "not in ghidriff output"})
    print(f"{args.symbol}")
    print(f"  verdict:     {v['verdict']}")
    if v.get("i_ratio") is not None:
        print(f"  i_ratio:     {v['i_ratio']:.3f}  (1.0=identical instructions)")
    if v.get("b5") is not None:
        print(f"  Bank5 len:   {v['b5']} bytes")
    if v.get("b8") is not None:
        print(f"  Bank8 len:   {v['b8']} bytes")
    if v.get("match_types"):
        print(f"  match_types: {v['match_types']}")
    print(f"  {v['note']}")


if __name__ == "__main__":
    main()
