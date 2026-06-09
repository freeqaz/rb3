#!/usr/bin/env python3
"""
closet-reach-test.py — empirical de-risk for the C11 closet route.

Boots rb3-native (RB3_GUEST_PROFILE=1 RB3_CHAR_PREVIEW=1), reaches main_hub,
toggles gPrefabIsCustomizable via {prefab_toggle_customizable}, re-checks
is_char_customizable, then directly fires the customize_character flow handler
and reports the resulting screen (customize_clothing_* = closet reached;
a crash/assert = the GetBandCharDesc/preview path needs more). Read-only.
"""
import http.client, json, os, signal, socket, subprocess, sys, time

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
BIN = os.path.join(REPO, "native", "build-native", "rb3-native")
DATA = os.path.join(REPO, "orig-assets", "extracted")
OVERLAY = os.path.join(REPO, "native", "dta")
START, CONFIRM, DDOWN = 11, 6, 14


def free_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM); s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]; s.close(); return p

def get(port, path, t=5):
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
        st, b = get(port, "/api/health")
        if st != 200: return None
        d = json.loads(b.decode("utf-8", "replace"))["data"]
        return int(d["frame"]), str(d["currentScreen"])
    except Exception: return None

def evald(port, expr):
    try:
        st, b = post(port, "/api/dta/eval", expr)
        if st != 200: return f"HTTP {st}"
        d = json.loads(b)
        return d["data"].get("value") if d.get("ok") else f"ERR {d.get('error')}"
    except Exception as e: return f"EXC {e}"

def alive(p): return p.poll() is None


def main():
    port = free_port()
    env = dict(os.environ)
    env.update({"RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
                "MILO_HEADLESS": "1", "RB3_DATA": DATA, "RB3_DTA_OVERLAY": OVERLAY,
                "RB3_GUEST_PROFILE": "1", "RB3_CHAR_PREVIEW": "1"})
    for kv in sys.argv[1:]:
        if "=" in kv: k, v = kv.split("=", 1); env[k] = v
    logp = f"/tmp/rb3-closetreach-{port}.log"
    log = open(logp, "w")
    proc = subprocess.Popen([BIN], env=env, stdout=log, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)
    try:
        dl = time.time() + 40
        while time.time() < dl and alive(proc) and not health(port):
            time.sleep(0.4)
        for _ in range(14):
            h = health(port)
            if h and h[1] == "main_hub_screen": break
            post(port, "/api/input", f"pad:{START}"); time.sleep(0.7)
        print(f"[1] at {health(port)}")
        u = "{user_mgr get_user_from_pad_num 0}"
        print(f"[2] is_char_customizable (baseline) = {evald(port, '{'+u+' is_char_customizable}')}")
        print(f"[3] toggle prefab_customizable      = {evald(port, '{prefab_toggle_customizable}')}")
        print(f"[4] is_char_customizable (after)    = {evald(port, '{'+u+' is_char_customizable}')}")
        print(f"[5] get_char                        = {evald(port, '{'+u+' get_char}')}")
        # Fire the customize_character flow directly via DTA (mirrors main_hub.dta customize_character.btn true branch)
        print("[6] firing closet route: set_critical_user + closet_mgr set_user + goto customize_clothing_enter_screen")
        evald(port, f"{{critical_user_listener set_critical_user {u}}}")
        evald(port, f"{{closet_mgr set_user {u}}}")
        evald(port, "{ui goto_screen customize_clothing_enter_screen}")
        shotdir = "/tmp/rb3-closet"; os.makedirs(shotdir, exist_ok=True)
        for i in range(20):
            if not alive(proc):
                print(f"[!] PROCESS DIED after closet route (frame loop {i}) — check {logp}")
                break
            h = health(port)
            extra = ""
            if h and h[1] == "customize_clothing_screen":
                extra = f"  char_probe0={evald(port, '{rb3_char_probe 0}')}"
                if i in (10, 15, 19):
                    st, data = get(port, "/api/screenshot", t=25)
                    if st == 200 and data[:8] == b"\x89PNG\r\n\x1a\n":
                        sp = os.path.join(shotdir, f"closet_{i:02d}.png")
                        open(sp, "wb").write(data); extra += f"  shot={sp}"
            print(f"   t+{i}: alive={alive(proc)} screen={h[1] if h else '?'}{extra}")
            time.sleep(0.5)
        # tail interesting log lines
        print("\n--- log tail (asserts/fails/closet) ---")
        with open(logp) as f: lines = f.readlines()
        for ln in lines:
            if any(k in ln for k in ("ASSERT", "FAIL", "closet", "Closet", "MILO_WARN", "PreviewChar", "GetBandCharDesc", "char_cache", "customize")):
                sys.stdout.write("   " + ln)
        sys.stdout.write("".join("   " + l for l in lines[-8:]))
    finally:
        try: os.killpg(os.getpgid(proc.pid), signal.SIGTERM); proc.wait(timeout=8)
        except Exception:
            try: os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
            except Exception: pass
        log.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
