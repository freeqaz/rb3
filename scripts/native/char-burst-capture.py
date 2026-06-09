#!/usr/bin/env python3
"""
char-burst-capture.py — boot to gameplay, then capture a burst of screenshots
over ~N seconds to catch the venue cameras cutting to band-member closeups, and
keep the engine stderr so the BoneSetup BONE DIAG (fires frame>=1000) is captured.

Reuses the keyboard-to-gameplay nav verbatim, then loops capturing.
"""
import argparse, http.client, json, os, signal, socket, subprocess, sys, time

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DEFAULT_BIN = os.path.join(REPO, "native", "build-native", "rb3-native")
DEFAULT_DATA = os.path.join(REPO, "orig-assets", "extracted")

START, CONFIRM, CANCEL, STAR = 11, 6, 5, 8
DUP, DRIGHT, DDOWN, DLEFT = 12, 13, 14, 15
DIFF_INDEX = {"easy": 0, "medium": 1, "hard": 2, "expert": 3}


def log(m): print(f"[burst] {m}", flush=True)
def free_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM); s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]; s.close(); return p

def http_get(port, path, timeout=20):
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=timeout)
    try:
        c.request("GET", path); r = c.getresponse(); return r.status, r.read()
    finally: c.close()

def http_post(port, path, body, timeout=15):
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=timeout)
    try:
        c.request("POST", path, body=body, headers={"Content-Type": "text/plain"})
        r = c.getresponse(); return r.status, r.read().decode("utf-8", "replace")
    finally: c.close()

def health(port):
    try:
        st, b = http_get(port, "/api/health")
        if st != 200: return None
        d = json.loads(b.decode("utf-8", "replace"))["data"]
        return int(d["frame"]), float(d["songMs"]), str(d["currentScreen"])
    except Exception: return None

def dta(port, expr):
    try:
        st, b = http_post(port, "/api/dta/eval", expr, timeout=10)
        if st != 200: return None
        d = json.loads(b)
        if not d.get("ok"): return None
        return d["data"].get("value")
    except Exception: return None

def press(port, bit): http_post(port, "/api/input", f"pad:{bit}")
def verb(port, v): return http_post(port, "/api/input", v)

def screenshot(port, path):
    st, data = http_get(port, "/api/screenshot", timeout=25)
    if st != 200 or not data or data[:8] != b"\x89PNG\r\n\x1a\n": return False
    with open(path, "wb") as f: f.write(data)
    return True

def wait_screen(port, want, timeout, proc):
    dl = time.time() + timeout
    while time.time() < dl:
        if proc.poll() is not None: return None
        h = health(port)
        if h and ((callable(want) and want(h[2])) or h[2] == want): return h
        time.sleep(0.3)
    return None

def overshell(port):
    v = dta(port, "{rb3_overshell}")
    if not v: return ("?", "?", "?")
    parts = dict(p.split(":", 1) for p in v.split("|") if ":" in p)
    return (parts.get("view", "?"), parts.get("track", "?"), parts.get("diff", "?"))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=0)
    ap.add_argument("--bin", default=DEFAULT_BIN)
    ap.add_argument("--data", default=DEFAULT_DATA)
    ap.add_argument("--overlay", default=os.path.join(REPO, "native", "dta"))
    ap.add_argument("--out", default="/tmp/rb3-charburst")
    ap.add_argument("--shots", type=int, default=24)
    ap.add_argument("--interval", type=float, default=1.5)
    ap.add_argument("--extra-env", default="")
    args = ap.parse_args()

    os.makedirs(args.out, exist_ok=True)
    port = args.port or free_port()
    log_path = os.path.join("/tmp", f"rb3-charburst-{port}.log")
    logf = open(log_path, "w")
    env = dict(os.environ)
    env.update({"RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
                "MILO_HEADLESS": "1", "RB3_DATA": args.data,
                "RB3_DTA_OVERLAY": args.overlay})
    for kv in args.extra_env.split():
        if "=" in kv:
            k, v = kv.split("=", 1); env[k] = v
    log(f"launching rb3-native (port {port}); log -> {log_path}")
    proc = subprocess.Popen([args.bin], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)
    try:
        if wait_screen(port, lambda s: True, 40, proc) is None:
            log("FAIL: server never came up"); return 1
        # splash -> main_hub
        for _ in range(8):
            if health(port)[2] == "main_hub_screen": break
            press(port, START); time.sleep(0.7)
        # main_hub -> song_select
        for _ in range(10):
            if health(port)[2] == "song_select_screen": break
            press(port, CONFIRM); time.sleep(0.7)
        if wait_screen(port, "song_select_screen", 40, proc) is None:
            log("FAIL: no song_select"); return 1
        time.sleep(1.5)
        for _ in range(4): press(port, DDOWN); time.sleep(0.2)
        press(port, CONFIRM)
        if wait_screen(port, "part_difficulty_screen", 60, proc) is None:
            log("FAIL: no part_difficulty"); return 1
        time.sleep(1.0); press(port, CONFIRM); time.sleep(1.0)  # confirm part (guitar)
        # diff: scroll to hard
        ov = overshell(port); guard = 0
        while ov[0] == "confirm_action" and guard < 4:
            press(port, CONFIRM); time.sleep(0.5); ov = overshell(port); guard += 1
        for _ in range(2): press(port, DDOWN); time.sleep(0.25)
        press(port, CONFIRM); time.sleep(0.5)
        ov = overshell(port); guard = 0
        while ov[0] == "confirm_action" and guard < 4:
            press(port, CONFIRM); time.sleep(0.5); ov = overshell(port); guard += 1
        h = wait_screen(port, "game_screen", 90, proc)
        if h is None: log("FAIL: no game_screen"); return 1
        log(f"game_screen reached frame={h[0]} songMs={h[1]:.0f}")
        verb(port, "nofail")

        # burst capture
        for i in range(args.shots):
            verb(port, "autohit")
            h = health(port)
            ok = screenshot(port, os.path.join(args.out, f"shot_{i:02d}.png"))
            log(f"  shot {i:02d}: frame={h[0] if h else '?'} songMs={h[1] if h else -1:.0f} ok={ok}")
            time.sleep(args.interval)
        log(f"done. screenshots in {args.out}, log {log_path}")
        return 0
    finally:
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
            try: proc.wait(timeout=8)
            except subprocess.TimeoutExpired: os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
        except Exception: pass
        logf.close()


if __name__ == "__main__":
    sys.exit(main())
