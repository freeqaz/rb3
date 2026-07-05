#!/usr/bin/env python3
"""
drawlog-golden.py — W0.3.S3 live draw-log golden capture / regression check.

Boots RB3_HTTP=1 RB3_DRAWLOG=1 rb3-native headless (mirrors
song-select-capture.py's subprocess pattern), waits for a fixed scene, GETs
/api/drawlog (native/src/rb3_http_server.cpp, added in this subtask), and
either:
  --update   writes the committed golden (native/tests/goldens/drawlog/<scene>.json)
  (default)  diffs the live capture against that golden using a Python port of
             native/tests/drawlog_compare.h's CompareDrawLogs() tolerance rules,
             exiting non-zero on any divergence.

    python3 scripts/native/drawlog-golden.py [--scene splash_screen] [--update]
            [--port N] [--data DIR] [--bin PATH] [--verbose]
            [--determinism-check N]   # capture N times, report pairwise diffs
            [--fail-red-audit]        # perturb the golden, confirm non-zero exit, revert

*** DETERMINISM CAVEAT (see docs/native/engine-arch-review-2026-07-05/execution/W0.3/STATUS.md
    "W0.3.S3" section for the full investigation) ***
No live-rendered scene reachable from a fresh headless boot in the current
engine build is exactly frame-reproducible across process launches: splash_screen
draw counts drift ~1-3% run-to-run (877-894 observed) even with
RB3_GAMEWARM_OFF=1 RB3_TEX_PREWARM_OFF=1 RB3_ASYNC_OPEN_OFF=1 all set (which
rules out background venue/texture prewarm and async file-open races as the
cause) and independent of settle window (30 vs 400 vs 700 frames all drift).
Earlier candidates were ruled out for a bigger reason: intro/boot screen has a
transitional frame that itself varies (1042-1048 draws) and main_hub_screen's
`message_rotation_ms` news-ticker is wall-clock-driven (30+ mesh names differ
per capture). splash_screen is the *tightest* jitter band found (~2%) of any
candidate tried. This script and its comparator are implemented per spec
(counts EXACT, scalar fields EXACT, world xfm float-eps, bind-group
sharing-pattern equality) — a real co-location or bind-group-collapse
regression will still fail loudly (proven by --fail-red-audit). Treat routine
`(default)` diff-mode runs as a **diagnostic**, not an unattended CI gate, until
the engine gains a deterministic/frozen clock for headless boots.

Exit codes: 0 = match (or --update / --determinism-check completed), 1 = boot
or navigation failure, 2 = draw-log comparison found divergence(s).
"""
import argparse, http.client, json, math, os, signal, socket, subprocess, sys, time

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DEFAULT_BIN = os.path.join(REPO, "native", "build-native", "rb3-native")
DEFAULT_DATA = os.path.join(REPO, "orig-assets", "extracted")
GOLDEN_DIR = os.path.join(REPO, "native", "tests", "goldens", "drawlog")

SERVER_READY_TIMEOUT = 40
SCENE_TIMEOUT = 60
# Settle window after reaching the scene: probing (see STATUS.md) found longer
# settle does NOT reduce jitter (700-frame settle was worse than 30-frame), so
# a short fixed window is used — it is not a stabilization mechanism, just
# enough for the screen's own entry transition to finish.
SETTLE_FRAMES = 30

# Best-known stabilization: disables background venue/texture prewarm + async
# file-open (rules those out as jitter sources; residual jitter persists
# regardless — see module docstring).
STABILIZE_ENV = {
    "RB3_GAMEWARM_OFF": "1",
    "RB3_TEX_PREWARM_OFF": "1",
    "RB3_ASYNC_OPEN_OFF": "1",
}


def log(m): print(f"[drawlog-golden] {m}", flush=True)


def free_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM); s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]; s.close(); return p


def http_get_bytes(port, path, timeout=10):
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
        return int(d["frame"]), str(d["currentScreen"])
    except Exception:
        return None


def wait_for(port, pred, timeout, label, verbose, proc):
    dl = time.time() + timeout; last = None
    while time.time() < dl:
        if proc.poll() is not None:
            log(f"FAIL: process exited (code {proc.returncode}) waiting for {label}"); return None
        h = health(port)
        if h is not None:
            f, s = h
            if verbose and (f, s) != last:
                log(f"  ...{label}: frame={f} screen='{s}'"); last = (f, s)
            if pred(f, s): return h
        time.sleep(0.05)
    return None


def get_drawlog(port, timeout=15):
    st, b = http_get_bytes(port, "/api/drawlog", timeout=timeout)
    if st != 200:
        return None
    return json.loads(b.decode("utf-8", "replace"))


