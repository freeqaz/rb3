#!/usr/bin/env python3
"""white_discriminate.py — Wave 9 Lane B (WHITE-fix) force-reproduce + discriminate.

Goal (WAVE9_KICKOFF A4): (1) a DETERMINISTIC / >=50%-rate WHITE reproducer for the
engaged-venue over-exposure, and (2) the PP_OFF scene-vs-composite discriminator.

Mechanism recap (from WASH-fix S1/S2/S3 + source read):
  - The scene renders into a UNORM (RGBA8) intermediate (RB3PostProc.cpp:155):
    any hot (>1.0) venue lighting is CLAMPED to white at the write, before any grade.
  - The engaged venue path (Rnd_Wgpu_RB3.cpp:~1520) sums real point/dir lights with
    per-light exposure knobs RB3_VENUE_POINT_EXPOSURE (def 0.70) / RB3_VENUE_DIR_EXPOSURE
    (def 0.80), each clamped 1.8 / 1.5 in the shader-fill. A hot shot (several point
    lights near cam) already sums >1.0 at default -> the stochastic ~1/6 WHITE.
  - Cranking the exposure knobs pins every venue light to its clamp -> a DETERMINISTIC
    hot engaged scene = a "forced-hot probe" (A4 sanctions this).

Discriminator: PP_OFF renders the scene DIRECT to the framebuffer (no intermediate,
no composite grade; Rnd_Wgpu_RB3.cpp:1834). If the forced-hot scene reads WHITE under
PP_OFF too -> the over-exposure is SCENE-SIDE (fix belongs in Rnd_Wgpu_RB3.cpp, Lane A).
If PP_OFF reads colored/moderate while PP_ON reads WHITE -> COMPOSITE GAIN (fix is mine).

Arms (all keep chroma-preserve at its default-ON; RB3_PP_LUMA_CEILING UNSET per A7):
  default            {}                                  natural engaged baseline
  forced_hot         {POINT_EXPOSURE, DIR_EXPOSURE hot}  the reproducer (PP ON)
  forced_hot_ppoff   forced_hot + RB3_PP_OFF=1           the discriminator
  default_ppoff      {RB3_PP_OFF=1}                      natural + no-composite control
  venue_light_off    {RB3_VENUE_LIGHT_OFF=1}             known deterministic flood (control)

Writes measure/<tag>.json (per-boot rows: class, mean_luma, hi_frac, tonal bands,
engagement tail) and keeps PNGs under --raws.

Usage:
  python3 white_discriminate.py --bin <rb3-native> --songms 21000 --n 5 \
      --arms default,forced_hot,forced_hot_ppoff,default_ppoff --out measure --raws /tmp/whitefix-caps
"""
import argparse, importlib.util, json, os, collections

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", "..", "..", ".."))
NSCR = os.path.join(REPO, "scripts", "native")

def _load(mod, path):
    spec = importlib.util.spec_from_file_location(mod, path)
    m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m); return m

wm = _load("wash_measure", os.path.join(NSCR, "wash-measure.py"))
ws = _load("wash_score", os.path.join(NSCR, "wash_score.py"))
tb = _load("tonal_band_sat", os.path.join(HERE, "..", "WASH-fix", "tonal_band_sat.py"))
wp = _load("wash_probe_run", os.path.join(HERE, "..", "WASH-fix", "wash_probe_run.py"))

# Forced-hot exposure: pin every venue light to its shader clamp (1.5 dir / 1.8 pt)
# and lift ambient so the engaged scene reliably exceeds 1.0 -> UNORM white clamp.
HOT = {"RB3_VENUE_POINT_EXPOSURE": "3.0", "RB3_VENUE_DIR_EXPOSURE": "3.0",
       "RB3_VENUE_AMBIENT_CLAMP": "0.6"}

ARMS = {
    "default":          {},
    "forced_hot":       dict(HOT),
    "forced_hot_ppoff": dict(HOT, **{"RB3_PP_OFF": "1"}),
    "default_ppoff":    {"RB3_PP_OFF": "1"},
    "venue_light_off":  {"RB3_VENUE_LIGHT_OFF": "1"},
    "venue_light_off_ppoff": {"RB3_VENUE_LIGHT_OFF": "1", "RB3_PP_OFF": "1"},
    # a milder forced-hot to check the over-exposure is graded, not just clipped hard
    "forced_hot_mid":       {"RB3_VENUE_POINT_EXPOSURE": "1.6", "RB3_VENUE_DIR_EXPOSURE": "1.6"},
    "forced_hot_mid_ppoff": {"RB3_VENUE_POINT_EXPOSURE": "1.6", "RB3_VENUE_DIR_EXPOSURE": "1.6", "RB3_PP_OFF": "1"},
    # ENGAGED (real env, NOT the flood): lift ambient floor + exposure so the engaged
    # venue path renders bright regardless of which (dark) song venue is pinned. Stays
    # on the sVenueLightEnabled ENGAGED branch (final_engaged=1) -> the true "hot
    # engaged venue" the Wave-8 disclosure names.
    "eng_hot":       {"RB3_VENUE_AMBIENT_FLOOR": "0.55", "RB3_VENUE_AMBIENT_CLAMP": "1.0",
                      "RB3_VENUE_POINT_EXPOSURE": "2.5", "RB3_VENUE_DIR_EXPOSURE": "2.5"},
    "eng_hot_ppoff": {"RB3_VENUE_AMBIENT_FLOOR": "0.55", "RB3_VENUE_AMBIENT_CLAMP": "1.0",
                      "RB3_VENUE_POINT_EXPOSURE": "2.5", "RB3_VENUE_DIR_EXPOSURE": "2.5", "RB3_PP_OFF": "1"},
}

