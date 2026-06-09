#!/usr/bin/env python3
"""
profile-gate-probe.py — boot rb3-native with RB3_GUEST_PROFILE=1, reach main_hub,
and eval the customize-gate predicates to build a runtime truth table. Read-only
(no build). Tells us exactly which DTA gate(s) the guest profile fails to flip.
"""
import http.client, json, os, signal, socket, subprocess, sys, time

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
BIN = os.path.join(REPO, "native", "build-native", "rb3-native")
DATA = os.path.join(REPO, "orig-assets", "extracted")
OVERLAY = os.path.join(REPO, "native", "dta")
START = 11

EXPRS = [
    "{profile_mgr has_primary_profile}",
    "{user_mgr get_user_from_pad_num 0}",
    "{{user_mgr get_user_from_pad_num 0} can_save_data}",
    "{{user_mgr get_user_from_pad_num 0} is_char_customizable}",
    "{{user_mgr get_user_from_pad_num 0} is_local}",
    "{profile_mgr get_profile {user_mgr get_user_from_pad_num 0}}",
    "{{profile_mgr get_profile {user_mgr get_user_from_pad_num 0}} get_num_chars}",
    "{platform_mgr is_signed_in 0}",
    "{platform_mgr is_user_a_guest {user_mgr get_user_from_pad_num 0}}",
]


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


def main():
    port = free_port()
    env = dict(os.environ)
    env.update({"RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
                "MILO_HEADLESS": "1", "RB3_DATA": DATA, "RB3_DTA_OVERLAY": OVERLAY,
                "RB3_GUEST_PROFILE": "1"})
    for kv in sys.argv[1:]:
        if "=" in kv: k, v = kv.split("=", 1); env[k] = v
    log = open(f"/tmp/rb3-gateprobe-{port}.log", "w")
    proc = subprocess.Popen([BIN], env=env, stdout=log, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)
    try:
        dl = time.time() + 40
        while time.time() < dl and proc.poll() is None and not health(port):
            time.sleep(0.4)
        for _ in range(14):
            h = health(port)
            if h and h[1] == "main_hub_screen": break
            post(port, "/api/input", f"pad:{START}"); time.sleep(0.7)
        h = health(port)
        print(f"screen={h[1] if h else 'NONE'} frame={h[0] if h else '?'}\n")
        for e in EXPRS:
            print(f"{e}\n   => {evald(port, e)}")
    finally:
        try: os.killpg(os.getpgid(proc.pid), signal.SIGTERM); proc.wait(timeout=8)
        except Exception:
            try: os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
            except Exception: pass
        log.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
