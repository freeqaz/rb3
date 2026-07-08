#!/usr/bin/env python3
"""
_grade_hub_discriminator.py — Wave-23 GRADE lane discriminator (S2 hub wash).

Boots rb3-native to main_hub (the boot landing screen), settles under a fixed
clock to a deterministic frame, captures /api/screenshot, and preserves the
stderr log (for RB3_WASH_PROBE / RB3_VENUE_PROBE digests). One boot per arm.

    python3 scripts/native/_grade_hub_discriminator.py --label ARM \
            --env RB3_VENUE_LIGHT_OFF=1 --out DIR [--settle 260]

Writes <out>/<label>.png and <out>/<label>.log ; prints the last WASHPROBE/
VENUE_PROBE lines to stdout. Exit 0 on success.
"""
import argparse, http.client, json, os, signal, socket, subprocess, sys, time

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DEFAULT_BIN = os.path.join(REPO, "native", "build-native", "rb3-native")
DEFAULT_DATA = os.path.join(REPO, "orig-assets", "extracted")
SERVER_READY_TIMEOUT = 60


def log(m): print(f"[grade-disc] {m}", flush=True)

def free_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM); s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]; s.close(); return p

def http_get_bytes(port, path, timeout=20):
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=timeout)
    try:
        c.request("GET", path); r = c.getresponse(); return r.status, r.read()
    finally:
        c.close()

def health(port):
    try:
        st, b = http_get_bytes(port, "/api/health")
        if st != 200: return None
        d = json.loads(b.decode("utf-8", "replace"))["data"]
        return int(d["frame"]), float(d.get("songMs", 0)), str(d["currentScreen"])
    except Exception:
        return None

def screenshot(port, path):
    st, data = http_get_bytes(port, "/api/screenshot", timeout=25)
    if st != 200 or not data or data[:8] != b"\x89PNG\r\n\x1a\n":
        return False
    with open(path, "wb") as f: f.write(data)
    return True


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--label", required=True)
    ap.add_argument("--env", action="append", default=[], help="KEY=VAL, repeatable")
    ap.add_argument("--out", default="/tmp/wave23-grade")
    ap.add_argument("--bin", default=DEFAULT_BIN)
    ap.add_argument("--data", default=DEFAULT_DATA)
    ap.add_argument("--settle", type=int, default=280, help="target frame to capture at")
    args = ap.parse_args()

    os.makedirs(args.out, exist_ok=True)
    port = free_port()
    log_path = os.path.join(args.out, f"{args.label}.log")
    png_path = os.path.join(args.out, f"{args.label}.png")
    logf = open(log_path, "w")

    env = dict(os.environ)
    env.update({"RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
                "MILO_HEADLESS": "1", "RB3_DATA": args.data, "RB3_FIXED_CLOCK": "1",
                "RB3_GAME_INPUT": "@10:start,@30:confirm"})
    for kv in args.env:
        if "=" in kv:
            k, v = kv.split("=", 1); env[k] = v
    log(f"arm={args.label} env={args.env} port={port}")
    proc = subprocess.Popen([args.bin], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)
    pgid = os.getpgid(proc.pid)
    rc = 1
    try:
        # wait for server
        dl = time.time() + SERVER_READY_TIMEOUT
        ready = False
        while time.time() < dl:
            if proc.poll() is not None:
                log(f"FAIL: process exited (code {proc.returncode}) before server up"); break
            if health(port) is not None: ready = True; break
            time.sleep(0.4)
        if not ready:
            log("FAIL: server never came up"); raise SystemExit
        # nav start,confirm -> main_hub_screen; settle N frames on hub, then capture
        dl = time.time() + 120; last = None; hub_frames = 0; hub_start = None
        while time.time() < dl:
            if proc.poll() is not None:
                log(f"FAIL: process exited (code {proc.returncode}) during settle"); break
            h = health(port)
            if h:
                f, m, s = h
                if (f // 40, s) != last:
                    log(f"  frame={f} screen='{s}'"); last = (f // 40, s)
                if s == "main_hub_screen":
                    if hub_start is None: hub_start = f
                    # settle >= args.settle frames on the hub for anim to stabilize
                    if f - hub_start >= args.settle: break
            time.sleep(0.3)
        h = health(port)
        if h:
            log(f"capturing at frame={h[0]} screen='{h[2]}'")
        if screenshot(port, png_path):
            log(f"OK screenshot -> {png_path}"); rc = 0
        else:
            log("FAIL: screenshot")
    finally:
        try:
            os.killpg(pgid, signal.SIGTERM); time.sleep(1.0)
            os.killpg(pgid, signal.SIGKILL)
        except Exception:
            pass
        logf.close()
    # surface probe lines
    try:
        with open(log_path) as f:
            lines = f.readlines()
        probe = [l.rstrip() for l in lines if "WASHPROBE" in l or "VENUE_PROBE" in l
                 or "CHAR_REAL" in l or "LIGHT_PROBE" in l]
        if probe:
            log("--- probe tail ---")
            for l in probe[-20:]: print(l, flush=True)
    except Exception:
        pass
    return rc


if __name__ == "__main__":
    sys.exit(main())
