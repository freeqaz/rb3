#!/usr/bin/env python3
"""
_uigrade_gate.py — Wave-13 Lane G (UIGRADE) A11 percentile contrast gate.

For each gated screen, compute the focused-item contrast ratio on the luma of a
focused-item ROI: ratio = p60(bar field) / p5(text stroke). Reports default vs
PP_OFF so the coordinator can pre-register PASS = ratio>=2.0 (where PP_OFF
reaches it) else PP_OFF-parity (within 5%).

    python3 scripts/native/_uigrade_gate.py --dir /tmp/uigrade
"""
import argparse, os, sys
import numpy as np
from PIL import Image

# Focused-item ROIs (x0,y0,x1,y1) at 1280x720. Derived by eye from the captures;
# stable across arms (fixed-clock, identical nav).
ROIS = {
    "hub":        (74, 207, 335, 248),   # PLAY NOW yellow bar + dark text
    "songselect": (12, 322, 892, 353),   # "25 or 6 to 4" highlighted row
    "partdiff":   (74, 516, 350, 552),   # EASY yellow bar + dark text
}

def luma(img):
    a = np.asarray(img.convert("RGB"), dtype=np.float64)
    # Rec.601 luma, 0..255
    return 0.299*a[...,0] + 0.587*a[...,1] + 0.114*a[...,2]

def gate_for(path, roi):
    im = Image.open(path)
    L = luma(im)
    x0,y0,x1,y1 = roi
    r = L[y0:y1, x0:x1]
    p5  = float(np.percentile(r, 5))
    p60 = float(np.percentile(r, 60))
    p95 = float(np.percentile(r, 95))
    ratio = p60 / max(p5, 1e-6)
    span  = p95 / max(p5, 1e-6)
    return dict(p5=p5, p60=p60, p95=p95, ratio=ratio, span=span)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dir", default="/tmp/uigrade")
    args = ap.parse_args()
    print(f"{'screen':<12} {'arm':<8} {'p5':>7} {'p60':>7} {'p95':>7} {'p60/p5':>8} {'p95/p5':>8}")
    results = {}
    for screen, roi in ROIS.items():
        for arm in ("default", "ppoff"):
            p = os.path.join(args.dir, f"{arm}_{screen}.png")
            if not os.path.exists(p):
                print(f"{screen:<12} {arm:<8} MISSING {p}"); continue
            g = gate_for(p, roi)
            results[(screen,arm)] = g
            print(f"{screen:<12} {arm:<8} {g['p5']:>7.1f} {g['p60']:>7.1f} {g['p95']:>7.1f} "
                  f"{g['ratio']:>8.2f} {g['span']:>8.2f}")
    print("\n--- Pre-registration (per screen) ---")
    for screen in ROIS:
        d = results.get((screen,"default")); o = results.get((screen,"ppoff"))
        if not d or not o: continue
        ppoff_r = o['ratio']
        if ppoff_r >= 2.0:
            crit = f"ratio>=2.0 (PP_OFF reaches {ppoff_r:.2f})"
        else:
            lo, hi = ppoff_r*0.95, ppoff_r*1.05
            crit = f"PP_OFF-parity: ON ratio within [{lo:.2f},{hi:.2f}] (PP_OFF={ppoff_r:.2f}<2.0)"
        print(f"  {screen:<12} default={d['ratio']:.2f}  PP_OFF={ppoff_r:.2f}  => PASS = {crit}")

if __name__ == "__main__":
    sys.exit(main())
