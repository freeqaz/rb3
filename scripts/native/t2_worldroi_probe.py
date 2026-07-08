#!/usr/bin/env python3
"""
t2_worldroi_probe.py — T2-WORLDROI (Wave 19) instrument harness.

Boots rb3-native headless to a band gameplay frame with RB3_DRAWLOG_PROV=1, then
dumps /api/drawlog?prov=1 and reports the skinned-pose provenance coverage:
  - total draws, skinned draws, rectKind distribution among skinned draws
  - N rectKind:3 (skinned-pose bbox) vs M rectKind:1 (disclosed sphere fallback)
  - boneFallback histogram (bones rendering at BIND under the placement contract)
  - a few example rectKind:3 rows (mesh / owner / bones / rect)

SPATIAL axis only (which mesh/bone/owner drew a pixel) — NOT the T1 frame-assignment
timing axis nor R4's ledger `order` axis.

Modes:
  (default)          coverage report on a gameplay frame; --json PATH dumps the raw drawlog.
  --roi X,Y,W,H      also print the draws whose prov.rect intersects the ROI, naming
                     mesh / owner / boneRects-in-ROI / mat.
  --shot PATH        save a screenshot of the analyzed frame.

Env knobs it forwards via --extra-env "K=V K=V":
  RB3_PROV_SKIN_SPHERE=1  forces the legacy sphere for skinned draws (G1 RED baseline).
"""
import argparse, http.client, json, os, signal, socket, subprocess, sys, time

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DEFAULT_BIN = os.path.join(REPO, "native", "build-native", "rb3-native")
DEFAULT_DATA = os.path.join(REPO, "orig-assets", "extracted")

START, CONFIRM, CANCEL, STAR = 11, 6, 5, 8
DUP, DRIGHT, DDOWN, DLEFT = 12, 13, 14, 15


def log(m): print(f"[t2] {m}", flush=True)


def free_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM); s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]; s.close(); return p


def http_get(port, path, timeout=25):
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
    try:
        st, b = http_get(port, "/api/health")
        if st != 200: return None
        d = json.loads(b.decode("utf-8", "replace"))["data"]
        return int(d["frame"]), float(d["songMs"]), str(d["currentScreen"])
    except Exception: return None


def dta(port, expr):
    try:
        st, b = http_post(port, "/api/dta/eval", expr, timeout=10)
        if st != 200: return None
        d = json.loads(b)
        if not d.get("ok"): return None
        return d["data"].get("value")
    except Exception: return None


def press(port, bit): http_post(port, "/api/input", f"pad:{bit}")
def verb(port, v): return http_post(port, "/api/input", v)


def screenshot(port, path):
    st, data = http_get(port, "/api/screenshot", timeout=25)
    if st != 200 or not data or data[:8] != b"\x89PNG\r\n\x1a\n": return False
    with open(path, "wb") as f: f.write(data)
    return True


def wait_screen(port, want, timeout, proc):
    dl = time.time() + timeout
    while time.time() < dl:
        if proc.poll() is not None: return None
        h = health(port)
        if h and ((callable(want) and want(h[2])) or h[2] == want): return h
        time.sleep(0.3)
    return None


def overshell(port):
    v = dta(port, "{rb3_overshell}")
    if not v: return ("?", "?", "?")
    parts = dict(p.split(":", 1) for p in v.split("|") if ":" in p)
    return (parts.get("view", "?"), parts.get("track", "?"), parts.get("diff", "?"))


def nav_to_gameplay(port, proc):
    if wait_screen(port, lambda s: True, 40, proc) is None:
        log("FAIL: server never came up"); return False
    for _ in range(8):
        if health(port)[2] == "main_hub_screen": break
        press(port, START); time.sleep(0.7)
    for _ in range(10):
        if health(port)[2] == "song_select_screen": break
        press(port, CONFIRM); time.sleep(0.7)
    if wait_screen(port, "song_select_screen", 40, proc) is None:
        log("FAIL: no song_select"); return False
    time.sleep(1.5)
    for _ in range(4): press(port, DDOWN); time.sleep(0.2)
    press(port, CONFIRM)
    if wait_screen(port, "part_difficulty_screen", 60, proc) is None:
        log("FAIL: no part_difficulty"); return False
    time.sleep(1.0); press(port, CONFIRM); time.sleep(1.0)
    ov = overshell(port); guard = 0
    while ov[0] == "confirm_action" and guard < 4:
        press(port, CONFIRM); time.sleep(0.5); ov = overshell(port); guard += 1
    for _ in range(2): press(port, DDOWN); time.sleep(0.25)
    press(port, CONFIRM); time.sleep(0.5)
    ov = overshell(port); guard = 0
    while ov[0] == "confirm_action" and guard < 4:
        press(port, CONFIRM); time.sleep(0.5); ov = overshell(port); guard += 1
    h = wait_screen(port, "game_screen", 90, proc)
    if h is None: log("FAIL: no game_screen"); return False
    log(f"game_screen reached frame={h[0]} songMs={h[1]:.0f}")
    verb(port, "nofail")
    return True


def fetch_drawlog(port, roi=None):
    path = "/api/drawlog?prov=1" if not roi else f"/api/drawlog?roi={roi}"
    st, b = http_get(port, path)
    if st != 200: raise RuntimeError(f"{path} status {st}")
    return json.loads(b.decode("utf-8", "replace"))


