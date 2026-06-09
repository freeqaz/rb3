#!/usr/bin/env python3
"""visual_diff_capture.py — capture canonical RB3 screens and diff two sources.

The capture+compare driver on top of visual_diff.py. Given two SOURCES, it
captures the SAME canonical screens (main_hub, song_select, optionally game)
from each, then runs the canonical diff per screen and emits a report.

A SOURCE is one of:
  native:<port>   launch rb3-native headless, capture via /api/screenshot
  web:<port>      drive the Playwright harness, capture the #rb3-canvas
  dir:<path>      a directory of pre-captured PNGs (<screen>.png)
  ref             the curated reference screenshots in images/retail-screenshots/

Two comparison modes (matching visual_diff.py):
  --mode strict       build-vs-build A/B; equal-size, per-channel |Δ|, %differing,
                      PASS/FAIL vs --threshold. Use this to gate a GPU-backend
                      change against the current build.
  --mode perceptual   resolution-agnostic similarity 0..100; use when B is the
                      reference photos (different renderer/AA/res) — a sanity
                      "did this screen render at all" check.

Examples:
  # A/B: two captures of the SAME live web build (determinism sanity, ~0%)
  visual_diff_capture.py --a web:8421 --b web:8421 --mode strict

  # strict: a candidate native build vs the baseline native build
  visual_diff_capture.py --a native:0 --b native:0 \
        --a-bin native/build-native/rb3-native \
        --b-bin /path/to/candidate/rb3-native --mode strict

  # perceptual: the live build vs the curated reference screenshots
  visual_diff_capture.py --a web:8421 --b ref --mode perceptual

  # diff already-captured dirs (no capture)
  visual_diff_capture.py --a dir:/tmp/run1 --b dir:/tmp/run2 --mode strict

Exit 0 iff every compared screen PASSes; 1 if any FAILs; 2 on a harness error.
The last stdout line is `VISUAL_DIFF_CAPTURE verdict=... ...` (machine-readable).
"""
from __future__ import annotations

import argparse
import http.client
import json
import os
import signal
import socket
import subprocess
import sys
import time

import visual_diff  # same dir

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
NATIVE_BIN = os.path.join(REPO, "native", "build-native", "rb3-native")
NATIVE_DATA = os.path.join(REPO, "orig-assets", "extracted")
REF_DIR = os.path.join(REPO, "images", "retail-screenshots")
WEB_CAPTURE = os.path.join(REPO, "scripts", "web", "visual-capture.mjs")

# Canonical screens, in boot order. Each maps to the native engine screen name
# and (for the reference source) the best ground-truth file.
CANONICAL = ["main_hub", "song_select", "game"]
NATIVE_SCREEN_NAME = {
    "main_hub": "main_hub_screen",
    "song_select": "song_select_screen",
    "game": "game_screen",
}
# Reference photo per canonical screen (curated set; Wii-native where available).
REF_FILE = {
    "main_hub": "yt_mhKNp9uAT48_menu_hub.png",
    "song_select": "yt_qRagnZCIMzk_song_select_list.png",
    "game": "yt_qRagnZCIMzk_gameplay_guitar.png",
}

# Native boot→screen nav (RB3_GAME_INPUT verb script). Drives down to game; the
# driver stops capturing once it has every requested screen.
NATIVE_NAV = (
    "@10:start,@30:confirm,"
    "@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn,"
    "@320:down,@350:msg:music_library:select_highlighted_node,"
    "@380:track:guitar,@390:difficulty:expert,@450:msg:overshell:end_override_flow:1:0,"
    "@500:nofail,@520:autohit"
)

SERVER_READY_TIMEOUT = 45
SCREEN_TIMEOUT = 180
SETTLE_S = 2.0


def log(m):
    print(f"[vcap] {m}", flush=True)


def free_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]
    s.close()
    return p


# ---------------------------------------------------------------------------
# Native capture (HTTP /api/screenshot)
# ---------------------------------------------------------------------------

def _http_get(port, path, timeout=20):
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=timeout)
    try:
        c.request("GET", path)
        r = c.getresponse()
        return r.status, r.read()
    finally:
        c.close()


def _native_health(port):
    try:
        st, b = _http_get(port, "/api/health", timeout=8)
        if st != 200:
            return None
        d = json.loads(b.decode("utf-8", "replace"))["data"]
        return int(d["frame"]), str(d["currentScreen"])
    except Exception:
        return None


