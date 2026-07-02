#!/usr/bin/env python3
"""
loadperf-frametail.py — TRACK-B measurement: boot rb3-native through the menu
into gameplay and report the long-frame tail (per-frame RunOneFrame wall time)
so the loader time-slicing fix can be measured.

The web freeze is exactly this native tail: a single RunOneFrame that runs
hundreds of ms of synchronous load work blocks the browser tab the same way it
blocks this native frame loop. Collapsing the native tail predicts a smooth tab.

Launches rb3-native with RB3_GAME=1 + RB3_GAME_INPUT=<nav-script> (the same
convention as song-end-test.py) and RB3_FRAME_INSTRUMENT=1, lets the canned nav
drive main_hub -> PLAY NOW -> QUICKPLAY -> song_select -> a song -> gameplay,
holds, then SIGTERMs. Parses "RB3 FRAME-INSTRUMENT ... LONG" log lines and
prints the long-frame count, max ms, worst frames, and a histogram.

Env knobs forwarded to the child:
  RB3_LOADER_BUDGET_MS            (default 8 in-binary; huge value ~= unbudgeted)
  RB3_FRAME_INSTRUMENT_THRESH_MS  (default 20)
  RB3_PUL_BUDGET_MS               (TRACK-B: PollUntilLoaded/Empty per-frame cap;
                                   default 0 = off = synchronous drain)

Usage:
  python3 scripts/native/loadperf-frametail.py [--budget-ms N] [--pul-ms N]
          [--thresh-ms N] [--label NAME] [--run-secs S]
"""
import argparse, http.client, json, os, re, signal, socket, subprocess, sys, time

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DEFAULT_BIN = os.path.join(REPO, "native", "build-native", "rb3-native")
DEFAULT_DATA = os.path.join(REPO, "orig-assets", "extracted")

# Same nav as song-end-test.py: boot -> main_hub -> PLAY NOW -> QUICKPLAY ->
# song_select -> pick highlighted song -> guitar/expert -> begin gameplay.
NAV_SCRIPT = (
    "@10:start,@30:confirm,"
    "@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn,"
    "@320:down,@350:msg:music_library:select_highlighted_node,"
    "@380:part:guitar,@400:diff:expert,"
    "@500:nofail,@520:autohit"
)
SERVER_READY_TIMEOUT = 50


def log(m): print(f"[loadperf] {m}", flush=True)

def free_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM); s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]; s.close(); return p

def http_get(port, path, timeout=10):
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=timeout)
    try:
        c.request("GET", path); r = c.getresponse(); return r.status, r.read()
    finally:
        c.close()

