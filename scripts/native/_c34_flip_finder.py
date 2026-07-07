#!/usr/bin/env python3
"""
_c34_flip_finder.py — rapid-fire screenshot loop through song_select to catch
the InlineHelp hold-label mid-rotation transition frame-by-frame, tagged with
the /api/health frame number so we can correlate against the RB3_HOLDLABEL_DBG
stderr log (which prints "RB3 Native: frame N complete" markers).
"""
import argparse, os, sys, time, subprocess, signal

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import importlib.util
spec = importlib.util.spec_from_file_location(
    "kbd2game", os.path.join(HERE, "keyboard-to-gameplay.py"))
k = importlib.util.module_from_spec(spec)
spec.loader.exec_module(k)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", default=k.DEFAULT_BIN)
    ap.add_argument("--data", default=k.DEFAULT_DATA)
    ap.add_argument("--out", default="/tmp/c34-flipfind")
    ap.add_argument("--duration", type=float, default=8.0)
    args = ap.parse_args()

    os.makedirs(args.out, exist_ok=True)
    port = k.free_port()
    log_path = os.path.join(args.out, "engine.log")
    logf = open(log_path, "w")
    env = dict(os.environ)
    env.update({"RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
                "MILO_HEADLESS": "1", "RB3_DATA": args.data,
                "RB3_FIXED_CLOCK": "1", "RB3_HOLDLABEL_DBG": "1"})
    proc = subprocess.Popen([args.bin], env=env, stdout=logf, stderr=subprocess.STDOUT,
                             cwd=k.REPO, start_new_session=True)
    try:
        k.wait_screen(port, lambda s: s and s != "", 40, proc)
        for attempt in range(8):
            cur = k.health(port)[2]
            if cur == "main_hub_screen": break
            k.press(port, k.START); k.drain_pad(port); time.sleep(0.6)
        k.wait_screen(port, "main_hub_screen", 30, proc)
        for attempt in range(10):
            cur = k.health(port)[2]
            if cur == "song_select_screen": break
            k.press(port, k.CONFIRM); k.drain_pad(port); time.sleep(0.7)
        k.wait_screen(port, "song_select_screen", 40, proc)
        time.sleep(2.0)
        for _ in range(2):
            k.press(port, k.DDOWN); k.drain_pad(port); time.sleep(0.3)

        t_end = time.time() + args.duration
        i = 0
        while time.time() < t_end:
            h = k.health(port)
            frame = h[0] if h else -1
            p = os.path.join(args.out, f"shot_{i:04d}_f{frame:06d}.png")
            k.screenshot(port, p)
            i += 1
        print(f"[flipfind] captured {i} frames -> {args.out}")
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