def capture_native(port, out_dir, want, bin_path, data_dir, verbose):
    """Boot rb3-native headless, navigate, /api/screenshot each requested screen."""
    if not os.path.exists(bin_path):
        log(f"FAIL: native binary not found: {bin_path}")
        return None
    os.makedirs(out_dir, exist_ok=True)
    port = port or free_port()
    log_path = os.path.join("/tmp", f"vcap-native-{port}.log")
    logf = open(log_path, "w")
    env = dict(os.environ)
    env.update({
        "RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
        "MILO_HEADLESS": "1", "RB3_DATA": data_dir, "RB3_GAME_INPUT": NATIVE_NAV,
    })
    log(f"native: launching {os.path.basename(bin_path)} on port {port} (log {log_path})")
    proc = subprocess.Popen([bin_path], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)
    captured = {}
    try:
        # wait for server
        dl = time.time() + SERVER_READY_TIMEOUT
        while time.time() < dl and _native_health(port) is None:
            if proc.poll() is not None:
                log(f"FAIL: native exited (code {proc.returncode}) before server up")
                return None
            time.sleep(0.4)
        if _native_health(port) is None:
            log("FAIL: native HTTP server never came up")
            return None

        # deepest screen we need (boot order) determines how far to drive.
        order = [s for s in CANONICAL if s in want]
        for short in order:
            target = NATIVE_SCREEN_NAME[short]
            dl = time.time() + SCREEN_TIMEOUT
            reached = False
            last = None
            while time.time() < dl:
                if proc.poll() is not None:
                    log(f"FAIL: native exited (code {proc.returncode}) waiting for {target}")
                    return captured
                h = _native_health(port)
                if h:
                    f, scr = h
                    if verbose and scr != last:
                        log(f"  ...native screen='{scr}' frame={f}")
                        last = scr
                    if scr == target:
                        reached = True
                        break
                time.sleep(0.4)
            if not reached:
                log(f"native: never reached {target}; stopping")
                break
            time.sleep(SETTLE_S)
            path = os.path.join(out_dir, f"{short}.png")
            st, data = _http_get(port, "/api/screenshot", timeout=25)
            if st == 200 and data[:8] == b"\x89PNG\r\n\x1a\n":
                with open(path, "wb") as f:
                    f.write(data)
                captured[short] = path
                log(f"native: captured {short} -> {path}")
            else:
                log(f"native: screenshot FAILED for {short} (http {st})")
        return captured
    finally:
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
            try:
                proc.wait(timeout=8)
            except subprocess.TimeoutExpired:
                os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
        except Exception:
            pass
        logf.close()


# ---------------------------------------------------------------------------
# Web capture (Playwright via visual-capture.mjs)
# ---------------------------------------------------------------------------

def capture_web(port, out_dir, want, query, verbose):
    """Drive scripts/web/visual-capture.mjs to capture the canvas per screen."""
    os.makedirs(out_dir, exist_ok=True)
    cmd = ["node", WEB_CAPTURE, "--port", str(port), "--out", out_dir,
           "--screens", ",".join(s for s in CANONICAL if s in want)]
    if query:
        cmd += ["--query", query]
    if verbose:
        cmd += ["--verbose"]
    log(f"web: {' '.join(cmd)}")
    cwd = os.path.join(REPO, "scripts", "web")
    try:
        r = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, timeout=600)
    except subprocess.TimeoutExpired:
        log("web: capture timed out")
        return None
    if verbose or r.returncode != 0:
        sys.stdout.write(r.stdout)
        sys.stderr.write(r.stderr)
    captured = {}
    for line in r.stdout.splitlines():
        if line.startswith("CAPTURE_RESULT "):
            try:
                obj = json.loads(line[len("CAPTURE_RESULT "):])
                for k, v in obj.get("screens", {}).items():
                    if v and os.path.exists(v):
                        captured[k] = v
            except Exception:
                pass
    if not captured:
        log("web: no screens captured (see output above)")
    return captured


# ---------------------------------------------------------------------------
# Source resolution
# ---------------------------------------------------------------------------

def resolve_source(spec, side, out_root, want, args, verbose):
    """Resolve a source spec to {screen: png_path}. `side` is 'a' or 'b'."""
    if spec == "ref":
        d = {}
        for s in want:
            p = os.path.join(REF_DIR, REF_FILE.get(s, ""))
            if os.path.exists(p):
                d[s] = p
            else:
                log(f"ref: no reference file for '{s}'")
        return d
    if ":" not in spec:
        log(f"bad source spec '{spec}' (want native:PORT | web:PORT | dir:PATH | ref)")
        return None
    kind, val = spec.split(":", 1)
    out_dir = os.path.join(out_root, side)
    if kind == "dir":
        d = {}
        for s in want:
            p = os.path.join(val, f"{s}.png")
            if os.path.exists(p):
                d[s] = p
        if not d:
            log(f"dir: no <screen>.png found under {val}")
        return d
    if kind == "native":
        bin_path = (args.a_bin if side == "a" else args.b_bin) or NATIVE_BIN
        data_dir = (args.a_data if side == "a" else args.b_data) or NATIVE_DATA
        return capture_native(int(val), out_dir, want, bin_path, data_dir, verbose)
    if kind == "web":
        query = args.a_query if side == "a" else args.b_query
        return capture_web(int(val), out_dir, want, query, verbose)
    log(f"unknown source kind '{kind}'")
    return None


