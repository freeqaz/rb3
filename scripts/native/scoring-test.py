#!/usr/bin/env python3
"""
scoring-test.py — prove the native/web RB3 scorer produces a REAL, non-zero score.

WHY THIS EXISTS
---------------
The other headless harnesses (song-end-test.py, keyboard-to-gameplay.py) reach the
end of a song with `{game jump <past-end>}`. That fast-forward is the engine's
rewind/replay primitive: `Game::Jump` (Game.cpp:629) -> `mBand->Restart(false)` ->
`Performer::Restart` (Performer.cpp:152) sets `mScore = 0`. So the band/player score
is INTENTIONALLY zeroed by the jump before `MetaPerformer::TriggerSongCompletion`
snapshots it into the coop_endgame results widget. That is why those harnesses show
`score=0 / num_stars=0` on the score-detail screen — a HARNESS ARTIFACT, not a
scoring bug.

This test instead plays a song to its NATURAL end (NO jump) with continuous
autohit, so the accumulated score survives into the endgame screen, and asserts:
  - the in-game band score climbs above zero during play, AND
  - `{{beatmatch main_performer} score}` and `num_stars` are non-zero on the
    coop_endgame score-detail screen (the exact bindings the widget reads:
    see ui/endgame/endgame_helpers.dta `score.scr set_values` / `stars.sd`).

Autohit is a genuine hit path: BeatMatcher::SetCheating(true) ->
TrackWatcherImpl::CheckForAutoplay -> HitGem -> GemPlayer::Hit -> AddHeadPoints ->
Performer::AddPoints (the SAME pipeline a real strum drives), so a non-zero autohit
score proves the scorer is wired correctly end to end.

Exit 0 = scorer produced a non-zero, valid endgame score (with stars). Exit 1 =
score stayed zero / song never ended / crash / boot failed.

USAGE
-----
    python3 scripts/native/scoring-test.py [--port N] [--bin PATH] [--data DIR]
            [--timeout SECS] [--shot PATH] [--verbose]
"""
import argparse, http.client, json, os, signal, socket, subprocess, sys, time

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DEFAULT_BIN = os.path.join(REPO, "native", "build-native", "rb3-native")
DEFAULT_DATA = os.path.join(REPO, "orig-assets", "extracted")

# Boot-to-gameplay nav (the V1 one-song sequence; nofail+autohit at the end).
# NOTE: deliberately NO jump — the song plays to its natural end.
NAV_SCRIPT = (
    "@10:start,@30:confirm,"
    "@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn,"
    "@320:down,@350:msg:music_library:select_highlighted_node,"
    "@380:part:guitar,@400:diff:expert,"
    "@500:nofail,@520:autohit"
)


def log(m): print(f"[scoring-test] {m}", flush=True)


def free_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.bind(("", 0)); p = s.getsockname()[1]; s.close(); return p


def req(port, method, path, body=None, raw=False, timeout=20):
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=timeout)
    try:
        c.request(method, path, body=body,
                  headers={"Content-Type": "text/plain"} if body else {})
        r = c.getresponse(); d = r.read()
        return r.status, (d if raw else d.decode("utf-8", "replace"))
    except Exception as e:
        return None, (b"" if raw else str(e))
    finally:
        c.close()


def health(port):
    s, d = req(port, "GET", "/api/health")
    if s != 200: return None
    try: return json.loads(d).get("data", {})
    except Exception: return None


def evl(port, expr):
    s, d = req(port, "POST", "/api/dta/eval", expr)
    if s != 200: return None
    try: return json.loads(d).get("data", {}).get("value")
    except Exception: return None


def verb(port, v): return req(port, "POST", "/api/input", v)


