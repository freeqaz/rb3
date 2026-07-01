#!/usr/bin/env python3
"""Capture rb3-native's audio across the WHOLE song-start window (boot -> nav ->
venue/crowd INTRO -> song stems) and report the per-second RMS so we can see
whether the intro window is silent (baseline bug) or now plays crowd audio.

The DC3_DUMP_AUDIO sink in AudioDevice records mixed output continuously from
audio-device init, so the dumped WAV's timeline == wall-clock from boot. We drive
the standard quickplay->gameplay nav, dump a long window, then print a per-second
RMS column plus the index of the FIRST non-silent second. With the CrowdAudio fix
the intro window (the seconds BEFORE the song stems start at clock 0) should be
non-silent; before the fix it is silent until the song mogg snaps in.

Usage:
    capture_intro_crowd_audio.py OUT.wav [--secs 40] [--port P] [--label before]
"""
import argparse, http.client, json, os, socket, subprocess, sys, time, wave
import numpy as np

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DEFAULT_BIN = os.path.join(REPO, "native", "build-native", "rb3-native")
DEFAULT_DATA = os.path.join(REPO, "orig-assets", "extracted")
NAV_SCRIPT = (
    "@10:start,@30:confirm,"
    "@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn,"
    "@320:down,@350:msg:music_library:select_highlighted_node,"
    "@380:track:guitar,@390:difficulty:expert,@450:msg:overshell:end_override_flow:1:0,"
    "@500:nofail,@520:autohit"
)


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
        j = json.loads(r.read())
        return (j.get("frame", 0), float(j.get("songMs", 0)), j.get("screen", "?"))
    except Exception:
        return None


def rms_per_second(wav_path):
    with wave.open(wav_path, "rb") as w:
        sr = w.getframerate(); ch = w.getnchannels()
        n = w.getnframes()
        raw = w.readframes(n)
    a = np.frombuffer(raw, dtype=np.int16).astype(np.float32) / 32768.0
    if ch > 1:
        a = a.reshape(-1, ch).mean(axis=1)
    secs = []
    for s in range(0, len(a), sr):
        seg = a[s:s + sr]
        if len(seg) == 0:
            break
        secs.append(float(np.sqrt(np.mean(seg ** 2))))
    return sr, secs


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("--secs", type=int, default=40)
    ap.add_argument("--port", type=int, default=0)
    ap.add_argument("--bin", default=DEFAULT_BIN)
    ap.add_argument("--data", default=DEFAULT_DATA)
    ap.add_argument("--label", default="run")
    ap.add_argument("--boot-timeout", type=int, default=240)
    args = ap.parse_args()

    out = os.path.abspath(args.out)
    if os.path.exists(out):
        os.remove(out)
    port = args.port or free_port()
    log_path = f"/tmp/rb3-intro-cap-{port}.log"
    logf = open(log_path, "w")

    env = dict(os.environ)
    env.update({
        "RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
        "MILO_HEADLESS": "1", "MILO_AUDIO": "1", "MILO_AUDIO_BACKEND": "null",
        "DC3_DUMP_AUDIO": out, "DC3_DUMP_SECONDS": str(args.secs),
        "RB3_DATA": args.data, "RB3_GAME_INPUT": NAV_SCRIPT,
    })
    print(f"[{args.label}] launching (port {port}), log -> {log_path}", flush=True)
    proc = subprocess.Popen([args.bin], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)
    try:
        deadline = time.time() + args.boot_timeout
        reached = None
        while time.time() < deadline:
            if proc.poll() is not None:
                print(f"[{args.label}] FAIL proc exited {proc.returncode}", flush=True)
                return 1
            h = health(port)
            if h and h[1] > 4000.0:   # song clock well into the song proper
                reached = h
                break
            time.sleep(0.5)
        if not reached:
            print(f"[{args.label}] FAIL never reached song proper", flush=True)
            # still finalize whatever was dumped
        else:
            print(f"[{args.label}] song underway: frame={reached[0]} songMs={reached[1]:.0f} screen={reached[2]}", flush=True)
        # let the dump fill to its cap
        time.sleep(min(args.secs, 12))
    finally:
        try:
            proc.terminate()
            time.sleep(2)
            if proc.poll() is None:
                proc.kill()
        except Exception:
            pass

    if not (os.path.exists(out) and os.path.getsize(out) > 1000):
        print(f"[{args.label}] FAIL no dump produced ({out})", flush=True)
        return 1
    sr, secs = rms_per_second(out)
    print(f"[{args.label}] WAV {out} ({os.path.getsize(out)} bytes, sr={sr})", flush=True)
    SIL = 1e-4
    first_audio = next((i for i, v in enumerate(secs) if v > SIL), None)
    print(f"[{args.label}] per-second RMS (silence < {SIL}):", flush=True)
    for i, v in enumerate(secs):
        bar = "#" * int(min(v, 0.3) / 0.3 * 40)
        mark = " <- first audio" if i == first_audio else ""
        print(f"    t={i:2d}s  rms={v:.5f}  {bar}{mark}", flush=True)
    print(f"[{args.label}] first non-silent second = {first_audio}", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
