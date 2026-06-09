#!/usr/bin/env python3
"""
audio_stem_verify.py — SAMPLE-ACCURATE per-stem decode verification.

The previous tools (audio_correlate.py / audio_verify.py) compared the game's
real-time POST-MIX capture against an offline decode. That path can never reach
high direct-waveform Pearson: the null-backend mixer phase-drifts, the master bus
limiter is nonlinear, and ~15 stems are summed — so raw Pearson collapses to ~0.03
and only a drift-robust spectrogram-shape metric works (and it can only certify
"same music", not "decodes correctly").

This tool takes the SAMPLE-ACCURATE path the user asked for. The engine (when
RB3_DUMP_STEMS=<dir> is set) dumps EACH Vorbis stem's decoded mono int16 PCM
straight out of StandardStream::ConsumeData — the exact samples handed to the
audio ring, sample-aligned across stems — to <dir>/stem_<NN>.s16 (+ stems.json).
We then INDEPENDENTLY decode the SAME mogg channel offline (decrypt_mogg ->
ffmpeg -> de-interleave channel NN), find the best integer sample lag, and report
the post-alignment DIRECT-WAVEFORM Pearson per stem.

Same Vorbis bitstream + same decoder family (libvorbis via ffmpeg vs the engine's
Tremor/Vorbis) -> ~0.95-0.99 sample-accurate after a tiny integer-lag alignment.
A garbled stem drops out, pinpointing a decode bug to that exact channel.

Usage:
    audio_stem_verify.py --stems-dir /tmp/rb3_stems --song 20thcenturyboy
    audio_stem_verify.py --stems-dir /tmp/rb3_stems --mogg /path/to/song.mogg
    audio_stem_verify.py --selftest
    audio_stem_verify.py --json ...

  --max-lag-s N   integer-lag search window (default 5.0s)
  --window-s N    correlate only the first N seconds (default: whole overlap)
  --skip-s N      skip the first N seconds of BOTH signals before correlating
                  (default 0; useful if the stems include a non-decoded preroll)

Verdict: DECODE-CORRECT if the MEDIAN per-stem Pearson >= 0.90 (after alignment).

Self-test (no engine, no mogg): builds synthetic 15-"stem" int16 PCM, an offline
"decode" that is a lag-shifted copy of each, plus one garbled stem, and confirms
the tool recovers ~1.0 on the clean stems and flags the garbled one.
"""
import argparse
import json
import os
import subprocess
import sys
import tempfile

import numpy as np

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, os.path.join(REPO, "scripts", "native"))


# ---------------------------------------------------------------------------
# I/O
# ---------------------------------------------------------------------------
def read_s16(path):
    """Read raw little-endian mono int16 PCM as float64 in [-1, 1]."""
    raw = np.fromfile(path, dtype="<i2")
    return raw.astype(np.float64) / 32768.0


def read_manifest(stems_dir):
    p = os.path.join(stems_dir, "stems.json")
    with open(p) as f:
        return json.load(f)


def decode_mogg_channels(mogg_path, out_dir):
    """decrypt_mogg -> ffmpeg -> float64 array (channels, samples) at source rate."""
    import decrypt_mogg
    pt, _info = decrypt_mogg.decrypt_mogg(mogg_path)
    assert pt[:4] == b"OggS", "decrypt FAILED — not OggS"
    ogg_path = os.path.join(out_dir, "stemverify_decode.ogg")
    with open(ogg_path, "wb") as f:
        f.write(pt)
    probe = subprocess.run(
        ["ffprobe", "-v", "error", "-select_streams", "a:0",
         "-show_entries", "stream=channels,sample_rate", "-of", "json", ogg_path],
        capture_output=True, text=True, check=True)
    info = json.loads(probe.stdout)["streams"][0]
    rate = int(info["sample_rate"])
    ch = int(info["channels"])
    proc = subprocess.run(
        ["ffmpeg", "-v", "error", "-i", ogg_path,
         "-f", "f32le", "-acodec", "pcm_f32le", "-ac", str(ch), "-"],
        capture_output=True, check=True)
    data = np.frombuffer(proc.stdout, dtype="<f4")
    n = data.size // ch
    data = data[: n * ch].reshape(n, ch).T.astype(np.float64)  # (ch, n)
    try:
        os.remove(ogg_path)
    except OSError:
        pass
    return rate, data


