# Audio correctness verification — reference-vs-output (2026-06-08)

**Question (user):** is the audio the game plays the *same* audio as the source
song data, and is it *correct*? Build a reusable tool to prove it.

**Answer:** YES on native. The default song's GAMEPLAY and PREVIEW audio is the
right song (20thcenturyboy), is **not clipped, not noise, not silent, and not
chipmunk'd**. The match to the clean source is *moderate* (chroma ~0.54), which is
**expected** — the ground-truth reference is an independent no-clamp stem downmix,
not the game's exact mix — not evidence of a bug.

## The tool — `scripts/native/audio_verify.py`

Reference-vs-output verifier. Pipeline + the new ideas it adds over the older
`audio_correlate.py` / `audio_coherence.py` (which STATE.md flagged "unreliable for
cross-run captures"):

| stage | method | catches |
|---|---|---|
| ground truth | `decrypt_mogg.py` → ffmpeg → game pan/vol downmix (`decode_reference.py`) | builds the expected stereo |
| **alignment** | **onset-envelope resample-search + per-window NCC** (no lag cap, length-robust) | song starting 20–40s into the capture; matching a short clip into a long reference without energy dilution |
| **identity** | **global centered-chroma cross-correlation** (12 pitch-classes, mix/EQ/gain-robust) | same song vs different song, when the mix differs from ours |
| identity #2 | **Chromaprint (`fpcalc`) raw fingerprint BER** | independent "same recording" corroborator |
| **rate** | **resample-search for the speed maximizing onset NCC** | the "chipmunk" 48000/44100 = 1.088× bug, *measured exactly* |
| anti-noise | reference-free spectral-flatness + zero-cross rate | broken buzz/static (chroma alone can't reject noise) |
| distortion | reference-free clip-ratio / flat-top / overflow-wrap / crest | the "clipped noise" symptom |

**Verdicts:** `MATCH` (same song, right rate, not clipped) · `DEGRADED`
(same-but wrong-rate / clipped) · `WRONG-SIGNAL` (different song / noise) ·
`SILENT/ERROR`. Exit codes 0/1/2/3.

### Why these choices (lessons from building it)
- **Raw-waveform Pearson fails** when our downmix ≠ the game's mix; **chroma**
  survives because harmonic content is shared across mixes.
- **Non-negative chroma has a ~0.6 cosine floor** — *center each frame* so the dot
  product is a true Pearson correlation (same ~1.0 / different ~0.3).
- **A single global xcorr can't see a time-warp** — a *resample-search* recovers
  the exact speed ratio; a real chipmunk shows a SHARP peak at 1.088, correct
  audio shows a FLAT speed curve (measured: 0.53–0.57 across 0.94–1.12×).
- **The "null-lift" idea backfires** for harmonically-consistent songs (the true
  song self-correlates at every offset → tiny lift). Dropped as a gate.
- **Max-over-offsets inflates** short-overlap correlations (white noise hit 0.54
  over a 5s sliver) → require near-full overlap (≥85% of the shorter signal).
- **Spectral-divergence / pitch-ratio vs our reference are unreliable** (different
  mix) → informational only; the *reference-free* clip metrics are the distortion
  gate; the *chroma sweep* is the rate gate.
- **The robust identity test is RELATIVE** (`--rank`): capture once, score against
  all candidate references, the right one wins by a margin.

### Self-test (proves the verdicts before trusting them)
`audio_verify.py --selftest` → 6/6: same+18s-offset→MATCH, 1.088×→DEGRADED(rate),
hard-clip→DEGRADED(clip), different-progression→WRONG-SIGNAL, quiet-copy→MATCH,
silence→SILENT.

## Measured result (native, build 2026-06-08, default song)

Capture: `capture_gameplay_audio.py` (45s gameplay) + `song-preview-audio-test.py`
(70s preview). References: all 3 mogg-backed songs (20thcenturyboy/15ch,
25or6to4/11ch, antibodies/11ch — the only extracted moggs).

**`--rank` (the definitive identity test):**
```
GAMEPLAY  winner 20thcenturyboy  chroma 0.543  fp_ber 0.334  margin +0.127 -> CONFIDENT
PREVIEW   winner 20thcenturyboy  chroma 0.548  fp_ber 0.312  margin +0.221 -> CONFIDENT
```
Both independent signals (chroma AND fingerprint) rank 20thcenturyboy #1 over the
other two songs. Cross-checks (20thc capture vs 25or6to4 / antibodies refs) →
WRONG-SIGNAL. White noise → WRONG-SIGNAL. Silence → SILENT.

**Single-ref (vs 20thcenturyboy):** VERDICT **MATCH** — clip-ratio **0.00%**,
flat-top 0, fs-pin 0%, wraps 0, crest **16.8 dB** (gameplay) / clean (preview);
rate "inconclusive but not a chipmunk" (flat speed curve). The earlier 1.04–1.11×
onset speed readings are noise on that flat landscape, not a rate error.

Artifacts: `baselines/w-verify-{gameplay,preview}-20thc.{txt,json}`,
`baselines/w-verify-rank-gameplay.json`.

## How to re-verify after any audio change
```bash
# native (fast): capture + rank against the 3 mogg songs
python3 scripts/native/capture_gameplay_audio.py /tmp/cap.wav --secs 45
python3 scripts/native/audio_verify.py /tmp/cap.wav \
        --rank 20thcenturyboy,25or6to4,antibodies --section gameplay
# preview is the cleaner signal (no count-in/crowd):
python3 scripts/native/song-preview-audio-test.py --out /tmp/pv.wav --secs 70
python3 scripts/native/audio_verify.py /tmp/pv.wav \
        --rank 20thcenturyboy,25or6to4,antibodies --section preview
# single reference, full report:
python3 scripts/native/audio_verify.py /tmp/cap.wav --song 20thcenturyboy --section gameplay
# always sanity-check the tool first:
python3 scripts/native/audio_verify.py --selftest
```
Web: capture via `scripts/web/web-song-preview-audio.mjs` then feed the WAV to the
same `audio_verify.py --ref` (decode the reference once with `decode_reference.py`).

## Caveats / future work
- Only **3 of 84** songs have extracted `.mogg`s, so `--song`/`--rank` is limited
  to those. Extract more moggs to broaden coverage.
- Absolute chroma threshold (0.45) is calibrated on these captures; **prefer
  `--rank`** for a margin-based call. The clean-mix ceiling (~0.55) means a
  per-reference absolute call is inherently thinner than the relative one.
- Native rate confirmation rests on the flat speed-curve (no chipmunk); a tighter
  absolute rate proof would need a sample-accurate same-mix reference.