def capture_once(args, tag=""):
    """Boot rb3-native, wait for args.scene, settle, GET /api/drawlog. Returns
    the parsed JSON dict, or None on boot/nav failure."""
    port = args.port or free_port()
    log_path = os.path.join("/tmp", f"rb3-drawlog-{port}.log")
    logf = open(log_path, "w")
    env = dict(os.environ)
    env.update({"RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
                "MILO_HEADLESS": "1", "RB3_DATA": args.data, "RB3_DRAWLOG": "1"})
    if not args.no_stabilize:
        env.update(STABILIZE_ENV)
    label = f"capture{tag}"
    log(f"[{label}] launching rb3-native (port {port}), log -> {log_path}")
    proc = subprocess.Popen([args.bin], env=env, stdout=logf, stderr=subprocess.STDOUT,
                             cwd=REPO, start_new_session=True)
    try:
        if wait_for(port, lambda f, s: True, SERVER_READY_TIMEOUT, "server", args.verbose, proc) is None:
            log(f"[{label}] FAIL: HTTP server never came up"); return None
        h = wait_for(port, lambda f, s: s == args.scene, SCENE_TIMEOUT, args.scene, args.verbose, proc)
        if h is None:
            log(f"[{label}] FAIL: never reached scene '{args.scene}'"); return None
        reach_frame = h[0]
        log(f"[{label}] reached '{args.scene}' at frame {reach_frame}, settling {SETTLE_FRAMES} frames...")
        target = reach_frame + SETTLE_FRAMES
        wait_for(port, lambda f, s: f >= target, 15, "settle", args.verbose, proc)
        d = get_drawlog(port)
        if d is None:
            log(f"[{label}] FAIL: /api/drawlog request failed"); return None
        log(f"[{label}] captured frame={d.get('frame')} count={d.get('count')}")
        return d
    finally:
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
            try: proc.wait(timeout=8)
            except subprocess.TimeoutExpired: os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
        except Exception: pass
        logf.close()
        if args.keep_log: log(f"[{label}] engine log: {log_path}")
        elif os.path.exists(log_path):
            try: os.remove(log_path)
            except OSError: pass


# ---------------------------------------------------------------------------
# Python port of native/tests/drawlog_compare.h's CompareDrawLogs(). Field
# names/order/semantics mirror the C++ exactly (see that file for the
# authoritative spec / comments); kept in lockstep by hand since this script
# has no C++ build step of its own.
# ---------------------------------------------------------------------------
ROT_EPS = 1e-4
TRANS_EPS = 1e-2
REL_EPS = 1e-4
W_EPS = 1e-6

SCALAR_FIELDS = [
    ("pipe", "pipe"), ("blend", "blend"), ("zmode", "zmode"), ("layout", "layout"),
    ("fmt", "fmt"), ("hasDepth", "hasDepth"), ("alphaCut", "alphaCut"),
    ("alphaWrite", "alphaWrite"), ("skinned", "skinned"), ("idx", "idx"),
    ("tris", "tris"), ("verts", "verts"), ("name", "name"),
]
SHARE_STREAMS = ["scene", "mat", "obj", "bone"]


def world_elem_ok(e, g, c):
    d = abs(c - g)
    if e == 15:
        return d <= W_EPS
    absEps = TRANS_EPS if 12 <= e <= 14 else ROT_EPS
    return d <= max(absEps, REL_EPS * abs(g))


def compare_drawlogs(golden, candidate):
    """Returns (passed: bool, failures: list[str])."""
    failures = []
    gd = golden.get("draws", [])
    cd = candidate.get("draws", [])

    if len(gd) != len(cd):
        failures.append(f"count: golden={len(gd)} candidate={len(cd)}")
        return False, failures  # misaligned; no point comparing further (mirrors C++)

    for i, (g, c) in enumerate(zip(gd, cd)):
        for gk, ck in SCALAR_FIELDS:
            gv, cv = g.get(gk), c.get(ck)
            if gv != cv:
                failures.append(f"draw {i} field={gk} golden={gv} cand={cv}")
        gw, cw = g.get("world", [0.0]*16), c.get("world", [0.0]*16)
        for e in range(16):
            if not world_elem_ok(e, gw[e], cw[e]):
                failures.append(f"draw {i} field=world world[{e}] golden={gw[e]:.7g} cand={cw[e]:.7g}")

    n = len(gd)
    for stream in SHARE_STREAMS:
        for i in range(n):
            for j in range(i + 1, n):
                g_share = gd[i].get(stream) == gd[j].get(stream)
                c_share = cd[i].get(stream) == cd[j].get(stream)
                if g_share != c_share:
                    failures.append(
                        f"draw {i}/{j} field={stream} golden={'shared' if g_share else 'distinct'} "
                        f"cand={'shared' if c_share else 'distinct'}")

    return (len(failures) == 0), failures


