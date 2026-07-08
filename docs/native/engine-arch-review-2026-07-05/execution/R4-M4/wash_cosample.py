#!/usr/bin/env python3
"""wash_cosample.py — Wave-19 Lane F (T1-FRAMETRACE) wash co-sampler v2.

Rewrite of the Wave-18 Lane W wash per-FX co-sampling instrument. v1 (BOOTRNG
backlog item 3) FALSELY reported instrument_validated=true on the committed
natural-venue capture (evidence/wash_natural.json) with AUC 0.000 — a tie-blind
argsort artifact over a covariate that had only TWO distinct values across 89
shots (a stale, collapsed join). v2 fixes the three defects, in this order
(PLAN §3, F1 correction, BINDING):

  1. PER-FRAME JOIN (not a stale trailing poll-window). Each screenshot is joined
     to ITS OWN frame's row of the T1 per-frame timeline table
     {frame -> (fx_emit, light_state, songMs)} — the same per-frame ledger
     emitTimeline reads — instead of a big window(fx_draws, fr, win) trailing sum
     that collapses to 2 clusters when fx_draws is bursty.
  2. >=N-DISTINCT-COVARIATE REFUSAL (the lint that refuses wash_natural.json).
     Before ANY AUC: count distinct values per COVARIATE. If any covariate has
     < N_MIN_DISTINCT (default 5), emit verdict DEGENERATE + instrument_validated
     False + an explicit refused:{covariate:n_distinct} block, and DO NOT compute
     a separation claim on a degenerate covariate.
  3. MIDRANK AUC/U. Ties get average (midrank) ranks so ties yield the true AUC
     (~0.32) not 0.000.
  4. LIGHT-POSITION AMPLITUDE covariate — added ONLY AFTER 1-3 (F1 ordering), and
     subject to the same >=N-distinct gate.

COVARIATE vs OUTCOME sets (PLAN_REVIEW R3, BINDING):
  COVARIATES (join inputs, subject to the distinct-gate): fx_emit_win (per-frame
    emission), light_changes_win (per-frame light state), songms, frame,
    light_pos_amp (step 4).
  OUTCOMES (labels/scores, EXEMPT from the distinct-gate — an 87-distinct hi_frac
    is healthy): hi_frac, mean_luma, class.
  The 2-distinct `frame` field on wash_natural.json is itself the smoking gun of
  the stale join (89 shots on 2 frames); v2 refusing on it is correct.

FAIL-RED (3), BINDING: `--regrade evidence/wash_natural.json` must return verdict
  DEGENERATE, instrument_validated False, refused superset {fx_emit_win:2,
  light_changes_win:2} — a mechanical, offline regression test (no boot).

capture_lints (A6): imports scripts/native/capture_lints.py (Lane I) for
  safe_json_dump (F10 allow_nan=False), partition_black_frames (F7), and
  attempt_disclosure. Landed; imported directly.
"""
import argparse, importlib.util, json, math, os, sys
import statistics as st
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", "..", "..", ".."))
NSCR = os.path.join(REPO, "scripts", "native")
WF = os.path.join(REPO, "docs", "native", "engine-arch-review-2026-07-05",
                  "execution", "WHITE-fix")


def _load(m, p):
    s = importlib.util.spec_from_file_location(m, p)
    x = importlib.util.module_from_spec(s); s.loader.exec_module(x); return x


# capture_lints is Lane I's shared module (landed). Direct import; a tiny local
# fallback keeps --regrade runnable if the file is ever absent (A6 handshake).
try:
    cl = _load("capture_lints", os.path.join(NSCR, "capture_lints.py"))
    _safe_json_dump = cl.safe_json_dump
    _partition_black = cl.partition_black_frames
    _attempt_disclosure = cl.attempt_disclosure
    CaptureLintError = cl.CaptureLintError
    _CAPTURE_LINTS = "scripts/native/capture_lints.py"
