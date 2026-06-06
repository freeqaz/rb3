#!/usr/bin/env python3
"""
frame_profiler.py — TASK A4 (audio-perf-investigation) frame-time / stutter
profiler. Boots rb3-native headless with the RB3_FRAME_TRACE JSONL tracer
(native/src/rb3_frame_trace.cpp), drives nav over the HTTP debug API through
  boot -> main_hub -> song_select (SCROLL several songs to trigger preview-stream
  + album-art loads) -> into a song (gameplay),
then parses the per-frame trace and reports:
  - frame-time histogram
  - the worst N stutters (dt + screen + asset event tag)
  - p50 / p95 / p99 frame ms (overall and per-screen)
  - which screen-transitions / asset events the spikes cluster on.

The tracer records EVERY frame as one JSONL object:
  f    frame index
  dt   wall ms for the whole RunOneFrame (the stutter metric)
  lp   ms in TheLoadMgr.Poll() this frame (budgeted background loader)
  lpu  ms in PollUntilLoaded/PollUntilEmpty this frame (SYNC drains)
  scr  current screen name
  ld   # new loaders ADDED this frame (file/dir/texture/milo mount)
  st   # new audio streams opened this frame (preview / gameplay mogg)
  pend # loaders still pending at end of frame (backlog depth)

Usage:
  python3 scripts/native/frame_profiler.py [--port N] [--data DIR] [--bin PATH]
          [--trace PATH] [--scroll N] [--into-song] [--run-secs S]
          [--worst N] [--keep-log] [--verbose]

Exit 0 = reached song_select + parsed a trace. Exit 1 = boot/nav failed.
"""
import argparse, http.client, json, os, signal, socket, subprocess, sys, time

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DEFAULT_BIN = os.path.join(REPO, "native", "build-native", "rb3-native")
DEFAULT_DATA = os.path.join(REPO, "orig-assets", "extracted")

# Boot -> main_hub -> PLAY NOW -> QUICKPLAY -> song_select. (Scrolling +
# song-confirm are driven live below so we can pace them and see the
# preview-stream / art-load events land on the right frames.)
NAV_TO_SONGSELECT = (
    "@10:start,@30:confirm,"
    "@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn"
)
SERVER_READY_TIMEOUT = 50
SONGSELECT_TIMEOUT = 140


def log(m): print(f"[frame-prof] {m}", flush=True)

def free_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM); s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]; s.close(); return p

def http_get(port, path, timeout=10):
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=timeout)
    try:
        c.request("GET", path); r = c.getresponse(); return r.status, r.read()
    finally:
        c.close()

def http_post(port, path, body, timeout=15):
    # Resilient: a heavy synchronous load can stall the single-threaded HTTP
    # responder past our timeout, or the verb can trigger a transition the
    # server briefly can't answer during. We only care that the verb was
    # delivered (it lands on the next RB3GameInputPoll drain) — swallow any
    # read/disconnect error so the profiler keeps driving + the trace completes.
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=timeout)
    try:
        c.request("POST", path, body=body, headers={"Content-Type": "text/plain"})
        r = c.getresponse(); return r.status, r.read().decode("utf-8", "replace")
    except Exception as e:
        return None, str(e)
    finally:
        try: c.close()
        except Exception: pass

def health(port):
    try:
        st, b = http_get(port, "/api/health")
        if st != 200: return None
        d = json.loads(b.decode("utf-8", "replace"))["data"]
        return int(d["frame"]), float(d.get("songMs", 0)), str(d.get("currentScreen", "?"))
    except Exception:
        return None

