#!/usr/bin/env python3
"""
dsp_shootout.py — MASTER-BUS DSP SHOOTOUT (offline, no game run, no build).

Wave-01 root cause (CONFIRMED): the summed mix bus peaks ~3.13x full scale
(reference /tmp/rb3_ref_20thcenturyboy_gameplay.wav is exactly that pre-master
mix bus: vols/pans applied, UN-clamped). The engine multiplies by a 1.1x master
gain then HARD-CLAMPS to [-1,1] -> square-wave clipping = "clipped noise."

This tool applies each candidate MASTER-BUS PROCESSOR to the reference offline
and measures the predicted engine output. The reference IS the input to the
processor (the float mix bus the engine sees right before the clamp loop at
AudioDevice.cpp:376-381). We process STEREO per-channel (matching the engine,
which iterates interleaved L/R samples) and report a candidate table.

Candidates:
  1. flat        — out = 0.30 * x                 (baseline trivial fix)
  2. countnorm   — out = (1/sqrt(N_active)) * x   (N=15 -> 0.258)
  3. softknee    — out = tanh(k*g*x)/tanh(k)       (sweep k, g picks RMS match)
     cubic       — cubic soft-clip variant
  4. limiter     — one-pole peak limiter, lookahead-free attack/release envelope
                   (sweep threshold T, attack/release ms)
  5. hybrid      — modest headroom gain (0.6) + soft-knee for residual peaks

Metrics per candidate (measured on processed output):
  - at-rail clip-ratio  (flat-top runs at +-1.0; target ~0.000)
  - flat-top run ratio  (same detector; reported explicitly)
  - peak                (must be <= 1.0 to be clip-safe)
  - RMS (dBFS)          (loudness; higher = better, the win over flat 0.30)
  - crest factor (dB)
  - Pearson corr vs UNPROCESSED reference (waveform faithfulness)
  - spectrogram-shape corr vs reference   (timbre faithfulness)
  - spectral divergence + THD (HF) ratio vs reference (added harmonic distortion)

Winner = clip-ratio ~0 AND loudest (highest RMS) AND most faithful (highest
corr, lowest added THD/spec-div).

Usage:
  python3 scripts/native/dsp_shootout.py [REFERENCE.wav]
  python3 scripts/native/dsp_shootout.py --json
"""
import argparse
import os
import subprocess
import sys

import numpy as np

# Reuse the wave-01 metric internals (clip-ratio flat-top detector, spectral
# divergence, THD, spectrogram-shape corr, Pearson) so numbers are comparable to
# audio_correlate.py / the wave-01 baselines.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import audio_correlate as ac  # noqa: E402

REF_DEFAULT = "/tmp/rb3_ref_20thcenturyboy_gameplay.wav"
N_ACTIVE = 15          # 20thcenturyboy has 15 audio channels (stems)
SR = 44100


# ---------------------------------------------------------------------------
# I/O — the reference is a 32-bit FLOAT wav (un-clamped). Load it WITHOUT the
# [-1,1] normalization audio_correlate._read_wav applies to int PCM: we need the
# true over-unity values.
# ---------------------------------------------------------------------------
def load_float_wav(path):
    """Load any wav as float64 [N, 2] preserving true amplitude (incl. >1.0).

    Uses ffmpeg -> raw f32le so a float WAV keeps its over-unity peaks. (int PCM
    would already be clamped, so a float reference is required for this tool.)"""
    p = subprocess.run(
        ["ffmpeg", "-v", "error", "-i", path, "-f", "f32le", "-ac", "2",
         "-ar", str(SR), "-c:a", "pcm_f32le", "-"],
        capture_output=True, check=True)
    a = np.frombuffer(p.stdout, dtype="<f4").astype(np.float64)
    return a.reshape(-1, 2)


# ---------------------------------------------------------------------------
# Candidate processors. Each takes stereo float [N,2] (the pre-master mix bus)
# and returns processed stereo float [N,2]. They mirror exactly what the C++
# would do per interleaved sample (stateless ones are trivially vectorized;
# the limiter carries a per-channel envelope state, as the C++ would).
# ---------------------------------------------------------------------------
def proc_flat(x, g=0.30):
    return g * x


def proc_countnorm(x, n=N_ACTIVE, headroom=1.0):
    g = min(1.0, headroom / np.sqrt(n))
    return g * x


def proc_softknee_tanh(x, k=2.0, g=1.0):
    """out = tanh(k*g*x) / tanh(k).  g = input drive, k = knee hardness.
    Bounded in (-1,1); identity-ish for small x (slope at 0 = k*g/tanh(k))."""
    return np.tanh(k * g * x) / np.tanh(k)


