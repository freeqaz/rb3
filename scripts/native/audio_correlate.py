#!/usr/bin/env python3
"""
audio_correlate.py — AUDIO DIVERGENCE METRIC.

Given a CAPTURE wav (the game's recorded output) and a REFERENCE wav (an
independent ground-truth decode of the same song), quantify *how much* they
diverge and *whether the capture is clipped / distorted*.

This is the test the earlier "real music, not noise" proof MISSED: a clipped or
wrongly-scaled copy of the real song is still reproducible and spectrally
non-flat, so reproducibility-corr and spectral-flatness both PASS on a badly
distorted output. The decisive signal is correlation against an *independent
reference* + clip-ratio + crest-factor + spectral divergence.

What it does
------------
1. Time-align capture to reference via cross-correlation (reports best lag).
2. After alignment + per-channel gain normalization:
     - Pearson correlation (overall + per-second), 0..1.
     - Divergence: normalized RMS error and SNR (dB) of (capture - g*reference).
     - CLIP RATIO of the capture: fraction of |sample| >= 0.99 full-scale.
       Crest factor (peak/RMS) — clipped audio has a LOW crest factor.
     - Spectral divergence: Welch-PSD log distance (catches harmonic distortion
       from clipping even when the waveform correlation is high).
     - THD-ish proxy: excess high-frequency energy of capture vs reference
       (hard clipping adds high-order harmonics → raised HF energy).
3. Prints a verdict table and an overall call:
     MATCH        — high corr, low clip-ratio, normal crest, low spectral div.
     CLIPPED      — high corr (it IS the song) BUT high clip-ratio / low crest /
                    raised harmonics / high spectral div.
     WRONG-SIGNAL — low correlation (not the reference song at all, e.g. noise).

Usage:  audio_correlate.py CAPTURE.wav REFERENCE.wav [--json]
Inputs may be any format ffmpeg can decode; non-WAV is transcoded to a temp WAV.

BEST PRACTICE: the REFERENCE should be a SAMPLE-ACCURATE offline decode of the
same song (e.g. `ffmpeg -i <id>.mogg ref.wav` with the same channel downmix) so
the waveform cross-correlation path is the primary judge. A second *game capture*
is not sample-accurate (the null-backend mixer is non-deterministic / phase-drifts),
so the tool falls back to a timing-robust spectrogram-shape correlation for those
pairs — which can certify a CLEAN same-music match but cannot reliably catch a
clipped-AND-drifted capture. Use an offline decode to judge clipping rigorously.

Self-test:  audio_correlate.py --selftest
  Generates a 440 Hz sine reference and tests it against
    (a) the same sine            -> expect MATCH
    (b) hard-clipped at 0.3       -> expect CLIPPED
    (c) white noise              -> expect WRONG-SIGNAL
"""
import argparse
import os
import subprocess
import sys
import tempfile
import wave

import numpy as np
from scipy import signal

# ---------------------------------------------------------------------------
# Decision thresholds (documented in phase1-metric.md).
# ---------------------------------------------------------------------------
CLIP_LEVEL = 0.99           # |x| >= this fraction of full-scale to be a clip candidate
CLIP_RATIO_BAD = 0.005      # >0.5% of samples on a FLAT TOP at the rail -> clipping
CORR_MATCH = 0.6            # xcorr/Pearson >= this means "this IS the reference song"
SPEC_CORR_MATCH = 0.4       # spectrogram-shape corr >= this = SAME music (phase-drift-robust)
SPECTRAL_DIV_BAD = 1.5      # mean |log10 PSD ratio| above this = strong harmonic div
THD_EXCESS_BAD = 2.0        # capture HF/total energy >= this * reference's = added harmonics
FLATTOP_MIN_RUN = 3         # >=3 consecutive samples at the same rail value = a flat top


