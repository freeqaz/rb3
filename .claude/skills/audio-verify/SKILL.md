---
name: audio-verify
description: Verify the game's recorded audio is the SAME audio as the source song AND is played CORRECTLY (right pitch/speed, not clipped/noise/silent). Decrypts+decodes the source mogg into a ground-truth reference, captures the game output headless, then measures identity (mix-robust chroma cross-correlation + Chromaprint fingerprint), playback rate (resample-search → speed ratio; catches the 'chipmunk' bug), and distortion (reference-free clip/flat-top/wrap). Use after any audio change (mixer, limiter, resampler, decode, gain) to confirm the right song still plays correctly, or to investigate an audio-fidelity report. Native-first; works for web captures too.
argument-hint: "[song-id] [gameplay|preview]  (default: rank the 3 mogg songs, gameplay)"
allowed-tools: Bash, Read, Write, Edit, Grep, Glob
---

# audio-verify — reference-vs-output audio correctness check

Prove the audio the game plays is the **right song**, played **correctly**. This
is the reference-vs-output verifier (not "is it audible" — that older tools
already answer; a clipped/sped-up/wrong copy is still audible).

The engine is shared, so **debug in native** (≈3s rebuilds, headless) — bugs
reproduce identically on web. Confirm a web-specific fix in the browser once.

## TL;DR

```bash
# 0. ALWAYS sanity-check the tool first (synthetic proof the verdicts are right)
python3 scripts/native/audio_verify.py --selftest        # expect 6/6 PASS

# 1. capture the game headless (native)
python3 scripts/native/capture_gameplay_audio.py /tmp/cap.wav --secs 45      # gameplay
python3 scripts/native/song-preview-audio-test.py --out /tmp/pv.wav --secs 70 # preview (cleaner)

# 2a. RANK mode — the robust identity test (right song wins by a margin)
python3 scripts/native/audio_verify.py /tmp/cap.wav \
        --rank 20thcenturyboy,25or6to4,antibodies --section gameplay

# 2b. single reference — full metric report + verdict
python3 scripts/native/audio_verify.py /tmp/cap.wav --song 20thcenturyboy --section gameplay

# 3. TIGHT rate bound (numeric, < 0.1%) — not the flat-curve argument above
python3 scripts/native/audio_drift.py --selftest                 # prove recovery first
python3 scripts/native/audio_drift.py /tmp/cap.wav --ref /tmp/cap.wav   # self-ref drift (no clock drift?)
python3 scripts/native/audio_drift.py /tmp/cap.wav --song 20thcenturyboy --ratefit  # rule out a uniform chipmunk
```

Verdicts: **MATCH** (same song, right rate, not clipped) · **DEGRADED**
(same-but wrong-rate / clipped) · **WRONG-SIGNAL** (different song / noise) ·
**SILENT/ERROR**. Exit 0/1/2/3.

## What it measures (and why each)