# ---------------------------------------------------------------------------
# Alignment + correlation
# ---------------------------------------------------------------------------
def best_lag_gccphat(a, b, sr, max_lag_s):
    """GCC-PHAT integer-lag estimate that best aligns `a` to `b`.

    PHAT-whitening flattens the cross-power spectrum so the lag is found by the
    PHASE structure alone — robust to spectral coloring / level differences
    between the two decoders. Returns (lag, peak_value). lag>0 => a is delayed
    vs b (a needs to be advanced / b delayed)."""
    n = len(a)
    m = len(b)
    if n == 0 or m == 0:
        return 0, 0.0
    L = 1
    while L < n + m:
        L <<= 1
    A = np.fft.rfft(a - a.mean(), L)
    B = np.fft.rfft(b - b.mean(), L)
    R = A * np.conj(B)
    R /= (np.abs(R) + 1e-12)          # PHAT weighting
    cc = np.fft.irfft(R, L)
    max_lag = int(max_lag_s * sr)
    # positive lags in cc[0:max_lag], negative in cc[L-max_lag:L]
    pos = cc[: max_lag + 1]
    neg = cc[L - max_lag:]
    full = np.concatenate([neg, pos])       # indices -max_lag .. +max_lag
    k = int(np.argmax(full))
    lag = k - max_lag
    return lag, float(full[k])


def apply_lag(a, b, lag):
    """Shift to align then crop to common overlap. lag>0 => drop lag samples of a."""
    if lag > 0:
        a = a[lag:]
    elif lag < 0:
        b = b[-lag:]
    n = min(len(a), len(b))
    return a[:n], b[:n]


def pearson(a, b):
    if len(a) < 2:
        return 0.0
    a0 = a - a.mean()
    b0 = b - b.mean()
    d = np.sqrt(np.dot(a0, a0) * np.dot(b0, b0))
    if d < 1e-30:
        return 0.0
    return float(np.dot(a0, b0) / d)


def refine_lag(a, b, lag0, sr, radius=64):
    """Local Pearson search +-radius samples around lag0 to pick the integer lag
    that MAXIMISES direct-waveform Pearson (GCC-PHAT gets within a few samples;
    this nails the exact best integer offset)."""
    best_lag, best_corr = lag0, -2.0
    # cap the correlation window for speed (first ~20 s)
    lim = int(20 * sr)
    for d in range(lag0 - radius, lag0 + radius + 1):
        ca, cb = apply_lag(a[:lim + abs(d) + 1], b[:lim + abs(d) + 1], d)
        if len(ca) < sr:
            continue
        c = pearson(ca[:lim], cb[:lim])
        if c > best_corr:
            best_corr, best_lag = c, d
    return best_lag, best_corr


def rms(x):
    return float(np.sqrt(np.mean(x ** 2))) if len(x) else 0.0


# ---------------------------------------------------------------------------
# Per-stem verification
# ---------------------------------------------------------------------------
def verify_stem(cap, ref, sr, max_lag_s):
    """cap, ref: float64 mono. Returns dict of metrics for this stem."""
    cap_rms = rms(cap)
    ref_rms = rms(ref)
    # Silent stems are unverifiable by correlation; report and skip from median.
    silent = cap_rms < 1e-4 and ref_rms < 1e-4
    lag, phat_peak = best_lag_gccphat(cap, ref, sr, max_lag_s)
    lag, _ = refine_lag(cap, ref, lag, sr)
    ca, cb = apply_lag(cap, ref, lag)
    corr = pearson(ca, cb)
    return dict(lag=lag, phat_peak=phat_peak, corr=corr,
                cap_rms=cap_rms, ref_rms=ref_rms, silent=silent,
                overlap=len(ca))


def run_verify(stems_dir, mogg_path, max_lag_s, window_s, skip_s):
    manifest = read_manifest(stems_dir)
    sr_cap = manifest["sample_rate"]
    num_ch = manifest["num_channels"]
    real_ch = manifest.get("real_channels", num_ch)

    with tempfile.TemporaryDirectory() as td:
        sr_ref, ref_channels = decode_mogg_channels(mogg_path, td)
    if sr_ref != sr_cap:
        print(f"[warn] dumped rate {sr_cap} != mogg rate {sr_ref}; correlating at "
              f"the dumped rate is still valid (both decode the same bitstream).")

    ref_ch_count = ref_channels.shape[0]
    skip = int(skip_s * sr_cap)
    win = int(window_s * sr_cap) if window_s else None

    results = []
    for ch in range(num_ch):
        cap_path = os.path.join(stems_dir, f"stem_{ch:02d}.s16")
        if not os.path.exists(cap_path):
            results.append(dict(ch=ch, error="missing dump", corr=0.0,
                                silent=False, lag=0))
            continue
        cap = read_s16(cap_path)
        if ch >= ref_ch_count:
            # virtual / mapped channel beyond the mogg's real channels
            results.append(dict(ch=ch, error="no mogg channel (virtual)",
                                corr=0.0, silent=False, lag=0,
                                cap_rms=rms(cap)))
            continue
        ref = ref_channels[ch]
        if skip:
            cap = cap[skip:]
            ref = ref[skip:]
        if win:
            cap = cap[:win + int(max_lag_s * sr_cap)]
            ref = ref[:win + int(max_lag_s * sr_cap)]
        r = verify_stem(cap, ref, sr_cap, max_lag_s)
        r["ch"] = ch
        results.append(r)

    return dict(sr=sr_cap, num_channels=num_ch, real_channels=real_ch,
                ref_channels=ref_ch_count, mogg=mogg_path, stems_dir=stems_dir,
                results=results)