except Exception as _e:  # pragma: no cover — fallback only if Lane I file missing
    class CaptureLintError(Exception):
        pass

    def _safe_json_dump(obj, path, indent=2):
        json.dump(obj, open(path, "w"), indent=indent, allow_nan=False)

    def _partition_black(rows, luma_key="mean_luma", thresh=0.05):
        kept = [r for r in rows if isinstance(r.get(luma_key), (int, float))
                and not (isinstance(r.get(luma_key), float) and math.isnan(r[luma_key]))
                and r.get(luma_key, 0) > thresh]
        excl = [r for r in rows if r not in kept]
        return kept, excl

    def _attempt_disclosure(attempts, captured, reasons):
        return {"attempts": int(attempts), "captured": int(captured),
                "discarded": int(attempts) - int(captured),
                "discarded_reasons": list(reasons or [])}
    _CAPTURE_LINTS = f"MISSING (fallback: {_e})"


# ---------------------------------------------------------------------------
# Covariate / outcome sets (R3) + the >=N-distinct refusal gate.
# ---------------------------------------------------------------------------
COVARIATES = ["fx_emit_win", "light_changes_win", "songms", "frame"]
OUTCOMES = ["hi_frac", "mean_luma", "class"]
N_MIN_DISTINCT = 5   # >= the "2 clusters" degeneracy with margin


def distinct_counts(shots, keys):
    out = {}
    for k in keys:
        vals = [s[k] for s in shots if k in s and s[k] is not None]
        out[k] = len(set(vals))
    return out


def distinct_gate(shots, covariates, n_min=N_MIN_DISTINCT):
    """Return (counts, refused). `refused` maps each covariate with fewer than
    n_min distinct values to its distinct count. A non-empty `refused` => the join
    is degenerate and NO separation claim may be computed on it."""
    counts = distinct_counts(shots, covariates)
    refused = {k: n for k, n in counts.items() if n < n_min}
    return counts, refused


# ---------------------------------------------------------------------------
# Midrank AUC / Mann-Whitney (fixes the tie-blind AUC-0.000 artifact).
# ---------------------------------------------------------------------------
def _midrank(vals):
    """Average (midrank) ranks, 1-based, ties share their mean rank."""
    a = np.asarray(vals, dtype=float)
    n = len(a)
    order = a.argsort(kind="mergesort")
    sorted_a = a[order]
    ranks = np.empty(n, dtype=float)
    i = 0
    while i < n:
        j = i
        while j + 1 < n and sorted_a[j + 1] == sorted_a[i]:
            j += 1
        avg = (i + j) / 2.0 + 1.0        # 1-based average of the tie block
        ranks[order[i:j + 1]] = avg
        i = j + 1
    return ranks


def auc_midrank(bad, good):
    """Directional AUC of `signal` separating BAD (positive class) from GOOD, using
    MIDRANK assignment so ties yield the true AUC (v1's argsort gave 0.000). Returns
    (auc, p) where p is the two-sided Mann-Whitney p from wash_score."""
    if not bad or not good:
        return float("nan"), float("nan")
    nb, ng = len(bad), len(good)
    ranks = _midrank(list(bad) + list(good))
    Rb = ranks[:nb].sum()
    Ub = Rb - nb * (nb + 1) / 2.0
    auc = float(Ub / (nb * ng))
    _, p = ws._mannwhitney_u(bad, good)
    return auc, float(p)


# ---------------------------------------------------------------------------
# Separation analysis (only reached when the distinct-gate passes).
# ---------------------------------------------------------------------------
def separation(shots, signals, bad_hi, good_hi):
    bad = [s for s in shots if s["hi_frac"] >= bad_hi]
    good = [s for s in shots if s["hi_frac"] <= good_hi]
    sep = {}
    for sig in signals:
        b = [s[sig] for s in bad if sig in s]
        g = [s[sig] for s in good if sig in s]
        A, p = auc_midrank(b, g)
        sep[sig] = {
            "n_bad": len(b), "n_good": len(g),
            "mean_bad": (st.mean(b) if b else None),
            "mean_good": (st.mean(g) if g else None),
            "auc": (None if (A != A) else A),
            "mannwhitney_p": (None if (p != p) else p),
            "separates": bool(A == A and (A >= 0.75 or A <= 0.25)
                              and p == p and p < 0.10),
        }
    return sep, len(bad), len(good)


