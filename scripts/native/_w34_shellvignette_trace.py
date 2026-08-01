#!/usr/bin/env python3
"""
_w34_shellvignette_trace.py — W34-CHARCLIP-EVAL STEP-0 repro (amendment A3).

The 4.2x upperArm detonation events all sit at engine frames 0-399 (~0-7s of the
shell/loading-vignette window), so NO gameplay nav is needed: boot rb3-native
headless, hold for N seconds, capture stderr. The probe envs are passed through
with --env.

    python3 scripts/native/_w34_shellvignette_trace.py \
        --seconds 14 --out /tmp/w34-run1 \
        --env BAND_ANIM_PROBE='*' --env BAND_ANIM_ANAT=1 --env BAND_ANIM_ANATX=1.5
"""
import argparse, importlib.util, json, os, signal, subprocess, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location(
    "kbd2game", os.path.join(HERE, "keyboard-to-gameplay.py"))
k = importlib.util.module_from_spec(spec)
spec.loader.exec_module(k)
REPO = k.REPO


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", default=os.path.join(REPO, "native", "build-agent-W34", "rb3-native"))
    ap.add_argument("--data", default=os.path.join(REPO, "orig-assets", "extracted"))
    ap.add_argument("--overlay", default=os.path.join(REPO, "native", "dta"))
    ap.add_argument("--seconds", type=float, default=14.0)
    ap.add_argument("--out", default="/tmp/w34-trace")
    ap.add_argument("--env", action="append", default=[])
    ap.add_argument("--shot", action="append", default=[],
                    help="seconds-after-boot to grab a screenshot at")
    ap.add_argument("--nav-hub", action="store_true",
                    help="press Start until main_hub_screen (where the shell "
                         "vignette characters actually play clips)")
    args = ap.parse_args()

    os.makedirs(args.out, exist_ok=True)
    port = k.free_port()
    log_path = os.path.join(args.out, "engine.log")
    logf = open(log_path, "w")
    env = dict(os.environ)
    env.update({"RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
                "MILO_HEADLESS": "1", "RB3_DATA": args.data,
                "RB3_DTA_OVERLAY": args.overlay})
    for e in args.env:
        kk, _, vv = e.partition("=")
        env[kk] = vv
    print(f"[w34] launching {args.bin} port={port} -> {log_path}", flush=True)
    proc = subprocess.Popen([args.bin], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)
    shots = sorted(float(s) for s in args.shot)
    try:
        if args.nav_hub:
            if k.wait_screen(port, lambda s: True, 40, proc) is None:
                print("[w34] FAIL: HTTP never came up", flush=True)
            for _ in range(10):
                h = k.health(port)
                if h and h[2] == "main_hub_screen":
                    break
                k.press(port, k.START)
                k.drain_pad(port)
                time.sleep(0.6)
            h = k.health(port)
            print(f"[w34] screen={h[2] if h else '?'}", flush=True)
    except Exception as ex:
        print(f"[w34] nav error: {ex}", flush=True)
    t0 = time.time()
    try:
        si = 0
        while time.time() - t0 < args.seconds:
            if proc.poll() is not None:
                print(f"[w34] engine exited rc={proc.returncode}", flush=True)
                break
            while si < len(shots) and (time.time() - t0) >= shots[si]:
                p = os.path.join(args.out, f"shot_{shots[si]:.1f}s.png")
                try:
                    k.screenshot(port, p)
                    print(f"[w34] shot {p}", flush=True)
                except Exception as ex:
                    print(f"[w34] shot failed: {ex}", flush=True)
                si += 1
            time.sleep(0.2)
    finally:
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
        except Exception:
            pass
        logf.close()
    n = sum(1 for _ in open(log_path, "rb"))
    print(f"[w34] done: {n} log lines in {log_path}", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
