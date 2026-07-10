#!/usr/bin/env python3
"""W29-CROWD-TRIGGER STEP-0 boot: reach main_hub, capture the interleaved
CHARDRV_PLAY / CHARDRV_PLAY_BT / PANELDBG stderr stream through the
splash(sv8 cityscape)->main_hub(sv3 streetslomo) transition.

Goal (STEP-0(i)): capture the WORKING beat-0 cityscape crowd1-5 CHARDRV_PLAY_BT
backtraces so the issuing trigger mechanism can be symbolized, and confirm the
zero-CHARDRV_PLAY-after-2.433 streetslomo gap.

One boot, all env in --extra-env; default probes CHARDRV_PROBE='*' CHARDRV_BT=1
RB3_CROWD_PANEL_DBG=1. stderr+stdout -> ONE file. Holds at main_hub HOLD_S so
the whole transition is captured. pgid-only cleanup.

Usage: python3 scripts/native/_w29_crowd_trigger_boot.py OUTLOG [--hold 20] [--extra-env k=v,k=v]
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


def dta(port, expr):
    try:
        c = http.client.HTTPConnection("127.0.0.1", port, timeout=15)
        body = json.dumps({"expr": expr})
        c.request("POST", "/api/dta/eval", body,
                  {"Content-Type": "application/json"})
        r = c.getresponse(); d = json.loads(r.read().decode("utf-8", "replace"))
        c.close(); return d
    except Exception as e:
        return {"error": str(e)}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("outlog")
    ap.add_argument("--hold", type=float, default=20.0)
    ap.add_argument("--extra-env", default="")  # k=v,k=v
    ap.add_argument("--dta-file", default="",
                    help="newline-separated dta exprs to eval at main_hub; "
                         "results appended to OUTLOG with [DTA_EVAL] markers")
    ap.add_argument("--keep", action="store_true")
    args = ap.parse_args()

    port = free_port()
    env = dict(os.environ)
    env.update({
        "RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
        "RB3_FIXED_CLOCK": "1", "MILO_HEADLESS": "1", "RB3_DATA": DATA,
        "RB3_GAME_INPUT": NAV,
        "RB3_CROWD_PANEL_DBG": "1", "CHARDRV_PROBE": "*", "CHARDRV_BT": "1",
    })
    for kv in args.extra_env.split(","):
        if "=" in kv:
            k, v = kv.split("=", 1); env[k] = v

    logf = open(args.outlog, "w")
    print(f"[w29-boot] port={port} log={args.outlog} hold={args.hold}s", flush=True)
    proc = subprocess.Popen([BIN], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)
    rc = 1
    try:
        dl = time.time() + 45
        while time.time() < dl:
            if proc.poll() is not None:
                print(f"[w29-boot] FAIL proc exited {proc.returncode}"); return 1
            if health(port):
                break
            time.sleep(0.4)
        dl = time.time() + 90; reached = False; last = None
        while time.time() < dl:
            if proc.poll() is not None:
                print(f"[w29-boot] FAIL proc exited {proc.returncode}"); return 1
            h = health(port)
            if h and h != last:
                print(f"[w29-boot] frame={h[0]} screen='{h[1]}'"); last = h
            if h and h[1] in ("main_hub", "main_hub_screen"):
                reached = True; break
            time.sleep(0.4)
        if not reached:
            print("[w29-boot] FAIL never reached main_hub"); return 1
        print(f"[w29-boot] main_hub reached; holding {args.hold}s")
        time.sleep(args.hold)

        if args.dta_file and os.path.exists(args.dta_file):
            with open(args.dta_file) as f:
                exprs = [ln.rstrip("\n") for ln in f if ln.strip()
                         and not ln.strip().startswith("#")]
            logf.flush()
            for e in exprs:
                res = dta(port, e)
                logf.write(f"[DTA_EVAL] expr={e!r}\n")
                logf.write(f"[DTA_EVAL] result={json.dumps(res)}\n")
                logf.flush()
                print(f"[w29-boot] dta {e!r} -> "
                      f"{json.dumps(res)[:200]}")
        if args.keep:
            print(f"[w29-boot] KEEP port={port} pid={proc.pid} "
                  f"pgid={os.getpgid(proc.pid)}")
            while proc.poll() is None:
                time.sleep(2)
        rc = 0
        print("[w29-boot] PASS")
        return 0
    finally:
        if not args.keep or proc.poll() is not None:
            try:
                os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
                try: proc.wait(timeout=8)
                except subprocess.TimeoutExpired:
                    os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
            except Exception:
                pass
        logf.close()
        print(f"[w29-boot] rc={rc} log={args.outlog}")


if __name__ == "__main__":
    sys.exit(main())
