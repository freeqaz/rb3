#!/usr/bin/env python3
"""W2.8e A.S3 floating-forearm triage harness.

Reproduces the two REBASELINE §(b) sightings with the shard-family probes ON
(SHARD_DBG drop-log, SHARD_RATIO_DBG census, SHARD_BONE_DBG outlier-bone) so a
single capture separates H1 (detached-mesh fling) / H2 (occlusion-wipe illusion)
/ H3 (sibling meshes guard-DROPped, one legit forearm survives).

Engine-read-only: only existing env-gated probes used. Build dir = build-agent-W2.8e
(flag-OFF default; RB3_APPENDAGE_ASSET_REBAKE unset -> the REFUTED S2 fix is inert).
"""
import argparse, http.client, json, os, signal, socket, subprocess, sys, time

REPO = "/home/free/code/milohax/rb3"
BIN = os.path.join(REPO, "native", "build-agent-W2.8e", "rb3-native")
DATA = os.path.join(REPO, "orig-assets", "extracted")

NAV_PARTDIFF = (
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
PROBE_ENV = {"SHARD_DBG": "1", "SHARD_RATIO_DBG": "1", "SHARD_BONE_DBG": "1"}


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
    st, data = hget(port, "/api/screenshot", t=20)
    if st != 200 or not data or data[:8] != b"\x89PNG\r\n\x1a\n": return False
    open(path, "wb").write(data); return True

def log(m): print(f"[triage] {m}", flush=True)

def launch(nav, logpath):
    port = free_port()
    logf = open(logpath, "w")
    env = dict(os.environ)
    env.update({"RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
                "MILO_HEADLESS": "1", "RB3_DATA": DATA, "RB3_GAME_INPUT": nav,
                "RB3_FIXED_CLOCK": "1"})
    env.update(PROBE_ENV)
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

def wait_frame(port, proc, target_frame, timeout):
    dl = time.time() + timeout
    while time.time() < dl:
        if proc.poll() is not None: return None
        h = health(port)
        if h and h[0] >= target_frame: return h
        time.sleep(0.1)
    return None

def wait_songms(port, proc, target_ms, timeout):
    dl = time.time() + timeout
    while time.time() < dl:
        if proc.poll() is not None: return None
        h = health(port)
        if h and h[2] == "game_screen" and h[1] >= target_ms: return h
        time.sleep(0.15)
    return None


def cap_partdiff(out):
    logp = os.path.join(out, "partdiff.log")
    proc, port, logf = launch(NAV_PARTDIFF, logp)
    frames = {}
    try:
        h = wait_screen(port, proc, "part_difficulty_screen", 120)
        if h is None: log("partdiff: FAIL never reached screen"); return frames
        arrived = h[0]
        log(f"partdiff arrived frame={arrived}")
        for tgt in [0, 30, 60, 90, 120, 180, 360]:
            hh = wait_frame(port, proc, arrived + tgt, 30)
            if hh is None: continue
            path = os.path.join(out, f"partdiff_{tgt:03d}.png")
            ok = shot(port, path)
            frames[tgt] = hh[0]
            log(f"partdiff +{tgt:>3}f: frame={hh[0]} screen={hh[2]} shot={'OK' if ok else 'FAIL'}")
    finally:
        teardown(proc, logf)
    json.dump(frames, open(os.path.join(out, "partdiff_frames.json"), "w"))
    return frames


def cap_gameplay(out, run_idx):
    logp = os.path.join(out, f"gameplay_run{run_idx}.log")
    proc, port, logf = launch(NAV_GAMEPLAY, logp)
    frames = {}
    try:
        h = wait_screen(port, proc, "game_screen", 150)
        if h is None: log(f"run{run_idx}: FAIL never reached game_screen"); return frames
        log(f"run{run_idx}: game_screen frame={h[0]} songMs={h[1]:.0f}")
        for name, ms in [("early", 3000), ("mid", 20000), ("late", 45000), ("band", 70000)]:
            hh = wait_songms(port, proc, ms, 90)
            if hh is None: log(f"run{run_idx} {name}: FAIL songMs>={ms}"); continue
            path = os.path.join(out, f"gameplay_{name}_run{run_idx}.png")
            ok = shot(port, path)
            frames[name] = {"frame": hh[0], "songMs": hh[1]}
            log(f"run{run_idx} {name}: frame={hh[0]} songMs={hh[1]:.0f} shot={'OK' if ok else 'FAIL'}")
    finally:
        teardown(proc, logf)
    json.dump(frames, open(os.path.join(out, f"gameplay_run{run_idx}_frames.json"), "w"))
    return frames


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="/tmp/w10-a-s3")
    ap.add_argument("--stage", default="partdiff", choices=["partdiff", "gameplay"])
    ap.add_argument("--run", type=int, default=1)
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)
    if args.stage == "partdiff":
        cap_partdiff(args.out)
    else:
        cap_gameplay(args.out, args.run)
    log("DONE")

if __name__ == "__main__":
    sys.exit(main())