def grade_shots(shots, covariates, signals, bad_hi, good_hi,
                exclude_black=True):
    """The full v2 grade: (F7) black-frame partition -> (step 2) distinct-gate ->
    (step 3) midrank separation, in that order. Returns a result dict; on a
    degenerate covariate it returns verdict DEGENERATE with the refused block and
    NEVER a separation claim (fail-red 3)."""
    black_excluded = []
    if exclude_black:
        shots, black_excluded = _partition_black(shots, "mean_luma")

    counts, refused = distinct_gate(shots, covariates)
    outcome_counts = distinct_counts(shots, OUTCOMES)

    if refused:
        return {
            "verdict": "DEGENERATE",
            "instrument_validated": False,
            "n_shots": len(shots),
            "covariate_distinct": counts,
            "outcome_distinct": outcome_counts,
            "refused": refused,
            "n_min_distinct": N_MIN_DISTINCT,
            "black_excluded": len(black_excluded),
            "note": ("degenerate covariate(s): each has < %d distinct values across "
                     "the joined shots (stale/collapsed join). No separation claim "
                     "computed — v1's false instrument_validated is mechanically "
                     "prevented." % N_MIN_DISTINCT),
        }

    sep, nbad, ngood = separation(shots, signals, bad_hi, good_hi)
    validated = any(v["separates"] for v in sep.values())
    return {
        "verdict": "GRADED",
        "instrument_validated": validated,
        "n_shots": len(shots),
        "covariate_distinct": counts,
        "outcome_distinct": outcome_counts,
        "refused": {},
        "n_min_distinct": N_MIN_DISTINCT,
        "black_excluded": len(black_excluded),
        "separation": sep,
        "n_bad": nbad, "n_good": ngood,
        "thresholds": {"bad_hi": bad_hi, "good_hi": good_hi},
    }


# ===========================================================================
# OFFLINE re-grade mode (fail-red 3): read a stored wash_natural.json-shape file
# and run the distinct-gate + separation with NO boot.
# ===========================================================================
def regrade(json_path, bad_hi, good_hi):
    d = json.load(open(json_path))
    shots = d.get("off_boot", {}).get("shots") or d.get("shots") or []
    # signals graded = covariates that are numeric join inputs (the F1 phase
    # signals live here); grade only over the covariate signals, gated first.
    signals = ["fx_emit_win", "light_changes_win"]
    res = grade_shots(shots, COVARIATES, signals, bad_hi, good_hi,
                      exclude_black=False)  # stored file already scored; no re-partition
    res["source"] = json_path
    res["mode"] = "regrade"
    return res


# ===========================================================================
# LIVE mode — capture a songMs sweep and PER-FRAME join (step 1).
# ===========================================================================
def _live_imports():
    global ws, wd, lg, capmod
    ws = _load("wash_score", os.path.join(NSCR, "wash_score.py"))
    wd = _load("white_discriminate", os.path.join(WF, "white_discriminate.py"))
    lg = _load("loaddet_gate", os.path.join(NSCR, "loaddet_gate.py"))
    capmod = _load("r4m4_capture", os.path.join(HERE, "r4m4_capture.py"))


import re
ATTRIB_RE = re.compile(r"\[LOADDET\] attrib frame=(-?\d+) pc=\S+ off=(0x[0-9a-f]+) sym=\S+ mod=\S+ draws=(\d+)")
FRAME_RE = re.compile(r"\[LOADDET\] frame=(\d+) gdraw=")
LIGHTVAL_RE = re.compile(r"\[BOOTRNG\] LIGHTVAL .*valhash=([0-9a-f]+)")
LIGHTPOS_RE = re.compile(r"\[BOOTRNG\] LIGHTVAL .*pos=\(?(-?\d+(?:\.\d+)?)[, ]+(-?\d+(?:\.\d+)?)[, ]+(-?\d+(?:\.\d+)?)")
WASH_ENGAGED_RE = re.compile(r"\[WASHPROBE\] SCENE .*engaged=1")
FX_SYM_RE = re.compile(r"InitParticle|CreateParticles|PartLauncher|RndParticle", re.I)


def seam_env(guard_on=False, natural=False):
    env = {} if natural else dict(wd.ARMS["eng_hot"])
    env.update({"RB3_FIXED_CLOCK": "1", "RB3_LOAD_DETERMINISM": "1",
                "RB3_LOADDET_ATTRIB": "1", "RB3_LOADDET_JITTER": "200",
                "RB3_LOADDET_TIMELINE": "1",   # T1: co-arm the timeline markers
                "RB3_BOOTRNG_PROBE": "1", "RB3_WASH_PROBE": "1"})
    if guard_on:
        env["RB3_VENUE_WHITE_GUARD"] = "1"
    return env


