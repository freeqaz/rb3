#!/usr/bin/env python3
"""build_accept_seeds.py — ACCEPT-only seed set for the two-pass VT-rescue run.

ROUND 2 (2026-06-11). The run-3 cascade was seeded from 1,213 high-confidence
pairs, but VTCombinedReference collapsed to 0.109 raw / 0.236 alias-credited
precision because VT (and the whole cascade) propagated from the full
7,426-match seeded graph contaminated with ~0.5-precision sub-threshold BSim
matches. Hypothesis: seeding the cascade from ONLY the vetted ACCEPT tier
(~0.93 precision) rescues VT precision.

This script builds that ACCEPT-only seed set from the ARCHIVED run-3 vetted
identities, with three anti-leak / data-hygiene filters:

  1. ANTI-LEAK: drop any ACCEPT entry whose xenon_addr is in the original
     holdout (xenon-seeds/holdout.json `entries[].addr`). The holdout MUST
     stay excluded from seeds so recall/precision remain readable and
     leak-free. The drop count is DERIVED from the LIVE holdout (no magic
     number) — it is exactly the number of ACCEPT xenon addrs present in
     holdout.json. (currently 85 — 54 BSIM / 26 ExactInstr / 3 Implied / 2
     SwitchSig; this auto-tracks holdout.json growth, e.g. 146->158 in round 2.)

  2. KNOWN-NEGATIVES: drop any ACCEPT entry whose EXACT (xenon_addr, wii_addr)
     pair appears in xenon-seeds/known_negatives.json — round-judged-wrong
     identities that must never seed a run. Matched by EXACT pair (NOT
     xenon-addr-only): a same-xenon / different-wii match may still be the true
     identity and must not be penalized. (currently 3 pairs.)

  3. STRICT 1:1: ghidriff seeds are pre-accepted match pairs; a duplicated p1
     (Wii) or p2 (Xenon) address makes the seed graph ambiguous. Drop ALL pairs
     sharing a duplicated p1 OR p2 address. (expected: exactly 1 dup p1 and 1
     dup p2 → 4 pairs dropped)

Output schema MATCHES xenon-seeds/seeds.json: a flat list of
  {"p1_addr": "0x80......" (Wii Bank8), "p2_addr": "0x82......" (Xenon)}
addresses lowercase-0x. A stats sidecar (.stats.json) records the counts.

Address-space invariant (round-1 ground truth):
  vetted entry  wii_addr (0x80...) -> seed p1_addr  (Wii Bank 8)
  vetted entry  xenon_addr (0x82...) -> seed p2_addr  (Xenon)
This is the SAME orientation as the canonical seeds.json (verified: seeds[0]
p1=0x8000fb10/p2=0x82260018 == vetted[0] wii=0x8000fb10/xenon=0x82260018).

Usage:
  build/SZBE69_B8/ghidra/ghidriff-venv/bin/python tools/ghidra/build_accept_seeds.py
"""

import json
import sys
from collections import Counter
from pathlib import Path

RB3_ROOT = Path(__file__).resolve().parents[2]

VETTED = RB3_ROOT / "build/SZBE69_B8/ghidra/ghidriff-xenon/run3-archive/vetted_identities.json"
HOLDOUT = RB3_ROOT / "build/SZBE69_B8/ghidra/xenon-seeds/holdout.json"
KNOWN_NEG = RB3_ROOT / "build/SZBE69_B8/ghidra/xenon-seeds/known_negatives.json"
OUT = RB3_ROOT / "build/SZBE69_B8/ghidra/xenon-seeds/seeds_accept_run3.json"
OUT_STATS = RB3_ROOT / "build/SZBE69_B8/ghidra/xenon-seeds/seeds_accept_run3.stats.json"

# Hard expectations from the planner's ground-truth check (PLAN.md §"Numbers …").
# NOTE: the holdout-drop count is intentionally NOT hardcoded — it is DERIVED
# from the live holdout.json so it auto-tracks holdout growth (see filter 1).
EXPECT_ACCEPT = 2207


def _norm(addr) -> int:
    """Normalize a hex address string (0x optional, any case) to an int."""
    return int(str(addr), 16)


