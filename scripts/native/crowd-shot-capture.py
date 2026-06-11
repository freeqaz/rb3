#!/usr/bin/env python3
"""Crowd-shot capture: navigate to gameplay, force the crowd camera, and capture
timed screenshot pairs (to show animation) plus SHARD diagnostics.

Reuses keyboard-to-gameplay's nav helpers. The crowd shot is forced via
`{band_director force_shot "coop_dir_crowd.shot"}` (scout doc §4). Two
screenshots 2.5s apart let us check the crowd animates (pose changes).

Usage:
  SHARD_DBG=1 SHARD_RATIO_DBG=1 python3 scripts/native/crowd-shot-capture.py \
      --port 8741 --diff hard --out /tmp/rp2-crowd/before --tag before
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
    ap.add_argument("--port", type=int, default=8741)
    ap.add_argument("--bin", default=k.DEFAULT_BIN)
    ap.add_argument("--data", default=k.DEFAULT_DATA)
    ap.add_argument("--overlay", default=os.path.join(REPO, "native", "dta"))
    ap.add_argument("--diff", default="hard", choices=list(k.DIFF_INDEX))
    ap.add_argument("--song-downs", type=int, default=4)
    ap.add_argument("--out", default="/tmp/rp2-crowd/cap")
    ap.add_argument("--tag", default="cap")
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    if not os.path.exists(args.bin):
        print(f"FAIL: binary not found: {args.bin}"); return 1
    os.makedirs(args.out, exist_ok=True)
    port = args.port
    log_path = os.path.join("/tmp", f"rb3-crowdshot-{port}.log")
    logf = open(log_path, "w")
    env = dict(os.environ)
    env.update({"RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
                "MILO_HEADLESS": "1", "RB3_DATA": args.data,
                "RB3_DTA_OVERLAY": args.overlay, "RB3_INPUT_DEBUG": "1"})
    print(f"[crowdshot] launching rb3-native (port {port}); log -> {log_path}")
    proc = subprocess.Popen([args.bin], env=env, stdout=logf,
                            stderr=subprocess.STDOUT, cwd=REPO,
                            start_new_session=True)
    diff_idx = k.DIFF_INDEX[args.diff]
    rc = 1
    try:
        if k.wait_screen(port, lambda s: True, 40, proc) is None:
            print("FAIL: HTTP server never came up"); return 1
        print("[crowdshot] HTTP up")

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
            for _ in range(diff_idx):
                k.press(port, k.DDOWN); k.drain_pad(port); time.sleep(0.25)
            k.press(port, k.CONFIRM); k.drain_pad(port)
        ov = k.overshell(port); g = 0
        while ov[0] == "confirm_action" and g < 4:
            k.press(port, k.CONFIRM); k.drain_pad(port); time.sleep(0.5)
            ov = k.overshell(port); g += 1

        k.wait_view(port, lambda v: v == "ready_to_play", 30, proc, "ready", args.verbose)
        h = k.wait_screen(port, "game_screen", 90, proc, args.verbose)
        if h is None:
            print("FAIL: no game_screen"); return 1
        print("[crowdshot] game_screen reached")
        k.verb(port, "nofail")

        # let the song actually start + a few seconds of crowd warm-up/fullness ramp
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
        print(f"[crowdshot] is_playing={is_playing} songMs={k.health(port)[1]:.0f}")

        # let crowd fullness ramp + autohit to keep the song advancing
        for _ in range(12):
            k.verb(port, "autohit"); time.sleep(0.4)

        # Force the crowd camera shot and screenshot IMMEDIATELY (the venue
        # director re-picks a shot every frame in OnSelectCamera, so any dwell
        # lets it override our forced crowd cam). We resolve the crowd shot
        # object by name from the venue dir and pin it via force_shot, then grab
        # the very next rendered frame. For the animation check we re-force and
        # grab a second frame a few seconds later (pose should differ).
        # Freeze the band director's camera auto-pick so a forced shot STICKS
        # (OnSelectCamera re-picks every frame unless mDisabled). SYNC_PROP
        # `disabled` -> mDisabled. Then force the crowd cam and capture an
        # animation pair 2.5s apart (idle should move).
        print("[crowdshot] disable director cam:",
              k.dta(port, "{$band_director set disabled 1}"))
        crowd_shots = ["coop_dir_crowd.shot", "coop_dir_crowdb.shot",
                       "coop_dir_crowdg.shot"]
        for shot in crowd_shots:
            base = shot.replace(".shot", "")
            r = k.dta(port, f'{{band_director force_shot "{shot}"}}')
            # let the forced cam settle a couple frames
            for _ in range(3):
                k.verb(port, "autohit"); time.sleep(0.2)
            for fi in range(2):
                p = os.path.join(args.out, f"{args.tag}_{base}_{fi}.png")
                ok = k.screenshot(port, p)
                print(f"[crowdshot]   {shot} -> {r}; {p} ok={ok} "
                      f"songMs={k.health(port)[1]:.0f}")
                if fi == 0:
                    t0 = time.time()
                    while time.time() < t0 + 2.5:
                        k.verb(port, "autohit"); time.sleep(0.3)
        rc = 0
        return rc
    finally:
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
            try: proc.wait(timeout=8)
            except subprocess.TimeoutExpired:
                os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
        except Exception: pass
        logf.close()
        print(f"[crowdshot] engine log: {log_path}")
        print(f"[crowdshot] screenshots: {args.out}")


if __name__ == "__main__":
    sys.exit(main())
