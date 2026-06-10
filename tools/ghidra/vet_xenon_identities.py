#!/usr/bin/env python3
"""Vetting protocol for Wii<->Xenon ghidriff matches.

Reads a ghidriff matches.json, the Wii CW map (for TU + size + category
attribution), the Bank 5 ELF (for rb3wii cross-check name bridging), and
unified_id_rb3wii.json (for cross-checks) and produces vetted_identities.json:
a tiered, annotated export of every match.

Tier definitions (CLI-overridable via --tier-config):
  ACCEPT        — high-precision match types: SeedMatch, ExactInstructions-
                  FunctionHasher, SymbolsHash, Implied Match, SwitchSigHasher
  FILTERED_VT   — VTCombinedReference pairs that pass the TU-cluster coherence
                  test: group by Wii TU; for groups with >= 2 members:
                  xenon_spread / max(wii_spread, 1) < 10 AND xenon_spread < 50000
  CAUTION       — VTCombinedReference singletons or scattered clusters (fail
                  coherence); also ExactMnemonics unless in accept_types
  REJECT        — StringsRefsHasher, StrUniqueFuncRefsHasher (pending T1
                  uniqueness gate), and any future unmeasured type unless
                  allowlisted via --accept-types

rb3wii cross-check (name-level join via Bank 5 ELF):
  confirmed     — Xenon addr in unified_id_rb3wii.json AND Bank5-ELF-mangled-
                  name at rb3wii's wii_addr == Bank8-map-symbol at ghidriff's p1
  contradicted  — Xenon addr in unified_id_rb3wii.json AND names disagree
  absent        — Xenon addr not in unified_id_rb3wii.json (or Bank 5 ELF
                  lookup fails)

Usage:
  python3 tools/ghidra/vet_xenon_identities.py
  python3 tools/ghidra/vet_xenon_identities.py --run-dir build/SZBE69_B8/ghidra/ghidriff-xenon
  python3 tools/ghidra/vet_xenon_identities.py --matches path/to/x.matches.json
  python3 tools/ghidra/vet_xenon_identities.py --min-vt-score 11.0
  python3 tools/ghidra/vet_xenon_identities.py --accept-types BSim,StringsRefsHasher
  python3 tools/ghidra/vet_xenon_identities.py --selftest

Selftest:
  Runs synthetic fixture covering each tier rule without any external files.
"""
from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from collections import Counter, defaultdict
from pathlib import Path
from typing import Dict, List, Optional, Tuple

TOOLS_DIR = Path(__file__).resolve().parent
RB3 = TOOLS_DIR.parents[1]
XENON = RB3.parent / "rb3-xenon"
MILO_LIB = RB3.parent / "milo-executable-library"

DEFAULT_RUN_DIR = RB3 / "build" / "SZBE69_B8" / "ghidra" / "ghidriff-xenon"
DEFAULT_WII_MAP = RB3 / "orig" / "SZBE69_B8" / "files" / "band_r_wii.map"
DEFAULT_RB3WII = XENON / "unified_id_rb3wii.json"
DEFAULT_BANK5_ELF = (MILO_LIB / "rb3" / "Wii Proto (Bank 5) (Debug)"
                     / "band_r_wii.elf")
DEFAULT_OUT = DEFAULT_RUN_DIR / "vetted_identities.json"

# ---------------------------------------------------------------------------
# Tier config (can be overridden via --tier-config JSON file or CLI args)
# ---------------------------------------------------------------------------
DEFAULT_TIER_CONFIG = {
    # match_types in this set always get tier ACCEPT
    # NOTE: ExactMnemonicsFunctionHasher is intentionally excluded — it is
    # unmeasured cross-compiler and the brief's rule is "REJECT unless
    # allowlisted via --accept-types". Use --accept-types ExactMnemonicsFunctionHasher
    # to promote it once precision is established.
    "accept_types": [
        "SeedMatch",
        "ExactInstructionsFunctionHasher",
        "SymbolsHash",
        "Implied Match",
        "SwitchSigHasher",
    ],
    # match_types in this set get tier REJECT (regardless of cluster coherence)
    "reject_types": [
        "StringsRefsHasher",
        "StrUniqueFuncRefsHasher",
    ],
    # VTCombinedReference cluster coherence thresholds
    "vt_max_xenon_spread": 50000,
    "vt_max_spread_ratio": 10.0,
    # optional: minimum VT score (from T2's score export); 0 = disabled
    "min_vt_score": 0.0,
}

# ---------------------------------------------------------------------------
# Address / map parsing (copied from eval_xenon_matches.py to avoid coupling)
# ---------------------------------------------------------------------------
_MAP_LINE_TU = re.compile(
    r"^\s+([0-9a-f]{8})\s+([0-9a-f]{6})\s+([0-9a-f]{8})\s+[0-9a-f]{8}\s+\d+\s+(\S+)"
    r"(?:\s*\t(.+))?$"
)


