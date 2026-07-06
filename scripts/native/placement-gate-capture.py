#!/usr/bin/env python3
"""placement-gate-capture.py — W2.1.S1 gameplay placement-gate capture.

Stands up the correctness gate the splash draw-log golden cannot provide. The
committed drawlog golden is `splash_screen` — NO crowd, NO drum kit — so the
canonical comparator is blind to the SYS-1 placement bug: `DrawMesh` forces
`obj.world = identity` for every skinned draw (Rnd_Wgpu_RB3.cpp:2847-2848), so
every crowd 3D-char instance co-locates at the origin.

This harness:
  1. boots rb3-native headless (RB3_HTTP) with RB3_DRAWLOG + RB3_PLACEMENT_PROBE
     on and RB3_FIXED_CLOCK for a stable sim clock;
  2. navigates to gameplay (reusing keyboard-to-gameplay's proven nav) where the
     crowd + drum kit are in the scene;
  3. optionally pins a wide venue camera (rb3_director_disable FIRST, then
     rb3_force_shot) so the crowd is in-frustum and the frame is reproducible;
  4. GETs /api/drawlog -> the per-draw state log JSON;
  5. extracts the RB3_PLACEMENT_PROBE lines (the faithful per-instance spXfm
     placement the decomp computed at Crowd.cpp) from the engine log;
  6. runs the placement oracle (native/tests/test_placement_oracle.cpp via
     rb3-tests, RealCaptureSpansBowl) over the two artifacts.

On the UNCHANGED 6221a56 build this is RED (crowd drawn at identity) — the free
fail-red proving the gate sees the bug. Under the flag-ON W2.1.S2 build it turns
GREEN.

Usage:
  python3 scripts/native/placement-gate-capture.py \
      [--bin native/build-agent-W2.1/rb3-native] \
      [--tests native/build-agent-W2.1/rb3-tests] \
      [--song-downs 4] [--shot coop_all_wide,coop_est_00] \
      [--out /tmp/rb3-placement-gate] [--update-red-golden] [--verbose]

Exit code:
  0 = oracle PASS (crowd spread == spXfm, spans the bowl) — the fixed build.
  1 = oracle RED (co-located / not-drawn-at-spXfm) — the current build's fail-red.
  2 = ERROR (never reached gameplay / no probe lines / capture failure).
Note: the fail-red run is EXPECTED to exit 1 on the current engine; that is the
gate working. Pass --expect-red to invert the exit code for CI-style fail-red
auditing (exit 0 iff the gate went RED).
"""
import argparse, json, os, re, signal, subprocess, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import importlib.util
_spec = importlib.util.spec_from_file_location(
    "kbd2game", os.path.join(HERE, "keyboard-to-gameplay.py"))
k = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(k)
REPO = k.REPO

# Wide / establishing venue-shot candidates that keep the crowd in-frustum. Sent
# both bare and with a `.shot` suffix (band-closeup-capture.py convention); the
# first that resolves via rb3_force_shot is pinned. If none resolve we still
# capture (the probe guarantees the RED signal regardless of the camera).
DEFAULT_SHOTS = ["coop_all_wide", "coop_all_n00", "coop_est_00", "coop_wide_00",
                 "coop_v_n00", "coop_front_n00", "coop_d_n01"]


def log(m): print(f"[placement-gate] {m}", flush=True)


def force_shot(port, name):
    """Try `name` and `name.shot`; return the resolved name or None."""
    for cand in (name, name if name.endswith(".shot") else name + ".shot"):
        r = k.dta(port, '{rb3_force_shot "%s"}' % cand)
        if isinstance(r, int):
            return None  # hook missing (returned int) — signal to caller
        if r and "ok" in str(r).lower():
            return cand
    return None


def cur_shot(port):
    return k.dta(port, "{rb3_cur_shot}")