# ---------------------------------------------------------------------------
# I/O
# ---------------------------------------------------------------------------
def _read_wav(path):
    """Return (float[-1,1] array shape [N, C], samplerate). 8/16/32-bit PCM."""
    w = wave.open(path, "rb")
    sr, ch, sw, n = w.getframerate(), w.getnchannels(), w.getsampwidth(), w.getnframes()
    raw = w.readframes(n)
    w.close()
    if sw == 2:
        a = np.frombuffer(raw, dtype="<i2").astype(np.float64) / 32768.0
    elif sw == 1:
        a = (np.frombuffer(raw, dtype="u1").astype(np.float64) - 128.0) / 128.0
    elif sw == 4:
        a = np.frombuffer(raw, dtype="<i4").astype(np.float64) / 2147483648.0
    else:
        raise ValueError(f"unsupported sample width {sw} bytes in {path}")
    a = a.reshape(-1, ch)
    return a, sr


def load_audio(path):
    """Load any ffmpeg-decodable file as (float[-1,1] [N,C], samplerate)."""
    if path.lower().endswith(".wav"):
        try:
            return _read_wav(path)
        except Exception:
            pass  # fall through to ffmpeg (e.g. float/extensible WAV)
    tmp = tempfile.NamedTemporaryFile(suffix=".wav", delete=False)
    tmp.close()
    try:
        subprocess.run(
            ["ffmpeg", "-v", "error", "-y", "-i", path, "-c:a", "pcm_s16le", tmp.name],
            check=True,
        )
        return _read_wav(tmp.name)
    finally:
        os.unlink(tmp.name)


def to_mono(a):
    return a.mean(axis=1) if a.ndim == 2 and a.shape[1] > 1 else a.reshape(-1)


def resample_to(x, sr_from, sr_to):
    if sr_from == sr_to:
        return x
    n = int(round(len(x) * sr_to / sr_from))
    return signal.resample(x, n)


# ---------------------------------------------------------------------------
# Alignment
# ---------------------------------------------------------------------------
def best_lag(cap, ref, sr, max_lag_s=10.0):
    """Cross-correlate (envelope-whitened) to find the integer lag (samples) that
    best aligns capture to reference. Positive lag => capture is delayed.

    Returns (lag_samples, normalized_xcorr_peak in [-1,1])."""
    # mean-subtract; correlate on a length-capped slice for speed
    cap0 = cap - cap.mean()
    ref0 = ref - ref.mean()
    # limit work: use up to ~30 s from each
    lim = int(30 * sr)
    c = cap0[:lim]
    r = ref0[:lim]
    xc = signal.correlate(c, r, mode="full", method="fft")
    norm = np.sqrt(np.dot(c, c) * np.dot(r, r)) + 1e-30
    xc = xc / norm
    mid = len(r) - 1
    lo = mid - int(max_lag_s * sr)
    hi = mid + int(max_lag_s * sr)
    lo = max(lo, 0)
    hi = min(hi, len(xc))
    seg = xc[lo:hi]
    k = int(np.argmax(np.abs(seg))) + lo
    lag = k - mid
    return lag, float(xc[k])


def align(cap, ref, lag):
    """Apply integer lag and crop both to the common overlap."""
    if lag > 0:
        cap = cap[lag:]
    elif lag < 0:
        ref = ref[-lag:]
    n = min(len(cap), len(ref))
    return cap[:n], ref[:n]


