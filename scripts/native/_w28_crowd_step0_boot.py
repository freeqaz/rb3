#!/usr/bin/env python3
"""W28-CROWD-OWNER STEP-0 boot: reach main_hub, capture the interleaved
CHARDRV + PANELDBG stderr stream (beat-2.433 REPLACE backtraces vs panel unloads).

One boot, three env vars (RB3_CROWD_PANEL_DBG=1 CHARDRV_PROBE=crowd CHARDRV_BT=1),
stderr+stdout to ONE file. Holds at main_hub HOLD_S seconds so the crowd lifecycle
counters (CHARDRV_LIFE) roll up. pgid-only cleanup.

Usage: python3 scripts/native/_w28_crowd_step0_boot.py OUTLOG [--hold 18]
"""
import argparse, http.client, json, os, signal, socket, subprocess, sys, time

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
BIN = os.path.join(REPO, "native", "build-native", "rb3-native")
DATA = os.path.join(REPO, "orig-assets", "extracted")
NAV = "@10:start,@30:confirm"


def free_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.bind(("127.0.0.1", 0)); p = s.getsockname()[1]; s.close(); return p


def health(port):
    try:
        c = http.client.HTTPConnection("127.0.0.1", port, timeout=8)
        c.request("GET", "/api/health"); r = c.getresponse()
        d = json.loads(r.read().decode("utf-8", "replace"))["data"]; c.close()
        return int(d["frame"]), str(d["currentScreen"])
    except Exception:
        return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("outlog")
    ap.add_argument("--hold", type=float, default=18.0)
    ap.add_argument("--extra-env", default="")  # k=v,k=v
    args = ap.parse_args()

    port = free_port()
    env = dict(os.environ)
    env.update({
        "RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
        "RB3_FIXED_CLOCK": "1", "MILO_HEADLESS": "1", "RB3_DATA": DATA,
        "RB3_GAME_INPUT": NAV,
        "RB3_CROWD_PANEL_DBG": "1", "CHARDRV_PROBE": "crowd", "CHARDRV_BT": "1",
    })
    for kv in args.extra_env.split(","):
        if "=" in kv:
            k, v = kv.split("=", 1); env[k] = v

    logf = open(args.outlog, "w")
    print(f"[w28-boot] port={port} log={args.outlog} hold={args.hold}s", flush=True)
    proc = subprocess.Popen([BIN], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)
    rc = 1
    try:
        dl = time.time() + 45
        while time.time() < dl:
            if proc.poll() is not None:
                print(f"[w28-boot] FAIL proc exited {proc.returncode}"); return 1
            if health(port):
                break
            time.sleep(0.4)
        # wait for main_hub
        dl = time.time() + 90; reached = False; last = None
        while time.time() < dl:
            if proc.poll() is not None:
                print(f"[w28-boot] FAIL proc exited {proc.returncode}"); return 1
            h = health(port)
            if h and h != last:
                print(f"[w28-boot] frame={h[0]} screen='{h[1]}'"); last = h
            if h and h[1] in ("main_hub", "main_hub_screen"):
                reached = True; break
            time.sleep(0.4)
        if not reached:
            print("[w28-boot] FAIL never reached main_hub"); return 1
        print(f"[w28-boot] main_hub reached; holding {args.hold}s")
        time.sleep(args.hold)
        rc = 0
        print("[w28-boot] PASS")
        return 0
    finally:
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
            try: proc.wait(timeout=8)
            except subprocess.TimeoutExpired:
                os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
        except Exception:
            pass
        logf.close()
        print(f"[w28-boot] rc={rc} log={args.outlog}")


if __name__ == "__main__":
    sys.exit(main())
