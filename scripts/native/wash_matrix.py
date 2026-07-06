#!/usr/bin/env python3
"""wash_matrix.py — Wave 7 Stage A.S3 (KEY=WASH): the 5-config isolation matrix.

Runs the stochastic venue-cam "wash" isolation matrix per the W2.1-flip-blocker
backlog protocol (STATUS.md §Backlog proposal — WASH). Unlike wash-measure.py (a
2-arm OFF/ON *placement-contract* A/B), this drives FIVE configs, ALL with the
placement contract at its DEFAULT (default-ON post-Wave-6) — no contract env is
pinned in any config. It reuses wash-measure.py's proven `capture_pinned`
primitive verbatim (songMs-pinned wide-venue shot; boots that overshoot the
window are discarded + re-run).

The 5 configs (per the Wave-7 kickoff Lane A + WAVE7_REVIEW A4):
  1. luma_on            RB3_PP_LUMA_CEILING=1                          (W3.3-fix ON baseline)
  2. highway_bloom_off  RB3_PP_LUMA_CEILING=1 RB3_HIGHWAY_BLOOM_OFF=1
  3. bloom_off          RB3_PP_LUMA_CEILING=1 RB3_BLOOM_OFF=1
  4. venue_light_off    RB3_PP_LUMA_CEILING=1 RB3_VENUE_LIGHT_OFF=1
  5. control_w33_off    (no env — W3.3-fix OFF, else default)         CONTROL

Configs 2-4 carry the W3.3 fix ON so they isolate the residual mechanism ON TOP
of the fix. Config 5 is the in-experiment baseline that answers "did the W3.3 fix
collapse the wash rate?" (config 1 vs config 5).

Round-robin interleaving (round r: one boot of each config, repeat N rounds)
controls for machine-state / thermal / cache drift over the ~1h run.

Each capture is scored with wash_score.score_image() -> class in
PINK|WHITE|NEARBLACK|NEUTRAL; a PINK-or-WHITE capture is "wash". Per-config wash
RATE = (#wash / N). We do NOT early-stop: every config needs its full N to
compare rates.

Usage:
  python3 scripts/native/wash_matrix.py \
      --bin native/build-agent-WASH/rb3-native \
      --raws /tmp/wash-matrix-captures \
      --out docs/native/engine-arch-review-2026-07-05/execution/WASH/measure \
      [--n 6] [--songms 21000] [--tol 250]
"""
import argparse
import importlib.util
import json
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))


def _load(mod, fname):
    spec = importlib.util.spec_from_file_location(mod, os.path.join(HERE, fname))
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


wm = _load("wash_measure", "wash-measure.py")   # capture_pinned, make_montage
ws = _load("wash_score", "wash_score.py")

# Config table: (key, env dict). Placement contract left at DEFAULT in all — no
# RB3_PLACEMENT_CONTRACT / _OFF pinned anywhere (per WAVE7_REVIEW A4).
CONFIGS = [
    ("luma_on",           {"RB3_PP_LUMA_CEILING": "1"}),
    ("highway_bloom_off", {"RB3_PP_LUMA_CEILING": "1", "RB3_HIGHWAY_BLOOM_OFF": "1"}),
    ("bloom_off",         {"RB3_PP_LUMA_CEILING": "1", "RB3_BLOOM_OFF": "1"}),
    ("venue_light_off",   {"RB3_PP_LUMA_CEILING": "1", "RB3_VENUE_LIGHT_OFF": "1"}),
    ("control_w33_off",   {}),
]


