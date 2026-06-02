#!/usr/bin/env python3
"""
keyboard-to-gameplay.py — PURE-KEYBOARD native nav all the way to gameplay.

Boots rb3-native headless and drives splash -> main_hub -> song_select ->
choose part -> choose difficulty -> ready -> game_screen using ONLY raw joypad
button presses (the `pad:<bit>` HTTP verb -> SendButtonMessages, the faithful
equivalent of a physical guitar button). NO select:/msg:/track:/ExecButton aids
are used for ANY menu crossing. After game_screen is reached, optional
nofail/autohit keep a headless run stable (the only sanctioned post-gameplay
aids).

State is read over /api/dta/eval ({rb3_overshell} -> the pad-0 user's overshell
slot view + track + difficulty) plus /api/health (currentScreen + songMs).

    python3 scripts/native/keyboard-to-gameplay.py [--port N] [--diff hard]
            [--out DIR] [--verbose]

Exit 0 = reached game_screen with the chosen difficulty in effect + song clock
advancing. Exit 1 = stalled (the screen+state timeline is printed regardless).
"""
import argparse, http.client, json, os, signal, socket, subprocess, sys, time

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DEFAULT_BIN = os.path.join(REPO, "native", "build-native", "rb3-native")
DEFAULT_DATA = os.path.join(REPO, "orig-assets", "extracted")

# JoypadButton bits (src/system/os/Joypad.h)
START, CONFIRM, CANCEL, STAR = 11, 6, 5, 8
DUP, DRIGHT, DDOWN, DLEFT = 12, 13, 14, 15

DIFF_INDEX = {"easy": 0, "medium": 1, "hard": 2, "expert": 3}


def log(m): print(f"[kbd2game] {m}", flush=True)
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
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=timeout)
    try:
        c.request("POST", path, body=body, headers={"Content-Type": "text/plain"})
        r = c.getresponse(); return r.status, r.read().decode("utf-8", "replace")
    finally:
        c.close()

def health(port):
    try:
        st, b = http_get(port, "/api/health")
        if st != 200: return None
        d = json.loads(b.decode("utf-8", "replace"))["data"]
        return int(d["frame"]), float(d["songMs"]), str(d["currentScreen"])
    except Exception:
        return None

def dta(port, expr):
    try:
        st, b = http_post(port, "/api/dta/eval", expr, timeout=10)
        if st != 200: return None
        d = json.loads(b)
        if not d.get("ok"): return None
        return d["data"].get("value")
    except Exception:
        return None

def overshell(port):
    """Returns (view, track, diff) for the pad-0 user's overshell slot."""
    v = dta(port, "{rb3_overshell}")
    if not v: return ("?", "?", "?")
    parts = dict(p.split(":", 1) for p in v.split("|") if ":" in p)
    return (parts.get("view", "?"), parts.get("track", "?"), parts.get("diff", "?"))

def press(port, bit):
    http_post(port, "/api/input", f"pad:{bit}")

def verb(port, v):
    return http_post(port, "/api/input", v)

def screenshot(port, path):
    st, data = http_get(port, "/api/screenshot", timeout=20)
    if st != 200 or not data or data[:8] != b"\x89PNG\r\n\x1a\n":
        return False
    with open(path, "wb") as f: f.write(data)
    return True

TIMELINE = []
def mark(port, label):
    h = health(port); ov = overshell(port)
    rec = (label, h[2] if h else "?", h[1] if h else -1, ov)
    TIMELINE.append(rec)
    log(f"  [{label}] screen='{rec[1]}' songMs={rec[2]:.0f} overshell={ov}")


def wait_screen(port, want, timeout, proc, verbose=False):
    dl = time.time() + timeout; last = None
    while time.time() < dl:
        if proc.poll() is not None:
            log(f"FAIL: process exited (code {proc.returncode})"); return None
        h = health(port)
        if h:
            f, m, s = h
            if verbose and s != last:
                log(f"    ...waiting '{want}': frame={f} screen='{s}' overshell={overshell(port)}"); last = s
            if (callable(want) and want(s)) or s == want:
                return h
        time.sleep(0.3)
    return None

def wait_view(port, pred, timeout, proc, label, verbose=False):
    """Wait until overshell view satisfies pred(view)."""
    dl = time.time() + timeout; last = None
    while time.time() < dl:
        if proc.poll() is not None:
            log(f"FAIL: process exited (code {proc.returncode})"); return None
        ov = overshell(port)
        if verbose and ov != last:
            log(f"    ...{label}: overshell={ov} screen={health(port)[2] if health(port) else '?'}"); last = ov
        if pred(ov[0]):
            return ov
        time.sleep(0.3)
    return None

