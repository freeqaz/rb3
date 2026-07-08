#!/usr/bin/env python3
"""W22-SWEEP: boot to song_select w/ DRAWLOG_PROV, query ROIs to NAME right-edge
avatar + red-bar + diff-panel elements. Scripts+docs-only lane helper."""
import http.client, json, os, signal, socket, subprocess, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import uidump_query as U

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
BIN = os.path.join(REPO, "native", "build-native", "rb3-native")
DATA = os.path.join(REPO, "orig-assets", "extracted")
NAV = ("@10:start,@30:confirm,@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn")

def free_port():
    s = socket.socket(); s.bind(("127.0.0.1", 0)); p = s.getsockname()[1]; s.close(); return p

def get(port, path, t=25):
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=t)
    try: c.request("GET", path); r = c.getresponse(); return r.status, r.read()
    finally: c.close()

def health(port):
    try:
        st, b = get(port, "/api/health")
        if st != 200: return None
        d = json.loads(b.decode("utf-8", "replace"))["data"]
        return int(d["frame"]), str(d["currentScreen"])
    except Exception: return None

port = free_port()
log = open(f"/tmp/w22-roi-{port}.log", "w")
env = dict(os.environ)
env.update({"RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
            "MILO_HEADLESS": "1", "RB3_DATA": DATA, "RB3_FIXED_CLOCK": "1",
            "RB3_DRAWLOG_PROV": "1", "RB3_GAME_INPUT": NAV})
proc = subprocess.Popen([BIN], env=env, stdout=log, stderr=subprocess.STDOUT,
                        cwd=REPO, start_new_session=True)
# ROIs (X,Y,W,H) in 1280x720: right-edge avatar, stray red bar, diff-dot panel
ROIS = {
    "right_edge_avatar": (1080, 360, 200, 300),
    "avatar_hand_right": (1180, 420, 100, 140),
    "stray_red_bar":     (845, 120, 30, 240),
    "diffdot_panel":     (900, 380, 300, 200),
}
try:
    dl = time.time() + 130; reached = False
    while time.time() < dl:
        if proc.poll() is not None:
            print(f"FAIL: proc exited {proc.returncode}"); break
        h = health(port)
        if h and h[1] == "song_select_screen":
            reached = True; break
        time.sleep(0.5)
    if not reached:
        print("FAIL: never reached song_select"); sys.exit(1)
    time.sleep(3.0)  # settle list + avatar
    authored = U.authored_index(U.fetch_uidump(port))
    for label, roi in ROIS.items():
        print(f"\n===== ROI[{label}] = {roi} =====", flush=True)
        try:
            doc = U.fetch_roi(port, list(map(float, roi)))
            U.print_roi(doc, authored, list(map(float, roi)))
        except Exception as e:
            print(f"  ROI query error: {e}")
    print("\nDONE")
finally:
    try:
        os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
        try: proc.wait(timeout=8)
        except subprocess.TimeoutExpired: os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
    except Exception: pass
    log.close()