def proc_cubic(x, g=1.0):
    """Cubic soft-clip: for |gx|<1 -> gx - (gx)^3/3 scaled; hard past the knee.
    Classic 'overdrive' curve, normalized so the rail sits at 1.0."""
    y = g * x
    # cubic region |y| <= 1 ; saturates to +-2/3 then we renormalize to +-1
    yc = np.clip(y, -1.0, 1.0)
    out = yc - (yc ** 3) / 3.0
    # past |y|>1 it saturates at +-2/3; renormalize whole curve to reach +-1
    out = out * 1.5
    return np.clip(out, -1.0, 1.0)


def proc_limiter(x, T=0.9, attack_ms=2.0, release_ms=120.0, sr=SR, ceiling=0.995):
    """One-pole peak limiter with a per-channel gain-reduction envelope.

    Stateless across a buffer is fine for the engine (envelope persists in a
    per-device member across callbacks). Here we run the full signal in one pass.

    Algorithm (per channel, per sample i):
        level   = |x[i]|
        desired = (level > T) ? T/level : 1.0          # gain to keep peak <= T
        coeff   = (desired < env) ? a_atk : a_rel       # fast down, slow up
        env     = coeff*env + (1-coeff)*desired         # one-pole smoothing
        out[i]  = x[i] * env
    Then a final hard ceiling at +-`ceiling` catches any envelope overshoot on
    the very first attack samples (sub-millisecond, inaudible). T is the LIMIT
    threshold; with attack/release smoothing the audible result is transparent
    on transients and full-level elsewhere."""
    a_atk = np.exp(-1.0 / (sr * attack_ms / 1000.0))
    a_rel = np.exp(-1.0 / (sr * release_ms / 1000.0))
    out = np.empty_like(x)
    for ch in range(x.shape[1]):
        env = 1.0
        col = x[:, ch]
        o = out[:, ch]
        for i in range(len(col)):
            level = abs(col[i])
            desired = (T / level) if level > T else 1.0
            coeff = a_atk if desired < env else a_rel
            env = coeff * env + (1.0 - coeff) * desired
            o[i] = col[i] * env
    return np.clip(out, -ceiling, ceiling)


def proc_limiter_linked(x, T=0.9, attack_ms=2.0, release_ms=120.0, sr=SR, ceiling=0.995):
    """Stereo-LINKED limiter: one shared envelope driven by max(|L|,|R|) so the
    stereo image doesn't wander (a per-channel limiter pans the image when one
    side limits and the other doesn't). This is the recommended production form.

    Vectorized-ish but the one-pole recursion is inherently sequential, so we
    loop. State = a single `env` scalar carried across the whole stream (in the
    engine, one float member per AudioDevice)."""
    a_atk = np.exp(-1.0 / (sr * attack_ms / 1000.0))
    a_rel = np.exp(-1.0 / (sr * release_ms / 1000.0))
    out = np.empty_like(x)
    level = np.max(np.abs(x), axis=1)
    desired_all = np.where(level > T, T / np.maximum(level, 1e-12), 1.0)
    env = 1.0
    envs = np.empty(len(x))
    for i in range(len(x)):
        d = desired_all[i]
        coeff = a_atk if d < env else a_rel
        env = coeff * env + (1.0 - coeff) * d
        envs[i] = env
    out[:, 0] = x[:, 0] * envs
    out[:, 1] = x[:, 1] * envs
    return np.clip(out, -ceiling, ceiling)


def proc_hybrid(x, g=0.6, k=1.6):
    """Modest headroom gain then tanh soft-knee on the residual.
    out = tanh(k * g*x) / tanh(k)."""
    return np.tanh(k * (g * x)) / np.tanh(k)


# ---------------------------------------------------------------------------
# Measurement — compute the metrics on a processed STEREO output vs the
# UNPROCESSED reference. clip-ratio is measured per-channel (flat-top at the
# rail) then averaged; corr/spectral are computed on mono down-mix.
# ---------------------------------------------------------------------------
def measure(proc_stereo, ref_stereo, sr=SR):
    # per-channel flat-top clip ratio + peak/rms/crest on the interleaved signal
    flat = proc_stereo.flatten()
    clip_ratio, crest_db, peak, rms = ac.clip_stats(flat)

    # also count samples literally at the +-1.0 rail (independent of flat-top run
    # detector) — the "at-rail" ratio the task asks for
    at_rail = float(np.mean(np.abs(proc_stereo) >= 0.999))

    rms_dbfs = 20.0 * np.log10(rms + 1e-30)

    # faithfulness vs the unprocessed reference (mono)
    pm = proc_stereo.mean(axis=1)
    rm = ref_stereo.mean(axis=1)
    corr = ac.pearson(pm, rm)
    spec_corr = ac.spectrogram_corr(pm, rm, sr)
    spec_div = ac.spectral_divergence(pm, rm, sr)
    thd_ratio, hf_cap, hf_ref = ac.thd_excess(pm, rm, sr)

    return dict(
        clip_ratio=clip_ratio, at_rail=at_rail, peak=peak, rms=rms,
        rms_dbfs=rms_dbfs, crest_db=crest_db,
        corr=corr, spec_corr=spec_corr, spec_div=spec_div,
        thd_ratio=thd_ratio,
    )


