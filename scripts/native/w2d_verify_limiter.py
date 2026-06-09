#!/usr/bin/env python3
"""
W2-D independent verification of the W2-A peak-limiter claim.

Implements the EXACT limiter spec from the claim (one-pole, stereo-linked,
feed-forward, no lookahead) in Python, applied to the un-clamped float reference
mix bus (= the pre-master mix), then re-derives every reported number:
  - reference peak / RMS / crest / fraction over 1.0
  - limiter output at-rail %, flat-top@0.995 %, peak, RMS dBFS, crest
  - corr-vs-reference (Pearson), THD/HF ratio, spectral divergence
  - "limiter WITHOUT clamp peaks at 2.62x / 1.1% > 1.0" sub-claim
  - flat-0.30 baseline RMS
This is a from-scratch reimplementation (does NOT reuse dsp_shootout.py) so it
is an independent cross-check, only borrowing audio_correlate.py's metric fns.
"""
import sys, os, wave, subprocess, tempfile
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import audio_correlate as ac  # reuse vetted metric internals

REF = "/tmp/rb3_ref_20thcenturyboy_gameplay.wav"

# ---- claim constants ----
T = 0.90
ATK_MS = 3.0
REL_MS = 80.0
MAKEUP = 1.0
CLAMP = 1.0
SAFETY_CEIL = 0.995  # the "brick-wall backstop" the claim attributes flatTop% to


def load_float_wav(path):
    """Load a float (or int) WAV as [N,2] float64 WITHOUT clamping."""
    # ffmpeg to f32 raw to preserve >1.0 values (wave module can't read float WAV)
    p = subprocess.run(
        ["ffprobe", "-v", "error", "-show_entries", "stream=sample_fmt,channels",
         "-of", "default=noprint_wrappers=1:nokey=1", path],
        capture_output=True, text=True)
    out = p.stdout.split()
    # decode to raw f32le, 2ch, preserving sample values (no clamp in ffmpeg copy of float)
    tmp = tempfile.NamedTemporaryFile(suffix=".raw", delete=False)
    tmp.close()
    subprocess.run(
        ["ffmpeg", "-v", "error", "-y", "-i", path, "-f", "f32le", "-acodec",
         "pcm_f32le", "-ac", "2", tmp.name], check=True)
    a = np.fromfile(tmp.name, dtype="<f4").astype(np.float64).reshape(-1, 2)
    os.unlink(tmp.name)
    return a


def limiter(stereo, T, atk_ms, rel_ms, makeup, sr, apply_clamp=True):
    """EXACT spec: per frame level=max(|L|,|R|); desired=(level>T)?T/level:1;
    coeff=(desired<env)?aAtk:aRel; env=coeff*env+(1-coeff)*desired;
    L*=env*makeup; R*=env*makeup; then clamp to [-1,1] (if apply_clamp).

    level & desired are vectorized; only the env one-pole recursion (coeff is
    env-dependent so it cannot be a single LTI filter) stays in the loop."""
    aAtk = np.exp(-1.0 / (sr * atk_ms / 1000.0))
    aRel = np.exp(-1.0 / (sr * rel_ms / 1000.0))
    L = stereo[:, 0]
    R = stereo[:, 1]
    n = len(L)
    level = np.maximum(np.abs(L), np.abs(R))
    desired = np.where(level > T, T / np.maximum(level, 1e-30), 1.0)
    env_arr = np.empty(n)
    env = 1.0
    dlist = desired.tolist()  # python-float iteration is ~3x faster than ndarray idx
    for i in range(n):
        d = dlist[i]
        coeff = aAtk if d < env else aRel
        env = coeff * env + (1.0 - coeff) * d
        env_arr[i] = env
    g = env_arr * makeup
    outL = L * g
    outR = R * g
    pre_clamp_peak = float(np.maximum(np.abs(outL), np.abs(outR)).max())
    over = float(np.mean(np.maximum(np.abs(outL), np.abs(outR)) > 1.0))
    if apply_clamp:
        outL = np.clip(outL, -CLAMP, CLAMP)
        outR = np.clip(outR, -CLAMP, CLAMP)
    return outL, outR, aAtk, aRel, pre_clamp_peak, over


def stats(x):
    peak = float(np.max(np.abs(x)))
    rms = float(np.sqrt(np.mean(x ** 2)))
    crest = 20.0 * np.log10(peak / (rms + 1e-30))
    rms_db = 20.0 * np.log10(rms + 1e-30)
    return peak, rms, rms_db, crest


def at_rail(x, lvl=0.999):
    return float(np.mean(np.abs(x) >= lvl))


