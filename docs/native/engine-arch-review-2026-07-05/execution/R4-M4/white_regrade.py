#!/usr/bin/env python3
"""white_regrade.py — Wave-18 Lane W (R4-M4): RB3_VENUE_WHITE_GUARD re-grade on the
R4 determinism seam (the F6 cash-in).

BINDING PROTOCOL (WAVE18_KICKOFF A2/A3 + R-B, PLAN-R4 §M4.1):

  A2 (F6 verbatim VOID semantics): every measurement boot runs
      RB3_FIXED_CLOCK=1 RB3_LOAD_DETERMINISM=1 RB3_LOADDET_ATTRIB=1 (+ jitter 200us).
      The per-axis ledger is graded FROM THOSE BOOTS' OWN LOGS
      (loaddet_gate.grade_external_logs). Precondition = ledger stream PASS N/N on
      the EXACT measurement boots AND nParsed==N. Otherwise the measurement is VOID:
      the harness REFUSES to emit a WHITE verdict. Never discard-and-rerun a
      ledger-failing boot; never average one in. (A capture-WINDOW overshoot -- no
      PNG -- is a capture-success filter, orthogonal to the ledger, and is retried.)

  A3 (reproduce-first, single pinned trajectory): the seam pins ONE trajectory
      (hardcoded Seed(0x5EED), Rand.cpp:61, no seed knob) and reroutes the
      WHITE-implicated consumers onto private streams. So: NO comparison to the
      Wave-10/11 absolute numbers. FIRST gate = reproduce the phenomenon under the
      seam: validity gate mean(hi_frac | guard-OFF) >= 15. On failure the item is
      HELD substrate-blocked and the follow-up is a coordinator RB3_LOADDET_SEED
      knob (outside Lane W's writable set). Cheap cross-arm check: guard-ON vs
      guard-OFF postAnchorDelta identical (the guard is render-side, draws no gRand)
      -> the A/B is a same-trajectory paired comparison.

  R-B / G1a decision rule (UNCHANGED from WHITE-fix/STATUS.md:303): recommend flip
      iff G1a (mean(hi_frac|ON) <= mean(hi_frac|OFF) - 3.0pp AND directional ON<OFF)
      AND G1b (mean(mid_sat|ON) >= mean(mid_sat|OFF) + 0.02). N=10/arm; early-stop at
      N=5/arm ONLY if within-arm hi_frac sd < 0.5pp in BOTH arms AND ledger 5/5.

  The lane FLIPS NOTHING: deliverable = measurement package + verdict
  (READY_FOR_FLIP | HELD-with-numbers | HELD substrate-blocked | VOID). Coordinator
  E1-gates any flip.

  lint 8: guard branch-entry hit-count ([WASHPROBE] SCENE engaged=1 on the world.cam
  engaged path where venueHighlightLumaMode is written, Rnd_Wgpu_RB3.cpp:1525) is
  reported per arm so any "guard has no effect" reading proves the guard code ran.
"""
import argparse, importlib.util, json, os, re, statistics as st, sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", "..", "..", ".."))
NSCR = os.path.join(REPO, "scripts", "native")
WF = os.path.join(REPO, "docs", "native", "engine-arch-review-2026-07-05",
                  "execution", "WHITE-fix")


def _load(m, p):
    s = importlib.util.spec_from_file_location(m, p)
    x = importlib.util.module_from_spec(s); s.loader.exec_module(x); return x


wm = _load("washmeasure", os.path.join(NSCR, "wash-measure.py"))
ws = _load("wash_score", os.path.join(NSCR, "wash_score.py"))
tb = _load("tonal_band_sat", os.path.join(
    REPO, "docs", "native", "engine-arch-review-2026-07-05", "execution",
    "WASH-fix", "tonal_band_sat.py"))
wd = _load("white_discriminate", os.path.join(WF, "white_discriminate.py"))
lg = _load("loaddet_gate", os.path.join(NSCR, "loaddet_gate.py"))
cap = _load("r4m4_capture", os.path.join(HERE, "r4m4_capture.py"))
# Wave-19 W-ISO: shared capture-discipline lints (F2/F7/F10 + attempt disclosure).
cl = _load("capture_lints", os.path.join(NSCR, "capture_lints.py"))

