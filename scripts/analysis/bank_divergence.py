#!/usr/bin/env python3
"""
bank_divergence.py — Is the Bank 5 DWARF (what Ghidra uses) trustworthy for a
given symbol, or is its body from a different era than the Bank 8 target?

WHY THIS EXISTS
---------------
The decomp target is the **Bank 8** prototype DOL (orig/SZBE69_B8/sys/main.dol,
stamped ~early-2010). Bank 8 ships NO DWARF ELF — only a DOL + CodeWarrior map.
The only banks that ship a DWARF .elf are **Bank 5** (~mid-2009) and Bank 6
(~2008). So Ghidra (tools/ghidra/pyghidra-service.sh) decompiles the Bank 5 ELF
for symbol-rich, DWARF-typed output.

Problem: Bank 5 is ~9-12 months older than Bank 8. Across the ~21.8k functions
common to both maps, **only 54% have an identical body size**; ~20% differ by
>128 bytes (a real rewrite — features added/removed). For those, Ghidra's
control-flow / pseudo-C is from a DIFFERENT function than the one we're matching
(the BandHeadShaper::Init trap: 840 bytes in Bank 5 vs 3084 in Bank 8).

The m2c leg (bin/decompile) reads build/SZBE69_B8/asm — the dtk split of the
**Bank 8** DOL — so it is always target-accurate. objdiff likewise diffs Bank 8.
So when Bank 5 diverges: trust m2c + objdiff, distrust Ghidra source intent.

This tool quantifies that per-symbol so the workflow can warn automatically.

USAGE
-----
  scripts/analysis/bank_divergence.py SYMBOL          # verdict for one symbol
  scripts/analysis/bank_divergence.py --report        # corpus-wide summary
  scripts/analysis/bank_divergence.py --refresh        # rebuild the cache
  scripts/analysis/bank_divergence.py --json SYMBOL    # machine-readable

Importable: `from bank_divergence import lookup; v = lookup("Init__14...Fv")`
"""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LIB = ROOT.parent / "milo-executable-library" / "rb3"
B8_MAP = ROOT / "orig" / "SZBE69_B8" / "files" / "band_r_wii.map"
B5_MAP = LIB / "Wii Proto (Bank 5) (Debug)" / "band_r_wii.map"
CACHE = ROOT / "build" / "SZBE69_B8" / "bank_divergence.json"

# CW map symbol line: "  OFF SIZE ADDR FILEOFF ALIGN SYMBOL \t lib path"
_LINE = re.compile(
    r"^  ([0-9a-fA-F]{8}) ([0-9a-fA-F]{6}) ([0-9a-fA-F]{8}) ([0-9a-fA-F]{8})\s+(\d+) (\S.*?) \t(.*)$"
)

# Verdict thresholds on |Bank8 - Bank5| body-size delta (bytes).
TRUST_MAX = 32      # <= this: same source, trivial codegen drift -> trust Bank 5
CAUTION_MAX = 128   # <= this: moderate change -> verify against m2c/objdiff


def _parse_map(path: Path) -> dict:
    """symbol -> max body size seen (across object files of that name)."""
    sizes: dict[str, int] = {}
    with open(path, "r", errors="replace") as f:
        for line in f:
            m = _LINE.match(line)
            if not m:
                continue
            size, sym = int(m.group(2), 16), m.group(6)
            if sym.startswith("."):  # section header line
                continue
            # Keep the largest instance (conservative: over-warns on collisions).
            if size > sizes.get(sym, -1):
                sizes[sym] = size
    return sizes


def _verdict(b5: int | None, b8: int | None) -> dict:
    if b8 is None:
        return {"verdict": "NO_TARGET", "note": "symbol not in Bank 8 map (not a target here)"}
    if b5 is None:
        return {
            "verdict": "NO_DWARF",
            "note": "absent from Bank 5 -> Ghidra has no DWARF body; use m2c/objdiff",
        }
    delta = b8 - b5
    ad = abs(delta)
    if ad == 0:
        v, note = "TRUST", "identical body size; Bank 5 DWARF matches the target"
    elif ad <= TRUST_MAX:
        v, note = "TRUST", "minor drift; Bank 5 source intent is reliable"
    elif ad <= CAUTION_MAX:
        v, note = "CAUTION", "moderate change; cross-check Ghidra against m2c/objdiff"
    else:
        v, note = "MISLEADING", "major rewrite between eras; IGNORE Ghidra body, use m2c (Bank 8)"
    return {"verdict": v, "b5": b5, "b8": b8, "delta": delta, "note": note}


