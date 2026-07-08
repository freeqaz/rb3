#!/usr/bin/env python3
"""
hud-k9-ab-capture.py — Wave-22 HUD lane STEP 0 (A4) discriminator.

Boots rb3-native headless to GAMEPLAY N times with different env sets (the
K9 A/B: default ON vs RB3_APPLY_HANDLER_FIX_OFF=1), captures a gameplay PNG
per run, and (optionally, with --drawlog) dumps the /api/drawlog provenance
so the scoreboard bbox can be located.

Reuses the boot-to-song nav from gameplay-depth-capture.py.

Usage:
  hud-k9-ab-capture.py --label default            # K9 ON (baseline)
  hud-k9-ab-capture.py --label k9off --env RB3_APPLY_HANDLER_FIX_OFF=1
"""
import argparse, http.client, json, os, socket, subprocess, sys, time

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DEFAULT_BIN = os.path.join(REPO, "native", "build-native", "rb3-native")
DEFAULT_DATA = os.path.join(REPO, "orig-assets", "extracted")

NAV_SCRIPT = (
    "@10:start,@30:confirm,"
    "@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn,"
    "@320:down,@350:msg:music_library:select_highlighted_node,"
    "@380:part:guitar,@400:diff:expert,"
    "@500:nofail,@520:autohit"
)

def log(m): print(f"[hud-k9-ab] {m}", flush=True)
def free_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM); s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]; s.close(); return p
def http_get_bytes(port, path, timeout=25):
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
    st, data = http_get_bytes(port, "/api/screenshot", timeout=30)
    if st != 200 or not data: return False
    with open(path, "wb") as f: f.write(data)
    return True

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=0)
    ap.add_argument("--bin", default=DEFAULT_BIN)
    ap.add_argument("--data", default=DEFAULT_DATA)
    ap.add_argument("--out", default="/tmp/rb3-hud-k9")
    ap.add_argument("--label", required=True)
    ap.add_argument("--env", action="append", default=[], help="extra KEY=VAL env, repeatable")
    ap.add_argument("--at", type=float, default=6000.0, help="songMs to capture at")
    ap.add_argument("--drawlog", action="store_true", help="also dump /api/drawlog?roi provenance")
    ap.add_argument("--keep-log", action="store_true")
    args = ap.parse_args()

    os.makedirs(args.out, exist_ok=True)
    port = args.port or free_port()
    log_path = os.path.join(args.out, f"boot-{args.label}.log")
    logf = open(log_path, "w")
    env = dict(os.environ)
    env.update({
        "RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
        "MILO_HEADLESS": "1", "RB3_DATA": args.data, "RB3_GAME_INPUT": NAV_SCRIPT,
        "RB3_FIXED_CLOCK": "1",
    })
    for kv in args.env:
        k, _, v = kv.partition("="); env[k] = v
    if args.drawlog:
        env["RB3_DRAWLOG"] = "1"; env["RB3_DRAWLOG_PROV"] = "1"
    log(f"launching label={args.label} port={port} extra_env={args.env} log={log_path}")
    proc = subprocess.Popen([args.bin], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)
    rc = 1
    try:
        if wait_for(port, lambda f, m, s: True, 45, "server-ready", proc) is None:
            log("FAIL: HTTP never came up"); return 1
        h = wait_for(port, lambda f, m, s: m > 2000.0, 260, "gameplay-start", proc)
        if h is None:
            log("FAIL: never reached gameplay"); return 1
        log(f"gameplay underway: frame={h[0]} songMs={h[1]:.0f} screen='{h[2]}'")
        h = wait_for(port, lambda f, m, s, _t=args.at: m >= _t, 90, f"songMs>={args.at:.0f}", proc)
        if h is None:
            log(f"WARN: never reached songMs {args.at}, capturing anyway")
        path = os.path.join(args.out, f"gameplay_{args.label}.png")
        ok = screenshot(port, path)
        log(f"screenshot={'OK' if ok else 'FAIL'} -> {path}")
        if args.drawlog:
            try:
                st, b = http_get_bytes(port, "/api/drawlog", timeout=15)
                dl_path = os.path.join(args.out, f"drawlog_{args.label}.json")
                with open(dl_path, "wb") as f: f.write(b)
                log(f"drawlog -> {dl_path} ({st}, {len(b)} bytes)")
            except Exception as e:
                log(f"drawlog dump failed: {e}")
        rc = 0 if ok else 1
    finally:
        try: proc.send_signal(2); time.sleep(0.5); proc.kill()
        except Exception: pass
        logf.close()
        if not args.keep_log:
            log(f"engine log kept at {log_path}")
    return rc

if __name__ == "__main__":
    sys.exit(main())
