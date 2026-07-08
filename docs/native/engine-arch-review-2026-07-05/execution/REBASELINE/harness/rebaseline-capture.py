#!/usr/bin/env python3
"""Wave 9 C-S1 current-state re-baseline capture.

Boots rb3-native headless (RB3_HTTP=1, RB3_FIXED_CLOCK=1) at HEAD defaults
(engine pin a320f9d — placement contract / black-head fix / hands rest-capture
/ UI text floor / hub-quad-hide / chroma-preserve all default-ON; no lane env
overrides set) and captures:

  - main_hub_screen
  - song_select_screen
  - part_difficulty_screen, settle sequence (7 frames, +0..+360 after arrival,
    NO part:/diff: press — mirrors W4.1's partdiff-settle-recap.py, which
    proved the Wave-6 frame-390 anomaly was a mid part:guitar camera zoom, not
    a bug)
  - gameplay early/mid/late + band-wide, captured via the real part:/diff:
    press verbs (W4.1-era nav), one continuous boot per run (songMs targets
    matching the archived Wave-6 baseline: ~3000/~20000/~45000/~70000)

Run with --run N to distinguish repeat boots for the gameplay N>=3 protocol
(A6): each run captures its own early/mid/late/band set into
<out>/run<N>_*.png so the coordinator can judge at the state level across
boots (director-shot RNG persists under RB3_FIXED_CLOCK).
"""
import argparse, http.client, json, os, signal, socket, subprocess, sys, time

REPO = "/home/free/code/milohax/rb3"
BIN = os.path.join(REPO, "native", "build-native", "rb3-native")
DATA = os.path.join(REPO, "orig-assets", "extracted")

NAV_HUB = "@10:start,@30:confirm"
NAV_SONGSELECT = "@10:start,@30:confirm,@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn"
NAV_PARTDIFF_SETTLE = (
    "@10:start,@30:confirm,@140:select:pn_quickplay.btn,"
    "@220:select:qp_quickplay.btn,@320:down,"
    "@350:msg:music_library:select_highlighted_node"
)
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
    st, data = hget(port, "/api/screenshot", t=20)
    if st != 200 or not data or data[:8] != b"\x89PNG\r\n\x1a\n": return False
    open(path, "wb").write(data); return True

def log(m): print(f"[rebaseline] {m}", flush=True)

def launch(nav, extra_env=None):
    port = free_port()
    logp = f"/tmp/rb3-w9-rebaseline-{port}.log"; logf = open(logp, "w")
    env = dict(os.environ)
    env.update({"RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
                "MILO_HEADLESS": "1", "RB3_DATA": DATA, "RB3_GAME_INPUT": nav,
                "RB3_FIXED_CLOCK": "1"})
    if extra_env: env.update(extra_env)
    proc = subprocess.Popen([BIN], env=env, stdout=logf, stderr=subprocess.STDOUT,
                             cwd=REPO, start_new_session=True)
    return proc, port, logp, logf

def teardown(proc, logf):
    try:
        os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
        try: proc.wait(timeout=8)
        except subprocess.TimeoutExpired: os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
    except Exception: pass
    logf.close()

def wait_screen(port, proc, screen, timeout, label):
    dl = time.time() + timeout
    while time.time() < dl:
        if proc.poll() is not None:
            log(f"FAIL: process exited waiting for {label}"); return None
        h = health(port)
        if h and h[2] == screen: return h
        time.sleep(0.3)
    return None

def wait_songms(port, proc, target_ms, timeout, label):
    dl = time.time() + timeout
    while time.time() < dl:
        if proc.poll() is not None:
            log(f"FAIL: process exited waiting for {label}"); return None
        h = health(port)
        if h and h[2] == "game_screen" and h[1] >= target_ms: return h
        time.sleep(0.2)
    return None