def parse_phase(binpath, log_path):
    """Build the T1 PER-FRAME timeline table: frame -> fx_emit (particle-attributed
    draws THAT frame), light_state (distinct valhash count THAT frame), light_pos_amp
    (per-frame light-position spread). Keyed on the attrib line's OWN frame= field
    (already end-of-frame-corrected at emission, R6.2) — NOT a trailing window."""
    fx_by_frame = {}       # frame -> particle-emission draws that frame
    light_hash = {}        # frame -> set(valhash)
    light_pos = {}         # frame -> list of (x,y,z)
    cur_frame = 0
    guard_hits = 0
    rows = []
    with open(log_path, "r", errors="replace") as f:
        for ln in f:
            m = FRAME_RE.search(ln)
            if m:
                cur_frame = int(m.group(1)); continue
            m = ATTRIB_RE.search(ln)
            if m:
                rows.append((int(m.group(1)), m.group(2), int(m.group(3)))); continue
            m = LIGHTVAL_RE.search(ln)
            if m:
                light_hash.setdefault(cur_frame, set()).add(m.group(1))
                mp = LIGHTPOS_RE.search(ln)
                if mp:
                    light_pos.setdefault(cur_frame, []).append(
                        (float(mp.group(1)), float(mp.group(2)), float(mp.group(3))))
                continue
            if WASH_ENGAGED_RE.search(ln):
                guard_hits += 1
    offs = sorted({off for _, off, _ in rows})
    syms = lg.resolve_offsets(binpath, offs)
    fx_offs = {o for o in offs if FX_SYM_RE.search(syms.get(o, ""))}
    fx_sym_examples = sorted({syms[o] for o in fx_offs})
    for fr, off, draws in rows:
        if off in fx_offs:
            fx_by_frame[fr] = fx_by_frame.get(fr, 0) + draws
    return {"fx_by_frame": fx_by_frame, "light_hash": light_hash,
            "light_pos": light_pos, "guard_hits": guard_hits,
            "fx_offs": sorted(fx_offs), "fx_syms": fx_sym_examples,
            "n_attrib_offs": len(offs)}


def _pos_amplitude(positions):
    """Per-frame light-position amplitude = mean pairwise spread of the light
    positions seen that frame (0 if <2 lights)."""
    if not positions or len(positions) < 2:
        return 0.0
    arr = np.asarray(positions, dtype=float)
    c = arr.mean(axis=0)
    return float(np.mean(np.linalg.norm(arr - c, axis=1)))


def perframe_join(shot_frame, ph, smooth=2):
    """Step 1: join a screenshot to ITS OWN frame's row, with a TIGHT symmetric
    smoothing window [fr-smooth, fr+smooth] (NOT a big trailing sum). Returns the
    per-frame covariates."""
    lo, hi = shot_frame - smooth, shot_frame + smooth
    fx = sum(ph["fx_by_frame"].get(fr, 0) for fr in range(lo, hi + 1))
    lights = set()
    poss = []
    for fr in range(lo, hi + 1):
        lights |= ph["light_hash"].get(fr, set())
        poss.extend(ph["light_pos"].get(fr, []))
    return {
        "fx_emit_win": fx,
        "light_changes_win": len(lights),
        "light_pos_amp": _pos_amplitude(poss),
    }


