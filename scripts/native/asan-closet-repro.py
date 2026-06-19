#!/usr/bin/env python3
"""asan-closet-repro.py — drive rb3-native (ASan) to the customize closet and
capture any heap-buffer-overflow report. Default-on guest + char-preview.
"""
import http.client, json, os, signal, socket, subprocess, sys, time

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
BIN = os.environ.get("RB3_BIN", os.path.join(REPO, "native", "build-fu-asan", "rb3-native"))
DATA = os.environ.get("RB3_DATA", os.path.join(REPO, "orig-assets", "extracted"))
OVERLAY = os.path.join(REPO, "native", "dta")
START, CONFIRM, CANCEL, DUP, DDOWN = 11, 6, 5, 12, 14
LOG = os.environ.get("RB3_LOG", "/tmp/rb3-asan-closet.log")


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

def scr(port):
    h = health(port); return h[1] if h else "?"

def press(port, bit, wait=0.7):
    try: post(port, "/api/input", f"pad:{bit}")
    except Exception: pass
    time.sleep(wait)

def alive(p): return p.poll() is None


def main():
    port = free_port()
    env = dict(os.environ)
    env.update({"RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
                "MILO_HEADLESS": "1", "RB3_DATA": DATA, "RB3_DTA_OVERLAY": OVERLAY,
                "RB3_GUEST_PROFILE": "1", "RB3_CHAR_PREVIEW": "1"})
    # ASan: keep running past first error so we capture the WRITE site.
    env["ASAN_OPTIONS"] = ("halt_on_error=0:alloc_dealloc_mismatch=0:"
                           "detect_leaks=0:abort_on_error=0:print_stats=0")
    # GPU off (NVIDIA ASan time() interceptor crashes); Dawn -> Null adapter.
    env.update({"VK_LOADER_DRIVERS_DISABLE": "*",
                "__EGL_VENDOR_LIBRARY_FILENAMES": "/usr/share/glvnd/egl_vendor.d/50_mesa.json",
                "EGL_PLATFORM": "surfaceless", "LIBGL_ALWAYS_SOFTWARE": "1"})
    for kv in sys.argv[1:]:
        if "=" in kv: k, v = kv.split("=", 1); env[k] = v
    log = open(LOG, "w")
    proc = subprocess.Popen([BIN], env=env, stdout=log, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)
    try:
        dl = time.time() + 90
        while time.time() < dl and alive(proc) and not health(port): time.sleep(0.5)
        if not health(port):
            print(f"[!] never came up (alive={alive(proc)})"); return 1
        for _ in range(16):
            if scr(port) == "main_hub_screen": break
            press(port, START)
        print(f"[main_hub] screen={scr(port)}")
        time.sleep(1.0)
        for i in range(3):
            press(port, DDOWN, 0.5); print(f"  down{i+1}: {scr(port)}")
        press(port, CONFIRM, 1.0); print(f"  confirm(customize): {scr(port)}")
        press(port, DDOWN, 0.5); print(f"  down(to char): {scr(port)}")
        press(port, CONFIRM, 1.2); print(f"  confirm(char): {scr(port)}")
        os.makedirs("/tmp/rb3-asan-closet", exist_ok=True)
        reached = False
        for i in range(40):
            if not alive(proc):
                print(f"[!] DIED at loop {i}"); break
            s = scr(port)
            print(f"   t+{i}: {s}")
            if s == "customize_clothing_screen":
                reached = True
            time.sleep(0.5)
        print(f"\nRESULT: {'REACHED CLOSET' if reached else 'did NOT reach closet'}  alive={alive(proc)}")
    finally:
        try: os.killpg(os.getpgid(proc.pid), signal.SIGTERM); proc.wait(timeout=8)
        except Exception:
            try: os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
            except Exception: pass
        log.close()
    # report ASan findings
    with open(LOG) as f: txt = f.read()
    print("\n=== ASan / error scan ===")
    hits = [ln for ln in txt.splitlines()
            if any(k in ln for k in ("ERROR: AddressSanitizer", "heap-buffer-overflow",
                   "WRITE of size", "READ of size", "is located", "allocated by",
                   "SUMMARY: AddressSanitizer"))]
    for ln in hits[:60]:
        print("  " + ln)
    if not hits:
        print("  (no ASan reports found)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
