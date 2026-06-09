# audio-perf-loop — proper audio fix: instant-attack limiter (2026-06-06)

> Additive note from the verification/double-check session. Does NOT modify the
> other session's `STATE.md` (which still reads "Wave 02 audio DONE — eliminated").
> **That claim was premature** — see below. The fix is now correct + verified.

## What the double-check found (Opus adversarial subagent + corrected instrument)
The landed limiter (engine `513dcd5`: one-pole peak limiter, **3 ms attack** + tanh
soft-knee) was a real improvement but **did NOT eliminate clipping** at the shipped
default. Measured on the LOUD REGION (not the diluted global stream):
- unity: **0.5–1.5% of samples pinned at ±32767**, literal square-wave artifacts
  (`…−32767,−32767,32767,32767…`), 17–25 per-channel rail-to-rail reversals/s.
- It got WORSE under overload (gain 3.0 → 471 wraps/s) — the limiter didn't hold.

**Root cause of the weakness:** a 3 ms attack on a *raw sample-peak* detector can't
duck a per-cycle bass peak — a ¼-cycle of 200 Hz is 1.25 ms < 3 ms — so peaks reach
the softclip and saturate at the rail. Architecture right, **parameters too weak**.

The prior "eliminated" verdict came from a `flat_top`-only check: the soft-knee
rounds the clip plateaus (so flat_top→0) while the signal still slams to the rail.
`flat_top` alone is blind to that; `fs_pin` (fraction at the exact rail) + per-channel
rail-to-rail are not.

## The proper fix (this session) — INSTANT-ATTACK peak limiter
`milo-native-engine/src/audio/AudioDevice.cpp` + `AudioDevice_Web.cpp` (mirrors):
the limiter gain now drops **immediately** to `kLimThreshold/level` when a peak
exceeds threshold (one-pole 80 ms release retained):
```
if (desired < env) env = desired;                 // instant attack — no overshoot
else env = aRel * env + (1.0f - aRel) * desired;  // one-pole release
out = SoftClip(sample * env);
```
**Why it's correct:** with instant attack, `env ≤ threshold/level` on the same sample
that caused the peak, so `|out| = |sample|·env ≤ threshold = 0.90` is *guaranteed* —
the output is bounded at 0.90·32767 = **29490** and can never rail, for ANY input
level. Soft-knee + hard clamp remain as untriggered backstops. Loudness-preserving
(no static trim). DC3-safe (content-adaptive, no per-game constant; only acts on the
rare over-threshold peak, so well-staged DC3 audio is untouched).

## Verification (corrected instrument, loud region)
| state | verdict | fs_pin | wraps/s | peak | crest |
|---|---|---|---|---|---|
| old 3 ms-attack limiter, unity | DEGRADED | 0.503% | 17.2 | 32767 (rail) | 13.8 |
| **instant-attack, unity (default)** | **COHERENT** | **0.000%** | **0** | **29490** | 18.8 |
| **instant-attack, gain 3.0 overload** | **COHERENT** | **0.000%** | **0** | **29490** | 16.0 |
Artifacts: `baselines/w-fix2-native-song-{unity,gain3}.json`.

- **Native: FIXED + verified.** Output bounded at 29490, 0% rail, 0 wraps, holds under 3× overload.
- **Web: same code mirrored** (`AudioDevice_Web.cpp`); web rebuilt+deployed this
  session. In-browser live-confirm is the only remaining follow-up (code identical to
  the verified native path).

## Instrument fixes (revalidation found real bugs in `audio_coherence.py`)
1. `overflow_wraps` ran on the INTERLEAVED stream → counted L↔R cross-channel jumps
   as false overflow. Now **per-channel** (the honest, higher number).
2. clip/rail/wrap now measured in the **loud region** only (global ratio was diluted
   by boot/intro/count-in silence → could read COHERENT while the loud section clipped).
3. Added **`fs_pin`** (fraction at the exact ±32767 rail) — the direct clip signal the
   soft-knee fooled `flat_top` on.
Note: `crest_db` was already correct + correctly used (clip verdict rests on
clip_ratio/flat_top/fs_pin, not crest — both clipping and limiting lower crest, so
crest alone can't decide). Reference spectrogram-corr proves "same spectral content,"
not "same song"; `fs_pin`/clip are the clipping discriminators.

## Open / follow-ups
- Commit the engine fix (2 files) — left UNCOMMITTED pending owner sign-off (shared
  engine; another session active).
- Update the canonical `STATE.md` (owned by the other session) to reflect audio is
  fixed *here*, not at `513dcd5`.
- In-browser web audio live-confirm.
- Perf/asset-stutter wave (separate from audio) — see `STATE.md` wave-03.
