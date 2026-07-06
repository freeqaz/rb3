#!/usr/bin/env python3
"""W5.1 C.S1 — venue black poster quad census.
Boot rb3-native to part_difficulty with RB3_HEADMAT_DBG=1, settle, press
part:guitar to zoom into the poster wall, capture screenshots + the full
[HEADMAT] material census (engine stderr). Zero new code — pure probe."""
import http.client, json, os, signal, socket, subprocess, sys, time

REPO = "/home/free/code/milohax/rb3"
BIN = os.path.join(REPO, "native", "build-agent-CS1", "rb3-native")
DATA = os.path.join(REPO, "orig-assets", "extracted")
OUT = "/tmp/w51-census"
# Reach part_difficulty, then press part:guitar to zoom the camera into the
# poster/menu backdrop wall (the frame-390-style zoom W4.1 identified).
NAV = ("@10:start,@30:confirm,@140:select:pn_quickplay.btn,"
       "@220:select:qp_quickplay.btn,@320:down,"
       "@350:msg:music_library:select_highlighted_node,"
       "@620:part:guitar")

def free_port():
    s = socket.socket(); s.bind(("127.0.0.1", 0)); p = s.getsockname()[1]; s.close(); return p
def hget(port, path, t=20):
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=t)
    try: c.request("GET", path); r = c.getresponse(); return r.status, r.read()
    finally: c.close()
def health(port):
    try:
        st, b = hget(port, "/api/health")
        if st != 200: return None
        d = json.loads(b.decode())["data"]
        return int(d["frame"]), float(d["songMs"]), str(d["currentScreen"])
    except Exception: return None
def shot(port, path):
    st, data = hget(port, "/api/screenshot")
    if st != 200 or data[:8] != b"\x89PNG\r\n\x1a\n": return False
    open(path, "wb").write(data); return True

def main():
    os.makedirs(OUT, exist_ok=True)
    port = free_port()
    logp = os.path.join(OUT, f"engine_{port}.log"); logf = open(logp, "w")
    env = dict(os.environ)
    env.update({"RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
                "MILO_HEADLESS": "1", "RB3_DATA": DATA, "RB3_GAME_INPUT": NAV,
                "RB3_FIXED_CLOCK": "1", "RB3_HEADMAT_DBG": "1"})
    # A7: ensure RB3_PP_LUMA_CEILING is unset
    env.pop("RB3_PP_LUMA_CEILING", None)
    print(f"[census] launch port={port} log={logp}", flush=True)
    proc = subprocess.Popen([BIN], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)
    try:
        dl = time.time() + 60
        while time.time() < dl and health(port) is None:
            if proc.poll() is not None: print("[census] proc died early"); return 1
            time.sleep(0.4)
        print("[census] server up", flush=True)
        dl = time.time() + 120; arrived = None
        while time.time() < dl:
            if proc.poll() is not None: print("[census] proc died"); return 1
            h = health(port)
            if h and h[2] == "part_difficulty_screen":
                arrived = h[0]; break
            time.sleep(0.3)
        if arrived is None:
            print("[census] never reached part_difficulty_screen"); return 1
        print(f"[census] part_difficulty at frame {arrived}", flush=True)
        for tgt in [0, 60, 150, 300, 450]:
            dl2 = time.time() + 40
            while time.time() < dl2:
                h = health(port)
                if h and h[0] >= arrived + tgt: break
                time.sleep(0.15)
            h = health(port)
            path = os.path.join(OUT, f"partdiff_{tgt:03d}.png")
            ok = shot(port, path)
            print(f"[census] +{tgt:>3}f frame={h[0] if h else '?'} screen={h[2] if h else '?'} shot={'OK' if ok else 'FAIL'}", flush=True)
        print("[census] DONE", flush=True)
        return 0
    finally:
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
            try: proc.wait(timeout=8)
            except subprocess.TimeoutExpired: os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
        except Exception: pass
        logf.close()
        print(f"[census] engine log: {logp}", flush=True)

if __name__ == "__main__":
    sys.exit(main())
