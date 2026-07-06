#!/usr/bin/env python3
"""
hand-closeup-capture.py — W2.2.S1c deterministic hand/finger/hair closeup capture.

Produces the reviewer-judged VISUAL layer of W2.2's four-layer anti-revert exit
(WAVE3_REVIEW Amendment B1, layer 3): framed hand/finger/hair closeup screenshots,
explicitly INCLUDING the count-in/walk-on window — the known thin-geo shard window
(memory: "walkon count-in pose ... thin-geo count-in shards = pose-independent
skinning residual") that the W0.5 lineup gate does NOT frame (it captures wide
establishing band shots, not per-member closeups, and not the count-in beat).

Boot/nav follows the same frame-timed `RB3_GAME_INPUT` script as
`hands_bind_characterize.py` / `song-end-test.py` (deterministic: keyed off
simulated frame count via `@N:verb` timestamps, not wall-clock sleeps) —
boot -> main_hub -> quickplay -> song_select -> game_screen.

Once on `game_screen`, this harness PINS a venue camera shot using the
native-only DTA accessors added for band-closeup-capture.py
(`native/src/rb3_http_handlers.cpp`):
  {rb3_director_disable 1}     -> freeze the per-frame auto camera re-pick
  {rb3_force_shot "<name>.shot"} -> pin a BandCamShot by name
  {rb3_cur_shot}                -> live mCurShot readback (the pin-held proof)
so every captured frame is framed on the SAME hand-visible shot instead of
whatever the auto-director happened to be showing (avoids the camera-desync
false-positive documented in docs/native/converge-2026-06-20/).

Shot candidates are tried in priority order (guitar/bass closeups show the most
finger/hand detail; vocals shows mic-holding fingers; drums shows stick-holding
hands) — see HAND_CLOSEUP_SHOTS. Each is tried bare and with a `.shot` suffix
(ObjectDir::Find needs the suffix in this venue family); the first that resolves
("force_shot ok") is pinned and used for ALL frames in this run (one shot per
run keeps every frame in the manifest comparable). If NONE resolve (unfamiliar
venue), the harness falls back to the free-running auto-director and captures
whatever it shows, marking the manifest `shot_pinned: false` so a reviewer knows
the frame set was not camera-deterministic.

Captures a FIXED, deterministic label set (same names every run -> stable diffing):
  countin     — game_screen entered, songMs ~0 (the count-in/walk-on window itself)
  walkon      — songMs advanced (via autohit, a deterministic sim-clock gate — NOT
                a wall sleep) to ~2/3 through the count-in, still walk-on/pre-note
  play_00..N  — steady-play frames at fixed songMs offsets past note-start

Each frame is written to a FIXED filename (no timestamp) so re-running overwrites
the same golden set for direct pixel/visual diffing; `manifest.json` records the
frame label, songMs, requested/resolved/readback shot name, and file path for
every capture — the record a reviewer (or scripts/analysis/visual_diff.py) needs.

To A/B the walk-on-snap fix mentioned in the S1c brief, set
`RB3_WALKON_SNAP_OFF=1` in the environment before invoking this script (it is
propagated into the subprocess env like every other native diagnostic toggle in
this repo — see band-closeup-capture.py) and compare the two manifests/PNG sets.

Usage:
    python3 scripts/native/hand-closeup-capture.py [--bin PATH] [--data DIR]
            [--out DIR] [--shots NAME1,NAME2,...] [--plays N] [--play-dt MS]
            [--verbose] [--keep-log]

Exit 0 = all frames captured (pin held throughout, or documented fallback).
Exit 1 = harness failure (boot/nav failed, never reached game_screen, or a
         screenshot call failed outright).
"""
import argparse
import http.client
import json
import os
import signal
import socket
import subprocess
import sys
import time

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DEFAULT_BIN = os.path.join(REPO, "native", "build-agent-W2.2", "rb3-native")
DEFAULT_DATA = os.path.join(REPO, "orig-assets", "extracted")
DEFAULT_OUT = os.path.join(
    REPO, "docs", "native", "engine-arch-review-2026-07-05",
    "execution", "W2.2", "goldens", "hand-closeup")