def run_boot(binpath, guard_on, natural, tag, targets, raws, hi_ms, win_smooth):
    pref = os.path.join(raws, tag)
    mc = capmod.multi_capture(binpath, seam_env(guard_on, natural), pref, targets,
                              hi_ms + 8000, song_downs=4)
    ph = parse_phase(binpath, mc["log"])
    shots = []
    for s in mc["shots"]:
        m = ws.score_image(s["png"])
        fr = s["frame"]
        pj = perframe_join(fr, ph, win_smooth)
        shots.append({
            "songms": s["songms"], "frame": fr,
            "hi_frac": m["hi_frac"], "mean_luma": m["mean_luma"],
            "class": m["wash_class"],
            "fx_emit_win": pj["fx_emit_win"],
            "light_changes_win": pj["light_changes_win"],
            "light_pos_amp": pj["light_pos_amp"],
        })
    disclosure = _attempt_disclosure(mc.get("attempts", len(shots)), len(shots),
                                     mc.get("discarded_reasons", []))
    return {"guard": "ON" if guard_on else "OFF", "shots": shots,
            "phase_meta": {k: ph[k] for k in ("guard_hits", "fx_offs", "fx_syms",
                                              "n_attrib_offs")},
            "attempt_disclosure": disclosure,
            "log": mc["log"], "n_shots": len(shots)}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin")
    ap.add_argument("--regrade", default=None,
                    help="OFFLINE fail-red: re-grade a stored wash_natural.json (no boot).")
    ap.add_argument("--lo", type=float, default=8000.0)
    ap.add_argument("--hi", type=float, default=26000.0)
    ap.add_argument("--step", type=float, default=400.0)
    ap.add_argument("--win", type=int, default=2,
                    help="TIGHT symmetric per-frame smoothing window (NOT a trailing sum).")
    ap.add_argument("--bad-hi", type=float, default=25.0)
    ap.add_argument("--good-hi", type=float, default=5.0)
    ap.add_argument("--natural", action="store_true",
                    help="natural venue (both wash classes) — the lint-3 substrate.")
    ap.add_argument("--out", default=os.path.join(HERE, "evidence"))
    ap.add_argument("--raws", default="/tmp/r4m4-wash-v2")
    ap.add_argument("--tag", default="wash_v2")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)

    # -------- OFFLINE regrade (fail-red 3) --------
    if a.regrade:
        res = regrade(a.regrade, a.bad_hi, a.good_hi)
        res["capture_lints"] = _CAPTURE_LINTS
        outp = os.path.join(a.out, f"{a.tag}_regrade_refusal.json")
        _safe_json_dump(res, outp)
        print("==================== WASH v2 REGRADE (offline) ====================")
        print(f"source: {a.regrade}")
        print(f"verdict: {res['verdict']}  instrument_validated: {res['instrument_validated']}")
        print(f"covariate_distinct: {res['covariate_distinct']}")
        print(f"outcome_distinct:   {res['outcome_distinct']}")
        print(f"refused: {res['refused']}")
        print(f"wrote {outp}")
        # exit non-zero iff the fail-red did NOT fire on the known-degenerate file
        ok = (res["verdict"] == "DEGENERATE" and res["instrument_validated"] is False
              and res["refused"].get("fx_emit_win") == 2
              and res["refused"].get("light_changes_win") == 2)
        print(f"FAIL-RED (3) refusal fired as required: {ok}")
        return 0 if ok else 1

    # -------- LIVE capture --------
    if not a.bin:
        print("live mode requires --bin", file=sys.stderr); return 2
    _live_imports()
    os.makedirs(a.raws, exist_ok=True)
    targets = [a.lo + i * a.step for i in range(int((a.hi - a.lo) / a.step) + 1)]

    print("== wash v2 co-sampling boot (guard OFF) ==")
    off = run_boot(a.bin, False, a.natural, a.tag + "_off", targets, a.raws, a.hi, a.win)
    print(f"  captured {off['n_shots']} shots; FX offsets: {off['phase_meta']['fx_syms']}")

    # Step 4 signal light_pos_amp is included in the covariate list only for the
    # separation pass; it is gated by the same distinct-gate as steps 1-3.
    signals = ["fx_emit_win", "light_changes_win", "light_pos_amp"]
    covs = COVARIATES + ["light_pos_amp"]
    res = grade_shots(list(off["shots"]), covs, signals, a.bad_hi, a.good_hi,
                      exclude_black=True)
    res["mode"] = "live"
    res["tag"] = a.tag
    res["bin"] = a.bin
    res["win_smooth"] = a.win
    res["off_boot"] = off
    res["capture_lints"] = _CAPTURE_LINTS

    outp = os.path.join(a.out, f"{a.tag}_live.json")
    _safe_json_dump(res, outp)
    print("\n==================== WASH v2 CO-SAMPLING (live) ====================")
    print(f"verdict: {res['verdict']}  instrument_validated: {res['instrument_validated']}")
    print(f"covariate_distinct: {res['covariate_distinct']}")
    print(f"refused: {res['refused']}")
    if res["verdict"] == "GRADED":
        for sig, v in res["separation"].items():
            print(f"  {sig:18s} AUC={v['auc']} p={v['mannwhitney_p']} separates={v['separates']}")
    else:
        print("  (degenerate join -> honest still_degenerate disclosure, no silent pass)")
    print(f"wrote {outp}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
