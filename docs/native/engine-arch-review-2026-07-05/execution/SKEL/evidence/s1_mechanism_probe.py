#!/usr/bin/env python3
"""Wave-13 Lane SKEL S-S1 — mechanism-study read-only probe harness.

Launches the committed W2.8g binary (probes RB3_APD_DIAG / HEAD_REBIND_PROBE /
SKEL_REBIND_PROBE / SKEL_REBAKE_PROBE / RELOAD_PROBE / BAND_ANIM_PROBE, all
render-inert, getenv-gated) into a live gameplay band render and captures stderr.
NO fix, NO behavior change: every flag here only prints.

Answers, from runtime:
 (1) EXISTENCE  — do finger bones resolve to a per-member instance (own!=bound)?
 (2) ANIMATION  — does the Find-resolved finger bone MOVE pre/post Poll?
 (4) WRITER     — is hands_naked rebound by RebindHeadHandsAtRest (HEAD_REBIND_*),
                  GeomOwner-shared (CHAR_MESH shared=1), or left to the engine?
 (5) BAKE-TIME  — APD_DIAG own=%p / bound=%p / basis angles at bake.
"""
import http.client, json, os, re, signal, socket, subprocess, time, sys

REPO = "/home/free/code/milohax/rb3"
BIN = os.path.join(REPO, "native", "build-agent-W2.8g", "rb3-native")
DATA = os.path.join(REPO, "orig-assets", "extracted")
LOG = "/tmp/wave13-skel-s1/gameplay.log"
os.makedirs("/tmp/wave13-skel-s1", exist_ok=True)

NAV_GAMEPLAY = ("@10:start,@30:confirm,@140:select:pn_quickplay.btn,"
    "@220:select:qp_quickplay.btn,@320:down,"
    "@350:msg:music_library:select_highlighted_node,"
    "@380:part:guitar,@410:diff:expert,@440:nofail")

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

def main():
    port = free_port(); logf = open(LOG, "w")
    env = dict(os.environ)
    env.update({"RB3_GAME":"1","RB3_HTTP":"1","RB3_HTTP_PORT":str(port),
        "MILO_HEADLESS":"1","RB3_DATA":DATA,"RB3_GAME_INPUT":NAV_GAMEPLAY,
        "RB3_FIXED_CLOCK":"1",
        "RB3_APD_DIAG":"1",            # own/bound ptr + basis angles for finger bones
        "HEAD_REBIND_PROBE":"1",       # anchor mine/foreign + pending (writer)
        "SKEL_REBIND_PROBE":"1",       # torso rebind magnet->own
        "SKEL_REBAKE_PROBE":"1",       # engine one-time rebake
        "RELOAD_PROBE":"1",            # CHAR_MESH inventory (shared=, rebound=)
        "BAND_ANIM_PROBE":"*",         # per-member bone pre/post Poll motion
        "BAND_ANIM_BONE":"bone_R-middlefinger03.mesh"})
    proc = subprocess.Popen([BIN], env=env, stdout=logf, stderr=subprocess.STDOUT,
        cwd=REPO, start_new_session=True)
    try:
        dl = time.time() + 150
        reached = False
        while time.time() < dl:
            if proc.poll() is not None:
                print("[s1] process exited early", flush=True); break
            h = health(port)
            if h:
                print(f"[s1] frame={h[0]} songMs={h[1]:.0f} screen={h[2]}", flush=True)
                if h[2] == "game_screen" and h[1] >= 4000:
                    reached = True; break
            time.sleep(1.0)
        # let a few gameplay polls emit probe lines
        if reached:
            time.sleep(6)
        print(f"[s1] reached_gameplay={reached}", flush=True)
    finally:
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
            try: proc.wait(timeout=8)
            except subprocess.TimeoutExpired: os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
        except Exception: pass
        logf.close()
    print(f"[s1] log at {LOG} ({os.path.getsize(LOG)} bytes)", flush=True)

if __name__ == "__main__":
    main()
