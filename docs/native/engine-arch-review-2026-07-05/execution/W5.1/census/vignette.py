#!/usr/bin/env python3
"""Catch the tv3_a transition vignette live: nav through part_difficulty into
gameplay-load, dense-screenshot + keep HEADMAT probe log. Census the vignette
poster/menu/flyer meshes (corkboard, menu_*, showtonight_poster)."""
import http.client, json, os, signal, socket, subprocess, sys, time

REPO = "/home/free/code/milohax/rb3"
BIN = os.path.join(REPO, "native", "build-agent-CS1", "rb3-native")
DATA = os.path.join(REPO, "orig-assets", "extracted")
OUT = "/tmp/w51-census"
# Go all the way: part:guitar, diff:expert -> triggers gameplay-load transition
NAV = ("@10:start,@30:confirm,@140:select:pn_quickplay.btn,"
       "@220:select:qp_quickplay.btn,@320:down,"
       "@350:msg:music_library:select_highlighted_node,"
       "@620:part:guitar,@700:diff:expert")

def free_port():
    s = socket.socket(); s.bind(("127.0.0.1", 0)); p = s.getsockname()[1]; s.close(); return p
def hget(port, path, t=20):
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=t)
    try: c.request("GET", path); r = c.getresponse(); return r.status, r.read()
    finally: c.close()
def health(port):
    try:
        st, b = hget(port, "/api/health")
        if st != 200: return None
        d = json.loads(b.decode())["data"]
        return int(d["frame"]), float(d["songMs"]), str(d["currentScreen"])
    except Exception: return None
def shot(port, path):
    st, data = hget(port, "/api/screenshot")
    if st != 200 or data[:8] != b"\x89PNG\r\n\x1a\n": return False
    open(path, "wb").write(data); return True

VIG = ("corkboard", "menu_0", "showtonight", "flyer_", "restaurant_sign",
       "bill_on_pole", "brick_wall", "bus_schedule")

def main():
    os.makedirs(OUT, exist_ok=True)
    port = free_port()
    logp = os.path.join(OUT, f"vignette_{port}.log"); logf = open(logp, "w")
    env = dict(os.environ)
    env.update({"RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
                "MILO_HEADLESS": "1", "RB3_DATA": DATA, "RB3_GAME_INPUT": NAV,
                "RB3_FIXED_CLOCK": "1", "RB3_HEADMAT_DBG": "1"})
    env.pop("RB3_PP_LUMA_CEILING", None)
    proc = subprocess.Popen([BIN], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)
    seen_vig = False
    try:
        dl = time.time() + 60
        while time.time() < dl and health(port) is None:
            if proc.poll() is not None: print("died early"); return 1
            time.sleep(0.4)
        # dense capture for ~90s across all transitions; snapshot log growth
        start = time.time(); last_screen = None; i = 0
        while time.time() - start < 120:
            if proc.poll() is not None: break
            h = health(port)
            if h:
                f, m, s = h
                if s != last_screen:
                    print(f"[vig] frame={f} songMs={m:.0f} screen={s}", flush=True)
                    last_screen = s
                    p = os.path.join(OUT, f"vig_{i:03d}_{s}.png"); shot(port, p); i += 1
                # check log for vignette meshes
                if not seen_vig:
                    try:
                        with open(logp) as lf:
                            txt = lf.read()
                        if any(v in txt for v in VIG):
                            seen_vig = True
                            print(f"[vig] VIGNETTE MESHES SEEN at frame={f} screen={s}", flush=True)
                            shot(port, os.path.join(OUT, f"vig_HIT_{f}.png"))
                    except Exception: pass
            time.sleep(0.25)
        print(f"[vig] done seen_vignette={seen_vig} log={logp}", flush=True)
        return 0
    finally:
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
            try: proc.wait(timeout=8)
            except subprocess.TimeoutExpired: os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
        except Exception: pass
        logf.close()

if __name__ == "__main__":
    sys.exit(main())
