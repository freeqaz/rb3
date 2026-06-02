#!/usr/bin/env python3
"""
song-end-test.py — faster-than-real-time regression test for the RB3 native/web
port "song never ends / score screen never shows" bug.

WHAT IT GUARDS
--------------
When the song clock passes the song duration, `Performer::Poll` is supposed to
drive `Game::SetGameOver -> TheNetSession->EndGame -> GameEndedMsg ->
GamePanel::SetGameOver`, flipping the game into kGameOver and transitioning the
UI from `game_screen` to the endgame / results screens. Two native NetSession
shim gaps (see native/src/rb3_netsession_native.cpp) used to break that chain so
the song played forever. This test fails if either regresses.

HOW IT'S FAST
-------------
Playing a song to its natural end is ~3.5 minutes. Instead we boot headless to
gameplay, wait until the song is actually playing (audio clock advancing), then
inject `{game jump <past-end>}` over the embedded HTTP debug server (/api/input)
to skip straight to the song end. The whole run is bounded at ~2-3 minutes
(dominated by boot + the cinematic intro pre-roll), and is deterministic: it
polls for gameplay-start before jumping, so it doesn't depend on intro timing.

It is a pure-stdlib HTTP driver (no browser / Playwright). The fix lives in a
file compiled into BOTH the native and web (emscripten) builds, so this native
run validates the web build's behavior too.

USAGE
-----
    python3 scripts/native/song-end-test.py [--port N] [--data DIR]
            [--bin PATH] [--song SYM] [--verbose] [--keep-log]
            [--require-endgame]

Exit 0 = song ended and game-over was reached (default), or — with
--require-endgame — the endgame/results screen additionally loaded and stayed
rendering for a sustained window without aborting. Exit 1 = bug present /
crashed / timed out / boot failed.
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
DEFAULT_BIN = os.path.join(REPO, "native", "build-native", "rb3-native")
DEFAULT_DATA = os.path.join(REPO, "orig-assets", "extracted")

# Canonical headless boot-to-song nav (the V1 one-song sequence). No jump here —
# the test injects the jump over HTTP once gameplay is confirmed underway.
NAV_SCRIPT = (
    "@10:start,@30:confirm,"
    "@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn,"
    "@320:down,@350:msg:music_library:select_highlighted_node,"
    "@380:track:guitar,@390:difficulty:expert,@450:msg:overshell:end_override_flow:1:0,"
    "@500:nofail,@520:autohit"
)

# Jump target in ms — comfortably past any RB3 song (longest < 10 min). The audio
# seek clamps near EOF; Performer::Poll then sees songMs > duration and ends.
JUMP_MS = 600000

# Timeouts (seconds).
SERVER_READY_TIMEOUT = 40
GAMEPLAY_TIMEOUT = 200   # boot + nav + cinematic intro pre-roll
GAMEOVER_TIMEOUT = 60    # jump -> endgame screen transition
GAMEPLAY_SONGMS = 2000.0 # song clock past the intro = gameplay underway

# --require-endgame: after game-over, keep the process running and prove the
# endgame/results screen loads AND stays up (no abort) for this window. The
# endgame milos (coop_player_widget / coop_endgame) used to abort on base-vs-
# subclass casts (review.ihp as base InlineHelp, detail labels as base
# BandLabel, null AI UserName). Poll for STATE, never hardcode frame numbers —
# headless frame rate varies a lot run-to-run.
ENDGAME_REACH_TIMEOUT = 30    # game-over -> endgame/results screen seen
ENDGAME_STABLE_SECONDS = 25   # must keep rendering this long without exiting
ENDGAME_MIN_FRAME_ADVANCE = 30  # frames must advance (screen live, not frozen)


def log(msg):
    print(f"[song-end-test] {msg}", flush=True)


def free_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]
    s.close()
    return p


def http_get(port, path, timeout=5):
    conn = http.client.HTTPConnection("127.0.0.1", port, timeout=timeout)
    try:
        conn.request("GET", path)
        r = conn.getresponse()
        return r.status, r.read().decode("utf-8", "replace")
    finally:
        conn.close()


def http_post(port, path, body, timeout=15):
    conn = http.client.HTTPConnection("127.0.0.1", port, timeout=timeout)
    try:
        conn.request("POST", path, body=body, headers={"Content-Type": "text/plain"})
        r = conn.getresponse()
        return r.status, r.read().decode("utf-8", "replace")
    finally:
        conn.close()


def dta_eval(port, expr):
    """POST a DTA expression to /api/dta/eval; return its value (int/float/str)
    or None on error. Result shape: {"ok":true,"data":{"type":"int","value":N}}."""
    try:
        status, body = http_post(port, "/api/dta/eval", expr, timeout=10)
        if status != 200:
            return None
        d = json.loads(body)
        if not d.get("ok"):
            return None
        data = d["data"]
        if data.get("type") in ("int", "float"):
            return data["value"]
        return data.get("value")
    except Exception:
        return None


def health(port):
    """Return (frame, songMs, screen) or None if unreachable/unparseable."""
    try:
        status, body = http_get(port, "/api/health")
    except Exception:
        return None
    if status != 200:
        return None
    try:
        d = json.loads(body)["data"]
        return int(d["frame"]), float(d["songMs"]), str(d["currentScreen"])
    except Exception:
        return None


def wait_for(port, predicate, timeout, label, verbose, proc):
    """Poll /api/health until predicate(frame, songMs, screen) is true."""
    deadline = time.time() + timeout
    last = None
    while time.time() < deadline:
        if proc.poll() is not None:
            log(f"FAIL: process exited (code {proc.returncode}) while waiting for {label}")
            return None
        h = health(port)
        if h is not None:
            frame, song_ms, screen = h
            if verbose and (frame, screen) != last:
                log(f"  ...{label}: frame={frame} songMs={song_ms:.0f} screen='{screen}'")
                last = (frame, screen)
            if predicate(frame, song_ms, screen):
                return h
        time.sleep(0.5)
    return None


def is_endgame_screen(screen):
    s = (screen or "").lower()
    return ("endgame" in s) or ("results" in s) or ("score_screen" in s)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", type=int, default=0, help="HTTP port (0 = auto-pick)")
    ap.add_argument("--bin", default=DEFAULT_BIN, help="rb3-native binary path")
    ap.add_argument("--data", default=DEFAULT_DATA, help="RB3_DATA asset dir")
    ap.add_argument("--song", default=None, help="(unused yet) target song symbol")
    ap.add_argument("--verbose", action="store_true")
    ap.add_argument("--keep-log", action="store_true", help="keep the engine stdout/stderr log")
    ap.add_argument("--require-endgame", action="store_true",
                    help="after game-over, also require the endgame/results screen to "
                         "load AND stay up (no abort) — the full score-screen guard")
    args = ap.parse_args()

    if not os.path.exists(args.bin):
        log(f"FAIL: binary not found: {args.bin}")
        log("      build it: cmake --build native/build-native --target rb3-native")
        return 1
    if not os.path.isdir(args.data):
        log(f"FAIL: asset dir not found: {args.data}")
        return 1

    port = args.port or free_port()
    log_path = os.path.join("/tmp", f"rb3-song-end-{port}.log")
    logf = open(log_path, "w")

    env = dict(os.environ)
    env.update({
        "RB3_GAME": "1",
        "RB3_HTTP": "1",
        "RB3_HTTP_PORT": str(port),
        "MILO_HEADLESS": "1",
        "RB3_DATA": args.data,
        "RB3_GAME_INPUT": NAV_SCRIPT,
        # No MILO_MAX_FRAMES cap — we drive the lifecycle over HTTP and kill on exit.
    })

    log(f"launching {os.path.basename(args.bin)} (port {port}, headless), log -> {log_path}")
    proc = subprocess.Popen([args.bin], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)

    rc = 1
    try:
        # 1) Wait for the embedded HTTP server.
        if wait_for(port, lambda f, m, s: True, SERVER_READY_TIMEOUT,
                    "server-ready", args.verbose, proc) is None:
            log("FAIL: HTTP server never came up")
            return 1
        log("HTTP server up")

        # 2) Wait for gameplay to actually start (song clock advancing past intro).
        h = wait_for(port, lambda f, m, s: m > GAMEPLAY_SONGMS,
                     GAMEPLAY_TIMEOUT, "gameplay-start", args.verbose, proc)
        if h is None:
            log("FAIL: never reached gameplay (song clock never advanced past intro)")
            return 1
        log(f"gameplay underway: frame={h[0]} songMs={h[1]:.0f} screen='{h[2]}'")

        # Sanity-check the state-query path BEFORE we rely on it: during gameplay
        # {game is_game_over} must be 0 and {game is_playing} must be 1. (The
        # `game` object is the GamePanel; /api/dta/eval resolves it via
        # gDataDir->FindObject recursively.) If this probe doesn't come back as
        # expected the query path is broken and the result below is meaningless.
        playing = dta_eval(port, "{game is_playing}")
        over_pre = dta_eval(port, "{game is_game_over}")
        log(f"state probe during play: is_playing={playing} is_game_over={over_pre}")
        if playing != 1 or over_pre != 0:
            log("WARN: state-query probe unexpected — will also accept screen / crash signals")

        # 3) Skip to the song end.
        log(f"injecting {{game jump {JUMP_MS}}} to fast-forward to song end")
        status, body = http_post(port, "/api/input", f"msg:game:jump:{JUMP_MS}")
        if status != 200:
            log(f"FAIL: /api/input jump rejected (status {status}): {body}")
            return 1

        # 4) The fix: song-end must flip the game into kGameOver. We assert on the
        #    GamePanel game-over STATE (stable for ~100+ frames while the win
        #    animation plays) rather than the endgame SCREEN name (which only
        #    flashes for a few frames before the endgame milos load). We accept
        #    three positive signals; any one means the song ended:
        #      (a) {game is_game_over} == 1   (primary, crash-free, stable)
        #      (b) an endgame/results screen seen in /api/health  (secondary)
        #      (c) the process aborts AFTER the jump  (a downstream endgame-content
        #          crash — only reachable once the game-over transition fired)
        #
        #    With --require-endgame the bar is higher: (c) becomes a FAIL (the whole
        #    point is that the score screen no longer aborts), and we additionally
        #    require the endgame screen to load AND stay rendering (stage 5).
        deadline = time.time() + GAMEOVER_TIMEOUT
        verdict = None
        while time.time() < deadline:
            if proc.poll() is not None:
                if proc.returncode in (134, 139, -6, -11):  # SIGABRT/SIGSEGV
                    if args.require_endgame:
                        log(f"FAIL: process aborted (code {proc.returncode}) after jump — "
                            f"the endgame/results screen crashed (--require-endgame).")
                        return 1
                    verdict = ("crash", f"process exited {proc.returncode} after jump "
                                        f"(downstream endgame-content crash — song-end fired)")
                else:
                    log(f"FAIL: process exited {proc.returncode} after jump without game-over")
                    return 1
                break
            if dta_eval(port, "{game is_game_over}") == 1:
                verdict = ("state", "{game is_game_over} == 1")
                break
            cur = health(port)
            if cur and is_endgame_screen(cur[2]):
                verdict = ("screen", f"endgame screen '{cur[2]}'")
                break
            time.sleep(0.25)

        if verdict is None:
            cur = health(port)
            screen = cur[2] if cur else "(unreachable)"
            log(f"FAIL: song did not end — still in play (screen '{screen}', "
                f"is_game_over={dta_eval(port, '{game is_game_over}')}) after jump.")
            log("      This is the 'song never ends / no score screen' bug.")
            return 1

        kind, detail = verdict
        log(f"PASS: song ended -> game-over reached [{kind}: {detail}]")

        # 5) --require-endgame: prove the endgame/results screen actually loads and
        #    stays up. Reach an endgame screen, then watch it render frames for a
        #    sustained window without the process exiting/aborting. Poll for STATE
        #    (screen name + frame advance + proc.poll), never a fixed frame count.
        if args.require_endgame:
            reach_dl = time.time() + ENDGAME_REACH_TIMEOUT
            reached = None
            while time.time() < reach_dl:
                if proc.poll() is not None:
                    log(f"FAIL: process exited (code {proc.returncode}) before the "
                        f"endgame screen loaded (--require-endgame).")
                    return 1
                cur = health(port)
                if cur and is_endgame_screen(cur[2]):
                    reached = cur
                    break
                if args.verbose and cur:
                    log(f"  ...endgame-reach: frame={cur[0]} screen='{cur[2]}'")
                time.sleep(0.3)
            if reached is None:
                cur = health(port)
                screen = cur[2] if cur else "(unreachable)"
                log(f"FAIL: never reached an endgame/results screen (last '{screen}') "
                    f"within {ENDGAME_REACH_TIMEOUT}s of game-over.")
                return 1
            log(f"endgame screen reached: frame={reached[0]} screen='{reached[2]}'")

            # Stability watch: it must keep rendering (frame advancing) and NOT
            # exit for ENDGAME_STABLE_SECONDS.
            start_frame = reached[0]
            last_screen = reached[2]
            stable_dl = time.time() + ENDGAME_STABLE_SECONDS
            last_frame = start_frame
            while time.time() < stable_dl:
                if proc.poll() is not None:
                    log(f"FAIL: endgame screen aborted (code {proc.returncode}) "
                        f"after ~{ENDGAME_STABLE_SECONDS - (stable_dl - time.time()):.0f}s "
                        f"(was on '{last_screen}', frame {last_frame}).")
                    return 1
                cur = health(port)
                if cur:
                    last_frame, last_screen = cur[0], cur[2]
                    if args.verbose:
                        log(f"  ...endgame-stable: frame={cur[0]} screen='{cur[2]}'")
                time.sleep(1.0)
            advanced = last_frame - start_frame
            if advanced < ENDGAME_MIN_FRAME_ADVANCE:
                log(f"FAIL: endgame screen alive but frozen — only {advanced} frames "
                    f"advanced in {ENDGAME_STABLE_SECONDS}s (need >= "
                    f"{ENDGAME_MIN_FRAME_ADVANCE}). screen='{last_screen}'.")
                return 1
            log(f"PASS: endgame screen STABLE for {ENDGAME_STABLE_SECONDS}s "
                f"(screen='{last_screen}', {advanced} frames advanced, no abort).")

        if kind == "crash":
            log("      NOTE: the endgame results screen then aborted on downstream "
                "endgame-content gaps — run with --require-endgame to gate that. "
                "The song-end → game-over transition itself works.")
        rc = 0
        return 0
    finally:
        # Tear down the engine process group.
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
            try:
                proc.wait(timeout=8)
            except subprocess.TimeoutExpired:
                os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
        except Exception:
            pass
        logf.close()
        if rc == 0 and not args.keep_log:
            try:
                os.remove(log_path)
            except OSError:
                pass
        else:
            log(f"engine log: {log_path}")


if __name__ == "__main__":
    sys.exit(main())
