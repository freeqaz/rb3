#!/usr/bin/env python3
"""r4m4_capture.py — seam-clean capture primitives for Lane W (R4-M4).

The Wave-10 WHITE HOLD had TWO confounds the R4 seam + this driver remove:
  (1) capture-pinned autohits POST-anchor (wash-measure.py:183) -> hit-FX / scoring
      draws that the seam does NOT isolate reach gRand -> postAnchorDelta diverges ->
      the ledger fails (measured: stream 2/3 through wm.capture_pinned). Fix: drive
      INPUT-FREE post-anchor (the proven M3 loaddet-driver discipline). The song is
      held alive by `nofail`; under RB3_FIXED_CLOCK frames advance without input.
  (2) the +/-tol songMs window admits different LIGHTING PHASES (Wave-11 bimodal
      window confound: NEARBLACK vs WHITE at the same ms). Fix: screenshot the FIRST
      frame with songMs >= target (one-sided, tol=0). Under the fixed clock songMs is
      a deterministic function of frame, so the crossing frame -- hence the captured
      song-time phase -- is identical across boots on a pinned trajectory.

Both the WHITE re-grade and the wash co-sampling instrument import these.
"""
import importlib.util, os, time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", "..", "..", ".."))
NSCR = os.path.join(REPO, "scripts", "native")


def _load(m, p):
    s = importlib.util.spec_from_file_location(m, p)
    x = importlib.util.module_from_spec(s); s.loader.exec_module(x); return x


k = _load("kbd2game", os.path.join(NSCR, "keyboard-to-gameplay.py"))
pgc = _load("pgc", os.path.join(NSCR, "placement-gate-capture.py"))
wm = _load("washmeasure", os.path.join(NSCR, "wash-measure.py"))


def _reach_playing(port, proc, song_downs, diff):
    """Nav to gameplay + wait for is_playing (anchor). Pre-anchor autohit only."""
    if not pgc.nav_to_gameplay(port, proc, song_downs, k.DIFF_INDEX[diff], False):
        return None, "nav_failed"
    k.verb(port, "nofail")
    dl = time.time() + 60
    while time.time() < dl:
        ip = k.dta(port, "{game is_playing}")
        h = k.health(port)
        if ip is not None and int(ip) == 1 and h and h[1] >= 0:
            return h[1], None            # start songMs
        k.verb(port, "autohit")          # PRE-anchor only
        time.sleep(0.2)
    return None, "song_never_played"


def _pin_shot(port):
    dz = k.dta(port, "{rb3_director_disable 1}")
    if isinstance(dz, int) and dz == 1:
        for cand in wm.SHOT_CANDIDATES:
            if pgc.force_shot(port, cand):
                return cand
    return None


def capture_at_songms(binpath, env, pref, target_ms, overshoot_ms,
                      song_downs=4, diff="hard", pin_shot=True):
    """Boot -> gameplay -> pin wide shot -> INPUT-FREE drive -> screenshot the first
    frame with songMs >= target_ms. Returns dict{png,songms,frame,log,shot} or None."""
    log_path = pref + ".engine.log"
    port, proc, logf = wm._boot(binpath, env, log_path)
    try:
        start, why = _reach_playing(port, proc, song_downs, diff)
        if start is None:
            return {"png": None, "reason": why, "log": log_path}
        shot = _pin_shot(port) if pin_shot else None
        dl = time.time() + 120
        while time.time() < dl:
            h = k.health(port)
            if not h:
                time.sleep(0.02); continue
            frame, ms, screen = h
            if ms >= target_ms:
                png = pref + ".png"
                if not k.screenshot(port, png):
                    return {"png": None, "reason": "screenshot_failed", "log": log_path}
                return {"png": png, "songms": ms, "frame": frame,
                        "log": log_path, "shot": shot, "start": start}
            if ms > overshoot_ms:
                return {"png": None, "reason": f"overshoot_ms={ms:.0f}", "log": log_path}
            time.sleep(0.01)             # INPUT-FREE post-anchor
        return {"png": None, "reason": "window_timeout", "log": log_path}
    finally:
        wm._kill(proc, logf)


def multi_capture(binpath, env, pref, targets_ms, overshoot_ms,
                  song_downs=4, diff="hard", pin_shot=True):
    """ONE boot; screenshot at the first frame crossing EACH target in `targets_ms`
    (ascending). Input-free post-anchor. Returns dict{shots:[{png,songms,frame}...],
    log, start} -- the per-frame temporal sweep the wash co-sampler needs."""
    log_path = pref + ".engine.log"
    port, proc, logf = wm._boot(binpath, env, log_path)
    shots = []
    try:
        start, why = _reach_playing(port, proc, song_downs, diff)
        if start is None:
            return {"shots": [], "reason": why, "log": log_path}
        shot = _pin_shot(port) if pin_shot else None
        tgts = sorted(targets_ms)
        ti = 0
        dl = time.time() + 240
        while time.time() < dl and ti < len(tgts):
            h = k.health(port)
            if not h:
                time.sleep(0.02); continue
            frame, ms, screen = h
            while ti < len(tgts) and ms >= tgts[ti]:
                png = f"{pref}_s{ti:02d}.png"
                if k.screenshot(port, png):
                    shots.append({"png": png, "songms": ms, "frame": frame,
                                  "target": tgts[ti]})
                ti += 1
            if ms > overshoot_ms:
                break
            time.sleep(0.01)             # INPUT-FREE post-anchor
        return {"shots": shots, "log": log_path, "start": start, "shot": shot}
    finally:
        wm._kill(proc, logf)
