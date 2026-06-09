#!/usr/bin/env python3
"""
capture_song_gameplay.py — drive rb3-native headless into GAMEPLAY of a NAMED song
(by shortname) and dump the post-mix audio to a WAV.

Unlike capture_gameplay_audio.py (which lands on whatever the first `down` selects),
this scrolls the Music Library until {music_library get_highlighted_node} resolves to
the requested song shortname, then selects it and plays it on guitar/expert with
nofail+autohit so all stems sum through AudioDevice::MixSources.

Usage:
    python3 scripts/native/capture_song_gameplay.py SONG_SHORTNAME OUT.wav [--secs N]
"""
import argparse, http.client, json, os, signal, socket, subprocess, sys, time

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DEFAULT_BIN = os.path.join(REPO, "native", "build-native", "rb3-native")
DEFAULT_DATA = os.path.join(REPO, "orig-assets", "extracted")

# Boot -> main_hub -> PLAY NOW -> QUICKPLAY -> song_select. (No down/select yet —
# we do song targeting over HTTP once we are in song_select.)
NAV_TO_SELECT = ("@10:start,@30:confirm,"
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
        if st != 200:
            return None
        d = json.loads(b)["data"]
        return int(d["frame"]), float(d["songMs"]), str(d["currentScreen"])
    except Exception:
        return None


def dta(port, expr):
    try:
        st, b = http_post(port, "/api/dta/eval", expr)
        if st != 200:
            return None
        d = json.loads(b)
        if not d.get("ok"):
            return None
        return d["data"].get("value")
    except Exception:
        return None


def log(m): print(f"[song-cap] {m}", flush=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("song")
    ap.add_argument("out")
    ap.add_argument("--secs", type=int, default=22)
    ap.add_argument("--port", type=int, default=0)
    ap.add_argument("--bin", default=DEFAULT_BIN)
    ap.add_argument("--data", default=DEFAULT_DATA)
    ap.add_argument("--max-downs", type=int, default=120)
    args = ap.parse_args()

    out = os.path.abspath(args.out)
    if os.path.exists(out):
        os.remove(out)
    port = args.port or free_port()
    log_path = f"/tmp/rb3-song-cap-{port}.log"
    logf = open(log_path, "w")

    env = dict(os.environ)
    env.update({
        "RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
        "MILO_HEADLESS": "1", "MILO_AUDIO": "1", "MILO_AUDIO_BACKEND": "null",
        "DC3_DUMP_AUDIO": out, "DC3_DUMP_SECONDS": str(args.secs),
        "RB3_DATA": args.data, "RB3_GAME_INPUT": NAV_TO_SELECT,
    })
    log(f"launching (port {port}), log -> {log_path}")
    proc = subprocess.Popen([args.bin], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)
    rc = 1
    try:
        # Wait for song_select_screen.
        dl = time.time() + 180
        while time.time() < dl:
            if proc.poll() is not None:
                log(f"FAIL proc exited {proc.returncode} before song_select"); return 1
            h = health(port)
            if h and h[2] == "song_select_screen":
                break
            time.sleep(0.5)
        else:
            log("FAIL never reached song_select"); return 1
        time.sleep(1.5)
        log(f"song_select reached; scrolling to '{args.song}'")

        # Scroll down until the highlighted node's token == our song shortname.
        # {{music_library get_highlighted_node} get_token} yields the SortNode's
        # song shortname (e.g. 'beastandtheharlot'); header rows yield 'A'/'B'/...
        target = args.song.strip().lower()
        found = False
        last = None
        for i in range(args.max_downs):
            hl = dta(port, "{{music_library get_highlighted_node} get_token}")
            hls = ("" if hl is None else str(hl)).strip().lower()
            if hls != last:
                log(f"  down {i}: token='{hl}'")
                last = hls
            if hls == target:
                found = True
                log(f"  MATCH on '{hl}' after {i} downs")
                break
            http_post(port, "/api/input", "down")
            time.sleep(0.18)
        if not found:
            log(f"WARN never highlighted '{args.song}' in {args.max_downs} downs; "
                f"last='{last}' — aborting"); return 2

        # Select it + drive into gameplay, mirroring capture_gameplay_audio.py's
        # proven frame-sequenced ordering (select -> settle on part_difficulty ->
        # track/diff -> end_override -> nofail/autohit). Firing track: before the
        # part screen exists trips a SongData::TrackInfo OOB abort, so we WAIT for
        # part_difficulty_screen first and space the verbs out.
        http_post(port, "/api/input", "msg:music_library:select_highlighted_node")
        # wait for the part-difficulty screen (overshell SongSettings flow).
        dl = time.time() + 60
        while time.time() < dl:
            if proc.poll() is not None:
                log(f"FAIL proc exited {proc.returncode} during song load"); return 1
            h = health(port)
            if h and h[2] == "part_difficulty_screen":
                break
            time.sleep(0.4)
        log("part_difficulty reached; committing guitar/expert")
        time.sleep(1.0)
        http_post(port, "/api/input", "track:guitar"); time.sleep(0.4)
        http_post(port, "/api/input", "difficulty:expert"); time.sleep(0.4)
        http_post(port, "/api/input", "msg:overshell:end_override_flow:1:0"); time.sleep(0.6)
        http_post(port, "/api/input", "nofail"); time.sleep(0.3)
        http_post(port, "/api/input", "autohit"); time.sleep(0.3)

        # Wait for gameplay.
        dl = time.time() + 120
        started = None
        while time.time() < dl:
            if proc.poll() is not None:
                log(f"FAIL proc exited {proc.returncode} before gameplay"); return 1
            h = health(port)
            if h and h[1] > GAMEPLAY_SONGMS:
                started = h
                break
            time.sleep(0.5)
        if not started:
            log("FAIL never reached gameplay"); return 1
        log(f"gameplay underway: frame={started[0]} songMs={started[1]:.0f} screen={started[2]}")

        wait_dl = time.time() + args.secs + 25
        while time.time() < wait_dl:
            if proc.poll() is not None:
                break
            time.sleep(1.0)
        time.sleep(2)
        sz = os.path.getsize(out) if os.path.exists(out) else 0
        log(f"captured {out} ({sz} bytes)")
        rc = 0 if sz > 1000 else 1
        return rc
    finally:
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
            try: proc.wait(timeout=8)
            except subprocess.TimeoutExpired: os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
        except Exception:
            pass
        logf.close()
        log(f"engine log: {log_path}")


if __name__ == "__main__":
    sys.exit(main())
