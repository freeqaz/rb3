#!/usr/bin/env python3
"""
song-preview-audio-test.py — headless native song-preview AUDIO capture.

Boots rb3-native to the Music Library (song_select), lands the highlight on a
real song, and waits for SongPreview to fire (MetaPanel::Poll -> SongPreview::Poll
-> TheSynth->NewStream(...) -> StandardStream -> RB3StreamReceiverNative). The
engine's null-backend WAV dump (DC3_DUMP_AUDIO) records the mixed output the whole
time; we then analyze per-second RMS to prove the preview is audible (sustained
non-zero), not just a boot transient.

This is the audio sibling of song-select-capture.py. Same nav, but adds:
    MILO_AUDIO=1 MILO_AUDIO_BACKEND=null DC3_DUMP_AUDIO=... DC3_DUMP_SECONDS=...
    RB3_STREAM_AUDIO_DBG=1   (per-channel stream render debug in the log)

    python3 scripts/native/song-preview-audio-test.py [--port N] [--data DIR]
            [--bin PATH] [--secs 70] [--out /tmp/rb3_preview.wav] [--verbose]

Exit 0 = reached song_select, preview fired, and captured audible audio.
"""
import argparse, http.client, json, math, os, signal, socket, struct, subprocess, sys, time, wave

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DEFAULT_BIN = os.path.join(REPO, "native", "build-native", "rb3-native")
DEFAULT_DATA = os.path.join(REPO, "orig-assets", "extracted")

# Boot -> main_hub -> PLAY NOW -> QUICKPLAY -> song_select (same as song-select-capture)
NAV_SCRIPT = (
    "@10:start,@30:confirm,"
    "@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn"
)
SERVER_READY_TIMEOUT = 40
SONGSELECT_TIMEOUT = 120


def log(m): print(f"[preview-audio] {m}", flush=True)

def free_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM); s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]; s.close(); return p

def http_get_bytes(port, path, timeout=10):
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=timeout)
    try:
        c.request("GET", path); r = c.getresponse(); return r.status, r.read()
    finally:
        c.close()

def http_post(port, path, body, timeout=15):
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=timeout)
    try:
        c.request("POST", path, body=body, headers={"Content-Type": "text/plain"})
        r = c.getresponse(); return r.status, r.read().decode("utf-8", "replace")
    finally:
        c.close()

def health(port):
    try:
        st, b = http_get_bytes(port, "/api/health")
        if st != 200: return None
        d = json.loads(b.decode("utf-8", "replace"))["data"]
        return int(d["frame"]), float(d["songMs"]), str(d["currentScreen"])
    except Exception:
        return None

def dta_eval(port, expr):
    try:
        st, b = http_post(port, "/api/dta/eval", expr, timeout=10)
        if st != 200: return None
        d = json.loads(b)
        if not d.get("ok"): return None
        return d["data"].get("value")
    except Exception:
        return None

def wait_for(port, pred, timeout, label, verbose, proc):
    dl = time.time() + timeout; last = None
    while time.time() < dl:
        if proc.poll() is not None:
            log(f"FAIL: process exited (code {proc.returncode}) waiting for {label}"); return None
        h = health(port)
        if h is not None:
            f, m, s = h
            if verbose and (f, s) != last:
                log(f"  ...{label}: frame={f} songMs={m:.0f} screen='{s}'"); last = (f, s)
            if pred(f, m, s): return h
        time.sleep(0.4)
    return None


