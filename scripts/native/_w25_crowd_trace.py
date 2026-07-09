#!/usr/bin/env python3
"""W25-CROWD STEP 0 trace: boot to main_hub with CHARDRV_PROBE=crowd set, so the
extended CharDriver probe reports each crowd proxy driver's mDefaultClip / mClips /
nclips and whether CharDriver::Enter fired. Runs {rb3_crowd_census} for the
animating/clip discriminator. Read-only / probe-only; probes inert unless env set.

Usage: _w25_crowd_trace.py <outdir> [dwell_s]
"""
import http.client, json, os, signal, socket, subprocess, sys, time

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
BIN = os.path.join(REPO, "native", "build-native", "rb3-native")
DATA = os.path.join(REPO, "orig-assets", "extracted")
OUT = sys.argv[1] if len(sys.argv) > 1 else "/tmp/w25-crowd-trace"
DWELL = float(sys.argv[2]) if len(sys.argv) > 2 else 8.0
NAV = "@10:start,@30:confirm"

def log(m): print(f"[w25trace] {m}", flush=True)
def free_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM); s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]; s.close(); return p
def hget(port, path, t=20):
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=t)
    try: c.request("GET", path); r = c.getresponse(); return r.status, r.read()
    finally: c.close()
def hpost(port, path, body, t=15):
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=t)
    try:
        c.request("POST", path, body=body, headers={"Content-Type": "text/plain"})
        r = c.getresponse(); return r.status, r.read()
    finally: c.close()
def health(port):
    try:
        st, b = hget(port, "/api/health", 10)
        if st != 200: return None
        d = json.loads(b).get("data", {})
        return int(d["frame"]), str(d.get("currentScreen", "?"))
    except Exception: return None
def dta(port, expr):
    try:
        st, b = hpost(port, "/api/dta/eval", expr, 12)
        if st != 200: return f"HTTP {st}"
        d = json.loads(b)
        return d.get("data", {}).get("value") if d.get("ok") else f"err:{d}"
    except Exception as e: return f"exc:{e}"

def main():
    os.makedirs(OUT, exist_ok=True)
    port = free_port()
    logp = os.path.join(OUT, "engine.log"); lf = open(logp, "w")
    env = dict(os.environ)
    env.update({"RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
                "MILO_HEADLESS": "1", "RB3_FIXED_CLOCK": "1", "RB3_DATA": DATA,
                "RB3_GAME_INPUT": NAV,
                "CHARDRV_PROBE": "crowd",  # match owning-dir name crowd_maleNN
                "CROWD_CENSUS_VERBOSE": "1"})
    # allow extra probe envs via caller
    for k in ("EXTRA_ENV",):
        pass
    log(f"launch port={port} log={logp}")
    proc = subprocess.Popen([BIN], env=env, stdout=lf, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)
    try:
        dl = time.time() + 60; up = False; tries = 0
        while time.time() < dl:
            if proc.poll() is not None: log(f"proc exited {proc.returncode}"); return 1
            h = health(port); tries += 1
            if h is not None: up = True; break
            time.sleep(0.4)
        if not up: log("server never up"); return 1
        log("server up")
        dl = time.time() + 120; scr = None
        while time.time() < dl:
            if proc.poll() is not None: log(f"proc exited {proc.returncode}"); return 1
            h = health(port)
            if h:
                if h[1] != scr: scr = h[1]; log(f"  frame={h[0]} screen={scr}")
                if scr and "main_hub" in scr: break
            time.sleep(0.4)
        log(f"at screen={scr}; dwelling {DWELL}s")
        time.sleep(DWELL)
        h = health(port); log(f"post-dwell frame={h[0] if h else '?'} screen={h[1] if h else '?'}")
        cc = dta(port, "{rb3_crowd_census}")
        log(f"crowd_census: {cc}")
        with open(os.path.join(OUT, "crowd_census.txt"), "w") as f: f.write(str(cc) + "\n")
        st, data = hget(port, "/api/screenshot", 20)
        if st == 200 and data[:8] == b"\x89PNG\r\n\x1a\n":
            with open(os.path.join(OUT, "main_hub.png"), "wb") as f: f.write(data)
            log("screenshot OK")
        return 0
    finally:
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
            try: proc.wait(timeout=8)
            except subprocess.TimeoutExpired: os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
        except Exception: pass
        lf.close()
        log(f"engine log -> {logp}")

if __name__ == "__main__": sys.exit(main())
