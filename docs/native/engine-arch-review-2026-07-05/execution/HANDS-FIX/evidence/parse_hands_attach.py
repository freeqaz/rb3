#!/usr/bin/env python3
"""Parse [HANDS_ATTACH] blocks from an rb3-native engine log into a gender-split
(nb=38 male / nb=40 female) per-mesh summary: Tier-1 count(>5deg) + worst,
Tier-2 joint-attach worst (R-radius) + exactWorst, wext distribution.

Usage: parse_hands_attach.py <engine.log> [mesh_substr]
Gates (HANDS-ADJUDICATION VERDICT §5): Tier-1 count(>5deg)==0 male AND female;
Tier-2 EXACT <=1u; wext DESCRIPTIVE only.
"""
import sys, re, statistics as st

log = sys.argv[1]
mesh_filter = sys.argv[2] if len(sys.argv) > 2 else "hands_naked"

hdr = re.compile(r"\[HANDS_ATTACH\] mesh='([^']*)' owner='[^']*' frame=\d+ nb=(\d+) wext=([\d.]+) rebound=(\d)")
t1 = re.compile(r"TIER1 .*worst=([\d.]+)deg .*count\(>5deg\)=(\d+) freshRecap=(\d+)")
t2 = re.compile(r"TIER2 .*worst=([\d.]+)u .*exactJoint=([\d.]+)u pairs=\d+  \(palette-wide exactWorst=([\d.]+)u\)")

# rows: mesh -> nb -> list of dicts
rows = {}
lines = open(log, errors="replace").read().splitlines()
i = 0
while i < len(lines):
    m = hdr.search(lines[i])
    if m and i+2 < len(lines):
        mesh, nb, wext, reb = m.group(1), int(m.group(2)), float(m.group(3)), int(m.group(4))
        a = t1.search(lines[i+1]); b = t2.search(lines[i+2])
        if a and b:
            rows.setdefault(mesh, {}).setdefault(nb, []).append(dict(
                wext=wext, reb=reb,
                t1w=float(a.group(1)), t1c=int(a.group(2)), recap=int(a.group(3)),
                t2w=float(b.group(1)), t2ej=float(b.group(2)), t2ew=float(b.group(3))))
        i += 3; continue
    i += 1

def summ(rs):
    n = len(rs)
    we = [r['wext'] for r in rs]
    return (f"blocks={n:5d} | Tier1 worst {min(r['t1w'] for r in rs):5.1f}-{max(r['t1w'] for r in rs):5.1f} "
            f"count(>5)max={max(r['t1c'] for r in rs):2d} count(>5)min={min(r['t1c'] for r in rs):2d} "
            f"count(>5)==0 in {sum(1 for r in rs if r['t1c']==0)}/{n} "
            f"| Tier2 R-radius worst {max(r['t2w'] for r in rs):6.2f}u EXACTworst {max(r['t2ew'] for r in rs):5.2f}u "
            f"| wext {min(we):.1f}-{max(we):.1f} mean {st.mean(we):.1f} distinct {len(set(we))} "
            f"| rebound {sum(1 for r in rs if r['reb'])}/{n}")

for mesh in sorted(rows):
    if mesh_filter and mesh_filter not in mesh:
        continue
    print(f"\n=== {mesh} ===")
    for nb in sorted(rows[mesh]):
        g = "male  " if nb == 38 else ("female" if nb == 40 else f"nb{nb} ")
        print(f"  {g}(nb={nb}) {summ(rows[mesh][nb])}")

# global gate verdict for hands_naked
print("\n--- GATE (hands_naked, Tier-1 count(>5deg)==0 male AND female) ---")
hn = None
for mesh in rows:
    if "hands_naked" in mesh:
        hn = rows[mesh]; break
if hn:
    for nb in sorted(hn):
        g = "male" if nb == 38 else ("female" if nb == 40 else f"nb{nb}")
        rs = hn[nb]
        allzero = all(r['t1c'] == 0 for r in rs)
        print(f"  {g}: Tier-1 count(>5)==0 on ALL {len(rs)} blocks: {'PASS' if allzero else 'FAIL'} "
              f"(max count={max(r['t1c'] for r in rs)}, max EXACTworst={max(r['t2ew'] for r in rs):.2f}u)")
