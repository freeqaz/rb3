#!/usr/bin/env python3
"""loaddet_gate.py — Wave-12 Lane A (W0.3d-b) A-S2 seam gate harness.

Measures the H-RESEED seam's PRIMARY gate: the POST-ANCHOR gRand stream position
collapses across boots when the seam is ON, and reproduces its spread when OFF —
both measured under env-gated worker-latency jitter (RB3_LOADDET_JITTER, the
fail-red control amplifying the worker<->main alloc-order race A-S1 traced).

Why post-anchor DELTA, not absolute gdraw: the seam RE-BASES the stream by
reseeding gRand to a canonical constant at the is_playing 0->1 anchor
(GamePanel::StartGame). After the reseed the RNG STATE (table + indices) is a
deterministic function of ONLY the number of draws SINCE the reseed — the
cumulative counter still carries the boot-varying PRE-anchor menu draws, so the
meaningful "stream position at the pinned capture" is
  postAnchorDelta = gdraw@[anchorFrame + K]  -  gdraw@anchorFrame
which is exactly the draws-since-reseed that fix the post-anchor RNG state (and
thus the BOOTRNG mid_sat visual). The seam collapses postAnchorDelta; it does NOT
(and is not meant to) collapse absolute gdraw.

Each boot:
  1. boots headless under RB3_FIXED_CLOCK + RB3_LOADDET_PROBE (+ arm env),
  2. navs to gameplay, waits is_playing==1 (the anchor fires here),
  3. drives the song (autohit) until songMs >= (K+MARGIN)*dt so the log holds
     frame anchor+K,
  4. parses engine.log:
        [LOADDET] anchor frame=A gdraw=Ga    (both arms — probe-gated marker)
        [LOADDET] frame=F gdraw=G            (every frame)
     -> Ga = gdraw@anchor; Gt = gdraw@(A+K); postAnchorDelta = Gt - Ga.

Arms:
  OFF : RB3_LOADDET_JITTER set, RB3_LOAD_DETERMINISM unset -> spread reproduces.
  ON  : RB3_LOADDET_JITTER set, RB3_LOAD_DETERMINISM=1     -> spread collapses.

Usage:
  python3 loaddet_gate.py --bin native/build-agent-W0.3d-b/rb3-native \
      --n 10 --k 480 --jitter 200 --out /tmp/loaddet-gate
"""
import argparse, importlib.util, json, os, re, signal, subprocess, sys, time
import statistics as st

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", "..", "..", ".."))
NSCR = os.path.join(REPO, "scripts", "native")


def _load(mod, path):
    spec = importlib.util.spec_from_file_location(mod, path)
    m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m); return m


k = _load("kbd2game", os.path.join(NSCR, "keyboard-to-gameplay.py"))
pgc = _load("pgc", os.path.join(NSCR, "placement-gate-capture.py"))

ANCHOR_RE = re.compile(r"\[LOADDET\] anchor frame=(\d+) gdraw=(\d+)")
FRAME_RE = re.compile(r"\[LOADDET\] frame=(\d+) gdraw=(\d+)")
RESEED_RE = re.compile(r"\[LOADDET\] reseed anchor=(\S+) seed=(\S+) gdrawBefore=(\d+)")
DT_MS = 1000.0 / 60.0


def log(m):
    print(f"[loaddet-gate] {m}", flush=True)