def nav_to_gameplay(port, proc, song_downs, diff_idx, verbose):
    """Reuse keyboard-to-gameplay's proven boot->game_screen nav (verbatim, as
    band-closeup-capture.py does)."""
    if k.wait_screen(port, lambda s: True, 40, proc) is None:
        log("FAIL: HTTP server never came up"); return False
    log("HTTP up")
    k.wait_screen(port, lambda s: s and s != "", 60, proc, verbose)
    for _ in range(8):
        if k.health(port)[2] == "main_hub_screen": break
        k.press(port, k.START); k.drain_pad(port); time.sleep(0.6)
    if k.wait_screen(port, "main_hub_screen", 30, proc, verbose) is None:
        log("FAIL: no main_hub"); return False
    for _ in range(10):
        if k.health(port)[2] == "song_select_screen": break
        k.press(port, k.CONFIRM); k.drain_pad(port); time.sleep(0.7)
    if k.wait_screen(port, "song_select_screen", 40, proc, verbose) is None:
        log("FAIL: no song_select"); return False
    time.sleep(1.5)
    for _ in range(song_downs):
        k.press(port, k.DDOWN); k.drain_pad(port); time.sleep(0.2)
    k.press(port, k.CONFIRM); k.drain_pad(port)
    if k.wait_screen(port, "part_difficulty_screen", 60, proc, verbose) is None:
        log("FAIL: no part_difficulty"); return False
    ov = k.wait_view(port, lambda v: v.startswith("choose_part") or v == "choose_diff",
                     30, proc, "choose_part", verbose)
    if ov and ov[0].startswith("choose_part"):
        k.press(port, k.CONFIRM); k.drain_pad(port)
    ov = k.overshell(port); g = 0
    while ov[0] == "confirm_action" and g < 4:
        k.press(port, k.CONFIRM); k.drain_pad(port); time.sleep(0.5); ov = k.overshell(port); g += 1
    ov = k.wait_view(port, lambda v: v == "choose_diff", 30, proc, "choose_diff", verbose)
    if ov:
        for _ in range(diff_idx):
            k.press(port, k.DDOWN); k.drain_pad(port); time.sleep(0.25)
        k.press(port, k.CONFIRM); k.drain_pad(port)
    ov = k.overshell(port); g = 0
    while ov[0] == "confirm_action" and g < 4:
        k.press(port, k.CONFIRM); k.drain_pad(port); time.sleep(0.5); ov = k.overshell(port); g += 1
    k.wait_view(port, lambda v: v == "ready_to_play", 30, proc, "ready", verbose)
    if k.wait_screen(port, "game_screen", 90, proc, verbose) is None:
        log("FAIL: no game_screen"); return False
    log("game_screen reached")
    return True


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", type=int, default=0)
    ap.add_argument("--bin", default=os.path.join(REPO, "native", "build-agent-W2.1", "rb3-native"))
    ap.add_argument("--tests", default=os.path.join(REPO, "native", "build-agent-W2.1", "rb3-tests"))
    ap.add_argument("--data", default=k.DEFAULT_DATA)
    ap.add_argument("--overlay", default=os.path.join(REPO, "native", "dta"))
    ap.add_argument("--diff", default="hard", choices=list(k.DIFF_INDEX))
    ap.add_argument("--song-downs", type=int, default=4)
    ap.add_argument("--shot", default=",".join(DEFAULT_SHOTS),
                    help="comma-separated wide-shot candidates; first that resolves is pinned")
    ap.add_argument("--no-force-shot", action="store_true",
                    help="do not pin a camera (capture whatever the director shows)")
    ap.add_argument("--out", default="/tmp/rb3-placement-gate")
    ap.add_argument("--update-red-golden", action="store_true",
                    help="copy the captured drawlog to the committed RED-reference golden")
    ap.add_argument("--expect-red", action="store_true",
                    help="invert exit: 0 iff the selected gate(s) went RED (fail-red audit)")
    ap.add_argument("--gate", default="crowd", choices=["crowd", "drum", "both"],
                    help="which placement oracle to gate the exit on: crowd "
                         "(RealCaptureSpansBowl, default), drum (RealCaptureDrumPlaced, "
                         "W2.1-flip.S2), or both. The other verdict is always reported.")
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    if not os.path.exists(args.bin):
        log(f"FAIL: binary not found: {args.bin}"); return 2
    os.makedirs(args.out, exist_ok=True)
    drawlog_path = os.path.join(args.out, "gameplay_crowd.drawlog.json")
    probe_path   = os.path.join(args.out, "gameplay_crowd.probe.log")

    port = args.port or k.free_port()
    log_path = os.path.join(args.out, f"engine-{port}.log")
    logf = open(log_path, "w")
    env = dict(os.environ)
    env.update({"RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
                "MILO_HEADLESS": "1", "RB3_DATA": args.data,
                "RB3_DTA_OVERLAY": args.overlay,
                "RB3_DRAWLOG": "1", "RB3_PLACEMENT_PROBE": "1",
                "RB3_FIXED_CLOCK": "1"})
    log(f"launching rb3-native (port {port}); engine log -> {log_path}")
    proc = subprocess.Popen([args.bin], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)
    rc = 2
    try:
        if not nav_to_gameplay(port, proc, args.song_downs, k.DIFF_INDEX[args.diff], args.verbose):
            return 2
        k.verb(port, "nofail")
        # let the song actually start (proves the crowd/venue scene is live)
        start = -1; dl = time.time() + 60
        while time.time() < dl:
            ip = k.dta(port, "{game is_playing}"); h = k.health(port)
            if ip is not None and int(ip) == 1 and start < 0 and h and h[1] >= 0:
                start = h[1]
            k.verb(port, "autohit")
            if start >= 0 and h and h[1] > start + 300:
                break
            time.sleep(0.5)
        log(f"is_playing songMs={k.health(port)[1]:.0f}")

        # pin a wide shot so the crowd is in-frustum + the frame is reproducible.
        if not args.no_force_shot:
            dz = k.dta(port, "{rb3_director_disable 1}")
            if isinstance(dz, int) and dz == 1:
                pinned = None
                for cand in [s.strip() for s in args.shot.split(",") if s.strip()]:
                    resolved = force_shot(port, cand)
                    if resolved:
                        pinned = resolved; break
                if pinned:
                    for _ in range(3):
                        k.verb(port, "autohit"); time.sleep(0.15)
                    log(f"pinned wide shot {pinned!r} (cur_shot={cur_shot(port)!r})")
                else:
                    log("no wide shot resolved — capturing at the director's current angle "
                        "(probe still guarantees the placement signal)")
            else:
                log(f"rb3_director_disable echoed {dz!r} — capturing without a pin")

        # Frame-scope the probe: record the engine-log size NOW so we extract only
        # the RB3_PLACEMENT_PROBE lines from the settle+capture window (the probe
        # fires every frame for every instance; over a whole session that is 100k+
        # lines). The probe writes to UNBUFFERED stderr merged into this log, so
        # the byte offset tracks it promptly.
        logf.flush()
        probe_offset = os.path.getsize(log_path)
        # settle a few frames so the crowd is fully posed + drawn, then capture.
        for _ in range(4):
            k.verb(port, "autohit"); time.sleep(0.2)
        status, raw = k.http_get(port, "/api/drawlog")
        body = raw.decode("utf-8", "replace") if isinstance(raw, (bytes, bytearray)) else str(raw)
        if status != 200 or not body:
            log(f"FAIL: /api/drawlog status={status} len={len(body)}"); return 2
        with open(drawlog_path, "w") as f:
            f.write(body)
        try:
            frame = json.loads(body)
            nskin = sum(1 for d in frame.get("draws", []) if d.get("skinned"))
            log(f"drawlog: {frame.get('count')} draws ({nskin} skinned) -> {drawlog_path}")
        except Exception as e:
            log(f"FAIL: drawlog did not parse: {e}"); return 2
        rc = 0
    finally:
        logf.flush()
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
            proc.wait(timeout=10)
        except Exception:
            try: os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
            except Exception: pass
        logf.close()
    if rc != 0:
        return rc

    # Extract the RB3_PLACEMENT_PROBE lines into the probe file.
    #  * crowd lines: only from the capture window (from probe_offset onward). The
    #    crowd probe fires every frame for every instance (100k+ lines / session),
    #    so window-scoping keeps the frame we captured.
    #  * drum lines: from the WHOLE log, deduped. The drum probe fires from
    #    BandConfiguration::SyncPlayMode at band/venue setup — typically BEFORE the
    #    window opens — and is rare (one per resolved slot per SyncPlayMode call),
    #    so whole-log dedupe is both correct and cheap.
    ncrowd = ndrum = 0
    crowd_lines, drum_lines, drum_seen = [], [], set()
    with open(log_path, "r", errors="replace") as lf:
        for line in lf:
            if "RB3_PLACEMENT_PROBE" not in line: continue
            if " drum inst=" in line:
                key = line.strip()
                if key not in drum_seen:
                    drum_seen.add(key)
                    drum_lines.append(line if line.endswith("\n") else line + "\n")
                    ndrum += 1
    with open(log_path, "r", errors="replace") as lf:
        try: lf.seek(probe_offset)
        except Exception: pass
        for line in lf:
            if "RB3_PLACEMENT_PROBE" in line and " crowd inst=" in line:
                crowd_lines.append(line if line.endswith("\n") else line + "\n")
                ncrowd += 1
    with open(probe_path, "w") as pf:
        pf.writelines(crowd_lines)
        pf.writelines(drum_lines)
    log(f"probe: {ncrowd} crowd-instance lines (window) + {ndrum} drum-ref lines "
        f"(whole log, deduped) -> {probe_path}")
    if ncrowd == 0 and ndrum == 0:
        log("FAIL: no RB3_PLACEMENT_PROBE lines captured — did gameplay load a crowd/"
            "band? (the venue may have an empty crowd, or RB3_PLACEMENT_PROBE was not "
            "honored)")
        return 2

    if args.update_red_golden:
        dst = os.path.join(REPO, "native", "tests", "goldens", "drawlog", "gameplay_crowd.json")
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        with open(drawlog_path) as s, open(dst, "w") as d:
            d.write(s.read())
        log(f"wrote RED-reference golden -> {dst}")

    # Run the live oracle(s) via rb3-tests. Crowd = RealCaptureSpansBowl,
    # drum (W2.1-flip.S2) = RealCaptureDrumPlaced. Both are always run+reported;
    # --gate selects which drives the exit code.
    GATE_TESTS = {"crowd": "RealCaptureSpansBowl", "drum": "RealCaptureDrumPlaced"}
    if not os.path.exists(args.tests):
        log(f"NOTE: rb3-tests not found at {args.tests}; artifacts written, oracle not run.")
        log(f"  run: RB3_PLACEMENT_DRAWLOG={drawlog_path} RB3_PLACEMENT_PROBE_LOG={probe_path} \\")
        log(f"       {args.tests} --gtest_filter='PlacementOracle.RealCaptureSpansBowl:"
            f"PlacementOracle.RealCaptureDrumPlaced'")
        return 0
    tenv = dict(os.environ)
    tenv.update({"RB3_PLACEMENT_DRAWLOG": drawlog_path, "RB3_PLACEMENT_PROBE_LOG": probe_path})
    gfilter = ":".join(f"PlacementOracle.{t}" for t in GATE_TESTS.values())
    log(f"running oracle(s): {gfilter}")
    tp = subprocess.run([args.tests, f"--gtest_filter={gfilter}"],
                        env=tenv, capture_output=True, text=True)
    sys.stdout.write(tp.stdout)
    if tp.stderr: sys.stderr.write(tp.stderr)

    def verdict(name):
        # 'green' | 'red' | 'skip' | 'missing' from the per-test gtest status line.
        if f"[       OK ] PlacementOracle.{name}" in tp.stdout: return "green"
        if f"[  FAILED  ] PlacementOracle.{name}" in tp.stdout: return "red"
        if f"[  SKIPPED ] PlacementOracle.{name}" in tp.stdout: return "skip"
        return "missing"

    verdicts = {g: verdict(t) for g, t in GATE_TESTS.items()}
    _label = {"crowd": ("crowd instances are NOT drawn at their faithful spXfm positions",
                        "crowd spread matches spXfm",
                        "capture did not reach a spread crowd frame"),
              "drum":  ("drum kit / band is NOT drawn near its band waypoint (kit at origin)",
                        "drum kit / band drawn near its band waypoint",
                        "no non-origin band waypoint reference (SyncPlayMode did not place)")}
    for g in ("crowd", "drum"):
        v = verdicts[g]
        red_msg, green_msg, skip_msg = _label[g]
        if v == "red":
            log(f"{g.upper()} ORACLE RED — {red_msg} (fail-red).")
        elif v == "green":
            log(f"{g.upper()} ORACLE GREEN — {green_msg}.")
        elif v == "skip":
            log(f"{g.upper()} ORACLE INCONCLUSIVE — {skip_msg}.")
        else:
            log(f"{g.upper()} ORACLE MISSING (test did not run — check filter/build).")

    want = ["crowd", "drum"] if args.gate == "both" else [args.gate]
    sel = [verdicts[g] for g in want]
    if args.expect_red:
        # fail-red audit: every selected gate must be RED.
        return 0 if all(v == "red" for v in sel) else 1
    if any(v in ("skip", "missing") for v in sel):
        return 2
    return 0 if all(v == "green" for v in sel) else 1


if __name__ == "__main__":
    sys.exit(main())