| signal | how | catches |
|---|---|---|
| identity | global **centered-chroma** cross-correlation (12 pitch-classes) | same vs different song, robust to mix/EQ/gain (raw waveform Pearson fails when our downmix ≠ the game's mix) |
| identity #2 | **Chromaprint `fpcalc`** raw-fingerprint BER | independent "same recording" corroborator |
| rate | onset-envelope **resample-search** for the speed maximizing alignment | the "chipmunk" 48000/44100 = 1.088× bug, measured exactly |
| alignment | per-window **NCC** (no lag cap) | song starting 20–40s into the capture; short clip vs long reference |
| anti-noise | spectral flatness + zero-cross rate (reference-free) | broken buzz/static (chroma can't reject noise alone) |
| distortion | clip-ratio / flat-top runs / overflow-wraps / crest (reference-free) | the "clipped noise" symptom |

`spec-div`, `HF/THD`, `pitch-ratio` vs the reference are **informational only** —
our reference is a deliberately-different no-clamp stem downmix, so they read high
by construction. The reference-free clip metrics are the distortion gate; the
chroma/speed sweep is the rate gate.

## The ground truth

`decode_reference.py <song>` builds the expected stereo:
`decrypt_mogg.py` (faithful HMX-mogg AES-CTR/ByteGrinder port, validated vs the
native MOGG_DBG) → ffmpeg multichannel decode → the game's exact pan/vol downmix
(`StreamReceiver_Native.cpp` linear pan law), no clamp (full headroom). Sections:
`--section gameplay` (whole song) or `--section preview` (the `(preview a b)`
window). **83 of 84 songs now have an extracted `.mogg`** (symlinked under
`orig-assets/extracted/songs/<id>/<id>.mogg`), so `--song` / `--rank` work for the
whole on-disk roster; the always-present trio is `20thcenturyboy` (15ch),
`25or6to4` (11ch), `antibodies` (11ch); `--ref WAV` works for any pre-decoded ref.

**`--same-mix` (post-limiter reference).** `decode_reference.py <song> --same-mix`
ALSO applies a faithful Python port of the engine's master bus
(`AudioDevice.cpp PumpAudio`: pre-gain + stereo-linked one-pole peak limiter,
INSTANT attack, `kLimThreshold=0.90` / `kLimReleaseMs=80` / `kSoftKnee=0.95`
tanh saturator) and makes THAT the primary WAV (the no-clamp downmix is kept under
a `_noclamp` suffix; `--pre-gain` mirrors `DC3_AUDIO_GAIN`). This is the game's
ACTUAL post-limiter mix — its peak lands on 0.900 exactly like the native capture,
proving the chain. **But it is chroma-/fingerprint-NEUTRAL**: the limiter changes
dynamics, not which pitch-classes sound per frame, and chroma is L2-normalised +
mean-centred, so same-mix does NOT raise the identity ceiling (verified:
no-clamp↔same-mix references are 0.997 per-frame chroma-identical; preview chroma
0.537→0.538, gameplay 0.532→0.531; fp_ber actually nudges *worse* +0.005..0.012).
Use `--same-mix` for the dynamics-sensitive metrics (clip / crest / spec-div), NOT
to improve identity — that needs matching the game's per-stem level/subset.

**Rate bound — `audio_drift.py`.** `audio_verify`'s `speed_ratio` is low-confidence
on real captures (weak onset lock vs a different-mix reference → "rate inconclusive",
a flat-curve argument, not a tight bound). `scripts/native/audio_drift.py` bounds the
rate numerically two complementary ways: per-window TIME-DRIFT slope (sample-tight
but needs a strong-locking reference — use the capture-vs-ITSELF self-control) and a
`--ratefit` constant-rate chroma fit (tolerates a weak/divergent reference but only a
uniform rate). `--selftest` PROVES it recovers injected 1.000× / 1.05× / 1.088× /
0.97×. Native result: self-ref drift **0.99995× ± 0.036 % (< 0.1 %)** so no clock
drift; `--ratefit` collapses the 1.088× chipmunk → ruled out. CAVEAT: the self-ref
control measures internal time-CONSISTENCY only — it cannot catch a *constant* rate
error vs the source (a forced chipmunk capture still self-refs "correct"); the
constant-rate fit is what rules out a uniform chipmunk, and the per-window
drift-vs-source mode honestly reports INCONCLUSIVE (peaks < 0.45) rather than a
spurious off-rate when the reference diverges too far.

## How to read it

- **Chroma is TEMPORALLY SMOOTHED (~1 s, `CHROMA_SMOOTH_S`)** so the chord
  PROGRESSION drives identity, not per-frame noise. This lifts a true match to a
  convincing absolute number: same song **~0.82–0.93**, a different-key song
  ~0.30. (Before smoothing it was only ~0.55, which read as ambiguous.)
- **Use a GOLDEN reference for the strongest call.** Comparing a capture against
  the game's actual mix — i.e. a known-good *real capture* of the same song
  (native is the bar) via `--ref native_capture.wav` — beats the synthetic downmix:
  web-vs-native scores **~0.89** where web-vs-downmix is ~0.82, because it's the
  same real recording, not an independent mix. For "is the audio correct", this is
  the test: capture ≈ golden (high chroma + not clipped + same rate) ⇒ correct.
- **`--rank` still helps pick WHICH song**, but two genuinely similar songs (e.g.
  20thcenturyboy vs 25or6to4 share a key) only separate by ~0.06 — chroma can't
  split them alone, smoothed or not. Corroborate with the fingerprint BER and the
  golden reference. Capture **long** (≥20 s): a 3 s clip is too short for fpcalc and
  for a stable rate/identity read.
- **MATCH with "rate inconclusive"** is the healthy native result: a flat
  speed-curve means no chipmunk (the onset alignment is weak vs a different-mix
  reference, so the exact speed can't be pinned — but there is no off-1.0 peak).
- **A real chipmunk** instead shows a SHARP speed peak (the self-test's 1.088×
  case proves it) and DEGRADED.
- **chroma low + WRONG-SIGNAL** against the expected song = the wrong audio is
  playing (wrong song / decode corruption / heavy pitch shift) — a real bug.
- **DEGRADED clipped** = the mixer/limiter is railing — the original
  "clipped noise" bug (fixed by the content-adaptive limiter; this catches a
  regression).

## Workflow for an audio change

1. Make the change; rebuild native: `cmake --build native/build-native -j$(nproc)`
   (engine target is separate — NOT `--target rb3-native`).
2. `--selftest` (tool sanity), then capture + `--rank` for gameplay AND preview.
3. Compare to the baseline in `docs/native/audio-perf-loop/baselines/w-verify-*`.
   A fix must keep MATCH/CONFIDENT and not introduce clipping (clip-ratio ~0,
   flat-top 0). Save a new wave-stamped baseline.
4. Web confirm (only for web-specific fixes): `scripts/web/build.sh`, capture via
   `scripts/web/web-song-preview-audio.mjs`, feed the WAV to
   `audio_verify.py --ref <decoded-reference.wav>`.

## Reference
- Tool: `scripts/native/audio_verify.py` (self-contained; `--selftest`).
- Rate bound: `scripts/native/audio_drift.py` (self-control drift + `--ratefit`; `--selftest`).
- Ground truth: `scripts/native/{decrypt_mogg,decode_reference}.py` (`--same-mix` for the post-limiter mix).
- Capture: `scripts/native/{capture_gameplay_audio,song-preview-audio-test}.py`.
- Findings + design lessons: `docs/native/audio-perf-loop/AUDIO_VERIFY_2026-06-08.md`,
  `docs/native/audio-perf-loop/SAMEMIX_RATEDRIFT_2026-06-08.md` (same-mix + rate-bound hardening).
- Reference-free coherence (audible/clipped, no source needed):
  `scripts/native/audio_coherence.py`.
- Deeper audio-fidelity *investigation* loop (multi-wave, orchestrated):
  the `audio-perf-loop` skill.
