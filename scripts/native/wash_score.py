#!/usr/bin/env python3
"""wash_score.py — numeric "wash" detector for the W2.1 placement-contract flip gate.

WHY THIS EXISTS
---------------
The Wave-5 flip of `RB3_PLACEMENT_CONTRACT` was HELD because one flag-ON gameplay
capture blew out nearly-white while the other three rendered normally. The
coordinator's concern: is that blow-out *caused by the flip*, or a pre-existing,
flag-INDEPENDENT wash that the small binary sample happened to land on flag-ON?

Judging that across 16+ PNGs by eye is exactly the confound that forced the hold.
This tool replaces the eyeball with a continuous, reproducible numeric score so the
S2 measurement can run a rank test (Mann-Whitney U) on the luma distributions and
apply a pre-declared decision rule.

WHAT IT MEASURES (per WAVE6_REVIEW A2)
--------------------------------------
For each image it reports CONTINUOUS metrics (not a single binary wash/no-wash bit):

  * mean_luma          — Rec.601 luma, normalized 0..1 (matches the recorded
                         Wave-5 luma values 95.2/23.7/202.5/122.6 when ×255).
  * hi_frac  (blowout) — fraction of pixels with luma > HI_THRESH (default 0.90).
  * lo_frac  (near-black) — fraction of pixels with luma < LO_THRESH (default 0.10).
  * pink_frac          — fraction of pixels in the magenta/pink hue band with
                         enough saturation+value to be the "broken-env" pink class.
  * p05 / p50 / p95    — luma percentiles (distribution shape).

BOTH TAILS are scored on purpose: the Wave-5 anomaly presented as blow-out (ON_1,
luma 202) AND near-black (OFF_2, luma 23.7). And HUE is scored separately because
the historical wash is described as *pink* (missing-texture / broken-env class, cf.
W0.5) which is a DIFFERENT phenomenon from a *white* exposure/bloom haze. Keeping
them as separate channels lets S2 attribute the mechanism, not just detect "bright".

CLASSIFICATION
--------------
Each image is bucketed into exactly one wash class using the continuous metrics:
  PINK      — pink_frac >= PINK_CLASS_FRAC                 (broken-env class)
  WHITE     — hi_frac  >= WHITE_CLASS_HI  or mean >= WHITE_CLASS_MEAN  (exposure/bloom)
  NEARBLACK — lo_frac  >= BLACK_CLASS_LO  or mean <= BLACK_CLASS_MEAN  (dark anomaly*)
  NEUTRAL   — none of the above
(*NEARBLACK can also be a legitimately dark venue shot; time-pinning in S2 controls
 for that. It is reported but is NOT counted as a "wash" for the existence-proof.)

A capture is "wash-class" (`is_wash`) iff class is PINK or WHITE. A PINK-or-WHITE
capture observed in a flag-OFF boot is an EXISTENCE PROOF of A/A-variability.

USAGE
-----
  # score one or more images -> JSON per image + summary
  python3 scripts/native/wash_score.py IMG [IMG...] [--json OUT.json]

  # compare two labeled sets (the S2 decision) -> Mann-Whitney + verdict
  python3 scripts/native/wash_score.py --compare \
      --off cap_OFF_1.png cap_OFF_2.png --on cap_ON_1.png cap_ON_2.png \
      [--json compare.json]

  # self-test on synthetic images (no game, no assets)
  python3 scripts/native/wash_score.py --selftest

This module is import-friendly: S2's capture harness can `import wash_score` and call
`score_image(path)` / `classify(metrics)` / `compare(off_scores, on_scores)` directly.
"""
import argparse
import json
import os
import sys

import numpy as np

try:
    from PIL import Image
except Exception as e:  # pragma: no cover
    Image = None
    _PIL_ERR = e

# ---- Metric thresholds (fractions are of total pixels; luma is normalized 0..1) ----
HI_THRESH = 0.90   # a pixel above this luma counts toward the blow-out (hi) tail
LO_THRESH = 0.10   # a pixel below this luma counts toward the near-black (lo) tail

# Pink / magenta "broken-env" hue band (degrees) + minimum saturation & value so that
# dark or desaturated pixels near the hue wheel's magenta don't get counted.
PINK_HUE_LO = 290.0
PINK_HUE_HI = 350.0
PINK_MIN_SAT = 0.20
PINK_MIN_VAL = 0.35

