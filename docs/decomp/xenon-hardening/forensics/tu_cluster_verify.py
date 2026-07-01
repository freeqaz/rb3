#!/usr/bin/env python3
"""Verify VT game-code matches using TU (translation unit) cluster coherence.

Reads eval_report.json new_coverage list and applies TU clustering heuristic:
- Group VT matches by Wii TU
- Accept cluster if Xenon_spread/Wii_spread < 10 AND Xenon_spread < 50KB
- Also cross-checks against rb3-xenon/unified_id_rb3wii.json

Usage:
    python3 forensics/tu_cluster_verify.py
    python3 forensics/tu_cluster_verify.py --category band3 --match-type VTCombinedReference
"""
from __future__ import annotations
import argparse
import json
from collections import defaultdict
from pathlib import Path

RB3 = Path(__file__).resolve().parents[3]  # rb3/
XENON = RB3.parent / "rb3-xenon"

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--eval-report",
        default=str(RB3/"build/SZBE69_B8/ghidra/ghidriff-xenon/eval_report.json"))
    ap.add_argument("--rb3wii",
        default=str(XENON/"unified_id_rb3wii.json"))
    ap.add_argument("--category", default="band3",
        help="wii_category to filter (default: band3)")
    ap.add_argument("--match-type", default="VTCombinedReference",
        help="match type to analyze (default: VTCombinedReference)")
    ap.add_argument("--max-spread-ratio", type=float, default=10.0,
        help="Xenon/Wii spread ratio threshold for cluster acceptance")
    ap.add_argument("--max-spread-bytes", type=int, default=50000,
        help="Maximum absolute Xenon spread for cluster acceptance")
    args = ap.parse_args()

    with open(args.eval_report) as f:
        data = json.load(f)
    new_cov = data["lists"]["new_coverage"]

    rb3wii = json.loads(Path(args.rb3wii).read_text())
    rb3wii_map = {e["rb3_addr"].lower(): e for e in rb3wii}

    # Filter to requested category + match type
    items = [item for item in new_cov
             if item.get("wii_category") == args.category
             and args.match_type in item.get("match_types", [])]
    print(f"Items ({args.category}/{args.match_type}): {len(items)}")

    # Group by TU
    by_tu: dict = defaultdict(list)
    for item in items:
        by_tu[item.get("wii_tu", "unknown")].append(item)

    accepted = []
    rejected = []
    single = []

    for tu, grp in sorted(by_tu.items()):
        if len(grp) == 1:
            single.extend(grp)
            continue
        xaddrs = sorted(int(it["xenon_addr"], 16) for it in grp)
        waddrs = sorted(int(it.get("wii_addr", "0"), 16) for it in grp)
        x_spread = xaddrs[-1] - xaddrs[0]
        w_spread = waddrs[-1] - waddrs[0]
        ratio = x_spread / max(w_spread, 1)
        if ratio < args.max_spread_ratio and x_spread < args.max_spread_bytes:
            for it in grp:
                it["_cluster_ok"] = True
                it["_x_spread"] = x_spread
                it["_w_spread"] = w_spread
                it["_ratio"] = ratio
            accepted.extend(grp)
        else:
            rejected.extend(grp)

    print(f"Accepted (coherent clusters): {len(accepted)}")
    print(f"Rejected (scattered clusters): {len(rejected)}")
    print(f"Single (no cluster): {len(single)}")

    # rb3wii cross-check
    all_items = accepted + rejected + single
    in_rb3wii = [it for it in all_items if it["xenon_addr"].lower() in rb3wii_map]
    agree = sum(1 for it in in_rb3wii
                if rb3wii_map[it["xenon_addr"].lower()].get("wii_addr", "").lower()
                   == it.get("wii_addr", "").lower())
    print(f"In rb3wii: {len(in_rb3wii)}, agree: {agree}, disagree: {len(in_rb3wii)-agree}")

    # Print accepted clusters
    print()
    print("=== Accepted clusters ===")
    by_tu_acc = defaultdict(list)
    for it in accepted:
        by_tu_acc[it.get("wii_tu", "?")].append(it)
    for tu, grp in sorted(by_tu_acc.items(), key=lambda x: -len(x[1])):
        x_spread = grp[0].get("_x_spread", 0)
        w_spread = grp[0].get("_w_spread", 0)
        print(f"  {tu}: {len(grp)} items, x_spread={x_spread}B, w_spread={w_spread}B")
        for it in grp:
            rw = rb3wii_map.get(it["xenon_addr"].lower(), {})
            rw_agree = rw.get("wii_addr","").lower() == it.get("wii_addr","").lower()
            rw_str = f" rb3wii={rw.get('wii_addr','?')[:12]} sim={rw.get('similarity','?'):.2f} {'AGREE' if rw_agree else 'DIFF'}" if rw else " rb3wii=n/a"
            print(f"    xenon={it['xenon_addr']} wii={it.get('wii_addr','?')} sym={it.get('wii_symbol','?')[:60]}{rw_str}")

if __name__ == "__main__":
    main()
