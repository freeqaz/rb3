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
window). **Only 3 songs have an extracted `.mogg`** today —
`20thcenturyboy` (15ch), `25or6to4` (11ch), `antibodies` (11ch) — so `--song` /
`--rank` are limited to those; `--ref WAV` works for any pre-decoded reference.

## How to read it

- **Prefer `--rank`.** The clean reference is only a *moderate* match to the
  game's mix (true song chroma ~0.55), so a per-reference absolute threshold is
  thin. Relative ranking is robust: the right song wins, the margin grades
  confidence (CONFIDENT ≥ 0.08 over #2).
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
- Ground truth: `scripts/native/{decrypt_mogg,decode_reference}.py`.
- Capture: `scripts/native/{capture_gameplay_audio,song-preview-audio-test}.py`.
- Findings + design lessons: `docs/native/audio-perf-loop/AUDIO_VERIFY_2026-06-08.md`.
- Reference-free coherence (audible/clipped, no source needed):
  `scripts/native/audio_coherence.py`.
- Deeper audio-fidelity *investigation* loop (multi-wave, orchestrated):
  the `audio-perf-loop` skill.