def parse_addr(s) -> Optional[int]:
    """Ghidra/json address -> int. Accepts '0x82260018', '82260018',
    'ram:82260018', ints. Returns None on garbage."""
    if isinstance(s, int):
        return s
    if not isinstance(s, str):
        return None
    s = s.strip().lower()
    if ":" in s:
        s = s.rsplit(":", 1)[1]
    if s.startswith("0x"):
        s = s[2:]
    try:
        return int(s, 16)
    except ValueError:
        return None


def categorize_tu(tu_field: str) -> Tuple[Optional[str], Optional[str]]:
    """Map TU column -> (tu_basename, category).

    Categories:
      'band3' / 'system' / 'network' — full CW source paths
      'main'  — bare single-token objects (App.o, Main.o, ...)
      'sdk'   — archive.a obj.o entries (RVL SDK / MSL / DWC / Bink ...)
    """
    if not tu_field:
        return None, None
    t = tu_field.strip()
    if not t:
        return None, None
    low = t.replace("/", "\\")
    if "band3_wii\\" in low:
        rest = low.split("band3_wii\\", 1)[1]
        basename = low.rsplit("\\", 1)[-1].strip()
        return basename, "band3"
    parts = t.split()
    if len(parts) == 1:
        return parts[0], "main"
    if parts[-1].endswith(".o"):
        return parts[-1], "sdk"
    return None, None


def parse_wii_map_index(map_path: Path) -> Dict[int, dict]:
    """CodeWarrior map -> {vaddr: {symbol, tu, category, size}} for .text/.init
    function symbols (size > 0). First symbol at an address wins.

    Uses _categorize_full (mirrors eval_xenon_matches.py's categorize_tu) so
    category values are: 'band3', 'system', 'network', 'main', 'sdk', or None.
    """
    out: Dict[int, dict] = {}
    in_code = False
    with map_path.open("r", encoding="utf-8", errors="replace") as f:
        for line in f:
            stripped = line.rstrip("\n")
            if stripped.endswith("section layout"):
                in_code = stripped.startswith((".text", ".init"))
                continue
            if not in_code:
                continue
            m = _MAP_LINE_TU.match(stripped)
            if not m:
                continue
            size = int(m.group(2), 16)
            if size == 0:
                continue
            sym = m.group(4)
            if sym[0] in "@*.":
                continue
            addr = int(m.group(3), 16)
            if addr in out:
                continue
            tu_raw = m.group(5) or ""
            tu, cat = _categorize_full(tu_raw)
            out[addr] = {"symbol": sym, "tu": tu, "tu_raw": tu_raw,
                         "category": cat, "size": size}
    return out


def _categorize_full(tu_field: str) -> Tuple[Optional[str], Optional[str]]:
    """Map TU column -> (tu_basename, category).

    Faithfully mirrors eval_xenon_matches.py's categorize_tu: extracts the
    <module> path component that follows 'band3_wii\\' in the full CW source
    path (e.g. 'band3_wii\\system\\src\\...' -> category='system',
    'band3_wii\\band3\\src\\...' -> category='band3',
    'band3_wii\\network\\src\\...' -> category='network').

    Categories:
      'band3' / 'system' / 'network' / 'sdk' — from the map path
      'main'  — bare single-token objects (App.o, Main.o, ...)
    """
    if not tu_field:
        return None, None
    t = tu_field.strip()
    if not t:
        return None, None
    low = t.replace("/", "\\")
    if "band3_wii\\" in low:
        rest = low.split("band3_wii\\", 1)[1]
        module = rest.split("\\", 1)[0]
        basename = low.rsplit("\\", 1)[-1].strip()
        return basename, module
    parts = t.split()
    if len(parts) == 1:
        return parts[0], "main"
    if parts[-1].endswith(".o"):
        return parts[-1], "sdk"
    return None, None


# ---------------------------------------------------------------------------
# Bank 5 ELF symbol table (for rb3wii cross-check name bridging)
# ---------------------------------------------------------------------------
def load_bank5_syms(elf_path: Path) -> Dict[int, str]:
    """Run `nm` on the Bank 5 ELF and return {addr: mangled_symbol} for
    text-section (T) symbols only.  Returns {} if the file is absent or nm
    fails (caller degrades rb3wii cross-check to 'absent' for all entries).
    """
    if not elf_path.exists():
        return {}
    try:
        result = subprocess.run(
            ["nm", str(elf_path)],
            capture_output=True, text=True, timeout=30,
        )
    except Exception:
        return {}
    out: Dict[int, str] = {}
    for line in result.stdout.splitlines():
        parts = line.split(None, 2)
        if len(parts) >= 3 and parts[1] == "T":
            try:
                addr = int(parts[0], 16)
                out[addr] = parts[2].strip()
            except ValueError:
                pass
    return out


# ---------------------------------------------------------------------------
# rb3wii cross-check index
# ---------------------------------------------------------------------------
def load_rb3wii_index(path: Path) -> Dict[int, dict]:
    """unified_id_rb3wii.json -> {xenon_addr: entry}.
    Duplicate Xenon addresses: last entry wins (rare; the file is already deduped).
    """
    out: Dict[int, dict] = {}
    with path.open("r", encoding="utf-8") as f:
        entries = json.load(f)
    for e in entries:
        a = parse_addr(e.get("rb3_addr"))
        if a is not None:
            out[a] = e
    return out


