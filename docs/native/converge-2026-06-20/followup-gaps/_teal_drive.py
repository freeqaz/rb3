#!/usr/bin/env python3
"""RESEARCH-ONLY driver for the teal-highway GAP A investigation. Boots the
master rb3-native, optionally sets a venue override, drives to a deep gameplay
anchor, captures screenshots, and lets env-gated diagnostics (GEM_DBG, SHARD_DBG,
etc.) log to the stderr log. Pass extra env via the shell (the binary inherits
the environment of this process)."""
import argparse, importlib.util, os, signal, subprocess, sys, time

WT = "/home/free/code/milohax/rb3"
spec = importlib.util.spec_from_file_location(
    "kg", os.path.join(WT, "scripts", "native", "keyboard-to-gameplay.py"))
kg = importlib.util.module_from_spec(spec); spec.loader.exec_module(kg)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--override", default="")
    ap.add_argument("--song-downs", type=int, default=4)
    ap.add_argument("--diff", default="hard")
    ap.add_argument("--hold-s", type=float, default=25.0)
    ap.add_argument("--log", default="/tmp/teal_drive.log")
    ap.add_argument("--out", default="/tmp/teal_drive")
    args = ap.parse_args()

    binp = os.path.join(WT, "native", "build-native", "rb3-native")
    data = os.path.join(WT, "orig-assets", "extracted")
    overlay = os.path.join(WT, "native", "dta")
    os.makedirs(args.out, exist_ok=True)
    port = kg.free_port()
    logf = open(args.log, "w")
    env = dict(os.environ)
    env.update({"RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
                "MILO_HEADLESS": "1", "RB3_DATA": data, "RB3_DTA_OVERLAY": overlay})
    print(f"[drive] launch port={port} override='{args.override}' downs={args.song_downs} log={args.log}")
    proc = subprocess.Popen([binp], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            cwd=WT, start_new_session=True)
    P = lambda b: kg.press(port, b)
    try:
        if kg.wait_screen(port, lambda s: True, 40, proc) is None:
            print("[drive] FAIL: no http"); return 1
        kg.wait_screen(port, lambda s: s and s != "", 60, proc)
        def setov():
            if args.override:
                r = kg.dta(port, '{meta_performer set_venue_override %s}' % args.override)
                print(f"[drive] set_venue_override {args.override} -> {r}")
        setov()
        for _ in range(8):
            if kg.health(port)[2] == "main_hub_screen": break
            P(kg.START); time.sleep(0.6)
        kg.wait_screen(port, "main_hub_screen", 30, proc); setov()
        for _ in range(10):
            if kg.health(port)[2] == "song_select_screen": break
            P(kg.CONFIRM); time.sleep(0.7)
        if kg.wait_screen(port, "song_select_screen", 40, proc) is None:
            print("[drive] FAIL: no song_select"); return 1
        time.sleep(1.5); setov()
        for _ in range(args.song_downs):
            P(kg.DDOWN); time.sleep(0.2)
        hl = kg.dta(port, "{music_library get_highlighted_node}")
        print(f"[drive] song node: {hl}")
        P(kg.CONFIRM)
        if kg.wait_screen(port, "part_difficulty_screen", 60, proc) is None:
            print("[drive] FAIL: no part_difficulty"); return 1
        setov()
        ov = kg.wait_view(port, lambda v: v.startswith("choose_part") or v == "choose_diff", 30, proc, "part")
        if ov and ov[0].startswith("choose_part"):
            P(kg.CONFIRM)
        o2 = kg.overshell(port); g = 0
        while o2[0] == "confirm_action" and g < 4:
            P(kg.CONFIRM); time.sleep(0.5); o2 = kg.overshell(port); g += 1
        ov = kg.wait_view(port, lambda v: v == "choose_diff", 30, proc, "diff")
        if ov:
            for _ in range(kg.DIFF_INDEX[args.diff]):
                P(kg.DDOWN); time.sleep(0.25)
            P(kg.CONFIRM)
        o2 = kg.overshell(port); g = 0
        while o2[0] == "confirm_action" and g < 4:
            P(kg.CONFIRM); time.sleep(0.5); o2 = kg.overshell(port); g += 1
        kg.wait_view(port, lambda v: v == "ready_to_play", 30, proc, "ready")
        h = kg.wait_screen(port, "game_screen", 90, proc)
        if h is None:
            print("[drive] FAIL: no game_screen"); return 1
        kg.verb(port, "nofail")
        print(f"[drive] game_screen reached songMs={h[1]:.0f}; holding {args.hold_s}s + autohit")
        dl = time.time() + args.hold_s
        shot_i = 0
        while time.time() < dl:
            kg.verb(port, "autohit")
            hh = kg.health(port)
            if hh is None: break
            if hh[1] > 30000 and shot_i < 6:
                p = os.path.join(args.out, f"anchor_{shot_i:02d}_ms{int(hh[1])}.png")
                if kg.screenshot(port, p):
                    print(f"[drive]   anchor shot {shot_i} songMs={hh[1]:.0f} -> {p}")
                    shot_i += 1
            time.sleep(0.8)
        hh = kg.health(port)
        print(f"[drive] DONE songMs={hh[1]:.0f} screen={hh[2]}")
        return 0
    finally:
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
            try: proc.wait(timeout=8)
            except subprocess.TimeoutExpired: os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
        except Exception: pass
        logf.close()


if __name__ == "__main__":
    sys.exit(main())