def wait_for(port, pred, timeout, label, verbose, proc):
    dl = time.time() + timeout; last = None
    while time.time() < dl:
        if proc.poll() is not None:
            log(f"FAIL: process exited (code {proc.returncode}) waiting for {label}")
            return None
        h = health(port)
        if h is not None:
            f, m, s = h
            if verbose and (f // 30, s) != last:
                log(f"  ...{label}: frame={f} songMs={m:.0f} screen='{s}'")
                last = (f // 30, s)
            if pred(f, m, s): return h
        time.sleep(0.3)
    return None


# --------------------------------------------------------------------------- #
# Trace parsing + reporting
# --------------------------------------------------------------------------- #
def parse_trace(path):
    rows = []
    with open(path, errors="replace") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            try:
                rows.append(json.loads(line))
            except Exception:
                pass
    return rows

def pct(sorted_vals, q):
    if not sorted_vals: return 0.0
    i = min(len(sorted_vals) - 1, int(round((q / 100.0) * (len(sorted_vals) - 1))))
    return sorted_vals[i]

def event_tag(r):
    tags = []
    if r.get("st", 0) > 0:   tags.append(f"STREAM+{r['st']}")
    if r.get("ld", 0) > 0:   tags.append(f"LOAD+{r['ld']}")
    if r.get("lpu", 0) > 1:  tags.append(f"syncDrain={r['lpu']:.0f}ms")
    if r.get("lp", 0) > 1:   tags.append(f"bgLoad={r['lp']:.0f}ms")
    if r.get("pend", 0) > 0: tags.append(f"pend={r['pend']}")
    return ",".join(tags) if tags else "-"

def report(rows, worst_n):
    if not rows:
        print("NO TRACE ROWS PARSED — did the tracer write the file?")
        return
    dts = sorted(r["dt"] for r in rows)
    n = len(rows)
    print("=" * 78)
    print(f"FRAME-TIME PROFILE  ({n} frames)")
    print("-" * 78)
    print(f"  p50={pct(dts,50):7.2f} ms   p95={pct(dts,95):7.2f} ms   "
          f"p99={pct(dts,99):7.2f} ms   max={dts[-1]:7.2f} ms   "
          f"mean={sum(dts)/n:7.2f} ms")

    print("\nHISTOGRAM (frame dt ms):")
    buckets = [(0, 8), (8, 16), (16, 33), (33, 50), (50, 100),
               (100, 250), (250, 500), (500, 1e12)]
    for lo, hi in buckets:
        c = sum(1 for d in dts if lo <= d < hi)
        hilabel = "inf" if hi > 1e11 else str(int(hi))
        bar = "#" * min(60, int(60 * c / n))
        print(f"  [{lo:4d}-{hilabel:>4}) ms : {c:6d}  {bar}")

    # Worst stutters
    sw = sorted(rows, key=lambda r: -r["dt"])[:worst_n]
    print(f"\nWORST {worst_n} STUTTERS (dt / screen / event):")
    print(f"  {'frame':>7} {'dt(ms)':>9}  {'screen':<26} event")
    for r in sw:
        print(f"  {r['f']:>7} {r['dt']:>9.2f}  {r['scr']:<26} {event_tag(r)}")

    # Per-screen percentiles
    by_screen = {}
    for r in rows:
        by_screen.setdefault(r["scr"], []).append(r["dt"])
    print("\nPER-SCREEN frame ms (where do spikes live?):")
    print(f"  {'screen':<26} {'n':>6} {'p50':>7} {'p95':>7} {'p99':>7} {'max':>8}")
    for scr, vals in sorted(by_screen.items(), key=lambda kv: -max(kv[1])):
        vs = sorted(vals)
        print(f"  {scr:<26} {len(vs):>6} {pct(vs,50):>7.1f} {pct(vs,95):>7.1f} "
              f"{pct(vs,99):>7.1f} {vs[-1]:>8.1f}")

    # Spike clustering: how much of the worst tail correlates with asset events
    long_thresh = max(33.0, pct(dts, 95))
    longs = [r for r in rows if r["dt"] >= long_thresh]
    if longs:
        with_event = sum(1 for r in longs
                         if r.get("ld", 0) or r.get("st", 0)
                         or r.get("lp", 0) > 1 or r.get("lpu", 0) > 1
                         or r.get("pend", 0) > 0)
        sumdt = sum(r["dt"] for r in longs)
        sumlp = sum(r.get("lp", 0) for r in longs)
        sumlpu = sum(r.get("lpu", 0) for r in longs)
        print(f"\nSPIKE CLUSTERING (frames >= {long_thresh:.0f} ms, n={len(longs)}):")
        print(f"  {with_event}/{len(longs)} long frames carry an asset signal "
              f"(load/stream/pend/loader-ms)")
        print(f"  of {sumdt:.0f} ms total long-frame time: "
              f"{sumlp:.0f} ms in LoadMgr.Poll (bg), "
              f"{sumlpu:.0f} ms in PollUntil* (SYNC drain), "
              f"{sumdt - sumlp - sumlpu:.0f} ms elsewhere (draw/poll/gpu)")
    # Transition spikes: first frame on each new screen
    print("\nSCREEN-TRANSITION COST (first frame after each screen change):")
    prev = None
    print(f"  {'frame':>7} {'dt(ms)':>9}  {'-> screen':<26} event")
    for r in rows:
        if r["scr"] != prev:
            print(f"  {r['f']:>7} {r['dt']:>9.2f}  {r['scr']:<26} {event_tag(r)}")
            prev = r["scr"]
    print("=" * 78)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=0)
    ap.add_argument("--bin", default=DEFAULT_BIN)
    ap.add_argument("--data", default=DEFAULT_DATA)
    ap.add_argument("--trace", default="/tmp/rb3-frame-trace.jsonl")
    ap.add_argument("--scroll", type=int, default=12,
                    help="# of song_select 'down' verbs to fire (drives preview + art loads)")
    ap.add_argument("--scroll-pace", type=float, default=0.45,
                    help="seconds between scroll verbs (slow enough to let a preview start)")
    ap.add_argument("--into-song", action="store_true",
                    help="after scrolling, confirm a song + start gameplay")
    ap.add_argument("--run-secs", type=float, default=20.0,
                    help="extra steady-state hold after nav (gameplay / hover dwell)")
    ap.add_argument("--worst", type=int, default=15)
    ap.add_argument("--keep-log", action="store_true")
    ap.add_argument("--verbose", action="store_true")
    ap.add_argument("--parse-only", default=None,
                    help="skip launch; just parse this existing trace file")
    args = ap.parse_args()

    if args.parse_only:
        report(parse_trace(args.parse_only), args.worst)
        return 0

    if not os.path.exists(args.bin):
        log(f"FAIL: binary not found: {args.bin}")
        log("      build it: cmake --build native/build-native -j$(nproc)")
        return 1

    if os.path.exists(args.trace):
        try: os.remove(args.trace)
        except OSError: pass

    port = args.port or free_port()
    log_path = os.path.join("/tmp", f"rb3-frameprof-{port}.log")
    logf = open(log_path, "w")
    env = dict(os.environ)
    env.update({
        "RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
        "MILO_HEADLESS": "1", "RB3_DATA": args.data,
        "RB3_FRAME_TRACE": args.trace,
        "RB3_GAME_INPUT": NAV_TO_SONGSELECT,
    })
    log(f"launching rb3-native (port {port}) trace -> {args.trace}, log -> {log_path}")
    proc = subprocess.Popen([args.bin], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)
    rc = 1
    try:
        if wait_for(port, lambda f, m, s: True, SERVER_READY_TIMEOUT,
                    "server", args.verbose, proc) is None:
            log("FAIL: HTTP server never came up"); return 1
        log("HTTP server up")
        h = wait_for(port, lambda f, m, s: s == "song_select_screen",
                     SONGSELECT_TIMEOUT, "song_select", args.verbose, proc)
        if h is None:
            log("FAIL: never reached song_select_screen"); return 1
        log(f"song_select reached at frame={h[0]}; scrolling {args.scroll} songs "
            f"(pace={args.scroll_pace}s) to trigger preview-stream + art loads")
        time.sleep(1.5)  # let the list settle + first preview timer arm

        for i in range(args.scroll):
            http_post(port, "/api/input", "down")
            time.sleep(args.scroll_pace)  # dwell so a hover preview can START
        log("scroll done")

        if args.into_song:
            log("confirming highlighted song -> guitar/expert -> gameplay")
            http_post(port, "/api/input", "msg:music_library:select_highlighted_node")
            time.sleep(1.0)
            http_post(port, "/api/input", "track:guitar"); time.sleep(0.3)
            http_post(port, "/api/input", "difficulty:expert"); time.sleep(0.3)
            http_post(port, "/api/input", "msg:overshell:end_override_flow:1:0")
            time.sleep(0.3)
            http_post(port, "/api/input", "nofail"); time.sleep(0.2)
            http_post(port, "/api/input", "autohit")
            # let gameplay run so the song mogg stream + in-song loads register
            t0 = time.time()
            while time.time() - t0 < args.run_secs:
                if proc.poll() is not None: break
                h = health(port)
                if args.verbose and h: log(f"  steady: frame={h[0]} screen={h[2]} songMs={h[1]:.0f}")
                time.sleep(0.5)
        else:
            t0 = time.time()
            while time.time() - t0 < args.run_secs:
                if proc.poll() is not None: break
                time.sleep(0.5)

        rc = 0
    finally:
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
            try: proc.wait(timeout=8)
            except subprocess.TimeoutExpired: os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
        except Exception: pass
        logf.close()

    # Trace is flushed per-frame, so even a SIGTERM leaves it complete.
    rows = parse_trace(args.trace)
    log(f"parsed {len(rows)} frame records from {args.trace}")
    report(rows, args.worst)
    if rc != 0 or args.keep_log:
        log(f"engine log: {log_path}")
    return rc


if __name__ == "__main__":
    sys.exit(main())