def health(port):
    try:
        st, b = http_get(port, "/api/health")
        if st != 200: return None
        d = json.loads(b.decode("utf-8", "replace"))["data"]
        return int(d["frame"]), float(d.get("songMs", 0)), str(d.get("currentScreen", "?"))
    except Exception:
        return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--budget-ms", default=None)
    ap.add_argument("--pul-ms", default=None)
    ap.add_argument("--thresh-ms", default="20")
    ap.add_argument("--label", default="run")
    ap.add_argument("--bin", default=DEFAULT_BIN)
    ap.add_argument("--data", default=DEFAULT_DATA)
    # +30s headroom vs the old direct-commit nav: the real part_difficulty
    # screen (part:/diff: pad-press verbs) takes longer to reach gameplay than
    # the old instant track:/difficulty: skip.
    ap.add_argument("--run-secs", type=float, default=90.0)
    args = ap.parse_args()

    if not os.path.exists(args.bin):
        log(f"FAIL: binary not found: {args.bin}"); return 1

    port = free_port()
    env = dict(os.environ)
    env.update({
        "RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
        "MILO_HEADLESS": "1", "RB3_DATA": args.data, "RB3_GAME_INPUT": NAV_SCRIPT,
        "RB3_FRAME_INSTRUMENT": "1", "RB3_FRAME_INSTRUMENT_THRESH_MS": args.thresh_ms,
    })
    if args.budget_ms is not None:
        env["RB3_LOADER_BUDGET_MS"] = args.budget_ms
    if args.pul_ms is not None:
        env["RB3_PUL_BUDGET_MS"] = args.pul_ms

    logpath = f"/tmp/loadperf_{args.label}.log"
    lf = open(logpath, "w")
    log(f"launching label={args.label} budget_ms={args.budget_ms} pul_ms={args.pul_ms} "
        f"port={port} -> {logpath}")
    proc = subprocess.Popen([args.bin], cwd=REPO, env=env,
                            stdout=lf, stderr=subprocess.STDOUT,
                            start_new_session=True)
    try:
        t0 = time.time(); ready = False
        while time.time() - t0 < SERVER_READY_TIMEOUT:
            if proc.poll() is not None:
                log(f"child exited early rc={proc.returncode}"); break
            if health(port) is not None: ready = True; break
            time.sleep(0.3)
        if not ready:
            log("server never came up"); return 2

        last = None; t1 = time.time()
        while time.time() - t1 < args.run_secs:
            if proc.poll() is not None:
                log(f"child exited rc={proc.returncode}"); break
            h = health(port)
            if h and h[2] != last:
                log(f"  frame={h[0]} screen={h[2]} songMs={h[1]:.0f}"); last = h[2]
            time.sleep(0.1)
    finally:
        if proc.poll() is None:
            try:
                os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
                proc.wait(timeout=6)
            except Exception:
                try: os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
                except Exception: pass
        lf.close()

    longs = []; maxms = 0.0
    # frame N LONG X ms (poll=P pollUntil=Q screen=S) [max=.. longCount=..]
    pat = re.compile(
        r"frame (\d+) LONG ([\d.]+) ms \(poll=([\d.]+) pollUntil=([\d.]+) screen=([^)]*)\)")
    for line in open(logpath, errors="replace"):
        m = pat.search(line)
        if m:
            ms = float(m.group(2))
            longs.append((ms, int(m.group(1)), float(m.group(3)),
                          float(m.group(4)), m.group(5)))
            if ms > maxms: maxms = ms
    longs.sort(reverse=True)

    print("=" * 64)
    print(f"LABEL={args.label}  budget_ms={args.budget_ms}  pul_ms={args.pul_ms}  "
          f"thresh_ms={args.thresh_ms}")
    # A frame is "loader-attributable" if >5ms of its time was in the loader
    # (poll+pollUntil). Steady-state splash/gameplay frames (poll=0,pollUntil=0)
    # are non-loader CPU and out of Track-B's loader-slicing scope — separate them.
    loaderframes = [t for t in longs if (t[2] + t[3]) > 5.0]
    cpuframes = [t for t in longs if (t[2] + t[3]) <= 5.0]
    print(f"long-frame count (>{args.thresh_ms}ms): {len(longs)}  "
          f"(loader-attributable={len(loaderframes)}, non-loader-cpu={len(cpuframes)})")
    print(f"max frame ms: {maxms:.1f}")
    loadermax = max((t[0] for t in loaderframes), default=0.0)
    print(f"max LOADER-attributable frame ms: {loadermax:.1f}")
    print("worst 12 LOADER long frames (ms / poll / pollUntil / frame# / screen):")
    for ms, fr, poll, pul, scr in loaderframes[:12]:
        print(f"   {ms:8.1f}  poll={poll:6.1f}  pollUntil={pul:7.1f}  frame {fr:5d}  {scr}")
    print("worst 5 NON-loader-cpu long frames (out of scope):")
    for ms, fr, poll, pul, scr in cpuframes[:5]:
        print(f"   {ms:8.1f}  poll={poll:6.1f}  pollUntil={pul:7.1f}  frame {fr:5d}  {scr}")
    buckets = [(20, 50), (50, 100), (100, 250), (250, 500), (500, 1e9)]
    print("histogram (loader-attributable only):")
    for lo, hi in buckets:
        n = sum(1 for t in loaderframes if lo <= t[0] < hi)
        hilabel = "inf" if hi > 1e8 else str(int(hi))
        print(f"   [{lo:4d}-{hilabel:>4}) ms : {n}")
    print(f"(full log: {logpath})")
    print("=" * 64)
    return 0


if __name__ == "__main__":
    sys.exit(main())
