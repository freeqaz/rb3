#!/usr/bin/env python3
"""song-restart-test.py — regression test for the mid-song restart hang.

Boots headless to gameplay (same nav as song-end-test.py), confirms the song
clock is advancing, then evals {session end_game 0} over the HTTP debug server —
byte-for-byte what the pause menu's Restart option runs ({session end_game
kRestart}, overshell slot_states.dta). That fires NetSession::EndGame ->
GameEndedMsg(kRestart) -> Game::OnMsg executes {game_restart} -> the
restart_sync_audio_net_screen resync flow -> song restarts from 0.

PASS = the song clock resets below 1.2s and advances past 1.5s again, for each
of --restarts consecutive restarts. FAIL = crash (SIGSEGV/SIGABRT), main-thread
hang (frame counter frozen), or the restart never completing (stuck on the sync
screen).

Guards two native bugs (fixed together, 2026-07-02):
 1. GamePanel::ClearDrawGlitch's Wii frame-flush loop read TheWiiRnd (a weak
    no-op stub) .mFramesBuffered = relocated-pointer garbage -> the main thread
    wedged in nested draws inside the {game_restart} eval. Every restart path
    runs {game clear_draw_glitch} (game_restart, endgame play-again,
    complete_panel exit), so this froze native AND the web tab.
 2. RB3NativeNetSession::Handle treated {session end_game ...} as an offline
    no-op, so the session never dropped back to kInLobby and the sync panel's
    NetSession::StartGame early-returned -> SyncStartGameMsg never fired ->
    restart stalled forever on restart_sync_audio_net_screen (panel stuck at
    kStartingSession).
"""

import argparse, http.client, json, os, signal, socket, subprocess, sys, time

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

def log(m):
    print(f"[restart-probe] {m}", flush=True)

def free_port():
    s = socket.socket(); s.bind(("127.0.0.1", 0)); p = s.getsockname()[1]; s.close(); return p

def http_req(port, method, path, body=None, timeout=10):
    conn = http.client.HTTPConnection("127.0.0.1", port, timeout=timeout)
    try:
        conn.request(method, path, body=body,
                     headers={"Content-Type": "text/plain"} if body else {})
        r = conn.getresponse()
        return r.status, r.read().decode("utf-8", "replace")
    finally:
        conn.close()

def health(port):
    try:
        st, body = http_req(port, "GET", "/api/health", timeout=5)
        if st != 200: return None
        d = json.loads(body)["data"]
        return int(d["frame"]), float(d["songMs"]), str(d["currentScreen"])
    except Exception:
        return None

def dta(port, expr, timeout=20):
    try:
        st, body = http_req(port, "POST", "/api/dta/eval", expr, timeout=timeout)
        return st, body
    except Exception as e:
        return None, str(e)

def wait(port, proc, pred, timeout, label, verbose=True):
    dl = time.time() + timeout
    last = None
    while time.time() < dl:
        if proc.poll() is not None:
            log(f"PROC EXIT code={proc.returncode} while waiting for {label}")
            return ("exit", proc.returncode)
        h = health(port)
        if h:
            if verbose and (h[0] // 100, h[2]) != last:
                log(f"  ...{label}: frame={h[0]} songMs={h[1]:.0f} screen='{h[2]}'")
                last = (h[0] // 100, h[2])
            if pred(*h):
                return ("ok", h)
        time.sleep(0.4)
    return ("timeout", health(port))

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", default=DEFAULT_BIN)
    ap.add_argument("--data", default=DEFAULT_DATA)
    ap.add_argument("--restarts", type=int, default=2)
    ap.add_argument("--settle-ms", type=float, default=4000.0,
                    help="songMs to reach before triggering (each) restart")
    ap.add_argument("--env", action="append", default=[],
                    help="extra KEY=VAL env for the engine (repeatable)")
    ap.add_argument("--hold-on-fail", type=float, default=0.0,
                    help="on FAIL keep the engine alive this many seconds "
                         "(prints pid) so a debugger can attach")
    args = ap.parse_args()

    port = free_port()
    log_path = f"/tmp/rb3-restart-probe-{port}.log"
    logf = open(log_path, "w")
    env = dict(os.environ)
    env.update({
        "RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
        "MILO_HEADLESS": "1", "RB3_DATA": args.data,
        "RB3_GAME_INPUT": NAV_SCRIPT,
    })
    for kv in args.env:
        k, _, v = kv.partition("=")
        env[k] = v
    log(f"launching (port {port}), log -> {log_path}")
    proc = subprocess.Popen([args.bin], env=env, stdout=logf,
                            stderr=subprocess.STDOUT, cwd=REPO, start_new_session=True)
    rc = 1
    try:
        k, _ = wait(port, proc, lambda f, m, s: True, 40, "server-ready", verbose=False)
        if k != "ok":
            log("FAIL: server never came up"); return 1
        log("HTTP up; waiting for gameplay")
        # +30s headroom vs the old direct-commit nav: part_difficulty is now a
        # real readiness-gated screen (part:/diff: pad-press verbs), not a
        # single-frame track:/difficulty: skip.
        k, h = wait(port, proc, lambda f, m, s: m > 2000.0, 250, "gameplay-start")
        if k != "ok":
            log(f"FAIL: no gameplay ({k})"); return 1
        log(f"gameplay underway: songMs={h[1]:.0f}")

        for i in range(1, args.restarts + 1):
            # let the song run a bit past the trigger threshold
            k, h = wait(port, proc, lambda f, m, s: m > args.settle_ms, 120,
                        f"pre-restart-{i} settle")
            if k != "ok":
                log(f"FAIL: song clock never reached {args.settle_ms}ms before restart {i} ({k})")
                return 1
            # Mirror the pause-menu restart exactly: {session end_game kRestart}
            # (kRestart == 0, config/macros.dta). NetSession::EndGame drops the
            # session to kInLobby and fires GameEndedMsg(kRestart), whose Game
            # handler executes {game_restart} itself — same cascade as console.
            log(f"restart #{i}: triggering {{session end_game 0}} at songMs={h[1]:.0f}")
            st, body = dta(port, "{session end_game 0}")
            log(f"  end_game eval -> {st}: {body[:160]}")
            if proc.poll() is not None:
                log(f"CRASH: process exited code={proc.returncode} right after restart eval")
                return 1
            # Expect: clock resets (goes below previous) then advances past 1500ms again.
            saw_reset = [False]
            def restarted(f, m, s):
                if m < 1200.0:
                    saw_reset[0] = True
                return saw_reset[0] and m > 1500.0
            k, h = wait(port, proc, restarted, 90, f"post-restart-{i} clock")
            if k == "exit":
                log(f"CRASH on restart #{i}: exit code {proc.returncode}")
                return 1
            if k != "ok":
                cur = health(port)
                log(f"FAIL: restart #{i} did not come back "
                    f"(saw_reset={saw_reset[0]}, now={cur})")
                return 1
            log(f"restart #{i} OK: songMs={h[1]:.0f} screen='{h[2]}'")

        log("PASS: all restarts survived, clock advancing")
        rc = 0
        return 0
    finally:
        if rc != 0 and args.hold_on_fail > 0 and proc.poll() is None:
            log(f"HOLD: engine pid={proc.pid} kept alive {args.hold_on_fail:.0f}s for debugger attach")
            time.sleep(args.hold_on_fail)
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
            try: proc.wait(timeout=8)
            except subprocess.TimeoutExpired:
                os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
        except Exception:
            pass
        logf.close()
        log(f"engine log: {log_path}")

if __name__ == "__main__":
    sys.exit(main())