def boot_measure(binpath, arm_env, k_frames, target_ms, log_path, diff="hard",
                 song_downs=4, verbose=False, autohit=False):
    """One boot: nav to gameplay, drive to target songMs, return dict with
    anchorFrame, gAnchor, gTarget, postAnchorDelta, absTarget, reseedFired."""
    port = k.free_port()
    logf = open(log_path, "w")
    env = dict(os.environ)
    env.update({
        "RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
        "MILO_HEADLESS": "1", "RB3_FIXED_CLOCK": "1", "RB3_LOADDET_PROBE": "1",
        "RB3_DATA": k.DEFAULT_DATA,
        "RB3_DTA_OVERLAY": os.path.join(REPO, "native", "dta"),
    })
    env.update(arm_env)
    proc = subprocess.Popen([binpath], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)
    result = {"ok": False, "reason": "?"}
    try:
        if not pgc.nav_to_gameplay(port, proc, song_downs, k.DIFF_INDEX[diff], verbose):
            result["reason"] = "nav_failed"; return result
        k.verb(port, "nofail")
        # wait for is_playing (anchor fires here)
        dl = time.time() + 60
        playing = False
        while time.time() < dl:
            ip = k.dta(port, "{game is_playing}")
            h = k.health(port)
            if ip is not None and int(ip) == 1 and h and h[1] >= 0:
                playing = True; break
            # PRE-anchor autohit: harmless to postAnchorDelta (measured from the
            # anchor FORWARD) and needed to reliably drive the song past the
            # intro/countdown to the is_playing edge. Only the POST-anchor drive
            # below is input-free.
            k.verb(port, "autohit")
            time.sleep(0.15)
        if not playing:
            result["reason"] = "song_never_played"; return result
        # drive to target songMs so the log holds frame anchor+K. INPUT-FREE by
        # default (nofail set above): under RB3_FIXED_CLOCK the headless main loop
        # free-runs RunOneFrame at fixed dt, so songMs advances with NO autohit —
        # removing the wall-clock HTTP-autohit input-timing confound that pollutes
        # the raw gdraw COUNT with variable note-hit RNG draws unrelated to the
        # boot-determinism axis. --autohit re-enables it for A/B.
        dl = time.time() + 120
        while time.time() < dl:
            h = k.health(port)
            if h and h[1] >= target_ms:
                break
            if autohit:
                k.verb(port, "autohit")
            time.sleep(0.02)
        # give the log a moment to flush the final frames, then stop
        time.sleep(0.3)
    finally:
        logf.flush()
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGTERM); proc.wait(timeout=10)
        except Exception:
            try: os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
            except Exception: pass
        logf.close()

    # parse
    anchor_frame = None; g_anchor = None; reseed_fired = False
    frames = {}
    with open(log_path, "r", errors="replace") as f:
        for ln in f:
            m = ANCHOR_RE.search(ln)
            if m and anchor_frame is None:
                anchor_frame = int(m.group(1)); g_anchor = int(m.group(2)); continue
            if RESEED_RE.search(ln):
                reseed_fired = True; continue
            m = FRAME_RE.search(ln)
            if m:
                frames[int(m.group(1))] = int(m.group(2)); continue
    if anchor_frame is None:
        result["reason"] = "no_anchor_marker"; return result
    tgt_frame = anchor_frame + k_frames
    # nearest logged frame >= tgt_frame (frames are per-RunOneFrame, contiguous)
    cand = [fr for fr in frames if fr >= tgt_frame]
    if not cand:
        result["reason"] = f"target_frame_not_reached(anchor={anchor_frame},maxlog={max(frames) if frames else -1})"
        return result
    tf = min(cand)
    g_target = frames[tf]
    result.update({
        "ok": True, "anchorFrame": anchor_frame, "targetFrame": tf,
        "gAnchor": g_anchor, "gTarget": g_target,
        "postAnchorDelta": g_target - g_anchor, "absTarget": g_target,
        "reseedFired": reseed_fired,
    })
    return result


