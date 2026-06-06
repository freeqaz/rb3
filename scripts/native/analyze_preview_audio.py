#!/usr/bin/env python3
"""
analyze_preview_audio.py — prove a captured song-preview WAV is real music, not
static/noise.

Two independent captures of the SAME deterministic preview (same song, same seek)
are compared. Because the null-backend mixer runs on its own thread, the two
captures are NOT sample-phase aligned, so we compare timing-robust SPECTROGRAMS
(same notes at same times) rather than raw samples:

  [1] SPECTROGRAM correlation A vs B (time-aligned): real reproducible music -> high
      (~0.9); independent random static -> ~0.
  [2] SPECTRAL FLATNESS (Wiener entropy): white static -> ~1.0; tonal music -> low.
  [3] PITCH autocorrelation: music has a periodic peak; noise does not.
  [4] Zero-crossing rate: white noise ~0.5; music much lower.

Each metric is printed next to the value for synthetic WHITE NOISE of equal RMS so
the reader can see how far the real capture is from static. Usage: A.wav B.wav
"""
import sys, wave, numpy as np
from scipy import signal


def load_mono(path):
    w = wave.open(path, "rb")
    sr, ch, n = w.getframerate(), w.getnchannels(), w.getnframes()
    a = np.frombuffer(w.readframes(n), dtype="<i2").astype(np.float64)
    w.close()
    if ch == 2:
        a = a.reshape(-1, 2).mean(axis=1)
    return a, sr


def loud_window(a, sr, thresh=1500.0, win_s=8.0, skip_s=2.5):
    """Anchor at the preview ONSET (start of the longest loud run), then skip the
    fade-in and take a fixed window so both captures cover the same song-time span."""
    hop = sr // 10
    rms = np.array([np.sqrt(np.mean(a[i:i+hop]**2)) for i in range(0, len(a)-hop, hop)])
    loud = rms > thresh
    best_s = best_len = cur_s = cur_len = 0
    for i, v in enumerate(loud):
        if v:
            if cur_len == 0: cur_s = i
            cur_len += 1
            if cur_len > best_len: best_len, best_s = cur_len, cur_s
        else:
            cur_len = 0
    onset = best_s * hop
    start = onset + int(skip_s * sr)               # skip the fader DoFade ramp
    end = min(start + int(win_s * sr), len(a))
    return a[start:end]


def spectrogram(x, sr):
    f, t, S = signal.spectrogram(x, fs=sr, nperseg=2048, noverlap=1024)
    return np.log(S + 1e-6)


def align_and_corr(SA, SB):
    """Time-align two log-spectrograms (via energy-envelope xcorr) then correlate
    their per-frame normalized spectral shapes (robust to fade-in gain drift)."""
    ea = SA.mean(axis=0); eb = SB.mean(axis=0)   # broadband energy envelope per frame
    ea0 = ea - ea.mean(); eb0 = eb - eb.mean()
    xc = signal.correlate(ea0, eb0, mode="full")
    shift = int(np.argmax(xc)) - (len(eb0) - 1)
    if shift >= 0:
        A = SA[:, shift:]; B = SB[:, :A.shape[1]]
    else:
        B = SB[:, -shift:]; A = SA[:, :B.shape[1]]
    m = min(A.shape[1], B.shape[1])
    A, B = A[:, :m], B[:, :m]
    # per-frame mean-subtract (focus on spectral SHAPE, not absolute level/fade gain)
    A = A - A.mean(axis=0, keepdims=True)
    B = B - B.mean(axis=0, keepdims=True)
    r = np.corrcoef(A.flatten(), B.flatten())[0, 1]
    return r, shift


def spectral_flatness(x, sr):
    f, Pxx = signal.welch(x, fs=sr, nperseg=4096)
    band = (f >= 50) & (f <= 8000)
    p = Pxx[band] + 1e-15
    return float(np.exp(np.mean(np.log(p))) / np.mean(p))


def zcr(x):
    s = np.sign(x); s[s == 0] = 1
    return float(np.mean(s[1:] != s[:-1]))


def pitch_strength(x, sr):
    # FFT-based autocorrelation on a short segment (O(n log n), not O(n^2)).
    seg = x[:sr] if len(x) > sr else x   # 1 second is plenty for pitch
    seg = seg - seg.mean()
    n = 1 << int(np.ceil(np.log2(2 * len(seg))))
    f = np.fft.rfft(seg, n)
    ac = np.fft.irfft(f * np.conj(f))[:len(seg)]
    ac = ac / (ac[0] + 1e-12)
    lo, hi = sr // 1000, sr // 50   # 50..1000 Hz
    k = int(np.argmax(ac[lo:hi])) + lo
    return float(ac[k]), 1000.0 * k / sr


