#!/usr/bin/env python3
"""Arena 2D bowl-imposter crowd capture (render-polish Fix B).

Boots rb3-native headless, navigates splash->hub->song_select, injects
`{meta_performer set_venue_override arena_06}` BEFORE confirming the song (so
BandDirector::EnterVenue's Fix-C bridge loads the arena venue), drives to
gameplay, then captures a wide venue shot + a crowd-facing shot so the bowl crowd
rows are visible. The 2D imposter path (WorldCrowd::DrawShowing) renders each
crowd archetype into the shared render-target tex and billboards thousands of
quads across the bowl — this harness is the before/after gate for that feature.

A/B: run with RB3_CROWD_IMPOSTER_OFF=1 (empty bowl, prior dead path) vs default
(crowd rows render). Reuses keyboard-to-gameplay's nav helpers.

Usage:
  python3 scripts/native/arena-crowd2d-capture.py --port 9911 \
      --out /tmp/crowd-2d/after --tag after
"""
import argparse, os, sys, time, subprocess, signal

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import importlib.util
spec = importlib.util.spec_from_file_location(
    "kbd2game", os.path.join(HERE, "keyboard-to-gameplay.py"))
k = importlib.util.module_from_spec(spec)
spec.loader.exec_module(k)
REPO = k.REPO


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=9911)
    ap.add_argument("--bin", default=k.DEFAULT_BIN)
    ap.add_argument("--data", default=k.DEFAULT_DATA)
    ap.add_argument("--overlay", default=os.path.join(REPO, "native", "dta"))
    ap.add_argument("--venue", default="arena_06")
    ap.add_argument("--song-downs", type=int, default=4)
    ap.add_argument("--out", default="/tmp/crowd-2d/cap")
    ap.add_argument("--tag", default="cap")
    ap.add_argument("--shots", default="", help="comma-separated .shot names to force-capture")
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    if not os.path.exists(args.bin):
        print(f"FAIL: binary not found: {args.bin}"); return 1
    os.makedirs(args.out, exist_ok=True)
    port = args.port
    log_path = os.path.join("/tmp", f"rb3-crowd2d-{port}.log")
    logf = open(log_path, "w")
    env = dict(os.environ)
    env.update({"RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
                "MILO_HEADLESS": "1", "RB3_DATA": args.data,
                "RB3_DTA_OVERLAY": args.overlay, "RB3_INPUT_DEBUG": "1",
                "RB3_RENDER_DBG": env.get("RB3_RENDER_DBG", "1")})
    print(f"[crowd2d] launching rb3-native (port {port}); log -> {log_path}")
    proc = subprocess.Popen([args.bin], env=env, stdout=logf,
                            stderr=subprocess.STDOUT, cwd=REPO,
                            start_new_session=True)
    rc = 1
    try:
        if k.wait_screen(port, lambda s: True, 40, proc) is None:
            print("FAIL: HTTP server never came up"); return 1
        print("[crowd2d] HTTP up")
        k.wait_screen(port, lambda s: s and s != "", 60, proc, args.verbose)

        for _ in range(8):
            if k.health(port)[2] == "main_hub_screen": break
            k.press(port, k.START); k.drain_pad(port); time.sleep(0.6)
        if k.wait_screen(port, "main_hub_screen", 30, proc, args.verbose) is None:
            print("FAIL: no main_hub"); return 1

        for _ in range(10):
            if k.health(port)[2] == "song_select_screen": break
            k.press(port, k.CONFIRM); k.drain_pad(port); time.sleep(0.7)
        if k.wait_screen(port, "song_select_screen", 40, proc, args.verbose) is None:
            print("FAIL: no song_select"); return 1
        time.sleep(1.5)

        # CRITICAL: set the venue override BEFORE the song confirm that triggers
        # EnterVenue (the Fix-C bridge reads meta_performer at force-load time).
        r = k.dta(port, f"{{meta_performer set_venue_override {args.venue}}}")
        chk = k.dta(port, "{meta_performer get_venue_override}")
        print(f"[crowd2d] set_venue_override {args.venue} -> {r}; now={chk}")
        if chk != args.venue:
            print(f"WARN: override didn't stick (got {chk})")

        for _ in range(args.song_downs):
            k.press(port, k.DDOWN); k.drain_pad(port); time.sleep(0.2)
        k.press(port, k.CONFIRM); k.drain_pad(port)
        if k.wait_screen(port, "part_difficulty_screen", 60, proc, args.verbose) is None:
            print("FAIL: no part_difficulty"); return 1

        ov = k.wait_view(port, lambda v: v.startswith("choose_part") or v == "choose_diff",
                         30, proc, "choose_part", args.verbose)
        if ov and ov[0].startswith("choose_part"):
            k.press(port, k.CONFIRM); k.drain_pad(port)
        ov = k.overshell(port); g = 0
        while ov[0] == "confirm_action" and g < 4:
            k.press(port, k.CONFIRM); k.drain_pad(port); time.sleep(0.5)
            ov = k.overshell(port); g += 1
        ov = k.wait_view(port, lambda v: v == "choose_diff", 30, proc, "choose_diff", args.verbose)
        if ov:
            k.press(port, k.CONFIRM); k.drain_pad(port)
        ov = k.overshell(port); g = 0
        while ov[0] == "confirm_action" and g < 4:
            k.press(port, k.CONFIRM); k.drain_pad(port); time.sleep(0.5)
            ov = k.overshell(port); g += 1

        k.wait_view(port, lambda v: v == "ready_to_play", 30, proc, "ready", args.verbose)
        h = k.wait_screen(port, "game_screen", 120, proc, args.verbose)
        if h is None:
            print("FAIL: no game_screen"); return 1
        print("[crowd2d] game_screen reached")
        k.verb(port, "nofail")

        # let the song start + crowd fullness ramp
        is_playing = 0; start = -1; dl = time.time() + 60
        while time.time() < dl:
            ip = k.dta(port, "{game is_playing}"); h = k.health(port)
            if ip is not None and int(ip) == 1:
                is_playing = 1
                if start < 0 and h and h[1] >= 0: start = h[1]
            k.verb(port, "autohit")
            if is_playing and h and h[1] > start + 200 and start >= 0:
                break
            time.sleep(0.5)
        print(f"[crowd2d] is_playing={is_playing} songMs={k.health(port)[1]:.0f}")
        for _ in range(10):
            k.verb(port, "autohit"); time.sleep(0.4)

        # Freeze the director cam so forced venue/crowd shots stick.
        print("[crowd2d] disable director cam:",
              k.dta(port, "{$band_director set disabled 1}"))

        # Wide venue + crowd-facing shots. The crowd shots aim the camera at the
        # bowl rows (where the 2D imposters live). Capture each immediately after
        # forcing (the venue re-picks a shot every frame absent mDisabled).
        shots = (args.shots.split(",") if args.shots else
                 ["coop_bftb_bf.shot", "coop_bftb_gf.shot", "coop_bftb_vf.shot",
                  "coop_dir_crowd.shot", "coop_dir_far.shot"])
        for shot in shots:
            base = shot.replace(".shot", "")
            r = k.dta(port, f'{{band_director force_shot "{shot}"}}')
            for _ in range(3):
                k.verb(port, "autohit"); time.sleep(0.2)
            p = os.path.join(args.out, f"{args.tag}_{base}.png")
            ok = k.screenshot(port, p)
            print(f"[crowd2d]   {shot} -> {r}; {p} ok={ok} "
                  f"songMs={k.health(port)[1]:.0f}")
        # plain in-game frame too
        p = os.path.join(args.out, f"{args.tag}_game.png")
        print(f"[crowd2d]   game -> {p} ok={k.screenshot(port, p)}")
        rc = 0
        return rc
    finally:
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
            try: proc.wait(timeout=8)
            except subprocess.TimeoutExpired:
                os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
        except Exception: pass


if __name__ == "__main__":
    sys.exit(main())
