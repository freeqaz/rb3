#!/usr/bin/env python3
"""Score a completed Wii<->Xenon ghidriff run (tools/ghidra/run_ghidriff_xenon.sh).

EXPERIMENT 1 success bar: precision > 0.32 (the BinDiff oracle's measured
precision) on game-code (band3/) identities at comparable recall.

Inputs:
  - the run's json/<name>.matches.json -> matches['function_matches'] list
    ({p1_addr, p2_addr, match_types, p1_name, p2_name}; addresses are Ghidra
    address strings, hex with or without 0x)
  - seeds.json (the --seed-matches input; seeded pairs are GIVENS, excluded
    from every metric)
  - holdout.json (146 cross-validated Wii<->Xenon identities, Xenon addr +
    TU stem only — the eval holdout build_xenon_seeds.py excluded from seeds)
  - rb3-xenon/tools/bindiff_match.json (Xenon addr -> DC3 MSVC name oracle,
    0.32 precision overall, much better at sim==1.0 & conf>=0.95)
  - the Wii CodeWarrior map (addr -> symbol + TU + module category)

Metrics:
  (a) holdout recovery — of the holdout Xenon addrs (minus any that were
      seeds), how many got matched to a Wii function whose TU stem agrees
      with the holdout's TU stem.
  (b) DC3-agreement — for matched Xenon addrs that also appear in
      bindiff_match.json (non-seed), does the matched Wii symbol agree with
      the DC3 name under build_xenon_seeds.normalize_demangled's join key
      (imported, not forked)? Reported for all bindiff entries and for the
      seed-grade high-confidence subset.
  (c) new-coverage — matched Xenon addrs with NO prior identity (not in
      seeds/bindiff/holdout), broken down by match type and by the Wii
      symbol's TU category (band3|main = game code, the prize).
  (d) per-match-type precision proxy from (a) + (b high-conf) combined.

SymbolsHash caveat: the Xenon binary is stripped; its only named functions
are XEXLoaderWV import thunks. Any cross-run pair whose ONLY match type is
SymbolsHash is flagged suspect and excluded from new-coverage counts (listed
separately in the report).

Usage:
  eval_xenon_matches.py --run-dir build/SZBE69_B8/ghidra/ghidriff-xenon
  eval_xenon_matches.py --matches path/to/x.matches.json --out report.json
  eval_xenon_matches.py --help

Unit tests (synthetic fixtures, no Ghidra, no demanglers):
  python3 tools/ghidra/test_eval_xenon_matches.py
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path
from typing import Dict, List, Optional, Set, Tuple

TOOLS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(TOOLS_DIR))

# Reuse the seed builder's normalize/demangle logic — single source of truth
# for what "the same function identity" means across the two manglings.
from build_xenon_seeds import (  # noqa: E402
    CRT_BLOCKLIST,
    demangle_msvc_batch,
    demangle_wii_batch,
    normalize_demangled,
)

RB3 = TOOLS_DIR.parents[1]
XENON = RB3.parent / "rb3-xenon"

DEFAULT_RUN_DIR = RB3 / "build" / "SZBE69_B8" / "ghidra" / "ghidriff-xenon"
DEFAULT_SEEDS = RB3 / "build" / "SZBE69_B8" / "ghidra" / "xenon-seeds" / "seeds.json"
DEFAULT_HOLDOUT = RB3 / "build" / "SZBE69_B8" / "ghidra" / "xenon-seeds" / "holdout.json"
DEFAULT_BINDIFF = XENON / "tools" / "bindiff_match.json"
DEFAULT_WII_MAP = RB3 / "orig" / "SZBE69_B8" / "files" / "band_r_wii.map"

GAME_CATEGORIES = {"band3", "main"}


# ---------------------------------------------------------------------------
# Address / map parsing
# ---------------------------------------------------------------------------
def parse_addr(s) -> Optional[int]:
    """Ghidra/json address -> int. Accepts '0x82260018', '82260018',
    'ram:82260018', ints. Returns None on garbage."""
    if isinstance(s, int):
        return s
    if not isinstance(s, str):
        return None
    s = s.strip().lower()
    if ":" in s:  # 'ram:80004000' style address-space prefix
        s = s.rsplit(":", 1)[1]
    if s.startswith("0x"):
        s = s[2:]
    try:
        return int(s, 16)
    except ValueError:
        return None


# Same line shape as build_xenon_seeds._MAP_LINE but keeps the TU column.
_MAP_LINE_TU = re.compile(
    r"^\s+([0-9a-f]{8})\s+([0-9a-f]{6})\s+([0-9a-f]{8})\s+[0-9a-f]{8}\s+\d+\s+(\S+)"
    r"(?:\s*\t(.+))?$"
)


def categorize_tu(tu_field: str) -> Tuple[Optional[str], Optional[str]]:
    """Map TU column -> (tu_basename like 'Game.o', category).

    Categories (from the map's own provenance):
      'band3' / 'system' / 'network' — full CodeWarrior source paths
          ('lib.a C:\\hproj\\band3_wii\\<module>\\src\\...\\X.o')
      'main'  — bare single-token objects (App.o, Main.o, ...) = game code
      'sdk'   — '<archive>.a obj.o' entries (RVL SDK / MSL / DWC / Bink ...)
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
    # '<lib>.a <obj>.o' (SDK/third-party) or odd linker-generated rows
    if parts[-1].endswith(".o"):
        return parts[-1], "sdk"
    return None, None


def tu_stem(tu_basename: Optional[str]) -> Optional[str]:
    if not tu_basename:
        return None
    return tu_basename[:-2] if tu_basename.endswith(".o") else tu_basename


def parse_wii_map_index(map_path: Path) -> Dict[int, dict]:
    """CodeWarrior map -> {vaddr: {symbol, tu, category}} for .text/.init
    function symbols (size > 0). First symbol at an address wins."""
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
            if int(m.group(2), 16) == 0:
                continue
            sym = m.group(4)
            if sym[0] in "@*.":
                continue
            addr = int(m.group(3), 16)
            if addr in out:
                continue
            tu, category = categorize_tu(m.group(5) or "")
            out[addr] = {"symbol": sym, "tu": tu, "category": category}
    return out


# ---------------------------------------------------------------------------
# Identity-agreement judgment (build_xenon_seeds join logic, applied pairwise)
# ---------------------------------------------------------------------------
def judge_agreement(wii_symbol: str, dc3_name: str,
                    wii_dem: Dict[str, str], msvc_dem: Dict[str, str]) -> Tuple[str, str]:
    """Does the matched Wii symbol denote the same function identity as the
    DC3 (MSVC) name, under the seed builder's normalize-join?

    Returns (verdict, detail) where verdict is one of:
      'agree' / 'disagree' / 'unjudgeable'
    wii_dem / msvc_dem: mangled -> demangled maps (injected; main() fills
    them with the real batch demanglers, tests inject fixtures).
    """
    wii_mangled = "__" in wii_symbol
    msvc_mangled = dc3_name.startswith("?")

    if not wii_mangled and not msvc_mangled:
        # plain/extern-"C" join: exact name equality, CRT names can't prove
        # shared source (MSVC CRT memcpy vs Wii MSL memcpy)
        if wii_symbol == dc3_name:
            if wii_symbol in CRT_BLOCKLIST or wii_symbol.startswith("_"):
                return "unjudgeable", "crt_blocklist_name"
            return "agree", "plain_name"
        return "disagree", f"plain {wii_symbol!r} != {dc3_name!r}"

    if not wii_mangled or not msvc_mangled:
        return "unjudgeable", "mangled_vs_plain"

    dem1 = wii_dem.get(wii_symbol)
    dem2 = msvc_dem.get(dc3_name)
    if not dem1 or dem1 == wii_symbol:
        return "unjudgeable", "wii_undemangleable"
    if not dem2:
        return "unjudgeable", "msvc_undemangleable"
    key1, cause1 = normalize_demangled(dem1)
    key2, cause2 = normalize_demangled(dem2)
    if key1 is None or key2 is None:
        return "unjudgeable", f"normalize: wii={cause1} msvc={cause2}"
    if key1 == key2:
        return "agree", "key_match"
    return "disagree", f"key {key1!r} != {key2!r}"


# ---------------------------------------------------------------------------
# Core evaluation (pure data; unit-testable without Ghidra or demanglers)
# ---------------------------------------------------------------------------
def evaluate(function_matches: List[dict],
             seeds: List[dict],
             holdout_entries: List[dict],
             bindiff_entries: List[dict],
             wii_index: Dict[int, dict],
             wii_dem: Optional[Dict[str, str]] = None,
             msvc_dem: Optional[Dict[str, str]] = None,
             bindiff_min_sim: float = 1.0,
             bindiff_min_conf: float = 0.95) -> dict:
    wii_dem = wii_dem or {}
    msvc_dem = msvc_dem or {}

    seed_p1 = {parse_addr(s["p1_addr"]) for s in seeds}
    seed_p2 = {parse_addr(s["p2_addr"]) for s in seeds}
    seed_p1.discard(None)
    seed_p2.discard(None)

    holdout_by_addr = {}
    for e in holdout_entries:
        a = parse_addr(e["addr"])
        if a is not None:
            holdout_by_addr[a] = e
    eligible_holdout = {a for a in holdout_by_addr if a not in seed_p2}

    bindiff_by_addr: Dict[int, dict] = {}
    bindiff_dupes = 0
    for e in bindiff_entries:
        a = parse_addr(e["rb3_addr"])
        if a is None:
            continue
        if a in bindiff_by_addr:
            bindiff_dupes += 1
            continue
        bindiff_by_addr[a] = e

    # ---- pair triage -----------------------------------------------------
    pairs = []          # non-seed, usable
    n_seed = 0
    seed_conflicts = []  # non-seed claim re-using a seeded endpoint (SymbolsHash etc.)
    n_bad_addr = 0
    for m in function_matches:
        p1 = parse_addr(m.get("p1_addr"))
        p2 = parse_addr(m.get("p2_addr"))
        if p1 is None or p2 is None:
            n_bad_addr += 1
            continue
        mtypes = sorted(set(m.get("match_types") or []))
        if "SeedMatch" in mtypes:
            n_seed += 1
            continue
        if p1 in seed_p1 or p2 in seed_p2:
            seed_conflicts.append({"p1_addr": hex(p1), "p2_addr": hex(p2),
                                   "match_types": mtypes})
            continue
        wii = wii_index.get(p1)
        pairs.append({
            "p1": p1, "p2": p2, "match_types": mtypes,
            "p1_name": m.get("p1_name"), "p2_name": m.get("p2_name"),
            "wii_symbol": wii["symbol"] if wii else None,
            "wii_tu": wii["tu"] if wii else None,
            "wii_category": wii["category"] if wii else None,
            "suspect_symbolshash": mtypes == ["SymbolsHash"],
        })

    by_p2: Dict[int, List[dict]] = defaultdict(list)
    for p in pairs:
        by_p2[p["p2"]].append(p)

    # ---- (a) holdout recovery --------------------------------------------
    # Some holdout TU stems are Xenon-only (no Wii TU of that name exists, e.g.
    # 360-specific files): a matched pair can never TU-agree there, so those
    # entries are reported but excluded from correct/wrong scoring.
    wii_stems = {tu_stem(v["tu"]) for v in wii_index.values() if v["tu"]}
    holdout_rows = []
    a_counts = Counter()
    judged = []  # (pair, correct?) feeding metric (d)
    for addr in sorted(eligible_holdout):
        entry = holdout_by_addr[addr]
        scoreable = entry["stem"] in wii_stems
        cands = by_p2.get(addr, [])
        if not cands:
            result = "unmatched" if scoreable else "unmatched_unscoreable_stem"
            a_counts[result] += 1
            holdout_rows.append({"xenon_addr": hex(addr), "stem": entry["stem"],
                                 "result": result})
            continue
        pair = cands[0]  # accepted matches are one-to-one per p2
        stem = tu_stem(pair["wii_tu"])
        if not scoreable:
            a_counts["recovered_unscoreable_stem"] += 1
            result = "recovered_unscoreable_stem"
        elif pair["wii_symbol"] is None:
            a_counts["recovered_unresolved_wii"] += 1
            result = "recovered_unresolved_wii"
        elif stem == entry["stem"]:
            a_counts["recovered_correct"] += 1
            result = "recovered_correct"
            judged.append((pair, True))
        else:
            a_counts["recovered_wrong"] += 1
            result = "recovered_wrong"
            judged.append((pair, False))
        holdout_rows.append({
            "xenon_addr": hex(addr), "stem": entry["stem"], "result": result,
            "wii_addr": hex(pair["p1"]), "wii_symbol": pair["wii_symbol"],
            "wii_tu": pair["wii_tu"], "match_types": pair["match_types"],
        })
    n_eligible = len(eligible_holdout)
    n_scoreable = n_eligible - (a_counts["recovered_unscoreable_stem"] +
                                a_counts["unmatched_unscoreable_stem"])
    holdout_metrics = {
        "eligible": n_eligible,
        "excluded_as_seeds": len(holdout_by_addr) - n_eligible,
        "scoreable": n_scoreable,
        **{k: a_counts.get(k, 0) for k in
           ("recovered_correct", "recovered_wrong", "recovered_unresolved_wii",
            "recovered_unscoreable_stem", "unmatched", "unmatched_unscoreable_stem")},
        "recovery_rate": (a_counts["recovered_correct"] / n_scoreable) if n_scoreable else None,
        "precision_on_recovered": (
            a_counts["recovered_correct"] /
            (a_counts["recovered_correct"] + a_counts["recovered_wrong"])
            if (a_counts["recovered_correct"] + a_counts["recovered_wrong"]) else None),
    }

    # ---- (b) DC3 (bindiff) agreement --------------------------------------
    dc3_rows = []
    b_all = Counter()
    b_high = Counter()
    for pair in pairs:
        entry = bindiff_by_addr.get(pair["p2"])
        if entry is None:
            continue
        high_conf = (entry.get("similarity", 0) >= bindiff_min_sim and
                     entry.get("confidence", 0) >= bindiff_min_conf)
        if pair["wii_symbol"] is None:
            verdict, detail = "unjudgeable", "wii_addr_not_in_map"
        else:
            verdict, detail = judge_agreement(pair["wii_symbol"], entry["dc3_name"],
                                              wii_dem, msvc_dem)
        b_all[verdict] += 1
        if high_conf:
            b_high[verdict] += 1
            if verdict in ("agree", "disagree"):
                judged.append((pair, verdict == "agree"))
        dc3_rows.append({
            "xenon_addr": hex(pair["p2"]), "wii_addr": hex(pair["p1"]),
            "wii_symbol": pair["wii_symbol"], "dc3_name": entry["dc3_name"],
            "bindiff_similarity": entry.get("similarity"),
            "bindiff_confidence": entry.get("confidence"),
            "high_conf": high_conf, "verdict": verdict, "detail": detail,
            "match_types": pair["match_types"],
        })

    def _agree_stats(c: Counter) -> dict:
        judged_n = c["agree"] + c["disagree"]
        return {
            "agree": c["agree"], "disagree": c["disagree"],
            "unjudgeable": c["unjudgeable"],
            "agreement_rate": (c["agree"] / judged_n) if judged_n else None,
        }

    dc3_metrics = {
        "matched_in_bindiff": len(dc3_rows),
        "bindiff_dupes_skipped": bindiff_dupes,
        "all_entries": _agree_stats(b_all),
        "high_conf": {"min_sim": bindiff_min_sim, "min_conf": bindiff_min_conf,
                      **_agree_stats(b_high)},
    }

    # ---- (c) new coverage --------------------------------------------------
    known_p2 = set(holdout_by_addr) | set(bindiff_by_addr) | seed_p2
    new_rows = []
    suspect_rows = []
    c_by_type = Counter()
    c_by_cat = Counter()
    for pair in pairs:
        if pair["p2"] in known_p2:
            continue
        row = {
            "xenon_addr": hex(pair["p2"]), "wii_addr": hex(pair["p1"]),
            "wii_symbol": pair["wii_symbol"], "wii_tu": pair["wii_tu"],
            "wii_category": pair["wii_category"],
            "match_types": pair["match_types"],
        }
        if pair["suspect_symbolshash"]:
            suspect_rows.append(row)
            continue
        new_rows.append(row)
        cat = pair["wii_category"] or "unresolved"
        c_by_cat[cat] += 1
        for t in pair["match_types"]:
            c_by_type[t] += 1
    new_metrics = {
        "total": len(new_rows),
        "game_code": sum(c_by_cat[c] for c in GAME_CATEGORIES),
        "by_category": dict(c_by_cat.most_common()),
        "by_match_type": dict(c_by_type.most_common()),
        "suspect_symbolshash_only": len(suspect_rows),
    }

    # ---- (d) per-match-type precision proxy --------------------------------
    d_tally: Dict[str, Counter] = defaultdict(Counter)
    for pair, correct in judged:
        d_tally["OVERALL"]["correct" if correct else "wrong"] += 1
        for t in pair["match_types"]:
            d_tally[t]["correct" if correct else "wrong"] += 1
    precision_by_type = {}
    for t, c in sorted(d_tally.items(), key=lambda kv: -(kv[1]["correct"] + kv[1]["wrong"])):
        n = c["correct"] + c["wrong"]
        precision_by_type[t] = {
            "judged": n, "correct": c["correct"], "wrong": c["wrong"],
            "precision": (c["correct"] / n) if n else None,
        }

    return {
        "totals": {
            "function_matches": len(function_matches),
            "seed_pairs_in_output": n_seed,
            "seed_conflicts": len(seed_conflicts),
            "bad_addresses": n_bad_addr,
            "scored_pairs": len(pairs),
            "wii_addr_resolved": sum(1 for p in pairs if p["wii_symbol"]),
        },
        "holdout_recovery": holdout_metrics,
        "dc3_agreement": dc3_metrics,
        "new_coverage": new_metrics,
        "precision_by_match_type": precision_by_type,
        "lists": {
            "holdout": holdout_rows,
            "dc3": dc3_rows,
            "new_coverage": new_rows,
            "suspect_symbolshash_only": suspect_rows,
            "seed_conflicts": seed_conflicts,
        },
    }


# ---------------------------------------------------------------------------
# IO + reporting
# ---------------------------------------------------------------------------
def load_function_matches(matches_path: Path) -> List[dict]:
    obj = json.loads(matches_path.read_text())
    if isinstance(obj, dict):
        if "function_matches" in obj:
            return obj["function_matches"]
        if "matches" in obj and isinstance(obj["matches"], dict) \
                and "function_matches" in obj["matches"]:
            return obj["matches"]["function_matches"]
    raise SystemExit(
        f"{matches_path}: no function_matches list found — was the run made with "
        f"the rb3-improvements ghidriff branch?")


def find_matches_json(run_dir: Path) -> Path:
    cands = sorted(run_dir.glob("json/*.matches.json")) + \
        sorted(run_dir.glob("*.matches.json"))
    if not cands:
        raise SystemExit(f"no *.matches.json under {run_dir} (looked in ./ and ./json/)")
    return max(cands, key=lambda p: p.stat().st_mtime)


def build_demangle_maps(pairs_syms_wii: Set[str], dc3_names: Set[str],
                        skip: bool) -> Tuple[Dict[str, str], Dict[str, str]]:
    wii_dem: Dict[str, str] = {}
    msvc_dem: Dict[str, str] = {}
    if skip:
        return wii_dem, msvc_dem
    wii_mangled = sorted(s for s in pairs_syms_wii if s and "__" in s)
    msvc_mangled = sorted(n for n in dc3_names if n and n.startswith("?"))
    if wii_mangled:
        try:
            wii_dem = demangle_wii_batch(wii_mangled)
        except (FileNotFoundError, OSError) as e:
            print(f"[warn] Wii demangler unavailable ({e}); mangled Wii joins "
                  f"will be 'unjudgeable'", file=sys.stderr)
    if msvc_mangled:
        try:
            msvc_dem = demangle_msvc_batch(msvc_mangled)
        except (FileNotFoundError, OSError) as e:
            print(f"[warn] llvm-undname unavailable ({e}); MSVC joins will be "
                  f"'unjudgeable'", file=sys.stderr)
    return wii_dem, msvc_dem


def print_summary(rep: dict) -> None:
    t = rep["totals"]
    print("=== Wii<->Xenon ghidriff eval ===")
    print(f"pairs in output: {t['function_matches']}  "
          f"(seeds: {t['seed_pairs_in_output']}, seed-conflicts: {t['seed_conflicts']}, "
          f"bad-addr: {t['bad_addresses']})")
    print(f"scored (non-seed) pairs: {t['scored_pairs']}  "
          f"wii-addr resolved in map: {t['wii_addr_resolved']}")

    h = rep["holdout_recovery"]
    print("\n--- (a) holdout recovery ---")
    print(f"eligible: {h['eligible']} (excluded as seeds: {h['excluded_as_seeds']}, "
          f"scoreable stems: {h['scoreable']})")
    print(f"recovered correct: {h['recovered_correct']}  wrong: {h['recovered_wrong']}  "
          f"unresolved-wii: {h['recovered_unresolved_wii']}  unmatched: {h['unmatched']}  "
          f"unscoreable-stem: "
          f"{h['recovered_unscoreable_stem'] + h['unmatched_unscoreable_stem']}")
    rr = h["recovery_rate"]
    pr = h["precision_on_recovered"]
    print(f"recovery rate: {rr:.3f}" if rr is not None else "recovery rate: n/a", end="  ")
    print(f"precision on recovered: {pr:.3f}" if pr is not None else "precision on recovered: n/a")

    d = rep["dc3_agreement"]
    print("\n--- (b) DC3 (bindiff) agreement ---")
    print(f"matched pairs also in bindiff: {d['matched_in_bindiff']}")
    for k in ("all_entries", "high_conf"):
        s = d[k]
        ar = s["agreement_rate"]
        ar_s = f"{ar:.3f}" if ar is not None else "n/a"
        print(f"  {k}: agree {s['agree']} / disagree {s['disagree']} "
              f"/ unjudgeable {s['unjudgeable']}  (agreement {ar_s})")

    n = rep["new_coverage"]
    print("\n--- (c) new coverage (no prior identity) ---")
    print(f"total: {n['total']}   GAME CODE (band3+main): {n['game_code']}   "
          f"suspect SymbolsHash-only (excluded): {n['suspect_symbolshash_only']}")
    print(f"by category: {n['by_category']}")
    print(f"by match type: {n['by_match_type']}")

    print("\n--- (d) precision proxy by match type (holdout + high-conf bindiff) ---")
    print(f"{'match type':<28} {'judged':>6} {'correct':>8} {'wrong':>6} {'precision':>10}")
    for t_name, s in rep["precision_by_match_type"].items():
        p = s["precision"]
        p_s = f"{p:.3f}" if p is not None else "n/a"
        print(f"{t_name:<28} {s['judged']:>6} {s['correct']:>8} {s['wrong']:>6} {p_s:>10}")
    overall = rep["precision_by_match_type"].get("OVERALL", {}).get("precision")
    if overall is not None:
        bar = "PASS" if overall > 0.32 else "FAIL"
        print(f"\nEXPERIMENT 1 bar (precision > 0.32): {overall:.3f} -> {bar}")


def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--run-dir", type=Path, default=DEFAULT_RUN_DIR,
                    help="ghidriff output dir (json/*.matches.json found inside)")
    ap.add_argument("--matches", type=Path, default=None,
                    help="explicit path to the .matches.json (overrides --run-dir)")
    ap.add_argument("--seeds", type=Path, default=DEFAULT_SEEDS)
    ap.add_argument("--holdout", type=Path, default=DEFAULT_HOLDOUT)
    ap.add_argument("--bindiff", type=Path, default=DEFAULT_BINDIFF)
    ap.add_argument("--wii-map", type=Path, default=DEFAULT_WII_MAP)
    ap.add_argument("--out", type=Path, default=None,
                    help="report JSON path (default: <run-dir>/eval_report.json)")
    ap.add_argument("--bindiff-min-sim", type=float, default=1.0,
                    help="high-conf bindiff subset gate (default 1.0, the seed filter)")
    ap.add_argument("--bindiff-min-conf", type=float, default=0.95,
                    help="high-conf bindiff subset gate (default 0.95, the seed filter)")
    ap.add_argument("--no-demangle", action="store_true",
                    help="skip the external demanglers (mangled joins become unjudgeable)")
    args = ap.parse_args()

    matches_path = args.matches or find_matches_json(args.run_dir)
    print(f"[eval] matches: {matches_path}", file=sys.stderr)
    function_matches = load_function_matches(matches_path)

    seeds = json.loads(args.seeds.read_text())
    holdout = json.loads(args.holdout.read_text())
    holdout_entries = holdout.get("entries", holdout) if isinstance(holdout, dict) else holdout
    bindiff_entries = json.loads(args.bindiff.read_text())
    wii_index = parse_wii_map_index(args.wii_map)
    print(f"[eval] wii map: {len(wii_index)} function addrs; seeds: {len(seeds)}; "
          f"holdout: {len(holdout_entries)}; bindiff: {len(bindiff_entries)}",
          file=sys.stderr)

    # demangle only what judgment (b) will actually look at
    bindiff_addrs = {parse_addr(e["rb3_addr"]) for e in bindiff_entries}
    need_wii: Set[str] = set()
    need_msvc: Set[str] = set()
    bindiff_by_addr = {parse_addr(e["rb3_addr"]): e for e in bindiff_entries}
    for m in function_matches:
        p2 = parse_addr(m.get("p2_addr"))
        if p2 in bindiff_addrs:
            p1 = parse_addr(m.get("p1_addr"))
            wii = wii_index.get(p1)
            if wii:
                need_wii.add(wii["symbol"])
            e = bindiff_by_addr.get(p2)
            if e:
                need_msvc.add(e["dc3_name"])
    wii_dem, msvc_dem = build_demangle_maps(need_wii, need_msvc, args.no_demangle)

    rep = evaluate(function_matches, seeds, holdout_entries, bindiff_entries,
                   wii_index, wii_dem, msvc_dem,
                   bindiff_min_sim=args.bindiff_min_sim,
                   bindiff_min_conf=args.bindiff_min_conf)
    rep["inputs"] = {
        "matches": str(matches_path), "seeds": str(args.seeds),
        "holdout": str(args.holdout), "bindiff": str(args.bindiff),
        "wii_map": str(args.wii_map),
    }

    print_summary(rep)

    out = args.out or ((args.run_dir if args.run_dir.is_dir() else matches_path.parent)
                       / "eval_report.json")
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(rep, indent=1) + "\n")
    print(f"\n[eval] report written: {out}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