# ---------------------------------------------------------------------------
# Driver
# ---------------------------------------------------------------------------

def main(argv=None):
    ap = argparse.ArgumentParser(description="Capture canonical RB3 screens and diff two sources.")
    ap.add_argument("--a", required=True, help="source A (native:PORT | web:PORT | dir:PATH | ref)")
    ap.add_argument("--b", required=True, help="source B (same forms)")
    ap.add_argument("--screens", default="main_hub,song_select",
                    help="comma list of canonical screens (main_hub,song_select,game)")
    ap.add_argument("--mode", choices=["strict", "perceptual"], default="strict")
    ap.add_argument("--out", default=None, help="output root (default /tmp/rb3-vcap-<ts>)")
    # strict knobs
    ap.add_argument("--tol", type=int, default=2)
    ap.add_argument("--threshold", type=float, default=0.10, help="strict: max %%differing for PASS")
    # perceptual knobs
    ap.add_argument("--min-score", type=float, default=35.0)
    # per-source native overrides
    ap.add_argument("--a-bin", default=None)
    ap.add_argument("--b-bin", default=None)
    ap.add_argument("--a-data", default=None)
    ap.add_argument("--b-data", default=None)
    # per-source web overrides
    ap.add_argument("--a-query", default="")
    ap.add_argument("--b-query", default="")
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args(argv)

    want = [s.strip() for s in args.screens.split(",") if s.strip()]
    for s in want:
        if s not in CANONICAL:
            log(f"unknown screen '{s}' (valid: {', '.join(CANONICAL)})")
            return 2

    ts = time.strftime("%Y%m%d-%H%M%S")
    out_root = args.out or f"/tmp/rb3-vcap-{ts}"
    os.makedirs(out_root, exist_ok=True)
    log(f"out: {out_root}")

    cap_a = resolve_source(args.a, "a", out_root, want, args, args.verbose)
    cap_b = resolve_source(args.b, "b", out_root, want, args, args.verbose)
    if cap_a is None or cap_b is None:
        log("FAIL: a source could not be resolved")
        return 2

    # diff per screen present in BOTH.
    report = []
    overall_pass = True
    any_compared = False
    diff_dir = os.path.join(out_root, "diff")
    os.makedirs(diff_dir, exist_ok=True)

    for s in want:
        pa, pb = cap_a.get(s), cap_b.get(s)
        if not pa or not pb:
            report.append({"screen": s, "status": "MISSING",
                           "a": bool(pa), "b": bool(pb)})
            log(f"{s}: MISSING (a={'ok' if pa else 'NO'} b={'ok' if pb else 'NO'})")
            overall_pass = False
            continue
        any_compared = True
        try:
            ia = visual_diff.load_rgb(pa)
            ib = visual_diff.load_rgb(pb)
        except Exception as e:  # noqa: BLE001
            report.append({"screen": s, "status": "ERROR", "error": str(e)})
            log(f"{s}: ERROR loading images: {e}")
            overall_pass = False
            continue

        if args.mode == "strict":
            try:
                res = visual_diff.diff_strict(ia, ib, tol=args.tol, threshold_pct=args.threshold)
            except ValueError as e:
                report.append({"screen": s, "status": "ERROR", "error": str(e)})
                log(f"{s}: SIZE MISMATCH ({e}) — strict mode needs equal-size frames")
                overall_pass = False
                continue
            heat = os.path.join(diff_dir, f"{s}_heatmap.png")
            visual_diff.write_heatmap(ia, ib, args.tol, heat)
            entry = visual_diff.asdict(res)
            entry["screen"] = s
            entry["heatmap"] = heat
            entry["a"] = pa
            entry["b"] = pb
            report.append(entry)
            log(f"{s}: {res.summary()}")
            if res.verdict != "PASS":
                overall_pass = False
        else:
            res = visual_diff.diff_perceptual(ia, ib, min_score=args.min_score)
            entry = visual_diff.asdict(res)
            entry["screen"] = s
            entry["a"] = pa
            entry["b"] = pb
            report.append(entry)
            log(f"{s}: {res.summary()}")
            if res.verdict != "PASS":
                overall_pass = False

    report_path = os.path.join(out_root, "report.json")
    with open(report_path, "w") as f:
        json.dump({"mode": args.mode, "a": args.a, "b": args.b,
                   "screens": report}, f, indent=2)
    log(f"report: {report_path}")

    verdict = "PASS" if (overall_pass and any_compared) else "FAIL"
    print(f"VISUAL_DIFF_CAPTURE verdict={verdict} mode={args.mode} "
          f"a={args.a} b={args.b} compared={sum(1 for r in report if r.get('status') != 'MISSING')} "
          f"report={report_path}")
    return 0 if verdict == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())
