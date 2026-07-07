#!/usr/bin/env python3
"""
_c34_holdlabel_probe.py — Wave12 W4.3-C34 diagnosis probe.

Boots rb3-native headless with RB3_HOLDLABEL_DBG=1 + RB3_FIXED_CLOCK=1, drives
into song_select (music_library) to capture the C3 hold-label rotation state
over several seconds, then navigates to main_hub to capture the C4 ticker
UIList/InlineHelp world xfms. Screenshots both scenes. Prints the tail of the
debug log for grep.

    python3 scripts/native/_c34_holdlabel_probe.py --bin native/build-agent-W4.3-C34/rb3-native
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
    ap.add_argument("--port", type=int, default=0)
    ap.add_argument("--bin", default=k.DEFAULT_BIN)
    ap.add_argument("--data", default=k.DEFAULT_DATA)
    ap.add_argument("--out", default="/tmp/c34-probe")
    ap.add_argument("--settle-secs", type=float, default=8.0,
                     help="seconds to sit in song_select watching the rotation clock")
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    if not os.path.exists(args.bin):
        print(f"FAIL: binary not found: {args.bin}"); return 1
    os.makedirs(args.out, exist_ok=True)

    port = args.port or k.free_port()
    log_path = os.path.join(args.out, "engine.log")
    logf = open(log_path, "w")
    env = dict(os.environ)
    env.update({"RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
                "MILO_HEADLESS": "1", "RB3_DATA": args.data,
                "RB3_FIXED_CLOCK": "1", "RB3_HOLDLABEL_DBG": "1"})
    print(f"[c34] launching rb3-native (port {port}); log -> {log_path}")
    proc = subprocess.Popen([args.bin], env=env, stdout=logf,
                             stderr=subprocess.STDOUT, cwd=REPO,
                             start_new_session=True)
    rc = 1
    try:
        if k.wait_screen(port, lambda s: True, 40, proc, args.verbose) is None:
            print("FAIL: HTTP server never came up"); return 1
        print("[c34] HTTP up")

        # --- splash: press Start to advance to main_hub ---
        k.wait_screen(port, lambda s: s and s != "", 60, proc, args.verbose)
        for attempt in range(8):
            cur = k.health(port)[2]
            if cur == "main_hub_screen": break
            k.press(port, k.START); k.drain_pad(port); time.sleep(0.6)
        if k.wait_screen(port, "main_hub_screen", 30, proc, args.verbose) is None:
            print("FAIL: never reached main_hub_screen"); return 1
        print("[c34] main_hub_screen reached")
        time.sleep(3.0)  # let ticker populate + settle
        p_hub = os.path.join(args.out, "hub.png")
        ok_hub = k.screenshot(port, p_hub)
        print(f"[c34] hub screenshot={'OK' if ok_hub else 'FAIL'} -> {p_hub}")

        # --- navigate into song_select (PLAY NOW -> QUICKPLAY via Confirm) ---
        for attempt in range(10):
            cur = k.health(port)[2]
            if cur == "song_select_screen": break
            k.press(port, k.CONFIRM); k.drain_pad(port); time.sleep(0.7)
        h = k.wait_screen(port, "song_select_screen", 40, proc, args.verbose)
        if h is None:
            print("FAIL: never reached song_select_screen"); return 1
        print("[c34] song_select_screen reached")
        time.sleep(2.0)
        for _ in range(2):
            k.press(port, k.DDOWN); k.drain_pad(port); time.sleep(0.3)
        p_ss0 = os.path.join(args.out, "song_select_t0.png")
        ok_ss0 = k.screenshot(port, p_ss0)
        print(f"[c34] song_select t0 screenshot={'OK' if ok_ss0 else 'FAIL'} -> {p_ss0}")

        # sit and watch the flip-rotation clock across its ~6s cycle
        steps = max(1, int(args.settle_secs / 1.0))
        for i in range(steps):
            time.sleep(1.0)
            p = os.path.join(args.out, f"song_select_t{i+1}.png")
            ok = k.screenshot(port, p)
            print(f"[c34] song_select t{i+1} screenshot={'OK' if ok else 'FAIL'} -> {p}")

        rc = 0
        print(f"[c34] PASS: captured to {args.out}")
        return 0
    finally:
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
            try: proc.wait(timeout=8)
            except subprocess.TimeoutExpired: os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
        except Exception: pass
        logf.close()
        print(f"[c34] engine log: {log_path}")


if __name__ == "__main__":
    sys.exit(main())