def _fmt(addr_int: int) -> str:
    """Render an int address as lowercase 0x hex (seeds.json convention)."""
    return f"0x{addr_int:08x}"


def main() -> int:
    if not VETTED.exists():
        print(f"ERROR: vetted identities not found: {VETTED}", file=sys.stderr)
        return 1
    if not HOLDOUT.exists():
        print(f"ERROR: holdout not found: {HOLDOUT}", file=sys.stderr)
        return 1

    vetted = json.loads(VETTED.read_text())
    entries = vetted["entries"]
    holdout = json.loads(HOLDOUT.read_text())["entries"]

    # ---- ACCEPT tier ----
    accept = [e for e in entries if e.get("tier") == "ACCEPT"]
    print(f"ACCEPT entries: {len(accept)}")
    assert len(accept) == EXPECT_ACCEPT, \
        f"expected {EXPECT_ACCEPT} ACCEPT, got {len(accept)}"

    null_wii = [e for e in accept if e.get("wii_addr") is None]
    assert not null_wii, f"{len(null_wii)} ACCEPT entries have null wii_addr (expected 0)"

    # ---- anti-leak: drop ACCEPT entries whose xenon_addr is in holdout ----
    holdout_xenon = {_norm(h["addr"]) for h in holdout}
    print(f"holdout xenon addrs: {len(holdout_xenon)}")

    drops = [e for e in accept if _norm(e["xenon_addr"]) in holdout_xenon]
    print(f"ACCEPT in holdout (dropped, anti-leak): {len(drops)} (derived from live holdout)")
    drop_types = Counter(e["match_types"][0] for e in drops)
    print(f"  drop match_types[0]: {dict(drop_types)}")
    # Soft self-consistency check (no magic constant): the drop count must equal
    # the number of ACCEPT xenon addrs that are present in the live holdout.
    assert len(drops) == sum(1 for e in accept if _norm(e["xenon_addr"]) in holdout_xenon), \
        "holdout-drop count is not self-consistent with the live holdout"

    kept = [e for e in accept if _norm(e["xenon_addr"]) not in holdout_xenon]
    print(f"kept after holdout drop: {len(kept)}")

    # ---- known-negatives: drop ACCEPT entries whose EXACT (xenon,wii) pair is
    #      a round-judged-wrong identity. Match by EXACT pair, NOT xenon-only:
    #      a same-xenon / different-wii match may still be the true identity. ----
    kn_pairs = set()
    if KNOWN_NEG.exists():
        kn = json.loads(KNOWN_NEG.read_text())["entries"]
        kn_pairs = {(_norm(k["xenon_addr"]), _norm(k["wii_addr_bank8"])) for k in kn}
    else:
        print(f"WARNING: known_negatives not found: {KNOWN_NEG} (no known-neg filter)")
    kn_dropped = [e for e in kept
                  if (_norm(e["xenon_addr"]), _norm(e["wii_addr"])) in kn_pairs]
    print(f"known-negative pairs dropped: {len(kn_dropped)}")
    for e in kn_dropped:
        print(f"  KN-DROP {e['wii_addr']} -> {e['xenon_addr']} "
              f"{e['match_types']} {e.get('wii_symbol', '')[:48]}")
    kept = [e for e in kept
            if (_norm(e["xenon_addr"]), _norm(e["wii_addr"])) not in kn_pairs]
    print(f"kept after known-negative drop: {len(kept)}")

    # ---- strict 1:1: drop ALL pairs that share a duplicated p1 or p2 ----
    p1_counts = Counter(_norm(e["wii_addr"]) for e in kept)
    p2_counts = Counter(_norm(e["xenon_addr"]) for e in kept)
    dup_p1 = {a for a, c in p1_counts.items() if c > 1}
    dup_p2 = {a for a, c in p2_counts.items() if c > 1}
    print(f"duplicated p1 (Wii) addrs: {[_fmt(a) for a in sorted(dup_p1)]}")
    print(f"duplicated p2 (Xenon) addrs: {[_fmt(a) for a in sorted(dup_p2)]}")

    removed = [e for e in kept
               if _norm(e["wii_addr"]) in dup_p1 or _norm(e["xenon_addr"]) in dup_p2]
    print(f"pairs removed by strict-1:1 dedup: {len(removed)}")
    for e in removed:
        print(f"  DEDUP-DROP {e['wii_addr']} -> {e['xenon_addr']} "
              f"{e['match_types']} {e.get('wii_symbol', '')[:48]}")

    final = [e for e in kept
             if _norm(e["wii_addr"]) not in dup_p1 and _norm(e["xenon_addr"]) not in dup_p2]
    print(f"FINAL seed pairs: {len(final)}")

    # ---- emit seeds (sorted by p1 for determinism) ----
    seeds = sorted(
        ({"p1_addr": _fmt(_norm(e["wii_addr"])),
          "p2_addr": _fmt(_norm(e["xenon_addr"]))} for e in final),
        key=lambda s: (s["p1_addr"], s["p2_addr"]),
    )

    # ---- in-script assertions: zero seeds ∩ holdout, strict 1:1 ----
    seed_p2 = {_norm(s["p2_addr"]) for s in seeds}
    inter = seed_p2 & holdout_xenon
    assert not inter, f"LEAK: {len(inter)} seed p2 addrs are in holdout: {[_fmt(a) for a in inter]}"
    print(f"ASSERT zero seeds∩holdout: PASS ({len(seed_p2)} seed p2 addrs, 0 in holdout)")

    seed_pairs = {(_norm(s["p2_addr"]), _norm(s["p1_addr"])) for s in seeds}
    kn_inter = seed_pairs & kn_pairs
    assert not kn_inter, \
        f"LEAK: {len(kn_inter)} known-negative pair(s) in seeds: {[(_fmt(x), _fmt(w)) for x, w in kn_inter]}"
    print(f"ASSERT zero seeds∩known-negatives: PASS ({len(kn_pairs)} known-negative pairs, 0 in seeds)")

    seed_p1_counts = Counter(_norm(s["p1_addr"]) for s in seeds)
    seed_p2_counts = Counter(_norm(s["p2_addr"]) for s in seeds)
    assert all(c == 1 for c in seed_p1_counts.values()), "p1 not 1:1"
    assert all(c == 1 for c in seed_p2_counts.values()), "p2 not 1:1"
    print(f"ASSERT strict 1:1 (unique p1 and p2): PASS")

    OUT.write_text(json.dumps(seeds, indent=2) + "\n")
    print(f"Wrote {len(seeds)} seeds -> {OUT}")

    stats = {
        "source_vetted": str(VETTED.relative_to(RB3_ROOT)),
        "source_holdout": str(HOLDOUT.relative_to(RB3_ROOT)),
        "source_known_negatives": str(KNOWN_NEG.relative_to(RB3_ROOT)),
        "accept_total": len(accept),
        "holdout_addrs": len(holdout_xenon),
        "holdout_drops": len(drops),
        "holdout_drop_match_types": dict(drop_types),
        "known_negatives_drops": len(kn_dropped),
        "known_negatives_pairs": [
            {"p1_addr": e["wii_addr"], "p2_addr": e["xenon_addr"],
             "match_types": e["match_types"], "wii_symbol": e.get("wii_symbol")}
            for e in kn_dropped
        ],
        "kept_after_holdout_drop": len(kept),
        "dup_p1_addrs": [_fmt(a) for a in sorted(dup_p1)],
        "dup_p2_addrs": [_fmt(a) for a in sorted(dup_p2)],
        "strict_1to1_removed": len(removed),
        "strict_1to1_removed_pairs": [
            {"p1_addr": e["wii_addr"], "p2_addr": e["xenon_addr"],
             "match_types": e["match_types"], "wii_symbol": e.get("wii_symbol")}
            for e in removed
        ],
        "final_seed_count": len(seeds),
        "seeds_holdout_intersection": 0,
    }
    OUT_STATS.write_text(json.dumps(stats, indent=2) + "\n")
    print(f"Wrote stats -> {OUT_STATS}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
