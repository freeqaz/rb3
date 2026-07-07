#!/usr/bin/env python3
"""W2.8f — trustworthy hands instrument capture harness.

Drives rb3-native to gameplay with RB3_HANDS_ATTACH_PROBE ON so the new palette-
internal Tier-1/Tier-2 invariants co-sample across an animated hands window. Also
captures a clean-body control mesh (greaserjacket_resource) in the SAME run.

Reuses the W2.8e/A.S3 nav + launch pattern. Build dir = build-agent-W2.8f.
"""
import argparse, http.client, json, os, signal, socket, subprocess, sys, time

REPO = "/home/free/code/milohax/rb3"
BIN = os.path.join(REPO, "native", "build-agent-W2.8f", "rb3-native")
DATA = os.path.join(REPO, "orig-assets", "extracted")

NAV_GAMEPLAY = (
    "@10:start,@30:confirm,@140:select:pn_quickplay.btn,"
    "@220:select:qp_quickplay.btn,@320:down,"
    "@350:msg:music_library:select_highlighted_node,"
    "@380:part:guitar,@410:diff:expert,@440:nofail"
)

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

def log(m): print(f"[attach] {m}", flush=True)

def launch(nav, logpath, probe_sel):
    port = free_port()
    logf = open(logpath, "w")
    env = dict(os.environ)
    env.update({"RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
                "MILO_HEADLESS": "1", "RB3_DATA": DATA, "RB3_GAME_INPUT": nav,
                "RB3_FIXED_CLOCK": "1",
                "RB3_HANDS_ATTACH_PROBE": probe_sel})
    proc = subprocess.Popen([BIN], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)
    return proc, port, logf

def teardown(proc, logf):
    try:
        os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
        try: proc.wait(timeout=8)
        except subprocess.TimeoutExpired: os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
    except Exception: pass
    logf.close()

def wait_screen(port, proc, screen, timeout):
    dl = time.time() + timeout
    while time.time() < dl:
        if proc.poll() is not None: return None
        h = health(port)
        if h and h[2] == screen: return h
        time.sleep(0.25)
    return None

def wait_songms(port, proc, target_ms, timeout):
    dl = time.time() + timeout
    while time.time() < dl:
        if proc.poll() is not None: return None
        h = health(port)
        if h and h[2] == "game_screen" and h[1] >= target_ms: return h
        time.sleep(0.15)
    return None

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="/tmp/w28f")
    ap.add_argument("--probe", default="hands_naked,finger,glove,greaserjacket_resource")
    ap.add_argument("--until-ms", type=int, default=70000)
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)
    logp = os.path.join(args.out, "attach.log")
    proc, port, logf = launch(NAV_GAMEPLAY, logp, args.probe)
    try:
        h = wait_screen(port, proc, "game_screen", 150)
        if h is None:
            log("FAIL never reached game_screen"); return 1
        log(f"game_screen frame={h[0]} songMs={h[1]:.0f}")
        for ms in [3000, 10000, 20000, 35000, 50000, args.until_ms]:
            hh = wait_songms(port, proc, ms, 120)
            if hh is None: log(f"songMs>={ms}: FAIL/end"); break
            log(f"songMs>={ms}: frame={hh[0]} songMs={hh[1]:.0f} screen={hh[2]}")
    finally:
        teardown(proc, logf)
    log(f"DONE log={logp}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