# ---------------------------------------------------------------------------
# Metrics
# ---------------------------------------------------------------------------
def clip_stats(x):
    """Measure the FLAT-TOP clipping signature, not mere peak dwell.

    A clean tone (e.g. a sine) spends a fair fraction of time near its peak, but
    the samples there are all *different* (smooth curvature). HARD CLIPPING pins
    runs of *consecutive identical* samples at the rail (a flat top). So we count
    only samples that are (a) near the extreme AND (b) part of a run of >=
    FLATTOP_MIN_RUN consecutive equal-valued samples.

    Returns (clip_ratio, crest_db, peak, rms).
    """
    n = len(x)
    peak = float(np.max(np.abs(x))) + 1e-30
    rms = float(np.sqrt(np.mean(x ** 2))) + 1e-30
    crest_db = 20.0 * np.log10(peak / rms)
    if n < FLATTOP_MIN_RUN or peak < 1e-6:
        return 0.0, float(crest_db), float(peak), float(rms)

    # candidates: samples near the extreme (within a tiny epsilon of +-peak)
    near = np.abs(np.abs(x) - peak) <= 1e-4 * peak + 1e-6
    # flat top: consecutive samples equal to their neighbour AND near the extreme
    same_as_next = np.empty(n, dtype=bool)
    same_as_next[:-1] = np.abs(np.diff(x)) <= 1e-4 * peak + 1e-9
    same_as_next[-1] = False
    # mark indices belonging to a run of length >= FLATTOP_MIN_RUN of equal samples
    flat = near & same_as_next
    # require a minimum run length: erode by AND-ing shifted copies
    run = flat.copy()
    acc = flat.astype(np.int32)
    for shift in range(1, FLATTOP_MIN_RUN):
        acc[:-shift] += flat[shift:]
    run = acc >= (FLATTOP_MIN_RUN - 1)  # start-of-run markers
    # count every sample covered by a qualifying run (expand the markers forward)
    covered = np.zeros(n, dtype=bool)
    starts = np.where(run & near)[0]
    for s in starts:
        e = s
        while e + 1 < n and same_as_next[e] and near[e + 1]:
            e += 1
        covered[s:e + 1] = True
    clip_ratio = float(np.mean(covered))
    return clip_ratio, float(crest_db), float(peak), float(rms)


def pearson(a, b):
    a0 = a - a.mean()
    b0 = b - b.mean()
    d = np.sqrt(np.dot(a0, a0) * np.dot(b0, b0)) + 1e-30
    return float(np.dot(a0, b0) / d)


def per_second_corr(cap, ref, sr):
    out = []
    for i in range(0, len(cap) - sr, sr):
        out.append(pearson(cap[i:i + sr], ref[i:i + sr]))
    return np.array(out) if out else np.array([0.0])


def gain_normalize(cap, ref):
    """Least-squares gain g minimizing ||cap - g*ref||; returns g and scaled ref."""
    denom = np.dot(ref, ref) + 1e-30
    g = float(np.dot(cap, ref) / denom)
    return g, g * ref


def divergence(cap, scaled_ref):
    err = cap - scaled_ref
    err_rms = np.sqrt(np.mean(err ** 2))
    sig_rms = np.sqrt(np.mean(cap ** 2)) + 1e-30
    nrmse = err_rms / sig_rms
    snr_db = 20.0 * np.log10(sig_rms / (err_rms + 1e-30))
    return float(nrmse), float(snr_db)


def welch_psd(x, sr):
    f, p = signal.welch(x, fs=sr, nperseg=min(4096, len(x)))
    return f, p


def spectral_divergence(cap, ref, sr):
    """Mean absolute log10 PSD ratio over the audible band (50 Hz–min(8k, Nyq))."""
    f, pc = welch_psd(cap, sr)
    _, pr = welch_psd(ref, sr)
    hi = min(8000.0, sr / 2.0 - 1)
    band = (f >= 50) & (f <= hi)
    # normalize each PSD to unit total band energy (remove overall gain)
    pcn = pc[band] / (pc[band].sum() + 1e-30)
    prn = pr[band] / (pr[band].sum() + 1e-30)
    ratio = np.log10((pcn + 1e-12) / (prn + 1e-12))
    return float(np.mean(np.abs(ratio)))


def hf_energy_fraction(x, sr, split_hz=4000.0):
    """Fraction of band-energy above split_hz (HF). Hard clipping raises this."""
    f, p = welch_psd(x, sr)
    hi = min(split_hz, sr / 2.0 - 1)
    total = p[(f >= 50)].sum() + 1e-30
    hf = p[(f >= hi)].sum()
    return float(hf / total)


