#!/usr/bin/env python3
"""
_review_lead_remeasure.py — INDEPENDENT adversarial re-measure of the audio-vs-track
lead on native, comparing FIX ON (default) vs FIX OFF (RB3_NO_AV_CALIBRATION=1).

Method (does NOT trust the GAME_DBG once-per-second cadence as the only signal):
  - Drives to game_screen with the same NAV used by scoring-test.py.
  - For ~N seconds, repeatedly polls /api/dta/eval for the RAW audio clock
    ({game song_ms} = mMaster->GetAudio()->GetTime()) AND the TaskMgr realtime
    clock ({beatmatch ...} / taskmgr seconds) at high frequency, computing
    lead = audioTime - trackTime.
  - Also parses the GAME_DBG lines from the process log as a cross-check.

Lead > 0  => audio leads the visual track (the bug).
Lead ~ 0  => track locked to audio (the fix).

Usage: python3 _review_lead_remeasure.py --bin PATH --data DIR [--noav] [--secs 8]
"""
import argparse, http.client, json, os, re, signal, socket, subprocess, sys, time

REPO = "/home/free/code/milohax/rb3/.claude/worktrees/audiosync-avlatency"
DEFAULT_BIN = os.path.join(REPO, "native", "build-native", "rb3-native")
DEFAULT_DATA = os.path.join(REPO, "orig-assets", "extracted")

NAV_SCRIPT = (
    "@10:start,@30:confirm,"
    "@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn,"
    "@320:down,@350:msg:music_library:select_highlighted_node,"
    "@380:track:guitar,@390:difficulty:expert,@450:msg:overshell:end_override_flow:1:0,"
    "@500:nofail,@520:autohit"
)

def log(m): print(f"[remeasure] {m}", flush=True)

def free_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.bind(("", 0)); p = s.getsockname()[1]; s.close(); return p

def req(port, method, path, body=None, timeout=20):
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=timeout)
    try:
        c.request(method, path, body=body, headers={"Content-Type": "text/plain"} if body else {})
        r = c.getresponse(); d = r.read()
        return r.status, d.decode("utf-8", "replace")
    except Exception as e:
        return None, str(e)
    finally:
        c.close()

def health(port):
    s, d = req(port, "GET", "/api/health")
    if s != 200: return None
    try: return json.loads(d).get("data", {})
    except Exception: return None

def evl(port, expr):
    s, d = req(port, "POST", "/api/dta/eval", expr)
    if s != 200: return None
    try: return json.loads(d).get("data", {}).get("value")
    except Exception: return None

def verb(port, v): return req(port, "POST", "/api/input", v)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", default=DEFAULT_BIN)
    ap.add_argument("--data", default=DEFAULT_DATA)
    ap.add_argument("--secs", type=float, default=8.0)
    ap.add_argument("--noav", action="store_true", help="set RB3_NO_AV_CALIBRATION=1 (FIX OFF / Wii)")
    args = ap.parse_args()

    port = free_port()
    env = dict(os.environ)
    env.update({"RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
                "MILO_HEADLESS": "1", "RB3_DATA": args.data,
                "GAME_DBG": "1", "RB3_GAME_INPUT": NAV_SCRIPT})
    if args.noav:
        env["RB3_NO_AV_CALIBRATION"] = "1"
    tag = "FIXOFF" if args.noav else "FIXON"
    log_path = os.path.join("/tmp", f"rb3-remeasure-{tag}-{port}.log")
    logf = open(log_path, "w")
    log(f"launching ({tag}) port {port} -> {log_path}")
    proc = subprocess.Popen([args.bin], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)
    try:
        dl = time.time() + 45
        while time.time() < dl:
            if health(port): break
            if proc.poll() is not None: log("FAIL: died during boot"); return 1
            time.sleep(0.5)
        else:
            log("FAIL: server never came up"); return 1

        dl = time.time() + 320; scr = None
        while time.time() < dl:
            if proc.poll() is not None: log("FAIL: died -> game_screen"); return 1
            h = health(port); scr = h.get("currentScreen") if h else None
            if scr == "game_screen": break
            time.sleep(1.0)
        if scr != "game_screen":
            log(f"FAIL: never reached game_screen (last={scr})"); return 1
        log(f"game_screen reached; song={evl(port, '{meta_performer song}')}")

        # Let the song play; GAME_DBG logs (audioTime, songMs) once/sec from Game::Poll.
        # We keep autohit alive and just wait out the window.
        t0 = time.time()
        while time.time() - t0 < args.secs:
            verb(port, "autohit")
            time.sleep(0.1)
        samples = []

        # pull GAME_DBG lines (the authoritative source: audioTime=GetAudio()->GetTime(),
        # songMs=1000*TaskMgr.Seconds(kRealTime) — exactly the highway-driving clock)
        logf.flush()
        dbg = []
        with open(log_path, errors="replace") as f:
            for ln in f:
                m = re.search(r"songMs=([\-\d.]+) audioTime=([\-\d.]+) streamPlaying=(\d)", ln)
                if m:
                    sm, at, sp = float(m.group(1)), float(m.group(2)), int(m.group(3))
                    if sp == 1 and at > 100 and sm > 100:
                        dbg.append((at, sm, at - sm))

        def stats(vals):
            if not vals: return (None, None, None, 0)
            vs = sorted(vals)
            mean = sum(vs) / len(vs)
            return (mean, vs[0], vs[-1], len(vs))

        dbg_leads = [d[2] for d in dbg]
        dm, dlo, dhi, dn = stats(dbg_leads)
        log(f"=== {tag} RESULTS ===")
        log(f"GAME_DBG (audioTime - songMs):  n={dn} mean={dm} range=[{dlo},{dhi}]")
        for at, sm, lead in dbg:
            log(f"   audioTime={at:8.1f} songMs={sm:8.1f} lead={lead:+7.1f}")
        print(json.dumps({"tag": tag, "dbg_mean": dm, "dbg_range": [dlo, dhi], "dbg_n": dn,
                          "av_cal_line": _avcal(log_path)}))
        return 0
    finally:
        try:
            proc.send_signal(signal.SIGTERM); proc.wait(timeout=8)
        except Exception:
            proc.kill()
        logf.close()

def _avcal(log_path):
    try:
        with open(log_path, errors="replace") as f:
            for ln in f:
                if "AV-cal" in ln:
                    return ln.strip()
    except Exception:
        pass
    return None

if __name__ == "__main__":
    sys.exit(main())