# ---- Classification thresholds (percent, i.e. 0..100 on the *_frac metrics) ----
# Chosen from the committed Wave-5 batch-0 captures (see PLAN.md "Batch 0 scores"):
#   OFF_1: pink 41.4  | OFF_2: lo 77.5  | ON_1: hi 77.1 mean .79 | ON_2: pink 75.5
# Separators are wide: neutral non-wash frames sit <2.5 on hi/pink and <60 on lo.
PINK_CLASS_FRAC = 15.0    # pink_frac% >= this  -> PINK
WHITE_CLASS_HI = 25.0     # hi_frac%  >= this   -> WHITE
WHITE_CLASS_MEAN = 0.65   # or mean_luma >= this
BLACK_CLASS_LO = 65.0     # lo_frac%  >= this   -> NEARBLACK
BLACK_CLASS_MEAN = 0.12   # or mean_luma <= this


def _rgb_to_luma_hsv(rgb01):
    """rgb01: HxWx3 float array in 0..1. Returns (luma, hue_deg, sat, val)."""
    r = rgb01[..., 0]
    g = rgb01[..., 1]
    b = rgb01[..., 2]
    luma = 0.299 * r + 0.587 * g + 0.114 * b
    mx = rgb01.max(axis=-1)
    mn = rgb01.min(axis=-1)
    val = mx
    d = mx - mn
    sat = np.where(mx > 1e-6, d / np.maximum(mx, 1e-6), 0.0)
    dd = d + 1e-6
    rc = (mx - r) / dd
    gc = (mx - g) / dd
    bc = (mx - b) / dd
    h = np.where(mx == r, bc - gc, np.where(mx == g, 2.0 + rc - bc, 4.0 + gc - rc))
    hue = (h / 6.0) % 1.0 * 360.0
    return luma, hue, sat, val


def metrics_from_rgb01(rgb01):
    """Compute the wash metrics dict from an HxWx3 float array in 0..1."""
    luma, hue, sat, val = _rgb_to_luma_hsv(rgb01)
    pink = (hue >= PINK_HUE_LO) & (hue <= PINK_HUE_HI) & (sat >= PINK_MIN_SAT) & (val >= PINK_MIN_VAL)
    m = {
        "mean_luma": float(luma.mean()),
        "mean_luma_255": float(luma.mean() * 255.0),
        "hi_frac": float(100.0 * (luma > HI_THRESH).mean()),
        "lo_frac": float(100.0 * (luma < LO_THRESH).mean()),
        "pink_frac": float(100.0 * pink.mean()),
        "p05": float(np.percentile(luma, 5)),
        "p50": float(np.percentile(luma, 50)),
        "p95": float(np.percentile(luma, 95)),
    }
    m["wash_class"] = classify(m)
    m["is_wash"] = m["wash_class"] in ("PINK", "WHITE")
    return m


def classify(m):
    """Bucket a metrics dict into PINK | WHITE | NEARBLACK | NEUTRAL.

    Order matters: PINK is checked first (a pink broken-env frame can also be bright,
    but the hue is the more specific signal), then WHITE, then NEARBLACK.
    """
    if m["pink_frac"] >= PINK_CLASS_FRAC:
        return "PINK"
    if m["hi_frac"] >= WHITE_CLASS_HI or m["mean_luma"] >= WHITE_CLASS_MEAN:
        return "WHITE"
    if m["lo_frac"] >= BLACK_CLASS_LO or m["mean_luma"] <= BLACK_CLASS_MEAN:
        return "NEARBLACK"
    return "NEUTRAL"


def score_image(path):
    """Load an image and return its metrics dict (adds 'path')."""
    if Image is None:  # pragma: no cover
        raise RuntimeError("Pillow (PIL) is required: %r" % (_PIL_ERR,))
    im = np.asarray(Image.open(path).convert("RGB")).astype(np.float32) / 255.0
    m = metrics_from_rgb01(im)
    m["path"] = path
    return m


def _mannwhitney_u(a, b):
    """Two-sided Mann-Whitney U on two 1-D samples. Returns (U, p). Falls back to a
    normal approximation if scipy is unavailable. Returns (nan, nan) if a side is empty."""
    a = np.asarray(a, dtype=float)
    b = np.asarray(b, dtype=float)
    if len(a) == 0 or len(b) == 0:
        return float("nan"), float("nan")
    try:
        from scipy.stats import mannwhitneyu
        U, p = mannwhitneyu(a, b, alternative="two-sided")
        return float(U), float(p)
    except Exception:
        # Normal approximation with tie correction.
        n1, n2 = len(a), len(b)
        allv = np.concatenate([a, b])
        order = allv.argsort()
        ranks = np.empty(len(allv), float)
        ranks[order] = np.arange(1, len(allv) + 1)
        # average ties
        _, inv, cnt = np.unique(allv, return_inverse=True, return_counts=True)
        sums = np.zeros(len(cnt))
        np.add.at(sums, inv, ranks)
        ranks = (sums / cnt)[inv]
        R1 = ranks[:n1].sum()
        U1 = R1 - n1 * (n1 + 1) / 2.0
        U = min(U1, n1 * n2 - U1)
        mu = n1 * n2 / 2.0
        sigma = np.sqrt(n1 * n2 * (n1 + n2 + 1) / 12.0)
        if sigma == 0:
            return float(U), float("nan")
        z = (U - mu) / sigma
        from math import erf, sqrt
        p = 2.0 * (1.0 - 0.5 * (1 + erf(abs(z) / sqrt(2))))
        return float(U), float(min(1.0, p))