def rect_isect(r, roi):
    return (r[0] < roi[0] + roi[2] and r[0] + r[2] > roi[0] and
            r[1] < roi[1] + roi[3] and r[1] + r[3] > roi[1])


def report_coverage(dl):
    draws = dl.get("draws", [])
    total = len(draws)
    skinned = [d for d in draws if d.get("skinned")]
    kinds = {0: 0, 1: 0, 2: 0, 3: 0}
    fb = {}
    for d in skinned:
        p = d.get("prov", {})
        k = int(p.get("rectKind", 2)); kinds[k] = kinds.get(k, 0) + 1
        n = int(p.get("boneFallback", 0)); fb[n] = fb.get(n, 0) + 1
    log(f"frame={dl.get('frame')} total_draws={total} skinned={len(skinned)}")
    log(f"  skinned rectKind: 3(pose-bbox)={kinds.get(3,0)} 1(sphere)={kinds.get(1,0)} "
        f"2(unavail)={kinds.get(2,0)} 0(verts)={kinds.get(0,0)}")
    log(f"  boneFallback histogram (N bones@bind -> #draws): "
        + ", ".join(f"{n}:{c}" for n, c in sorted(fb.items())))
    examples = [d for d in skinned if int(d.get("prov", {}).get("rectKind", 2)) == 3]
    for d in examples[:6]:
        p = d["prov"]
        bones = [br["bone"] for br in p.get("boneRects", [])][:8]
        r = p.get("rect", [])
        log(f"  ex rectKind3: mesh='{p.get('mesh')}' owner='{p.get('owner')}' "
            f"rect=[{r[0]:.0f},{r[1]:.0f},{r[2]:.0f},{r[3]:.0f}] "
            f"boneFallback={p.get('boneFallback')} bones={bones}")
    return {"total": total, "skinned": len(skinned), "kinds": kinds, "fb": fb,
            "rectKind3": kinds.get(3, 0), "rectKind1": kinds.get(1, 0)}


def report_roi(dl, roi):
    draws = dl.get("draws", [])
    hits = []
    for d in draws:
        p = d.get("prov")
        if not p: continue
        r = p.get("rect")
        if not r or int(p.get("rectKind", 2)) == 2 or r[2] < 0: continue
        if not rect_isect(r, roi): continue
        bones_in = []
        for br in p.get("boneRects", []):
            if rect_isect(br["rect"], roi): bones_in.append(br["bone"])
        hits.append({"mesh": p.get("mesh"), "owner": p.get("owner"),
                     "mat": p.get("mat"), "rectKind": p.get("rectKind"),
                     "skinned": d.get("skinned"), "bonesInRoi": bones_in,
                     "boneFallback": p.get("boneFallback"), "rect": r})
    log(f"ROI {roi}: {len(hits)} draws intersect")
    for h in hits:
        log(f"  mesh='{h['mesh']}' owner='{h['owner']}' mat='{h['mat']}' "
            f"kind={h['rectKind']} skinned={h['skinned']} "
            f"boneFallback={h['boneFallback']} bones={h['bonesInRoi']}")
    return hits


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=0)
    ap.add_argument("--bin", default=DEFAULT_BIN)
    ap.add_argument("--data", default=DEFAULT_DATA)
    ap.add_argument("--overlay", default=os.path.join(REPO, "native", "dta"))
    ap.add_argument("--roi", default="", help="X,Y,W,H pixel ROI")
    ap.add_argument("--shot", default="", help="save screenshot to this path")
    ap.add_argument("--json", default="", help="dump raw drawlog json to this path")
    ap.add_argument("--settle", type=float, default=2.0, help="seconds of autohit before capture")
    ap.add_argument("--extra-env", default="")
    args = ap.parse_args()

    port = args.port or free_port()
    log_path = os.path.join("/tmp", f"rb3-t2-{port}.log")
    logf = open(log_path, "w")
    env = dict(os.environ)
    env.update({"RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
                "MILO_HEADLESS": "1", "RB3_DATA": args.data,
                "RB3_DTA_OVERLAY": args.overlay,
                "RB3_DRAWLOG_PROV": "1", "RB3_FIXED_CLOCK": "1"})
    for kv in args.extra_env.split():
        if "=" in kv:
            k, v = kv.split("=", 1); env[k] = v
    log(f"launching rb3-native (port {port}); log -> {log_path}; "
        f"skin_sphere={env.get('RB3_PROV_SKIN_SPHERE','0')}")
    proc = subprocess.Popen([args.bin], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)
    rc = 1
    try:
        if not nav_to_gameplay(port, proc):
            return 1
        t = time.time() + args.settle
        while time.time() < t:
            verb(port, "autohit"); time.sleep(0.25)
        if args.shot and screenshot(port, args.shot):
            log(f"screenshot -> {args.shot}")
        roi = None
        if args.roi:
            roi = [float(x) for x in args.roi.split(",")]
        dl = fetch_drawlog(port, roi=args.roi if roi else None)
        if args.json:
            with open(args.json, "w") as f: json.dump(dl, f)
            log(f"raw drawlog -> {args.json}")
        if roi:
            report_roi(dl, roi)
        else:
            report_coverage(dl)
        rc = 0
        return rc
    finally:
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
            try: proc.wait(timeout=8)
            except subprocess.TimeoutExpired: os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
        except Exception: pass
        logf.close()


if __name__ == "__main__":
    sys.exit(main())