# ---------------------------------------------------------------------------
# TU-cluster coherence
# ---------------------------------------------------------------------------
def compute_vt_tiers(
    vt_entries: List[dict],
    max_xenon_spread: float,
    max_spread_ratio: float,
) -> Dict[int, str]:
    """Assign FILTERED_VT or CAUTION to each VT entry by TU-cluster coherence.

    vt_entries: list of dicts with keys p2 (int xenon addr), p1 (int wii addr),
                wii_tu (str or None).

    Returns {p2: tier_string} for every input entry.
    """
    # Group by wii_tu (None goes to its own singleton group)
    tu_groups: Dict[Optional[str], List[dict]] = defaultdict(list)
    for e in vt_entries:
        tu_groups[e["wii_tu"]].append(e)

    result: Dict[int, str] = {}
    for tu, entries in tu_groups.items():
        if len(entries) < 2:
            # Singleton -> CAUTION
            for e in entries:
                result[e["p2"]] = "CAUTION"
            continue
        xenon_addrs = [e["p2"] for e in entries]
        wii_addrs = [e["p1"] for e in entries]
        xenon_spread = max(xenon_addrs) - min(xenon_addrs)
        wii_spread = max(wii_addrs) - min(wii_addrs)
        ratio = xenon_spread / max(wii_spread, 1)
        if ratio < max_spread_ratio and xenon_spread < max_xenon_spread:
            for e in entries:
                result[e["p2"]] = "FILTERED_VT"
        else:
            for e in entries:
                result[e["p2"]] = "CAUTION"
    return result


# ---------------------------------------------------------------------------
# Core vetting logic
# ---------------------------------------------------------------------------
def vet(
    function_matches: List[dict],
    wii_index: Dict[int, dict],
    rb3wii_index: Dict[int, dict],
    tier_config: dict,
    bank5_syms: Optional[Dict[int, str]] = None,
) -> Tuple[List[dict], dict]:
    """Apply the tier protocol to all matches.

    Returns (entries, summary) where entries is the per-match list and
    summary contains per-tier and per-category counts.

    bank5_syms: {addr: mangled_symbol} from `nm` on the Bank 5 ELF.  Used to
    bridge the rb3wii address-space gap: rb3wii.wii_addr is a Bank 5 address,
    but ghidriff's p1 is a Bank 8 address.  We look up the Bank 5 symbol at
    rb3wii.wii_addr and compare it to the Bank 8 map symbol at p1.  If
    bank5_syms is None or empty, the cross-check degrades to 'absent' for
    entries whose Bank 5 lookup would be needed.
    """
    if bank5_syms is None:
        bank5_syms = {}
    accept_types = set(tier_config.get("accept_types", DEFAULT_TIER_CONFIG["accept_types"]))
    reject_types = set(tier_config.get("reject_types", DEFAULT_TIER_CONFIG["reject_types"]))
    vt_max_xenon = tier_config.get("vt_max_xenon_spread",
                                   DEFAULT_TIER_CONFIG["vt_max_xenon_spread"])
    vt_max_ratio = tier_config.get("vt_max_spread_ratio",
                                   DEFAULT_TIER_CONFIG["vt_max_spread_ratio"])
    min_vt_score = tier_config.get("min_vt_score", 0.0)

    # First pass: build VT entry list for cluster analysis (skip seeds)
    vt_candidates = []
    for m in function_matches:
        mtypes = set(m.get("match_types") or [])
        if "SeedMatch" in mtypes:
            continue
        if "VTCombinedReference" not in mtypes:
            continue
        # Score gate (if T2 scores are present)
        if min_vt_score > 0:
            scores = m.get("scores", {})
            vt_score = scores.get("VTCombinedReference", {}).get("product", None)
            if vt_score is not None and vt_score < min_vt_score:
                continue
        p1 = parse_addr(m.get("p1_addr"))
        p2 = parse_addr(m.get("p2_addr"))
        if p1 is None or p2 is None:
            continue
        wii = wii_index.get(p1)
        vt_candidates.append({
            "p1": p1,
            "p2": p2,
            "wii_tu": wii["tu"] if wii else None,
        })

    vt_tier_map = compute_vt_tiers(vt_candidates, vt_max_xenon, vt_max_ratio)

    # Second pass: build full output
    entries = []
    n_bad_addr = 0

    for m in function_matches:
        p1 = parse_addr(m.get("p1_addr"))
        p2 = parse_addr(m.get("p2_addr"))
        if p1 is None or p2 is None:
            n_bad_addr += 1
            continue

        mtypes = list(m.get("match_types") or [])
        mtype_set = set(mtypes)
        wii = wii_index.get(p1)

        # Determine tier
        tier = _assign_tier(
            mtype_set, p2, vt_tier_map,
            accept_types, reject_types,
            min_vt_score, m.get("scores"),
        )

        # rb3wii cross-check (name-level via Bank 5 ELF)
        rb3wii_entry = rb3wii_index.get(p2)
        bank8_sym = wii["symbol"] if wii else None
        rb3wii_check, rb3wii_wii_addr = _rb3wii_check(
            p1, bank8_sym, rb3wii_entry, bank5_syms
        )

        entry: dict = {
            "xenon_addr": hex(p2),
            # Always emit wii_addr: use map lookup if available, else raw p1_addr
            "wii_addr": hex(p1),
            "wii_symbol": bank8_sym,
            "tier": tier,
            "match_types": mtypes,
            "tu": wii["tu"] if wii else None,
            "category": wii["category"] if wii else None,
            "rb3wii_check": rb3wii_check,
        }
        if rb3wii_entry is not None:
            entry["rb3wii_wii_addr"] = rb3wii_wii_addr
            entry["rb3wii_wii_name"] = rb3wii_entry.get("wii_name")
            entry["rb3wii_similarity"] = rb3wii_entry.get("similarity")
            entry["rb3wii_confidence"] = rb3wii_entry.get("confidence")

        entries.append(entry)

    # Build summary
    tier_counts: Counter = Counter()
    cat_counts: Dict[str, Counter] = defaultdict(Counter)
    rb3wii_counts: Counter = Counter()
    rb3wii_contradicted_examples = []

    for e in entries:
        t = e["tier"]
        c = e["category"] or "unresolved"
        tier_counts[t] += 1
        cat_counts[c][t] += 1
        rb3wii_counts[e["rb3wii_check"]] += 1
        if e["rb3wii_check"] == "contradicted":
            rb3wii_contradicted_examples.append({
                "xenon_addr": e["xenon_addr"],
                "wii_addr": e["wii_addr"],
                "wii_symbol": e["wii_symbol"],
                "tier": t,
                "rb3wii_wii_addr": e.get("rb3wii_wii_addr"),
                "rb3wii_wii_name": e.get("rb3wii_wii_name"),
            })

    summary = {
        "total_entries": len(entries),
        "bad_addr_skipped": n_bad_addr,
        "tier_counts": dict(tier_counts),
        "per_category": {c: dict(v) for c, v in sorted(cat_counts.items())},
        "rb3wii_check_counts": dict(rb3wii_counts),
        "rb3wii_contradicted_examples": rb3wii_contradicted_examples[:20],
        "tier_config_used": {
            "accept_types": sorted(accept_types),
            "reject_types": sorted(reject_types),
            "vt_max_xenon_spread": vt_max_xenon,
            "vt_max_spread_ratio": vt_max_ratio,
            "min_vt_score": min_vt_score,
        },
    }
    return entries, summary


