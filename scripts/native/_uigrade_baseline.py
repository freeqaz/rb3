#!/usr/bin/env python3
"""
_uigrade_baseline.py — Wave-13 Lane G (UIGRADE) per-screen contrast baselines.

Boots rb3-native headless, drives to the three A7-gated screens
(hub focused / song-select highlighted row / partdiff GUITAR), and screenshots
each. Run once per ARM (default and RB3_PP_OFF=1) via --arm; the env flag is
taken from the ambient environment so the caller sets RB3_PP_OFF.

  RB3_HUB_TEXT_CONTRAST is left UNSET in all arms (A7: it worsens the metric).

    python3 scripts/native/_uigrade_baseline.py --arm default --out /tmp/uigrade
    RB3_PP_OFF=1 python3 scripts/native/_uigrade_baseline.py --arm ppoff --out /tmp/uigrade

Exit 0 = reached all three screens and captured. Screens saved as
<out>/<arm>_hub.png, <arm>_songselect.png, <arm>_partdiff.png.
"""
import argparse, http.client, json, os, signal, socket, subprocess, sys, time

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DEFAULT_BIN = os.path.join(REPO, "native", "build-native", "rb3-native")
DEFAULT_DATA = os.path.join(REPO, "orig-assets", "extracted")

def log(m): print(f"[uigrade] {m}", flush=True)

def free_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM); s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]; s.close(); return p

def http_get_bytes(port, path, timeout=20):
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
        st, b = http_get_bytes(port, "/api/health")
        if st != 200: return None
        d = json.loads(b.decode("utf-8", "replace"))["data"]
        return int(d["frame"]), float(d["songMs"]), str(d["currentScreen"])
    except Exception:
        return None

def wait_for(port, pred, timeout, label, proc):
    dl = time.time() + timeout; last = None
    while time.time() < dl:
        if proc.poll() is not None:
            log(f"FAIL: process exited (code {proc.returncode}) waiting for {label}"); return None
        h = health(port)
        if h is not None:
            f, m, s = h
            if (f, s) != last:
                log(f"  ...{label}: frame={f} screen='{s}'"); last = (f, s)
            if pred(f, m, s): return h
        time.sleep(0.3)
    return None

def screenshot(port, path):
    st, data = http_get_bytes(port, "/api/screenshot", timeout=20)
    if st != 200 or not data or data[:8] != b"\x89PNG\r\n\x1a\n":
        log(f"FAIL screenshot: status={st} len={len(data) if data else 0}")
        return False
    with open(path, "wb") as f: f.write(data)
    log(f"OK screenshot -> {os.path.basename(path)}")
    return True

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--arm", required=True)
    ap.add_argument("--port", type=int, default=0)
    ap.add_argument("--bin", default=DEFAULT_BIN)
    ap.add_argument("--data", default=DEFAULT_DATA)
    ap.add_argument("--out", default="/tmp/uigrade")
    args = ap.parse_args()

    if not os.path.exists(args.bin):
        log(f"FAIL: binary not found: {args.bin}"); return 1
    os.makedirs(args.out, exist_ok=True)
    port = args.port or free_port()
    log_path = f"/tmp/rb3-uigrade-{args.arm}-{port}.log"
    logf = open(log_path, "w")
    env = dict(os.environ)
    env.update({"RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
                "MILO_HEADLESS": "1", "RB3_DATA": args.data, "RB3_FIXED_CLOCK": "1",
                "RB3_GAME_INPUT": "@10:start"})
    env.pop("RB3_HUB_TEXT_CONTRAST", None)  # A7: keep OFF in all arms
    log(f"arm={args.arm} port={port} PP_OFF={env.get('RB3_PP_OFF','<unset>')} log={log_path}")
    proc = subprocess.Popen([args.bin], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)
    rc = 1
    try:
        if wait_for(port, lambda f, m, s: True, 40, "server", proc) is None:
            log("FAIL: server never up"); return 1
        # --- Screen 1: hub focused (PLAY NOW default focus) ---
        if wait_for(port, lambda f, m, s: s == "main_hub_screen", 90, "main_hub", proc) is None:
            log("FAIL: never reached main_hub"); return 1
        time.sleep(2.5)
        if not screenshot(port, os.path.join(args.out, f"{args.arm}_hub.png")): return 1
        # --- Screen 2: song-select highlighted row ---
        http_post(port, "/api/input", "select:pn_quickplay.btn"); time.sleep(1.0)
        http_post(port, "/api/input", "select:qp_quickplay.btn"); time.sleep(1.0)
        if wait_for(port, lambda f, m, s: s == "song_select_screen", 90, "song_select", proc) is None:
            log("FAIL: never reached song_select"); return 1
        time.sleep(2.5)
        for _ in range(3):
            http_post(port, "/api/input", "down"); time.sleep(0.25)
        time.sleep(1.5)
        if not screenshot(port, os.path.join(args.out, f"{args.arm}_songselect.png")): return 1
        # --- Screen 3: partdiff GUITAR ---
        http_post(port, "/api/input", "msg:music_library:select_highlighted_node"); time.sleep(0.5)
        if wait_for(port, lambda f, m, s: s == "part_difficulty_screen", 90, "partdiff", proc) is None:
            log("FAIL: never reached part_difficulty"); return 1
        time.sleep(3.0)
        # ensure GUITAR row highlighted (part: verb targets guitar directly)
        http_post(port, "/api/input", "part:guitar"); time.sleep(1.0)
        if not screenshot(port, os.path.join(args.out, f"{args.arm}_partdiff.png")): return 1
        rc = 0
        log(f"PASS arm={args.arm}: 3 screens captured in {args.out}")
        return 0
    finally:
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
            try: proc.wait(timeout=8)
            except subprocess.TimeoutExpired: os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
        except Exception: pass
        logf.close()
        log(f"engine log: {log_path}")

if __name__ == "__main__":
    sys.exit(main())
