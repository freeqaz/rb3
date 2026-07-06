#!/usr/bin/env python3
"""
_w42-hub-text-capture.py — Wave 7 C.S1 (W4.2-fix) UI-text floor gate capture.

Boots rb3-native headless to main_hub_screen, sweeps the menu selection
up/down (pure pad DUP/DDOWN, no select:/msg: aids) and screenshots each
selection state, so the focused/unfocused label colour contrast can be
judged against images/retail-screenshots/. Also revisits the news-ticker
area (no nav needed -- always visible on main_hub) since that and the
FRIEND RANKINGS / CHOOSE INSTRUMENT labels are the "originally rescued"
labels the floor must keep readable.

    RB3_UI_TEXT_FLOOR_RELAXED=1 python3 scripts/native/_w42-hub-text-capture.py \
        --bin native/build-agent-CS1/rb3-native --out /tmp/w42/relaxed-on
    python3 scripts/native/_w42-hub-text-capture.py \
        --bin native/build-agent-CS1/rb3-native --out /tmp/w42/relaxed-off

Exit 0 = reached main_hub_screen and captured every sweep step.
"""
import argparse, os, sys, time

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
    ap.add_argument("--downs", type=int, default=3, help="how many DDOWN sweep steps to capture")
    ap.add_argument("--out", default="/tmp/w42/cap")
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    if not os.path.exists(args.bin):
        print(f"FAIL: binary not found: {args.bin}"); return 1
    os.makedirs(args.out, exist_ok=True)

    port = args.port or k.free_port()
    log_path = os.path.join("/tmp", f"rb3-w42-{port}.log")
    logf = open(log_path, "w")
    env = dict(os.environ)
    env.update({"RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
                "MILO_HEADLESS": "1", "RB3_DATA": args.data,
                "RB3_FIXED_CLOCK": "1"})
    print(f"[w42] launching rb3-native (port {port}); log -> {log_path}"
          f" RB3_UI_TEXT_FLOOR_RELAXED={env.get('RB3_UI_TEXT_FLOOR_RELAXED', '<unset>')}")
    import subprocess, signal
    proc = subprocess.Popen([args.bin], env=env, stdout=logf,
                             stderr=subprocess.STDOUT, cwd=REPO,
                             start_new_session=True)
    rc = 1
    try:
        if k.wait_screen(port, lambda s: True, 40, proc, args.verbose) is None:
            print("FAIL: HTTP server never came up"); return 1
        print("[w42] HTTP up")

        k.wait_screen(port, lambda s: s and s != "", 60, proc, args.verbose)
        for _ in range(8):
            h = k.health(port)
            if h and h[2] == "main_hub_screen": break
            k.press(port, k.START); k.drain_pad(port); time.sleep(0.6)
        if k.wait_screen(port, "main_hub_screen", 30, proc, args.verbose) is None:
            print("FAIL: never reached main_hub_screen"); return 1
        print("[w42] main_hub_screen reached")
        time.sleep(2.0)  # let enter-anim + ticker settle

        ok_all = True
        p0 = os.path.join(args.out, "sel_00_initial.png")
        ok = k.screenshot(port, p0)
        print(f"[w42] sel_00 (initial focus): screenshot={'OK' if ok else 'FAIL'} -> {p0}")
        ok_all &= ok

        for i in range(1, args.downs + 1):
            k.press(port, k.DDOWN); k.drain_pad(port); time.sleep(0.5)
            path = os.path.join(args.out, f"sel_{i:02d}_down.png")
            ok = k.screenshot(port, path)
            print(f"[w42] sel_{i:02d} (down x{i}): screenshot={'OK' if ok else 'FAIL'} -> {path}")
            ok_all &= ok

        rc = 0 if ok_all else 1
        print(f"[w42] {'PASS' if rc == 0 else 'FAIL'}: captured to {args.out}")
        return rc
    finally:
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
            try: proc.wait(timeout=8)
            except subprocess.TimeoutExpired: os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
        except Exception: pass
        logf.close()
        if rc != 0: print(f"[w42] engine log: {log_path}")


if __name__ == "__main__":
    sys.exit(main())
