#!/usr/bin/env python3
"""_w32-boxambient-ab.py — W3.2 BoxMapLighting prototype before/after venue A/B.

Reuses w21flip-dolphin-ab.py's proven boot_capture_gameplay() recipe (nav to
gameplay -> director_disable -> wide venue shot -> screenshot under
RB3_FIXED_CLOCK) to produce songMs-pinned venue captures with the box-ambient
prototype OFF (scalar ambient, legacy) vs ON (RB3_BOX_AMBIENT=1, 6-axis cube).

Captures 2xOFF + 2xON so the A/A pairs separate boot variance from the flag
effect, and scores each capture numerically (mean luma, blow-out %>0.95,
crush %<0.05, pink-hue fraction) per the W3.2 G-B gate design — so "the ambient
changed" is machine-visible, not eyeballed.

Usage:
  python3 scripts/native/_w32-boxambient-ab.py \
      --bin /tmp/wave6-boxmap-build/rb3-native \
      --out docs/native/engine-arch-review-2026-07-05/execution/W3.2/captures
"""
import argparse, importlib.util, json, os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))

_spec = importlib.util.spec_from_file_location(
    "w21ab", os.path.join(HERE, "w21flip-dolphin-ab.py"))
w21 = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(w21)


def score(png):
    from PIL import Image
    import numpy as np
    im = np.asarray(Image.open(png).convert("RGB"), dtype=np.float32) / 255.0
    r, g, b = im[..., 0], im[..., 1], im[..., 2]
    luma = 0.2126 * r + 0.7152 * g + 0.0722 * b
    # pink/magenta: red & blue both high, green notably lower (broken-env class).
    pink = (r > 0.45) & (b > 0.35) & (g < np.minimum(r, b) - 0.10)
    n = luma.size
    return {
        "mean_luma": float(luma.mean() * 255.0),
        "pct_blowout": float((luma > 0.95).sum() / n * 100.0),
        "pct_crush": float((luma < 0.05).sum() / n * 100.0),
        "pct_pink": float(pink.sum() / n * 100.0),
    }


def songms_from_log(prefix):
    try:
        t = open(prefix + ".engine.log", errors="ignore").read()
    except OSError:
        return None
    m = re.findall(r"is_playing songMs=([0-9.]+)", t)
    return float(m[-1]) if m else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", default="/tmp/wave6-boxmap-build/rb3-native")
    ap.add_argument("--out", default=os.path.join(
        REPO, "docs", "native", "engine-arch-review-2026-07-05",
        "execution", "W3.2", "captures"))
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()
    if not os.path.exists(args.bin):
        print(f"FAIL: binary not found: {args.bin}"); return 2
    os.makedirs(args.out, exist_ok=True)

    plan = [("OFF", 1, {}), ("OFF", 2, {}),
            ("ON", 1, {"RB3_BOX_AMBIENT": "1"}),
            ("ON", 2, {"RB3_BOX_AMBIENT": "1"})]
    results = {}
    for flag, idx, env in plan:
        prefix = os.path.join(args.out, f"cap_{flag}_{idx}")
        print(f"[w32-ab] capturing {flag}#{idx} (env={env or 'none'})", flush=True)
        p = w21.boot_capture_gameplay(args.bin, env, prefix, args.verbose)
        if p is None:
            print(f"[w32-ab] FAIL capture {flag}#{idx}; see {prefix}.engine.log")
            return 2
        s = score(p)
        s["songMs"] = songms_from_log(prefix)
        results[f"{flag}_{idx}"] = {"png": os.path.relpath(p, REPO), **s}
        print(f"[w32-ab]   {flag}#{idx} luma={s['mean_luma']:.1f} "
              f"blow={s['pct_blowout']:.2f}% crush={s['pct_crush']:.2f}% "
              f"pink={s['pct_pink']:.2f}% songMs={s['songMs']}", flush=True)

    # side-by-side montage OFF_1 | ON_1 for eyeballing
    try:
        w21.make_side_by_side(
            [(results["OFF_1"]["png"], "OFF (scalar ambient)"),
             (results["ON_1"]["png"], "ON (RB3_BOX_AMBIENT cube)")],
            os.path.join(args.out, "boxambient_off_vs_on.png"),
            "W3.2 box-ambient prototype: venue OFF vs ON")
    except Exception as e:
        print(f"[w32-ab] montage skipped: {e}")

    scores_path = os.path.join(args.out, "scores.json")
    json.dump(results, open(scores_path, "w"), indent=2)
    print(f"[w32-ab] wrote {scores_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
