#!/usr/bin/env python3
"""
capture.py — Wave 7 C.S3 flip-decision package capture for RB3_HUB_MENU_QUAD_HIDE.

Boots rb3-native to main_hub twice per flag state (OFF, OFF, ON, ON — 4 boots
total) so the reviewer gets a genuine A/A pair alongside the A/B comparison,
plus crops the PLAY-NOW label ROI out of every capture so "the label still
renders" is a pixel fact, not an eyeball claim.

Usage:
    python3 capture.py --bin PATH/TO/rb3-native --out DIR

Exit 0 = all 4 boots reached main_hub_screen and captured. Exit 1 otherwise.
"""
import argparse, http.client, json, os, signal, socket, subprocess, sys, time

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "..", "..", "..", ".."))
DEFAULT_DATA = os.path.join(REPO, "orig-assets", "extracted")
NAV = "@10:start,@30:confirm"
TARGET = "main_hub_screen"
READY_TIMEOUT = 45
SCREEN_TIMEOUT = 130


def log(m): print(f"[hub-quad-flip-capture] {m}", flush=True)


def free_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]
    s.close()
    return p


def http_get(port, path, timeout=20):
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=timeout)
    try:
        c.request("GET", path)
        r = c.getresponse()
        return r.status, r.read()
    finally:
        c.close()


def health(port):
    try:
        st, b = http_get(port, "/api/health", timeout=8)
        if st != 200:
            return None
        d = json.loads(b.decode("utf-8", "replace"))["data"]
        return int(d["frame"]), float(d["songMs"]), str(d["currentScreen"])
    except Exception:
        return None


def screenshot(port, path):
    st, data = http_get(port, "/api/screenshot", timeout=25)
    if st != 200 or not data or data[:8] != b"\x89PNG\r\n\x1a\n":
        return False
    with open(path, "wb") as f:
        f.write(data)
    return True


def wait_for(port, pred, timeout, label, proc):
    dl = time.time() + timeout
    while time.time() < dl:
        if proc.poll() is not None:
            log(f"FAIL: process exited (code {proc.returncode}) waiting {label}")
            return None
        h = health(port)
        if h is not None and pred(h):
            return h
        time.sleep(0.4)
    return None


def capture_one(tag, hide, bin_path, data_dir, out_dir):
    port = free_port()
    log_path = os.path.join("/tmp", f"hub-quad-flip-{tag}-{port}.log")
    logf = open(log_path, "w")
    env = dict(os.environ)
    env.update({"RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
                "MILO_HEADLESS": "1", "RB3_FIXED_CLOCK": "1",
                "RB3_DATA": data_dir, "RB3_GAME_INPUT": NAV})
    if hide:
        env["RB3_HUB_MENU_QUAD_HIDE"] = "1"
    else:
        env.pop("RB3_HUB_MENU_QUAD_HIDE", None)
    # W4.2-fix / W3.3-fix are both landed default-OFF at current engine HEAD;
    # leave them at their default (unset = OFF) so this package reflects the
    # currently-shipping decision state, not a pre-empted coordinator flip.
    env.pop("RB3_UI_TEXT_FLOOR_RELAXED", None)
    env.pop("RB3_POSTPROC_CEILING_FIX", None)
    proc = subprocess.Popen([bin_path], env=env, stdout=logf, stderr=subprocess.STDOUT,
                             cwd=REPO, start_new_session=True)
    try:
        if wait_for(port, lambda h: True, READY_TIMEOUT, "server", proc) is None:
            log(f"FAIL {tag}: server never up")
            return None
        h = wait_for(port, lambda h: h[2] == TARGET, SCREEN_TIMEOUT, "main_hub", proc)
        if h is None:
            log(f"FAIL {tag}: never reached {TARGET}")
            return None
        time.sleep(3.0)  # settle (venue-backdrop parallax / MOTD ticker)
        out = os.path.join(out_dir, f"{tag}.png")
        ok = screenshot(port, out)
        log(f"{tag}: frame={h[0]} songMs={h[1]:.0f} screen='{h[2]}' shot={'OK' if ok else 'FAIL'} -> {out}")
        return out if ok else None
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


def crop_roi(src, dst, box):
    try:
        from PIL import Image
    except ImportError:
        log("PIL not available, skipping ROI crop")
        return False
    im = Image.open(src)
    im.crop(box).save(dst)
    return True


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--data", default=DEFAULT_DATA)
    args = ap.parse_args()
    if not os.path.exists(args.bin):
        log(f"FAIL: binary not found: {args.bin}")
        return 1
    os.makedirs(args.out, exist_ok=True)

    plan = [("off_1", 0), ("off_2", 0), ("on_1", 1), ("on_2", 1)]
    fail = 0
    shots = {}
    for tag, hide in plan:
        r = capture_one(tag, hide, args.bin, args.data, args.out)
        if r is None:
            fail += 1
        else:
            shots[tag] = r

    # PLAY-NOW label ROI, from the known main_hub layout (1280x720):
    # covers PLAY NOW / QUICKPLAY / START A ROAD CHALLENGE stack.
    label_box = (140, 195, 700, 330)
    # Grey-quad ROI (the thing being hidden), below the label stack.
    quad_box = (380, 365, 900, 410)
    for tag, path in shots.items():
        crop_roi(path, os.path.join(args.out, f"{tag}_label_roi.png"), label_box)
        crop_roi(path, os.path.join(args.out, f"{tag}_quad_roi.png"), quad_box)

    log(f"{'PASS' if fail == 0 else 'FAIL'}: {len(plan)-fail}/{len(plan)} boots captured")
    return 1 if fail else 0


if __name__ == "__main__":
    sys.exit(main())
