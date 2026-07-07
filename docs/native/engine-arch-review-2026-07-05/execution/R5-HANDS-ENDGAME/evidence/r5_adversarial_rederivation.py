#!/usr/bin/env python3
"""R5-HANDS-ENDGAME — adversarial re-derivation of the D4 headline, from raw evidence.

Consumes ONLY committed R1-DOLPHIN evidence (D4_native_sweep_raw.tar.gz +
D4_delta_table.json). Re-derives, independently of scripts/analysis/
interbone_framematch.py:

  F1  per-member native clip identity (the two male members play DIFFERENT clips)
  F2  per-pair native |relRot| envelopes match the join script's (script vindicated)
  F3  exactly-shared envelope endpoints across the two DIFFERENT male clips
  F4  within-member cross-pair correlation of |relRot| == 1.0000 (a single scalar
      curl parameter drives every middle/ring pair: a 1-D pose family)
  F5  pooled across both male clips, (mf12 -> rf12) is one monotone curve
      (both clips sample the SAME 1-D family, adjacent intervals)
  F6  the Wii settled magnitude sits BELOW the native envelope on EVERY surviving
      pair (uniformly "less curled"), and INSIDE the envelope of the one member
      (slot1) whose clip covers the relaxed region (floors ~0.1-0.65 deg)

Conclusion these facts force (see VERDICT.md): the D4 "surviving" middle/ring
FLOOR is a property of WHICH CURL INTERVAL each vignette clip animates, measured
against a DRIVERLESS Wii settled pose — not evidence of a per-bone basis error
in the native skeleton. Run from the evidence directory:

  python3 r5_adversarial_rederivation.py \
      --d4-dir ../../R1-DOLPHIN/evidence
"""
import argparse, io, json, math, sys, tarfile
from statistics import correlation

import numpy as np

L_PAIRS = [
    ("anchor", "bone_L-foreArm.mesh", "bone_L-hand.mesh"),
    ("mf01", "bone_L-hand.mesh", "bone_L-middlefinger01.mesh"),
    ("mf12", "bone_L-middlefinger01.mesh", "bone_L-middlefinger02.mesh"),
    ("mf23", "bone_L-middlefinger02.mesh", "bone_L-middlefinger03.mesh"),
    ("rf01", "bone_L-hand.mesh", "bone_L-ringfinger01.mesh"),
    ("rf12", "bone_L-ringfinger01.mesh", "bone_L-ringfinger02.mesh"),
    ("rf23", "bone_L-ringfinger02.mesh", "bone_L-ringfinger03.mesh"),
    ("th01", "bone_L-hand.mesh", "bone_L-thumb01.mesh"),
    ("th12", "bone_L-thumb01.mesh", "bone_L-thumb02.mesh"),
    ("th23", "bone_L-thumb02.mesh", "bone_L-thumb03.mesh"),
]
# Wii settled magnitudes (D2 chain, male; from D4_delta_table.md L-hand rows)
WII = {"anchor": 91.31, "mf01": 5.404, "mf12": 24.366, "mf23": 17.732,
       "rf01": 7.428, "rf12": 26.126, "rf23": 5.739,
       "th01": 123.644, "th12": 23.562, "th23": 11.948}
SURVIVING = ["mf01", "mf12", "mf23", "rf01", "rf12", "rf23"]