def compare(off_scores, on_scores, alpha=0.05):
    """Given two lists of metrics dicts, run the S2 decision rule.

    Returns a dict with the Mann-Whitney result on mean_luma, per-state wash counts,
    and a verdict: 'A/A-variable' | 'flag-ON-specific' | 'inconclusive'.
    """
    off_luma = [s["mean_luma"] for s in off_scores]
    on_luma = [s["mean_luma"] for s in on_scores]
    off_wash = [s for s in off_scores if s["is_wash"]]
    on_wash = [s for s in on_scores if s["is_wash"]]
    U, p = _mannwhitney_u(off_luma, on_luma)

    off_wash_in = len(off_wash) > 0
    all_wash_on = (len(on_wash) > 0) and (len(off_wash) == 0)

    # Decision rule (pre-declared; see PLAN.md "S2 decision rule"):
    #   1. A wash-class capture in ANY flag-OFF boot -> A/A-variable (existence proof).
    #   2. Else, all wash mass in flag-ON AND luma distributions differ (p<alpha) -> flag-ON-specific.
    #   3. Else -> inconclusive.
    if off_wash_in:
        verdict = "A/A-variable"
        reason = ("wash-class capture(s) present in flag-OFF: "
                  + ", ".join("%s(%s)" % (os.path.basename(s.get("path", "?")), s["wash_class"]) for s in off_wash))
    elif all_wash_on and (p == p and p < alpha):
        verdict = "flag-ON-specific"
        reason = "all wash mass in flag-ON and Mann-Whitney p=%.4g < %.3g" % (p, alpha)
    else:
        verdict = "inconclusive"
        reason = ("no flag-OFF wash but distributions not separated (p=%s) "
                  "or no flag-ON wash either" % ("%.4g" % p if p == p else "nan"))

    return {
        "verdict": verdict,
        "reason": reason,
        "mannwhitney_U": U,
        "mannwhitney_p": p,
        "alpha": alpha,
        "n_off": len(off_scores),
        "n_on": len(on_scores),
        "off_mean_luma": off_luma,
        "on_mean_luma": on_luma,
        "off_wash_classes": [s["wash_class"] for s in off_scores],
        "on_wash_classes": [s["wash_class"] for s in on_scores],
        "off_wash_count": len(off_wash),
        "on_wash_count": len(on_wash),
    }


# --------------------------------------------------------------------------- selftest
def _solid(rgb, h=64, w=64):
    a = np.zeros((h, w, 3), np.float32)
    a[..., 0] = rgb[0]
    a[..., 1] = rgb[1]
    a[..., 2] = rgb[2]
    return a


