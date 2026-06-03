#!/usr/bin/env python3
"""Verify the intro-cinematic screen flow in rb3-native (headless).

The intro video itself is a browser-only <video> overlay (web build); on native
desktop there are no pixels, but the *screen flow* — intro_movie_screen holds for
the movie duration, then movie_done advances to splash_screen — is shared logic
and is exactly what this exercises via RB3_INTRO_SECS virtual playback.

Usage: scripts/native/intro-cinematic-test.py [--secs 4] [--bin <path>]
Exit 0 if the timeline shows intro_movie_screen held >= ~secs*0.5 then advanced.
"""
import argparse, http.client, json, os, subprocess, sys, time

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DEFAULT_BIN = os.path.join(REPO, "native", "build-native", "rb3-native")


def health(port):
    try:
        c = http.client.HTTPConnection("127.0.0.1", port, timeout=10)
        try:
            c.request("GET", "/api/health"); r = c.getresponse()
            if r.status != 200: return None
            d = json.loads(r.read().decode("utf-8", "replace"))["data"]
            return int(d["frame"]), str(d["currentScreen"])
        finally:
            c.close()
    except Exception:
        return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", default=DEFAULT_BIN)
    ap.add_argument("--secs", type=float, default=4.0)
    ap.add_argument("--watch", type=float, default=18.0)
    ap.add_argument("--data", default=os.path.join(REPO, "orig-assets", "extracted"))
    args = ap.parse_args()

    if not os.path.exists(args.bin):
        print(f"FAIL: binary not found: {args.bin}"); return 1

    port = 8731
    log_path = f"/tmp/rb3-intro-{port}.log"; logf = open(log_path, "w")
    env = dict(os.environ)
    env.update({"RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
                "MILO_HEADLESS": "1", "RB3_DATA": args.data,
                "RB3_INTRO_SECS": str(args.secs)})
    print(f"launching rb3-native (port {port}, RB3_INTRO_SECS={args.secs}), log -> {log_path}")
    proc = subprocess.Popen([args.bin], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)
    timeline = []  # (elapsed, screen)
    try:
        t0 = time.time()
        # wait for server
        while time.time() - t0 < 30:
            if health(port) is not None: break
            if proc.poll() is not None:
                print(f"FAIL: process exited early rc={proc.returncode}"); return 1
            time.sleep(0.2)
        else:
            print("FAIL: HTTP server never came up"); return 1

        last_screen = None
        t0 = time.time()
        while time.time() - t0 < args.watch:
            h = health(port)
            if h is not None and h[1] != last_screen:
                el = time.time() - t0
                timeline.append((round(el, 2), h[1]))
                print(f"  t={el:5.2f}s  screen={h[1]}  frame={h[0]}")
                last_screen = h[1]
            time.sleep(0.15)
    finally:
        try: proc.terminate(); proc.wait(timeout=5)
        except Exception:
            try: proc.kill()
            except Exception: pass

    screens = [s for _, s in timeline]
    print("\nTIMELINE:", " -> ".join(screens))
    # intro must appear, hold, then advance to splash (or beyond)
    if "intro_movie_screen" not in screens:
        print("FAIL: never entered intro_movie_screen"); return 1
    intro_t = next(t for t, s in timeline if s == "intro_movie_screen")
    after = [(t, s) for t, s in timeline if t > intro_t and s != "intro_movie_screen"]
    if not after:
        print("FAIL: intro_movie_screen never advanced"); return 1
    held = after[0][0] - intro_t
    print(f"intro held {held:.2f}s then -> {after[0][1]}")
    if held < args.secs * 0.5:
        print(f"WARN: intro held only {held:.2f}s (< {args.secs*0.5:.2f}s); expected ~{args.secs}s")
        return 2
    if after[0][1] not in ("splash_screen", "main_hub_screen"):
        print(f"WARN: advanced to {after[0][1]} (expected splash_screen)")
    print("PASS: intro held for its duration then advanced")
    return 0


if __name__ == "__main__":
    sys.exit(main())