def run_arm(binpath, name, arm_env, n, k_frames, target_ms, out_dir, diff, verbose, autohit):
    log(f"=== ARM {name}: n={n} env={arm_env} ===")
    boots = []
    for i in range(n):
        lp = os.path.join(out_dir, f"{name}-boot{i}.log")
        r = boot_measure(binpath, arm_env, k_frames, target_ms, lp, diff=diff, verbose=verbose, autohit=autohit)
        if r["ok"]:
            log(f"  {name} boot{i}: anchor@{r['anchorFrame']} gAnchor={r['gAnchor']} "
                f"gTarget={r['gTarget']} postDelta={r['postAnchorDelta']} "
                f"reseed={r['reseedFired']}")
        else:
            log(f"  {name} boot{i}: FAILED ({r['reason']})")
        boots.append(r)
    ok = [b for b in boots if b["ok"]]
    deltas = [b["postAnchorDelta"] for b in ok]
    absv = [b["absTarget"] for b in ok]
    summary = {
        "arm": name, "env": arm_env, "n": n, "ok": len(ok),
        "postAnchorDeltas": deltas,
        "distinctDeltas": sorted(set(deltas)),
        "deltaSpread": (max(deltas) - min(deltas)) if deltas else None,
        "absTargets": absv,
        "absSpread": (max(absv) - min(absv)) if absv else None,
        "reseedFiredAll": all(b["reseedFired"] for b in ok) if ok else False,
        "boots": boots,
    }
    return summary


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", default=os.path.join(REPO, "native", "build-agent-W0.3d-b", "rb3-native"))
    ap.add_argument("--n", type=int, default=10)
    ap.add_argument("--k", type=int, default=480, help="post-anchor frame offset")
    ap.add_argument("--jitter", type=int, default=200, help="RB3_LOADDET_JITTER max microseconds")
    ap.add_argument("--diff", default="hard")
    ap.add_argument("--song-downs", type=int, default=4)
    ap.add_argument("--out", default="/tmp/loaddet-gate")
    ap.add_argument("--tag", default="gate")
    ap.add_argument("--arm", default="both", choices=["both", "off", "on"])
    ap.add_argument("--verbose", action="store_true")
    ap.add_argument("--autohit", action="store_true", help="inject autohit during drive (confounds count; default input-free)")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    target_ms = (a.k + 60) * DT_MS
    jitter = {"RB3_LOADDET_JITTER": str(a.jitter)} if a.jitter > 0 else {}

    arms = {}
    if a.arm in ("both", "off"):
        arms["OFF"] = run_arm(a.bin, "OFF", dict(jitter), a.n, a.k, target_ms, a.out, a.diff, a.verbose, a.autohit)
    if a.arm in ("both", "on"):
        on_env = dict(jitter); on_env["RB3_LOAD_DETERMINISM"] = "1"
        arms["ON"] = run_arm(a.bin, "ON", on_env, a.n, a.k, target_ms, a.out, a.diff, a.verbose, a.autohit)

    out = {"tag": a.tag, "bin": a.bin, "k": a.k, "target_ms": target_ms,
           "jitter_us": a.jitter, "arms": arms}
    outpath = os.path.join(a.out, f"{a.tag}.json")
    with open(outpath, "w") as f:
        json.dump(out, f, indent=2)

    print("\n==================== GATE RESULT ====================")
    for nm, s in arms.items():
        print(f"ARM {nm}: ok={s['ok']}/{s['n']}  "
              f"postAnchorDeltaSpread={s['deltaSpread']}  "
              f"distinctDeltas={s['distinctDeltas']}  "
              f"absSpread={s['absSpread']}  reseedAll={s['reseedFiredAll']}")
    if "OFF" in arms and "ON" in arms:
        off, on = arms["OFF"], arms["ON"]
        primary_on = (on["ok"] >= 1 and on["deltaSpread"] == 0 and len(on["distinctDeltas"]) == 1)
        failred_off = (off["ok"] >= 1 and off["deltaSpread"] and off["deltaSpread"] > 0)
        print(f"\nPRIMARY (ON collapses 10/10):  {'PASS' if primary_on else 'FAIL'} "
              f"(ON distinctDeltas={on['distinctDeltas']})")
        print(f"FAIL-RED (OFF reproduces spread under jitter):  {'PASS' if failred_off else 'FAIL'} "
              f"(OFF spread={off['deltaSpread']})")
    print(f"\nwrote {outpath}")


if __name__ == "__main__":
    main()
