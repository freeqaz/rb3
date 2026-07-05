#!/usr/bin/env python3
"""
drawlog-golden.py — W0.3.S3 live draw-log golden capture / regression check.

Two capture modes:

  --fixed-clock (NEW, W0.3b.S3 — the reliable unattended gate)
      Bounded, non-HTTP boot: MILO_MAX_FRAMES=N RB3_FIXED_CLOCK=1 RB3_DRAWLOG=1
      RB3_DRAWLOG_DUMP=<tmp>, wrapped in `setarch -R` (ASLR off — required, see
      below), then reads <tmp> once the process has produced it. This pins BOTH
      the sim clock (W0.3b.S1/S2's RB3FixedClockActive()/RB3TaskReplayFixedClock()
      seam: kTaskSeconds advances by a constant per-frame dt keyed off an
      always-advancing frame index) AND the resident/loaded set (S2's
      Loader.cpp drain-to-empty-per-Poll under the flag) at a fixed absolute
      frame — replacing the old HTTP wait-for-scene+settle capture, whose
      capture *frame index* was itself wall-clock/HTTP-timing-dependent and
      therefore non-deterministic. This mode IS a reliable unattended gate
      (see the RESIDUAL note below for the one bounded, tracked exception).

  (legacy, no --fixed-clock — kept as a diagnostic tool, NOT a gate)
      Boots RB3_HTTP=1 RB3_DRAWLOG=1 rb3-native headless, waits for a fixed
      scene via /api/health polling, settles, GETs /api/drawlog. The capture
      frame index here depends on wall-clock/HTTP-round-trip timing, so it is
      NOT frame-reproducible (see the DETERMINISM CAVEAT below) — this is
      exactly the W0.3 exit-#6 blocker that --fixed-clock closes.

Either mode supports:
  --update              writes the committed golden (native/tests/goldens/drawlog/<scene>.json)
  (default)              diffs the live capture against that golden, exiting non-zero on divergence
  --determinism-check N  capture N times, report pairwise diffs (informational)
  --fail-red-audit       perturb the golden, confirm non-zero exit, revert (golden untouched on disk)

    python3 scripts/native/drawlog-golden.py --fixed-clock [--scene splash_screen] [--update]
            [--frames N] [--data DIR] [--bin PATH] [--verbose]
            [--determinism-check N] [--fail-red-audit] [--no-aslr-off]

*** RESIDUAL (fixed-clock mode; see docs/native/engine-arch-review-2026-07-05/
    execution/W0.3b/STATUS.md "W0.3b.S2"/"W0.3b.S3" for the full investigation) ***
Under --fixed-clock, draw **count** and **every scalar/bind-group-sharing
field** are exactly reproducible across independent boots (proven across many
boot-pairs — this is the W0.3 exit-#6 blocker, CLOSED). One bounded, fully
characterized exception remains: a fixed set of 26 draws (7 distinct meshes —
character eyes; see `<scene>.fixedclock-residual.json` next to the golden)
have `world` transforms that vary run-to-run by up to ~2.0 units. This is a
pre-existing, order-dependent engine nondeterminism in CharEyes/CharLookAt's
per-frame jitter (root-caused by W0.3b.S2: survives a true `dt=0` clock freeze
and a fixed RNG seed, so it is not clock- or seed-driven; disappears for the
*skinned* half of the same symptom class when ASLR is disabled, but this
non-skinned eye-mesh half persists even then — most likely heap-layout/
iteration-order dependent, NOT reproducible by any lever available outside the
engine's char/eye code) — closing it is a separate, substantive engine-side
investigation, out of scope for this mechanical item (no engine changes were
made chasing it here). `compare_drawlogs()` (the shared, UNCHANGED comparator
— same one native/tests/drawlog_compare.h's C++ gtests exercise) still reports
these as failures if run directly; `compare_fixed_clock()` is a thin
gate-decision wrapper that additionally partitions those failures against the
committed, itemized residual sidecar (index + mesh-name-hash, bounded to a
worst-case eps far below any real bug's magnitude — see FIXED_CLOCK_RESIDUAL_EPS)
and only lets a failure through the gate if it EXACTLY matches an itemized,
bounded entry. Any count mismatch, scalar mismatch, bind-group-sharing
mismatch, or *any* world divergence on a non-itemized draw (or one exceeding
the bound) still fails the gate loudly — proven by --fail-red-audit, which
perturbs draw 0 (never in the residual set) by an offset ~50x the residual
bound.

*** DETERMINISM CAVEAT for the LEGACY (non-fixed-clock) mode *** (see
    docs/native/engine-arch-review-2026-07-05/execution/W0.3/STATUS.md
    "W0.3.S3" section for the full investigation)
No live-rendered scene reachable from a fresh headless boot in the current
engine build is exactly frame-reproducible across process launches in this
mode: splash_screen draw counts drift ~1-3% run-to-run (877-894 observed) even
with RB3_GAMEWARM_OFF=1 RB3_TEX_PREWARM_OFF=1 RB3_ASYNC_OPEN_OFF=1 all set
(which rules out background venue/texture prewarm and async file-open races as
the cause) and independent of settle window (30 vs 400 vs 700 frames all
drift). This is exactly the non-determinism --fixed-clock mode fixes by
pinning the capture to an absolute frame count instead of wall-clock/HTTP
timing. Treat routine legacy `(default)` diff-mode runs as a **diagnostic**,
not an unattended CI gate; use `--fixed-clock` for the real gate.

Exit codes: 0 = match (or --update / --determinism-check completed), 1 = boot
or navigation failure, 2 = draw-log comparison found divergence(s).
"""
import argparse, http.client, json, math, os, shutil, signal, socket, subprocess, sys, tempfile, time

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

