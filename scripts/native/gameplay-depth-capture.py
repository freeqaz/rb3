#!/usr/bin/env python3
"""
gameplay-depth-capture.py — boot rb3-native headless to GAMEPLAY, let the song
play a few seconds so gems are travelling the highway, then capture PNG
screenshot(s). Used to investigate the "gems on the note highway get occluded by
background characters/objects" depth-ordering bug.

Reuses the boot-to-song nav from song-end-test.py. Captures at several song
times so we can see gems at different highway positions. Pass --bin to point at
an alternate (e.g. worktree) build.
"""
import argparse, http.client, json, os, socket, subprocess, sys, time

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DEFAULT_BIN = os.path.join(REPO, "native", "build-native", "rb3-native")
DEFAULT_DATA = os.path.join(REPO, "orig-assets", "extracted")

NAV_SCRIPT = (
    "@10:start,@30:confirm,"
    "@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn,"
    "@320:down,@350:msg:music_library:select_highlighted_node,"
    "@380:track:guitar,@450:msg:overshell:end_override_flow:1:0,"
    "@500:nofail,@520:autohit"
)

def log(m): print(f"[gp-capture] {m}", flush=True)
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
    try: st, b = http_get_bytes(port, "/api/health")
    except Exception: return None
    if st != 200: return None
    try:
        d = json.loads(b)["data"]; return int(d["frame"]), float(d["songMs"]), str(d["currentScreen"])
    except Exception: return None
def wait_for(port, pred, timeout, label, proc):
    deadline = time.time() + timeout; last = None
    while time.time() < deadline:
        if proc.poll() is not None:
            log(f"FAIL: process exited ({proc.returncode}) waiting for {label}"); return None
        h = health(port)
        if h is not None:
            if (h[0], h[2]) != last:
                log(f"  ...{label}: frame={h[0]} songMs={h[1]:.0f} screen='{h[2]}'"); last = (h[0], h[2])
            if pred(*h): return h
        time.sleep(0.5)
    return None
def screenshot(port, path):
    st, data = http_get_bytes(port, "/api/screenshot", timeout=25)
    if st != 200 or not data: return False
    with open(path, "wb") as f: f.write(data)
    return True

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=0)
    ap.add_argument("--bin", default=DEFAULT_BIN)
    ap.add_argument("--data", default=DEFAULT_DATA)
    ap.add_argument("--out", default="/tmp/rb3-gameplay")
    ap.add_argument("--times", default="3500,6000,9000",
                    help="comma list of songMs targets to capture at")
    ap.add_argument("--keep-log", action="store_true")
    args = ap.parse_args()

    os.makedirs(args.out, exist_ok=True)
    times = sorted(float(t) for t in args.times.split(","))
    port = args.port or free_port()
    log_path = os.path.join("/tmp", f"rb3-gp-{port}.log")
    logf = open(log_path, "w")
    env = dict(os.environ)
    env.update({
        "RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
        "MILO_HEADLESS": "1", "RB3_DATA": args.data, "RB3_GAME_INPUT": NAV_SCRIPT,
    })
    log(f"launching {os.path.basename(args.bin)} port={port} log={log_path}")
    proc = subprocess.Popen([args.bin], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)
    rc = 1
    try:
        if wait_for(port, lambda f, m, s: True, 40, "server-ready", proc) is None:
            log("FAIL: HTTP never came up"); return 1
        h = wait_for(port, lambda f, m, s: m > 2000.0, 220, "gameplay-start", proc)
        if h is None:
            log("FAIL: never reached gameplay"); return 1
        log(f"gameplay underway: frame={h[0]} songMs={h[1]:.0f} screen='{h[2]}'")
        captured = 0
        for t in times:
            h = wait_for(port, lambda f, m, s, _t=t: m >= _t, 60, f"songMs>={t:.0f}", proc)
            if h is None:
                log(f"WARN: never reached songMs {t}"); continue
            path = os.path.join(args.out, f"gameplay_{int(t):05d}ms.png")
            ok = screenshot(port, path)
            log(f"songMs~{h[1]:.0f}: screenshot={'OK' if ok else 'FAIL'} -> {path}")
            if ok: captured += 1
        rc = 0 if captured else 1
    finally:
        try: proc.send_signal(2); time.sleep(0.5); proc.kill()
        except Exception: pass
        logf.close()
        if not args.keep_log:
            try: os.remove(log_path)
            except OSError: pass
        else:
            log(f"engine log kept at {log_path}")
    return rc

if __name__ == "__main__":
    sys.exit(main())
