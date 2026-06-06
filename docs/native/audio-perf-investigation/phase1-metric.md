# Phase 1 — Audio divergence metric (A3)

**Tool:** `scripts/native/audio_correlate.py`
**Purpose:** Given a CAPTURE wav (the game's recorded output) and a REFERENCE wav
(an independent ground-truth decode of the same song), quantify *how much* they
diverge and *whether the capture is clipped/distorted* — the test that the earlier
"real music, not noise" proof missed.

## Why the earlier proof was insufficient (the bug class this catches)

`analyze_preview_audio.py` proves a capture is **reproducible** (spectrogram corr
of two runs ~0.9) and **non-flat** (SFM << white noise). Both of those PASS on a
**clipped / wrongly-scaled copy of the real song**, because a clipped copy is
still reproducible and still spectrally non-flat. The README's leading hypotheses
(H1 over-unity sum + hard clamp, H2 int16-range floats → ±1 clamp) all produce
exactly that: a signal that *carries the song envelope* but is hard-clipped. The
decisive discriminators are correlation **against an independent reference** plus
clip-ratio, crest factor, and spectral divergence.

## Metrics

Pipeline: load (any ffmpeg format) → mono → resample to ref rate → time-align via
cross-correlation → per-channel gain-normalize → measure.

| Metric | What it is | Clean (MATCH) | Distorted (CLIPPED) | Different (WRONG) |
|---|---|---|---|---|
| **alignment lag** | best integer sample shift (xcorr) | small | small | meaningless |
| **xcorr peak** | normalized aligned-waveform similarity, −1..1 | high (≥0.6) | high (clipping barely lowers it) | ~0 |
| **spectrogram-shape corr** | timing-robust corr of per-frame log-spectrogram *shape*; survives sample-phase drift | high | high | low |
| **Pearson (overall / per-sec)** | raw-waveform corr after alignment | ~1 | high | ~0 |
| **fit gain `g`** | least-squares level `cap ≈ g·ref` | any (level-agnostic) | any | n/a |
| **norm RMS error / SNR(dB)** | divergence of `cap − g·ref` | low NRMSE / high SNR | higher | n/a (only valid with a sample-accurate ref) |
| **CLIP RATIO (flat-top)** | fraction of samples in a **run of ≥3 consecutive identical samples pinned at the rail** | ~0% | **high (≫0.5%)** | ~0% |
| **crest factor (dB)** | peak/RMS; clipped audio is flat-topped → low. **Corroboration only** | varies | low | varies |
| **spectral divergence** | mean \|log₁₀ PSD ratio\| over 50 Hz–8 kHz, gain-normalized | low (<1.5) | **high** | high (but song is wrong) |
| **HF-energy ratio (THD proxy)** | capture HF-fraction ÷ reference HF-fraction; clipping adds high-order harmonics | ~1× | **≫2×** | n/a |

### Key design decision: clip-ratio = FLAT-TOP runs, not near-peak dwell

A naive "fraction of samples ≥0.99·peak" **false-positives on a clean sine** (a
sine spends ~9% of its time within 1% of its peak because the crest is smooth).
Real hard clipping pins **runs of consecutive *identical* samples** at the rail (a
flat top). `clip_stats()` therefore counts only samples that are (a) within ε of
the extreme **and** (b) part of a run of ≥`FLATTOP_MIN_RUN`(=3) equal-valued
samples. This is what drops the clean 440 Hz sine from 8.98% → **0.00%** while the
hard-clipped sine reads **75.56%**.

### Key design decision: crest factor corroborates, never decides

A pure sine has a crest factor of only ~3 dB — legitimately low. So crest factor
**cannot** be a standalone CLIPPED trigger; it's reported as corroboration. The
CLIPPED decision rests on flat-top clip-ratio, or on spectral-divergence-AND-THD
together (both, to avoid pure-tone HF-ratio blow-ups from a near-zero denominator).

## Decision logic & thresholds

```
is it the same song?
  sample_accurate = |xcorr peak| ≥ 0.6  OR  Pearson ≥ 0.6      # CORR_MATCH
  spec_same       = spectrogram-corr ≥ 0.4 AND spec-div < 1.5  # drift-robust fallback
  same_music      = sample_accurate OR spec_same
  if NOT same_music                                  -> WRONG-SIGNAL

is it clipped? (only once it's the same song)
  clip-ratio ≥ 0.5%                                  -> CLIPPED   # CLIP_RATIO_BAD
  OR (spec-div ≥ 1.5 AND THD ≥ 2× AND hf_cap ≥ 0.01) -> CLIPPED   # SPECTRAL_DIV_BAD, THD_EXCESS_BAD
  else                                               -> MATCH
```

| Threshold | Value | Rationale |
|---|---|---|
| `CORR_MATCH` | 0.60 | a clipped copy keeps xcorr high; noise is ~0 |
| `SPEC_CORR_MATCH` | 0.40 | same-music phase-drift pair scores ~0.47; noise ~0 |
| `CLIP_RATIO_BAD` | 0.5% | flat-top run fraction; clean audio is ~0% |
| `SPECTRAL_DIV_BAD` | 1.5 | gain-normalized PSD log-distance; clean copy ~0.3 |
| `THD_EXCESS_BAD` | 2.0× | clipping at least doubles HF-energy fraction |
| `FLATTOP_MIN_RUN` | 3 | ≥3 consecutive equal rail samples = a flat top |

## Self-test (synthetic ground truth — `--selftest`, no game run)

440 Hz sine reference + a richer multi-harmonic "song" reference, tested against
six signals. (a)–(c) are the required cases; (d)–(f) guard against overfitting —
**(d) is the actual game bug**: a clipped copy of the real song, the exact case
reproducibility-corr + SFM both miss.

```
case                  expected      got             xcorr   clip%   crest  specdiv      THD  ok
(a) same sine         MATCH         MATCH            1.00    0.00     3.0     0.00     0.18  OK
(b) hard-clip @0.3    CLIPPED       CLIPPED          0.96   75.56     0.8     1.60   5.2e5  OK
(c) white noise       WRONG-SIGNAL  WRONG-SIGNAL    -0.00    0.00    10.5     8.75   6.7e8  OK
(d) clipped real song CLIPPED       CLIPPED          0.97   18.85     4.4     3.98   3.0e4  OK
(e) quiet clean copy  MATCH         MATCH            1.00    0.00     8.5     0.29     1.99  OK
(f) different song    WRONG-SIGNAL  WRONG-SIGNAL    -0.07    0.00     9.6     4.04   1.8e3  OK

RESULT: ALL PASS — separates MATCH / CLIPPED / WRONG-SIGNAL
```

Reading the table:
- **(a) clean sine → MATCH.** xcorr 1.0, flat-top clip 0.00% (the flat-top detector
  ignores the sine's smooth crest dwell). Crest 3.0 dB is low but is corroboration
  only, so it does **not** false-trigger CLIPPED.
- **(b) hard-clipped sine → CLIPPED.** xcorr still 0.96 (it IS the sine) but clip
  75.56%, crest 0.8 dB, raised harmonics.
- **(c) white noise → WRONG-SIGNAL.** xcorr ~0 and spectrogram-corr ~0.
- **(d) clipped copy of the real song → CLIPPED.** xcorr 0.97 — reproducibility
  PASSES — yet clip 18.85% + spec-div 3.98 + THD flag it. This is the case the old
  proof missed.
- **(e) half-gain clean copy → MATCH.** Gain normalization (`fit gain` 0.5) removes
  the level mismatch; no clipping → MATCH. Proves "wrong level" ≠ "clipped".
- **(f) different clean song → WRONG-SIGNAL.** Different pitches *and* a different
  temporal envelope decorrelate both waveform and spectrogram; spec-div 4.04 blocks
  the spectrogram fallback from mislabeling it same-music.

## Real-data cross-checks (existing `/tmp` captures from prior waves)

| capture vs reference | verdict | note |
|---|---|---|
| `prevA.wav` vs `prevB.wav` (two runs of same preview) | **MATCH** | raw xcorr only 0.23 (non-deterministic mixer → phase drift), but spectrogram-corr 0.47 + spec-div 0.23 → certifies same clean music |
| `prevA.wav` vs `rb3-native-song-long.wav` (different audio) | **WRONG-SIGNAL** | spectrogram-corr 0.32, decorrelated |
| `prevA.wav` vs itself | **MATCH** | identity sanity |

## Known limitation — use a sample-accurate reference

Two *game captures* are not sample-accurate (the null-backend mixer is
non-deterministic and phase-drifts), so the tool falls back to the timing-robust
spectrogram path. That path can certify a **clean** same-music match but **cannot
reliably catch a capture that is BOTH clipped AND phase-drifted** (high spec-div
then looks like "different song"). To judge clipping rigorously, the REFERENCE must
be a **sample-accurate offline decode of the same mogg** (e.g.
`ffmpeg -i <id>.mogg -ac 2 ref.wav` with the matching channel downmix) so the
waveform-xcorr path is the primary judge. That is the recommended next-wave step:
produce an offline reference decode and run a real gameplay capture against it.

## Usage

```bash
python3 scripts/native/audio_correlate.py CAPTURE.wav REFERENCE.wav     # report table
python3 scripts/native/audio_correlate.py CAPTURE.wav REFERENCE.wav --json
python3 scripts/native/audio_correlate.py --selftest                    # synthetic proof
# exit code: 0 MATCH, 1 CLIPPED, 2 WRONG-SIGNAL
```

Deps: numpy + scipy (present); ffmpeg for non-WAV inputs (present). No soundfile needed.