def rotmag(P, C):
    R = P.T @ C
    c = max(-1.0, min(1.0, (np.trace(R) - 1) / 2))
    return math.degrees(math.acos(c))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--d4-dir", default="../../R1-DOLPHIN/evidence")
    a = ap.parse_args()

    table = json.load(open(f"{a.d4_dir}/D4_delta_table.json"))
    tar = tarfile.open(f"{a.d4_dir}/D4_native_sweep_raw.tar.gz")
    snaps = []
    for m in sorted(tar.getmembers(), key=lambda m: m.name):
        if m.name.endswith(".json") and "sweep_" in m.name:
            snaps.append(json.load(io.TextIOWrapper(tar.extractfile(m))))
    assert len(snaps) == 32, len(snaps)

    # F1 — clip identity per member
    clips, series = {}, {}
    for d in snaps:
        for mem in d["members"]:
            slot = mem["slot"]
            drv = [x for x in mem["drivers"] if x["clip_type"] == "vignette"][0]
            clips.setdefault((slot, mem["gender"]), set()).add(drv["playing_clip"])
            bones = {b["name"]: np.array(b["world"]["rows"]) for b in mem["bones"]}
            for key, pn, cn in L_PAIRS:
                series.setdefault((slot, key), []).append(rotmag(bones[pn], bones[cn]))
    print("F1 clips per member:", {k: sorted(v) for k, v in sorted(clips.items())})
    assert clips[(0, "male")] == {"player3_m"} and clips[(2, "male")] == {"player0_m"}, \
        "the two male members play DIFFERENT vignette clips"

    # F2 — envelopes reproduce the committed join table (slot2 L, the decisive rows)
    m2 = [m for m in table["members"] if m["slot"] == 2 and m["hand"] == "L"][0]
    byname = {r["pair"].split(">")[-1]: r for r in m2["rows"]}
    checks = {"mf12": "l-middlefinger02", "rf12": "l-ringfinger02"}
    for key, suffix in checks.items():
        row = next(r for r in m2["rows"] if r["pair"].endswith(suffix))
        lo, hi = min(series[(2, key)]), max(series[(2, key)])
        assert abs(lo - row["native_relRot_min"]) < 0.05 and abs(hi - row["native_relRot_max"]) < 0.05, \
            (key, lo, hi, row["native_relRot_min"], row["native_relRot_max"])
    print("F2 join-script envelopes independently reproduced (slot2 L mf12/rf12): OK")

    # F3 — shared endpoints across the two DIFFERENT male clips
    print("F3 shared male envelope endpoints (slot0 max vs slot2 min):")
    shared = 0
    for key in ["mf01", "mf12", "mf23", "rf01", "rf12"]:
        s0max, s2min = max(series[(0, key)]), min(series[(2, key)])
        flag = "SHARED" if abs(s0max - s2min) < 0.15 else "-"
        if flag == "SHARED":
            shared += 1
        print(f"    {key}: slot0 max {s0max:7.2f}  slot2 min {s2min:7.2f}  {flag}")
    assert shared >= 4, "different clips share exact envelope endpoints => shared pose family"

    # F4 — 1-D family: perfect cross-pair correlation within each member
    print("F4 within-member cross-pair correlation of |relRot| (1-D curl parameter):")
    for slot in (0, 1, 2, 3):
        c1 = correlation(series[(slot, "mf12")], series[(slot, "rf12")])
        c2 = correlation(series[(slot, "mf12")], series[(slot, "mf23")])
        print(f"    slot{slot}: corr(mf12,rf12)={c1:.4f} corr(mf12,mf23)={c2:.4f}")
        assert c1 > 0.999 and c2 > 0.999
    # F5 — pooled monotone curve across both male clips
    pool = sorted(zip(series[(0, "mf12")] + series[(2, "mf12")],
                      series[(0, "rf12")] + series[(2, "rf12")]))
    viol = sum(1 for (x0, y0), (x1, y1) in zip(pool, pool[1:]) if y1 < y0 - 1.0)
    print(f"F5 pooled male (mf12->rf12) monotonicity violations >1deg: {viol}/{len(pool)-1}")
    assert viol == 0

    # F6 — Wii sits below every male envelope on every surviving pair; inside slot1's
    print("F6 Wii settled magnitude vs native envelopes (surviving pairs):")
    for key in SURVIVING:
        w = WII[key]
        below0 = w < min(series[(0, key)])
        below2 = w < min(series[(2, key)])
        in1 = min(series[(1, key)]) <= w <= max(series[(1, key)])
        print(f"    {key}: wii {w:6.2f}  < slot0 min: {below0}  < slot2 min: {below2}  inside slot1: {in1}")
        assert below0 and below2, "uniform less-curled direction"
    print("\nALL ASSERTIONS PASS — see VERDICT.md for what these facts force.")


if __name__ == "__main__":
    sys.exit(main())