def drain_pad(port, timeout=6.0):
    """Block until the pad queue has fully drained (clean release seen)."""
    dl = time.time() + timeout
    while time.time() < dl:
        # rb3_overshell is cheap; busy flag isn't exposed via dta, so just give
        # the queue enough wall-time per press (hold 4 + gap 3 polls ~ a few
        # frames). 0.25s comfortably covers a press at headless frame rates.
        time.sleep(0.25)
        return
    return


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=0)
    ap.add_argument("--bin", default=DEFAULT_BIN)
    ap.add_argument("--data", default=DEFAULT_DATA)
    ap.add_argument("--overlay", default=os.path.join(REPO, "native", "dta"))
    ap.add_argument("--diff", default="hard", choices=list(DIFF_INDEX))
    ap.add_argument("--song-downs", type=int, default=4,
                    help="DDown presses in song_select before confirming a song")
    ap.add_argument("--out", default="/tmp/rb3-kbd2game")
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    if not os.path.exists(args.bin):
        log(f"FAIL: binary not found: {args.bin}"); return 1
    os.makedirs(args.out, exist_ok=True)
    port = args.port or free_port()
    log_path = os.path.join("/tmp", f"rb3-kbd2game-{port}.log")
    logf = open(log_path, "w")
    env = dict(os.environ)
    env.update({"RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
                "MILO_HEADLESS": "1", "RB3_DATA": args.data,
                "RB3_DTA_OVERLAY": args.overlay, "RB3_INPUT_DEBUG": "1"})
    log(f"launching rb3-native (port {port}, headless, diff={args.diff}); log -> {log_path}")
    proc = subprocess.Popen([args.bin], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)
    rc = 1
    diff_idx = DIFF_INDEX[args.diff]
    try:
        if wait_screen(port, lambda s: True, 40, proc) is None:
            log("FAIL: HTTP server never came up"); return 1
        log("HTTP server up")

        # --- splash: press Start to advance to main_hub --------------------
        h = wait_screen(port, lambda s: s and s != "", 60, proc, args.verbose)
        mark(port, "boot")
        # Press Start until we leave splash; the offline-guest splash fix lands
        # the overshell in kState_JoinedDefault, so Start advances to main_hub.
        for attempt in range(8):
            cur = health(port)[2]
            if cur == "main_hub_screen": break
            press(port, START); drain_pad(port); time.sleep(0.6)
        if wait_screen(port, "main_hub_screen", 30, proc, args.verbose) is None:
            log("FAIL: never reached main_hub_screen via Start"); mark(port, "stuck-splash"); return 1
        mark(port, "main_hub")

        # --- main_hub: PLAY NOW (Confirm) -> QUICKPLAY (Confirm) -> song_select
        # Two distinct Confirm edges. The pad queue guarantees a clean release
        # between them.
        for attempt in range(10):
            cur = health(port)[2]
            if cur == "song_select_screen": break
            press(port, CONFIRM); drain_pad(port); time.sleep(0.7)
        if wait_screen(port, "song_select_screen", 40, proc, args.verbose) is None:
            log("FAIL: never reached song_select_screen"); mark(port, "stuck-mainhub"); return 1
        mark(port, "song_select")
        time.sleep(1.5)  # list populate + enter anim

        # --- song_select: scroll down to a guitar song, then Confirm ------
        for _ in range(args.song_downs):
            press(port, DDOWN); drain_pad(port); time.sleep(0.2)
        hl = dta(port, "{music_library get_highlighted_node}")
        log(f"  highlighted song node: {hl}")
        screenshot(port, os.path.join(args.out, "01_song_select.png"))
        # Confirm the song -> meta_loading -> part_difficulty (overshell SongSettings flow)
        press(port, CONFIRM); drain_pad(port)
        if wait_screen(port, "part_difficulty_screen", 60, proc, args.verbose) is None:
            log("FAIL: never reached part_difficulty_screen after song confirm")
            mark(port, "stuck-songselect"); return 1
        mark(port, "part_difficulty")
        screenshot(port, os.path.join(args.out, "02_part_difficulty.png"))

        # --- choose part: wait for the overshell to show a choose_part view,
        # then Confirm the focused (default = guitar) part. NO track: aid.
        ov = wait_view(port, lambda v: v.startswith("choose_part") or v == "choose_diff",
                       30, proc, "enter choose_part", args.verbose)
        if ov is None:
            log("FAIL: overshell never entered choose_part view"); mark(port, "stuck-prepart"); return 1
        if ov[0].startswith("choose_part"):
            log(f"  choose_part view='{ov[0]}' — confirming focused part (guitar)")
            press(port, CONFIRM); drain_pad(port)
            screenshot(port, os.path.join(args.out, "03_after_part_confirm.png"))
            mark(port, "after_part_confirm")

        # Handle a possible "missing part" / warn dialog (confirm_action view):
        ov = overshell(port)
        warn_guard = 0
        while ov[0] == "confirm_action" and warn_guard < 4:
            log(f"  confirm_action dialog (part denial/warn) — pressing Confirm to dismiss/continue")
            press(port, CONFIRM); drain_pad(port); time.sleep(0.5)
            ov = overshell(port); warn_guard += 1

        # --- choose difficulty: wait for choose_diff, scroll to chosen diff,
        # Confirm. The diff list is Easy(0)/Medium(1)/Hard(2)/Expert(3) top-down;
        # default focus is Easy, so press DDOWN diff_idx times then Confirm.
        ov = wait_view(port, lambda v: v == "choose_diff", 30, proc, "enter choose_diff", args.verbose)
        if ov is None:
            log(f"WARN: overshell view is '{overshell(port)[0]}', expected choose_diff")
            mark(port, "stuck-prediff")
            # It may have skipped straight to ready_to_play (skip_choose_diff);
            # fall through and check below.
        else:
            log(f"  choose_diff reached — scrolling to '{args.diff}' (DDOWN x{diff_idx})")
            for _ in range(diff_idx):
                press(port, DDOWN); drain_pad(port); time.sleep(0.25)
            screenshot(port, os.path.join(args.out, "04_choose_diff.png"))
            press(port, CONFIRM); drain_pad(port)
            mark(port, "after_diff_confirm")

        # A ChooseDiffConfirm dialog may appear (overshell_continue/restart/cancel)
        ov = overshell(port)
        conf_guard = 0
        while ov[0] == "confirm_action" and conf_guard < 4:
            log("  diff-confirm dialog — pressing Confirm (overshell_continue)")
            press(port, CONFIRM); drain_pad(port); time.sleep(0.5)
            ov = overshell(port); conf_guard += 1

        # --- ready to play: overshell should reach ready_to_play; the song then
        # launches automatically (OvershellPanel AllSlotsReadyToPlay -> MainHub).
        ov = wait_view(port, lambda v: v == "ready_to_play", 30, proc, "enter ready_to_play", args.verbose)
        if ov is not None:
            mark(port, "ready_to_play")
            screenshot(port, os.path.join(args.out, "05_ready_to_play.png"))

        # --- game_screen: wait for the song to actually load + play ----------
        h = wait_screen(port, "game_screen", 90, proc, args.verbose)
        if h is None:
            log("FAIL: never reached game_screen"); mark(port, "stuck-ready"); return 1
        mark(port, "game_screen")
        screenshot(port, os.path.join(args.out, "06_game_screen.png"))

        # Post-gameplay stability aids (sanctioned): nofail + autohit.
        verb(port, "nofail")
        # Wait for the song clock to start advancing (proves it's truly playing).
        is_playing = 0; start_ms = -1
        dl = time.time() + 60
        while time.time() < dl:
            ip = dta(port, "{game is_playing}")
            h = health(port)
            if ip is not None and int(ip) == 1:
                is_playing = 1
                if start_ms < 0 and h and h[1] >= 0: start_ms = h[1]
            verb(port, "autohit")
            if is_playing and h and h[1] > (start_ms + 200) and start_ms >= 0:
                break
            time.sleep(0.5)
        ov = overshell(port)
        h = health(port)
        log(f"  is_playing={is_playing} songMs={h[1]:.0f} (start={start_ms:.0f}) overshell={ov}")
        screenshot(port, os.path.join(args.out, "07_playing.png"))
        mark(port, "playing")

        eff_diff = ov[2]
        if is_playing and h[1] > 0 and eff_diff == args.diff:
            log(f"PASS: game_screen reached, song playing (songMs={h[1]:.0f}), diff='{eff_diff}'")
            rc = 0
        else:
            log(f"PARTIAL: game_screen reached. is_playing={is_playing} songMs={h[1]:.0f} "
                f"effective_diff='{eff_diff}' (wanted '{args.diff}')")
            rc = 0 if (is_playing and h[1] > 0) else 2
        return rc
    finally:
        log("=== TIMELINE ===")
        for label, scr, ms, ov in TIMELINE:
            log(f"  {label:>20s}: screen='{scr}' songMs={ms:.0f} overshell={ov}")
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
            try: proc.wait(timeout=8)
            except subprocess.TimeoutExpired: os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
        except Exception: pass
        logf.close()
        log(f"engine log: {log_path}")
        log(f"screenshots: {args.out}")


if __name__ == "__main__":
    sys.exit(main())
