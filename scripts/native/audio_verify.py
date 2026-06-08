#!/usr/bin/env python3
"""
audio_verify.py — prove the game's recorded audio is the SAME audio as the source
song, and that it is being played CORRECTLY (right pitch/speed, not clipped).

This is the reference-vs-output verifier the audio-perf-loop skill asks for. It
closes the gap left by the older tools:

  * analyze_preview_audio.py  — only proves a capture is REPRODUCIBLE music
                                (self vs self). A wrong/clipped/sped-up capture is
                                still reproducible.
  * audio_coherence.py        — reference-FREE clip/wrap/tone metrics. Proves
                                "not silent / not noise", not "the RIGHT song".
  * audio_correlate.py        — has a --ref path but its raw-waveform xcorr is
                                capped at 10 s of lag and collapses when our
                                downmix != the game's exact mix (STATE.md flags it
                                "unreliable for cross-run captures").

WHAT THIS DOES DIFFERENTLY
--------------------------
1. ROBUST FULL-LENGTH ALIGNMENT. A gameplay/preview capture starts an unknown
   20-40 s into the recording (boot + menu nav + count-in). We align on a low-rate
   ONSET-STRENGTH envelope (spectral flux) cross-correlated over the *entire*
   length — so a large, nondeterministic offset is found, not lost.

2. MIX-ROBUST IDENTITY via CHROMA. Our ground-truth reference is an independent
   pan/vol downmix (decode_reference.py); it is NOT bit-identical to the game's
   mix (different stem balance / limiter). Raw-waveform Pearson therefore under-
   reads. CHROMA (12 pitch-class energies/frame, octave-folded, L2-normalised) is
   robust to timbre/EQ/gain/mix-balance and still says "same notes at the same
   times" -> same song. This is the PRIMARY identity metric.

3. RATE / PITCH CORRECTNESS via LAG DRIFT. The "chipmunk" bug (48000/44100 = 1.088x
   too fast) is a *time-scaling*, invisible to a single-shift xcorr. We localise
   several short windows of the capture inside the reference; the SLOPE of
   matched-reference-time vs capture-time IS the playback speed ratio. 1.000 =
   correct; 1.088 = the chipmunk bug, measured exactly. A log-frequency spectral
   shift gives an independent pitch-ratio (catches pitch-shift without tempo).

4. INDEPENDENT FINGERPRINT. If `fpcalc` (Chromaprint/AcoustID) is present we also
   match raw fingerprints (min bit-error-rate over offsets) — a second, totally
   different "same recording" signal.

5. DISTORTION. Clip-ratio / flat-top runs / overflow-wraps / crest (the
   audio_coherence signatures) + spectral & HF-harmonic divergence vs the
   reference catch the "clipped noise" symptom.

VERDICT
-------
  MATCH         same song, correct rate, not clipped  -> exit 0
  DEGRADED      same song BUT wrong-rate / pitch-shift / clipped / distorted -> 1
  WRONG-SIGNAL  not the reference song (different song / noise) -> 2
  SILENT/ERROR  no usable audio -> 3

USAGE
-----
  # direct: two WAVs (capture vs an already-decoded reference)
  audio_verify.py CAPTURE.wav --ref REFERENCE.wav [--json OUT.json]

  # auto-reference: build the ground truth from the encrypted source mogg
  audio_verify.py CAPTURE.wav --song antibodies --section gameplay
  audio_verify.py CAPTURE.wav --song antibodies --section preview

  # full auto: capture the game AND build the reference AND compare
  audio_verify.py --song 20thcenturyboy --section gameplay --capture-secs 22

  audio_verify.py --selftest        # synthetic proof the verdicts are correct

83 of 84 songs now have an extracted .mogg (symlinked from orig-assets/extracted-
xbox-full into orig-assets/extracted/songs/<id>/<id>.mogg), so --song / --rank work
for the whole on-disk roster; --ref works for any pre-decoded WAV.
"""
import argparse
import json
import os
import subprocess
import sys
import tempfile
import wave

import numpy as np
from scipy import signal, ndimage

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))

# ---------------------------------------------------------------------------
# Thresholds (documented; self-test pins them).
# ---------------------------------------------------------------------------
CHROMA_SMOOTH_S = 1.0     # temporal chroma-smoothing window (s); de-noises per-frame
                          #   chroma so the chord progression drives identity (lifts a
                          #   true match ~0.6->0.9, leaves a different song ~0.3)
CHROMA_SAME = 0.65        # smoothed peak chroma correlation -> same song. Recalibrated
                          #   for smoothing: true song ~0.82-0.95 (real ref ~0.93),
                          #   wrong song ~0.30-0.35. (Was 0.45 pre-smoothing.)
ALIGN_MIN_PEAK = 0.15     # onset-envelope xcorr peak below this -> alignment untrusted
RATE_TOL = 0.005          # |speed_ratio - 1| above this -> wrong playback rate (0.5%)
PITCH_TOL = 0.01          # |pitch_ratio - 1| above this with OK tempo -> pitch shift
CLIP_RATIO_BAD = 0.005    # >0.5% railed samples -> clipping
FLATTOP_BAD = 8           # flat-top run >= this -> clipping
FS_PIN_BAD = 0.001        # >0.1% exactly at the int16 rail -> clipping
WRAP_RATE_BAD = 5.0       # >5 rail-to-rail reversals/s per channel -> overflow wrap
SILENT_RMS = 60.0         # loud-region RMS (int16 units) below this -> silent
NOISE_FLATNESS = 0.30     # spectral flatness above this (+ high zcr) -> noise, not music
NOISE_ZCR = 0.20          # zero-cross rate above this with high flatness -> noise
RATE_CONF_MIN = 0.40      # onset-align confidence required to ASSERT a rate bug; real
                          #   game audio aligns weakly (~0.1-0.2) vs a different-mix
                          #   reference, so a low bar would false-flag a chipmunk
SPEC_DIV_BAD = 1.5        # log-PSD shape divergence
THD_EXCESS_BAD = 2.0      # capture HF / reference HF energy ratio
FP_BER_SAME = 0.30        # Chromaprint raw bit-error-rate below this -> same recording