def shot(port, path):
    s, d = req(port, "GET", "/api/screenshot", raw=True)
    if s == 200 and isinstance(d, (bytes, bytearray)) and len(d) > 100:
        with open(path, "wb") as f: f.write(d)
        return True
    return False


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=0)
    ap.add_argument("--bin", default=DEFAULT_BIN)
    ap.add_argument("--data", default=DEFAULT_DATA)
    ap.add_argument("--timeout", type=float, default=300.0,
                    help="max seconds to play before giving up on natural EOF")
    ap.add_argument("--shot", default=None, help="save endgame screenshot here")
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    if not os.path.exists(args.bin):
        log(f"FAIL: binary not found: {args.bin}"); return 1
    if not os.path.isdir(args.data):
        log(f"FAIL: asset dir not found: {args.data}"); return 1

    port = args.port or free_port()
    env = dict(os.environ)
    env.update({"RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
                "MILO_HEADLESS": "1", "RB3_DATA": args.data,
                "RB3_GAME_INPUT": NAV_SCRIPT})
    log_path = os.path.join("/tmp", f"rb3-scoring-{port}.log")
    logf = open(log_path, "w")
    log(f"launching rb3-native (port {port}, headless), log -> {log_path}")
    proc = subprocess.Popen([args.bin], env=env, stdout=logf,
                            stderr=subprocess.STDOUT, cwd=REPO,
                            start_new_session=True)
    try:
        # server up
        dl = time.time() + 45
        while time.time() < dl:
            if health(port): break
            if proc.poll() is not None: log("FAIL: died during boot"); return 1
            time.sleep(0.5)
        else:
            log("FAIL: server never came up"); return 1

        # reach game_screen (+30s headroom for the real part_difficulty screen
        # nav vs the old direct-commit track:/difficulty: skip)
        dl = time.time() + 350; scr = None
        while time.time() < dl:
            if proc.poll() is not None: log("FAIL: died -> game_screen"); return 1
            h = health(port); scr = h.get("currentScreen") if h else None
            if scr == "game_screen": break
            time.sleep(1.0)
        if scr != "game_screen":
            log(f"FAIL: never reached game_screen (last={scr})"); return 1
        song = evl(port, "{meta_performer song}")
        log(f"game_screen reached; song={song}")

        # play to natural end with continuous autohit; track peak in-game score
        t0 = time.time(); started = False; peak = 0
        while time.time() - t0 < args.timeout:
            if proc.poll() is not None: log("FAIL: died during play"); return 1
            verb(port, "autohit")
            h = health(port)
            songms = h.get("songMs", -1) if h else -1
            scr = h.get("currentScreen") if h else None
            if evl(port, "{game is_playing}") == 1 and songms > 2000:
                started = True
            bs = evl(port, "{{beatmatch main_performer} score}")
            if isinstance(bs, (int, float)) and bs > peak: peak = bs
            if started and scr != "game_screen":
                log(f"song ended naturally at songMs={songms:.0f}; peak in-game band score={peak}")
                break
            if args.verbose and int(time.time() - t0) % 15 == 0:
                log(f"  t={time.time()-t0:5.0f}s songMs={songms:8.0f} band_score={bs}")
            time.sleep(0.5)
        else:
            log(f"FAIL: song did not end within {args.timeout}s (peak band score={peak})")
            return 1

        # wait for the coop_endgame score-detail screen
        dl = time.time() + 90; endscr = scr
        while time.time() < dl:
            if proc.poll() is not None: log("FAIL: died -> endgame"); break
            h = health(port); endscr = h.get("currentScreen") if h else None
            if endscr == "coop_endgame_screen": break
            time.sleep(1.0)
        log(f"endgame screen = {endscr}")
        time.sleep(3.0)

        invalid = evl(port, "{beatmatch is_invalid_score}")
        band_score = evl(port, "{{beatmatch main_performer} score}")
        band_stars = evl(port, "{{beatmatch main_performer} num_stars}")
        if args.shot:
            shot(port, args.shot)
            log(f"endgame screenshot -> {args.shot}")

        log(f"RESULT: is_invalid_score={invalid} peak_in_game_score={peak} "
            f"endgame_band_score={band_score} endgame_band_stars={band_stars}")

        ok = (isinstance(peak, (int, float)) and peak > 0 and
              isinstance(band_score, (int, float)) and band_score > 0 and
              isinstance(band_stars, (int, float)) and band_stars > 0 and
              invalid == 0 and endscr == "coop_endgame_screen")
        if ok:
            log(f"PASS: native scorer produced a real, non-zero endgame score "
                f"({band_score} pts, {band_stars} stars).")
            return 0
        log("FAIL: endgame score/stars not non-zero (see RESULT above).")
        return 1
    finally:
        try:
            proc.send_signal(signal.SIGTERM); proc.wait(timeout=8)
        except Exception:
            proc.kill()
        logf.close()


if __name__ == "__main__":
    sys.exit(main())
