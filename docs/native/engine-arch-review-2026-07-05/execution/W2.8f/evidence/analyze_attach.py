#!/usr/bin/env python3
"""Parse HANDS_ATTACH probe blocks -> per-mesh Tier1/Tier2 stats + co-variation."""
import re, sys, statistics as st
from collections import defaultdict

path = sys.argv[1] if len(sys.argv) > 1 else "/tmp/w28f/attach.log"
txt = open(path, errors="replace").read().splitlines()

rows = defaultdict(list)  # mesh -> list of dict
i = 0
hdr = re.compile(r"\[HANDS_ATTACH\] mesh='([^']*)' owner='([^']*)' frame=(\d+) nb=(\d+) wext=([\d.]+) rebound=(\d+)")
t1 = re.compile(r"TIER1 rest-coherence: worst=([\d.]+)deg bone\[(-?\d+)\]='([^']*)' count\(>5deg\)=(\d+) freshRecap=(\d+)\s+xcheck\(invOff-vs-restW\) worst=([\d.]+)deg bone\[(-?\d+)\]")
t2 = re.compile(r"TIER2 joint-attach\(PRIMARY\): worst=([\d.]+)u bone\[(-?\d+)\]='([^']*)' parent\[(-?\d+)\] R=([\d.]+) exactJoint=([\d.]+)u pairs=(\d+)\s+\(palette-wide exactWorst=([\d.]+)u\)")

while i < len(txt):
    m = hdr.search(txt[i])
    if m and i+2 < len(txt):
        a = t1.search(txt[i+1]); b = t2.search(txt[i+2])
        if a and b:
            rows[m.group(1)].append(dict(
                frame=int(m.group(3)), nb=int(m.group(4)), wext=float(m.group(5)), rebound=int(m.group(6)),
                t1worst=float(a.group(1)), t1cnt=int(a.group(4)), fresh=int(a.group(5)), t1x=float(a.group(6)),
                t2worst=float(b.group(1)), t2R=float(b.group(5)), t2exact=float(b.group(6)), pairs=int(b.group(7)),
                t2palexact=float(b.group(8))))
            i += 3; continue
    i += 1

def stat(v): return f"min={min(v):.2f} med={st.median(v):.2f} max={max(v):.2f} n={len(v)}"

print(f"{'mesh':<30} {'nb':>3} {'reb':>3} | Tier1 worst(deg)          Tier1xcheck(deg)        | Tier2 worst(u)            Tier2 exactJoint(u)")
for mesh in sorted(rows):
    r = rows[mesh]
    print(f"{mesh:<30} {r[0]['nb']:>3} {r[0]['rebound']:>3} | "
          f"{stat([x['t1worst'] for x in r]):<24} {stat([x['t1x'] for x in r]):<22} | "
          f"{stat([x['t2worst'] for x in r]):<24} {stat([x['t2exact'] for x in r])}")

# Co-variation Tier2 vs wext (Pearson) on hands_naked
print("\n=== A7 co-variation: Tier2 worst vs wext ===")
for mesh in sorted(rows):
    r = rows[mesh]
    w = [x['wext'] for x in r]; t = [x['t2worst'] for x in r]; te = [x['t2exact'] for x in r]
    if len(set(w)) > 2 and len(r) > 5:
        try:
            pr = st.correlation(w, t); pre = st.correlation(w, te)
        except Exception:
            pr = pre = float('nan')
        wr = max(w)-min(w)
        print(f"{mesh:<30} wext[{min(w):.1f}..{max(w):.1f}] Δ={wr:.1f}  corr(wext,Tier2)={pr:+.2f}  corr(wext,exact)={pre:+.2f}")

# co-sample table for hands_naked: show wext vs t2 over time (sampled)
hn = rows.get("hands_naked.mesh", [])
if hn:
    print("\n=== hands_naked.mesh co-sample (every ~8th block) ===")
    print(f"{'frame':>7} {'wext':>7} {'t2worst':>8} {'t2exact':>8} {'t1worst':>8} {'t1cnt':>5}")
    for x in hn[::max(1,len(hn)//24)]:
        print(f"{x['frame']:>7} {x['wext']:>7.1f} {x['t2worst']:>8.2f} {x['t2exact']:>8.2f} {x['t1worst']:>8.1f} {x['t1cnt']:>5}")