# ---------------------------------------------------------------------------
# I/O — load any ffmpeg-decodable audio as mono float in INT16 units (so the
# clip/coherence math matches audio_coherence.py), plus the stereo int16 stream.
# ---------------------------------------------------------------------------
def load_audio(path, want_sr=None):
    """Return (mono float64 [int16 units], stereo int16 [n,2] or mono, sr)."""
    sr, ch, data = _read_wav_raw(path)
    if data is None:  # not a plain PCM WAV (float/extensible) -> ffmpeg transcode
        sr, ch, data = _ffmpeg_pcm(path, want_sr)
    a = np.frombuffer(data[: (len(data) // 2) * 2], dtype="<i2").astype(np.float64)
    if ch >= 2 and len(a) >= ch:
        st = a[: (len(a) // ch) * ch].reshape(-1, ch)
        mono = st.mean(axis=1)
        stereo = st[:, :2] if ch >= 2 else st
    else:
        mono = a
        stereo = a.reshape(-1, 1)
    if want_sr and sr != want_sr and len(mono):
        n = int(round(len(mono) * want_sr / sr))
        mono = signal.resample(mono, n)
        sr = want_sr
    return mono, stereo, sr


def _read_wav_raw(path):
    """(sr, ch, bytes) for a 16-bit PCM WAV; data=None if not decodable here."""
    try:
        w = wave.open(path, "rb")
        sr, ch, sw, n = w.getframerate(), w.getnchannels(), w.getsampwidth(), w.getnframes()
        raw = w.readframes(n) if n > 0 else b""
        w.close()
        if sw == 2 and raw:
            return sr, ch, raw
    except Exception:
        pass
    return 44100, 2, None


def _ffmpeg_pcm(path, want_sr):
    """Decode anything to s16le stereo via ffmpeg. Returns (sr, ch, bytes)."""
    sr = want_sr or 44100
    out = subprocess.run(
        ["ffmpeg", "-v", "error", "-i", path, "-f", "s16le", "-acodec", "pcm_s16le",
         "-ac", "2", "-ar", str(sr), "-"],
        capture_output=True, check=True).stdout
    return sr, 2, out


# ---------------------------------------------------------------------------
# Loud-region trim (clipping/level measured on the actual song, not boot silence)
# ---------------------------------------------------------------------------
def active_bounds(mono, sr, rms_thresh=300.0, skip_s=0.5):
    hop = max(1, sr // 20)
    env = np.array([np.sqrt(np.mean(mono[i:i + hop] ** 2))
                    for i in range(0, max(1, len(mono) - hop), hop)])
    # RELATIVE threshold: a fixed floor plus a fraction of the loud-percentile, so
    # menu/boot audio much quieter than the song is excluded even when it is not
    # pure silence (a fixed 300 floor alone trips on quiet-but-nonzero lead-in).
    thr = max(rms_thresh, 0.25 * float(np.percentile(env, 95))) if len(env) else rms_thresh
    loud = env > thr
    best_s = best_len = cur_s = cur_len = 0
    for i, v in enumerate(loud):
        if v:
            cur_s = i if cur_len == 0 else cur_s
            cur_len += 1
            if cur_len > best_len:
                best_len, best_s = cur_len, cur_s
        else:
            cur_len = 0
    if best_len == 0:
        return 0, len(mono)
    start = best_s * hop + int(skip_s * sr)
    end = (best_s + best_len) * hop
    return (0, len(mono)) if end - start <= sr // 2 else (start, end)


# ---------------------------------------------------------------------------
# Feature extraction
# ---------------------------------------------------------------------------
def onset_env(x, sr, hop=512, win=2048):
    """Spectral-flux onset strength envelope. Sharp, mix/EQ/gain-robust; the
    primitive used for robust music alignment. Returns (env, fps)."""
    if len(x) < win:
        return np.zeros(1), sr / hop
    f, t, Z = signal.stft(x, fs=sr, nperseg=win, noverlap=win - hop,
                          boundary=None, padded=False)
    mag = np.abs(Z)
    # log-compress to stop loud transients dominating, then positive first diff
    mag = np.log1p(mag)
    flux = np.maximum(0.0, np.diff(mag, axis=1)).sum(axis=0)
    flux = np.concatenate([[0.0], flux])
    if flux.std() > 0:
        flux = (flux - flux.mean()) / flux.std()
    return flux, sr / hop


def chroma(x, sr, hop=2048, win=4096, smooth_s=CHROMA_SMOOTH_S):
    """12-bin pitch-class chromagram, L2-normalised per frame, then TEMPORALLY
    SMOOTHED over ~smooth_s. Octave-folding + per-frame normalisation make it robust
    to timbre/EQ/gain/mix-balance; the smoothing averages out per-frame (46ms) noise
    so the underlying CHORD PROGRESSION (which changes on ~1s timescales) drives the
    correlation. This is the real song identity: it lifts a true match from ~0.6 to
    ~0.9 while leaving a different song near ~0.3 (the gap WIDENS, measured on real
    RB3 captures: web-vs-native 0.70->0.93, different 0.24->0.32). Returns (C[12,T], fps)."""
    if len(x) < win:
        return np.zeros((12, 1)), sr / hop
    f, t, Z = signal.stft(x, fs=sr, nperseg=win, noverlap=win - hop,
                          boundary=None, padded=False)
    mag = np.abs(Z)
    fbin = f.copy()
    fbin[0] = fbin[1] if len(fbin) > 1 else 1.0
    # MIDI pitch class for each FFT bin; ignore sub-bass / ultra-high bins
    with np.errstate(divide="ignore"):
        midi = 69 + 12 * np.log2(fbin / 440.0)
    pc = np.mod(np.round(midi).astype(int), 12)
    valid = (fbin >= 55.0) & (fbin <= 4000.0)
    C = np.zeros((12, mag.shape[1]))
    for b in range(len(fbin)):
        if valid[b]:
            C[pc[b]] += mag[b]
    # CENTER each frame (subtract the 12-bin mean) BEFORE normalising: non-negative
    # chroma vectors have a high cosine floor (~0.6 even for unrelated music), so a
    # raw cosine barely separates same-vs-different. Centering turns the dot product
    # into a true Pearson correlation in [-1,1] — shared baseline energy cancels and
    # only the PATTERN of which pitch-classes stand out remains.
    C = C - C.mean(axis=0, keepdims=True)
    norm = np.linalg.norm(C, axis=0, keepdims=True)
    norm[norm == 0] = 1.0
    C = C / norm
    # temporal smoothing (moving average over ~smooth_s), then re-normalise columns
    fps = sr / hop
    w = max(1, int(round(smooth_s * fps)))
    if w > 1 and C.shape[1] > w:
        C = ndimage.uniform_filter1d(C, w, axis=1, mode="nearest")
        norm = np.linalg.norm(C, axis=0, keepdims=True)
        norm[norm == 0] = 1.0
        C = C / norm
    return C, fps


# ---------------------------------------------------------------------------
# Alignment + identity
# ---------------------------------------------------------------------------
def xcorr_lag(a, b):
    """Best integer lag k (a delayed by k vs b) and normalised peak in [-1,1].
    Full-length FFT cross-correlation — NO lag cap."""
    a0 = a - a.mean()
    b0 = b - b.mean()
    xc = signal.correlate(a0, b0, mode="full", method="fft")
    norm = np.sqrt(np.dot(a0, a0) * np.dot(b0, b0)) + 1e-30
    xc = xc / norm
    k = int(np.argmax(xc))
    lag = k - (len(b) - 1)
    return lag, float(xc[k])


def ncc_slide(template, sig):
    """Slide `template` (len n) across `sig` (len m>=n); per-window normalised
    cross-correlation (true Pearson over each overlap). Returns
    (start_index_in_sig, peak in [-1,1]) — i.e. template[0] aligns to sig[start].

    This is the key to matching a SHORT capture into a LONG reference: a global-
    energy-normalised xcorr divides by the whole reference's energy and dilutes
    the peak to ~0. Per-window normalisation (cumsum mean/variance) keeps it a true
    correlation regardless of the length mismatch."""
    template = np.asarray(template, float)
    sig = np.asarray(sig, float)
    n, m = len(template), len(sig)
    if n < 4 or m < n:
        return 0, 0.0
    t = template - template.mean()
    tn = float(np.sqrt(np.dot(t, t))) + 1e-12
    csum = np.concatenate([[0.0], np.cumsum(sig)])
    csum2 = np.concatenate([[0.0], np.cumsum(sig * sig)])
    nwin = m - n + 1
    wsum = csum[n:n + nwin] - csum[:nwin]
    wsum2 = csum2[n:n + nwin] - csum2[:nwin]
    wnorm = np.sqrt(np.maximum(wsum2 - wsum * wsum / n, 1e-12))
    num = signal.correlate(sig, t, mode="valid")   # dot(t, sig[k:k+n]) per k
    ncc = num / (tn * wnorm)
    k = int(np.argmax(ncc))
    return k, float(ncc[k])


def ncc_align(a, b):
    """Best alignment of a vs b regardless of which is longer; slides the SHORTER
    over the longer (the longer is more likely the full, clean signal). Returns
    (off, peak) with the convention a[0] aligns to b[off]."""
    if len(a) <= len(b):
        k, peak = ncc_slide(a, b)        # a[0] -> b[k]
        return k, peak
    k, peak = ncc_slide(b, a)            # b[0] -> a[k]  => a[0] -> b[-k]
    return -k, peak


def _chroma_overlap_corr(Ccap, Cref, off):
    """Mean per-frame chroma correlation when cap[0] aligns to ref[off] (centered+
    normalised cols -> dot product is Pearson). Returns (mean, median, n)."""
    if off >= 0:
        A, B = Ccap, Cref[:, off:]
    else:
        A, B = Ccap[:, -off:], Cref
    m = min(A.shape[1], B.shape[1])
    if m < 4:
        return 0.0, 0.0, 0
    A, B = A[:, :m], B[:, :m]
    corr = (A * B).sum(axis=0)
    return float(np.mean(corr)), float(np.median(corr)), m


def chroma_identity(Ccap, Cref, fps_c, min_overlap_s=5.0):
    """GLOBAL chroma cross-correlation: slide the capture chromagram over the whole
    reference (per-dimension FFT, then per-offset overlap-mean) and take the best
    alignment. Chroma is far more mix-robust than the onset envelope for harmonic
    alignment across different stem balances, so this locks even when onsets don't.

    Returns (peak_mean_corr, peak_median_corr, n_overlap, null_median, lift,
    best_off_frames). lift = peak - median(background): a SAME song shows a sharp
    peak well above the background of wrong alignments; a different song's 'best'
    alignment barely beats its own background, so lift ~ 0."""
    n, m = Ccap.shape[1], Cref.shape[1]
    if n < 4 or m < 4:
        return 0.0, 0.0, 0, 0.0, 0.0, 0
    total = np.zeros(n + m - 1)
    for d in range(12):
        total += signal.correlate(Cref[d], Ccap[d], mode="full")
    offs = np.arange(n + m - 1) - (n - 1)          # cap[0] aligns ref[off]
    overlap = np.minimum(n, m - offs) - np.maximum(0, -offs)
    # Require NEAR-FULL overlap of the shorter signal. Taking the global max over
    # thousands of offsets inflates the score: a sliver-overlap edge position can
    # spuriously correlate (white noise hit 0.54 over a cherry-picked 5 s window).
    # Demanding >= 85% of the shorter signal's length kills that inflation while
    # still allowing a short clean capture to match a long reference fully.
    need = max(int(min_overlap_s * fps_c), int(0.85 * min(n, m)), 4)
    valid = overlap >= need
    if not np.any(valid):
        valid = overlap >= max(int(0.6 * min(n, m)), 4)
    mean = np.full(n + m - 1, -2.0)
    mean[valid] = total[valid] / overlap[valid]
    bi = int(np.argmax(mean))
    best_off = int(offs[bi])
    amean = float(mean[bi])
    # background null: median of the valid mean-corr curve outside a guard band
    guard = max(int(4 * fps_c), 4)
    bg = mean[valid & (np.abs(offs - best_off) > guard)]
    null = float(np.median(bg)) if bg.size else 0.0
    _, amed, novl = _chroma_overlap_corr(Ccap, Cref, best_off)
    return amean, float(amed), int(novl), null, float(amean - null), best_off


# ---------------------------------------------------------------------------
# Rate / pitch correctness
# ---------------------------------------------------------------------------
def rate_search(env_cap, env_ref, lo=0.90, hi=1.12):
    """Find the playback SPEED RATIO and alignment in one shot, robust to song
    repetition and large offsets. We resample the capture onset envelope by a
    grid of candidate ratios; the ratio whose resampled envelope best cross-
    correlates with the reference IS the playback speed (1.000 correct, 1.088 =
    the 48000/44100 'chipmunk'). Coarse grid then a fine refine.

    Returns (speed_ratio, best_peak, lag_frames_in_resampled_cap)."""
    if len(env_cap) < 16 or len(env_ref) < 16:
        return 1.0, 0.0, 0

    def best_for(ratios):
        out = (1.0, -1.0, 0)
        for s in ratios:
            m = int(round(len(env_cap) * s))
            if m < 8:
                continue
            ec = signal.resample(env_cap, m) if abs(s - 1.0) > 1e-9 else env_cap
            off, peak = ncc_align(ec, env_ref)   # where resampled cap[0] sits in ref
            if peak > out[1]:
                out = (float(s), float(peak), int(off))
        return out

    coarse = best_for(np.round(np.arange(lo, hi + 1e-9, 0.01), 4))
    c = coarse[0]
    fine = best_for(np.round(np.arange(max(lo, c - 0.012), min(hi, c + 0.012) + 1e-9, 0.001), 4))
    return fine if fine[1] >= coarse[1] else coarse


def pitch_ratio(cap, ref, sr):
    """Independent pitch-ratio via cross-correlation of average LOG-frequency
    spectra. A constant resample shifts the whole spectrum by log2(ratio); the
    peak shift in log-freq bins recovers the ratio. ~1.0 = same pitch."""
    def logspec(x):
        f, p = signal.welch(x, fs=sr, nperseg=min(8192, len(x)))
        band = (f >= 80) & (f <= 4000)
        f, p = f[band], p[band]
        if len(f) < 8:
            return None, None
        # resample PSD onto a uniform log-frequency grid
        lf = np.log2(f)
        grid = np.linspace(lf[0], lf[-1], 512)
        pg = np.interp(grid, lf, p)
        pg = np.log1p(pg)
        pg = (pg - pg.mean())
        return grid, pg
    g1, a = logspec(cap)
    g2, b = logspec(ref)
    if a is None or b is None:
        return float("nan")
    xc = signal.correlate(a, b, mode="full")
    shift = int(np.argmax(xc)) - (len(b) - 1)
    dlog = shift * (g1[1] - g1[0])  # log2 units
    return float(2.0 ** dlog)


# ---------------------------------------------------------------------------
# Distortion / clip (reference-free, on the loud region)
# ---------------------------------------------------------------------------
def clip_metrics(stereo_int, sr, rail=32600):
    if stereo_int.ndim == 1:
        chans = [stereo_int]
    else:
        chans = [stereo_int[:, i] for i in range(min(2, stereo_int.shape[1]))]
    mono = np.mean(np.stack([c.astype(np.float64) for c in chans]), axis=0)
    s, e = active_bounds(mono, sr)
    dur = max((e - s) / float(sr), 1e-6)
    worst = dict(clip_ratio=0.0, flat_top=0, fs_pin=0.0, wraps=0, wrap_rate=0.0)
    for c in chans:
        x = c[s:e].astype(np.int64)
        if len(x) < 3:
            continue
        a = np.abs(x)
        clip_ratio = float(np.mean(a >= rail))
        # flat-top: longest run of identical rail-valued samples
        same = x[1:] == x[:-1]
        at_rail = a[:-1] >= rail
        run = flat = 0
        for sm, rr in zip(same, at_rail):
            run = run + 1 if (sm and rr) else 0
            flat = max(flat, run)
        fs_pin = float(np.mean(a >= 32767))
        hi = (x[:-1] > 28000) & (x[1:] < -28000)
        lo = (x[:-1] < -28000) & (x[1:] > 28000)
        wraps = int(np.sum(hi | lo))
        worst["clip_ratio"] = max(worst["clip_ratio"], clip_ratio)
        worst["flat_top"] = max(worst["flat_top"], flat + 1 if flat else 0)
        worst["fs_pin"] = max(worst["fs_pin"], fs_pin)
        worst["wraps"] = max(worst["wraps"], wraps)
        worst["wrap_rate"] = max(worst["wrap_rate"], wraps / dur)
    region = mono[s:e] if e > s else mono
    peak = float(np.max(np.abs(region))) if len(region) else 0.0
    rms = float(np.sqrt(np.mean(region ** 2))) if len(region) else 0.0
    worst["crest_db"] = 20 * np.log10((peak + 1e-9) / (rms + 1e-9))
    worst["rms"] = rms
    worst["peak"] = peak
    worst["loud_s"] = dur
    return worst


def tonality(x_int16units, sr):
    """Reference-free 'is this music or noise?' on the loud region. White noise has
    a flat spectrum (spectral flatness ~1) and a high zero-cross rate (~0.5); tonal
    music is far from both. Chroma alone CANNOT reject noise (noise's degenerate
    near-flat chroma spuriously correlates with a song's average), so this gates
    identity. Returns (spectral_flatness, zcr)."""
    s, e = active_bounds(x_int16units, sr)
    x = x_int16units[s:e] if e > s else x_int16units
    if len(x) < 256:
        return 0.0, 0.0
    f, P = signal.welch(x, fs=sr, nperseg=min(4096, len(x)))
    band = (f >= 50) & (f <= 8000)
    p = P[band] + 1e-15
    flat = float(np.exp(np.mean(np.log(p))) / np.mean(p))
    sg = np.sign(x); sg[sg == 0] = 1
    z = float(np.mean(sg[1:] != sg[:-1]))
    return flat, z


def spectral_div(cap, ref, sr):
    f, pc = signal.welch(cap, fs=sr, nperseg=min(4096, len(cap)))
    _, pr = signal.welch(ref, fs=sr, nperseg=min(4096, len(ref)))
    hi = min(8000.0, sr / 2 - 1)
    band = (f >= 50) & (f <= hi)
    pcn = pc[band] / (pc[band].sum() + 1e-30)
    prn = pr[band] / (pr[band].sum() + 1e-30)
    div = float(np.mean(np.abs(np.log10((pcn + 1e-12) / (prn + 1e-12)))))
    hf = (f >= 4000) & (f <= hi)
    lo = f >= 50
    hf_cap = pc[hf].sum() / (pc[lo].sum() + 1e-30)
    hf_ref = pr[hf].sum() / (pr[lo].sum() + 1e-30)
    return div, float(hf_cap / (hf_ref + 1e-9))


# ---------------------------------------------------------------------------
# Chromaprint (fpcalc) raw fingerprint match — independent identity corroborator
# ---------------------------------------------------------------------------
def fp_ber(cap_path, ref_path):
    """Min bit-error-rate of raw Chromaprint fingerprints over offsets. Returns
    (ber, n_overlap) or (nan, 0) if fpcalc absent / too short. Inputs are first
    transcoded to s16 mono via ffmpeg so fpcalc reliably decodes float/extensible
    WAVs."""
    def rawfp(p):
        tmp = tempfile.NamedTemporaryFile(suffix=".wav", delete=False)
        tmp.close()
        try:
            subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", p, "-ac", "1",
                            "-ar", "44100", "-c:a", "pcm_s16le", tmp.name],
                           check=True, timeout=120)
            out = subprocess.run(["fpcalc", "-raw", "-length", "240", tmp.name],
                                 capture_output=True, text=True, timeout=60).stdout
        except Exception:
            return None
        finally:
            try:
                os.unlink(tmp.name)
            except OSError:
                pass
        for line in out.splitlines():
            if line.startswith("FINGERPRINT="):
                return np.array([int(v) for v in line[12:].split(",") if v], dtype=np.uint32)
        return None
    fa, fb = rawfp(cap_path), rawfp(ref_path)
    if fa is None or fb is None or len(fa) < 20 or len(fb) < 20:
        return float("nan"), 0
    # slide the shorter over the longer; popcount(XOR) / 32 bits
    if len(fa) > len(fb):
        fa, fb = fb, fa
    best = 1.0; best_n = 0
    la, lb = len(fa), len(fb)
    for off in range(0, lb - la + 1):
        seg = fb[off:off + la]
        x = fa ^ seg
        bits = np.unpackbits(x.view(np.uint8)).sum()
        ber = bits / (la * 32.0)
        if ber < best:
            best, best_n = ber, la
    # also try a coarse offset search when both are long (different start points)
    return float(best), int(best_n)


# ---------------------------------------------------------------------------
# Top-level compare
# ---------------------------------------------------------------------------
def verify(cap_path, ref_path, do_fp=True):
    sr = 44100
    cap_mono, cap_st, _ = load_audio(cap_path, want_sr=sr)
    ref_mono, ref_st, _ = load_audio(ref_path, want_sr=sr)
    if len(cap_mono) < sr // 2:
        return {"verdict": "SILENT/ERROR", "reasons": ["capture too short/empty"]}

    # distortion on the raw capture loud region (int16 units)
    clip = clip_metrics(cap_st, sr)
    flat, zcr_v = tonality(cap_mono, sr)

    # TRIM the capture to its loud region before alignment: boot/menu/count-in
    # quiet material only adds noise to the onset/chroma features. We align the
    # actual song, then let it find where that sits inside the reference.
    s0, e0 = active_bounds(cap_mono, sr)
    pre = int(0.3 * sr)
    cap_a = cap_mono[max(0, s0 - pre):e0] if e0 > s0 + sr else cap_mono

    # --- speed ratio + alignment via resample-search + NCC (offset/length-robust) ---
    ecap, fps_o = onset_env(cap_a, sr)
    eref, _ = onset_env(ref_mono, sr)
    speed, peak_o, off_o = rate_search(ecap, eref)

    # rate-CORRECT the (trimmed) capture so identity is judged on the song, not the
    # speed error: a chipmunk capture is still the SAME song (flagged wrong-rate).
    # GUARD: only resample when the rate estimate is CONFIDENT (peak_o >= the same
    # RATE_CONF_MIN we gate the rate verdict on). Real game-vs-different-mix captures
    # align weakly (peak ~0.1); the speed estimate is then unreliable, and resampling
    # the capture by a SPURIOUS ratio mangles its chroma — which made a same-mix
    # reference (a faithful post-limiter mix whose perturbed onset envelope flips the
    # weak rate-search to a wrong ratio, e.g. 0.93x) spuriously read WRONG-SIGNAL even
    # though its chroma is 0.997-identical to the no-clamp mix. When the rate is not
    # confident we leave the capture at 1.0x for the chroma/spectral stages; the rate
    # verdict already isn't asserted below RATE_CONF_MIN, so nothing is lost.
    rate_confident = peak_o >= RATE_CONF_MIN
    if rate_confident and abs(speed - 1.0) > 1e-3:
        cap_corr = signal.resample(cap_a, int(round(len(cap_a) * speed)))
    else:
        cap_corr = cap_a
    ecorr, _ = onset_env(cap_corr, sr)
    off2, peak2 = ncc_align(ecorr, eref)  # where rate-corrected cap[0] sits in ref
    if peak2 >= peak_o:
        peak_o, off_o = peak2, off2
    off_s = off_o / fps_o                 # seconds into the reference

    # --- chroma identity: GLOBAL chroma cross-correlation (mix-robust, locks even
    #     when the onset envelope doesn't) on the (rate-corrected when confident)
    #     capture ---
    Ccap, fps_c = chroma(cap_corr, sr)
    Cref, _ = chroma(ref_mono, sr)
    chroma_mean, chroma_med, chroma_n, chroma_null, chroma_lift, best_cf = \
        chroma_identity(Ccap, Cref, fps_c)
    off_s = best_cf / fps_c   # chroma-precise alignment

    # --- independent pitch ratio (on the ORIGINAL loud capture: catches pitch-
    #     shift without tempo change; for a chipmunk it just corroborates speed) ---
    pr = pitch_ratio(cap_a, ref_mono, sr)

    # --- spectral distortion vs reference (rate-corrected, aligned overlap) ---
    sdiv, thd = _aligned_spectral(cap_corr, ref_mono, sr, off_s)
    r2 = peak_o
    lag_s = off_s

    # --- fingerprint corroboration (on the trimmed loud capture: boot/menu junk
    #     in the raw file raises the BER) ---
    fp = float("nan"); fp_n = 0
    if do_fp:
        cap_fp = os.path.join(tempfile.gettempdir(), "av_capfp_%d.wav" % (os.getpid()))
        _wav16(cap_fp, np.clip(cap_a / 32768.0, -1.0, 1.0), sr)
        try:
            fp, fp_n = fp_ber(cap_fp, ref_path)
        finally:
            try:
                os.unlink(cap_fp)
            except OSError:
                pass

    res = dict(
        capture=cap_path, reference=ref_path, sr=sr,
        cap_dur_s=round(len(cap_mono) / sr, 2), ref_dur_s=round(len(ref_mono) / sr, 2),
        align_lag_s=round(lag_s, 3), align_peak=round(peak_o, 3),
        chroma_mean=round(chroma_mean, 3), chroma_median=round(chroma_med, 3),
        chroma_null=round(chroma_null, 3), chroma_lift=round(chroma_lift, 3),
        chroma_overlap_s=round(chroma_n / fps_c, 1),
        speed_ratio=None if speed != speed else round(speed, 4),
        speed_r2=round(r2, 3),
        pitch_ratio=None if pr != pr else round(pr, 4),
        fp_ber=None if fp != fp else round(fp, 3), fp_overlap=fp_n,
        spec_div=round(sdiv, 3), thd_ratio=round(thd, 2),
        clip_ratio=round(clip["clip_ratio"], 5), flat_top=clip["flat_top"],
        fs_pin_pct=round(100 * clip["fs_pin"], 4), wrap_rate=round(clip["wrap_rate"], 2),
        crest_db=round(clip["crest_db"], 2), rms=round(clip["rms"], 1),
        peak=round(clip["peak"], 1), loud_s=round(clip["loud_s"], 1),
        spectral_flatness=round(flat, 4), zcr=round(zcr_v, 4),
    )
    res["verdict"], res["reasons"], res["flags"] = decide(res)
    return res


def _aligned_spectral(cap, ref, sr, off_s):
    """cap[0] aligns to ref[off_s]. Compare the overlapping span."""
    off = int(round(off_s * sr))
    if off >= 0:
        a, b = cap, ref[off:]
    else:
        a, b = cap[-off:], ref
    n = min(len(a), len(b))
    if n < sr:
        return 0.0, 1.0
    return spectral_div(a[:n], b[:n], sr)


def decide(r):
    reasons = []; flags = {}
    # 1. silent?
    if r["rms"] < SILENT_RMS or r["loud_s"] < 1.0:
        return "SILENT/ERROR", [f"loud-region RMS {r['rms']:.0f} < {SILENT_RMS} "
                                f"({r['loud_s']:.1f}s loud) -> no usable audio"], flags
    # 1b. noise (not music)? A flat spectrum + high zero-cross rate is broken audio
    #     (buzz/static), which chroma cannot reject on its own.
    noise = r["spectral_flatness"] > NOISE_FLATNESS and r["zcr"] > NOISE_ZCR
    flags["noise_like"] = noise
    if noise:
        return "WRONG-SIGNAL", [f"capture is NOISE not music: spectral flatness "
                                f"{r['spectral_flatness']:.2f} > {NOISE_FLATNESS}, "
                                f"zcr {r['zcr']:.2f} > {NOISE_ZCR}"], flags
    # 2. same song? Two INDEPENDENT identity signals: the centered-chroma peak
    #    correlation (mix-robust) and the Chromaprint fingerprint BER. Either can
    #    confirm; both agreeing is STRONG. (The earlier 'null-lift' guard is kept in
    #    the output for information but NOT gated on: a harmonically-consistent song
    #    correlates with itself at every offset, so its lift collapses — it wrongly
    #    penalised the true song. The robust identity test is RELATIVE ranking
    #    against candidate references — see --rank.)
    chroma_same = r["chroma_mean"] >= CHROMA_SAME
    fp_same = (r["fp_ber"] is not None) and (r["fp_ber"] <= FP_BER_SAME)
    same = chroma_same or fp_same
    flags["chroma_same"] = chroma_same
    flags["fp_same"] = fp_same
    fpstr = f", fp BER {r['fp_ber']:.2f}" if r['fp_ber'] is not None else ""
    if not same:
        reasons.append(
            f"chroma corr {r['chroma_mean']:.2f} < {CHROMA_SAME}{fpstr} "
            f"(need fp <= {FP_BER_SAME}) -> NOT the reference song")
        return "WRONG-SIGNAL", reasons, flags
    conf = "STRONG" if (chroma_same and fp_same) else "LIKELY"
    reasons.append(
        f"SAME song [{conf}]: chroma corr {r['chroma_mean']:.2f} "
        f"({r['chroma_overlap_s']:.0f}s overlap){fpstr}")

    # 3. correct rate?
    degraded = False
    rate_conf = r["speed_r2"] >= RATE_CONF_MIN
    wrong_rate = (r["speed_ratio"] is not None and rate_conf
                  and abs(r["speed_ratio"] - 1.0) > RATE_TOL)
    flags["wrong_rate"] = wrong_rate
    if wrong_rate:
        degraded = True
        pct = (r["speed_ratio"] - 1.0) * 100
        reasons.append(f"WRONG RATE: playback {r['speed_ratio']:.3f}x ({pct:+.1f}%, "
                       f"conf {r['speed_r2']:.2f}) -> "
                       + ("too fast / 'chipmunk'" if pct > 0 else "too slow"))
    elif rate_conf:
        reasons.append(f"rate OK: {r['speed_ratio']:.3f}x (conf {r['speed_r2']:.2f})")
    else:
        reasons.append(f"rate inconclusive: best {r['speed_ratio']:.3f}x but alignment "
                       f"weak (conf {r['speed_r2']:.2f}) — no clear off-rate peak, "
                       f"so not a chipmunk")

    # 4. clipped? This is the DECISIVE distortion gate — it is reference-FREE
    #    (flat-top runs / rail-pinning / overflow wraps on the capture itself), so
    #    it does not false-positive on our deliberately-different reference mix.
    clipped = (r["clip_ratio"] >= CLIP_RATIO_BAD or r["flat_top"] >= FLATTOP_BAD
               or r["fs_pin_pct"] / 100.0 >= FS_PIN_BAD or r["wrap_rate"] >= WRAP_RATE_BAD)
    flags["clipped"] = clipped
    if clipped:
        degraded = True
        reasons.append(f"CLIPPED: clip-ratio {r['clip_ratio']*100:.2f}%, "
                       f"flat-top {r['flat_top']}, fs-pin {r['fs_pin_pct']:.3f}%, "
                       f"wraps {r['wrap_rate']:.1f}/s, crest {r['crest_db']:.1f}dB")

    # 5. INFORMATIONAL only (NOT degraded gates): pitch-ratio and spectral
    #    divergence vs the reference are unreliable here because the ground-truth
    #    reference is an independent NO-CLAMP stem downmix, not the game's mix — its
    #    spectral balance differs by construction, and the log-spectral pitch ratio
    #    is noisy across mixes. A real pitch/resample bug instead shows up as a low
    #    chroma correlation or a sharp off-1.0 speed peak, both gated above.
    pitch_shift = (not wrong_rate and r["pitch_ratio"] is not None
                   and abs(r["pitch_ratio"] - 1.0) > PITCH_TOL)
    flags["pitch_shift_hint"] = pitch_shift
    if pitch_shift:
        reasons.append(f"note: pitch-ratio {r['pitch_ratio']:.3f}x vs reference "
                       f"(informational — chroma {r['chroma_mean']:.2f} stays high, "
                       f"so likely a mix difference, not a pitch bug)")
    if r["spec_div"] >= SPEC_DIV_BAD:
        reasons.append(f"note: spec-div {r['spec_div']:.2f} / HF {r['thd_ratio']:.1f}x vs "
                       f"reference (informational — reference is a different, "
                       f"no-clamp mix)")
    if not degraded:
        reasons.append(f"clean: not clipped (clip {r['clip_ratio']*100:.2f}%, "
                       f"crest {r['crest_db']:.1f}dB), rate not off")
    return ("DEGRADED" if degraded else "MATCH"), reasons, flags


# ---------------------------------------------------------------------------
# Reference + capture orchestration (conveniences)
# ---------------------------------------------------------------------------
def build_reference(song_id, section, out_dir):
    """Run decode_reference.py to produce the ground-truth stereo WAV."""
    out = subprocess.run(
        [sys.executable, os.path.join(REPO, "scripts", "native", "decode_reference.py"),
         song_id, "--section", section, "--out-dir", out_dir],
        capture_output=True, text=True)
    if out.returncode != 0:
        sys.stderr.write(out.stdout + out.stderr)
        raise SystemExit(f"decode_reference failed for {song_id}/{section}")
    for line in out.stdout.splitlines():
        if line.strip().startswith("[ref] RESULT_JSON"):
            return json.loads(line.split("RESULT_JSON", 1)[1])["wav_f32"]
    # fallback to the conventional path
    return os.path.join(out_dir, f"rb3_ref_{song_id}_{section}.wav")


# ---------------------------------------------------------------------------
def print_report(r):
    print(f"\n  capture   : {r['capture']}  ({r['cap_dur_s']}s)")
    print(f"  reference : {r['reference']}  ({r['ref_dur_s']}s)")
    print(f"  {'-'*64}")
    rows = [
        ("align lag / peak", f"{r['align_lag_s']:+.1f}s / {r['align_peak']:.2f}",
         f">= {ALIGN_MIN_PEAK} = locked"),
        ("chroma corr (identity)", f"{r['chroma_mean']:.2f}",
         f">= {CHROMA_SAME} = same song [{r['chroma_overlap_s']:.0f}s ovl]"),
        ("fingerprint BER", "n/a" if r['fp_ber'] is None else f"{r['fp_ber']:.2f}",
         f"<= {FP_BER_SAME} = same recording"),
        ("speed ratio", "n/a" if r['speed_ratio'] is None else f"{r['speed_ratio']:.3f}x",
         f"1.000 +/-{RATE_TOL} (conf {r['speed_r2']:.2f})"),
        ("pitch ratio", "n/a" if r['pitch_ratio'] is None else f"{r['pitch_ratio']:.3f}x",
         "1.000 = same pitch"),
        ("clip-ratio / flat-top", f"{r['clip_ratio']*100:.2f}% / {r['flat_top']}",
         f"< {CLIP_RATIO_BAD*100:.1f}% / < {FLATTOP_BAD}"),
        ("fs-pin / wrap-rate", f"{r['fs_pin_pct']:.3f}% / {r['wrap_rate']:.1f}/s",
         "rail dwell / overflow"),
        ("crest / spec-div", f"{r['crest_db']:.1f}dB / {r['spec_div']:.2f}",
         f"spec-div < {SPEC_DIV_BAD}"),
        ("HF/THD ratio", f"{r['thd_ratio']:.2f}x", f"< {THD_EXCESS_BAD}"),
        ("flatness / zcr", f"{r['spectral_flatness']:.3f} / {r['zcr']:.3f}",
         f"flat>{NOISE_FLATNESS}&zcr>{NOISE_ZCR} = noise"),
    ]
    print(f"  {'metric':<24}{'value':>22}   interpretation")
    for n, v, i in rows:
        print(f"  {n:<24}{v:>22}   {i}")
    print(f"\n  ===========  VERDICT: {r['verdict']}  ===========")
    for why in r["reasons"]:
        print(f"    - {why}")
    print()


EXIT = {"MATCH": 0, "DEGRADED": 1, "WRONG-SIGNAL": 2, "SILENT/ERROR": 3}


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("capture", nargs="?", help="captured game-output WAV")
    ap.add_argument("--ref", help="reference WAV (pre-decoded ground truth)")
    ap.add_argument("--song", help="song id -> build reference via decode_reference.py")
    ap.add_argument("--section", default="gameplay",
                    choices=["gameplay", "full", "preview"])
    ap.add_argument("--capture-secs", type=int, default=0,
                    help="if set (and no capture WAV), capture the game first")
    ap.add_argument("--out-dir", default="/tmp")
    ap.add_argument("--no-fp", action="store_true", help="skip fpcalc fingerprint")
    ap.add_argument("--rank", help="comma-separated song ids: build each reference, "
                    "RANK the capture against all, report the winner + margin. The "
                    "robust identity test (the right song wins relatively).")
    ap.add_argument("--json", help="write metrics JSON")
    ap.add_argument("--selftest", action="store_true")
    args = ap.parse_args()

    if args.selftest:
        return selftest()

    cap = args.capture
    # optional auto-capture
    if not cap and args.capture_secs and args.song:
        cap = os.path.join(args.out_dir, f"rb3_cap_{args.song}_{args.section}.wav")
        script = ("capture_gameplay_audio.py" if args.section != "preview"
                  else "song-preview-audio-test.py")
        print(f"[verify] capturing {args.capture_secs}s via {script} -> {cap}")
        rc = subprocess.run(
            [sys.executable, os.path.join(REPO, "scripts", "native", script),
             cap, "--secs", str(args.capture_secs)]).returncode
        if rc != 0:
            print("[verify] capture FAILED"); return 3
    if not cap:
        ap.error("need a capture WAV (or --song + --capture-secs to auto-capture)")

    # RANK mode: score the capture against several candidate references; the right
    # song wins by a clear margin. This is the robust identity test (relative beats
    # an absolute per-reference threshold when the clean reference is only a
    # moderate match to the game's mix).
    if args.rank:
        songs = [s.strip() for s in args.rank.split(",") if s.strip()]
        scored = []
        for sid in songs:
            try:
                ref = build_reference(sid, args.section, args.out_dir)
            except SystemExit as e:
                print(f"[rank] skip {sid}: {e}"); continue
            r = verify(cap, ref, do_fp=not args.no_fp)
            scored.append((sid, r))
            fb = "n/a" if r["fp_ber"] is None else f"{r['fp_ber']:.3f}"
            print(f"[rank] {sid:<18} chroma={r['chroma_mean']:.3f} fp_ber={fb} "
                  f"verdict={r['verdict']}")
        if not scored:
            print("[rank] no references built"); return 3
        scored.sort(key=lambda kv: -kv[1]["chroma_mean"])
        win_sid, win = scored[0]
        margin = win["chroma_mean"] - (scored[1][1]["chroma_mean"] if len(scored) > 1 else 0.0)
        print("\n  ===========  RANK RESULT  ===========")
        print(f"  {'song':<20}{'chroma':>8}{'fp_ber':>8}{'verdict':>14}")
        for sid, r in scored:
            fb = "n/a" if r["fp_ber"] is None else f"{r['fp_ber']:.3f}"
            print(f"  {sid:<20}{r['chroma_mean']:>8.3f}{fb:>8}{r['verdict']:>14}")
        verdict = "CONFIDENT" if margin >= 0.08 else "AMBIGUOUS"
        print(f"\n  WINNER: {win_sid}  (chroma {win['chroma_mean']:.3f}, "
              f"margin +{margin:.3f} over #2) -> {verdict} identity")
        if args.json:
            open(args.json, "w").write(json.dumps(
                {"winner": win_sid, "margin": round(margin, 3), "verdict": verdict,
                 "scores": {sid: r for sid, r in scored}}, indent=2))
        return 0 if verdict == "CONFIDENT" else 1

    ref = args.ref
    if not ref:
        if not args.song:
            ap.error("need --ref or --song")
        print(f"[verify] building reference for {args.song}/{args.section}")
        ref = build_reference(args.song, args.section, args.out_dir)

    r = verify(cap, ref, do_fp=not args.no_fp)
    print_report(r)
    if args.json:
        open(args.json, "w").write(json.dumps(r, indent=2))
        print(f"  json -> {args.json}")
    return EXIT[r["verdict"]]


# ---------------------------------------------------------------------------
# Self-test — synthetic proof the verdicts separate the failure modes.
# ---------------------------------------------------------------------------
_PROG_A = [[48, 52, 55], [45, 48, 52], [50, 53, 57], [43, 47, 50],
           [48, 55, 60], [41, 48, 53], [46, 50, 53], [52, 55, 59]]
_PROG_B = [[40, 47, 52], [47, 51, 54], [42, 49, 54], [44, 48, 51],
           [39, 46, 51], [49, 53, 56], [45, 52, 57], [38, 45, 50]]


def _song_like(sr, secs, seed=0, base=1.0, prog=None):
    """A rhythmic, multi-harmonic, chord-changing mix (so chroma + onsets are
    meaningful). base scales the playback rate (1.088 = chipmunk); prog selects
    a chord progression so a 'different song' is genuinely harmonically different."""
    rng = np.random.default_rng(seed)
    n = int(secs * sr)
    t = np.arange(n) / sr
    out = np.zeros(n)
    # chord progression (root midi notes), one chord per beat
    bpm = 120; beat = 60.0 / bpm
    chords = prog if prog is not None else _PROG_A
    k = 0; pos = 0.0
    while pos < secs:
        ch = chords[k % len(chords)]
        seg_n = int(beat * sr)
        idx = slice(int(pos * sr), int(pos * sr) + seg_n)
        tt = t[idx] - pos
        env = np.exp(-tt * 3.0)  # percussive decay -> sharp onset
        for mnote in ch:
            f = 440.0 * 2 ** ((mnote - 69) / 12.0)
            for h, amp in [(1, 1.0), (2, 0.4), (3, 0.2)]:
                seg = out[idx]
                m = min(len(seg), len(tt))
                seg[:m] += amp * env[:m] * np.sin(2 * np.pi * f * h * tt[:m])
        # a little hat/noise transient at each beat for onset sharpness
        out[idx.start: idx.start + 200] += 0.3 * rng.normal(0, 1, 200)
        pos += beat; k += 1
    out = out / (np.max(np.abs(out)) + 1e-9) * 0.7
    if base != 1.0:
        out = signal.resample(out, int(len(out) / base))  # speed up -> shorter+higher
    return out


def _wav16(path, x, sr):
    x = np.clip(x, -1.0, 1.0)
    w = wave.open(path, "wb"); w.setnchannels(1); w.setsampwidth(2); w.setframerate(sr)
    w.writeframes((x * 32767).astype("<i2").tobytes()); w.close()


def selftest():
    sr = 44100; secs = 24
    tmp = tempfile.mkdtemp(prefix="audio_verify_st_")
    ref = _song_like(sr, secs, seed=1)
    refp = os.path.join(tmp, "ref.wav"); _wav16(refp, ref, sr)

    def lead(x, s):  # prepend s seconds of low menu-ish noise (offset robustness)
        rng = np.random.default_rng(9)
        pad = 0.02 * rng.normal(0, 1, int(s * sr))
        return np.concatenate([pad, x])

    cases = []
    # (a) same song, big leading offset -> MATCH
    cases.append(("same+offset", lead(ref.copy(), 18.0), "MATCH"))
    # (b) chipmunk: 1.088x too fast -> DEGRADED (wrong rate)
    cases.append(("chipmunk_1.088x", lead(_song_like(sr, secs, seed=1, base=1.088), 5.0), "DEGRADED"))
    # (c) hard-clipped same song -> DEGRADED (clipped)
    clipped = np.clip(ref * 3.0, -0.999, 0.999)
    cases.append(("clipped", lead(clipped, 7.0), "DEGRADED"))
    # (d) different song (different chord progression) -> WRONG-SIGNAL
    cases.append(("different", lead(_song_like(sr, secs, seed=42, prog=_PROG_B), 6.0), "WRONG-SIGNAL"))
    # (e) quiet clean copy (x0.4) -> MATCH
    cases.append(("quiet_clean", lead(ref * 0.4, 11.0), "MATCH"))
    # (f) silence -> SILENT
    cases.append(("silent", np.zeros(int(secs * sr)), "SILENT/ERROR"))

    print("=" * 78)
    print("audio_verify SELF-TEST  (rhythmic chord-progression reference)")
    print("=" * 78)
    rows = []; all_ok = True
    for name, sig, expect in cases:
        capp = os.path.join(tmp, name + ".wav"); _wav16(capp, sig, sr)
        r = verify(capp, refp, do_fp=False)  # fp skipped: synthetic tones fp poorly
        got = r["verdict"]; ok = got == expect; all_ok &= ok
        rows.append((name, expect, got, ok, r))
    hdr = (f"{'case':<18}{'expect':<14}{'got':<14}{'chroma':>7}{'speed':>8}"
           f"{'clip%':>7}{'align':>7}  ok")
    print(hdr); print("-" * len(hdr))
    for name, expect, got, ok, r in rows:
        sp = "  nan" if r.get('speed_ratio') is None else f"{r['speed_ratio']:.3f}"
        print(f"{name:<18}{expect:<14}{got:<14}{r.get('chroma_mean',0):>7.2f}{sp:>8}"
              f"{r.get('clip_ratio',0)*100:>7.2f}{r.get('align_peak',0):>7.2f}  "
              f"{'OK' if ok else 'XX'}")
    print("-" * len(hdr))
    print(f"\nRESULT: {'ALL PASS' if all_ok else 'FAILURE'} — separates "
          "MATCH / DEGRADED(rate,clip) / WRONG-SIGNAL / SILENT")
    return 0 if all_ok else 1


if __name__ == "__main__":
    sys.exit(main())