def thd_excess(cap, ref, sr):
    """Capture HF-fraction / reference HF-fraction. >1 => capture has added
    high-frequency harmonics (clipping signature)."""
    hc = hf_energy_fraction(cap, sr)
    hr = hf_energy_fraction(ref, sr)
    return float(hc / (hr + 1e-9)), hc, hr


def spectrogram_corr(cap, ref, sr):
    """Timing-robust correlation of log-spectrogram SHAPE (per-frame spectral
    envelope, mean-subtracted). Survives sample-phase drift between two
    non-deterministic captures of the same music — where raw-waveform Pearson
    collapses. Returns a coefficient in [-1, 1].

    This is the disambiguator: if the raw correlation is low but THIS is high,
    the two signals are the SAME music, merely phase-drifted (the reference is a
    non-sample-accurate capture). If both are low, it's a genuinely different
    signal (WRONG-SIGNAL)."""
    def slog(x):
        f, ts, S = signal.spectrogram(x, fs=sr, nperseg=2048, noverlap=1024)
        return np.log(S + 1e-6)
    SA, SB = slog(cap), slog(ref)
    # align on broadband energy envelope
    ea = SA.mean(axis=0) - SA.mean()
    eb = SB.mean(axis=0) - SB.mean()
    xc = signal.correlate(ea, eb, mode="full")
    shift = int(np.argmax(xc)) - (len(eb) - 1)
    if shift >= 0:
        A, B = SA[:, shift:], SB[:, :SA.shape[1] - shift]
    else:
        A, B = SA[:, :SB.shape[1] + shift], SB[:, -shift:]
    m = min(A.shape[1], B.shape[1])
    if m < 2:
        return 0.0
    A, B = A[:, :m], B[:, :m]
    # focus on spectral SHAPE per frame (remove per-frame level / fade gain)
    A = A - A.mean(axis=0, keepdims=True)
    B = B - B.mean(axis=0, keepdims=True)
    return float(np.corrcoef(A.flatten(), B.flatten())[0, 1])


# ---------------------------------------------------------------------------
# Driver
# ---------------------------------------------------------------------------
def analyze(cap_path, ref_path):
    cap_a, cap_sr = load_audio(cap_path)
    ref_a, ref_sr = load_audio(ref_path)
    cap = to_mono(cap_a)
    ref = to_mono(ref_a)
    # work at the reference rate
    if cap_sr != ref_sr:
        cap = resample_to(cap, cap_sr, ref_sr)
    sr = ref_sr

    return analyze_arrays(cap, ref, sr, cap_path, ref_path)


def analyze_arrays(cap, ref, sr, cap_path="capture", ref_path="reference"):
    # clip stats are computed on the RAW capture (pre-alignment, pre-scaling)
    clip_ratio, crest_db, peak, rms = clip_stats(cap)

    lag, xcorr_peak = best_lag(cap, ref, sr)
    cap_al, ref_al = align(cap, ref, lag)
    if len(cap_al) < sr:  # too little overlap to be meaningful
        cap_al, ref_al = cap[:len(ref)], ref[:len(cap)]
        n = min(len(cap_al), len(ref_al))
        cap_al, ref_al = cap_al[:n], ref_al[:n]

    corr = pearson(cap_al, ref_al)
    psc = per_second_corr(cap_al, ref_al, sr)
    g, scaled_ref = gain_normalize(cap_al, ref_al)
    nrmse, snr_db = divergence(cap_al, scaled_ref)
    spec_div = spectral_divergence(cap_al, ref_al, sr)
    thd_ratio, hf_cap, hf_ref = thd_excess(cap_al, ref_al, sr)
    # spectrogram correlation is computed on the UN-aligned full signals (it does
    # its own timing alignment) so it survives sample-phase drift.
    spec_corr = spectrogram_corr(cap, ref, sr)

    res = dict(
        cap_path=cap_path, ref_path=ref_path, sr=sr,
        lag_samples=lag, lag_ms=1000.0 * lag / sr, xcorr_peak=xcorr_peak,
        spec_corr=spec_corr,
        corr=corr, corr_p50=float(np.median(psc)), corr_p10=float(np.percentile(psc, 10)),
        gain=g, nrmse=nrmse, snr_db=snr_db,
        clip_ratio=clip_ratio, crest_db=crest_db, peak=peak, rms=rms,
        spec_div=spec_div, thd_ratio=thd_ratio, hf_cap=hf_cap, hf_ref=hf_ref,
        n_overlap=len(cap_al),
    )
    res["verdict"], res["reasons"] = decide(res)
    res["per_sec_corr"] = psc.tolist()
    return res