# --- --fixed-clock mode (W0.3b.S3) ------------------------------------------
# Absolute frame count the bounded boot runs for before MILO_MAX_FRAMES exits
# the loop and the process (and its final RB3_DRAWLOG_DUMP write) tears down.
# Chosen empirically (W0.3b.S2/S3): splash_screen's resident set is already
# fully settled by frame 5 under the fixed clock's forced loader drain-to-
# empty, so 60 leaves ample margin without materially changing what is drawn.
FIXED_CLOCK_FRAMES = 60
FIXED_CLOCK_BOOT_TIMEOUT = 60

# ASLR-off is REQUIRED for --fixed-clock determinism: W0.3b.S2 root-caused a
# ~10-draw skinned-body divergence class to pointer-order-dependent skin/pose
# iteration that vanishes entirely when ASLR is disabled (`setarch -R`). This
# is an invocation-time wrapper (no engine/source change) — exactly the kind
# of harness knob this mechanical item is scoped to use.
def setarch_prefix(no_aslr_off):
    if no_aslr_off:
        return []
    path = shutil.which("setarch")
    if not path:
        log("WARNING: `setarch` not found on PATH — running WITHOUT ASLR-off. "
            "W0.3b.S2 found this reintroduces a ~10-draw skinned-body divergence "
            "class; the gate may spuriously fail. Install util-linux's `setarch` "
            "for a reliable gate, or pass --no-aslr-off to acknowledge.")
        return []
    return [path, "-R"]


def residual_path(scene):
    return os.path.join(GOLDEN_DIR, f"{scene}.fixedclock-residual.json")


def load_residual(scene):
    """Load the committed, itemized fixed-clock residual sidecar (see module
    docstring). Returns {"eps": float, "draws": {index: name}} or None if the
    scene has no sidecar (fixed-clock gate then requires an EXACT match — no
    residual exceptions)."""
    p = residual_path(scene)
    if not os.path.exists(p):
        return None
    with open(p) as f:
        d = json.load(f)
    return {"eps": float(d["eps"]), "draws": {int(x["index"]): x["name"] for x in d["draws"]}}


