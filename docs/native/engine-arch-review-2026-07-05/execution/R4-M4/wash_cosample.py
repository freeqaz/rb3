#!/usr/bin/env python3
"""wash_cosample.py — Wave-18 Lane W (R4-M4): the wash per-FX co-sampling instrument
(BOOTRNG backlog item 3, the Wave-11 FX/swept-light PHASE axis).

WHAT IT DOES
------------
BOOTRNG proved the residual WHITE fires at a grade + venue-light COLOR state IDENTICAL
to non-washed frames -> the over-exposure is per-FX / per-swept-light rendering fidelity
at a specific ANIMATION PHASE (BOOTRNG/STATUS.md NEW FINDING; PLAN-R4 §M4.2). This
instrument co-samples, PER FRAME, the two named phase signals against hi_frac:

  * particle-emission phase = per-frame gRand-tap draws attributed (addr2line) to
    RndParticleSys::InitParticle / CreateParticles / PartLauncher (RB3_LOADDET_ATTRIB).
    Summed over a trailing window [F-W, F] = "recently-spawned FX intensity at frame F".
  * swept-light phase = distinct [BOOTRNG] LIGHTVAL valhash count over [F-W, F]
    (position+range folded into the hash in the v2 probe) = "light motion at frame F".

Within ONE seam-pinned boot (RB3_LOAD_DETERMINISM makes the whole per-frame trace
reproducible) we screenshot a songMs SWEEP, score hi_frac per shot, and read the
co-sampled phase signals at that shot's frame.

LINT 3 (no unvalidated oracle): the instrument's numbers mean nothing until it
demonstrates KNOWN-GOOD / KNOWN-BAD separation. Ground-truth label = hi_frac (BAD =
WHITE-class over-exposed shot; GOOD = low-hi_frac shot). The instrument is VALIDATED
iff a phase signal SEPARATES the two classes (Mann-Whitney + AUC). If NO phase signal
separates them, that is reported as "FX-emission not the driver" -- a finding, not a
silent pass.

LINT 8: an optional guard-ON boot reports the guard branch-entry hit-count
([WASHPROBE] SCENE engaged=1, the world.cam path where venueHighlightLumaMode is
written) so any "guard changes nothing" reading proves the guard code ran.
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


ws = _load("wash_score", os.path.join(NSCR, "wash_score.py"))
wd = _load("white_discriminate", os.path.join(WF, "white_discriminate.py"))
lg = _load("loaddet_gate", os.path.join(NSCR, "loaddet_gate.py"))
cap = _load("r4m4_capture", os.path.join(HERE, "r4m4_capture.py"))

ATTRIB_RE = re.compile(r"\[LOADDET\] attrib frame=(-?\d+) pc=\S+ off=(0x[0-9a-f]+) sym=\S+ mod=\S+ draws=(\d+)")
FRAME_RE = re.compile(r"\[LOADDET\] frame=(\d+) gdraw=")
LIGHTVAL_RE = re.compile(r"\[BOOTRNG\] LIGHTVAL .*valhash=([0-9a-f]+)")
WASH_ENGAGED_RE = re.compile(r"\[WASHPROBE\] SCENE .*engaged=1")
FX_SYM_RE = re.compile(r"InitParticle|CreateParticles|PartLauncher|RndParticle", re.I)


def seam_env(guard_on=False, natural=False):
    # NATURAL venue (no eng_hot forcing) gives BOTH classes across a sweep: a
    # non-washed floor (GOOD) + phase-specific WHITE spikes (BAD) -> the lint-3
    # known-good/known-bad substrate. eng_hot forces the WHOLE song hot (0 GOOD).
    env = {} if natural else dict(wd.ARMS["eng_hot"])
    env.update({"RB3_FIXED_CLOCK": "1", "RB3_LOAD_DETERMINISM": "1",
                "RB3_LOADDET_ATTRIB": "1", "RB3_LOADDET_JITTER": "200",
                "RB3_BOOTRNG_PROBE": "1", "RB3_WASH_PROBE": "1"})
    if guard_on:
        env["RB3_VENUE_WHITE_GUARD"] = "1"
    return env


def parse_phase(binpath, log_path):
    """Parse the boot log into per-frame FX-emission draws (particle offsets only,
    resolved via addr2line) + per-frame swept-light valhashes + guard hit count."""
    fx_draws = {}       # frame -> particle-emission draws
    light_hash = {}     # frame -> set(valhash) seen at/after this frame line
    cur_frame = 0
    guard_hits = 0
    # first pass: collect the offset set + raw rows
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
                light_hash.setdefault(cur_frame, set()).add(m.group(1)); continue
            if WASH_ENGAGED_RE.search(ln):
                guard_hits += 1
    offs = sorted({off for _, off, _ in rows})
    syms = lg.resolve_offsets(binpath, offs)
    fx_offs = {o for o in offs if FX_SYM_RE.search(syms.get(o, ""))}
    fx_sym_examples = sorted({syms[o] for o in fx_offs})
    for fr, off, draws in rows:
        if off in fx_offs:
            fx_draws[fr] = fx_draws.get(fr, 0) + draws
    return {"fx_draws": fx_draws, "light_hash": light_hash,
            "guard_hits": guard_hits, "fx_offs": sorted(fx_offs),
            "fx_syms": fx_sym_examples, "n_attrib_offs": len(offs)}


def window(dmap, frame, w, agg="sum"):
    vals = [dmap.get(fr, 0) for fr in range(frame - w, frame + 1)]
    return sum(vals)


def light_window(light_hash, frame, w):
    s = set()
    for fr in range(frame - w, frame + 1):
        s |= light_hash.get(fr, set())
    return len(s)


def auc(bad, good):
    """AUC of `signal` separating BAD (positive) from GOOD via Mann-Whitney U."""
    if not bad or not good:
        return float("nan"), float("nan")
    U, p = ws._mannwhitney_u(bad, good)
    # AUC = U_bad / (n_bad*n_good); _mannwhitney_u returns min(U1,U2) -> recompute directional
    import numpy as np
    nb, ng = len(bad), len(good)
    allv = np.concatenate([np.asarray(bad, float), np.asarray(good, float)])
    order = allv.argsort(); ranks = np.empty(len(allv)); ranks[order] = np.arange(1, len(allv) + 1)
    Rb = ranks[:nb].sum(); Ub = Rb - nb * (nb + 1) / 2.0
    return float(Ub / (nb * ng)), float(p)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", required=True)
    ap.add_argument("--lo", type=float, default=8000.0)
    ap.add_argument("--hi", type=float, default=26000.0)
    ap.add_argument("--step", type=float, default=400.0)
    ap.add_argument("--win", type=int, default=30, help="trailing frame window for co-sampling")
    ap.add_argument("--bad-hi", type=float, default=25.0, help="hi_frac >= this = known-BAD (WHITE)")
    ap.add_argument("--good-hi", type=float, default=5.0, help="hi_frac <= this = known-GOOD")
    ap.add_argument("--guard-boot", action="store_true", help="also run a guard-ON boot (lint 8)")
    ap.add_argument("--natural", action="store_true",
                    help="natural venue (no eng_hot) — lint-3 substrate with both classes")
    ap.add_argument("--out", default=os.path.join(HERE, "evidence"))
    ap.add_argument("--raws", default="/tmp/r4m4-wash")
    ap.add_argument("--tag", default="wash")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    os.makedirs(a.raws, exist_ok=True)
    targets = [a.lo + i * a.step for i in range(int((a.hi - a.lo) / a.step) + 1)]

    def run_boot(guard_on, tag):
        pref = os.path.join(a.raws, tag)
        mc = cap.multi_capture(a.bin, seam_env(guard_on, a.natural), pref, targets, a.hi + 8000,
                               song_downs=4)
        ph = parse_phase(a.bin, mc["log"])
        shots = []
        for s in mc["shots"]:
            m = ws.score_image(s["png"])
            fr = s["frame"]
            shots.append({
                "songms": s["songms"], "frame": fr, "hi_frac": m["hi_frac"],
                "mean_luma": m["mean_luma"], "class": m["wash_class"],
                "fx_emit_win": window(ph["fx_draws"], fr, a.win),
                "light_changes_win": light_window(ph["light_hash"], fr, a.win),
            })
        return {"guard": "ON" if guard_on else "OFF", "shots": shots,
                "phase_meta": {k: ph[k] for k in ("guard_hits", "fx_offs", "fx_syms",
                                                  "n_attrib_offs")},
                "log": mc["log"], "n_shots": len(shots)}

    print("== wash co-sampling boot (guard OFF) ==")
    off = run_boot(False, a.tag + "_off")
    print(f"  captured {off['n_shots']} shots; FX offsets resolved: {off['phase_meta']['fx_syms']}")

    # ---- lint 3: known-good/known-bad separation ----
    shots = off["shots"]
    bad = [s for s in shots if s["hi_frac"] >= a.bad_hi]
    good = [s for s in shots if s["hi_frac"] <= a.good_hi]
    sep = {}
    for sig in ("fx_emit_win", "light_changes_win"):
        b = [s[sig] for s in bad]; g = [s[sig] for s in good]
        A, p = auc(b, g)
        sep[sig] = {"n_bad": len(b), "n_good": len(g),
                    "mean_bad": st.mean(b) if b else None,
                    "mean_good": st.mean(g) if g else None,
                    "auc": A, "mannwhitney_p": p,
                    "separates": (A == A and (A >= 0.75 or A <= 0.25) and (p == p and p < 0.10))}
    # validated iff at least one phase signal separates BAD from GOOD
    validated = any(v["separates"] for v in sep.values())

    result = {
        "tag": a.tag, "bin": a.bin, "win": a.win,
        "thresholds": {"bad_hi": a.bad_hi, "good_hi": a.good_hi},
        "off_boot": off,
        "separation(lint3)": sep,
        "instrument_validated": validated,
        "n_bad": len(bad), "n_good": len(good),
    }

    if a.guard_boot:
        print("== wash co-sampling boot (guard ON) — lint 8 ==")
        on = run_boot(True, a.tag + "_on")
        result["on_boot"] = on
        # guard effect on the co-sampled wash, carrying the branch-entry hit count
        off_hi = [s["hi_frac"] for s in off["shots"]]
        on_hi = [s["hi_frac"] for s in on["shots"]]
        result["guard_effect"] = {
            "guard_hits_OFF": off["phase_meta"]["guard_hits"],
            "guard_hits_ON": on["phase_meta"]["guard_hits"],
            "mean_hi_OFF": st.mean(off_hi) if off_hi else None,
            "mean_hi_ON": st.mean(on_hi) if on_hi else None,
            "note": ("lint 8: guard branch-entry hit-count on both arms proves the guard "
                     "code ran; any near-zero delta is not 'guard absent'."),
        }

    outp = os.path.join(a.out, f"{a.tag}.json")
    json.dump(result, open(outp, "w"), indent=2)

    print("\n==================== WASH CO-SAMPLING ====================")
    print(f"shots={len(shots)}  BAD(hi>={a.bad_hi})={len(bad)}  GOOD(hi<={a.good_hi})={len(good)}")
    for sig, v in sep.items():
        print(f"  {sig:20s} mean_bad={v['mean_bad']} mean_good={v['mean_good']} "
              f"AUC={v['auc']:.3f} p={v['mannwhitney_p']:.3g} separates={v['separates']}")
    print(f"\nINSTRUMENT VALIDATED (lint 3, known-good/bad separation): {validated}")
    if a.guard_boot:
        ge = result["guard_effect"]
        print(f"guard hits OFF={ge['guard_hits_OFF']} ON={ge['guard_hits_ON']}  "
              f"mean_hi OFF={ge['mean_hi_OFF']:.2f} ON={ge['mean_hi_ON']:.2f} (lint 8)")
    print(f"wrote {outp}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