# Choose g for tanh/cubic so processed RMS matches the reference RMS (loudness
# parity) — but never let the input drive push peaks so hard the curve flat-tops.
def fit_drive_for_rms(curve, ref_stereo, target_rms, lo=0.2, hi=6.0, iters=22):
    """Binary-search the input drive g so curve(g) has RMS ~= target_rms."""
    rm_target = target_rms
    for _ in range(iters):
        mid = 0.5 * (lo + hi)
        y = curve(ref_stereo, mid)
        r = np.sqrt(np.mean(y ** 2))
        if r < rm_target:
            lo = mid
        else:
            hi = mid
    return 0.5 * (lo + hi)


def run_shootout(ref_path):
    ref = load_float_wav(ref_path)
    ref_rms = float(np.sqrt(np.mean(ref ** 2)))
    ref_peak = float(np.max(np.abs(ref)))
    print(f"# reference: {ref_path}")
    print(f"#   shape {ref.shape}  peak {ref_peak:.4f}  rms {ref_rms:.4f} "
          f"({20*np.log10(ref_rms):.2f} dBFS)  frac|x|>1 {np.mean(np.abs(ref)>1.0)*100:.2f}%")
    print(f"#   N_active = {N_ACTIVE} stems")
    print()

    # baseline reference metrics (un-clamped) for the "loudness vs flat 0.30" delta
    ref_rms_dbfs = 20.0 * np.log10(ref_rms)

    candidates = []

    # 1. flat headroom gain 0.30
    candidates.append(("flat g=0.30", proc_flat(ref, 0.30),
                       "flat 0.30 headroom gain"))

    # 1b. flat gain chosen so peak hits exactly 1.0 (= 1/peak ~ 0.32): the most
    #     loudness the linear approach can give without clipping
    g_peak = 1.0 / ref_peak
    candidates.append((f"flat g=1/peak={g_peak:.3f}", proc_flat(ref, g_peak),
                       "flat gain set to 1/peak (loudest no-clip linear)"))

    # 2. count-normalize 1/sqrt(N)
    g_cn = 1.0 / np.sqrt(N_ACTIVE)
    candidates.append((f"countnorm 1/sqrt(15)={g_cn:.3f}", proc_countnorm(ref),
                       "1/sqrt(N_active) count normalize"))
    # 2b. 1/N (linear count) — for contrast (over-attenuates)
    candidates.append((f"countnorm 1/15={1.0/N_ACTIVE:.3f}", proc_flat(ref, 1.0/N_ACTIVE),
                       "1/N_active linear count normalize"))

    # 3. soft-knee tanh — sweep k, drive g fit to reference RMS
    for k in (1.5, 2.0, 3.0):
        curve = lambda xx, gg, kk=k: proc_softknee_tanh(xx, k=kk, g=gg)
        g = fit_drive_for_rms(curve, ref, ref_rms)
        candidates.append((f"tanh k={k} g={g:.2f}", curve(ref, g),
                           f"tanh soft-knee, drive fit to ref RMS"))
    # tanh with modest fixed drive (no RMS-match, just headroom)
    candidates.append(("tanh k=2 g=0.5", proc_softknee_tanh(ref, k=2.0, g=0.5),
                       "tanh soft-knee, fixed drive 0.5"))

    # 3b. cubic soft-clip
    for g in (0.5, 0.8):
        candidates.append((f"cubic g={g}", proc_cubic(ref, g=g),
                           f"cubic soft-clip drive {g}"))

    # 4. one-pole peak limiter (stereo-linked) — sweep threshold
    for T in (0.80, 0.90, 0.95):
        candidates.append((f"limiter T={T} (linked)",
                           proc_limiter_linked(ref, T=T, attack_ms=2.0, release_ms=120.0),
                           f"linked one-pole limiter T={T} atk2/rel120"))
    # limiter, faster/slower release for contrast
    candidates.append(("limiter T=0.9 rel60 (linked)",
                       proc_limiter_linked(ref, T=0.9, attack_ms=1.0, release_ms=60.0),
                       "linked limiter T=0.9 atk1/rel60 (faster)"))
    candidates.append(("limiter T=0.9 rel200 (linked)",
                       proc_limiter_linked(ref, T=0.9, attack_ms=5.0, release_ms=200.0),
                       "linked limiter T=0.9 atk5/rel200 (slower)"))
    # per-channel (un-linked) limiter for the stereo-image contrast note
    candidates.append(("limiter T=0.9 (per-ch)",
                       proc_limiter(ref, T=0.9, attack_ms=2.0, release_ms=120.0),
                       "PER-CHANNEL limiter (image-wander risk)"))

    # 5. hybrid: 0.6 headroom + tanh residual
    for (g, k) in ((0.6, 1.6), (0.5, 2.0)):
        candidates.append((f"hybrid g={g} k={k}", proc_hybrid(ref, g=g, k=k),
                           f"hybrid {g} headroom + tanh k={k}"))

    rows = []
    for name, out, desc in candidates:
        m = measure(out, ref)
        m["name"] = name
        m["desc"] = desc
        m["loud_vs_flat030"] = m["rms_dbfs"] - (ref_rms_dbfs + 20*np.log10(0.30))
        rows.append(m)

    return rows, ref_rms_dbfs, ref_peak