def decide(report):
    corrs = [r["corr"] for r in report["results"]
             if not r.get("silent") and not r.get("error")]
    if not corrs:
        return "NO-DATA", 0.0, 0.0
    med = float(np.median(corrs))
    mx = float(np.max(corrs))
    verdict = "DECODE-CORRECT" if med >= 0.90 else (
        "PARTIAL" if mx >= 0.90 else "DECODE-WRONG")
    return verdict, med, mx


def print_report(report):
    print(f"\n  stems dir : {report['stems_dir']}")
    print(f"  mogg      : {report['mogg']}")
    print(f"  rate={report['sr']} Hz  dumped-channels={report['num_channels']}  "
          f"mogg-channels={report['ref_channels']}\n")
    hdr = (f"  {'stem':>4} {'corr':>8} {'lag':>7} {'phat':>6} "
           f"{'cap_rms':>9} {'ref_rms':>9}  note")
    print(hdr)
    print("  " + "-" * (len(hdr) - 2))
    for r in report["results"]:
        if r.get("error"):
            print(f"  {r['ch']:>4} {'-':>8} {'-':>7} {'-':>6} "
                  f"{r.get('cap_rms', 0):>9.5f} {'-':>9}  {r['error']}")
            continue
        note = "SILENT" if r.get("silent") else ("LOW" if r["corr"] < 0.90 else "")
        print(f"  {r['ch']:>4} {r['corr']:>8.4f} {r['lag']:>7d} "
              f"{r.get('phat_peak', 0):>6.3f} {r['cap_rms']:>9.5f} "
              f"{r['ref_rms']:>9.5f}  {note}")
    verdict, med, mx = decide(report)
    print(f"\n  median per-stem Pearson : {med:.4f}")
    print(f"  max    per-stem Pearson : {mx:.4f}")
    print(f"\n  ============  VERDICT: {verdict}  ============")
    print("    DECODE-CORRECT = median stem corr >= 0.90 (sample-accurate decode).\n")


