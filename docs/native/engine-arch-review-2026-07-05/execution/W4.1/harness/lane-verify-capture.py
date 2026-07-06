#!/usr/bin/env python3
"""
lane-verify-capture.py — C.S5 independent lane verify for W4.1 (Lane C UI parity).

Boots rb3-native to each of the three lane screens (main_hub, song_select,
part_difficulty), settles, and screenshots — parameterized by the
RB3_HUB_MENU_QUAD_HIDE flag state so an A/A (OFF) vs A/B (ON) pair can be
diffed for the main_hub grey-quad fix. song_select + part_difficulty are
captured to confirm no cross-screen regression.

    python3 lane-verify-capture.py --hide {0,1} --out DIR [--screens ...]

Exit 0 = reached every requested screen and captured. Exit 1 = boot/nav failed.
"""
import argparse, http.client, json, os, signal, socket, subprocess, sys, time

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "..", "..", "..", ".."))
DEFAULT_BIN = os.path.join(REPO, "native", "build-native", "rb3-native")
DEFAULT_DATA = os.path.join(REPO, "orig-assets", "extracted")

NAV = {
    "main_hub": "@10:start,@30:confirm",
    "song_select": ("@10:start,@30:confirm,@140:select:pn_quickplay.btn,"
                    "@220:select:qp_quickplay.btn"),
    "part_difficulty": ("@10:start,@30:confirm,@140:select:pn_quickplay.btn,"
                        "@220:select:qp_quickplay.btn,@320:down,"
                        "@350:msg:music_library:select_highlighted_node"),
}
TARGET = {
    "main_hub": "main_hub_screen",
    "song_select": "song_select_screen",
    "part_difficulty": "part_difficulty_screen",
}
READY_TIMEOUT = 45
SCREEN_TIMEOUT = 130


def log(m): print(f"[lane-verify] {m}", flush=True)
def free_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM); s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]; s.close(); return p

def http_get(port, path, timeout=20):
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=timeout)
    try:
        c.request("GET", path); r = c.getresponse(); return r.status, r.read()
    finally:
        c.close()

def health(port):
    try:
        st, b = http_get(port, "/api/health", timeout=8)
        if st != 200: return None
        d = json.loads(b.decode("utf-8", "replace"))["data"]
        return int(d["frame"]), float(d["songMs"]), str(d["currentScreen"])
    except Exception:
        return None

def screenshot(port, path):
    st, data = http_get(port, "/api/screenshot", timeout=25)
    if st != 200 or not data or data[:8] != b"\x89PNG\r\n\x1a\n":
        return False
    with open(path, "wb") as f: f.write(data)
    return True

def wait_for(port, pred, timeout, label, proc):
    dl = time.time() + timeout
    while time.time() < dl:
        if proc.poll() is not None:
            log(f"FAIL: process exited (code {proc.returncode}) waiting {label}"); return None
        h = health(port)
        if h is not None and pred(h): return h
        time.sleep(0.4)
    return None

def capture_screen(screen, hide, bin_path, data_dir, out_dir):
    port = free_port()
    log_path = os.path.join("/tmp", f"lane-verify-{screen}-{port}.log"); logf = open(log_path, "w")
    env = dict(os.environ)
    env.update({"RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
                "MILO_HEADLESS": "1", "RB3_FIXED_CLOCK": "1",
                "RB3_DATA": data_dir, "RB3_GAME_INPUT": NAV[screen]})
    if hide: env["RB3_HUB_MENU_QUAD_HIDE"] = "1"
    else: env.pop("RB3_HUB_MENU_QUAD_HIDE", None)
    proc = subprocess.Popen([bin_path], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)
    tag = "on" if hide else "off"
    try:
        if wait_for(port, lambda h: True, READY_TIMEOUT, "server", proc) is None:
            log(f"FAIL {screen}/{tag}: server never up"); return None
        h = wait_for(port, lambda h: h[2] == TARGET[screen], SCREEN_TIMEOUT, screen, proc)
        if h is None:
            log(f"FAIL {screen}/{tag}: never reached {TARGET[screen]}"); return None
        time.sleep(3.0)  # settle
        out = os.path.join(out_dir, f"{screen}_{tag}.png")
        ok = screenshot(port, out)
        log(f"{screen}/{tag}: frame={h[0]} screen='{h[2]}' shot={'OK' if ok else 'FAIL'} -> {out}")
        return out if ok else None
    finally:
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
            try: proc.wait(timeout=8)
            except subprocess.TimeoutExpired: os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
        except Exception: pass
        logf.close()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--hide", type=int, choices=[0, 1], required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--bin", default=DEFAULT_BIN)
    ap.add_argument("--data", default=DEFAULT_DATA)
    ap.add_argument("--screens", default="main_hub,song_select,part_difficulty")
    args = ap.parse_args()
    if not os.path.exists(args.bin):
        log(f"FAIL: binary not found: {args.bin}"); return 1
    os.makedirs(args.out, exist_ok=True)
    screens = [s.strip() for s in args.screens.split(",") if s.strip()]
    fail = 0
    for s in screens:
        if capture_screen(s, bool(args.hide), args.bin, args.data, args.out) is None:
            fail += 1
    log(f"{'PASS' if fail == 0 else 'FAIL'}: {len(screens)-fail}/{len(screens)} screens captured")
    return 1 if fail else 0


if __name__ == "__main__":
    sys.exit(main())