def capture_fixed_clock(args, tag=""):
    """Bounded, non-HTTP boot under RB3_FIXED_CLOCK: MILO_MAX_FRAMES pins the
    absolute frame count, RB3_DRAWLOG_DUMP is read directly off disk after the
    process exits (no HTTP round trip in the loop at all). Tolerates a
    pre-existing, unrelated teardown SIGSEGV on bounded non-HTTP boots
    (documented in W0.3.STATUS S1 — occurs with RB3_DRAWLOG off too, so it is
    not from this seam) as long as the dump file was written with the expected
    frame index before the crash."""
    dump_path = os.path.join(tempfile.gettempdir(), f"rb3-drawlog-fixedclock-{os.getpid()}{tag.replace('[','_').replace(']','')}.json")
    if os.path.exists(dump_path):
        os.remove(dump_path)
    log_path = os.path.join("/tmp", f"rb3-drawlog-fixedclock-{os.getpid()}{tag.replace('[','_').replace(']','')}.log")
    logf = open(log_path, "w")
    env = dict(os.environ)
    env.update({
        "RB3_GAME": "1", "MILO_HEADLESS": "1", "RB3_DATA": args.data,
        "RB3_DRAWLOG": "1", "RB3_DRAWLOG_DUMP": dump_path,
        "RB3_FIXED_CLOCK": "1",
        "MILO_MAX_FRAMES": str(args.frames),
    })
    env.pop("RB3_HTTP", None)  # bounded non-HTTP boot: no server, no port
    if not args.no_stabilize:
        env.update(STABILIZE_ENV)
    cmd = setarch_prefix(args.no_aslr_off) + [args.bin]
    label = f"capture{tag}"
    log(f"[{label}] launching (fixed-clock, {args.frames} frames): {' '.join(cmd)}, log -> {log_path}")
    try:
        proc = subprocess.run(cmd, env=env, stdout=logf, stderr=subprocess.STDOUT,
                               cwd=REPO, timeout=FIXED_CLOCK_BOOT_TIMEOUT)
        rc = proc.returncode
    except subprocess.TimeoutExpired:
        log(f"[{label}] FAIL: process did not exit within {FIXED_CLOCK_BOOT_TIMEOUT}s")
        logf.close()
        return None
    finally:
        logf.close()
    if not os.path.exists(dump_path):
        log(f"[{label}] FAIL: no dump written at {dump_path} (rc={rc}); see {log_path}")
        return None
    try:
        with open(dump_path) as f:
            d = json.load(f)
    except Exception as e:
        log(f"[{label}] FAIL: dump at {dump_path} did not parse as JSON: {e}")
        return None
    finally:
        try: os.remove(dump_path)
        except OSError: pass
    # MILO_MAX_FRAMES=N: empirically the dump's "frame" counter reads N at exit
    # (post-increment past the Nth Poll), not N-1 — confirmed live (N=60 -> 60).
    expected_frame = args.frames
    if rc != 0:
        log(f"[{label}] note: process exited rc={rc} (non-zero) — tolerated, this is the "
            f"pre-existing bounded-boot teardown SIGSEGV documented in W0.3.STATUS S1, "
            f"unrelated to RB3_DRAWLOG/RB3_FIXED_CLOCK; dump was written before teardown.")
    if d.get("frame") != expected_frame:
        log(f"[{label}] FAIL: dump frame={d.get('frame')} != expected {expected_frame} "
            f"(MILO_MAX_FRAMES={args.frames} - 1); capture did not reach the pinned frame.")
        return None
    if args.keep_log: log(f"[{label}] engine log: {log_path}")
    else:
        try: os.remove(log_path)
        except OSError: pass
    log(f"[{label}] captured frame={d.get('frame')} count={d.get('count')}")
    return d