# Canonical headless boot-to-song nav (same sequence as hands_bind_characterize.py /
# song-end-test.py). Frame-timestamped (`@N:`), not wall-clock — the same nav that
# W2.2.S1a already proved reaches game_screen deterministically in this build.
NAV_SCRIPT = (
    "@10:start,@30:confirm,"
    "@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn,"
    "@320:down,@350:msg:music_library:select_highlighted_node,"
    "@380:part:guitar,@400:diff:expert,"
    "@500:nofail,@520:autohit"
)

SERVER_READY_TIMEOUT = 45
GAMEPLAY_TIMEOUT = 240  # boot + nav + cinematic intro pre-roll

# Per-member hand-closeup shot candidates, priority order (memory-proven names in
# the club-family venue quickplay lands on; band-closeup-capture.py MEMBER_SHOTS).
# Tried bare then with a `.shot` suffix; first "force_shot ok" wins for the run.
HAND_CLOSEUP_SHOTS = [
    "coop_g_cg", "coop_g_cg01", "coop_g_n01", "coop_g_n03",   # guitar hand/fret
    "coop_b_cg", "coop_b_cg01", "coop_b_n01",                  # bass hand/fret
    "coop_d_n01", "coop_d_n02",                                 # drums stick-hands
    "coop_v_n01", "coop_front_n01", "coop_front_n00",          # vocals mic-hand
    "coop_k_n01",                                               # keys hand
]

# Count-in duration is ~0-7s (memory: walkon count-in doc). "walkon" samples partway
# through that window (still pre-note); "play_*" frames start comfortably after
# note-start so autohit has real notes to hit.
WALKON_SONGMS = 2500.0
PLAY_START_SONGMS = 4000.0


def log(msg):
    print(f"[hand-closeup] {msg}", flush=True)


def free_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]
    s.close()
    return p


def http_get(port, path, timeout=10):
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=timeout)
    try:
        c.request("GET", path)
        r = c.getresponse()
        return r.status, r.read()
    finally:
        c.close()


def http_post(port, path, body, timeout=15):
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=timeout)
    try:
        c.request("POST", path, body=body, headers={"Content-Type": "text/plain"})
        r = c.getresponse()
        return r.status, r.read().decode("utf-8", "replace")
    finally:
        c.close()


def health(port):
    try:
        st, b = http_get(port, "/api/health")
        if st != 200:
            return None
        d = json.loads(b.decode("utf-8", "replace"))["data"]
        return int(d["frame"]), float(d["songMs"]), str(d["currentScreen"])
    except Exception:
        return None


def dta(port, expr):
    try:
        st, b = http_post(port, "/api/dta/eval", expr, timeout=10)
        if st != 200:
            return None
        d = json.loads(b)
        if not d.get("ok"):
            return None
        return d["data"].get("value")
    except Exception:
        return None


def screenshot(port, path):
    st, data = http_get(port, "/api/screenshot", timeout=20)
    if st != 200 or not data or data[:8] != b"\x89PNG\r\n\x1a\n":
        return False
    with open(path, "wb") as f:
        f.write(data)
    return True


def is_game_screen(screen):
    s = (screen or "").lower()
    return ("game_screen" in s) or (s == "game") or ("gameplay" in s)


