#!/usr/bin/env python3
"""C2a census + capture. Boots build-agent-W4.3-C2 with RB3_SS_CENSUS=1 (+ any
extra env passed as KEY=VAL argv), navigates to song_select, captures the sidebar,
and dumps the [C2DIR]/[C2OBJ]/[C2GRP*]/[C2CENSUS]/[C2BG] census lines.
pgid-only cleanup (start_new_session + killpg)."""
import http.client, json, os, signal, socket, subprocess, sys, time

REPO = "/home/free/code/milohax/rb3"
BIN = os.path.join(REPO, "native", "build-agent-W4.3-C2", "rb3-native")
DATA = os.path.join(REPO, "orig-assets", "extracted")
OUT = "/tmp/wave13-c2a"

def log(m): print(f"[c2a] {m}", flush=True)
def free_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM); s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]; s.close(); return p
def http_get_bytes(port, path, timeout=20):
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=timeout)
    try: c.request("GET", path); r = c.getresponse(); return r.status, r.read()
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
    except Exception: return None
def wait_for(port, pred, timeout, label, proc):
    dl = time.time() + timeout; last = None
    while time.time() < dl:
        if proc.poll() is not None:
            log(f"FAIL: process exited ({proc.returncode}) waiting {label}"); return None
        h = health(port)
        if h is not None:
            f, m, s = h
            if (f, s) != last: log(f"  ...{label}: frame={f} screen='{s}'"); last=(f,s)
            if pred(f, m, s): return h
        time.sleep(0.3)
    return None
def screenshot(port, name):
    st, data = http_get_bytes(port, "/api/screenshot", timeout=20)
    if st != 200 or not data or data[:8] != b"\x89PNG\r\n\x1a\n":
        log(f"FAIL screenshot {name}: status={st}"); return False
    with open(os.path.join(OUT, name), "wb") as f: f.write(data)
    log(f"OK screenshot -> {name}"); return True

def main():
    os.makedirs(OUT, exist_ok=True)
    port = free_port()
    log_path = os.path.join(OUT, f"census-{port}.log")
    logf = open(log_path, "w")
    env = dict(os.environ)
    env.update({
        "RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
        "MILO_HEADLESS": "1", "RB3_DATA": DATA, "RB3_FIXED_CLOCK": "1",
        "RB3_GAME_INPUT": "@10:start", "RB3_SS_CENSUS": "1",
    })
    extra = sys.argv[1:]
    shot = "ss_census.png"
    for kv in extra:
        if kv.startswith("SHOT="): shot = kv.split("=",1)[1]; continue
        k, _, v = kv.partition("="); env[k] = v
    log(f"port={port} log={log_path} shot={shot} extra={extra}")
    proc = subprocess.Popen([BIN], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)
    try:
        if wait_for(port, lambda f,m,s: True, 40, "server", proc) is None: return 1
        if wait_for(port, lambda f,m,s: s=="main_hub_screen", 60, "main_hub", proc) is None: return 1
        time.sleep(2.0)
        http_post(port, "/api/input", "select:pn_quickplay.btn"); time.sleep(1.0)
        http_post(port, "/api/input", "select:qp_quickplay.btn"); time.sleep(1.0)
        if wait_for(port, lambda f,m,s: s=="song_select_screen", 60, "song_select", proc) is None: return 1
        time.sleep(2.5)
        screenshot(port, shot)
        for _ in range(3):
            http_post(port, "/api/input", "down"); time.sleep(0.25)
        time.sleep(2.0)
        screenshot(port, shot.replace(".png","_scrolled.png"))
        return 0
    finally:
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
            try: proc.wait(timeout=8)
            except subprocess.TimeoutExpired: os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
        except Exception: pass
        logf.close()
        try:
            with open(log_path) as f:
                lines = [l for l in f if any(t in l for t in
                         ("[C2DIR]","[C2OBJ]","[C2GRPMEMB]","[C2CENSUS]","[C2BG]","[C2GRP]"))]
            print(f"[c2a] === {len(lines)} census lines ===", flush=True)
            for l in lines: print(l.rstrip(), flush=True)
            with open(os.path.join(OUT, "census.txt"), "w") as f: f.writelines(lines)
        except Exception as e:
            print(f"[c2a] logdump failed: {e}", flush=True)

if __name__ == "__main__":
    sys.exit(main())