def main():
    print("Loading reference (un-clamped float mix bus):", REF)
    ref = load_float_wav(REF)
    refmono = ref.mean(axis=1)
    sr = 44100

    # ---- 1. reference profile ----
    rpeak, rrms, rrms_db, rcrest = stats(refmono)
    frac_over1 = float(np.mean(np.max(np.abs(ref), axis=1) > 1.0))
    p99 = float(np.percentile(np.max(np.abs(ref), axis=1), 99))
    med = float(np.median(np.abs(refmono)))
    print(f"\n[REFERENCE] peak={rpeak:.3f}x  RMS={rrms_db:.2f}dBFS  crest={rcrest:.1f}dB"
          f"  frac|x|>1.0={frac_over1*100:.2f}%  p99={p99:.2f}x  median|x|={med:.3f}")

    # ---- 2. coeff sanity ----
    aAtk = np.exp(-1.0 / (sr * ATK_MS / 1000.0))
    aRel = np.exp(-1.0 / (sr * REL_MS / 1000.0))
    print(f"[COEFFS]  aAtk=exp(-1/{sr*ATK_MS/1000:.1f})={aAtk:.5f}   "
          f"aRel=exp(-1/{sr*REL_MS/1000:.1f})={aRel:.5f}")

    # ---- 3. fraction passing through untouched (|level|<=T) ----
    level = np.max(np.abs(ref), axis=1)
    pass_frac = float(np.mean(level <= T))
    print(f"[PASS]    {pass_frac*100:.1f}% of frames have level<=T={T} (untouched, env->1.0)")

    # ---- 4. limiter WITH clamp ----
    print("\nRunning limiter (this takes ~30s, sample-by-sample)...")
    L, R, _, _, pcp, ov = limiter(ref, T, ATK_MS, REL_MS, MAKEUP, sr, apply_clamp=True)
    out = np.stack([L, R], axis=1)
    omono = out.mean(axis=1)
    opeak, orms, orms_db, ocrest = stats(omono)
    rail = at_rail(omono, 0.999)
    ceil_touch = float(np.mean(np.abs(out).max(axis=1) >= SAFETY_CEIL - 1e-4))
    print(f"[LIMITER+clamp] peak={opeak:.3f}  RMS={orms_db:.2f}dBFS  crest={ocrest:.1f}dB")
    print(f"                at-rail(|x|>=0.999, mono)={rail*100:.4f}%   "
          f"touch>=0.995(stereo)={ceil_touch*100:.2f}%")

    # ---- 5. limiter WITHOUT clamp -> verify 2.62x / ~1.1% sub-claim ----
    print(f"[NO-CLAMP] pre-clamp peak={pcp:.2f}x   frac>1.0={ov*100:.2f}%   "
          f"(claim: 2.62x / 1.1%)")

    # ---- 6. flat 0.30 baseline ----
    f030 = refmono * 0.30
    _, _, f030_db, _ = stats(f030)
    print(f"[FLAT0.30] RMS={f030_db:.2f}dBFS  -> limiter is +{orms_db-f030_db:.2f}dB louder")

    # ---- 7. fidelity metrics vs un-processed reference (limiter output) ----
    # align (should be ~0 lag, same source) and compute corr/specdiv/thd
    lag, xc = ac.best_lag(omono, refmono, sr)
    cap_al, ref_al = ac.align(omono, refmono, lag)
    corr = ac.pearson(cap_al, ref_al)
    specdiv = ac.spectral_divergence(cap_al, ref_al, sr)
    thd, hfc, hfr = ac.thd_excess(cap_al, ref_al, sr)
    speccorr = ac.spectrogram_corr(omono, refmono, sr)
    print(f"\n[FIDELITY vs reference] Pearson={corr:.4f}  spectrogram-corr={speccorr:.4f}"
          f"  specDiv={specdiv:.4f}  THD/HF={thd:.3f}")

    # ---- 8. flat-0.30 fidelity for comparison (should be corr~1.0) ----
    lag2, _ = ac.best_lag(f030, refmono, sr)
    f_al, r_al = ac.align(f030, refmono, lag2)
    f030_corr = ac.pearson(f_al, r_al)
    print(f"[FLAT0.30 fidelity] Pearson={f030_corr:.4f} (sanity: pure gain -> ~1.0)")

    # ---- 9. broken-build at-rail for comparison ----
    cap_broken, sr_b = ac.load_audio("/tmp/rb3_native_gameplay.wav")
    cb = ac.to_mono(cap_broken)
    cr_broken, crest_b, _, _ = ac.clip_stats(cb)
    print(f"\n[BROKEN build] flat-top clip-ratio={cr_broken*100:.2f}%  crest={crest_b:.1f}dB"
          f"  (claim: 5.33% at-rail / 10.0dB crest)")

    print("\n=== SUMMARY (claim vs measured) ===")
    print(f"  ref peak           claim 3.128   measured {rpeak:.3f}")
    print(f"  ref RMS dBFS       claim -7.63   measured {rrms_db:.2f}")
    print(f"  ref crest dB       claim 17.5    measured {rcrest:.1f}")
    print(f"  ref frac>1.0       claim 2.51    measured {frac_over1*100:.2f}")
    print(f"  aAtk               claim 0.99247 measured {aAtk:.5f}")
    print(f"  aRel               claim 0.99972 measured {aRel:.5f}")
    print(f"  pass-through %     claim 94.8    measured {pass_frac*100:.1f}")
    print(f"  lim at-rail %      claim 0.0000  measured {rail*100:.4f}")
    print(f"  lim peak           claim 0.995   measured {opeak:.3f}")
    print(f"  lim RMS dBFS       claim -8.86   measured {orms_db:.2f}")
    print(f"  lim crest dB       claim 8.8     measured {ocrest:.1f}")
    print(f"  +dB vs flat0.30    claim +9.22   measured {orms_db-f030_db:+.2f}")
    print(f"  corr vs ref        claim 0.9953  measured {corr:.4f}")
    print(f"  spectrogram corr   claim 0.9966  measured {speccorr:.4f}")
    print(f"  specDiv            claim 0.0074  measured {specdiv:.4f}")
    print(f"  THD/HF             claim 1.012   measured {thd:.3f}")
    print(f"  no-clamp peak      claim 2.62    measured {pcp:.2f}")
    print(f"  no-clamp frac>1.0  claim 1.1     measured {ov*100:.2f}")


if __name__ == "__main__":
    main()