def _assign_tier(
    mtype_set: set,
    p2: int,
    vt_tier_map: Dict[int, str],
    accept_types: set,
    reject_types: set,
    min_vt_score: float,
    scores,
) -> str:
    # ACCEPT: contains any accept-type
    if mtype_set & accept_types:
        return "ACCEPT"

    # REJECT: contains any reject-type (and no accept-type)
    if mtype_set & reject_types:
        return "REJECT"

    # VT: use cluster coherence result
    if "VTCombinedReference" in mtype_set:
        # Score gate applied during candidate collection; if score field absent
        # (pre-T2 artifact), proceed without gating
        if min_vt_score > 0 and scores:
            vt_score = scores.get("VTCombinedReference", {}).get("product", None)
            if vt_score is not None and vt_score < min_vt_score:
                return "REJECT"
        return vt_tier_map.get(p2, "CAUTION")

    # Anything else not in the known tiers: CAUTION
    return "CAUTION"


def _rb3wii_check(
    p1: int,
    bank8_sym: Optional[str],
    rb3wii_entry,
    bank5_syms: Dict[int, str],
) -> Tuple[str, Optional[str]]:
    """Return (check_status, rb3wii_wii_addr_hex or None).

    Name-level join via Bank 5 ELF:
      1. Look up rb3wii.wii_addr in bank5_syms to get the Bank 5 mangled name.
      2. Compare with the Bank 8 map symbol at p1 (bank8_sym).
      3. confirmed iff names agree; contradicted iff names disagree; absent if
         the rb3wii entry is missing, the Bank 5 ELF lacks the address, or
         the Bank 8 map lacks the Bank 8 symbol.
    """
    if rb3wii_entry is None:
        return "absent", None
    rb_wii = parse_addr(rb3wii_entry.get("wii_addr"))
    rb_hex = hex(rb_wii) if rb_wii is not None else None
    if rb_wii is None:
        return "absent", rb_hex
    # Resolve Bank 5 mangled name at the rb3wii wii_addr
    bank5_sym = bank5_syms.get(rb_wii)
    if bank5_sym is None:
        # Bank 5 ELF doesn't cover this address (or ELF not loaded); degrade
        return "absent", rb_hex
    # Resolve Bank 8 map symbol at p1
    if bank8_sym is None:
        return "absent", rb_hex
    if bank5_sym == bank8_sym:
        return "confirmed", rb_hex
    return "contradicted", rb_hex


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------
def find_matches_json(run_dir: Path) -> Optional[Path]:
    """Find the single .matches.json in run_dir/json/."""
    json_dir = run_dir / "json"
    if not json_dir.exists():
        return None
    candidates = list(json_dir.glob("*.matches.json"))
    if len(candidates) == 1:
        return candidates[0]
    if len(candidates) > 1:
        # Prefer the most-recently-modified
        return sorted(candidates, key=lambda p: p.stat().st_mtime)[-1]
    return None


