#!/usr/bin/env python3
"""
glow_ab_capture.py — W26-GLOW lane. Drive rb3-native headless into a DRIVEN-COMBO
gameplay state (natural autohit build → >=4x multiplier → PeakState/now-bar glow)
and screenshot with RB3_SMASHER_HALO on vs off for the S5 combo-glow A/B.

autohit hits every gem, so the streak multiplier climbs to 4x within a few seconds
of gameplay. We wait a fixed dwell past gameplay-start (well past the 4x threshold),
then grab a burst of screenshots. Run once per flag value (env passed through).

Usage:
    RB3_SMASHER_HALO=1 python3 scripts/native/glow_ab_capture.py OUT_PREFIX [--dwell 12] [--shots 3]
"""
import argparse, http.client, json, os, signal, socket, subprocess, sys, time

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DEFAULT_BIN = os.path.join(REPO, "native", "build-native", "rb3-native")
DEFAULT_DATA = os.path.join(REPO, "orig-assets", "extracted")
GAMEPLAY_SONGMS = 2000.0

NAV_TO_SELECT = ("@10:start,@30:confirm,"
                 "@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn")


def log(m): print(f"[glow] {m}", flush=True)


def free_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.bind(("127.0.0.1", 0)); p = s.getsockname()[1]; s.close()
    return p


def http_get(port, path, timeout=6):
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=timeout)
    try:
        c.request("GET", path); r = c.getresponse(); return r.status, r.read()
    finally:
        c.close()


def http_get_bytes(port, path, timeout=20):
    return http_get(port, path, timeout)


def http_post(port, path, body, timeout=10):
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=timeout)
    try:
        c.request("POST", path, body); r = c.getresponse(); return r.status, r.read()
    finally:
        c.close()


def health(port):
    try:
        st, b = http_get(port, "/api/health")
        if st != 200: return None
        j = json.loads(b)
        d = j.get("data", j)  # health payload is nested under "data" (JsonOk wrapper)
        return (d.get("frame", 0), float(d.get("songMs", 0.0)), d.get("currentScreen", "?"))
    except Exception:
        return None


def screenshot(port, path):
    try:
        st, data = http_get_bytes(port, "/api/screenshot", timeout=25)
        if st != 200 or not data: return False
        with open(path, "wb") as f: f.write(data)
        return True
    except Exception:
        return False


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out_prefix")
    ap.add_argument("--bin", default=DEFAULT_BIN)
    ap.add_argument("--data", default=DEFAULT_DATA)
    ap.add_argument("--dwell", type=float, default=14.0,
                    help="seconds of gameplay to dwell before shots (streak > 4x)")
    ap.add_argument("--shots", type=int, default=3)
    args = ap.parse_args()

    port = free_port()
    env = dict(os.environ)
    env.update({
        "RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
        "MILO_HEADLESS": "1", "RB3_FIXED_CLOCK": "1",
        "RB3_DATA": args.data, "RB3_GAME_INPUT": NAV_TO_SELECT,
    })
    log(f"port={port} RB3_SMASHER_HALO={env.get('RB3_SMASHER_HALO','<unset>')}")

    logf = open(f"/tmp/rb3-glow-{port}.log", "w")
    proc = subprocess.Popen(
        [args.bin], env=env, stdout=logf, stderr=subprocess.STDOUT,
        cwd=REPO, preexec_fn=os.setsid)
    pgid = os.getpgid(proc.pid)
    rc = 1
    try:
        # server ready
        dl = time.time() + 40
        while time.time() < dl:
            if proc.poll() is not None:
                log(f"FAIL proc exited {proc.returncode} at boot"); return 1
            if health(port): break
            time.sleep(0.4)
        log("server ready")

        # wait for song_select
        dl = time.time() + 60
        while time.time() < dl:
            h = health(port)
            if h and h[2] == "song_select_screen": break
            if proc.poll() is not None:
                log(f"FAIL exited {proc.returncode} before song_select"); return 1
            time.sleep(0.4)
        log("song_select reached; selecting first song")
        time.sleep(1.0)
        http_post(port, "/api/input", "down"); time.sleep(0.5)
        http_post(port, "/api/input", "msg:music_library:select_highlighted_node")
        time.sleep(1.0)

        # part_difficulty
        dl = time.time() + 60
        while time.time() < dl:
            h = health(port)
            if h and h[2] == "part_difficulty_screen": break
            if proc.poll() is not None:
                log(f"FAIL exited {proc.returncode} before part_difficulty"); return 1
            time.sleep(0.4)
        log("part_difficulty; committing guitar/expert/nofail/autohit")
        time.sleep(1.0)
        http_post(port, "/api/input", "part:guitar"); time.sleep(1.0)
        http_post(port, "/api/input", "diff:expert"); time.sleep(1.0)
        http_post(port, "/api/input", "nofail"); time.sleep(0.3)
        http_post(port, "/api/input", "autohit"); time.sleep(0.3)

        # wait for gameplay (game_screen)
        dl = time.time() + 280
        started = None
        while time.time() < dl:
            h = health(port)
            if h and h[2] == "game_screen":
                started = h; break
            if proc.poll() is not None:
                log(f"FAIL exited {proc.returncode} before gameplay"); return 1
            time.sleep(0.5)
        if not started:
            log("FAIL never reached game_screen"); return 1
        log(f"gameplay underway frame={started[0]} songMs={started[1]:.0f}")

        # dwell so autohit builds streak past 4x
        log(f"dwelling {args.dwell}s for streak build")
        time.sleep(args.dwell)

        for i in range(args.shots):
            h = health(port)
            path = f"{args.out_prefix}_{i}.png"
            ok = screenshot(port, path)
            log(f"shot {i}: {'OK' if ok else 'FAIL'} -> {path}  songMs={h[1]:.0f} frame={h[0]}")
            time.sleep(0.8)
        rc = 0
    finally:
        try: os.killpg(pgid, signal.SIGTERM)
        except Exception: pass
        try: proc.wait(timeout=8)
        except Exception:
            try: os.killpg(pgid, signal.SIGKILL)
            except Exception: pass
    return rc


if __name__ == "__main__":
    sys.exit(main())