# ---------------------------------------------------------------------------
# Self-test
# ---------------------------------------------------------------------------
def selftest():
    sr = 44100
    n = 10 * sr
    rng = np.random.default_rng(0)
    td = tempfile.mkdtemp(prefix="stemverify_selftest_")
    os.makedirs(td, exist_ok=True)
    num_ch = 6

    # Build "reference" channels (the offline decode): band-limited noise + tones,
    # one per channel, distinct content. These stand in for ffmpeg-decoded mogg ch.
    t = np.arange(n) / sr
    ref = []
    for c in range(num_ch):
        sig = (0.5 * np.sin(2 * np.pi * (110 * (c + 1)) * t)
               + 0.3 * np.sin(2 * np.pi * (220 * (c + 1) + 7) * t)
               + 0.1 * rng.normal(0, 1, n))
        sig = sig / (np.max(np.abs(sig)) + 1e-9) * 0.7
        ref.append(sig)
    ref = np.array(ref)

    # The "dumped" capture: each stem is the SAME content, delayed by a small
    # per-stem integer lag (simulating the decoder warm-up offset), quantised to
    # int16 (the real dump is int16). Stem 3 is GARBLED (random) -> must drop.
    lags = [0, 17, 200, 5, -33, 1024]
    for c in range(num_ch):
        if c == 3:
            cap = rng.normal(0, 0.3, n)        # garbled stem
        else:
            lag = lags[c]
            cap = np.zeros(n)
            if lag >= 0:
                cap[lag:] = ref[c][: n - lag]
            else:
                cap[: n + lag] = ref[c][-lag:]
        pcm = np.clip(cap, -1, 1)
        (pcm * 32767.0).astype("<i2").tofile(os.path.join(td, f"stem_{c:02d}.s16"))

    # Write the same channels as a multichannel ogg via ffmpeg so the tool's real
    # decode path (decrypt is bypassed by patching the mogg loader) is exercised.
    # Simpler + hermetic: write an interleaved f32 wav and decode directly here.
    inter = np.ascontiguousarray(ref.T.astype("<f4"))
    import struct
    wavp = os.path.join(td, "ref_multich.wav")
    raw = inter.tobytes()
    with open(wavp, "wb") as f:
        f.write(b"RIFF"); f.write(struct.pack("<I", 36 + len(raw))); f.write(b"WAVE")
        f.write(b"fmt "); f.write(struct.pack("<IHHIIHH", 16, 3, num_ch, sr,
                                              sr * num_ch * 4, num_ch * 4, 32))
        f.write(b"data"); f.write(struct.pack("<I", len(raw))); f.write(raw)

    manifest = dict(sample_rate=sr, num_channels=num_ch, real_channels=num_ch,
                    total_samples=n, bytes_per_sample=2, format="s16le")
    with open(os.path.join(td, "stems.json"), "w") as f:
        json.dump(manifest, f)

    # monkeypatch decode_mogg_channels to read the wav instead of decrypting a mogg
    global decode_mogg_channels
    orig = decode_mogg_channels

    def fake_decode(mogg_path, out_dir):
        proc = subprocess.run(
            ["ffmpeg", "-v", "error", "-i", wavp, "-f", "f32le",
             "-acodec", "pcm_f32le", "-ac", str(num_ch), "-"],
            capture_output=True, check=True)
        data = np.frombuffer(proc.stdout, dtype="<f4")
        nn = data.size // num_ch
        return sr, data[: nn * num_ch].reshape(nn, num_ch).T.astype(np.float64)

    decode_mogg_channels = fake_decode
    try:
        report = run_verify(td, "FAKE.mogg", max_lag_s=2.0, window_s=0, skip_s=0)
    finally:
        decode_mogg_channels = orig

    print_report(report)

    # validate: 5 clean stems recover ~1.0 at their injected lag; the garbled one
    # is far below 0.9; the verdict is DECODE-CORRECT (median of clean stems ~1.0).
    ok = True
    by_ch = {r["ch"]: r for r in report["results"]}
    for c in range(num_ch):
        r = by_ch[c]
        if c == 3:
            if r["corr"] >= 0.5:
                print(f"  FAIL: garbled stem {c} corr {r['corr']:.3f} not flagged")
                ok = False
        else:
            if r["corr"] < 0.97:
                print(f"  FAIL: clean stem {c} corr {r['corr']:.3f} < 0.97")
                ok = False
            if r["lag"] != lags[c]:
                print(f"  NOTE: stem {c} recovered lag {r['lag']} (injected {lags[c]})")
    verdict, med, mx = decide(report)
    if verdict != "DECODE-CORRECT":
        print(f"  FAIL: verdict {verdict} (median {med:.3f})")
        ok = False
    print(f"\nSELF-TEST: {'ALL PASS — recovers per-stem lag + flags garbled stem' if ok else 'FAILURE'}")
    return 0 if ok else 1


# ---------------------------------------------------------------------------
def find_mogg(song_id):
    cands = [
        os.path.join(REPO, "orig-assets", "extracted", "songs", song_id, song_id + ".mogg"),
        os.path.join(REPO, "orig-assets", "extracted-xbox-full", "songs", song_id, song_id + ".mogg"),
    ]
    for c in cands:
        if os.path.exists(c):
            return c
    raise FileNotFoundError(f"no mogg found for song {song_id}: tried {cands}")


def main():
    ap = argparse.ArgumentParser(description="Per-stem sample-accurate decode verify")
    ap.add_argument("--stems-dir", help="dir with stem_NN.s16 + stems.json")
    ap.add_argument("--song", help="song id (locate mogg under orig-assets)")
    ap.add_argument("--mogg", help="explicit mogg path")
    ap.add_argument("--max-lag-s", type=float, default=5.0)
    ap.add_argument("--window-s", type=float, default=0.0,
                    help="correlate only first N seconds (0 = whole overlap)")
    ap.add_argument("--skip-s", type=float, default=0.0,
                    help="skip first N seconds of both signals")
    ap.add_argument("--json", action="store_true")
    ap.add_argument("--selftest", action="store_true")
    args = ap.parse_args()

    if args.selftest:
        return selftest()
    if not args.stems_dir:
        ap.error("need --stems-dir (or --selftest)")
    mogg = args.mogg or (find_mogg(args.song) if args.song else None)
    if not mogg:
        ap.error("need --mogg or --song")

    report = run_verify(args.stems_dir, mogg, args.max_lag_s,
                        args.window_s, args.skip_s)
    verdict, med, mx = decide(report)
    report["verdict"], report["median_corr"], report["max_corr"] = verdict, med, mx
    if args.json:
        print(json.dumps(report, indent=2))
    else:
        print_report(report)
    return 0 if verdict == "DECODE-CORRECT" else (1 if verdict == "PARTIAL" else 2)


if __name__ == "__main__":
    sys.exit(main())
