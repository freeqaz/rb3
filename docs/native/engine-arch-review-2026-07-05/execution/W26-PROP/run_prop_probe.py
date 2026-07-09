#!/usr/bin/env python3
"""W26-PROP STEP-0 discriminator harness: boot rb3-native headless to in-song
gameplay with the prop-bone probes armed, capture stderr, dump the log.

Reuses capture_song_gameplay.py's proven nav sequence. Env passthrough lets the
caller arm IK_PROP_DBG / IK_ROOTCMP / IK_TGT_DBG / RB3_IK_REACH_CLAMP* / RB3_IK_CLAMP_DBG.
"""
import http.client, json, os, signal, socket, subprocess, sys, time

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "..", "..", ".."))
BIN = os.path.join(REPO, "native", "build-native", "rb3-native")
DATA = os.path.join(REPO, "orig-assets", "extracted")
NAV = ("@10:start,@30:confirm,"
       "@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn")
GAMEPLAY_SONGMS = 2000.0


def free_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.bind(("127.0.0.1", 0)); p = s.getsockname()[1]; s.close()
    return p


def http_get(port, path, timeout=6):
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=timeout)
    try:
        c.request("GET", path); r = c.getresponse(); return r.status, r.read()
    finally:
        c.close()


def http_post(port, path, body, timeout=10):
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=timeout)
    try:
        c.request("POST", path, body=body, headers={"Content-Type": "text/plain"})
        r = c.getresponse(); return r.status, r.read().decode("utf-8", "replace")
    finally:
        c.close()


def health(port):
    try:
        st, b = http_get(port, "/api/health")
        if st != 200: return None
        d = json.loads(b)["data"]
        return int(d["frame"]), float(d["songMs"]), str(d["currentScreen"])
    except Exception:
        return None


def dta(port, expr):
    try:
        st, b = http_post(port, "/api/dta/eval", expr)
        if st != 200: return None
        d = json.loads(b)
        if not d.get("ok"): return None
        return d["data"].get("value")
    except Exception:
        return None


def log(m): print(f"[prop-probe] {m}", flush=True)


def main():
    song = sys.argv[1] if len(sys.argv) > 1 else "beastandtheharlot"
    out_log = sys.argv[2] if len(sys.argv) > 2 else "/tmp/prop-probe.log"
    play_secs = int(sys.argv[3]) if len(sys.argv) > 3 else 18
    port = free_port()
    logf = open(out_log, "w")
    env = dict(os.environ)
    env.update({
        "RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
        "RB3_FIXED_CLOCK": "1",
        "MILO_HEADLESS": "1", "MILO_AUDIO": "1", "MILO_AUDIO_BACKEND": "null",
        "RB3_DATA": DATA, "RB3_GAME_INPUT": NAV,
    })
    log(f"launch port={port} song={song} log={out_log}")
    for k in ("IK_PROP_DBG", "IK_ROOTCMP", "IK_TGT_DBG", "RB3_IK_REACH_CLAMP",
              "RB3_IK_REACH_CLAMP_OFF", "RB3_IK_CLAMP_DBG", "RB3_IK_REACH_K",
              "RB3_PROP_POSE", "RB3_PROP_POSE_DBG"):
        if k in os.environ:
            log(f"  env {k}={os.environ[k]}")
    proc = subprocess.Popen([BIN], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)
    rc = 1
    try:
        dl = time.time() + 180
        while time.time() < dl:
            if proc.poll() is not None:
                log(f"FAIL exited {proc.returncode} before song_select"); return 1
            h = health(port)
            if h and h[2] == "song_select_screen": break
            time.sleep(0.5)
        else:
            log("FAIL never reached song_select"); return 1
        time.sleep(1.5)
        target = song.strip().lower()
        found = False; last = None
        for i in range(120):
            hl = dta(port, "{{music_library get_highlighted_node} get_token}")
            hls = ("" if hl is None else str(hl)).strip().lower()
            if hls != last:
                log(f"  down {i}: token='{hl}'"); last = hls
            if hls == target:
                found = True; log(f"  MATCH '{hl}' after {i} downs"); break
            http_post(port, "/api/input", "down"); time.sleep(0.18)
        if not found:
            log(f"WARN never highlighted '{song}'; last='{last}'"); return 2
        http_post(port, "/api/input", "msg:music_library:select_highlighted_node")
        dl = time.time() + 60
        while time.time() < dl:
            if proc.poll() is not None:
                log(f"FAIL exited {proc.returncode} during load"); return 1
            h = health(port)
            if h and h[2] == "part_difficulty_screen": break
            time.sleep(0.4)
        time.sleep(1.0)
        http_post(port, "/api/input", "part:guitar"); time.sleep(1.0)
        http_post(port, "/api/input", "diff:expert"); time.sleep(1.0)
        http_post(port, "/api/input", "nofail"); time.sleep(0.3)
        http_post(port, "/api/input", "autohit"); time.sleep(0.3)
        dl = time.time() + 150
        started = None
        while time.time() < dl:
            if proc.poll() is not None:
                log(f"FAIL exited {proc.returncode} before gameplay"); return 1
            h = health(port)
            if h and h[1] > GAMEPLAY_SONGMS:
                started = h; break
            time.sleep(0.5)
        if not started:
            log("FAIL never reached gameplay"); return 1
        log(f"gameplay: frame={started[0]} songMs={started[1]:.0f}")
        # let the prop bones animate through a few beats
        end = time.time() + play_secs
        while time.time() < end:
            if proc.poll() is not None: break
            time.sleep(1.0)
        rc = 0
        return rc
    finally:
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
            try: proc.wait(timeout=8)
            except subprocess.TimeoutExpired: os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
        except Exception: pass
        logf.close()
        log(f"done rc={rc}; log -> {out_log}")


if __name__ == "__main__":
    sys.exit(main())
