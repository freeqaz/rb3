#!/usr/bin/env python3
"""W22-SWEEP: capture native main_hub after boot settles. Scripts+docs-only lane helper."""
import http.client, json, os, signal, socket, subprocess, sys, time

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
BIN = os.path.join(REPO, "native", "build-native", "rb3-native")
DATA = os.path.join(REPO, "orig-assets", "extracted")
OUT = sys.argv[1] if len(sys.argv) > 1 else "/tmp/w22-hub.png"


def free_port():
    s = socket.socket(); s.bind(("127.0.0.1", 0)); p = s.getsockname()[1]; s.close(); return p

def get(port, path, t=20):
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=t)
    try: c.request("GET", path); r = c.getresponse(); return r.status, r.read()
    finally: c.close()

def health(port):
    try:
        st, b = get(port, "/api/health")
        if st != 200: return None
        d = json.loads(b.decode("utf-8", "replace"))["data"]
        return int(d["frame"]), str(d["currentScreen"])
    except Exception: return None

port = free_port()
log = open(f"/tmp/w22-hub-{port}.log", "w")
env = dict(os.environ)
env.update({"RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
            "MILO_HEADLESS": "1", "RB3_DATA": DATA, "RB3_FIXED_CLOCK": "1",
            "RB3_GAME_INPUT": "@10:start,@30:confirm"})
proc = subprocess.Popen([BIN], env=env, stdout=log, stderr=subprocess.STDOUT,
                        cwd=REPO, start_new_session=True)
rc = 1
try:
    dl = time.time() + 120; settled = 0; reached = False
    while time.time() < dl:
        if proc.poll() is not None:
            print(f"FAIL: proc exited {proc.returncode}"); break
        h = health(port)
        if h:
            f, s = h
            print(f"  frame={f} screen='{s}'", flush=True)
            if s == "main_hub_screen":
                reached = True; settled += 1
                if settled >= 6:  # let hub anim settle
                    st, png = get(port, "/api/screenshot")
                    if st == 200 and png[:8] == b"\x89PNG\r\n\x1a\n":
                        open(OUT, "wb").write(png); print(f"PASS -> {OUT}"); rc = 0
                    break
        time.sleep(0.5)
    if not reached: print("FAIL: never reached main_hub")
finally:
    try:
        os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
        try: proc.wait(timeout=8)
        except subprocess.TimeoutExpired: os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
    except Exception: pass
    log.close()
sys.exit(rc)
