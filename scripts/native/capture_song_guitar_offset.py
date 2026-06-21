#!/usr/bin/env python3
"""
capture_song_guitar_offset.py — drive rb3-native headless into GAMEPLAY of a
chosen song (by number of DOWN presses from the top of the alphabetical song
list) and capture BOTH:
  * the post-mix gameplay audio WAV  (DC3_DUMP_AUDIO)
  * the per-channel decoded stems    (RB3_DUMP_STEMS dir -> stem_NN.s16 + stems.json)

This lets an offline tool measure (a) whether the mixed guitar content lines up
in time with the chart's note events, and (b) whether the decoded stems are
sample-aligned across channels (they should be — the decode shares one cursor).

The whole point: diagnose the "guitar audio 10s off the chart" report. We pick a
song by DOWN count because DTA song-name introspection is awkward over the debug
API; 25 or 6 to 4 is 3 downs from the top (after the "123" category header and
"20th Century Boy").

Usage:
  python3 scripts/native/capture_song_guitar_offset.py OUT.wav STEMDIR \
      [--downs N] [--secs N] [--port P] [--gain G]

Diagnostic-only. Does NOT modify game behavior beyond the env knobs the engine
already honors (RB3_DUMP_STEMS, DC3_DUMP_AUDIO). Mirrors capture_gameplay_audio.py.
"""
import argparse, http.client, json, os, signal, socket, subprocess, sys, time

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DEFAULT_BIN = os.path.join(REPO, "native", "build-native", "rb3-native")
DEFAULT_DATA = os.path.join(REPO, "orig-assets", "extracted")

GAMEPLAY_SONGMS = 2000.0


def free_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.bind(("127.0.0.1", 0)); p = s.getsockname()[1]; s.close()
    return p


def health(port):
    try:
        c = http.client.HTTPConnection("127.0.0.1", port, timeout=4)
        c.request("GET", "/api/health"); r = c.getresponse()
        if r.status != 200:
            return None
        d = json.loads(r.read())["data"]
        return int(d["frame"]), float(d["songMs"]), str(d["currentScreen"])
    except Exception:
        return None
    finally:
        try: c.close()
        except Exception: pass


def build_nav(downs):
    # Frame-timed input verbs. Same chain as song-end-test.py but with a
    # parametric number of DOWN presses (spaced out) to land on the target song,
    # then select + track guitar + expert + nofail + autohit.
    parts = ["@10:start", "@30:confirm",
             "@140:select:pn_quickplay.btn", "@220:select:qp_quickplay.btn"]
    f = 320
    for _ in range(downs):
        parts.append(f"@{f}:down")
        f += 12
    parts.append(f"@{f+20}:msg:music_library:select_highlighted_node")
    parts.append(f"@{f+60}:track:guitar")
    parts.append(f"@{f+70}:difficulty:expert")
    parts.append(f"@{f+130}:msg:overshell:end_override_flow:1:0")
    parts.append(f"@{f+180}:nofail")
    parts.append(f"@{f+200}:autohit")
    return ",".join(parts)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("stemdir")
    ap.add_argument("--downs", type=int, default=3)
    ap.add_argument("--secs", type=int, default=40)
    ap.add_argument("--port", type=int, default=0)
    ap.add_argument("--bin", default=DEFAULT_BIN)
    ap.add_argument("--data", default=DEFAULT_DATA)
    ap.add_argument("--gain", default=None)
    ap.add_argument("--boot-timeout", type=int, default=240)
    args = ap.parse_args()

    out = os.path.abspath(args.out)
    stemdir = os.path.abspath(args.stemdir)
    os.makedirs(stemdir, exist_ok=True)
    if os.path.exists(out):
        os.remove(out)
    port = args.port or free_port()
    log_path = f"/tmp/rb3-guitaroff-cap-{port}.log"
    logf = open(log_path, "w")

    nav = build_nav(args.downs)
    env = dict(os.environ)
    env.update({
        "RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
        "MILO_HEADLESS": "1", "MILO_AUDIO": "1", "MILO_AUDIO_BACKEND": "null",
        "DC3_DUMP_AUDIO": out, "DC3_DUMP_SECONDS": str(args.secs),
        "RB3_DUMP_STEMS": stemdir,
        "RB3_DATA": args.data, "RB3_GAME_INPUT": nav,
    })
    if args.gain is not None:
        env["DC3_AUDIO_GAIN"] = str(args.gain)

    print(f"[guitaroff] launching (port {port}), downs={args.downs}, log -> {log_path}", flush=True)
    print(f"[guitaroff] nav={nav}", flush=True)
    proc = subprocess.Popen([args.bin], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)
    try:
        deadline = time.time() + args.boot_timeout
        started = None
        while time.time() < deadline:
            if proc.poll() is not None:
                print(f"[guitaroff] FAIL proc exited {proc.returncode} before gameplay", flush=True)
                return 1
            h = health(port)
            if h and h[1] > GAMEPLAY_SONGMS:
                started = h
                break
            time.sleep(0.5)
        if not started:
            print("[guitaroff] FAIL never reached gameplay", flush=True)
            return 1
        print(f"[guitaroff] gameplay: frame={started[0]} songMs={started[1]:.0f} screen={started[2]}", flush=True)
        wait_dl = time.time() + args.secs + 30
        while time.time() < wait_dl:
            if proc.poll() is not None:
                break
            time.sleep(1.0)
        time.sleep(2)
        sz = os.path.getsize(out) if os.path.exists(out) else 0
        manifest = os.path.join(stemdir, "stems.json")
        msz = os.path.getsize(manifest) if os.path.exists(manifest) else 0
        print(f"[guitaroff] wav={out} ({sz} bytes) stems_manifest={manifest} ({msz} bytes)", flush=True)
        return 0 if sz > 1000 else 1
    finally:
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
            try: proc.wait(timeout=8)
            except subprocess.TimeoutExpired: os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
        except Exception:
            pass
        logf.close()
        print(f"[guitaroff] engine log: {log_path}", flush=True)


if __name__ == "__main__":
    sys.exit(main())