def wait_for(port, predicate, timeout, label, verbose, proc):
    deadline = time.time() + timeout
    last = None
    while time.time() < deadline:
        if proc.poll() is not None:
            log(f"FAIL: process exited (code {proc.returncode}) while waiting for {label}")
            return None
        h = health(port)
        if h is not None:
            frame, song_ms, screen = h
            if verbose and (frame // 20, screen) != last:
                log(f"  ...{label}: frame={frame} songMs={song_ms:.0f} screen='{screen}'")
                last = (frame // 20, screen)
            if predicate(frame, song_ms, screen):
                return h
        time.sleep(0.3)
    return None


def advance_to_songms(port, target_ms, max_wait=20.0):
    """autohit + poll until songMs >= target_ms. A deterministic sim-clock gate
    (the engine's own beat clock), NOT a wall-clock sleep — matches
    band-closeup-capture.py's advance_to_songms()."""
    dl = time.time() + max_wait
    last = -1.0
    while time.time() < dl:
        h = health(port)
        if h and h[1] >= target_ms:
            return h[1]
        if h:
            last = h[1]
        http_post(port, "/api/input", "autohit")
        time.sleep(0.1)
    return last


def try_pin_shot(port, candidates, verbose):
    """Freeze the director, then try each candidate (bare, then `.shot`
    suffixed) in order. Returns (requested, resolved_or_None)."""
    disabled = dta(port, "{rb3_director_disable 1}")
    if verbose:
        log(f"director_disable(1) -> {disabled}")
    for name in candidates:
        for cand in (name, name if name.endswith(".shot") else name + ".shot"):
            r = dta(port, '{rb3_force_shot "%s"}' % cand)
            if r == "force_shot ok":
                return name, cand
            if isinstance(r, int):
                # Hook missing entirely (returns a bare int, not the status
                # string) -> no point trying further candidates.
                return name, None
    return candidates[0] if candidates else None, None


def capture(port, out_dir, label, manifest, shot_requested, shot_resolved, keep_going=True):
    h = health(port)
    frame, song_ms, screen = h if h else (-1, -1.0, "?")
    cur = dta(port, "{rb3_cur_shot}") if shot_resolved else None
    fname = f"handcloseup_{label}.png"
    path = os.path.join(out_dir, fname)
    ok = screenshot(port, path)
    entry = {
        "label": label,
        "file": fname,
        "frame": frame,
        "songMs": round(song_ms, 1),
        "screen": screen,
        "shot_requested": shot_requested,
        "shot_resolved": shot_resolved,
        "cur_shot": cur,
        "shot_pinned": bool(shot_resolved),
        "ok": ok,
    }
    manifest.append(entry)
    log(f"[{label}] frame={frame} songMs={song_ms:.0f} shot={shot_resolved or '(unpinned)'} "
        f"cur_shot={cur} -> {'OK' if ok else 'FAIL'} {path}")
    if not ok and not keep_going:
        raise RuntimeError(f"screenshot failed for label={label}")
    return entry


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--bin", default=DEFAULT_BIN)
    ap.add_argument("--data", default=DEFAULT_DATA)
    ap.add_argument("--out", default=DEFAULT_OUT)
    ap.add_argument("--shots", default="",
                    help="comma-separated shot candidates (overrides HAND_CLOSEUP_SHOTS)")
    ap.add_argument("--plays", type=int, default=3, help="number of steady-play frames")
    ap.add_argument("--play-dt", type=int, default=1500,
                    help="songMs advance between play frames")
    ap.add_argument("--port", type=int, default=0)
    ap.add_argument("--verbose", action="store_true")
    ap.add_argument("--keep-log", action="store_true")
    args = ap.parse_args()

    if not os.path.exists(args.bin):
        log(f"FAIL: binary not found: {args.bin}")
        log("      build it: cmake --build native/build-agent-W2.2 --target rb3-native")
        return 1
    os.makedirs(args.out, exist_ok=True)

    candidates = ([s.strip() for s in args.shots.split(",") if s.strip()]
                  if args.shots else HAND_CLOSEUP_SHOTS)

    port = args.port or free_port()
    log_path = os.path.join("/tmp", f"rb3-handcloseup-{port}.log")
    logf = open(log_path, "wb")
    env = dict(os.environ)
    env.update({
        "RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
        "MILO_HEADLESS": "1", "RB3_DATA": args.data, "RB3_GAME_INPUT": NAV_SCRIPT,
    })
    log(f"launching {os.path.basename(args.bin)} (port {port}), log -> {log_path}")
    if "RB3_WALKON_SNAP_OFF" in env:
        log(f"RB3_WALKON_SNAP_OFF={env['RB3_WALKON_SNAP_OFF']} (A/B toggle propagated)")
    proc = subprocess.Popen([args.bin], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)

    manifest = []
    rc = 1
    try:
        if wait_for(port, lambda f, m, s: True, SERVER_READY_TIMEOUT,
                    "server-ready", args.verbose, proc) is None:
            log("FAIL: HTTP server never came up")
            return 1
        log("HTTP server up")

        # Reach the game screen — count-in entry, songMs may still be ~0. This IS
        # the count-in/walk-on window we need to frame, so capture it immediately
        # (before touching the camera) then pin the shot for the rest of the run.
        h = wait_for(port, lambda f, m, s: is_game_screen(s), GAMEPLAY_TIMEOUT,
                     "game-screen", args.verbose, proc)
        if h is None:
            log("FAIL: never reached the game screen")
            return 1
        log(f"game screen reached: frame={h[0]} songMs={h[1]:.0f} screen='{h[2]}'")

        shot_requested, shot_resolved = try_pin_shot(port, candidates, args.verbose)
        if shot_resolved:
            log(f"camera pinned: '{shot_requested}' -> '{shot_resolved}'")
        else:
            log(f"WARN: no candidate shot resolved (tried {len(candidates)}); "
                f"falling back to the free-running auto-director for this run")

        # 1) count-in/walk-on window — the moment the W0.5 lineup gate does not
        #    frame. Capture right away (songMs still ~0-1 typically).
        capture(port, args.out, "countin", manifest, shot_requested, shot_resolved)

        # 2) still-in-count-in walk-on sample, deterministically clock-gated.
        advance_to_songms(port, WALKON_SONGMS)
        capture(port, args.out, "walkon", manifest, shot_requested, shot_resolved)

        # 3) steady-play frames past note-start.
        target = max(PLAY_START_SONGMS, WALKON_SONGMS + 500.0)
        for i in range(args.plays):
            advance_to_songms(port, target)
            capture(port, args.out, f"play_{i:02d}", manifest, shot_requested, shot_resolved)
            target += args.play_dt

        # A held pin is itself a determinism proof worth recording even on the
        # fallback path (cur_shot may simply be whatever the auto-director chose,
        # but it's still logged per-frame in the manifest above).
        if shot_resolved:
            still = dta(port, "{rb3_cur_shot}")
            held = (still == shot_resolved)
            log(f"pin-held check: cur_shot='{still}' expected='{shot_resolved}' -> "
                f"{'HELD' if held else 'LOST'}")

        manifest_path = os.path.join(args.out, "manifest.json")
        with open(manifest_path, "w") as f:
            json.dump({
                "shots_tried": candidates,
                "shot_requested": shot_requested,
                "shot_resolved": shot_resolved,
                "shot_pinned": bool(shot_resolved),
                "frames": manifest,
            }, f, indent=2)
        log(f"manifest -> {manifest_path}")

        all_ok = all(e["ok"] for e in manifest)
        rc = 0 if all_ok else 1
        if all_ok:
            log(f"PASS: captured {len(manifest)} frames in {args.out}")
        else:
            log("FAIL: one or more screenshots failed (see manifest)")
        return rc
    finally:
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
            try:
                proc.wait(timeout=8)
            except subprocess.TimeoutExpired:
                os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
        except Exception:
            pass
        logf.close()
        if rc != 0 or args.keep_log:
            log(f"engine log: {log_path}")
        elif os.path.exists(log_path):
            try:
                os.remove(log_path)
            except OSError:
                pass


if __name__ == "__main__":
    sys.exit(main())