def compare_fixed_clock(golden, candidate, residual):
    """Gate-decision wrapper around the UNCHANGED compare_drawlogs(): runs the
    exact same comparator (same tolerance constants, same rules — no change to
    world_elem_ok/compare_drawlogs), then partitions any failures against a
    committed, itemized residual (index + expected mesh-name-hash, one bounded
    eps) — see module docstring "RESIDUAL". A failure string is only ever
    reclassified as "expected" by re-deriving it from the SAME golden/candidate
    JSON compare_drawlogs() was given (never by re-parsing the failure string's
    own printed values), so formatting changes to compare_drawlogs() can't
    silently widen what this accepts. Returns
    (gate_passed, all_failures, unexpected_failures, expected_failures)."""
    passed, failures = compare_drawlogs(golden, candidate)
    if passed or not residual:
        return passed, failures, ([] if passed else failures), []

    eps = residual["eps"]
    known = residual["draws"]
    gd, cd = golden.get("draws", []), candidate.get("draws", [])
    unexpected, expected = [], []
    for f in failures:
        toks = f.split()
        idx_tok = toks[1] if len(toks) > 1 else ""
        field = toks[2].split("=", 1)[1] if len(toks) > 2 and toks[2].startswith("field=") else None
        ok = False
        # Only single-draw "world" failures are eligible; "draw N/M field=..."
        # (bind-group sharing) and "count: ..." lines are always hard fails.
        if field == "world" and "/" not in idx_tok and idx_tok.isdigit():
            idx = int(idx_tok)
            if idx in known and idx < len(gd) and idx < len(cd) and gd[idx].get("name") == known[idx]:
                gw, cw = gd[idx].get("world", [0.0] * 16), cd[idx].get("world", [0.0] * 16)
                if all(abs(cw[e] - gw[e]) <= eps for e in range(16)):
                    ok = True
        (expected if ok else unexpected).append(f)
    return (len(unexpected) == 0), failures, unexpected, expected


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
    ap.add_argument("--fixed-clock", action="store_true",
                     help="W0.3b: bounded non-HTTP boot under RB3_FIXED_CLOCK — the reliable "
                          "unattended gate (see module docstring). Without this flag, capture "
                          "uses the legacy HTTP wait-for-scene diagnostic path.")
    ap.add_argument("--frames", type=int, default=FIXED_CLOCK_FRAMES, metavar="N",
                     help="--fixed-clock only: MILO_MAX_FRAMES to pin the capture to (default %(default)s)")
    ap.add_argument("--no-aslr-off", action="store_true",
                     help="--fixed-clock only: skip the `setarch -R` ASLR-off wrapper "
                          "(W0.3b.S2 found this reintroduces skinned-body divergence)")
    ap.add_argument("--verbose", action="store_true")
    ap.add_argument("--keep-log", action="store_true")
    args = ap.parse_args()

    if not os.path.exists(args.bin):
        log(f"FAIL: binary not found: {args.bin}")
        log("      build it: cmake --build native/build-native --target rb3-native")
        return 1

    def do_capture(tag=""):
        return capture_fixed_clock(args, tag=tag) if args.fixed_clock else capture_once(args, tag=tag)

    if args.determinism_check:
        caps = []
        for i in range(args.determinism_check):
            d = do_capture(tag=f"[{i}]")
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
        # of transEps alone. Draw 0 is never in the fixed-clock residual
        # sidecar (see load_residual), and 100.0 is ~50x the residual's eps,
        # so this proves a real regression is still caught loudly in EITHER
        # mode.
        perturbed["draws"][0]["world"][12] += 100.0
        if args.fixed_clock:
            residual = load_residual(args.scene)
            passed, failures, unexpected, expected = compare_fixed_clock(golden, perturbed, residual)
        else:
            passed, failures = compare_drawlogs(golden, perturbed)
        if passed:
            log("FAIL-RED AUDIT FAILED: perturbed golden compared as PASS (comparator is not catching drift)")
            return 2
        log(f"FAIL-RED AUDIT OK: perturbed golden correctly compared as FAIL ({len(failures)} divergence(s)):")
        for x in failures[:5]:
            log(f"  {x}")
        # golden-on-disk is never touched (only in-memory `perturbed` copy was modified)
        return 0

    if args.update:
        d = do_capture()
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
    candidate = do_capture()
    if candidate is None:
        return 1

    if args.fixed_clock:
        residual = load_residual(args.scene)
        passed, failures, unexpected, expected = compare_fixed_clock(golden, candidate, residual)
        if passed:
            extra = f" ({len(expected)} known-residual divergence(s) within bound, non-blocking)" if expected else ""
            log(f"PASS: live capture matches golden ({candidate.get('count')} draws){extra}")
            return 0
        log(f"FAIL: {len(unexpected)} unexpected divergence(s) vs golden "
            f"({len(expected)} known-residual divergence(s) tolerated):")
        for x in unexpected[:30]:
            log(f"  {x}")
        if len(unexpected) > 30:
            log(f"  ... and {len(unexpected) - 30} more")
        return 2

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
