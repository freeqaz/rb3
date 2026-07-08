#!/usr/bin/env python3
"""parse_hands_attach.py — Lane DISCRIM (Wave 21) HANDS_ATTACH block parser.

Parses the engine RB3_HANDS_ATTACH_PROBE 3-line blocks (engine e69a35f) from a
gameplay log and reports the DRAW-frame Tier-1 (own-vs-B basis) + Tier-2 (inter-bone
joint-attach) quantities, GENDER-SPLIT by owner bone count (nb=38 male / 40 female),
per member (owner pointer is implicit; we split by nb + owner name).

A block:
  [HANDS_ATTACH] mesh='..' owner='..' frame=N nb=M wext=W rebound=R
    TIER1 rest-coherence: worst=Xdeg bone[i]='..' count(>5deg)=C freshRecap=F  xcheck(..) worst=Ydeg bone[j]
    TIER2 joint-attach(PRIMARY): worst=Au bone[i]='..' parent[p] R=Rr exactJoint=Eu pairs=P  (palette-wide exactWorst=Zu)

Tier-1 worst/count = per-bone off_b*restW vs I: the own basis vs authored bind B.
  ~0  => own == B (coherent basis).  87deg-mode => seed-R rebake shipped default.
Tier-2 worst (jx = -off.v) vs exactJoint (inverse(off).v): the inter-bone tear on the
  uploaded palette. exactJoint ~0 => joints attach coherently (rest-free) => NO tear.
  A large jx-worst with exactJoint~0 is the seed-R conjugation (rigid), NOT a blend tear.
"""
import argparse, re, sys
from statistics import median

BLK = re.compile(r"\[HANDS_ATTACH\] mesh='([^']*)' owner='([^']*)' frame=(\d+) nb=(\d+) wext=([\d.]+) rebound=(\d+)")
T1 = re.compile(r"TIER1 rest-coherence: worst=([\d.]+)deg bone\[(\d+)\]='([^']*)' count\(>5deg\)=(\d+) freshRecap=(\d+)\s+xcheck\(invOff-vs-restW\) worst=([\d.]+)deg")
T2 = re.compile(r"TIER2 joint-attach\(PRIMARY\): worst=([\d.]+)u bone\[(\d+)\]='([^']*)' parent\[(\d+)\] R=([\d.]+) exactJoint=([\d.]+)u pairs=(\d+)\s+\(palette-wide exactWorst=([\d.]+)u\)")

def parse(path):
    lines = open(path, errors="replace").read().splitlines()
    recs = []
    i = 0
    while i < len(lines):
        m = BLK.search(lines[i])
        if not m:
            i += 1; continue
        rec = dict(mesh=m.group(1), owner=m.group(2), frame=int(m.group(3)),
                   nb=int(m.group(4)), wext=float(m.group(5)), rebound=int(m.group(6)))
        # next two lines
        for j in (i+1, i+2):
            if j < len(lines):
                t1 = T1.search(lines[j])
                if t1:
                    rec.update(t1_worst=float(t1.group(1)), t1_bone=t1.group(3),
                               t1_count=int(t1.group(4)), t1_recap=int(t1.group(5)),
                               t1_xcheck=float(t1.group(6)))
                t2 = T2.search(lines[j])
                if t2:
                    rec.update(t2_worst=float(t2.group(1)), t2_bone=t2.group(3),
                               t2_R=float(t2.group(5)), t2_exact=float(t2.group(6)),
                               t2_pairs=int(t2.group(7)), t2_exactworst=float(t2.group(8)))
        if "t1_worst" in rec and "t2_worst" in rec:
            recs.append(rec)
        i += 3
    return recs

def summ(vals):
    if not vals: return "n=0"
    vals = sorted(vals)
    return f"n={len(vals)} min={vals[0]:.1f} med={median(vals):.1f} max={vals[-1]:.1f}"

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("log")
    ap.add_argument("--mesh", default="hands_naked", help="mesh name substring filter")
    a = ap.parse_args()
    recs = [r for r in parse(a.log) if a.mesh in r["mesh"]]
    if not recs:
        print(f"NO {a.mesh} HANDS_ATTACH blocks in {a.log}"); return 1
    # gender split by nb
    for label, pred in (("MALE(nb=38)", lambda r: r["nb"] == 38),
                        ("FEMALE(nb=40)", lambda r: r["nb"] == 40),
                        ("OTHER", lambda r: r["nb"] not in (38, 40))):
        g = [r for r in recs if pred(r)]
        if not g: continue
        print(f"\n=== {label}  blocks={len(g)} meshes={sorted(set(r['mesh'] for r in g))} ===")
        print(f"  TIER1 own-vs-B worst(deg):   {summ([r['t1_worst'] for r in g])}")
        print(f"  TIER1 count(>5deg):          {summ([float(r['t1_count']) for r in g])}")
        print(f"  TIER1 xcheck(invOff-restW):  {summ([r['t1_xcheck'] for r in g])}")
        print(f"  TIER2 joint-attach worst(u): {summ([r['t2_worst'] for r in g])}   bone(mode)={_mode([r['t2_bone'] for r in g])}")
        print(f"  TIER2 EXACT-joint(u):        {summ([r['t2_exact'] for r in g])}")
        print(f"  TIER2 palette exactWorst(u): {summ([r['t2_exactworst'] for r in g])}")
        print(f"  wext(descriptive only):      {summ([r['wext'] for r in g])}")
        print(f"  rebound:                     {sorted(set(r['rebound'] for r in g))}")
    return 0

def _mode(xs):
    from collections import Counter
    if not xs: return "-"
    return Counter(xs).most_common(1)[0][0]

if __name__ == "__main__":
    sys.exit(main())
