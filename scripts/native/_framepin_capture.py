#!/usr/bin/env python3
"""_framepin_capture.py — frame-PINNED native capture for strict A/B.

Boots rb3-native headless, navigates to a target screen with a nav that HALTS
there (no further screen advancement), then waits until a FIXED frame number is
reached WHILE on the target screen, and screenshots exactly then. Because both
builds run identical deterministic engine logic, "frame N on screen X" is the
same content on both — so the strict pixel diff is valid (no animation-phase
desync from a wall-clock settle, no screen-name race).

Usage:
    _framepin_capture.py --bin PATH --screen main_hub|song_select|game \
        --pin-frame N --out OUT.png [--port P] [--data DIR] [--extra-down K]

Exit 0 = captured at the pinned frame on the target screen.
"""
import argparse, http.client, json, os, signal, socket, subprocess, sys, time

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DEFAULT_DATA = os.path.join(REPO, "orig-assets", "extracted")

SCREEN_NAME = {
    "main_hub": "main_hub_screen",
    "song_select": "song_select_screen",
    "game": "game_screen",
}

# Nav that STOPS at each target (no verbs past it, so the screen is stable while
# we wait for the pinned frame). main_hub: just boot. song_select: into the
# library, no song-confirm. game: full into a song.
NAV = {
    "main_hub": "@10:start,@30:confirm",
    "song_select": "@10:start,@30:confirm,@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn",
    "game": ("@10:start,@30:confirm,@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn,"
             "@320:down,@350:msg:music_library:select_highlighted_node,"
             "@380:part:guitar,@400:diff:expert,"
             "@500:nofail,@520:autohit"),
}

def log(m): print(f"[fp] {m}", flush=True)

def free_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM); s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]; s.close(); return p

def http_get(port, path, timeout=20):
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=timeout)
    try:
        c.request("GET", path); r = c.getresponse(); return r.status, r.read()
    finally:
        c.close()

def health(port):
    try:
        st, b = http_get(port, "/api/health", timeout=8)
        if st != 200: return None
        d = json.loads(b.decode("utf-8", "replace"))["data"]
        return int(d["frame"]), float(d.get("songMs", 0)), str(d["currentScreen"])
    except Exception:
        return None

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", required=True)
    ap.add_argument("--screen", required=True, choices=list(SCREEN_NAME))
    ap.add_argument("--pin-frame", type=int, required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--port", type=int, default=0)
    ap.add_argument("--data", default=DEFAULT_DATA)
    # +30s headroom vs the old direct-commit nav: the "game" target now waits
    # on the real part_difficulty screen (part:/diff: pad-press verbs) instead
    # of an instant track:/difficulty: skip.
    ap.add_argument("--timeout", type=int, default=270)
    args = ap.parse_args()

    if not os.path.exists(args.bin):
        log(f"FAIL: binary not found: {args.bin}"); return 1
    target = SCREEN_NAME[args.screen]
    port = args.port or free_port()
    log_path = os.path.join("/tmp", f"fp-{args.screen}-{port}.log")
    logf = open(log_path, "w")
    env = dict(os.environ)
    env.update({"RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
                "MILO_HEADLESS": "1", "RB3_DATA": args.data,
                "RB3_GAME_INPUT": NAV[args.screen]})
    log(f"launch {os.path.basename(args.bin)} screen={args.screen} pin-frame={args.pin_frame} port={port}")
    proc = subprocess.Popen([args.bin], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)
    rc = 1
    try:
        dl = time.time() + args.timeout
        on_target_since = None
        while time.time() < dl:
            if proc.poll() is not None:
                log(f"FAIL: process exited (code {proc.returncode}) before pin"); return 1
            h = health(port)
            if h is None:
                time.sleep(0.2); continue
            frame, songms, scr = h
            if scr == target:
                # On target screen. Wait until the pinned frame is reached, then shoot.
                if frame >= args.pin_frame:
                    st, data = http_get(port, "/api/screenshot", timeout=25)
                    if st == 200 and data[:8] == b"\x89PNG\r\n\x1a\n":
                        # re-read frame to record what we actually captured
                        h2 = health(port)
                        with open(args.out, "wb") as f: f.write(data)
                        log(f"PASS captured screen={scr} at frame~{h2[0] if h2 else frame} -> {args.out}")
                        rc = 0; return 0
                    else:
                        log(f"FAIL: screenshot http {st}"); return 1
            time.sleep(0.1)
        log(f"FAIL: timed out (never reached frame {args.pin_frame} on {target})"); return 1
    finally:
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
            try: proc.wait(timeout=8)
            except subprocess.TimeoutExpired: os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
        except Exception: pass
        logf.close()
        if rc != 0: log(f"engine log: {log_path}")

if __name__ == "__main__":
    sys.exit(main())
