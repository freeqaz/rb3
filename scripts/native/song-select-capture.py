#!/usr/bin/env python3
"""
song-select-capture.py — headless native song_select visual capture.

Boots rb3-native to the Music Library (song_select) screen, scrolls the song
list by injecting `down` verbs over the embedded HTTP debug server, and writes a
PNG screenshot (/api/screenshot) at several scroll depths. Far faster than the
browser/web loop: no Playwright, no brotli, ~3s incremental rebuilds.

Used to validate the song_select rendering fixes (stale header-text overlap;
album-art center bleed-through) and as the template for native UI visual tests.

    python3 scripts/native/song-select-capture.py [--port N] [--data DIR]
            [--bin PATH] [--depths 0,8,16,30,50] [--out DIR] [--verbose]

Exit 0 = reached song_select and captured all depths. Exit 1 = boot/nav failed.
"""
import argparse, http.client, json, os, signal, socket, subprocess, sys, time

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DEFAULT_BIN = os.path.join(REPO, "native", "build-native", "rb3-native")
DEFAULT_DATA = os.path.join(REPO, "orig-assets", "extracted")

# Boot → main_hub → PLAY NOW → QUICKPLAY → song_select, then STOP (no song
# confirm — we stay in the library to scroll/inspect). Mirrors the song-end nav
# up to entering the library.
NAV_SCRIPT = (
    "@10:start,@30:confirm,"
    "@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn"
)
SERVER_READY_TIMEOUT = 40
SONGSELECT_TIMEOUT = 120


def log(m): print(f"[ss-capture] {m}", flush=True)
def free_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM); s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]; s.close(); return p

def http_get_bytes(port, path, timeout=10):
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=timeout)
    try:
        c.request("GET", path); r = c.getresponse(); return r.status, r.read()
    finally:
        c.close()

def http_post(port, path, body, timeout=15):
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=timeout)
    try:
        c.request("POST", path, body=body, headers={"Content-Type": "text/plain"})
        r = c.getresponse(); return r.status, r.read().decode("utf-8", "replace")
    finally:
        c.close()

def health(port):
    try:
        st, b = http_get_bytes(port, "/api/health")
        if st != 200: return None
        d = json.loads(b.decode("utf-8", "replace"))["data"]
        return int(d["frame"]), float(d["songMs"]), str(d["currentScreen"])
    except Exception:
        return None

def dta_eval(port, expr):
    try:
        st, b = http_post(port, "/api/dta/eval", expr, timeout=10)
        if st != 200: return None
        d = json.loads(b)
        if not d.get("ok"): return None
        return d["data"].get("value")
    except Exception:
        return None

def wait_for(port, pred, timeout, label, verbose, proc):
    dl = time.time() + timeout; last = None
    while time.time() < dl:
        if proc.poll() is not None:
            log(f"FAIL: process exited (code {proc.returncode}) waiting for {label}"); return None
        h = health(port)
        if h is not None:
            f, m, s = h
            if verbose and (f, s) != last:
                log(f"  ...{label}: frame={f} songMs={m:.0f} screen='{s}'"); last = (f, s)
            if pred(f, m, s): return h
        time.sleep(0.4)
    return None

def screenshot(port, path):
    st, data = http_get_bytes(port, "/api/screenshot", timeout=20)
    if st != 200 or not data or data[:8] != b"\x89PNG\r\n\x1a\n":
        return False
    with open(path, "wb") as f: f.write(data)
    return True


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=0)
    ap.add_argument("--bin", default=DEFAULT_BIN)
    ap.add_argument("--data", default=DEFAULT_DATA)
    ap.add_argument("--depths", default="0,8,16,30,50")
    ap.add_argument("--out", default="/tmp/rb3-ss-native")
    ap.add_argument("--verbose", action="store_true")
    ap.add_argument("--keep-log", action="store_true")
    args = ap.parse_args()

    if not os.path.exists(args.bin):
        log(f"FAIL: binary not found: {args.bin}")
        log("      build it: cmake --build native/build-native --target rb3-native"); return 1
    os.makedirs(args.out, exist_ok=True)
    depths = sorted(int(x) for x in args.depths.split(",") if x.strip())

    port = args.port or free_port()
    log_path = os.path.join("/tmp", f"rb3-ss-{port}.log"); logf = open(log_path, "w")
    env = dict(os.environ)
    env.update({"RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
                "MILO_HEADLESS": "1", "RB3_DATA": args.data, "RB3_GAME_INPUT": NAV_SCRIPT})
    log(f"launching rb3-native (port {port}, headless), log -> {log_path}")
    proc = subprocess.Popen([args.bin], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)
    rc = 1
    try:
        if wait_for(port, lambda f, m, s: True, SERVER_READY_TIMEOUT, "server", args.verbose, proc) is None:
            log("FAIL: HTTP server never came up"); return 1
        log("HTTP server up")
        h = wait_for(port, lambda f, m, s: s == "song_select_screen",
                     SONGSELECT_TIMEOUT, "song_select", args.verbose, proc)
        if h is None:
            log("FAIL: never reached song_select_screen"); return 1
        log(f"song_select reached: frame={h[0]} screen='{h[2]}'")
        time.sleep(2.0)  # let the list populate + enter anim settle

        cur = 0
        for d in depths:
            while cur < d:
                http_post(port, "/api/input", "down"); cur += 1
                time.sleep(0.12)
            time.sleep(0.5)
            hl = dta_eval(port, "{music_library get_highlighted_node}")
            path = os.path.join(args.out, f"native_depth_{d:02d}.png")
            ok = screenshot(port, path)
            log(f"depth {d:>2}: screenshot={'OK' if ok else 'FAIL'} -> {path}  highlight={hl}")
            if not ok:
                log("FAIL: screenshot failed"); return 1
        rc = 0
        log(f"PASS: captured {len(depths)} frames in {args.out}")
        return 0
    finally:
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
            try: proc.wait(timeout=8)
            except subprocess.TimeoutExpired: os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
        except Exception: pass
        logf.close()
        if rc != 0 or args.keep_log: log(f"engine log: {log_path}")
        elif os.path.exists(log_path):
            try: os.remove(log_path)
            except OSError: pass


if __name__ == "__main__":
    sys.exit(main())