def run_arm(binp, arm, env, n, lo, hi, overshoot, rawdir, tag, song=4):
    rows = []
    got = 0
    attempts = 0
    while got < n and attempts < n * 3:
        attempts += 1
        pref = os.path.join(rawdir, f"{tag}_{arm}_{attempts:02d}")
        e = dict(env); e["RB3_WASH_PROBE"] = "1"
        png, info = wm.capture_pinned(binp, e, pref, lo, hi, overshoot, song_downs=song, verbose=False)
        if png is None:
            print(f"  [{arm} #{attempts}] skip: {info}")
            continue
        got += 1
        m = ws.score_image(png)
        bands = tb.bands(png)
        log = pref + ".engine.log"
        plog = wp.parse_log(log)
        row = {
            "arm": arm, "png": png, "songms": float(info),
            "class": m["wash_class"], "is_wash": m["is_wash"],
            "mean_luma": m["mean_luma"], "hi_frac": m["hi_frac"],
            "lo_frac": m["lo_frac"], "pink_frac": m["pink_frac"],
            "p50": m["p50"], "p95": m["p95"],
            "mid_sat": bands["mid_sat"], "low_sat": bands["low_sat"],
            "high_sat": bands["high_sat"], "mean_val": bands["mean_val"],
            "tail_engaged": plog.get("tail_engaged"), "tail_missed": plog.get("tail_missed"),
            "tail_miss_reasons": plog.get("tail_miss_reasons"),
            "tail_envs": plog.get("tail_envs"), "final_env": plog.get("final_env"),
            "final_engaged": plog.get("final_engaged"), "pp": plog.get("pp"),
        }
        rows.append(row)
        print(f"  [{arm} #{attempts}] {m['wash_class']:9s} mean={m['mean_luma']:.3f} hi={m['hi_frac']:5.2f} "
              f"mid_sat={bands['mid_sat']:.3f} eng={plog.get('tail_engaged')}/{plog.get('tail_missed')} "
              f"env={plog.get('final_env')} ms={info:.0f}")
    return rows

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", required=True)
    ap.add_argument("--songms", type=float, default=21000.0)
    ap.add_argument("--tol", type=float, default=1500.0)
    ap.add_argument("--n", type=int, default=5)
    ap.add_argument("--arms", default="default,forced_hot,forced_hot_ppoff,default_ppoff")
    ap.add_argument("--out", default=os.path.join(HERE, "measure"))
    ap.add_argument("--raws", default="/tmp/whitefix-caps")
    ap.add_argument("--tag", default="disc")
    ap.add_argument("--song", type=int, default=4)
    ap.add_argument("--shots", default=None,
                    help="comma-separated shot candidates to pin (first that resolves). "
                         "Overrides wash-measure's crowd-first SHOT_CANDIDATES.")
    args = ap.parse_args()

    if args.shots:
        wm.SHOT_CANDIDATES = [s.strip() for s in args.shots.split(",") if s.strip()]
        print("SHOT_CANDIDATES overridden ->", wm.SHOT_CANDIDATES)

    os.makedirs(args.out, exist_ok=True)
    os.makedirs(args.raws, exist_ok=True)
    lo, hi = args.songms - args.tol, args.songms + args.tol
    overshoot = hi + 6000

    allrows = []
    for arm in args.arms.split(","):
        arm = arm.strip()
        if arm not in ARMS:
            print(f"unknown arm {arm}"); continue
        print(f"== arm {arm} env={ARMS[arm]} ==")
        rows = run_arm(args.bin, arm, ARMS[arm], args.n, lo, hi, overshoot, args.raws, args.tag, song=args.song)
        allrows += rows

    outp = os.path.join(args.out, f"{args.tag}.json")
    with open(outp, "w") as f:
        json.dump(allrows, f, indent=2)
    print("wrote", outp)

    # summary
    print("\n=== SUMMARY (per arm) ===")
    by = collections.defaultdict(list)
    for r in allrows:
        by[r["arm"]].append(r)
    for arm, rs in by.items():
        nwhite = sum(1 for r in rs if r["class"] == "WHITE")
        nwash = sum(1 for r in rs if r["is_wash"])
        ml = [r["mean_luma"] for r in rs]
        hi_ = [r["hi_frac"] for r in rs]
        ms = [r["mid_sat"] for r in rs]
        import statistics as st
        print(f"{arm:20s} N={len(rs)} WHITE={nwhite} wash={nwash} "
              f"mean_luma={st.mean(ml):.3f} hi_frac={st.mean(hi_):.2f} mid_sat={st.mean(ms):.3f} "
              f"classes={[r['class'] for r in rs]}")

if __name__ == "__main__":
    main()