def print_table(rows, ref_rms_dbfs):
    flat030_rms_dbfs = ref_rms_dbfs + 20 * np.log10(0.30)
    print(f"# flat-0.30 baseline RMS = {flat030_rms_dbfs:.2f} dBFS "
          f"(reference un-clamped RMS {ref_rms_dbfs:.2f} dBFS)")
    print()
    hdr = (f"{'candidate':<28}{'atRail%':>8}{'flatTop%':>9}{'peak':>7}"
           f"{'RMSdBFS':>9}{'+dBvs.30':>9}{'crest':>7}{'corr':>7}{'specC':>7}"
           f"{'specDiv':>8}{'THD':>7}")
    print(hdr)
    print("-" * len(hdr))
    for m in rows:
        print(f"{m['name']:<28}{m['at_rail']*100:>8.3f}{m['clip_ratio']*100:>9.3f}"
              f"{m['peak']:>7.3f}{m['rms_dbfs']:>9.2f}{m['loud_vs_flat030']:>+9.2f}"
              f"{m['crest_db']:>7.1f}{m['corr']:>7.3f}{m['spec_corr']:>7.3f}"
              f"{m['spec_div']:>8.3f}{m['thd_ratio']:>7.2f}")
    print("-" * len(hdr))


def pick_winner(rows):
    """Winner = clip-safe (at_rail < 0.05% AND peak <= 1.0) AND loudest AND
    faithful (corr high, spec_div low). Rank clip-safe candidates by a combined
    score = RMS(dBFS) - penalty*(spec_div) - penalty2*(1-corr)."""
    safe = [m for m in rows if m["at_rail"] < 0.0005 and m["peak"] <= 1.0001]
    if not safe:
        safe = rows

    def score(m):
        # loudness dominates; faithfulness penalties keep distortion in check
        return m["rms_dbfs"] - 6.0 * max(0.0, m["spec_div"] - 0.3) \
            - 12.0 * max(0.0, 0.98 - m["corr"]) - 2.0 * max(0.0, m["thd_ratio"] - 1.3)

    ranked = sorted(safe, key=score, reverse=True)
    return ranked


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("reference", nargs="?", default=REF_DEFAULT)
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args()

    rows, ref_rms_dbfs, ref_peak = run_shootout(args.reference)

    if args.json:
        import json
        print(json.dumps({"ref_rms_dbfs": ref_rms_dbfs, "ref_peak": ref_peak,
                          "rows": rows}, indent=2))
        return 0

    print_table(rows, ref_rms_dbfs)
    print()
    ranked = pick_winner(rows)
    print("# RANKED clip-safe candidates (best first, score = loudness - distortion penalties):")
    for i, m in enumerate(ranked[:6]):
        print(f"  {i+1}. {m['name']:<26} RMS {m['rms_dbfs']:.2f} dBFS  "
              f"corr {m['corr']:.3f}  specDiv {m['spec_div']:.3f}  "
              f"THD {m['thd_ratio']:.2f}  atRail {m['at_rail']*100:.3f}%")
    print()
    print(f"# WINNER: {ranked[0]['name']}  ({ranked[0]['desc']})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