def golden_path(scene):
    return os.path.join(GOLDEN_DIR, f"{scene}.json")


def diff_names(a, b):
    """Informational helper for --determinism-check: mesh-name-hash multiset diff."""
    from collections import Counter
    ca = Counter(d["name"] for d in a.get("draws", []))
    cb = Counter(d["name"] for d in b.get("draws", []))
    return ca - cb, cb - ca


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=0)
    ap.add_argument("--bin", default=DEFAULT_BIN)
    ap.add_argument("--data", default=DEFAULT_DATA)
    ap.add_argument("--scene", default="splash_screen")
    ap.add_argument("--update", action="store_true", help="write the committed golden from a live capture")
    ap.add_argument("--determinism-check", type=int, default=0, metavar="N",
                     help="capture N times, report pairwise count/name-set drift (informational)")
    ap.add_argument("--fail-red-audit", action="store_true",
                     help="perturb the committed golden's draw[0] translation, confirm non-zero exit, then revert")
    ap.add_argument("--no-stabilize", action="store_true",
                     help="skip RB3_GAMEWARM_OFF/RB3_TEX_PREWARM_OFF/RB3_ASYNC_OPEN_OFF")
    ap.add_argument("--verbose", action="store_true")
    ap.add_argument("--keep-log", action="store_true")
    args = ap.parse_args()

    if not os.path.exists(args.bin):
        log(f"FAIL: binary not found: {args.bin}")
        log("      build it: cmake --build native/build-native --target rb3-native")
        return 1

    if args.determinism_check:
        caps = []
        for i in range(args.determinism_check):
            d = capture_once(args, tag=f"[{i}]")
            if d is None:
                return 1
            caps.append(d)
        counts = [d["count"] for d in caps]
        log(f"determinism-check: counts={counts} (min={min(counts)} max={max(counts)} spread={max(counts)-min(counts)})")
        for i in range(1, len(caps)):
            onlyA, onlyB = diff_names(caps[0], caps[i])
            log(f"  capture[0] vs capture[{i}]: {len(onlyA)} names only-in-0, {len(onlyB)} names only-in-{i}")
        return 0

    if args.fail_red_audit:
        gp = golden_path(args.scene)
        if not os.path.exists(gp):
            log(f"FAIL: no golden at {gp} to audit; run --update first"); return 1
        with open(gp) as f: original = f.read()
        golden = json.loads(original)
        perturbed = json.loads(original)
        if not perturbed.get("draws"):
            log("FAIL: golden has no draws to perturb"); return 1
        # Translation X, draw 0. Must clear max(transEps, relEps*|g|) — for a
        # scene-scale golden value (thousands of world units) relEps*|g| can
        # exceed transEps, so use a large fixed offset rather than a multiple
        # of transEps alone.
        perturbed["draws"][0]["world"][12] += 100.0
        passed, failures = compare_drawlogs(golden, perturbed)
        if passed:
            log("FAIL-RED AUDIT FAILED: perturbed golden compared as PASS (comparator is not catching drift)")
            return 2
        log(f"FAIL-RED AUDIT OK: perturbed golden correctly compared as FAIL ({len(failures)} divergence(s)):")
        for x in failures[:5]:
            log(f"  {x}")
        return 0

    if args.update:
        d = capture_once(args)
        if d is None:
            return 1
        os.makedirs(GOLDEN_DIR, exist_ok=True)
        gp = golden_path(args.scene)
        with open(gp, "w") as f:
            json.dump(d, f, indent=2)
            f.write("\n")
        log(f"PASS: wrote golden {gp} ({d.get('count')} draws)")
        return 0

    # Default: diff live capture against committed golden.
    gp = golden_path(args.scene)
    if not os.path.exists(gp):
        log(f"FAIL: no golden at {gp}; run with --update first"); return 1
    with open(gp) as f:
        golden = json.load(f)
    candidate = capture_once(args)
    if candidate is None:
        return 1
    passed, failures = compare_drawlogs(golden, candidate)
    if passed:
        log(f"PASS: live capture matches golden ({candidate.get('count')} draws)")
        return 0
    log(f"FAIL: {len(failures)} divergence(s) vs golden:")
    for x in failures[:30]:
        log(f"  {x}")
    if len(failures) > 30:
        log(f"  ... and {len(failures) - 30} more")
    return 2


if __name__ == "__main__":
    sys.exit(main())