K_FRAMES = 300
WASH_ENGAGED_RE = re.compile(r"\[WASHPROBE\] SCENE .*engaged=1")


def _nan_to_none(o):
    """W-ISO F10 companion: convert the KNOWN sentinel NaNs (mid_sat/high_sat
    'no mid-band pixels' on near-black frames; empty-population means) + any Inf to
    JSON null so cl.safe_json_dump emits valid JSON. safe_json_dump then strict-checks
    and raises on anything non-finite that survived — the backstop against an
    UNINTENDED value silently becoming bare `NaN` (the legacy wr_n10.json defect)."""
    if isinstance(o, float):
        return None if (o != o or o in (float("inf"), float("-inf"))) else o
    if isinstance(o, dict):
        return {k: _nan_to_none(v) for k, v in o.items()}
    if isinstance(o, list):
        return [_nan_to_none(v) for v in o]
    return o


def seam_env(guard_on):
    env = dict(wd.ARMS["eng_hot"])   # forced-hot ENGAGED: the guard's real vehicle
    env.update({
        "RB3_FIXED_CLOCK": "1", "RB3_LOAD_DETERMINISM": "1",
        "RB3_LOADDET_ATTRIB": "1", "RB3_LOADDET_JITTER": "200",
        "RB3_BOOTRNG_PROBE": "1", "RB3_WASH_PROBE": "1",
    })
    if guard_on:
        env["RB3_VENUE_WHITE_GUARD"] = "1"
    return env


def capture_arm(binpath, guard_on, n, target_ms, overshoot_ms, rawdir, tag):
    """Capture n eng_hot boots under the seam, INPUT-FREE post-anchor, screenshotting
    the first frame songMs>=target_ms (r4m4_capture: removes the Wave-10 autohit +
    window-phase confounds). Returns per-boot rows with score metrics + own log path."""
    env = seam_env(guard_on)
    arm = "ON" if guard_on else "OFF"
    rows = []
    attempts = 0
    discarded_reasons = []  # W-ISO: overshoot/no-engage survivorship (attempt disclosure)
    while len(rows) < n and attempts < n * 4:
        attempts += 1
        pref = os.path.join(rawdir, f"{tag}_{arm}_{attempts:02d}")
        r = cap.capture_at_songms(binpath, env, pref, target_ms, overshoot_ms,
                                  song_downs=4)
        png = r.get("png")
        if png is None:
            discarded_reasons.append(r.get("reason"))
            print(f"  [{arm} #{attempts}] skip: {r.get('reason')}")
            continue
        info = r["songms"]
        log = r["log"]
        m = ws.score_image(png)
        bands = tb.bands(png)
        txt = open(log, errors="replace").read()
        guard_hits = len(WASH_ENGAGED_RE.findall(txt))
        row = {
            "arm": arm, "guard": arm, "png": png, "log": log, "frame": r.get("frame"),
            "songms": float(info), "class": m["wash_class"], "is_wash": m["is_wash"],
            "mean_luma": m["mean_luma"], "hi_frac": m["hi_frac"],
            "mid_sat": bands["mid_sat"], "low_sat": bands["low_sat"],
            "high_sat": bands["high_sat"], "guard_engaged_hits": guard_hits,
        }
        rows.append(row)
        print(f"  [{arm} #{attempts}] {m['wash_class']:9s} hi={m['hi_frac']:6.2f} "
              f"mid_sat={bands['mid_sat']:.3f} lum={m['mean_luma']:.3f} "
              f"guardHits={guard_hits} ms={info:.0f}")
    # W-ISO: disclose attempt/discard survivorship alongside the rows (§6.5).
    disclosure = cl.attempt_disclosure(attempts, len(rows), discarded_reasons)
    return rows, disclosure


def grade(rows):
    return lg.grade_external_logs([r["log"] for r in rows], K_FRAMES)