def log(m):
    print(f"[wash-matrix] {m}", flush=True)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--bin", default=os.path.join(REPO, "native", "build-agent-WASH", "rb3-native"))
    ap.add_argument("--raws", default="/tmp/wash-matrix-captures")
    ap.add_argument("--out", default=os.path.join(
        REPO, "docs", "native", "engine-arch-review-2026-07-05", "execution", "WASH", "measure"))
    ap.add_argument("--n", type=int, default=6, help="captures per config (>=6)")
    ap.add_argument("--songms", type=float, default=21000.0)
    ap.add_argument("--tol", type=float, default=250.0)
    ap.add_argument("--max-retries", type=int, default=8,
                    help="max discarded (overshoot/nav) boots per needed capture")
    ap.add_argument("--only", default=None, help="comma list of config keys to run (default all)")
    args = ap.parse_args()

    if not os.path.exists(args.bin):
        log(f"FAIL: binary not found: {args.bin}")
        return 2
    os.makedirs(args.raws, exist_ok=True)
    os.makedirs(args.out, exist_ok=True)
    lo, hi = args.songms - args.tol, args.songms + args.tol
    omax = hi

    configs = CONFIGS
    if args.only:
        want = set(args.only.split(","))
        configs = [c for c in CONFIGS if c[0] in want]

    scores = {k: [] for k, _ in configs}
    batch_log = []

    # round-robin: each round captures one boot of each config that still needs more
    for rnd in range(1, args.n + 1):
        for key, env in configs:
            if len(scores[key]) >= args.n:
                continue
            got = None
            for attempt in range(1, args.max_retries + 1):
                idx = len(scores[key]) + 1
                prefix = os.path.join(args.raws, f"{key}_{idx:02d}_try{attempt}")
                log(f"round {rnd} {key}#{idx} attempt {attempt}")
                png, info = wm.capture_pinned(args.bin, dict(env), prefix, lo, hi, omax)
                if png is not None:
                    m = ws.score_image(png)
                    m["config"] = key
                    m["songMs"] = float(info)
                    m["attempt"] = attempt
                    final = os.path.join(args.raws, f"{key}_{idx:02d}_{int(info)}.png")
                    try:
                        os.replace(png, final)
                        m["path"] = final
                    except Exception:
                        pass
                    scores[key].append(m)
                    batch_log.append(m)
                    got = m
                    log(f"  scored: class={m['wash_class']} mean={m['mean_luma']:.3f} "
                        f"hi={m['hi_frac']:.1f} lo={m['lo_frac']:.1f} pink={m['pink_frac']:.1f} "
                        f"songMs={m['songMs']:.0f}")
                    break
                else:
                    log(f"  discarded ({info})")
            if got is None:
                log(f"WARN: could not get a valid {key} capture after {args.max_retries} tries "
                    f"(round {rnd}); will retry next round")
            # persist progress after every capture (crash-safe)
            with open(os.path.join(args.out, "batch_log.json"), "w") as f:
                json.dump(batch_log, f, indent=2)

    # ---- summarize per-config wash rates + classes ----
    summary = {}
    for key, _ in configs:
        arr = scores[key]
        n = len(arr)
        washed = [m for m in arr if m["is_wash"]]
        classes = {}
        for m in arr:
            classes[m["wash_class"]] = classes.get(m["wash_class"], 0) + 1
        summary[key] = {
            "n": n,
            "wash_count": len(washed),
            "wash_rate": (len(washed) / n) if n else None,
            "classes": classes,
            "mean_luma": [round(m["mean_luma"], 3) for m in arr],
            "pink_frac": [round(m["pink_frac"], 1) for m in arr],
            "captures": arr,
        }

    result = {
        "songms_window": [lo, hi],
        "n_target": args.n,
        "bin": args.bin,
        "configs": [k for k, _ in configs],
        "summary": summary,
    }
    with open(os.path.join(args.out, "matrix.json"), "w") as f:
        json.dump(result, f, indent=2)

    # ---- print a compact table ----
    log("=" * 72)
    log(f"{'config':<20} {'n':>3} {'wash':>5} {'rate':>6}  classes")
    for key, _ in configs:
        s = summary[key]
        rate = f"{s['wash_rate']:.2f}" if s["wash_rate"] is not None else "n/a"
        cls = " ".join(f"{k}:{v}" for k, v in sorted(s["classes"].items()))
        log(f"{key:<20} {s['n']:>3} {s['wash_count']:>5} {rate:>6}  {cls}")
    log("=" * 72)

    # ---- montage: up to 3 representative captures per config (wash-first) ----
    try:
        entries = []
        for key, _ in configs:
            arr = scores[key]
            washed = [m for m in arr if m["is_wash"]]
            neutral = [m for m in arr if not m["is_wash"]]
            picked = (washed[:2] + neutral[:1])[:2] if arr else []
            for m in picked:
                entries.append((m["path"], f"{key}\n{m['wash_class']} L{m['mean_luma']:.2f}\n@{int(m['songMs'])}ms"))
        out_m = os.path.join(args.out, "montage.png")
        wm.make_montage(entries, [], out_m, "WASH A.S3 — 5-config isolation matrix (songMs %d-%d)" % (int(lo), int(hi)))
        log(f"montage -> {out_m}")
    except Exception as e:
        log(f"NOTE: montage failed non-fatally: {e}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
