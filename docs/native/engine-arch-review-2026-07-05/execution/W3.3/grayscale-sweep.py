#!/usr/bin/env python3
"""grayscale-sweep.py — W3.3 (D.S2) characterization of the grayscale venue at song start.

Boots rb3-native headless, navigates to gameplay (QUICKPLAY default song), and
captures a songMs sweep (0..25s) PLUS the RB3_RENDER_DBG postproc-change log, so
we can pin the exact grayscale window and prove which mechanism produces it.

Flag-isolation configs (--config):
  default        : RB3_RENDER_DBG=1  (logs the active postproc name + saturation)
  pp_off         : RB3_PP_OFF=1      (disable the RB3PostProc grade composite)
  venue_light_off: RB3_VENUE_LIGHT_OFF=1 (disable P4 per-environ venue lighting)

Hypotheses:
  (i)   authored B&W postproc grade (B+W_film02.pp, saturation -40) -> pp_off makes
        the venue full-color even at 3s; RB3_RENDER_DBG names the active grade.
  (ii)  P4 grey-fallback lighting misfiring -> venue_light_off changes the window.
  (iii) postproc/tonemap uninitialized -> would be NO grade (full color), not B&W.

Usage:
  python3 grayscale-sweep.py --config default --out /tmp/w33/default \
      --bin /tmp/rb3-native-w33 [--song-downs 0] [--verbose]
"""
import argparse, os, signal, subprocess, sys, time, importlib.util

HERE = os.path.dirname(os.path.abspath(__file__))
KDIR = os.path.abspath(os.path.join(HERE, "..", "..", "..", "..", "..", "scripts", "native"))
_spec = importlib.util.spec_from_file_location("kbd2game", os.path.join(KDIR, "keyboard-to-gameplay.py"))
k = importlib.util.module_from_spec(_spec); _spec.loader.exec_module(k)
REPO = k.REPO

SWEEP_MS = [200, 1000, 2000, 3000, 4000, 6000, 9000, 13000, 18000, 25000]


def log(m): print(f"[w33-sweep] {m}", flush=True)


def nav_to_gameplay(port, proc, song_downs, diff_idx, verbose, cap=lambda: None):
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
    # From here on the song audio Play() is imminent. The engine free-runs FAST
    # during Python time.sleep but paces to ~1 frame per HTTP poll, so from this
    # point we go strictly poll-paced (NO sleeps) and let the capture callback
    # bucket songMs the instant it becomes valid — otherwise the clock races to
    # 20s+ during menu-confirm sleeps and the 0-15s grayscale window is skipped.
    def spin(n):
        for _ in range(n):
            cap()  # poll-paced tick (also captures songMs buckets)
    ov = k.wait_view(port, lambda v: v.startswith("choose_part") or v == "choose_diff", 30, proc, "choose_part", verbose)
    if ov and ov[0].startswith("choose_part"):
        k.press(port, k.CONFIRM)
    spin(4); ov = k.overshell(port); g = 0
    while ov[0] == "confirm_action" and g < 4:
        k.press(port, k.CONFIRM); spin(6); ov = k.overshell(port); g += 1
    ov = k.wait_view(port, lambda v: v == "choose_diff", 30, proc, "choose_diff", verbose)
    if ov:
        for _ in range(diff_idx):
            k.press(port, k.DDOWN); spin(3)
        k.press(port, k.CONFIRM)
    spin(4); ov = k.overshell(port); g = 0
    while ov[0] == "confirm_action" and g < 4:
        k.press(port, k.CONFIRM); spin(6); ov = k.overshell(port); g += 1
    log("post-confirm — poll-paced tight capture running")
    return True


CONFIG_ENV = {
    "default":         {"RB3_RENDER_DBG": "1"},
    "pp_off":          {"RB3_RENDER_DBG": "1", "RB3_PP_OFF": "1"},
    "venue_light_off": {"RB3_RENDER_DBG": "1", "RB3_VENUE_LIGHT_OFF": "1"},
}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--config", choices=list(CONFIG_ENV), default="default")
    ap.add_argument("--bin", default="/tmp/rb3-native-w33")
    ap.add_argument("--data", default=k.DEFAULT_DATA)
    ap.add_argument("--out", default="/tmp/w33/default")
    ap.add_argument("--song-downs", type=int, default=0)
    ap.add_argument("--diff-idx", type=int, default=3)  # expert
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    os.makedirs(args.out, exist_ok=True)
    if not os.path.exists(args.bin):
        log(f"FAIL: binary not found: {args.bin}"); return 2

    port = k.free_port()
    log_path = os.path.join(args.out, f"engine-{args.config}.log")
    logf = open(log_path, "w")
    env = dict(os.environ)
    env.update({"RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
                "MILO_HEADLESS": "1", "RB3_DATA": args.data, "RB3_FIXED_CLOCK": "1"})
    env.update(CONFIG_ENV[args.config])
    log(f"launching config={args.config} port={port} -> {log_path}")
    proc = subprocess.Popen([args.bin], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)
    try:
        # Poll-paced capture callback: each call advances ~1 frame and screenshots
        # the earliest frame meeting the next songMs target. Shared across nav +
        # post-nav so we catch the window whether Play() fires during choose_diff
        # or later.
        results = []
        state = {"i": 0, "reached_game": False}
        def cap():
            if state["i"] >= len(SWEEP_MS): return
            h = k.health(port)
            if h is None: return
            frame, ms, screen = h
            if screen == "game_screen": state["reached_game"] = True
            if ms < 0: return  # audio not live yet
            tgt = SWEEP_MS[state["i"]]
            if ms >= tgt:
                path = os.path.join(args.out, f"{args.config}_ms{tgt:05d}.png")
                ok = k.screenshot(port, path)
                results.append((tgt, ms, frame, screen, path, ok))
                log(f"  ms>={tgt:5d}: songMs={ms:7.0f} frame={frame} screen={screen} shot={'OK' if ok else 'FAIL'}")
                state["i"] += 1

        if not nav_to_gameplay(port, proc, args.song_downs, args.diff_idx, args.verbose, cap):
            log("FAIL: never reached gameplay"); return 2
        dl = time.time() + 120
        while state["i"] < len(SWEEP_MS) and time.time() < dl:
            if proc.poll() is not None:
                log("process died"); break
            cap()
        log(f"PASS config={args.config}: {len(results)} captures (reached_game={state['reached_game']}) in {args.out}")
        return 0
    finally:
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
            try: proc.wait(timeout=8)
            except subprocess.TimeoutExpired: os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
        except Exception: pass
        logf.close()
        log(f"engine log: {log_path}")


if __name__ == "__main__":
    sys.exit(main())
