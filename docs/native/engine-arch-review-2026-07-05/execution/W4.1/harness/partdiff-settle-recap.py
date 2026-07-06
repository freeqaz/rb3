#!/usr/bin/env python3
"""Recapture part_difficulty with generous settle frames + a sequence, to rule
out a mid-transition frame (Wave6 W4.1 subitem c diagnosis)."""
import http.client, json, os, signal, socket, subprocess, sys, time

REPO = "/home/free/code/milohax/rb3"
BIN = os.path.join(REPO, "native", "build-native", "rb3-native")
DATA = os.path.join(REPO, "orig-assets", "extracted")
OUT = "/tmp/wave6-partdiff-recap"
# Navigate to part_difficulty and STOP (no part:/diff: press).
NAV = ("@10:start,@30:confirm,@140:select:pn_quickplay.btn,"
       "@220:select:qp_quickplay.btn,@320:down,"
       "@350:msg:music_library:select_highlighted_node")

def free_port():
    s = socket.socket(); s.bind(("127.0.0.1", 0)); p = s.getsockname()[1]; s.close(); return p
def hget(port, path, t=20):
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=t)
    try: c.request("GET", path); r = c.getresponse(); return r.status, r.read()
    finally: c.close()
def hpost(port, path, body, t=15):
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=t)
    try:
        c.request("POST", path, body=body, headers={"Content-Type": "text/plain"})
        r = c.getresponse(); return r.status, r.read().decode("utf-8", "replace")
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
def deval(port, expr):
    try:
        st, b = hpost(port, "/api/dta/eval", expr, t=10)
        if st != 200: return None
        d = json.loads(b)
        return d["data"].get("value") if d.get("ok") else None
    except Exception: return None

def main():
    os.makedirs(OUT, exist_ok=True)
    port = free_port()
    logp = f"/tmp/rb3-partdiff-recap-{port}.log"; logf = open(logp, "w")
    env = dict(os.environ)
    env.update({"RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
                "MILO_HEADLESS": "1", "RB3_DATA": DATA, "RB3_GAME_INPUT": NAV,
                "RB3_FIXED_CLOCK": "1"})
    print(f"[recap] launch port={port} log={logp}", flush=True)
    proc = subprocess.Popen([BIN], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)
    try:
        # wait for server
        dl = time.time() + 40
        while time.time() < dl and health(port) is None:
            if proc.poll() is not None: print("[recap] proc died early"); return 1
            time.sleep(0.4)
        print("[recap] server up", flush=True)
        # wait for part_difficulty
        dl = time.time() + 120; arrived = None
        while time.time() < dl:
            if proc.poll() is not None: print("[recap] proc died"); return 1
            h = health(port)
            if h:
                f, m, s = h
                if s == "part_difficulty_screen":
                    arrived = f; break
            time.sleep(0.3)
        if arrived is None:
            print("[recap] never reached part_difficulty_screen"); return 1
        print(f"[recap] part_difficulty at frame {arrived}", flush=True)
        # capture a sequence: settle windows in frames after arrival
        targets = [0, 30, 60, 120, 180, 240, 360]
        for tgt in targets:
            # busy-wait until frame >= arrived + tgt
            dl2 = time.time() + 30
            while time.time() < dl2:
                h = health(port)
                if h and h[0] >= arrived + tgt: break
                time.sleep(0.15)
            h = health(port)
            path = os.path.join(OUT, f"partdiff_settle_{tgt:03d}.png")
            ok = shot(port, path)
            # probe some panel state via dta
            print(f"[recap] +{tgt:>3}f: frame={h[0] if h else '?'} screen={h[2] if h else '?'} shot={'OK' if ok else 'FAIL'} -> {path}", flush=True)
        print("[recap] DONE", flush=True)
        return 0
    finally:
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
            try: proc.wait(timeout=8)
            except subprocess.TimeoutExpired: os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
        except Exception: pass
        logf.close()
        print(f"[recap] engine log: {logp}", flush=True)

if __name__ == "__main__":
    sys.exit(main())