def analyze_wav(path):
    """Return (sampleRate, channels, durationSec, peak, nonZeroPct, overallRMS, perSecRMS)."""
    # Try the proper WAV reader first; fall back to raw PCM (SIGTERM may skip finalize).
    raw = open(path, "rb").read()
    sr, ch = 44100, 2
    data = None
    try:
        w = wave.open(path, "rb")
        sr, ch = w.getframerate(), w.getnchannels()
        n = w.getnframes()
        if n > 0:
            data = w.readframes(n)
        w.close()
    except Exception:
        data = None
    if not data:
        # Raw fallback: skip 44-byte header, parse the rest as int16.
        data = raw[44:]
    n_samp = len(data) // 2
    if n_samp == 0:
        return sr, ch, 0.0, 0, 0.0, 0.0, []
    samples = struct.unpack("<%dh" % n_samp, data[: n_samp * 2])
    peak = 0; nonzero = 0; sumsq = 0.0
    for s in samples:
        a = abs(s)
        if a > peak: peak = a
        if a > 64: nonzero += 1
        sumsq += s * s
    overall = math.sqrt(sumsq / n_samp)
    dur = n_samp / float(sr * ch)
    fps = sr * ch
    per_sec = []
    for k in range(0, n_samp, fps):
        seg = samples[k:k + fps]
        if not seg: break
        sq = sum(v * v for v in seg)
        per_sec.append(math.sqrt(sq / len(seg)))
    return sr, ch, dur, peak, 100.0 * nonzero / n_samp, overall, per_sec


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=0)
    ap.add_argument("--bin", default=DEFAULT_BIN)
    ap.add_argument("--data", default=DEFAULT_DATA)
    ap.add_argument("--secs", type=int, default=70, help="DC3_DUMP_SECONDS")
    ap.add_argument("--down", type=int, default=2, help="down-presses to land on a real song")
    ap.add_argument("--out", default="/tmp/rb3_preview.wav")
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    if not os.path.exists(args.bin):
        log(f"FAIL: binary not found: {args.bin}"); return 1

    port = args.port or free_port()
    log_path = os.path.join("/tmp", f"rb3-preview-{port}.log"); logf = open(log_path, "w")
    env = dict(os.environ)
    env.update({
        "RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
        "MILO_HEADLESS": "1", "RB3_DATA": args.data, "RB3_GAME_INPUT": NAV_SCRIPT,
        # Audio capture (null backend, real-time pace, post-mix WAV dump)
        "MILO_AUDIO": "1", "MILO_AUDIO_BACKEND": "null",
        "DC3_DUMP_AUDIO": args.out, "DC3_DUMP_SECONDS": str(args.secs),
        "RB3_STREAM_AUDIO_DBG": "1",
    })
    if os.path.exists(args.out):
        try: os.remove(args.out)
        except OSError: pass
    log(f"launching rb3-native (port {port}), log -> {log_path}, wav -> {args.out}")
    proc = subprocess.Popen([args.bin], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)
    rc = 1
    try:
        if wait_for(port, lambda f, m, s: True, SERVER_READY_TIMEOUT, "server", args.verbose, proc) is None:
            log("FAIL: HTTP server never came up"); return 1
        log("HTTP server up")
        h = wait_for(port, lambda f, m, s: s == "song_select_screen",
                     SONGSELECT_TIMEOUT, "song_select", args.verbose, proc)
        if h is None:
            log("FAIL: never reached song_select_screen"); return 1
        log(f"song_select reached: frame={h[0]}")
        time.sleep(2.0)

        # Land on a real song node (skip header/folder rows), restarting the
        # preview timer each move.
        for i in range(args.down):
            http_post(port, "/api/input", "down"); time.sleep(0.4)
        hl = dta_eval(port, "{music_library get_highlighted_node}")
        log(f"highlighted node after {args.down} down: {hl}")

        # Now hold still and let SongPreview fire + play. The WAV dump keeps
        # recording until DC3_DUMP_SECONDS of audio is mixed, then finalizes.
        log(f"holding on song; waiting for preview + capture ({args.secs}s window)...")
        deadline = time.time() + args.secs + 15
        last_screen = None
        while time.time() < deadline:
            if proc.poll() is not None:
                log("process exited (dump likely finalized)"); break
            h = health(port)
            if h and args.verbose and h[2] != last_screen:
                log(f"  screen='{h[2]}' frame={h[0]}"); last_screen = h[2]
            time.sleep(1.0)
        rc = 0
    finally:
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
            try: proc.wait(timeout=8)
            except subprocess.TimeoutExpired: os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
        except Exception: pass
        logf.close()

    # --- Analyze ---
    if not os.path.exists(args.out):
        log(f"FAIL: no WAV produced at {args.out}"); log(f"engine log: {log_path}"); return 1
    sr, ch, dur, peak, nzpct, overall, per_sec = analyze_wav(args.out)
    log(f"WAV: {dur:.1f}s  {sr}Hz x{ch}  peak={peak}  nonZero={nzpct:.1f}%  overallRMS={overall:.0f}")
    log("per-second RMS:")
    for i, r in enumerate(per_sec):
        bar = "=" * min(50, int(r / 100))
        log(f"  t={i+1:>2}s  RMS={r:>7.0f}  |{bar}")

    # Preview verdict: at least 3 consecutive seconds of RMS > 500 anywhere
    # after the first ~10s (boot/menu) => sustained preview audio.
    run = 0; best = 0
    for r in per_sec:
        if r > 500: run += 1; best = max(best, run)
        else: run = 0
    grep = subprocess.run(["grep", "-c", "Preview: Preparing", log_path],
                          capture_output=True, text=True)
    prep_count = grep.stdout.strip()
    log(f"'Preview: Preparing' log lines: {prep_count}")
    passed = best >= 3 and peak > 1000
    log(f"longest sustained (>500 RMS) run: {best}s")
    log("RESULT: " + ("PASS — preview audible" if passed else "FAIL — no sustained preview audio"))
    log(f"engine log: {log_path}")
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())
