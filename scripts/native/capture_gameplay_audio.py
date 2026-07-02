#!/usr/bin/env python3
"""
capture_gameplay_audio.py — drive rb3-native headless into actual GAMEPLAY of the
default first song and dump the post-mix audio to a WAV (null audio backend, real-
time pace, auto-finalize).

This reaches REAL gameplay (all song stems summed through AudioDevice::MixSources),
not the song-select preview. It reuses song-end-test.py's proven nav script and
polls /api/health until the song clock is advancing past the intro before letting
the DC3_DUMP_AUDIO dump fill.

Usage:
    python3 scripts/native/capture_gameplay_audio.py OUT.wav [--secs N] [--port P]

Env knobs forwarded: DC3_AUDIO_GAIN (master gain override), MILO_AUDIO_BACKEND.
"""
import argparse, http.client, json, os, signal, socket, subprocess, sys, time

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DEFAULT_BIN = os.path.join(REPO, "native", "build-native", "rb3-native")
DEFAULT_DATA = os.path.join(REPO, "orig-assets", "extracted")

# Same canonical boot-to-gameplay nav as song-end-test.py (reaches game_screen,
# nofail + autohit so the song plays itself). part:/diff: are real pad-press
# verbs (readiness-gated on the part_difficulty overshell view) — no
# end_override_flow needed; diff:'s Confirm drives the overshell to
# ready_to_play, which auto-launches the song.
NAV_SCRIPT = (
    "@10:start,@30:confirm,"
    "@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn,"
    "@320:down,@350:msg:music_library:select_highlighted_node,"
    "@380:part:guitar,@400:diff:expert,"
    "@500:nofail,@520:autohit"
)
GAMEPLAY_SONGMS = 2000.0  # song clock past intro = gameplay underway


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


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("--secs", type=int, default=20)
    ap.add_argument("--port", type=int, default=0)
    ap.add_argument("--bin", default=DEFAULT_BIN)
    ap.add_argument("--data", default=DEFAULT_DATA)
    ap.add_argument("--gain", default=None, help="DC3_AUDIO_GAIN override")
    ap.add_argument("--boot-timeout", type=int, default=250)  # +30s: real part/diff pad-press verbs take longer than the old instant commit
    args = ap.parse_args()

    out = os.path.abspath(args.out)
    if os.path.exists(out):
        os.remove(out)
    port = args.port or free_port()
    log_path = f"/tmp/rb3-gameplay-cap-{port}.log"
    logf = open(log_path, "w")

    env = dict(os.environ)
    env.update({
        "RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
        "MILO_HEADLESS": "1", "MILO_AUDIO": "1", "MILO_AUDIO_BACKEND": "null",
        "DC3_DUMP_AUDIO": out, "DC3_DUMP_SECONDS": str(args.secs),
        "RB3_DATA": args.data, "RB3_GAME_INPUT": NAV_SCRIPT,
    })
    if args.gain is not None:
        env["DC3_AUDIO_GAIN"] = str(args.gain)

    print(f"[gameplay-cap] launching (port {port}), log -> {log_path}", flush=True)
    proc = subprocess.Popen([args.bin], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)
    rc = 1
    try:
        # Wait for gameplay (song clock advancing). The DUMP records from the first
        # audio frame, but we want the dump window to contain real gameplay, so we
        # confirm gameplay started, then let it run secs+slack.
        deadline = time.time() + args.boot_timeout
        started = None
        while time.time() < deadline:
            if proc.poll() is not None:
                print(f"[gameplay-cap] FAIL proc exited {proc.returncode} before gameplay", flush=True)
                return 1
            h = health(port)
            if h and h[1] > GAMEPLAY_SONGMS:
                started = h
                break
            time.sleep(0.5)
        if not started:
            print("[gameplay-cap] FAIL never reached gameplay", flush=True)
            return 1
        print(f"[gameplay-cap] gameplay underway: frame={started[0]} songMs={started[1]:.0f} screen={started[2]}", flush=True)
        # let the dump fill: it records the FIRST secs of audio frames, which start
        # at the intro. To capture gameplay we keep running; the dump auto-finalizes
        # at secs of audio. Wait for finalize.
        wait_dl = time.time() + args.secs + 25
        while time.time() < wait_dl:
            if proc.poll() is not None:
                break
            if os.path.exists(out) and os.path.getsize(out) > 1000:
                # finalized header present? give a bit more then check size stable
                pass
            time.sleep(1.0)
        time.sleep(2)
        sz = os.path.getsize(out) if os.path.exists(out) else 0
        print(f"[gameplay-cap] captured {out} ({sz} bytes)", flush=True)
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
        print(f"[gameplay-cap] engine log: {log_path}", flush=True)


if __name__ == "__main__":
    sys.exit(main())