def decide(r):
    """MATCH / CLIPPED / WRONG-SIGNAL from the measured numbers."""
    reasons = []
    # Is it even the same song? Three escalating tests:
    #  1. waveform xcorr/Pearson (needs a SAMPLE-ACCURATE reference — an offline
    #     decode of the same mogg). A clipped copy keeps xcorr high.
    #  2. spectrogram-shape corr — survives sample-phase DRIFT between two
    #     non-deterministic game captures of the same music (raw corr collapses
    #     there, but the notes-at-times structure still matches).
    sample_accurate = (abs(r["xcorr_peak"]) >= CORR_MATCH) or (r["corr"] >= CORR_MATCH)
    # The spectrogram-corr branch (for phase-drifted non-sample-accurate refs)
    # only certifies "same music" when the spectrogram SHAPE matches AND the
    # overall PSD shape is close. A genuinely different song can score a borderline
    # spectrogram-corr from shared partials, but its PSD shape (spec_div) is far
    # off — that distinguishes it. (A same-music phase-drift pair has LOW spec_div,
    # e.g. 0.2; a different song has HIGH spec_div, e.g. 4+.)
    spec_same = (r["spec_corr"] >= SPEC_CORR_MATCH) and (r["spec_div"] < SPECTRAL_DIV_BAD)
    same_music = sample_accurate or spec_same
    if not same_music:
        reasons.append(
            f"xcorr {r['xcorr_peak']:.2f}, Pearson {r['corr']:.2f}, spectrogram-corr "
            f"{r['spec_corr']:.2f} (spec-div {r['spec_div']:.2f}) -> not the reference song")
        return "WRONG-SIGNAL", reasons
    if not sample_accurate:
        # same music but phase-drifted: waveform-level divergence (SNR, NRMSE)
        # is NOT meaningful here; rely on clip-ratio + spectral metrics only.
        reasons.append(
            f"NOTE: same music by spectrogram (corr {r['spec_corr']:.2f}, spec-div "
            f"{r['spec_div']:.2f}) but raw waveform un-aligned (xcorr {r['xcorr_peak']:.2f}) "
            f"-> reference is not sample-accurate; SNR/NRMSE ignored, judging clip on "
            f"clip-ratio+spectrum")

    # It correlates with the song. Now: is it clipped/distorted?
    #
    # The decisive, false-positive-resistant signal is FLAT-TOP clipping: runs of
    # consecutive samples pinned at the rail. (A clean sine has low crest factor
    # and high near-peak dwell too, so crest factor alone must NOT decide.)
    # Spectral divergence corroborates: hard clipping injects harmonics, raising
    # both spectral-divergence and the HF-energy ratio versus the reference.
    clipped = False

    if r["clip_ratio"] >= CLIP_RATIO_BAD:
        clipped = True
        reasons.append(
            f"flat-top clip-ratio {r['clip_ratio']*100:.2f}% >= "
            f"{CLIP_RATIO_BAD*100:.1f}% (consecutive samples pinned at the rail)")

    # Spectral distortion: PSD shape differs AND extra HF harmonics are present.
    # Both together (not either alone) avoids tripping on a clean-but-different mix
    # and on pure-tone HF-ratio blow-ups from a near-zero denominator.
    if r["spec_div"] >= SPECTRAL_DIV_BAD and r["thd_ratio"] >= THD_EXCESS_BAD \
            and r["hf_cap"] >= 0.01:
        clipped = True
        reasons.append(
            f"spectral divergence {r['spec_div']:.2f} (>= {SPECTRAL_DIV_BAD}) with "
            f"HF-energy {r['thd_ratio']:.1f}x reference (>= {THD_EXCESS_BAD}) "
            f"-> harmonic distortion")

    if clipped:
        reasons.insert(0, f"correlates with the song (xcorr {r['xcorr_peak']:.2f}) "
                          f"but is distorted")
        # crest factor is reported as corroboration only
        reasons.append(f"corroborating: crest factor {r['crest_db']:.1f} dB "
                       f"(flat-topped if low), SNR {r['snr_db']:.1f} dB")
        return "CLIPPED", reasons

    reasons.append(
        f"correlates (xcorr {r['xcorr_peak']:.2f}, Pearson {r['corr']:.2f}), "
        f"flat-top clip-ratio {r['clip_ratio']*100:.2f}%, spec-div {r['spec_div']:.2f}, "
        f"SNR {r['snr_db']:.1f} dB -> clean")
    return "MATCH", reasons