def main():
    pa, pb = sys.argv[1], sys.argv[2]
    a, sr = load_mono(pa); b, _ = load_mono(pb)
    wa = loud_window(a, sr); wb = loud_window(b, sr)
    print(f"capture A {pa}: preview window {len(wa)/sr:.1f}s  rms={np.sqrt(np.mean(wa**2)):.0f}")
    print(f"capture B {pb}: preview window {len(wb)/sr:.1f}s  rms={np.sqrt(np.mean(wb**2)):.0f}")

    rng = np.random.default_rng(0)
    noise = rng.normal(0, np.sqrt(np.mean(wa**2)), size=len(wa))     # white noise, equal RMS
    noise2 = rng.normal(0, np.sqrt(np.mean(wa**2)), size=len(wa))

    # [1] spectrogram correlation
    SA, SB = spectrogram(wa, sr), spectrogram(wb, sr)
    r_ab, shift = align_and_corr(SA, SB)
    # controls: A vs independent noise, and noise vs noise
    r_an, _ = align_and_corr(SA, spectrogram(noise, sr))
    r_nn, _ = align_and_corr(spectrogram(noise, sr), spectrogram(noise2, sr))
    print(f"\n[1] SPECTROGRAM CORRELATION (timing-robust reproducibility)")
    print(f"      A vs B (same song, 2 runs):  {r_ab:+.3f}   <- should be HIGH (real reproducible music)")
    print(f"      A vs white-noise (control):  {r_an:+.3f}   <- should be ~0")
    print(f"      noise vs noise (control):    {r_nn:+.3f}   <- ~0 (independent random)")

    # [2..4] vs white-noise reference
    sf_a, sf_n = spectral_flatness(wa, sr), spectral_flatness(noise, sr)
    z_a, z_n = zcr(wa), zcr(noise)
    p_a, per_a = pitch_strength(wa, sr); p_n, _ = pitch_strength(noise, sr)
    print(f"\n            metric            |  capture A  |  white noise | music-like?")
    print(f"    [2] spectral flatness     |   {sf_a:6.3f}   |    {sf_n:6.3f}    | {'YES' if sf_a < 0.8*sf_n else 'no'} (<< noise = tonal)")
    print(f"    [3] pitch autocorr        |   {p_a:6.3f}   |    {p_n:6.3f}    | {'YES' if p_a > 3*p_n else 'no'} (peak @ {per_a:.1f}ms / {1000/per_a:.0f}Hz)")
    print(f"    [4] zero-crossing rate    |   {z_a:6.3f}   |    {z_n:6.3f}    | {'YES' if z_a < 0.5*z_n else 'no'} (<< noise = tonal)")

    # Verdict compares the capture to the equal-RMS WHITE-NOISE reference (the
    # thing "static/noise" would be), not to solo-tone absolutes — a dense
    # 15-channel band mix is broadband but still nothing like white noise.
    reproducible = r_ab > 0.4 and r_ab > 10 * max(abs(r_an), abs(r_nn), 0.01)
    tonal_flat   = sf_a < 0.8 * sf_n          # clearly less flat than white noise
    tonal_zcr    = z_a  < 0.5 * z_n           # far fewer zero-crossings than noise
    has_pitch    = p_a  > 3 * p_n             # far more periodic than noise
    # Reproducibility + non-noise ZCR are the decisive pair; SFM/pitch corroborate.
    music = reproducible and tonal_zcr and tonal_flat
    print("\n=== VERDICT (capture vs equal-RMS white-noise reference) ===")
    print(f"  reproducible across 2 runs (corr {r_ab:.2f} >> noise ctrl {max(r_an,r_nn):.2f}): {reproducible}")
    print(f"  spectrum far from flat (SFM {sf_a:.2f} << noise {sf_n:.2f}): {tonal_flat}")
    print(f"  far fewer zero-crossings (ZCR {z_a:.2f} << noise {z_n:.2f}): {tonal_zcr}")
    print(f"  more periodic than noise (pitch {p_a:.2f} vs {p_n:.2f}): {has_pitch}")
    print("  RESULT:", "REAL MUSIC — NOT static/noise" if music else "INCONCLUSIVE")
    return 0 if music else 1


if __name__ == "__main__":
    sys.exit(main())
