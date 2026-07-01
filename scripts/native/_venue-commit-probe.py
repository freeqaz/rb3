#!/usr/bin/env python3
"""Probe WHEN the quickplay venue commits relative to the texture-reveal frame.

Drives boot -> song_select -> part_difficulty -> game_screen, and at each phase
queries MetaPerformer venue state over /api/dta/eval, timestamping each screen
transition. VENUE_DBG=1 makes BandDirector::EnterVenue log the venue it actually
force-loads. We diff:
  - {meta_performer get_venue}          (what the prototype prewarm reads)
  - {meta_performer get_venue_override} (what EnterVenue actually honors natively)
against the venue EnterVenue logs, and measure the dwell from each to game_screen.
"""
import http.client, json, os, signal, socket, subprocess, sys, time

REPO = "/home/free/code/milohax/rb3/.claude/worktrees/texwarm-venuepredict"
BIN = REPO + "/native/build-native/rb3-native"
DATA = REPO + "/orig-assets/extracted"
NAV = "@10:start,@30:confirm,@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn"
SONG = sys.argv[1] if len(sys.argv) > 1 else "20thcenturyboy"

def free_port():
    s = socket.socket(); s.bind(("127.0.0.1", 0)); p = s.getsockname()[1]; s.close(); return p

def http_get(port, path):
    try:
        c = http.client.HTTPConnection("127.0.0.1", port, timeout=5)
        c.request("GET", path); r = c.getresponse(); b = r.read(); c.close()
        return r.status, b
    except Exception: return None, None

def http_post(port, path, body):
    try:
        c = http.client.HTTPConnection("127.0.0.1", port, timeout=5)
        c.request("POST", path, body); r = c.getresponse(); b = r.read(); c.close()
        return r.status, b
    except Exception: return None, None

def health(port):
    st, b = http_get(port, "/api/health")
    if st != 200 or not b: return None
    try:
        j = json.loads(b)
        d = j.get("data", j)
        return d.get("frame"), d.get("songMs", 0.0), d.get("currentScreen", "")
    except Exception: return None

def dta(port, expr):
    st, b = http_post(port, "/api/dta/eval", expr)
    if st != 200 or not b: return None
    try:
        j = json.loads(b)
        d = j.get("data", j)
        if isinstance(d, dict) and "value" in d: return d.get("value")
        return d
    except Exception: return b.decode("utf-8", "replace")

def venue_state(port):
    return {
        "get_venue": dta(port, "{meta_performer get_venue}"),
        "get_venue_override": dta(port, "{meta_performer get_venue_override}"),
    }

def main():
    port = free_port()
    logp = f"/tmp/venue-probe-{port}.log"
    logf = open(logp, "w")
    env = dict(os.environ)
    env.update({
        "RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
        "MILO_HEADLESS": "1", "MILO_AUDIO": "1", "MILO_AUDIO_BACKEND": "null",
        "RB3_DATA": DATA, "RB3_GAME_INPUT": NAV,
        "VENUE_DBG": "1", "RB3_TEX_PREWARM_DBG": "1",
        # turn the prewarm OFF so its own re-kick logging doesn't confuse the trace;
        # we just want to know WHEN the real venue commits.
    })
    t0 = time.time()
    def ts(): return f"{time.time()-t0:7.2f}s"
    proc = subprocess.Popen([BIN], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)
    timeline = []
    try:
        dl = time.time() + 180
        while time.time() < dl:
            if proc.poll() is not None:
                print(f"FAIL exited {proc.returncode} before song_select"); return 1
            h = health(port)
            if h and h[2] == "song_select_screen": break
            time.sleep(0.5)
        else:
            print("FAIL never reached song_select"); return 1
        time.sleep(1.5)
        vs = venue_state(port)
        timeline.append((time.time()-t0, "song_select", vs))
        print(f"[{ts()}] song_select  venue={vs}")

        # scroll to target song
        target = SONG.strip().lower(); found = False; last = None
        for i in range(120):
            hl = dta(port, "{{music_library get_highlighted_node} get_token}")
            hls = ("" if hl is None else str(hl)).strip().lower()
            if hls == target: found = True; break
            http_post(port, "/api/input", "down"); time.sleep(0.18)
        print(f"[{ts()}] song scroll done found={found}")

        http_post(port, "/api/input", "msg:music_library:select_highlighted_node")
        dl = time.time() + 60; reached = False
        while time.time() < dl:
            if proc.poll() is not None:
                print(f"FAIL exited during load"); return 1
            h = health(port)
            if h and h[2] == "part_difficulty_screen": reached = True; break
            time.sleep(0.25)
        # sample venue state AT part_difficulty as soon as we see it
        vs = venue_state(port)
        t_part = time.time()-t0
        timeline.append((t_part, "part_difficulty", vs))
        print(f"[{ts()}] part_difficulty  venue={vs}  (load_panels select_random_venue fires here)")
        time.sleep(1.0)
        vs2 = venue_state(port)
        print(f"[{ts()}] part_difficulty+1s  venue={vs2}")

        http_post(port, "/api/input", "track:guitar"); time.sleep(0.4)
        http_post(port, "/api/input", "difficulty:expert"); time.sleep(0.4)
        http_post(port, "/api/input", "msg:overshell:end_override_flow:1:0"); time.sleep(0.3)
        vs = venue_state(port)
        t_confirm = time.time()-t0
        print(f"[{ts()}] song confirmed (end_override)  venue={vs}")
        http_post(port, "/api/input", "nofail"); time.sleep(0.2)
        http_post(port, "/api/input", "autohit"); time.sleep(0.2)

        # poll venue + screen every 150ms until game_screen / songMs flowing,
        # so we catch any re-roll between confirm and reveal.
        dl = time.time() + 120; t_reveal = None; last_screen = None
        last_venue = None
        while time.time() < dl:
            if proc.poll() is not None:
                print(f"FAIL exited before gameplay"); break
            h = health(port)
            if not h: time.sleep(0.15); continue
            v = dta(port, "{meta_performer get_venue}")
            if v != last_venue:
                print(f"[{ts()}] get_venue -> {v}  (screen={h[2]} songMs={h[1]:.0f})")
                last_venue = v
            if h[2] != last_screen:
                print(f"[{ts()}] screen -> {h[2]}  songMs={h[1]:.0f} venue={v}")
                last_screen = h[2]
            if h[1] > 100.0:  # gameplay underway = reveal happened
                t_reveal = time.time()-t0
                print(f"[{ts()}] REVEAL/gameplay songMs={h[1]:.0f} screen={h[2]}")
                break
            time.sleep(0.15)

        print("\n==== DWELL SUMMARY ====")
        if t_reveal is not None:
            print(f"part_difficulty(load_panels venue commit) -> reveal:  {t_reveal - t_part:6.2f}s")
            print(f"song-confirm(end_override)                 -> reveal:  {t_reveal - t_confirm:6.2f}s")
        else:
            print("never reached reveal")
    finally:
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
            try: proc.wait(timeout=8)
            except subprocess.TimeoutExpired: os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
        except Exception: pass
        logf.close()
    print(f"\nengine log: {logp}")
    print("---- VENUE_DBG lines from engine log ----")
    try:
        with open(logp) as f:
            for line in f:
                if "VENUE_DBG" in line or "LOADING VENUE" in line or "select_random_venue" in line:
                    print(line.rstrip())
    except Exception as e: print("(log read failed)", e)
    return 0

sys.exit(main())