def main(argv=None):
    p = argparse.ArgumentParser(
        description="Vet Wii<->Xenon ghidriff identity matches (tier + rb3wii cross-check)."
    )
    p.add_argument(
        "--run-dir", type=Path, default=DEFAULT_RUN_DIR,
        help="ghidriff run directory (contains json/*.matches.json). "
             "Used to locate matches.json when --matches is absent.",
    )
    p.add_argument(
        "--matches", type=Path, default=None,
        help="Explicit path to *.matches.json. Overrides --run-dir discovery.",
    )
    p.add_argument(
        "--wii-map", type=Path, default=DEFAULT_WII_MAP,
        help="Wii CodeWarrior linker map (band_r_wii.map).",
    )
    p.add_argument(
        "--rb3wii", type=Path, default=DEFAULT_RB3WII,
        help="rb3-xenon/unified_id_rb3wii.json for cross-check.",
    )
    p.add_argument(
        "--out", type=Path, default=DEFAULT_OUT,
        help="Output path for vetted_identities.json.",
    )
    p.add_argument(
        "--tier-config", type=Path, default=None,
        help="Optional JSON file overriding tier thresholds and type lists. "
             "Keys: accept_types, reject_types, vt_max_xenon_spread, "
             "vt_max_spread_ratio, min_vt_score.",
    )
    p.add_argument(
        "--accept-types", default=None,
        help="Comma-separated extra match types to force into ACCEPT tier "
             "(e.g. 'BSim,StringsRefsHasher'). Added to default accept_types.",
    )
    p.add_argument(
        "--min-vt-score", type=float, default=None,
        help="Minimum VT product score (from T2's score export). "
             "0 = disabled (default). Requires scores field in matches.json.",
    )
    p.add_argument(
        "--bank5-elf", type=Path, default=DEFAULT_BANK5_ELF,
        help="Path to the Bank 5 Wii debug ELF (band_r_wii.elf). Used to "
             "bridge rb3wii wii_addr (Bank 5 addresses) to mangled names for "
             "the name-level rb3wii cross-check.",
    )
    p.add_argument(
        "--selftest", action="store_true",
        help="Run synthetic fixtures and exit. Does not read any real files.",
    )
    args = p.parse_args(argv)

    if args.selftest:
        run_selftests()
        return

    # --- load tier config ---
    tier_config = dict(DEFAULT_TIER_CONFIG)
    if args.tier_config:
        with args.tier_config.open() as f:
            tier_config.update(json.load(f))
    if args.accept_types:
        extra = [t.strip() for t in args.accept_types.split(",") if t.strip()]
        tier_config["accept_types"] = list(
            set(tier_config.get("accept_types", [])) | set(extra)
        )
    if args.min_vt_score is not None:
        tier_config["min_vt_score"] = args.min_vt_score

    # --- locate matches.json ---
    matches_path = args.matches
    if matches_path is None:
        matches_path = find_matches_json(args.run_dir)
        if matches_path is None:
            print(
                f"ERROR: no *.matches.json found under {args.run_dir / 'json'}. "
                "Pass --matches explicitly.", file=sys.stderr
            )
            sys.exit(1)

    print(f"Reading matches: {matches_path}", file=sys.stderr)
    with matches_path.open("r", encoding="utf-8") as f:
        d = json.load(f)
    function_matches = d.get("function_matches") or d  # handle bare list

    # --- load Wii map ---
    print(f"Reading Wii map: {args.wii_map}", file=sys.stderr)
    wii_index = parse_wii_map_index(args.wii_map)
    print(f"  {len(wii_index):,} Wii symbols indexed.", file=sys.stderr)

    # --- load rb3wii ---
    print(f"Reading rb3wii: {args.rb3wii}", file=sys.stderr)
    rb3wii_index = load_rb3wii_index(args.rb3wii)
    print(f"  {len(rb3wii_index):,} rb3wii entries indexed.", file=sys.stderr)

    # --- load Bank 5 ELF for rb3wii name-bridging ---
    print(f"Reading Bank 5 ELF: {args.bank5_elf}", file=sys.stderr)
    bank5_syms = load_bank5_syms(args.bank5_elf)
    if bank5_syms:
        print(f"  {len(bank5_syms):,} Bank 5 text symbols loaded.", file=sys.stderr)
    else:
        print("  WARNING: Bank 5 ELF not found or nm failed; "
              "rb3wii cross-check will report 'absent' for all entries.",
              file=sys.stderr)

    # --- vet ---
    entries, summary = vet(
        function_matches, wii_index, rb3wii_index, tier_config, bank5_syms
    )

    # --- output ---
    out = {
        "summary": summary,
        "entries": entries,
    }
    args.out.parent.mkdir(parents=True, exist_ok=True)
    with args.out.open("w", encoding="utf-8") as f:
        json.dump(out, f, indent=2)
    print(f"Written: {args.out}", file=sys.stderr)

    # Print summary table
    print("\n=== TIER SUMMARY ===")
    for tier in ("ACCEPT", "FILTERED_VT", "CAUTION", "REJECT"):
        n = summary["tier_counts"].get(tier, 0)
        print(f"  {tier:<14} {n:>5}")
    print()
    print("=== PER CATEGORY ===")
    for cat in ("band3", "system", "network", "main", "sdk", "unresolved"):
        ctiers = summary["per_category"].get(cat, {})
        if ctiers:
            print(f"  {cat}:")
            for tier in ("ACCEPT", "FILTERED_VT", "CAUTION", "REJECT"):
                n = ctiers.get(tier, 0)
                if n:
                    print(f"    {tier:<14} {n:>5}")
    print()
    print("=== rb3wii CROSS-CHECK ===")
    for k in ("confirmed", "contradicted", "absent"):
        n = summary["rb3wii_check_counts"].get(k, 0)
        print(f"  {k:<16} {n:>5}")
    if summary["rb3wii_contradicted_examples"]:
        print("\n  First 5 contradicted examples:")
        for ex in summary["rb3wii_contradicted_examples"][:5]:
            print(f"    xenon={ex['xenon_addr']} ghidriff_wii={ex['wii_addr']} "
                  f"({ex['wii_symbol']}) vs rb3wii={ex.get('rb3wii_wii_addr')} "
                  f"({ex.get('rb3wii_wii_name')})")


