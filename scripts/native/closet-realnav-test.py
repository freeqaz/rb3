#!/usr/bin/env python3
"""
closet-realnav-test.py — prove the REAL menu nav reaches the closet now that the
guest install flips the gates (no DTA poke). Boots RB3_GUEST_PROFILE=1
RB3_CHAR_PREVIEW=1, reaches main_hub, then navigates the actual menu:
  down*3 -> 'customize' (mb_shop) -> confirm -> submenu
  down*1 -> customize_character    -> confirm -> closet (customize_clothing_screen)
Reports each screen + screenshots the closet.
"""
import http.client, json, os, signal, socket, subprocess, sys, time

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
BIN = os.path.join(REPO, "native", "build-native", "rb3-native")
DATA = os.path.join(REPO, "orig-assets", "extracted")
OVERLAY = os.path.join(REPO, "native", "dta")
START, CONFIRM, CANCEL, DUP, DDOWN = 11, 6, 5, 12, 14


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
    post(port, "/api/input", f"pad:{bit}"); time.sleep(wait)

def alive(p): return p.poll() is None


def main():
    port = free_port()
    env = dict(os.environ)
    env.update({"RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
                "MILO_HEADLESS": "1", "RB3_DATA": DATA, "RB3_DTA_OVERLAY": OVERLAY,
                "RB3_GUEST_PROFILE": "1", "RB3_CHAR_PREVIEW": "1"})
    for kv in sys.argv[1:]:
        if "=" in kv: k, v = kv.split("=", 1); env[k] = v
    log = open(f"/tmp/rb3-realnav-{port}.log", "w")
    proc = subprocess.Popen([BIN], env=env, stdout=log, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)
    try:
        dl = time.time() + 40
        while time.time() < dl and alive(proc) and not health(port): time.sleep(0.4)
        for _ in range(14):
            if scr(port) == "main_hub_screen": break
            press(port, START)
        print(f"[main_hub] screen={scr(port)}")
        if scr(port) != "main_hub_screen":
            print("FAIL: never reached main_hub"); return 1
        time.sleep(1.0)
        # navigate down to 'customize' (mb_shop) then into the submenu
        for i in range(3):
            press(port, DDOWN, 0.5); print(f"  down{i+1}: {scr(port)}")
        press(port, CONFIRM, 1.0); print(f"  confirm(customize): {scr(port)}")
        # submenu focus = customize_band; one down -> customize_character
        press(port, DDOWN, 0.5); print(f"  down(to char): {scr(port)}")
        press(port, CONFIRM, 1.2); print(f"  confirm(char): {scr(port)}")
        # let the closet load
        os.makedirs("/tmp/rb3-realnav", exist_ok=True)
        reached = False
        for i in range(24):
            if not alive(proc):
                print(f"[!] DIED at loop {i}"); break
            s = scr(port)
            print(f"   t+{i}: {s}")
            if s == "customize_clothing_screen":
                reached = True
                if i in (8, 16, 23):
                    st, data = get(port, "/api/screenshot")
                    if st == 200 and data[:8] == b"\x89PNG\r\n\x1a\n":
                        sp = f"/tmp/rb3-realnav/closet_{i:02d}.png"
                        open(sp, "wb").write(data); print(f"      shot={sp}")
            time.sleep(0.5)
        print(f"\nRESULT: {'REACHED CLOSET via real nav' if reached else 'did NOT reach closet'}")
        print("\n--- log tail (asserts/screen) ---")
        with open(log.name) as f: lines = f.readlines()
        for ln in lines:
            if any(k in ln for k in ("ASSERT", "FAIL", "RB3 screen", "guest profile", "0xA2A")):
                sys.stdout.write("   " + ln)
    finally:
        try: os.killpg(os.getpgid(proc.pid), signal.SIGTERM); proc.wait(timeout=8)
        except Exception:
            try: os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
            except Exception: pass
        log.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
