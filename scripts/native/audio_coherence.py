#!/usr/bin/env python3
"""
audio_coherence.py — measure whether a captured game-audio WAV is COHERENT
(real, undistorted music) or DEGRADED (clipped / overflow-wrapped / noise),
and — when a reference PCM is supplied — how far it DIVERGES from the expected
source.

Motivation: existing tools (song-preview-audio-test.py, web-song-preview-audio.mjs)
only assert AUDIBILITY (peak>thresh, non-zero ratio). A clipped or
overflow-wrapped signal trivially passes those yet sounds like "clipped noise".
This tool measures the actual symptom.

Reference-FREE metrics (need no decrypted source — the RB3 gameplay moggs are
v0x10 encrypted, so ffmpeg can't decode them; these work on the capture alone):

  [clip]   clip_ratio          fraction of samples railed at |s|>=32600
           flat_top            longest run of identical rail-valued samples
  [wrap]   overflow_wraps      adjacent-sample jumps that cross near +full to
                               -full scale (int16 mix overflow wrapping — the
                               classic source of harsh "clipped noise")
           wrap_rate           wraps per second
  [dyn]    crest_db            20*log10(peak/rms): music ~12-20dB, hard-clipped
                               <~6dB, square wave ~3dB
           dyn_range_db        loud-second RMS spread (musical dynamics)
           dc_offset           mean / full-scale (mix bias)
  [tone]   spectral_flatness   Wiener entropy: white noise ~1.0, tonal <~0.4
           zcr                 zero-crossing rate: noise ~0.5, tonal much lower
           pitch_strength      autocorrelation peak: periodic (music) >> noise

Reference mode (optional 2nd WAV, e.g. a decrypted+decoded source or a known-good
capture): time-aligns via energy-envelope xcorr and reports a fade/gain-robust
spectrogram correlation (1.0=identical content, ~0=unrelated).

Usage:
  audio_coherence.py CAPTURE.wav [--ref REFERENCE.wav] [--json OUT.json]
                     [--rail 32600] [--skip-s 0.5]

Exit 0 = COHERENT (music, not degraded). Exit 1 = DEGRADED/INCOHERENT.
"""
import argparse, json, sys, wave
import numpy as np
from scipy import signal