def selftest():
    fails = []

    def check(name, cond, detail=""):
        status = "ok" if cond else "FAIL"
        print("  [%s] %s %s" % (status, name, detail))
        if not cond:
            fails.append(name)

    # 1. Neutral mid-gray -> NEUTRAL.
    m = metrics_from_rgb01(_solid((0.5, 0.5, 0.5)))
    check("gray->NEUTRAL", m["wash_class"] == "NEUTRAL", "(%s, mean=%.2f)" % (m["wash_class"], m["mean_luma"]))
    check("gray mean~0.5", abs(m["mean_luma"] - 0.5) < 0.02)
    check("gray no tails/pink", m["hi_frac"] < 1 and m["lo_frac"] < 1 and m["pink_frac"] < 1)

    # 2. Full white -> WHITE, hi_frac ~100.
    m = metrics_from_rgb01(_solid((0.99, 0.99, 0.99)))
    check("white->WHITE", m["wash_class"] == "WHITE", "(hi=%.1f, mean=%.2f)" % (m["hi_frac"], m["mean_luma"]))
    check("white hi~100", m["hi_frac"] > 99)

    # 3. Full black -> NEARBLACK, lo_frac ~100.
    m = metrics_from_rgb01(_solid((0.0, 0.0, 0.0)))
    check("black->NEARBLACK", m["wash_class"] == "NEARBLACK", "(lo=%.1f, mean=%.2f)" % (m["lo_frac"], m["mean_luma"]))
    check("black lo~100", m["lo_frac"] > 99)

    # 4. Magenta -> PINK, pink_frac ~100, and is_wash True.
    m = metrics_from_rgb01(_solid((1.0, 0.0, 1.0)))
    check("magenta->PINK", m["wash_class"] == "PINK", "(pink=%.1f)" % m["pink_frac"])
    check("magenta pink~100", m["pink_frac"] > 99)
    check("magenta is_wash", m["is_wash"] is True)

    # 5. A dark saturated pink must NOT count as pink (value gate) -> not PINK.
    m = metrics_from_rgb01(_solid((0.15, 0.0, 0.15)))
    check("dark-magenta not PINK", m["wash_class"] != "PINK", "(pink=%.1f, class=%s)" % (m["pink_frac"], m["wash_class"]))

    # 6. Half bright / half dark -> both tails nonzero, mean mid, NEUTRAL-ish.
    a = np.zeros((64, 64, 3), np.float32)
    a[:32] = 0.98
    a[32:] = 0.02
    m = metrics_from_rgb01(a)
    check("split both tails", m["hi_frac"] > 40 and m["lo_frac"] > 40, "(hi=%.1f lo=%.1f)" % (m["hi_frac"], m["lo_frac"]))

    # 7. compare(): a PINK flag-OFF capture forces A/A-variable regardless of ON.
    off = [metrics_from_rgb01(_solid((1.0, 0.0, 1.0))), metrics_from_rgb01(_solid((0.5, 0.5, 0.5)))]
    on = [metrics_from_rgb01(_solid((0.5, 0.5, 0.5))), metrics_from_rgb01(_solid((0.5, 0.5, 0.5)))]
    for s in off:
        s["path"] = "off"
    for s in on:
        s["path"] = "on"
    c = compare(off, on)
    check("compare A/A-variable on OFF-wash", c["verdict"] == "A/A-variable", "(%s)" % c["verdict"])

    # 8. compare(): all-ON wash + separated luma -> flag-ON-specific.
    off = [metrics_from_rgb01(_solid((0.3, 0.3, 0.3))) for _ in range(4)]
    on = [metrics_from_rgb01(_solid((0.99, 0.99, 0.99))) for _ in range(4)]
    for s in off:
        s["path"] = "off"
    for s in on:
        s["path"] = "on"
    c = compare(off, on)
    check("compare flag-ON-specific", c["verdict"] == "flag-ON-specific", "(%s, p=%.3g)" % (c["verdict"], c["mannwhitney_p"]))

    print()
    if fails:
        print("SELFTEST FAILED: %d/%d checks failed: %s" % (len(fails), 8, ", ".join(fails)))
        return 1
    print("SELFTEST PASSED")
    return 0


# --------------------------------------------------------------------------- main
def _print_row(m):
    print("%-40s class=%-9s mean=%.3f hi=%5.2f lo=%5.2f pink=%5.2f p05=%.2f p95=%.2f" % (
        os.path.basename(m.get("path", "?")), m["wash_class"], m["mean_luma"],
        m["hi_frac"], m["lo_frac"], m["pink_frac"], m["p05"], m["p95"]))


def main(argv=None):
    ap = argparse.ArgumentParser(description="Numeric wash detector for the W2.1 flip gate.")
    ap.add_argument("images", nargs="*", help="image files to score")
    ap.add_argument("--selftest", action="store_true", help="run synthetic self-test and exit")
    ap.add_argument("--compare", action="store_true", help="compare --off vs --on sets (S2 decision)")
    ap.add_argument("--off", nargs="*", default=[], help="flag-OFF images (with --compare)")
    ap.add_argument("--on", nargs="*", default=[], help="flag-ON images (with --compare)")
    ap.add_argument("--alpha", type=float, default=0.05)
    ap.add_argument("--json", help="write results JSON to this path")
    args = ap.parse_args(argv)

    if args.selftest:
        return selftest()

    if args.compare:
        off = [score_image(p) for p in args.off]
        on = [score_image(p) for p in args.on]
        print("flag-OFF:")
        for m in off:
            _print_row(m)
        print("flag-ON:")
        for m in on:
            _print_row(m)
        c = compare(off, on, alpha=args.alpha)
        print("\nVERDICT: %s" % c["verdict"])
        print("  %s" % c["reason"])
        print("  Mann-Whitney U=%.3g p=%.4g (n_off=%d n_on=%d)" % (
            c["mannwhitney_U"], c["mannwhitney_p"], c["n_off"], c["n_on"]))
        if args.json:
            with open(args.json, "w") as f:
                json.dump({"compare": c, "off": off, "on": on}, f, indent=2)
            print("wrote", args.json)
        return 0

    if not args.images:
        ap.error("no images given (use --selftest or --compare or pass image paths)")
    scored = [score_image(p) for p in args.images]
    for m in scored:
        _print_row(m)
    if args.json:
        with open(args.json, "w") as f:
            json.dump(scored, f, indent=2)
        print("wrote", args.json)
    return 0


if __name__ == "__main__":
    sys.exit(main())
