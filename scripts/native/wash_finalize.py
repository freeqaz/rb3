#!/usr/bin/env python3
"""wash_finalize.py — combine ALL songMs-pinned W2.1-flip-blocker captures into the
final S2 verdict + montage.

The S2 measurement ran in two passes against the same binary/protocol/songMs window:
a cold-cache pass (which surfaced the PINK broken-env wash) and a warm-cache pass
(which converged NEARBLACK in both flag states). Both passes wrote raw PNGs into the
same directory with flag+songMs-encoded names (`<FLAG>_<idx>_<songMs>.png`). This
script scores every such PNG once, tags its flag from the filename, runs the
pre-declared decision rule (`wash_score.compare`), and writes the combined
batch_log.json / verdict.json / montage.png. Combining is monotone-safe: more
captures can only strengthen an A/A-variable verdict (rule 2 requires ZERO flag-OFF
wash), never invent a flag-ON-specific one.

Usage:
  python3 scripts/native/wash_finalize.py \
      --raws /tmp/wave6-flipblocker-captures \
      --out  docs/native/engine-arch-review-2026-07-05/execution/W2.1-flip-blocker/measure
"""
import argparse, glob, json, os, re, sys, importlib.util

HERE = os.path.dirname(os.path.abspath(__file__))
ws = importlib.util.module_from_spec(
    importlib.util.spec_from_file_location("wash_score", os.path.join(HERE, "wash_score.py")))
importlib.util.spec_from_file_location("wash_score", os.path.join(HERE, "wash_score.py")).loader.exec_module(ws)

NAME_RE = re.compile(r"^(OFF|ON)_(\d+)_(\d+)\.png$")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--raws", default="/tmp/wave6-flipblocker-captures")
    ap.add_argument("--out", required=True)
    ap.add_argument("--alpha", type=float, default=0.05)
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)

    scores = {"OFF": [], "ON": []}
    for p in sorted(glob.glob(os.path.join(args.raws, "*.png"))):
        mo = NAME_RE.match(os.path.basename(p))
        if not mo:
            continue
        flag, idx, songms = mo.group(1), int(mo.group(2)), int(mo.group(3))
        m = ws.score_image(p)
        m["flag"] = flag
        m["songMs"] = float(songms)
        scores[flag].append(m)

    batch = scores["OFF"] + scores["ON"]
    for m in sorted(batch, key=lambda m: (m["flag"], m["songMs"])):
        print("%-4s songMs=%d class=%-9s mean=%.3f hi=%4.1f lo=%4.1f pink=%4.1f  %s" % (
            m["flag"], int(m["songMs"]), m["wash_class"], m["mean_luma"],
            m["hi_frac"], m["lo_frac"], m["pink_frac"], os.path.basename(m["path"])))

    verdict = ws.compare(scores["OFF"], scores["ON"], alpha=args.alpha)
    print("\nVERDICT:", verdict["verdict"], "-", verdict["reason"])
    print("  Mann-Whitney U=%s p=%s (n_off=%d n_on=%d)" % (
        verdict["mannwhitney_U"], verdict["mannwhitney_p"], verdict["n_off"], verdict["n_on"]))
    print("  OFF classes:", [s["wash_class"] for s in scores["OFF"]])
    print("  ON  classes:", [s["wash_class"] for s in scores["ON"]])

    with open(os.path.join(args.out, "batch_log.json"), "w") as f:
        json.dump(batch, f, indent=2)
    with open(os.path.join(args.out, "verdict.json"), "w") as f:
        json.dump(verdict, f, indent=2)

    # montage: show the wash-class captures first (the anomalies), then neutrals/nearblack
    try:
        from PIL import Image, ImageDraw

        def pick(flag):
            arr = sorted(scores[flag], key=lambda m: (not m["is_wash"], m["songMs"]))
            return arr[:4]
        items = []
        for flag in ("OFF", "ON"):
            for m in pick(flag):
                items.append((m["path"],
                              f"{flag} @{int(m['songMs'])}ms\n{m['wash_class']}\nL{m['mean_luma']:.2f} pink{m['pink_frac']:.0f}"))
        imgs = [(Image.open(p).convert("RGB"), lab) for p, lab in items]
        th = 300
        scaled = [(im.resize((max(1, int(im.width * th / im.height)), th)), lab) for im, lab in imgs]
        pad, top, caph = 6, 26, 46
        W = sum(im.width for im, _ in scaled) + pad * (len(scaled) + 1)
        canvas = Image.new("RGB", (W, th + top + caph), (20, 20, 20))
        d = ImageDraw.Draw(canvas)
        d.text((pad, 6), "W2.1-flip-blocker S2 — combined songMs-pinned 20750-21250 — VERDICT: %s"
                % verdict["verdict"], fill=(255, 255, 0))
        x = pad
        for im, lab in scaled:
            canvas.paste(im, (x, top))
            for i, line in enumerate(lab.split("\n")):
                d.text((x, top + th + 2 + i * 12), line, fill=(230, 230, 230))
            x += im.width + pad
        canvas.save(os.path.join(args.out, "montage.png"))
        print("montage ->", os.path.join(args.out, "montage.png"))
    except Exception as e:
        print("montage failed:", e)
    return 0


if __name__ == "__main__":
    sys.exit(main())
