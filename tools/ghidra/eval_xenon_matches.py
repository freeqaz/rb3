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

# ---------------------------------------------------------------------------
# Wii<->Xbox platform-class rename aliasing (forensics §1D V1 + scout 4 §4)
#
# The same Harmonix source class is renamed between the Wii (MWCC) build and
# the Xbox 360 (MSVC/DC3) build. ghidriff/VT correctly pairs the *function*
# but the eval's class-name join can't bridge the rename, so the pair is
# falsely judged 'disagree'. We credit these as agree but tag them distinctly
# ('agree (platform-alias)') so they stay auditable and never masquerade as a
# real name-key match.
#
# The mechanism: normalize the FIRST scope (class) token on each side by
# stripping/mapping a known platform prefix, then re-compare the keys. A
# rename is credited ONLY when, after prefix normalization, scope+method+
# arity+constness ALL match — i.e. the rename is the *sole* difference. This
# refuses to alias genuinely-different functions (e.g. KeylessHash<char,char>
# vs KeylessHash<AllocInfo,void> share no method+arity coincidence to launder).
#
# Two complementary tables:
#   _PLATFORM_PREFIXES — generic prefix pairs applied to the bare class stem
#                        (Wii<->Dx, Wii<->Ng, Wii<->Xbox, Band<->Ham, ...).
#   _CLASS_ALIASES     — explicit full-name pairs for the documented twins,
#                        as a belt-and-suspenders backstop / audit anchor.
# ---------------------------------------------------------------------------
_PLATFORM_PREFIXES = [
    ("Wii", "Dx"),
    ("Wii", "Ng"),
    ("Wii", "Xbox"),
    ("Wii", ""),
    ("Band", "Ham"),
    ("Band", ""),
]

# Explicit class-rename twins called out in the forensics doc (forensics §1D
# V1). Stored canonicalized (sorted) so lookup is order-independent.
_CLASS_ALIASES_RAW = [
    ("WiiMovie", "DxMovie"),
    ("BandCamShot", "HamCamShot"),
    ("BandSongMetadata", "HamSongMetadata"),
    ("BandIKEffector", "HamIKEffector"),
    ("WiiPostProc", "NgPostProc"),
    ("WiiDOFProc", "NgDOFProc"),
    ("WiiMovie", "DxMovie"),
]
_CLASS_ALIAS_PAIRS = {frozenset(p) for p in _CLASS_ALIASES_RAW}

# Default tolerance for cross-compiler arity drift on an otherwise-identical
# (same scope, method, constness) identity. The same source function can
# demangle to differing arity across MWCC vs MSVC — e.g. QuatKeys::SetFrame is
# (float,float) on Wii but (float,float,float) on Xbox (forensics §1D V2: the
# Xbox build took an extra float; bindiff sim=1.0 conf=0.993). This is NOT a
# normalize_demangled parse bug — both manglings parse correctly; the arity
# genuinely differs. Credited only on a tiny delta so unrelated overloads that
# happen to share a class+method don't get laundered.
ARITY_TOLERANCE = 1


def _strip_platform_prefix(stem: str) -> Set[str]:
    """All plausible platform-prefix-normalized forms of a bare class stem.

    Returns a set that always includes `stem` itself plus, for every known
    prefix pair (a,b), the result of swapping an `a`-prefix to `b` and vice
    versa (and dropping a bare prefix). Used to test whether two class stems
    are the same source class under a platform rename.
    """
    forms = {stem}
    for a, b in _PLATFORM_PREFIXES:
        if a and stem.startswith(a) and len(stem) > len(a):
            forms.add(b + stem[len(a):])
        if b and stem.startswith(b) and len(stem) > len(b):
            forms.add(a + stem[len(b):])
    return forms


def _class_tokens_alias(t1: str, t2: str) -> bool:
    """Do two class-scope tokens denote the same source class under a known
    Wii<->Xbox rename? True if equal, in the explicit alias table, or unified
    by a platform-prefix swap."""
    if t1 == t2:
        return True
    if frozenset((t1, t2)) in _CLASS_ALIAS_PAIRS:
        return True
    return bool(_strip_platform_prefix(t1) & _strip_platform_prefix(t2))