def cap_hub(out):
    proc, port, logp, logf = launch(NAV_HUB)
    try:
        h = wait_screen(port, proc, "main_hub_screen", 60, "main_hub")
        if h is None: return False
        time.sleep(2.0)
        h = health(port)
        ok = shot(port, os.path.join(out, "mainhub_default.png"))
        log(f"main_hub: frame={h[0]} screen={h[2]} shot={'OK' if ok else 'FAIL'}")
        return ok
    finally:
        teardown(proc, logf)

def cap_songselect(out):
    proc, port, logp, logf = launch(NAV_SONGSELECT)
    try:
        h = wait_screen(port, proc, "song_select_screen", 90, "song_select")
        if h is None: return False
        time.sleep(2.0)
        h = health(port)
        ok = shot(port, os.path.join(out, "songselect_default.png"))
        log(f"song_select: frame={h[0]} screen={h[2]} shot={'OK' if ok else 'FAIL'}")
        return ok
    finally:
        teardown(proc, logf)

def cap_partdiff_settle(out):
    proc, port, logp, logf = launch(NAV_PARTDIFF_SETTLE)
    try:
        h = wait_screen(port, proc, "part_difficulty_screen", 120, "part_difficulty")
        if h is None: return False
        arrived = h[0]
        log(f"part_difficulty arrived at frame {arrived}")
        targets = [0, 30, 60, 120, 180, 240, 360]
        last_ok = None
        for tgt in targets:
            dl = time.time() + 30
            while time.time() < dl:
                hh = health(port)
                if hh and hh[0] >= arrived + tgt: break
                time.sleep(0.15)
            hh = health(port)
            path = os.path.join(out, f"partdiff_settle_{tgt:03d}.png")
            ok = shot(port, path)
            log(f"partdiff +{tgt:>3}f: frame={hh[0] if hh else '?'} shot={'OK' if ok else 'FAIL'}")
            if ok: last_ok = path
        # Representative frame for the montage = the last (fully settled) one.
        if last_ok:
            import shutil
            shutil.copyfile(last_ok, os.path.join(out, "partdiff_default_settled.png"))
        return last_ok is not None
    finally:
        teardown(proc, logf)

def cap_gameplay_run(out, run_idx):
    proc, port, logp, logf = launch(NAV_GAMEPLAY)
    try:
        h = wait_screen(port, proc, "game_screen", 150, "game_screen")
        if h is None: return False
        log(f"run{run_idx}: game_screen at frame {h[0]} songMs={h[1]:.0f}")
        targets = [("early", 3000), ("mid", 20000), ("late", 45000), ("band", 70000)]
        ok_all = True
        for name, ms in targets:
            hh = wait_songms(port, proc, ms, 60, f"run{run_idx}-{name}")
            if hh is None:
                log(f"run{run_idx} {name}: FAIL never reached songMs>={ms}")
                ok_all = False
                continue
            path = os.path.join(out, f"gameplay_{name}_run{run_idx}.png")
            ok = shot(port, path)
            log(f"run{run_idx} {name}: frame={hh[0]} songMs={hh[1]:.0f} shot={'OK' if ok else 'FAIL'}")
            ok_all = ok_all and ok
        return ok_all
    finally:
        teardown(proc, logf)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="/tmp/wave9-current-state")
    ap.add_argument("--stage", default="all",
                    choices=["all", "hub", "songselect", "partdiff", "gameplay"])
    ap.add_argument("--run", type=int, default=0, help="gameplay run index (1..N)")
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)

    if not os.path.exists(BIN):
        log(f"FAIL: binary not found: {BIN}"); return 1

    rc = 0
    if args.stage in ("all", "hub"):
        if not cap_hub(args.out): rc = 1
    if args.stage in ("all", "songselect"):
        if not cap_songselect(args.out): rc = 1
    if args.stage in ("all", "partdiff"):
        if not cap_partdiff_settle(args.out): rc = 1
    if args.stage == "gameplay":
        if not cap_gameplay_run(args.out, args.run): rc = 1
    elif args.stage == "all":
        if not cap_gameplay_run(args.out, args.run or 1): rc = 1

    log("DONE" if rc == 0 else "DONE WITH FAILURES")
    return rc


if __name__ == "__main__":
    sys.exit(main())