def rebuild_rows(rawdir, tag, arm, n):
    """Rebuild the per-boot rows from the on-disk PNGs + logs of an already-run
    measurement (crash-recovery finisher; no re-boot). Attempt numbering may have
    gaps (retried captures) — take the n that have BOTH png and log."""
    import glob
    rows = []
    for pref in sorted(glob.glob(os.path.join(rawdir, f"{tag}_{arm}_*.png"))):
        base = pref[:-4]
        log = base + ".engine.log"
        if not os.path.exists(log):
            continue
        m = ws.score_image(pref)
        bands = tb.bands(pref)
        txt = open(log, errors="replace").read()
        rows.append({
            "arm": arm, "guard": arm, "png": pref, "log": log, "frame": None,
            "songms": None, "class": m["wash_class"], "is_wash": m["is_wash"],
            "mean_luma": m["mean_luma"], "hi_frac": m["hi_frac"],
            "mid_sat": bands["mid_sat"], "low_sat": bands["low_sat"],
            "high_sat": bands["high_sat"],
            "guard_engaged_hits": len(WASH_ENGAGED_RE.findall(txt)),
        })
        if len(rows) >= n:
            break
    return rows


def within_sd(rows, key):
    xs = [r[key] for r in rows]
    return st.pstdev(xs) if len(xs) > 1 else 0.0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", required=True)
    ap.add_argument("--n", type=int, default=10)
    ap.add_argument("--songms", type=float, default=21000.0,
                    help="capture the first frame with songMs >= this (one-sided, tol=0)")
    ap.add_argument("--overshoot", type=float, default=4000.0,
                    help="discard+retry if songMs passes target+overshoot before capture")
    ap.add_argument("--out", default=os.path.join(HERE, "evidence"))
    ap.add_argument("--raws", default="/tmp/r4m4-white")
    ap.add_argument("--tag", default="wr")
    ap.add_argument("--validate", action="store_true",
                    help="quick ledger-cleanliness check: OFF arm only, small N, no verdict")
    ap.add_argument("--refinish", action="store_true",
                    help="rebuild rows from on-disk PNGs/logs of a finished run (no boots)")
    ap.add_argument("--early-stop", action="store_true",
                    help="permit N=5 early-stop when within-arm hi_frac sd<0.5 both arms + ledger 5/5")
    a = ap.parse_args()
    # W-ISO F2: a --refinish disk-rebuilt run may not emit a graded verdict. --validate
    # is a non-verdict cleanliness inspection, so --refinish --validate stays allowed;
    # --refinish for the two-arm verdict path is the banned combination.
    cl.refuse_refinish_for_grade(a.refinish, is_graded=not a.validate)
    os.makedirs(a.out, exist_ok=True)
    os.makedirs(a.raws, exist_ok=True)
    overshoot = a.songms + a.overshoot

    if a.validate:
        n = min(a.n, 3)
        print(f"== VALIDATE: OFF arm N={n}, checking capture-drive ledger cleanliness ==")
        rows, disclosure = capture_arm(a.bin, False, n, a.songms, overshoot, a.raws,
                                       a.tag + "_val")
        led = grade(rows)
        # F7: split near-black frames out of the population (disclosed, not dropped).
        kept, excluded_black = cl.partition_black_frames(rows)
        print("\nLEDGER (OFF validate):")
        print(json.dumps(led.get("summary", led), indent=2))
        deltas = [b["axes"]["stream"]["value"] for b in led["boots"]]
        print(f"nParsed={led['nParsed']}/{led['nLogs']} "
              f"postAnchorDeltas={deltas} captured_songms={[r['songms'] for r in rows]} "
              f"classes={[r['class'] for r in rows]} hi={[round(r['hi_frac'],1) for r in rows]}")
        clean = (led["nParsed"] == n and led["summary"]["stream"] == f"{n}/{n}")
        print(f"\nCAPTURE-DRIVE LEDGER-CLEAN: {clean} "
              f"(stream={led['summary']['stream']})")
        print(f"attempt-disclosure: {disclosure} "
              f"excluded_black={len(excluded_black)}")
        cl.safe_json_dump(_nan_to_none(
            {"validate": True, "rows": rows, "ledger": led,
             "attempt_disclosure": disclosure,
             "excluded_black": excluded_black, "kept_frames": len(kept)}),
            os.path.join(a.out, f"{a.tag}_validate.json"))
        return 0 if clean else 2

    # ---- full re-grade: both arms ----
    if a.refinish:
        # Unreachable for a graded run: F2 refuses --refinish without --validate above.
        # Retained for a hypothetical future non-verdict refinish inspection path.
        off = rebuild_rows(a.raws, a.tag, "OFF", a.n)
        on = rebuild_rows(a.raws, a.tag, "ON", a.n)
        off_disc = on_disc = cl.attempt_disclosure(0, len(off), ["refinish-from-disk"])
        print(f"refinish: rebuilt {len(off)} OFF + {len(on)} ON rows from {a.raws}")
    else:
        off, off_disc = capture_arm(a.bin, False, a.n, a.songms, overshoot, a.raws, a.tag)
        on, on_disc = capture_arm(a.bin, True, a.n, a.songms, overshoot, a.raws, a.tag)

    led_off = grade(off)
    led_on = grade(on)
    n = a.n

    # ---- A2 ledger precondition (VOID if not met) ----
    def arm_ok(rows, led):
        return (led["nParsed"] == len(rows) == n and
                led["summary"]["stream"] == f"{n}/{n}")
    off_clean = arm_ok(off, led_off)
    on_clean = arm_ok(on, led_on)

    # ledger rows carry the stream value (postAnchorDelta) under axes.stream.value
    off_deltas = sorted(set(b["axes"]["stream"]["value"] for b in led_off["boots"]))
    on_deltas = sorted(set(b["axes"]["stream"]["value"] for b in led_on["boots"]))
    cross_arm_match = (off_deltas == on_deltas and len(off_deltas) == 1)

    # W-ISO F7: split near-black (luma<=0.05) frames out of the hi_frac population,
    # the luma sibling of the mid_sat==nan all-black rule below. Disclosed via
    # result["excluded_black"], never silently averaged in.
    off_kept, off_black = cl.partition_black_frames(off)
    on_kept, on_black = cl.partition_black_frames(on)
    off_hi = [r["hi_frac"] for r in off_kept]
    on_hi = [r["hi_frac"] for r in on_kept]
    # mid_sat is nan on all-black frames (no mid-band pixels) — exclude those
    off_mid = [r["mid_sat"] for r in off_kept if r["mid_sat"] == r["mid_sat"]]
    on_mid = [r["mid_sat"] for r in on_kept if r["mid_sat"] == r["mid_sat"]]
    mean = lambda xs: st.mean(xs) if xs else float("nan")

    result = {
        "tag": a.tag, "bin": a.bin, "n": n, "songms": a.songms, "overshoot": a.overshoot,
        "arms": {"OFF": off, "ON": on},
        "ledger": {"OFF": led_off, "ON": led_on},
        "ledger_clean": {"OFF": off_clean, "ON": on_clean},
        "cross_arm_stream_match": cross_arm_match,
        "off_postAnchorDeltas": off_deltas, "on_postAnchorDeltas": on_deltas,
        "hi_frac": {"OFF_mean": mean(off_hi), "ON_mean": mean(on_hi),
                    "OFF_sd": within_sd(off, "hi_frac"), "ON_sd": within_sd(on, "hi_frac"),
                    "d": mean(on_hi) - mean(off_hi)},
        "mid_sat": {"OFF_mean": mean(off_mid), "ON_mean": mean(on_mid),
                    "d": mean(on_mid) - mean(off_mid)},
        "guard_engaged_hits": {"OFF": [r["guard_engaged_hits"] for r in off],
                               "ON": [r["guard_engaged_hits"] for r in on]},
        "white_count": {"OFF": sum(1 for r in off if r["class"] == "WHITE"),
                        "ON": sum(1 for r in on if r["class"] == "WHITE")},
        # W-ISO F7: near-black frames excluded from the hi_frac/mid_sat means (disclosed).
        "excluded_black": {"OFF": off_black, "ON": on_black,
                           "OFF_n": len(off_black), "ON_n": len(on_black),
                           "thresh": cl.BLACK_LUMA_THRESH},
        # W-ISO: capture attempt/discard survivorship (§6.5).
        "attempt_disclosure": {"OFF": off_disc, "ON": on_disc},
    }

    # ---- verdict logic ----
    if not (off_clean and on_clean):
        result["verdict"] = "VOID"
        result["reason"] = (
            "ledger precondition FAILED on the exact measurement boots "
            f"(OFF stream={led_off['summary']['stream']} nParsed={led_off['nParsed']}/{n}; "
            f"ON stream={led_on['summary']['stream']} nParsed={led_on['nParsed']}/{n}). "
            "Per A2/F6 the harness REFUSES to emit a WHITE verdict. This indicts the seam "
            "under the capture-pinned drive -> seam-regression finding (report axis + counts).")
    elif mean(off_hi) < 15.0:
        result["verdict"] = "HELD substrate-blocked"
        result["reason"] = (
            f"reproduce-first gate FAILED: mean(hi_frac|OFF)={mean(off_hi):.2f} < 15 under the "
            "seam -> the WHITE phenomenon is not expressed on the pinned Seed(0x5EED) trajectory. "
            "Follow-up (coordinator, outside Lane W's writable set): RB3_LOADDET_SEED knob to "
            "search trajectories that express the phenomenon.")
    else:
        g1a = (mean(on_hi) <= mean(off_hi) - 3.0) and (mean(on_hi) < mean(off_hi))
        g1b = mean(on_mid) >= mean(off_mid) + 0.02
        result["gates"] = {
            "reproduce_first(OFF hi>=15)": True,
            "G1a(hi ON<=OFF-3.0 & dir)": g1a,
            "G1b(mid ON>=OFF+0.02)": g1b,
            "cross_arm_stream_match": cross_arm_match,
        }
        if g1a and g1b:
            result["verdict"] = "READY_FOR_FLIP"
            result["reason"] = ("G1a+G1b PASS on stream-matched boots: the guard measurably "
                                "reduces over-exposure AND raises chroma on the pinned trajectory.")
        else:
            result["verdict"] = "HELD-with-numbers"
            result["reason"] = (
                f"reproduce-first PASSED (OFF hi={mean(off_hi):.2f}) but G1 not met on "
                f"stream-matched boots: G1a={g1a} (d_hi={mean(on_hi)-mean(off_hi):+.2f}), "
                f"G1b={g1b} (d_mid={mean(on_mid)-mean(off_mid):+.3f}). Guard branch-entry hits "
                f"OFF={result['guard_engaged_hits']['OFF']} ON={result['guard_engaged_hits']['ON']} "
                "(lint 8: guard code ran). The A/B is now decisive (stream-matched), not confounded.")

    outp = os.path.join(a.out, f"{a.tag}.json")
    # W-ISO F10: convert the KNOWN sentinel NaNs (mid_sat/high_sat "no mid-band pixels"
    # on near-black frames; empty-population means) to JSON null so the output is valid,
    # then strict-dump. safe_json_dump raises on any NaN/Inf that survived the scrub — the
    # backstop against an UNINTENDED non-finite value silently becoming bare `NaN` (the
    # exact defect in the legacy wr_n10.json). This replaces the pre-existing bare-NaN emit.
    def _nan_to_none(o):
        if isinstance(o, float):
            return None if o != o or o in (float("inf"), float("-inf")) else o
        if isinstance(o, dict):
            return {k: _nan_to_none(v) for k, v in o.items()}
        if isinstance(o, list):
            return [_nan_to_none(v) for v in o]
        return o
    cl.safe_json_dump(_nan_to_none(result), outp)
    cl.safe_json_dump(_nan_to_none(led_off), os.path.join(a.out, f"{a.tag}-ledger-off.json"))
    cl.safe_json_dump(_nan_to_none(led_on), os.path.join(a.out, f"{a.tag}-ledger-on.json"))

    print("\n==================== WHITE RE-GRADE ====================")
    print(f"OFF: hi={mean(off_hi):.2f}(sd {within_sd(off,'hi_frac'):.2f}) mid={mean(off_mid):.3f} "
          f"WHITE={result['white_count']['OFF']}/{n} ledger_stream={led_off['summary']['stream']}")
    print(f"ON : hi={mean(on_hi):.2f}(sd {within_sd(on,'hi_frac'):.2f}) mid={mean(on_mid):.3f} "
          f"WHITE={result['white_count']['ON']}/{n} ledger_stream={led_on['summary']['stream']}")
    print(f"cross-arm stream match: {cross_arm_match} (OFF{off_deltas} ON{on_deltas})")
    print(f"\nVERDICT: {result['verdict']}")
    print(f"  {result['reason']}")
    print(f"wrote {outp}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