def _scope_aliases(s1: Tuple[str, ...], s2: Tuple[str, ...]) -> bool:
    """Two scope tuples are platform-alias-equal when they have the same
    length and each component pair aliases (only the class tokens ever differ
    in practice, but we check every component for safety)."""
    if len(s1) != len(s2):
        return False
    return all(_class_tokens_alias(a, b) for a, b in zip(s1, s2))


def _credit_aliased(key1, key2) -> Optional[str]:
    """If two normalized identity keys disagree ONLY by a Wii<->Xbox class
    rename and/or a tolerated arity drift, return a credit tag string;
    otherwise None. key = (scope_tuple, method, argc, is_const)."""
    s1, m1, a1, c1 = key1
    s2, m2, a2, c2 = key2
    if m1 != m2 or c1 != c2:
        return None
    arity_ok = (a1 == a2) or (a1 is not None and a2 is not None
                              and abs(a1 - a2) <= ARITY_TOLERANCE)
    if not arity_ok:
        return None
    scope_same = s1 == s2
    scope_alias = (not scope_same) and _scope_aliases(s1, s2)
    if not (scope_same or scope_alias):
        return None
    tags = []
    if scope_alias:
        tags.append("platform-alias")
    if a1 != a2:
        tags.append("arity-tolerant")
    if not tags:
        return None  # identical key — caller already handled it as a match
    return "+".join(tags)


DEFAULT_RUN_DIR = RB3 / "build" / "SZBE69_B8" / "ghidra" / "ghidriff-xenon"
DEFAULT_SEEDS = RB3 / "build" / "SZBE69_B8" / "ghidra" / "xenon-seeds" / "seeds.json"
DEFAULT_HOLDOUT = RB3 / "build" / "SZBE69_B8" / "ghidra" / "xenon-seeds" / "holdout.json"
DEFAULT_KNOWN_NEGATIVES = (RB3 / "build" / "SZBE69_B8" / "ghidra" / "xenon-seeds"
                           / "known_negatives.json")
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
    """CodeWarrior map -> {vaddr: {symbol, tu, category, size}} for .text/.init
    function symbols (size > 0). First symbol at an address wins. `size` is the
    CW map's byte size column (used for the BinDiff-stub oracle-trust gate)."""
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
            tu, category = categorize_tu(m.group(5) or "")
            out[addr] = {"symbol": sym, "tu": tu, "category": category,
                         "size": size}
    return out


# ---------------------------------------------------------------------------
# Identity-agreement judgment (build_xenon_seeds join logic, applied pairwise)
# ---------------------------------------------------------------------------
def is_agree(verdict: str) -> bool:
    """True for any agreement verdict, plain or aliased
    ('agree', 'agree (platform-alias)', 'agree (arity-tolerant)', ...)."""
    return verdict == "agree" or verdict.startswith("agree (")


def verdict_bucket(verdict: str) -> str:
    """Collapse a (possibly tagged) verdict into a counter bucket:
    'agree' (plain key/name match), 'agree_alias' (credited via a Wii<->Xbox
    rename and/or arity tolerance), 'disagree', or 'unjudgeable'."""
    if verdict == "agree":
        return "agree"
    if verdict.startswith("agree ("):
        return "agree_alias"
    return verdict if verdict in ("disagree", "unjudgeable") else "unjudgeable"


