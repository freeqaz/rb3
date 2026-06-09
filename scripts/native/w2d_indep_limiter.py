#!/usr/bin/env python3
"""
w2d_indep_limiter.py — W2-D INDEPENDENT cross-check of the peak-limiter claim.

Separate from w2d_verify_limiter.py (prior agent). Decodes via ffmpeg to f32le
(no soundfile), implements the limiter strictly from the claim's C++ pseudocode,
applies engine clamp, and re-derives every decisive number from scratch.
"""
import math
import subprocess
import sys
import numpy as np


def load_wav_f32(path):
    out = subprocess.check_output(
        ["ffprobe", "-v", "error", "-select_streams", "a:0",
         "-show_entries", "stream=sample_rate", "-of", "csv=p=0", path]
    ).decode().strip()
    sr = int(out.split(",")[0])
    raw = subprocess.check_output(
        ["ffmpeg", "-v", "error", "-i", path, "-f", "f32le",
         "-acodec", "pcm_f32le", "-ac", "2", "-"])
    a = np.frombuffer(raw, dtype="<f4").astype(np.float64).reshape(-1, 2)
    return a, sr


def st(x):
    peak = float(np.max(np.abs(x)))
    rms = float(np.sqrt(np.mean(x ** 2)))
    return dict(peak=peak,
                rms_db=20 * math.log10(rms + 1e-30),
                crest=20 * math.log10((peak + 1e-30) / (rms + 1e-30)),
                over1=100 * float(np.mean(np.abs(x) > 1.0)),
                atrail=100 * float(np.mean(np.abs(x) >= 0.999)))


def limiter(x, sr, T=0.90, atk=3.0, rel=80.0, makeup=1.0, clamp=True):
    aA = math.exp(-1.0 / (sr * atk / 1000.0))
    aR = math.exp(-1.0 / (sr * rel / 1000.0))
    L = x[:, 0]; R = x[:, 1]
    lvl = np.maximum(np.abs(L), np.abs(R))
    env = 1.0
    g = np.empty(x.shape[0])
    for i in range(x.shape[0]):
        v = lvl[i]
        des = (T / v) if v > T else 1.0
        c = aA if des < env else aR
        env = c * env + (1.0 - c) * des
        g[i] = env * makeup
    out = x * g[:, None]
    if clamp:
        np.clip(out, -1.0, 1.0, out=out)
    return out, aA, aR


def pear(a, b):
    a0 = a - a.mean(); b0 = b - b.mean()
    return float(np.dot(a0, b0) /
                 (math.sqrt(float(np.dot(a0, a0)) * float(np.dot(b0, b0))) + 1e-30))


x, sr = load_wav_f32("/tmp/rb3_ref_20thcenturyboy_gameplay.wav")
r = st(x)
print(f"REF sr={sr} frames={x.shape[0]} peak={r['peak']:.3f} "
      f"rms={r['rms_db']:.2f}dBFS crest={r['crest']:.1f} over1={r['over1']:.2f}%")
lvl = np.maximum(np.abs(x[:, 0]), np.abs(x[:, 1]))
print(f"    pass-through(level<=0.9)={100*np.mean(lvl<=0.9):.1f}%")

y, aA, aR = limiter(x, sr)
ly = st(y)
print(f"LIM  aAtk={aA:.5f} aRel={aR:.5f}")
print(f"    at-rail={ly['atrail']:.4f}% peak={ly['peak']:.3f} "
      f"rms={ly['rms_db']:.2f}dBFS crest={ly['crest']:.1f}")
print(f"    touch>=0.995={100*np.mean(np.abs(y)>=0.995):.2f}%  "
      f"corr={pear(y.flatten(), x.flatten()):.4f}")

yn, _, _ = limiter(x, sr, clamp=False)
ln = st(yn)
print(f"NOCLAMP peak={ln['peak']:.3f} over1={ln['over1']:.2f}%")

flat = np.clip(x * 0.30, -1, 1)
fs = st(flat)
print(f"FLAT0.30 rms={fs['rms_db']:.2f}dBFS  "
      f"limiter is {ly['rms_db']-fs['rms_db']:+.2f}dB louder")

# DC3 safety: -2 dB content
dc3 = x * (10 ** (-2.0 / 20.0))
dlvl = np.maximum(np.abs(dc3[:, 0]), np.abs(dc3[:, 1]))
print(f"DC3(-2dB): frames>T  rb3={100*np.mean(lvl>0.9):.1f}%  "
      f"dc3={100*np.mean(dlvl>0.9):.1f}% (dc3 should be lower)")