# ---------------------------------------------------------------------------
# Pretty-print
# ---------------------------------------------------------------------------
def print_report(r):
    print(f"\n  capture   : {r['cap_path']}")
    print(f"  reference : {r['ref_path']}   (sr={r['sr']} Hz, overlap={r['n_overlap']/r['sr']:.1f}s)")
    print(f"\n  {'metric':<26}{'value':>14}   interpretation")
    print(f"  {'-'*26}{'-'*14}   {'-'*40}")
    rows = [
        ("alignment lag", f"{r['lag_ms']:+.1f} ms", "time-shift of capture vs ref"),
        ("xcorr peak", f"{r['xcorr_peak']:+.3f}", "aligned waveform similarity (-1..1)"),
        ("spectrogram-shape corr", f"{r['spec_corr']:+.3f}", f">= {SPEC_CORR_MATCH} = same music (drift-robust)"),
        ("Pearson corr (overall)", f"{r['corr']:+.3f}", f">= {CORR_MATCH} means it IS the song"),
        ("Pearson corr (median/s)", f"{r['corr_p50']:+.3f}", "per-second robustness"),
        ("Pearson corr (p10/s)", f"{r['corr_p10']:+.3f}", "worst-second floor"),
        ("fit gain (cap = g*ref)", f"{r['gain']:.3f}", "level mismatch (1.0 = matched)"),
        ("norm RMS error", f"{r['nrmse']:.3f}", "0 = identical after scaling"),
        ("SNR (signal/error)", f"{r['snr_db']:.1f} dB", "higher = closer to ref"),
        ("CLIP RATIO (flat-top)", f"{r['clip_ratio']*100:.2f}%", f"< {CLIP_RATIO_BAD*100:.1f}% clean / higher = clipped"),
        ("crest factor", f"{r['crest_db']:.1f} dB", "corroboration only (low+clip = flat-topped)"),
        ("peak / rms", f"{r['peak']:.3f} / {r['rms']:.3f}", "full-scale peak vs RMS"),
        ("spectral divergence", f"{r['spec_div']:.2f}", f"< {SPECTRAL_DIV_BAD} clean / higher = distorted"),
        ("HF-energy ratio (THD)", f"{r['thd_ratio']:.2f}x", f"< {THD_EXCESS_BAD} clean / higher = harmonics"),
    ]
    for name, val, interp in rows:
        print(f"  {name:<26}{val:>14}   {interp}")
    print(f"\n  ============  VERDICT: {r['verdict']}  ============")
    for why in r["reasons"]:
        print(f"    - {why}")
    print()


