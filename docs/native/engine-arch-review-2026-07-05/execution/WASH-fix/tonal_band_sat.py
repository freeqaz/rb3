#!/usr/bin/env python3
"""tonal_band_sat.py — per-tonal-band HSV saturation for the H2 (composite mid-tone
desaturation) test. Reproduces Wave-7 A.S2's method: split a venue-wall crop by
luma into low/mid/high bands and report mean saturation per band. H2 predicts the
composite (default) collapses MID-band saturation on the hot venue reveal while the
composite-off control (RB3_PP_OFF) keeps it — i.e. default mid-sat << pp_off mid-sat
at ms3000, with the venue path ENGAGED (SCENE engaged=1 in the boot log).

Usage:
  python3 tonal_band_sat.py <img1.png> [img2.png ...]
  # crop defaults to a centre-left venue-wall region avoiding HUD/highway
"""
import sys, colorsys
from PIL import Image

def bands(path, crop=None):
    im = Image.open(path).convert("RGB")
    W, H = im.size
    # venue-wall crop: upper-centre band (avoid bottom highway/HUD + top letterbox)
    if crop is None:
        crop = (int(W*0.15), int(H*0.12), int(W*0.85), int(H*0.55))
    im = im.crop(crop)
    px = list(im.getdata())
    lo = {"s": 0.0, "n": 0}; mid = {"s": 0.0, "n": 0}; hi = {"s": 0.0, "n": 0}
    lumas = []
    for r, g, b in px:
        rf, gf, bf = r/255.0, g/255.0, b/255.0
        h, s, v = colorsys.rgb_to_hsv(rf, gf, bf)  # v == max(rgb) tone proxy
        lumas.append(v)
        if   v < 0.25: d = lo
        elif v < 0.65: d = mid
        else:          d = hi
        d["s"] += s; d["n"] += 1
    def mean(d): return (d["s"]/d["n"]) if d["n"] else float("nan")
    meanv = sum(lumas)/len(lumas)
    return {"low_sat": mean(lo), "mid_sat": mean(mid), "high_sat": mean(hi),
            "low_n": lo["n"], "mid_n": mid["n"], "hi_n": hi["n"], "mean_val": meanv}

if __name__ == "__main__":
    for p in sys.argv[1:]:
        b = bands(p)
        print(f"{p}: mid_sat={b['mid_sat']:.3f} low_sat={b['low_sat']:.3f} "
              f"high_sat={b['high_sat']:.3f} mean_val={b['mean_val']:.3f} "
              f"(n lo/mid/hi = {b['low_n']}/{b['mid_n']}/{b['hi_n']})")