def _build_index() -> dict:
    b5, b8 = _parse_map(B5_MAP), _parse_map(B8_MAP)
    idx = {}
    for sym in set(b5) | set(b8):
        idx[sym] = _verdict(b5.get(sym), b8.get(sym))
    return idx


def _load_index(refresh: bool = False) -> dict:
    if not refresh and CACHE.exists():
        # Rebuild if either map is newer than the cache.
        try:
            cm = CACHE.stat().st_mtime
            if all(p.exists() and p.stat().st_mtime <= cm for p in (B5_MAP, B8_MAP)):
                return json.loads(CACHE.read_text())
        except (OSError, json.JSONDecodeError):
            pass
    idx = _build_index()
    try:
        CACHE.parent.mkdir(parents=True, exist_ok=True)
        CACHE.write_text(json.dumps(idx))
    except OSError:
        pass
    return idx


_INDEX = None


def lookup(symbol: str, refresh: bool = False) -> dict:
    """Return the divergence verdict dict for one symbol. Safe to import."""
    global _INDEX
    if _INDEX is None or refresh:
        _INDEX = _load_index(refresh)
    return _INDEX.get(symbol, {"verdict": "UNKNOWN", "note": "symbol in neither map"})


def warning_banner(symbol: str) -> str | None:
    """A one-paragraph warning for CAUTION/MISLEADING/NO_DWARF, else None.

    Wired into tools/analyze_function.py to gate the Ghidra section.
    """
    v = lookup(symbol)
    verdict = v["verdict"]
    if verdict in ("TRUST", "NO_TARGET", "UNKNOWN"):
        return None
    if verdict == "NO_DWARF":
        return f"[bank-divergence] {symbol}: {v['note']}."
    return (
        f"[bank-divergence: {verdict}] Bank 5 (Ghidra/DWARF) body is "
        f"{v['b5']}B vs Bank 8 (target) {v['b8']}B ({v['delta']:+d}). {v['note']}."
    )


def _report():
    idx = _load_index()
    buckets = {"TRUST": 0, "CAUTION": 0, "MISLEADING": 0, "NO_DWARF": 0, "NO_TARGET": 0}
    # Restrict the corpus summary to function-like symbols common to both maps.
    common_funcs = 0
    for sym, v in idx.items():
        if v["verdict"] == "NO_TARGET":
            continue
        is_func = "__" in sym and "F" in sym and not sym.startswith(("@", "__vt"))
        if not is_func:
            continue
        if v["verdict"] == "NO_DWARF":
            buckets["NO_DWARF"] += 1
            continue
        common_funcs += 1
        buckets[v["verdict"]] += 1
    print("Bank 5 (Ghidra/DWARF, ~mid-2009) vs Bank 8 (target, ~2010) divergence")
    print(f"  function symbols common to both maps: {common_funcs}")
    for k in ("TRUST", "CAUTION", "MISLEADING"):
        c = buckets[k]
        print(f"    {k:11} {c:6d}  ({100*c/max(1,common_funcs):4.1f}%)")
    print(f"  Bank8-only funcs (no Bank 5 DWARF at all): {buckets['NO_DWARF']}")
    print("\nVerdict guide:")
    print("  TRUST       -> Ghidra source intent is reliable")
    print("  CAUTION     -> cross-check Ghidra against m2c/objdiff (Bank 8)")
    print("  MISLEADING  -> ignore Ghidra body; use m2c (bin/decompile reads Bank 8 asm)")


def main():
    args = sys.argv[1:]
    if "--refresh" in args:
        _load_index(refresh=True)
        args.remove("--refresh")
        if not args:
            print(f"cache rebuilt -> {CACHE}")
            return
    if not args or "--report" in args:
        _report()
        return
    as_json = "--json" in args
    if as_json:
        args.remove("--json")
    sym = args[0]
    v = lookup(sym)
    if as_json:
        print(json.dumps({"symbol": sym, **v}))
        return
    print(f"{sym}")
    print(f"  verdict: {v['verdict']}")
    if "b5" in v:
        print(f"  Bank 5 (Ghidra): {v['b5']}B   Bank 8 (target): {v['b8']}B   delta: {v['delta']:+d}")
    print(f"  {v['note']}")


if __name__ == "__main__":
    main()