# ---------------------------------------------------------------------------
# Self-test
# ---------------------------------------------------------------------------
def run_selftests():
    """Synthetic fixture tests — each tier rule exercised without real files."""
    errors = []

    # Minimal synthetic wii_index
    wii_index = {
        0x80100000: {"symbol": "Foo__3BarFv",  "tu": "Bar.o",  "category": "band3", "size": 80},
        0x80100080: {"symbol": "Baz__3BarFv",  "tu": "Bar.o",  "category": "band3", "size": 80},
        0x80100300: {"symbol": "Qux__3BarFv",  "tu": "Bar.o",  "category": "band3", "size": 80},
        0x80200000: {"symbol": "Sys__5EngineFv","tu": "Engine.o","category": "system","size": 100},
        0x80300000: {"symbol": "Stub__3NopFv",  "tu": "Nop.o",  "category": "band3", "size": 60},
        0x80400000: {"symbol": "AccAcc__6AccMgrFv","tu":"AccMgr.o","category":"band3","size":80},
        0x80400200: {"symbol": "AccB__6AccMgrFv",  "tu":"AccMgr.o","category":"band3","size":80},
    }
    # rb3wii index: 0x82100010 confirms, 0x82200020 contradicts.
    # Bank 5 wii_addrs are different from Bank 8 wii_addrs (different builds).
    # We use synthetic Bank 5 addrs: 0x90100000 for Foo (confirmed), 0x90999999
    # for the contradicted entry (Bank 5 ELF will say a different symbol there).
    rb3wii_index = {
        # confirmed: rb3wii points to Bank5 0x90100000 which nm says "Foo__3BarFv"
        # matches Bank8 symbol at p1=0x80100000 -> "Foo__3BarFv" -> confirmed
        0x82100010: {"rb3_addr": "0x82100010", "wii_addr": hex(0x90100000),
                     "wii_name": "Bar::Foo()", "similarity": 1.0, "confidence": 0.96},
        # contradicted: rb3wii points to Bank5 0x90999999 which nm says "Other__Fn"
        # mismatches Bank8 symbol at p1=0x80100080 -> "Baz__3BarFv" -> contradicted
        0x82200020: {"rb3_addr": "0x82200020", "wii_addr": "0x90999999",
                     "wii_name": "SomeOther::Fn()", "similarity": 1.0, "confidence": 0.96},
    }

    # Synthetic Bank 5 symbol table (addr -> mangled name)
    # 0x90100000: "Foo__3BarFv" -> matches Bank8 at 0x80100000 -> confirmed
    # 0x90999999: "Other__Fn"   -> mismatches Bank8 at 0x80100080 -> contradicted
    bank5_syms = {
        0x90100000: "Foo__3BarFv",
        0x90999999: "Other__Fn",
    }

    matches = [
        # 1. SeedMatch -> ACCEPT
        {"p1_addr": "80100000", "p2_addr": "82100010",
         "match_types": ["SeedMatch"], "p1_name": "Foo__3BarFv", "p2_name": "FN1"},

        # 2. ExactInstructions -> ACCEPT
        {"p1_addr": "80100080", "p2_addr": "82100020",
         "match_types": ["ExactInstructionsFunctionHasher"], "p1_name": "Baz__3BarFv", "p2_name": "FN2"},

        # 3. VT coherent cluster (3 entries in Bar.o, small spread) -> FILTERED_VT
        # xenon_spread = 0x82200300 - 0x82200000 = 0x300 = 768 bytes < 50000
        # wii_spread = 0x80100300 - 0x80100000 = 0x300 = 768 bytes
        # ratio = 768/768 = 1.0 < 10  => coherent
        {"p1_addr": "80100000", "p2_addr": "82200000",
         "match_types": ["VTCombinedReference"], "p1_name": "Foo__3BarFv", "p2_name": "FN3"},
        {"p1_addr": "80100080", "p2_addr": "82200100",
         "match_types": ["VTCombinedReference"], "p1_name": "Baz__3BarFv", "p2_name": "FN4"},
        {"p1_addr": "80100300", "p2_addr": "82200300",
         "match_types": ["VTCombinedReference"], "p1_name": "Qux__3BarFv", "p2_name": "FN5"},

        # 4. VT scattered (AccMgr.o, xenon_spread >> max) -> CAUTION
        # xenon_spread = 0x82800000 - 0x82200500 = big (> 50000)
        {"p1_addr": "80400000", "p2_addr": "82200500",
         "match_types": ["VTCombinedReference"], "p1_name": "AccAcc__6AccMgrFv", "p2_name": "FN6"},
        {"p1_addr": "80400200", "p2_addr": "82800000",
         "match_types": ["VTCombinedReference"], "p1_name": "AccB__6AccMgrFv", "p2_name": "FN7"},

        # 5. VT singleton -> CAUTION
        {"p1_addr": "80300000", "p2_addr": "82900000",
         "match_types": ["VTCombinedReference"], "p1_name": "Stub__3NopFv", "p2_name": "FN8"},

        # 6. StringsRefsHasher -> REJECT
        {"p1_addr": "80200000", "p2_addr": "82a00000",
         "match_types": ["StringsRefsHasher"], "p1_name": "Sys__5EngineFv", "p2_name": "FN9"},

        # 7. StrUniqueFuncRefsHasher -> REJECT
        {"p1_addr": "80200000", "p2_addr": "82b00000",
         "match_types": ["StrUniqueFuncRefsHasher"], "p1_name": "Sys__5EngineFv", "p2_name": "FN10"},
    ]

    entries, summary = vet(
        matches, wii_index, rb3wii_index, DEFAULT_TIER_CONFIG, bank5_syms
    )

    # Build lookup by p2 for assertions
    by_p2 = {parse_addr(e["xenon_addr"]): e for e in entries}

    def check(p2_hex, expected_tier, expected_rb3wii):
        p2 = parse_addr(p2_hex)
        e = by_p2.get(p2)
        if e is None:
            errors.append(f"MISSING entry for {p2_hex}")
            return
        if e["tier"] != expected_tier:
            errors.append(f"{p2_hex}: tier {e['tier']!r} != expected {expected_tier!r}")
        if e["rb3wii_check"] != expected_rb3wii:
            errors.append(f"{p2_hex}: rb3wii_check {e['rb3wii_check']!r} != expected {expected_rb3wii!r}")

    # SeedMatch -> ACCEPT; rb3wii: p2=0x82100010, Bank5[0x90100000]="Foo__3BarFv"
    # == Bank8 symbol at p1=0x80100000 -> confirmed
    check("82100010", "ACCEPT", "confirmed")
    # ExactInstructions -> ACCEPT, not in rb3wii -> absent
    check("82100020", "ACCEPT", "absent")
    # VT coherent cluster -> FILTERED_VT
    check("82200000", "FILTERED_VT", "absent")
    check("82200100", "FILTERED_VT", "absent")
    check("82200300", "FILTERED_VT", "absent")
    # VT scattered -> CAUTION
    check("82200500", "CAUTION", "absent")
    check("82800000", "CAUTION", "absent")
    # VT singleton -> CAUTION
    check("82900000", "CAUTION", "absent")
    # StringsRefsHasher -> REJECT
    check("82a00000", "REJECT", "absent")
    # StrUnique -> REJECT
    check("82b00000", "REJECT", "absent")

    # rb3wii contradicted: p2=0x82200020, wii by ghidriff=0x80100080 (Baz),
    # rb3wii points to Bank5 0x90999999 = "Other__Fn" != "Baz__3BarFv" -> contradicted
    # Manually test the contradicted case by inserting an ExactInstr entry
    matches2 = [
        {"p1_addr": "80100080", "p2_addr": "82200020",
         "match_types": ["ExactInstructionsFunctionHasher"],
         "p1_name": "Baz__3BarFv", "p2_name": "FN_TEST"},
    ]
    entries2, _ = vet(matches2, wii_index, rb3wii_index, DEFAULT_TIER_CONFIG, bank5_syms)
    if not entries2:
        errors.append("Contradicted test: no entries produced")
    else:
        e2 = entries2[0]
        if e2["rb3wii_check"] != "contradicted":
            errors.append(f"Contradicted test: got {e2['rb3wii_check']!r}, expected 'contradicted'")

    # Test wii_addr emitted unconditionally even when p1 not in wii_index
    matches_novel = [
        {"p1_addr": "80ff0000", "p2_addr": "82cc0000",
         "match_types": ["SeedMatch"], "p1_name": "Novel__Fn", "p2_name": "FN_NOVEL"},
    ]
    entries_novel, _ = vet(matches_novel, wii_index, rb3wii_index, DEFAULT_TIER_CONFIG, bank5_syms)
    if not entries_novel:
        errors.append("wii_addr-always test: no entries produced")
    else:
        en = entries_novel[0]
        if en.get("wii_addr") != "0x80ff0000":
            errors.append(f"wii_addr-always: got {en.get('wii_addr')!r}, expected '0x80ff0000'")

    # Test ExactMnemonicsFunctionHasher is NOT in default ACCEPT (goes to CAUTION)
    matches_mnem = [
        {"p1_addr": "80100000", "p2_addr": "82dd0000",
         "match_types": ["ExactMnemonicsFunctionHasher"], "p1_name": "Foo__3BarFv", "p2_name": "FN_MNEM"},
    ]
    entries_mnem, _ = vet(matches_mnem, wii_index, rb3wii_index, DEFAULT_TIER_CONFIG, bank5_syms)
    if not entries_mnem:
        errors.append("ExactMnemonics default CAUTION test: no entries produced")
    else:
        em = entries_mnem[0]
        if em["tier"] != "CAUTION":
            errors.append(f"ExactMnemonics default: tier {em['tier']!r} != 'CAUTION'")
    # And confirm --accept-types ExactMnemonicsFunctionHasher promotes it to ACCEPT
    cfg_mnem = dict(DEFAULT_TIER_CONFIG)
    cfg_mnem["accept_types"] = list(set(cfg_mnem["accept_types"]) | {"ExactMnemonicsFunctionHasher"})
    entries_mnem2, _ = vet(matches_mnem, wii_index, rb3wii_index, cfg_mnem, bank5_syms)
    if not entries_mnem2 or entries_mnem2[0]["tier"] != "ACCEPT":
        errors.append(f"ExactMnemonics via --accept-types: expected ACCEPT, got "
                      f"{entries_mnem2[0]['tier'] if entries_mnem2 else 'nothing'!r}")

    # Test --accept-types override: adding StringsRefsHasher to accept
    cfg2 = dict(DEFAULT_TIER_CONFIG)
    cfg2["accept_types"] = list(set(cfg2["accept_types"]) | {"StringsRefsHasher"})
    entries3, _ = vet(matches, wii_index, rb3wii_index, cfg2, bank5_syms)
    by_p2_cfg2 = {parse_addr(e["xenon_addr"]): e for e in entries3}
    srh_e = by_p2_cfg2.get(parse_addr("82a00000"))
    if srh_e is None:
        errors.append("--accept-types override test: no entry for SRH")
    elif srh_e["tier"] != "ACCEPT":
        errors.append(f"--accept-types override: tier {srh_e['tier']!r} != 'ACCEPT'")

    # Test min_vt_score gate (requires scores field)
    matches4 = [
        {"p1_addr": "80100000", "p2_addr": "82200000",
         "match_types": ["VTCombinedReference"],
         "p1_name": "Foo__3BarFv", "p2_name": "FN_LOWSCORE",
         "scores": {"VTCombinedReference": {"similarity": 0.5, "confidence": 10.0, "product": 5.0}}},
        {"p1_addr": "80100080", "p2_addr": "82200100",
         "match_types": ["VTCombinedReference"],
         "p1_name": "Baz__3BarFv", "p2_name": "FN_HIGHSCORE",
         "scores": {"VTCombinedReference": {"similarity": 0.9, "confidence": 15.0, "product": 13.5}}},
    ]
    cfg4 = dict(DEFAULT_TIER_CONFIG)
    cfg4["min_vt_score"] = 10.0
    entries4, _ = vet(matches4, wii_index, rb3wii_index, cfg4, bank5_syms)
    by_p2_4 = {parse_addr(e["xenon_addr"]): e for e in entries4}
    low_e = by_p2_4.get(parse_addr("82200000"))
    high_e = by_p2_4.get(parse_addr("82200100"))
    if low_e is None or high_e is None:
        errors.append("min_vt_score test: entries missing")
    else:
        if low_e["tier"] != "REJECT":
            errors.append(f"min_vt_score low: tier {low_e['tier']!r} != 'REJECT'")
        if high_e["tier"] not in ("FILTERED_VT", "CAUTION"):
            errors.append(f"min_vt_score high: tier {high_e['tier']!r} unexpected")

    # Count total checks
    n_checks = (
        10  # tier+rb3wii for main 10 entries
        + 1  # contradicted explicit test
        + 1  # wii_addr-always
        + 2  # ExactMnemonics default CAUTION + via --accept-types
        + 1  # --accept-types SRH override
        + 2  # min_vt_score low+high
    )

    if errors:
        print("SELFTEST FAILURES:")
        for err in errors:
            print(f"  FAIL: {err}")
        sys.exit(1)
    else:
        print(f"SELFTEST: all {n_checks} checks passed.")


if __name__ == "__main__":
    main()
