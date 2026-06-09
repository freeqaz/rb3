#!/usr/bin/env python3
"""
closet-anim-verify.py — prove the closet character STANDS + ANIMATES.

Boots rb3-native (guest profile + char preview), reaches the closet via the real
customize_character flow, captures a dense screenshot burst, and pixel-diffs
consecutive frames. A moving (idle-breathing/swaying) character => non-trivial
mean abs diff over time; a frozen/absent character => near-zero diff.
Also samples rb3_char_probe for skinned-mesh count across slots 0..3.
"""
import http.client, json, os, signal, socket, subprocess, sys, time
import numpy as np
from PIL import Image
import io

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
BIN = os.path.join(REPO, "native", "build-native", "rb3-native")
DATA = os.path.join(REPO, "orig-assets", "extracted")
OVERLAY = os.path.join(REPO, "native", "dta")
START = 11
SHOTDIR = "/tmp/rb3-closet-anim"


def free_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM); s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]; s.close(); return p

def get(port, path, t=25):
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=t)
    try: c.request("GET", path); r = c.getresponse(); return r.status, r.read()
    finally: c.close()

def post(port, path, body, t=10):
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=t)
    try:
        c.request("POST", path, body=body, headers={"Content-Type": "text/plain"})
        r = c.getresponse(); return r.status, r.read().decode("utf-8", "replace")
    finally: c.close()

def health(port):
    try:
        st, b = get(port, "/api/health", t=5)
        if st != 200: return None
        d = json.loads(b.decode("utf-8", "replace"))["data"]
        return int(d["frame"]), str(d["currentScreen"])
    except Exception: return None

def evald(port, expr):
    try:
        st, b = post(port, "/api/dta/eval", expr)
        if st != 200: return f"HTTP {st}"
        d = json.loads(b); return d["data"].get("value") if d.get("ok") else f"ERR {d.get('error')}"
    except Exception as e: return f"EXC {e}"

def alive(p): return p.poll() is None

def shot_arr(port):
    st, data = get(port, "/api/screenshot")
    if st != 200 or data[:8] != b"\x89PNG\r\n\x1a\n": return None, None
    img = Image.open(io.BytesIO(data)).convert("RGB")
    return np.asarray(img, dtype=np.int16), data


def main():
    os.makedirs(SHOTDIR, exist_ok=True)
    port = free_port()
    env = dict(os.environ)
    env.update({"RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
                "MILO_HEADLESS": "1", "RB3_DATA": DATA, "RB3_DTA_OVERLAY": OVERLAY,
                "RB3_GUEST_PROFILE": "1", "RB3_CHAR_PREVIEW": "1"})
    for kv in sys.argv[1:]:
        if "=" in kv: k, v = kv.split("=", 1); env[k] = v
    log = open(f"/tmp/rb3-closetanim-{port}.log", "w")
    proc = subprocess.Popen([BIN], env=env, stdout=log, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)
    try:
        dl = time.time() + 40
        while time.time() < dl and alive(proc) and not health(port): time.sleep(0.4)
        for _ in range(14):
            h = health(port)
            if h and h[1] == "main_hub_screen": break
            post(port, "/api/input", f"pad:{START}"); time.sleep(0.7)
        # open closet (the guest install already flips prefab-customizable + primary profile)
        u = "{user_mgr get_user_from_pad_num 0}"
        evald(port, f"{{critical_user_listener set_critical_user {u}}}")
        evald(port, f"{{closet_mgr set_user {u}}}")
        evald(port, "{ui goto_screen customize_clothing_enter_screen}")
        # wait for closet
        dl = time.time() + 20
        while time.time() < dl and alive(proc):
            h = health(port)
            if h and h[1] == "customize_clothing_screen": break
            time.sleep(0.4)
        h = health(port)
        print(f"closet screen: {h}")
        if not (h and h[1] == "customize_clothing_screen"):
            print("FAIL: never reached closet"); return 1
        # let menu text + load settle so it doesn't confound the motion diff
        time.sleep(4.0)
        for s in range(4):
            print(f"  char_probe[{s}] = {evald(port, '{rb3_char_probe '+str(s)+'}')}")
        # dense burst
        frames, raws = [], []
        N = 16
        for i in range(N):
            a, raw = shot_arr(port)
            if a is not None:
                frames.append(a); raws.append(raw)
                if i in (0, N//2, N-1):
                    open(os.path.join(SHOTDIR, f"f{i:02d}.png"), "wb").write(raw)
            time.sleep(0.35)
        print(f"\ncaptured {len(frames)} frames")
        if len(frames) < 3:
            print("FAIL: too few frames"); return 1
        # consecutive mean abs diff (whole frame) — UI is static so motion = char
        base = frames[0]
        diffs = []
        for i in range(1, len(frames)):
            d = np.abs(frames[i] - frames[i-1]).mean()
            diffs.append(d)
        vs0 = [np.abs(frames[i] - base).mean() for i in range(1, len(frames))]
        print(f"consecutive-frame mean|Δ|: min={min(diffs):.3f} max={max(diffs):.3f} avg={sum(diffs)/len(diffs):.3f}")
        print(f"vs-frame0      mean|Δ|: max={max(vs0):.3f}")
        # focus on a TORSO/LEGS region (exclude left menu text AND the head shards)
        H, W, _ = base.shape
        roi = (slice(int(H*0.40), int(H*0.92)), slice(int(W*0.50), int(W*0.88)))
        rdiffs = [np.abs(frames[i][roi] - frames[i-1][roi]).mean() for i in range(1, len(frames))]
        # head region separately (top-center) to see shard flicker vs body
        hroi = (slice(int(H*0.05), int(H*0.38)), slice(int(W*0.55), int(W*0.85)))
        hdiffs = [np.abs(frames[i][hroi] - frames[i-1][hroi]).mean() for i in range(1, len(frames))]
        print(f"ROI(torso/legs) consecutive mean|Δ|: max={max(rdiffs):.3f} avg={sum(rdiffs)/len(rdiffs):.3f}")
        print(f"ROI(head)       consecutive mean|Δ|: max={max(hdiffs):.3f} avg={sum(hdiffs)/len(hdiffs):.3f}")
        verdict = "ANIMATING" if max(rdiffs) > 0.5 else "STATIC/FROZEN"
        print(f"\nVERDICT: body region is {verdict}  (threshold 0.5)")
        print(f"sample shots in {SHOTDIR}")
    finally:
        try: os.killpg(os.getpgid(proc.pid), signal.SIGTERM); proc.wait(timeout=8)
        except Exception:
            try: os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
            except Exception: pass
        log.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
