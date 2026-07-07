#!/usr/bin/env python3
"""analyze_disc.py — summarize the WHITE-fix discriminator batch(es).

Reads one or more measure/<tag>.json produced by white_discriminate.py and prints,
per arm: N, WHITE count, wash count, engaged fraction, and the distribution of
mean_luma / hi_frac / per-tonal-band saturation. Then prints the PP_ON vs PP_OFF
discriminator comparison (paired arm names differing only by the _ppoff suffix).
"""
import json, sys, statistics as st, collections

def load(paths):
    rows = []
    for p in paths:
        rows += json.load(open(p))
    return rows

def stat(xs):
    xs = [x for x in xs if x is not None]
    if not xs: return "n/a"
    return f"mean={st.mean(xs):.3f} med={st.median(xs):.3f} min={min(xs):.3f} max={max(xs):.3f}"

def main():
    rows = load(sys.argv[1:])
    by = collections.defaultdict(list)
    for r in rows:
        by[r["arm"]].append(r)
    print("=== per-arm ===")
    for arm in sorted(by):
        rs = by[arm]
        nW = sum(1 for r in rs if r["class"] == "WHITE")
        nwash = sum(1 for r in rs if r.get("is_wash"))
        eng = sum(1 for r in rs if (r.get("tail_engaged") or 0) > (r.get("tail_missed") or 0))
        print(f"\n[{arm}] N={len(rs)} WHITE={nW} wash={nwash} engaged_boots={eng}/{len(rs)}")
        print(f"   mean_luma: {stat([r['mean_luma'] for r in rs])}")
        print(f"   hi_frac  : {stat([r['hi_frac'] for r in rs])}")
        print(f"   mid_sat  : {stat([r['mid_sat'] for r in rs])}")
        print(f"   low_sat  : {stat([r['low_sat'] for r in rs])}")
        print(f"   classes  : {[r['class'] for r in rs]}")
        print(f"   envs     : {collections.Counter(r.get('final_env') for r in rs)}")

    # PP_ON vs PP_OFF pairs
    print("\n=== discriminator (PP_ON vs PP_OFF) ===")
    for arm in sorted(by):
        if arm.endswith("_ppoff"):
            continue
        pp = arm + "_ppoff"
        if pp not in by:
            continue
        on, off = by[arm], by[pp]
        onml = [r["mean_luma"] for r in on]; offml = [r["mean_luma"] for r in off]
        onhi = [r["hi_frac"] for r in on]; offhi = [r["hi_frac"] for r in off]
        print(f"\n{arm}: PP_ON vs PP_OFF")
        print(f"   mean_luma  ON {st.mean(onml):.3f} (max {max(onml):.3f})  vs  OFF {st.mean(offml):.3f} (max {max(offml):.3f})")
        print(f"   hi_frac    ON {st.mean(onhi):.2f} (max {max(onhi):.2f})  vs  OFF {st.mean(offhi):.2f} (max {max(offhi):.2f})")
        onW = sum(1 for r in on if r['class']=='WHITE'); offW = sum(1 for r in off if r['class']=='WHITE')
        print(f"   WHITE      ON {onW}/{len(on)}  vs  OFF {offW}/{len(off)}")
        verdict = ("SCENE-SIDE (PP_OFF >= PP_ON over-exposure: composite is not the gain)"
                   if st.mean(offhi) >= st.mean(onhi) - 2 or max(offhi) >= max(onhi)
                   else "COMPOSITE-GAIN (PP_OFF materially cooler than PP_ON)")
        print(f"   => {verdict}")

if __name__ == "__main__":
    main()