def load(path):
    """Load WAV → (mono float64 in int16 units, stereo-or-mono float array, sr, ch).
    Falls back to raw int16 if the header is unfinalized (SIGTERM-killed dumps)."""
    raw = open(path, "rb").read()
    sr, ch, data = 44100, 2, None
    try:
        w = wave.open(path, "rb")
        sr, ch, n = w.getframerate(), w.getnchannels(), w.getnframes()
        if n > 0:
            data = w.readframes(n)
        w.close()
    except Exception:
        data = None
    if not data:
        data = raw[44:]
        # crude sr/ch sniff from the (unfinalized) header if present
        try:
            ch = int.from_bytes(raw[22:24], "little") or 2
            sr = int.from_bytes(raw[24:28], "little") or 44100
        except Exception:
            pass
    a = np.frombuffer(data[: (len(data) // 2) * 2], dtype="<i2").astype(np.float64)
    if ch == 2 and len(a) >= 2:
        st = a[: (len(a) // 2) * 2].reshape(-1, 2)
        mono = st.mean(axis=1)
    else:
        mono = a
    return mono, a, sr, ch


def active_bounds(mono, sr, rms_thresh=300.0, skip_s=0.5):
    """Frame-index [start, end) of the longest sustained-loud run (the actual song/
    preview), skipping a short fade-in. Clipping is measured ONLY inside this region:
    a global ratio is diluted by the boot/intro/count-in silence and can read 'clean'
    while the loud section clips."""
    hop = max(1, sr // 20)
    env = np.array([np.sqrt(np.mean(mono[i:i + hop] ** 2)) for i in range(0, max(1, len(mono) - hop), hop)])
    loud = env > rms_thresh
    best_s = best_len = cur_s = cur_len = 0
    for i, v in enumerate(loud):
        if v:
            if cur_len == 0:
                cur_s = i
            cur_len += 1
            if cur_len > best_len:
                best_len, best_s = cur_len, cur_s
        else:
            cur_len = 0
    if best_len == 0:
        return 0, len(mono)
    start = best_s * hop + int(skip_s * sr)
    end = (best_s + best_len) * hop
    if end - start <= sr // 2:
        return 0, len(mono)
    return start, end


def active_region(mono, sr, rms_thresh=300.0, skip_s=0.5):
    s, e = active_bounds(mono, sr, rms_thresh, skip_s)
    return mono[s:e]


def clip_stats(x_int, rail):
    """Operate on the raw (possibly stereo-interleaved) int16 stream."""
    a = np.abs(x_int)
    railed = a >= rail
    clip_ratio = float(np.mean(railed)) if len(a) else 0.0
    # longest run of identical rail-valued samples (flat-top)
    flat = 0
    if len(x_int):
        same = x_int[1:] == x_int[:-1]
        at_rail = a[:-1] >= rail
        run = 0
        for s, r in zip(same, at_rail):
            if s and r:
                run += 1
                flat = max(flat, run)
            else:
                run = 0
    return clip_ratio, flat + 1 if flat else 0


def overflow_wraps(x_int, near=28000):
    """Count adjacent samples that jump from near +full-scale to near -full-scale
    (or vice versa): the signature of an int16 mix sum overflowing and wrapping,
    which is what turns a loud mix into harsh buzzing 'clipped noise'. A genuine
    musical waveform almost never moves ~60000 units between two 1/44100s samples."""
    if len(x_int) < 2:
        return 0, 0.0
    d = np.diff(x_int.astype(np.int64))
    hi = (x_int[:-1] > near) & (x_int[1:] < -near)
    lo = (x_int[:-1] < -near) & (x_int[1:] > near)
    wraps = int(np.sum(hi | lo))
    return wraps, float(np.mean(np.abs(d)))


def spectral_flatness(x, sr):
    f, P = signal.welch(x, fs=sr, nperseg=min(4096, len(x) if len(x) else 4096))
    band = (f >= 50) & (f <= 8000)
    p = P[band] + 1e-15
    return float(np.exp(np.mean(np.log(p))) / np.mean(p))


def zcr(x):
    if len(x) < 2:
        return 0.0
    s = np.sign(x)
    s[s == 0] = 1
    return float(np.mean(s[1:] != s[:-1]))


def pitch_strength(x, sr):
    seg = x[:sr] if len(x) > sr else x
    if len(seg) < 64:
        return 0.0, 0.0
    seg = seg - seg.mean()
    n = 1 << int(np.ceil(np.log2(2 * len(seg))))
    f = np.fft.rfft(seg, n)
    ac = np.fft.irfft(f * np.conj(f))[: len(seg)]
    ac = ac / (ac[0] + 1e-12)
    lo, hi = sr // 1000, sr // 50
    if hi <= lo:
        return 0.0, 0.0
    k = int(np.argmax(ac[lo:hi])) + lo
    return float(ac[k]), 1000.0 * k / sr


def per_sec_rms(mono, sr):
    out = []
    for k in range(0, len(mono), sr):
        seg = mono[k:k + sr]
        if len(seg):
            out.append(float(np.sqrt(np.mean(seg ** 2))))
    return out


def spectrogram(x, sr):
    f, t, S = signal.spectrogram(x, fs=sr, nperseg=2048, noverlap=1024)
    return np.log(S + 1e-6)


def ref_corr(cap, ref, sr):
    """Fade/gain-robust spectrogram correlation between capture and reference."""
    SA, SB = spectrogram(cap, sr), spectrogram(ref, sr)
    ea, eb = SA.mean(0), SB.mean(0)
    xc = signal.correlate(ea - ea.mean(), eb - eb.mean(), mode="full")
    shift = int(np.argmax(xc)) - (len(eb) - 1)
    if shift >= 0:
        A, B = SA[:, shift:], SB[:, : SA[:, shift:].shape[1]]
    else:
        B, A = SB[:, -shift:], SA[:, : SB[:, -shift:].shape[1]]
    m = min(A.shape[1], B.shape[1])
    A, B = A[:, :m], B[:, :m]
    A = A - A.mean(0, keepdims=True)
    B = B - B.mean(0, keepdims=True)
    return float(np.corrcoef(A.flatten(), B.flatten())[0, 1])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("capture")
    ap.add_argument("--ref", default=None)
    ap.add_argument("--json", default=None)
    ap.add_argument("--rail", type=int, default=32600)
    ap.add_argument("--skip-s", type=float, default=0.5)
    args = ap.parse_args()

    mono, inter, sr, ch = load(args.capture)
    if len(mono) == 0:
        print("FAIL: empty/zero-sample capture")
        return 1
    # Deinterleave: clip/rail/wrap are PER-CHANNEL — on the interleaved stream the
    # wrap detector counts L<->R cross-channel jumps as false overflow. Measure inside
    # the loud region only (active_bounds), worst-of-L/R.
    if ch == 2 and len(inter) >= 2:
        L = inter[0::2]; R = inter[1::2]
        n = min(len(L), len(R)); L, R = L[:n], R[:n]
    else:
        L = R = inter
    s, e = active_bounds(mono, sr, skip_s=args.skip_s)
    Lr = L[s:e].astype(np.int64); Rr = R[s:e].astype(np.int64)
    region = mono[s:e] if e > s else mono
    region_dur = max((e - s) / float(sr), 1e-6)

    clipL, flatL = clip_stats(Lr, args.rail); clipR, flatR = clip_stats(Rr, args.rail)
    clip_ratio = max(clipL, clipR); flat_top = max(flatL, flatR)
    wrapsL, stepL = overflow_wraps(Lr); wrapsR, stepR = overflow_wraps(Rr)
    wraps = max(wrapsL, wrapsR); mean_step = max(stepL, stepR)
    wrap_rate = wraps / region_dur
    fs_pin = max(float(np.mean(np.abs(Lr) >= 32767)) if len(Lr) else 0.0,
                 float(np.mean(np.abs(Rr) >= 32767)) if len(Rr) else 0.0)

    peak = float(max(np.max(np.abs(Lr)) if len(Lr) else 0,
                     np.max(np.abs(Rr)) if len(Rr) else 0))
    rms = float(np.sqrt(np.mean(region ** 2))) or 1.0
    crest_db = 20 * np.log10((peak + 1e-9) / rms)
    sf = spectral_flatness(region, sr)
    z = zcr(region)
    pstr, pms = pitch_strength(region, sr)
    psec = per_sec_rms(mono, sr)
    loud = [r for r in psec if r > 300]
    dyn_db = 20 * np.log10((max(loud) + 1) / (min(loud) + 1)) if len(loud) >= 2 else 0.0
    dc = float(np.mean(region)) / 32768.0
    dur = len(inter) / float(sr * max(ch, 1))

    m = dict(file=args.capture, sr=sr, ch=ch, dur_s=round(dur, 2),
             loud_region_s=round(region_dur, 2),
             peak=peak, rms=round(rms, 1), crest_db=round(crest_db, 2),
             clip_ratio=round(clip_ratio, 5), flat_top_run=flat_top,
             fs_pin_pct=round(100 * fs_pin, 4),
             overflow_wraps=wraps, wrap_rate=round(wrap_rate, 2),
             mean_abs_step=round(mean_step, 1),
             spectral_flatness=round(sf, 4), zcr=round(z, 4),
             pitch_strength=round(pstr, 3), pitch_ms=round(pms, 1),
             dyn_range_db=round(dyn_db, 1), dc_offset=round(dc, 4))

    # Verdicts (per-channel, loud-region). fs_pin = fraction at the exact int16 rail:
    # the most direct clip signal, and the one flat_top misses when a soft-knee rounds
    # the plateaus while the signal still slams rail-to-rail.
    railed_peak = peak >= args.rail
    clipping = clip_ratio > 0.005 or flat_top >= 8 or fs_pin > 0.001
    wrapping = wrap_rate > 5.0          # >5 per-channel rail-to-rail reversals/sec
    noisy = sf > 0.5 and z > 0.30 and pstr < 0.2
    low_dynamics = crest_db < 6.0       # railed/over-compressed
    degraded = clipping or wrapping or noisy
    m["verdicts"] = dict(railed_peak=railed_peak, clipping=clipping,
                         overflow_wrapping=wrapping, noise_like=noisy,
                         low_dynamics=low_dynamics, degraded=degraded)

    if args.ref:
        rmono, _, rsr, _ = load(args.ref)
        rregion = active_region(rmono, rsr, skip_s=args.skip_s)
        if rsr != sr:  # resample reference envelope-grade (cheap) for corr
            rregion = signal.resample(rregion, int(len(rregion) * sr / rsr))
        c = ref_corr(region, rregion, sr)
        m["ref_corr"] = round(c, 3)
        m["verdicts"]["matches_reference"] = c > 0.45

    # ---- report ----
    print(f"=== audio_coherence: {args.capture} ===")
    print(f"  {dur:.1f}s file / {region_dur:.1f}s loud  {sr}Hz x{ch}  peak={peak:.0f}  rms={rms:.0f}  crest={crest_db:.1f}dB  dc={dc:+.3f}")
    print(f"  CLIP   clip_ratio={clip_ratio:.4f}  flat_top_run={flat_top}  fs_pin={100*fs_pin:.3f}%  (railed_peak={railed_peak})")
    print(f"  WRAP   overflow_wraps={wraps}/ch  rate={wrap_rate:.1f}/s  mean_step={mean_step:.0f}")
    print(f"  DYN    crest={crest_db:.1f}dB  dyn_range={dyn_db:.1f}dB")
    print(f"  TONE   flatness={sf:.3f} (noise~1)  zcr={z:.3f} (noise~.5)  pitch={pstr:.3f}@{pms:.1f}ms")
    if args.ref:
        print(f"  REF    spectrogram_corr={m['ref_corr']:.3f}  (matches_reference={m['verdicts']['matches_reference']})")
    print("  --")
    print(f"  clipping={clipping}  overflow_wrapping={wrapping}  noise_like={noisy}  low_dynamics={low_dynamics}")
    print(f"  RESULT: {'DEGRADED — ' + ', '.join(k for k,v in m['verdicts'].items() if v and k!='degraded') if degraded else 'COHERENT — music, not degraded'}")

    if args.json:
        open(args.json, "w").write(json.dumps(m, indent=2,
            default=lambda o: o.item() if hasattr(o, "item") else bool(o)))
        print(f"  json -> {args.json}")
    return 1 if degraded else 0


if __name__ == "__main__":
    sys.exit(main())