def judge_agreement(wii_symbol: str, dc3_name: str,
                    wii_dem: Dict[str, str], msvc_dem: Dict[str, str],
                    credit_aliases: bool = False) -> Tuple[str, str]:
    """Does the matched Wii symbol denote the same function identity as the
    DC3 (MSVC) name, under the seed builder's normalize-join?

    Returns (verdict, detail) where verdict is one of:
      'agree' / 'disagree' / 'unjudgeable'
    or, when credit_aliases is True and the only difference is a Wii<->Xbox
    class rename and/or a tolerated arity drift, a tagged
      'agree (platform-alias)' / 'agree (arity-tolerant)' /
      'agree (platform-alias+arity-tolerant)'.

    credit_aliases defaults False so the eval is byte-identical to the legacy
    report unless the operator opts in (--credit-platform-alias).
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
    # Same source function, Wii<->Xbox class rename and/or cross-compiler
    # arity drift only? Credit it but tag the verdict distinctly so the report
    # can audit (and so it never looks like a clean name-key match).
    if credit_aliases:
        tag = _credit_aliased(key1, key2)
        if tag is not None:
            return f"agree ({tag})", f"aliased {key1!r} ~= {key2!r}"
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
             bindiff_min_conf: float = 0.95,
             credit_aliases: bool = False,
             exclude_match_types: Optional[Set[str]] = None,
             min_vt_score: Optional[float] = None,
             low_trust_stub: bool = False,
             stub_size_max: int = 88,
             stratify: bool = False,
             known_negatives: Optional[List[dict]] = None) -> dict:
    """Score a ghidriff Wii<->Xenon run.

    New optional behaviours (all DEFAULT-OFF so the report is byte-identical
    to the legacy one when unset):
      credit_aliases      — credit Wii<->Xbox class renames + arity drift as
                            agreement (forensics §1D V1/V2); tagged distinctly.
      exclude_match_types — drop matches whose ENTIRE type set lies within this
                            set BEFORE any metric (e.g. the StringsRefsHasher
                            noise). A pair keeps surviving if it carries any
                            non-excluded type.
      min_vt_score        — if a pair's exported VTCombinedReference
                            scores.product is below this floor, drop the VT
                            type from the pair (and the pair if VT was its only
                            type). Inactive when the scores field is absent.
      low_trust_stub      — enable the BinDiff-stub oracle-trust gate below.
      stub_size_max       — DC3-oracle low-trust threshold: bindiff 'disagree'
                            verdicts on Wii functions of size <= this are
                            BinDiff-untrustworthy stub shapes (forensics §1B);
                            bucketed separately, not scored as hard-wrong.
      stratify            — also emit precision_by_match_type broken down per
                            wii_category (band3/system/network/sdk/main).
      known_negatives     — round-2 human-judged-WRONG (xenon_addr -> wii_addr)
                            identity pairs. A scored match that recurs the EXACT
                            (p1,p2) pair counts WRONG in per-type precision and a
                            new 'known_negatives' report section. The SAME p2
                            matched to a DIFFERENT p1 is NOT penalized (it may be
                            the true identity). DEFAULT-OFF (None/[] => no-op,
                            byte-identical report).
    """
    exclude_match_types = exclude_match_types or set()
    wii_dem = wii_dem or {}
    msvc_dem = msvc_dem or {}

    # known-negative (p2 -> {p1, ...}) index. A pair is a known-negative recur
    # only when BOTH endpoints match an entry (the wrong p1 for that p2).
    kn_by_p2: Dict[int, Set[int]] = defaultdict(set)
    kn_entries = known_negatives or []
    for e in kn_entries:
        kp2 = parse_addr(e.get("xenon_addr"))
        kp1 = parse_addr(e.get("wii_addr_bank8") or e.get("wii_addr"))
        if kp2 is not None and kp1 is not None:
            kn_by_p2[kp2].add(kp1)

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
    n_filtered_types = 0   # whole pair dropped: all types excluded / VT culled
    n_vt_culled = 0        # VT type stripped below --min-vt-score (pair survived)
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
        # --- optional offline filters (all no-ops at default args) ---------
        # The exported per-match scores (PLAN.md §3 schema) are OPTIONAL; the
        # field is absent in legacy matches.json, in which case min_vt_score
        # is inactive and the pair replays byte-identically.
        scores = m.get("scores") or {}
        if (min_vt_score is not None and "VTCombinedReference" in mtypes):
            vt = scores.get("VTCombinedReference") or {}
            prod = vt.get("product")
            if prod is not None and prod < min_vt_score:
                mtypes = [t for t in mtypes if t != "VTCombinedReference"]
                n_vt_culled += 1
        if exclude_match_types:
            mtypes = [t for t in mtypes if t not in exclude_match_types]
        if not mtypes:
            n_filtered_types += 1
            continue
        wii = wii_index.get(p1)
        pairs.append({
            "p1": p1, "p2": p2, "match_types": mtypes,
            "p1_name": m.get("p1_name"), "p2_name": m.get("p2_name"),
            "wii_symbol": wii["symbol"] if wii else None,
            "wii_tu": wii["tu"] if wii else None,
            "wii_category": wii["category"] if wii else None,
            "wii_size": wii["size"] if wii else None,
            "suspect_symbolshash": mtypes == ["SymbolsHash"],
        })

    by_p2: Dict[int, List[dict]] = defaultdict(list)
    for p in pairs:
        by_p2[p["p2"]].append(p)

    # ---- (a) holdout recovery --------------------------------------------
    # Two scoring modes per holdout entry:
    #   EXACT-ADDR  — when the entry carries `wii_addr_bank8` (round-2 judged
    #                 identities): correctness = the matched Wii p1 equals that
    #                 exact Bank-8 address. Stronger than the TU-stem heuristic
    #                 (a stem-correct match to a different function in the same
    #                 file would falsely count correct). Always scoreable.
    #   TU-STEM     — the legacy mode for the original 146 (Xenon addr + stem
    #                 only). Some holdout TU stems are Xenon-only (no Wii TU of
    #                 that name exists, e.g. 360-specific files): a matched pair
    #                 can never TU-agree there, so those entries are reported
    #                 but excluded from correct/wrong scoring.
    wii_stems = {tu_stem(v["tu"]) for v in wii_index.values() if v["tu"]}
    holdout_rows = []
    a_counts = Counter()
    judged = []  # (pair, correct?) feeding metric (d)
    for addr in sorted(eligible_holdout):
        entry = holdout_by_addr[addr]
        exact_wii = parse_addr(entry["wii_addr_bank8"]) \
            if entry.get("wii_addr_bank8") else None
        scoreable = (exact_wii is not None) or (entry["stem"] in wii_stems)
        cands = by_p2.get(addr, [])
        if not cands:
            result = "unmatched" if scoreable else "unmatched_unscoreable_stem"
            a_counts[result] += 1
            row = {"xenon_addr": hex(addr), "stem": entry["stem"], "result": result}
            if exact_wii is not None:   # round-2 entry: tag the scoring mode
                row["score_mode"] = "exact"
            holdout_rows.append(row)
            continue
        pair = cands[0]  # accepted matches are one-to-one per p2
        stem = tu_stem(pair["wii_tu"])
        if exact_wii is not None:
            # exact Bank-8 address agreement (round-2 judged holdout)
            if pair["p1"] == exact_wii:
                a_counts["recovered_correct"] += 1
                result = "recovered_correct"
                judged.append((pair, True))
            else:
                a_counts["recovered_wrong"] += 1
                result = "recovered_wrong"
                judged.append((pair, False))
        elif not scoreable:
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
        row = {
            "xenon_addr": hex(addr), "stem": entry["stem"], "result": result,
            "wii_addr": hex(pair["p1"]), "wii_symbol": pair["wii_symbol"],
            "wii_tu": pair["wii_tu"], "match_types": pair["match_types"],
        }
        if exact_wii is not None:   # round-2 entry: tag the scoring mode
            row["score_mode"] = "exact"
        holdout_rows.append(row)
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
                                              wii_dem, msvc_dem,
                                              credit_aliases=credit_aliases)
        # Oracle-trust: a BinDiff 'disagree' on a tiny stub-shaped Wii function
        # (<= stub_size_max bytes) is untrustworthy — BinDiff pairs identical
        # 'return Symbol(...)' shapes arbitrarily (forensics §1B). Bucket those
        # separately instead of scoring them as hard-wrong. DEFAULT OFF.
        low_trust = (low_trust_stub and verdict == "disagree"
                     and pair["wii_size"] is not None
                     and pair["wii_size"] <= stub_size_max)
        bucket = "low_trust_stub" if low_trust else verdict_bucket(verdict)
        b_all[bucket] += 1
        if high_conf:
            b_high[bucket] += 1
            if bucket in ("agree", "agree_alias", "disagree"):
                judged.append((pair, is_agree(verdict)))
        dc3_rows.append({
            "xenon_addr": hex(pair["p2"]), "wii_addr": hex(pair["p1"]),
            "wii_symbol": pair["wii_symbol"], "dc3_name": entry["dc3_name"],
            "bindiff_similarity": entry.get("similarity"),
            "bindiff_confidence": entry.get("confidence"),
            "high_conf": high_conf, "verdict": verdict, "detail": detail,
            "match_types": pair["match_types"],
        })

    def _agree_stats(c: Counter) -> dict:
        agree_total = c["agree"] + c["agree_alias"]
        judged_n = agree_total + c["disagree"]
        out = {
            "agree": agree_total,
            "disagree": c["disagree"],
            "unjudgeable": c["unjudgeable"],
            "agreement_rate": (agree_total / judged_n) if judged_n else None,
        }
        # Extra, audit-only sub-buckets — emitted only when the producing
        # feature is active so the default report is byte-identical to legacy.
        if credit_aliases:
            out["agree_exact"] = c["agree"]
            out["agree_platform_alias"] = c["agree_alias"]
        if low_trust_stub:
            out["low_trust_stub"] = c["low_trust_stub"]
        return out

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

    # ---- (b') known-negatives oracle ---------------------------------------
    # A scored match that recurs an EXACT human-judged-WRONG (p2,p1) pair is a
    # hard error: count it WRONG in per-type precision (via `judged`) and list
    # it. The SAME p2 matched to a DIFFERENT p1 is NOT penalized — that may be
    # the true identity the round-2 judge implied. Emitted only when the oracle
    # is supplied, so the default report stays byte-identical.
    kn_rows = []
    kn_recur = 0
    if kn_by_p2:
        for pair in pairs:
            wrong_p1s = kn_by_p2.get(pair["p2"])
            if wrong_p1s and pair["p1"] in wrong_p1s:
                kn_recur += 1
                judged.append((pair, False))  # hard-wrong in metric (d)
                kn_rows.append({
                    "xenon_addr": hex(pair["p2"]), "wii_addr": hex(pair["p1"]),
                    "wii_symbol": pair["wii_symbol"],
                    "match_types": pair["match_types"],
                })

    # ---- (d) per-match-type precision proxy --------------------------------
    def _tally_to_precision(tally: Dict[str, Counter]) -> dict:
        out = {}
        for t, c in sorted(tally.items(),
                           key=lambda kv: -(kv[1]["correct"] + kv[1]["wrong"])):
            n = c["correct"] + c["wrong"]
            out[t] = {
                "judged": n, "correct": c["correct"], "wrong": c["wrong"],
                "precision": (c["correct"] / n) if n else None,
            }
        return out

    d_tally: Dict[str, Counter] = defaultdict(Counter)
    # per-category stratification (scout 4 §4: VT ~0-20% band3 vs 40-54% engine)
    d_strat: Dict[str, Dict[str, Counter]] = defaultdict(lambda: defaultdict(Counter))
    for pair, correct in judged:
        k = "correct" if correct else "wrong"
        d_tally["OVERALL"][k] += 1
        cat = pair["wii_category"] or "unresolved"
        d_strat[cat]["OVERALL"][k] += 1
        for t in pair["match_types"]:
            d_tally[t][k] += 1
            d_strat[cat][t][k] += 1
    precision_by_type = _tally_to_precision(d_tally)

    totals = {
        "function_matches": len(function_matches),
        "seed_pairs_in_output": n_seed,
        "seed_conflicts": len(seed_conflicts),
        "bad_addresses": n_bad_addr,
        "scored_pairs": len(pairs),
        "wii_addr_resolved": sum(1 for p in pairs if p["wii_symbol"]),
    }
    # Filter bookkeeping — only surfaced when a filter is actually active, so
    # the default report stays byte-identical to legacy.
    if exclude_match_types:
        totals["filtered_excluded_match_types"] = n_filtered_types
    if min_vt_score is not None:
        totals["vt_below_min_score_culled"] = n_vt_culled

    rep = {
        "totals": totals,
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
    # Known-negatives report — only present when the oracle is supplied, so the
    # default/legacy report is byte-identical.
    if kn_by_p2:
        rep["known_negatives"] = {
            "oracle_pairs": sum(len(v) for v in kn_by_p2.values()),
            "recurred_exact": kn_recur,
            "note": "matches recurring an exact human-judged-WRONG (p2->p1) pair "
                    "(counted WRONG in precision_by_match_type); a different p1 "
                    "for the same p2 is NOT penalized",
        }
        rep["lists"]["known_negative_recurrences"] = kn_rows
    if stratify:
        rep["precision_by_match_type_by_category"] = {
            cat: _tally_to_precision(d_strat[cat]) for cat in sorted(d_strat)
        }
    return rep


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
        extra = ""
        if "agree_platform_alias" in s:
            extra = (f" [exact {s['agree_exact']} + alias "
                     f"{s['agree_platform_alias']}]")
        if "low_trust_stub" in s:
            extra += f" (low-trust-stub {s['low_trust_stub']})"
        print(f"  {k}: agree {s['agree']}{extra} / disagree {s['disagree']} "
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

    kn = rep.get("known_negatives")
    if kn:
        print("\n--- (b') known-negatives oracle ---")
        print(f"oracle pairs: {kn['oracle_pairs']}   exact recurrences "
              f"(counted WRONG): {kn['recurred_exact']}")

    strat = rep.get("precision_by_match_type_by_category")
    if strat:
        print("\n--- (e) precision by match type, stratified by wii_category ---")
        for cat in sorted(strat):
            print(f"  [{cat}]")
            for t_name, s in strat[cat].items():
                p = s["precision"]
                p_s = f"{p:.3f}" if p is not None else "n/a"
                print(f"    {t_name:<26} {s['judged']:>5} {s['correct']:>7} "
                      f"{s['wrong']:>5} {p_s:>9}")


def parse_sweep(spec: str) -> Optional[List[float]]:
    """'9.5,11,13' -> [9.5,11.0,13.0]; 'lo:hi:step' -> inclusive range; ''
    -> None (sweep mode off)."""
    spec = spec.strip()
    if not spec:
        return None
    if ":" in spec:
        lo, hi, step = (float(x) for x in spec.split(":"))
        vals, v = [], lo
        # accumulate to avoid float drift past hi
        n = 0
        while v <= hi + 1e-9:
            vals.append(round(v, 6))
            n += 1
            v = lo + n * step
        return vals
    return [float(x) for x in spec.split(",")]


def run_vt_sweep(eval_fn, thresholds: List[float]) -> None:
    """For each VT score threshold, run the eval with --min-vt-score set and
    print the VTCombinedReference judged precision + yield (matches kept).
    Consumes the §3 exported scores; if matches.json has no scores, every
    threshold is identical to the baseline (VT never culled) — which the
    printout makes obvious (constant 'kept')."""
    print(f"{'min_vt_score':>12}  {'VT judged':>9}  {'correct':>7}  "
          f"{'wrong':>5}  {'precision':>9}  {'VT culled':>9}")
    base = eval_fn(None)
    base_vt = base["precision_by_match_type"].get("VTCombinedReference", {})
    base_kept = base_vt.get("judged", 0)
    print(f"{'(none)':>12}  {base_kept:>9}  {base_vt.get('correct',0):>7}  "
          f"{base_vt.get('wrong',0):>5}  "
          f"{(base_vt.get('precision') or 0):>9.3f}  {0:>9}")
    for thr in thresholds:
        rep = eval_fn(thr)
        vt = rep["precision_by_match_type"].get("VTCombinedReference", {})
        culled = rep["totals"].get("vt_below_min_score_culled", 0)
        p = vt.get("precision")
        p_s = f"{p:.3f}" if p is not None else "n/a"
        print(f"{thr:>12.3f}  {vt.get('judged',0):>9}  {vt.get('correct',0):>7}  "
              f"{vt.get('wrong',0):>5}  {p_s:>9}  {culled:>9}")


def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--run-dir", type=Path, default=DEFAULT_RUN_DIR,
                    help="ghidriff output dir (json/*.matches.json found inside)")
    ap.add_argument("--matches", type=Path, default=None,
                    help="explicit path to the .matches.json (overrides --run-dir)")
    ap.add_argument("--seeds", type=Path, default=DEFAULT_SEEDS)
    ap.add_argument("--holdout", type=Path, default=DEFAULT_HOLDOUT)
    ap.add_argument("--known-negatives", type=Path, default=DEFAULT_KNOWN_NEGATIVES,
                    help="round-2 known-negatives oracle (judged-WRONG p2->p1 "
                         "pairs); a match recurring an exact pair counts WRONG. "
                         "Auto-loaded if present; --no-known-negatives disables.")
    ap.add_argument("--no-known-negatives", dest="known_negatives",
                    action="store_const", const=None,
                    help="disable the known-negatives oracle (byte-identical to "
                         "the legacy report)")
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
    # --- new offline-tunable features (all default-off => byte-identical) ---
    ap.add_argument("--credit-platform-alias", action="store_true",
                    help="credit Wii<->Xbox class renames + cross-compiler arity "
                         "drift as agreement (tagged 'agree (platform-alias)'); "
                         "raises measured VT precision (forensics §1D V1/V2)")
    ap.add_argument("--exclude-match-types", default="",
                    help="comma-separated match types to drop before scoring "
                         "(e.g. StringsRefsHasher,StrUniqueFuncRefsHasher); a "
                         "pair survives if it carries any non-excluded type")
    ap.add_argument("--stratify", action="store_true",
                    help="also emit precision_by_match_type_by_category "
                         "(band3/system/network/sdk) — the aggregate misleads")
    ap.add_argument("--low-trust-stub", action="store_true",
                    help="bucket BinDiff 'disagree' verdicts on Wii functions "
                         "<= --stub-size-max bytes as low-trust (not hard-wrong)")
    ap.add_argument("--stub-size-max", type=int, default=88,
                    help="Wii byte-size ceiling for the --low-trust-stub gate")
    ap.add_argument("--min-vt-score", type=float, default=None,
                    help="drop VTCombinedReference below this scores.product "
                         "floor (PLAN.md §3 schema; inactive if scores absent)")
    ap.add_argument("--sweep-vt-score", default="",
                    help="comma-separated VT score thresholds (or lo:hi:step) — "
                         "print VT precision/yield per threshold vs the judged "
                         "set and exit, instead of writing a report")
    args = ap.parse_args()

    exclude_types = {t.strip() for t in args.exclude_match_types.split(",") if t.strip()}

    matches_path = args.matches or find_matches_json(args.run_dir)
    print(f"[eval] matches: {matches_path}", file=sys.stderr)
    function_matches = load_function_matches(matches_path)

    seeds = json.loads(args.seeds.read_text())
    holdout = json.loads(args.holdout.read_text())
    holdout_entries = holdout.get("entries", holdout) if isinstance(holdout, dict) else holdout
    bindiff_entries = json.loads(args.bindiff.read_text())
    wii_index = parse_wii_map_index(args.wii_map)

    # Known-negatives oracle (round 2): auto-loaded if the file exists, unless
    # --no-known-negatives. Absent file => None => byte-identical legacy report.
    known_negatives: Optional[List[dict]] = None
    if args.known_negatives is not None and args.known_negatives.is_file():
        kn_doc = json.loads(args.known_negatives.read_text())
        known_negatives = (kn_doc.get("entries", kn_doc)
                           if isinstance(kn_doc, dict) else kn_doc)
    print(f"[eval] wii map: {len(wii_index)} function addrs; seeds: {len(seeds)}; "
          f"holdout: {len(holdout_entries)}; bindiff: {len(bindiff_entries)}; "
          f"known-negatives: {len(known_negatives) if known_negatives else 0}",
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

    def _eval(min_vt: Optional[float]) -> dict:
        return evaluate(
            function_matches, seeds, holdout_entries, bindiff_entries,
            wii_index, wii_dem, msvc_dem,
            bindiff_min_sim=args.bindiff_min_sim,
            bindiff_min_conf=args.bindiff_min_conf,
            credit_aliases=args.credit_platform_alias,
            exclude_match_types=exclude_types,
            min_vt_score=min_vt,
            low_trust_stub=args.low_trust_stub,
            stub_size_max=args.stub_size_max,
            stratify=args.stratify,
            known_negatives=known_negatives)

    # ---- VT score-sweep mode (no report written) -------------------------
    sweep = parse_sweep(args.sweep_vt_score)
    if sweep is not None:
        run_vt_sweep(_eval, sweep)
        return 0

    rep = _eval(args.min_vt_score)
    rep["inputs"] = {
        "matches": str(matches_path), "seeds": str(args.seeds),
        "holdout": str(args.holdout), "bindiff": str(args.bindiff),
        "wii_map": str(args.wii_map),
    }
    # only surface the oracle path when it actually contributed (byte-identical
    # to legacy otherwise)
    if known_negatives:
        rep["inputs"]["known_negatives"] = str(args.known_negatives)

    print_summary(rep)

    out = args.out or ((args.run_dir if args.run_dir.is_dir() else matches_path.parent)
                       / "eval_report.json")
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(rep, indent=1) + "\n")
    print(f"\n[eval] report written: {out}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