# ---------------------------------------------------------------------------
# Self-test
# ---------------------------------------------------------------------------
def _write_wav(path, x, sr):
    x = np.clip(x, -1.0, 1.0)
    pcm = (x * 32767.0).astype("<i2")
    w = wave.open(path, "wb")
    w.setnchannels(1)
    w.setsampwidth(2)
    w.setframerate(sr)
    w.writeframes(pcm.tobytes())
    w.close()


def selftest():
    sr = 44100
    t = np.arange(int(5 * sr)) / sr
    # Reference: 440 Hz sine at 0.8 full-scale (a clean tone)
    ref = 0.8 * np.sin(2 * np.pi * 440 * t)

    # (a) same sine -> MATCH
    same = ref.copy()
    # (b) hard-clip at 0.3 (relative to the 0.8 sine -> heavy clipping) then it
    #     pins near full-scale after the playback gain stage. We clip the raw sine
    #     at +-0.3 and then re-normalize so the rails are at +-0.99 (the game's
    #     "sum -> clamp to [-1,1]" signature).
    clipped = np.clip(ref, -0.3, 0.3)
    clipped = clipped / 0.3 * 0.99  # push flat tops to the +-0.99 rail
    # (c) white noise -> WRONG-SIGNAL
    rng = np.random.default_rng(0)
    noise = rng.normal(0, 0.3, size=len(ref))

    # Extra cases — guard against overfitting to the 3 required ones.
    # A richer "song-like" signal (multi-harmonic + slow AM envelope) so clipping
    # has somewhere to go spectrally, mirroring a real band mix.
    fund = (0.5 * np.sin(2 * np.pi * 220 * t)
            + 0.25 * np.sin(2 * np.pi * 440 * t)
            + 0.15 * np.sin(2 * np.pi * 660 * t)
            + 0.10 * np.sin(2 * np.pi * 110 * t))
    env = 0.6 + 0.4 * np.sin(2 * np.pi * 1.5 * t)   # tremolo so it isn't stationary
    song = fund * env
    song = song / (np.max(np.abs(song)) + 1e-9) * 0.7   # leave headroom

    tmp = tempfile.mkdtemp(prefix="audcorr_selftest_")
    refp = os.path.join(tmp, "ref_440.wav")
    _write_wav(refp, ref, sr)
    songp = os.path.join(tmp, "ref_song.wav")
    _write_wav(songp, song, sr)
    refmap = {id(ref): refp, id(song): songp}

    # (d) the ACTUAL game bug: a clipped copy of the real song (high corr + flat
    #     tops) — the case reproducibility-corr and SFM both miss.
    song_clipped = np.clip(song, -0.35, 0.35) / 0.35 * 0.99
    # (e) wrongly-scaled but CLEAN copy (half gain, no clipping) -> still MATCH
    song_quiet = song * 0.5
    # (f) capture of a DIFFERENT clean song -> WRONG-SIGNAL. A real different song
    #     has both different pitches AND a different temporal envelope (different
    #     note onsets / rhythm), which decorrelates the spectrogram SHAPE too.
    #     Model that: rhythmic note bursts at unrelated times + different pitches.
    rng2 = np.random.default_rng(7)
    other = np.zeros_like(t)
    note_hz = [294, 392, 494, 349, 440]   # a different melody
    pos = 0
    while pos < len(t):
        dur = int(rng2.uniform(0.15, 0.45) * sr)     # irregular note lengths
        hz = note_hz[rng2.integers(len(note_hz))]
        seg = np.arange(min(dur, len(t) - pos)) / sr
        # percussive AD envelope so the spectrogram envelope is spiky, not steady
        amp = np.exp(-seg * 8.0)
        other[pos:pos + len(seg)] += 0.7 * amp * np.sin(2 * np.pi * hz * seg)
        pos += len(seg)
    other = other / (np.max(np.abs(other)) + 1e-9) * 0.7

    cases = [("(a) same sine", same, "MATCH", ref),
             ("(b) hard-clip @0.3", clipped, "CLIPPED", ref),
             ("(c) white noise", noise, "WRONG-SIGNAL", ref),
             ("(d) clipped real song", song_clipped, "CLIPPED", song),
             ("(e) quiet clean copy", song_quiet, "MATCH", song),
             ("(f) different song", other, "WRONG-SIGNAL", song)]

    print("=" * 78)
    print("SELF-TEST: 440 Hz sine reference vs {same, hard-clipped, white-noise}")
    print("=" * 78)

    table = []
    all_ok = True
    for i, (label, sig, expected, refsig) in enumerate(cases):
        capp = os.path.join(tmp, f"case{i}.wav")
        _write_wav(capp, sig, sr)
        r = analyze(capp, refmap[id(refsig)])
        got = r["verdict"]
        ok = (got == expected)
        all_ok &= ok
        table.append((label, expected, got, ok, r))

    # detailed per-case reports
    for label, expected, got, ok, r in table:
        print(f"\n----- {label}  (expect {expected}) -----")
        print(f"  xcorr={r['xcorr_peak']:+.3f}  Pearson={r['corr']:+.3f}  "
              f"clip-ratio={r['clip_ratio']*100:.2f}%  crest={r['crest_db']:.1f}dB  "
              f"spec-div={r['spec_div']:.2f}  THD={r['thd_ratio']:.2f}x")
        print(f"  -> {got}   {'OK' if ok else 'WRONG'}")

    # summary table
    print("\n" + "=" * 78)
    print("SELF-TEST VERDICT TABLE")
    print("=" * 78)
    hdr = f"{'case':<22}{'expected':<14}{'got':<14}{'xcorr':>7}{'clip%':>8}{'crest':>8}{'specdiv':>9}{'THD':>7}  ok"
    print(hdr)
    print("-" * len(hdr))
    for label, expected, got, ok, r in table:
        print(f"{label:<22}{expected:<14}{got:<14}"
              f"{r['xcorr_peak']:>7.2f}{r['clip_ratio']*100:>8.2f}{r['crest_db']:>8.1f}"
              f"{r['spec_div']:>9.2f}{r['thd_ratio']:>7.2f}  {'OK' if ok else 'XX'}")
    print("-" * len(hdr))
    print(f"\nRESULT: {'ALL PASS — separates MATCH / CLIPPED / WRONG-SIGNAL' if all_ok else 'FAILURE — verdicts wrong'}")
    return 0 if all_ok else 1


# ---------------------------------------------------------------------------
def main():
    ap = argparse.ArgumentParser(description="Audio divergence metric (capture vs reference).")
    ap.add_argument("capture", nargs="?", help="captured WAV (game output)")
    ap.add_argument("reference", nargs="?", help="reference WAV (ground-truth decode)")
    ap.add_argument("--selftest", action="store_true", help="run synthetic self-test")
    ap.add_argument("--json", action="store_true", help="emit JSON instead of table")
    args = ap.parse_args()

    if args.selftest:
        return selftest()
    if not args.capture or not args.reference:
        ap.error("need CAPTURE and REFERENCE (or --selftest)")

    r = analyze(args.capture, args.reference)
    if args.json:
        import json
        slim = {k: v for k, v in r.items() if k != "per_sec_corr"}
        print(json.dumps(slim, indent=2))
    else:
        print_report(r)
    # exit code: 0 MATCH, 1 CLIPPED, 2 WRONG-SIGNAL
    return {"MATCH": 0, "CLIPPED": 1, "WRONG-SIGNAL": 2}[r["verdict"]]


if __name__ == "__main__":
    sys.exit(main())
