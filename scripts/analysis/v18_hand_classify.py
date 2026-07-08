#!/usr/bin/env python3
"""v18_hand_classify.py — Wave-18 Lane V (VISCAP) articulated-capture classifier.

Consumes the Wii GAMEPLAY articulated capture produced by
  milo-trace tools/wii_visgame_capture.py gpcap
(a set of per-frame D2_wii_bones-format dumps: records + interbone_tables incl
D_rel_rot), validates G-D5-1 (finger curl swing >=15deg across frames), then
classifies per the VERDICT (execution/R5-HANDS-ENDGAME/VERDICT.md) §4 branch table
using two clip-matched native references:

  (1) NATIVE GAMEPLAY finger curl  — D3_delta_table_gameplay.json `native_relRot`
      (director band on guitar; the clip-MATCHED partner to a Wii gameplay capture).
  (2) NATIVE main_hub vignette SWEEP — the D4 sweep dir (skeleton curl family, a
      cross-clip skeleton reference for envelope-overlap).

Branch table (thresholds inherited PLAN-R5 §3.1; ep = max(3, 2*ep_noise)):
  * all finger pairs' Wii curl within native envelope (<= ep) at matched curl -> GT-A
  * >=2 pairs DIVERGENT (median>5 & >3*ep_noise) + CONSTANT (std<max(2,ep_noise)) -> GT-B
  * pairs DIVERGENT + TIME-VARYING -> GT-C
  * anchors themselves > ep -> GT-U
  * capture power-floor not met (<5 frames or <2 pairs or swing<15) -> indeterminate

This is a MEASUREMENT/classification tool only (Lane V stops at the branch letter;
GT-A palette forensics is a separate coordinator lane).

Usage:
  v18_hand_classify.py --gpdir /tmp/v18-evidence/gpdump \\
     --native-gameplay <D3_delta_table_gameplay.json> [--ep-noise 0.06]
"""
import argparse, glob, json, os, sys
from statistics import median, pstdev

FINGER_PAIRS = [
    "middlefinger01->middlefinger02", "middlefinger02->middlefinger03",
    "ringfinger01->ringfinger02", "ringfinger02->ringfinger03",
    "thumb01->thumb02", "thumb02->thumb03",
]
ANCHOR = "forearm->hand"


def load_wii_frames(gpdir):
    """Each gameplay frame dump -> {pair(no side): [relRot per side...]} using L+R."""
    frames = []
    for fp in sorted(glob.glob(os.path.join(gpdir, "wii_gp_frame_*.json"))):
        d = json.load(open(fp))
        perpair = {}
        for side in ("L", "R"):
            for row in d.get("interbone_tables", {}).get(side, []):
                # strip leading side token e.g. "L-middlefinger01->L-middlefinger02"
                pk = row["pair"].replace(f"{side}-", "")
                perpair.setdefault(pk, []).append(row["rel_rot_deg"])
        frames.append({"file": os.path.basename(fp), "pairs": perpair})
    return frames


def native_gameplay_curl(path):
    """D3 gameplay native_relRot per finger pair (across members = a curl range)."""
    d = json.load(open(path))
    out = {}
    for r in d.get("rows", []):
        if r.get("status") != "ok":
            continue
        pk = r["pair"].replace("l-", "").replace("r-", "")
        v = r.get("native_relRot")
        if v is not None:
            out.setdefault(pk, []).append(v)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--gpdir", required=True)
    ap.add_argument("--native-gameplay", required=True)
    ap.add_argument("--ep-noise", type=float, default=0.06)
    ap.add_argument("--out", default=None)
    a = ap.parse_args()

    frames = load_wii_frames(a.gpdir)
    n = len(frames)
    ep = max(3.0, 2 * a.ep_noise)
    nat = native_gameplay_curl(a.native_gameplay)

    # Wii curl per finger pair across frames (pool L+R samples per frame).
    wii_curl = {}
    for pk in FINGER_PAIRS:
        vals = []
        for f in frames:
            vals += f["pairs"].get(pk, [])
        wii_curl[pk] = vals
    # anchor read (interpretability gate)
    anchor_vals = []
    for f in frames:
        anchor_vals += f["pairs"].get(ANCHOR, [])

    report = {"n_frames": n, "ep": ep, "ep_noise": a.ep_noise, "pairs": {}}
    swing_ok = 0
    divergent = 0
    constant_pairs = 0
    for pk in FINGER_PAIRS:
        wv = wii_curl[pk]
        nv = nat.get(pk, [])
        if len(wv) < 2 or not nv:
            report["pairs"][pk] = {"status": "insufficient", "wii_n": len(wv)}
            continue
        wmin, wmax = min(wv), max(wv)
        swing = wmax - wmin
        nmin, nmax = min(nv), max(nv)
        # overlap: does the Wii curl range intersect the native curl range (+/- ep)?
        overlap = not (wmax < nmin - ep or wmin > nmax + ep)
        # distance from Wii range to nearest native sample (0 if overlapping)
        gap = 0.0 if overlap else min(abs(wmin - nmax), abs(wmax - nmin))
        std = pstdev(wv) if len(wv) > 1 else 0.0
        is_div = gap > 5.0 and gap > 3 * a.ep_noise
        is_const = std < max(2.0, a.ep_noise)
        if swing >= 15:
            swing_ok += 1
        if is_div:
            divergent += 1
            if is_const:
                constant_pairs += 1
        report["pairs"][pk] = {
            "wii_range": [round(wmin, 2), round(wmax, 2)], "wii_swing": round(swing, 2),
            "native_range": [round(nmin, 2), round(nmax, 2)],
            "overlap": overlap, "gap_deg": round(gap, 2), "wii_std": round(std, 2),
            "divergent": is_div, "constant": is_const,
        }

    max_swing = max((report["pairs"][p].get("wii_swing", 0)
                     for p in FINGER_PAIRS if "wii_swing" in report["pairs"][p]),
                    default=0)
    anchor_ok = (max(anchor_vals) - min(anchor_vals) < ep) if len(anchor_vals) >= 2 else None
    report["max_finger_swing"] = round(max_swing, 2)
    report["G_D5_1_articulating"] = max_swing >= 15.0
    report["power_floor_met"] = (n >= 5 and sum(
        1 for p in FINGER_PAIRS if "wii_swing" in report["pairs"][p]) >= 2 and max_swing >= 15)

    # ---- mechanical branch (G-D5-4) ----
    if not report["power_floor_met"]:
        branch = "INDETERMINATE (power-floor: need >=5 frames, >=2 pairs, swing>=15deg)"
    elif divergent == 0:
        branch = "GT-A (all finger pairs within native envelope at matched curl)"
    elif divergent >= 2 and constant_pairs >= 2:
        branch = "GT-B (>=2 pairs DIVERGENT + CONSTANT)"
    elif divergent >= 2:
        branch = "GT-C (>=2 pairs DIVERGENT + TIME-VARYING)"
    else:
        branch = "INDETERMINATE (<2 divergent pairs)"
    report["branch"] = branch
    report["divergent_pairs"] = divergent
    report["constant_divergent_pairs"] = constant_pairs

    print(json.dumps(report, indent=1))
    if a.out:
        json.dump(report, open(a.out, "w"), indent=1)
        print(f"\nwrote {a.out}", file=sys.stderr)
    print(f"\nBRANCH: {branch}", file=sys.stderr)


if __name__ == "__main__":
    main()
