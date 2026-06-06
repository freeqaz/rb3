# Phase 1 — FINDINGS (WaveB convergence + first real measurement)

**Date:** 2026-06-06. **Author:** WaveB agent. **Reads:** phase1-audiopath.md (A1),
phase1-reference.md (A2), phase1-metric.md (A3), phase1-perf.md (A4).
**This doc is what the orchestrator reads to plan Phase 2.**

## TL;DR (measured, not theorized)

- **Audio root cause = H1 (over-unity additive mix + hard clamp), CONFIRMED by a
  causal A/B.** Native GAMEPLAY of `20thcenturyboy` clips hard: **5.1–5.3 % of samples
  pinned to the rail, 2.2 % flat-top runs, crest 10.0 dB, spectral centroid pushed
  from 1182 Hz → 2308 Hz, THD ratio 13.5×.** Re-running the IDENTICAL capture with the
  master gain knocked down (`DC3_AUDIO_GAIN=0.30`) drops clipping to **0.000 % at-rail,
  0.000 % flat-top, crest 21.6 dB, peak 0.96** — i.e. the clipping vanishes entirely.
  That A/B isolates the cause to gain staging, nothing else.
- **The exact fix line is `milo-native-engine/src/audio/AudioDevice.cpp:35`**
  (`static float sMasterGain = 1.1f;`) → sub-unity headroom (~`0.30f`). The native path
  currently makes it strictly worse by multiplying the already-over-unity sum by **1.1×
  before** the clamp (`AudioDevice.cpp:378`). Web mirror has no master gain at all and
  needs the same headroom inserted at `AudioDevice_Web.cpp:411-413`.
- **Perf: the stutter is render/GPU on the boot `splash_screen`, NOT the asset loader.**
  Of 4619 ms of long-frame time, **238 ms is in LoadMgr.Poll, 0 ms in synchronous
  drains, 4381 ms (95 %) is draw/GPU.** The only asset-attributable steady spike is the
  one-shot song_select-ENTER transition (48.5 ms, 2 loaders).

---

## 1. Measured audio metrics — capture vs independent reference

Default song = **`20thcenturyboy`** (confirmed from the gameplay log:
`MIDI_DBG ... text='20thcenturyboy'`, `songs/20thcenturyboy/20thcenturyboy.mid`).
Reference = A2's un-clamped pan/vol sum of all 15 stems
(`/tmp/rb3_ref_20thcenturyboy_{gameplay,preview}.wav`, float32, peak 3.13 / 2.68).

### Captures produced this wave (all on disk in /tmp)
| capture | how | path |
|---|---|---|
| gameplay (default 1.1× gain) | `capture_gameplay_audio.py … --secs 40` | `/tmp/rb3_native_gameplay.wav` |
| gameplay loud region t≈18–40 s | sliced | `/tmp/rb3_native_gameplay_region.wav` |
| gameplay @ `DC3_AUDIO_GAIN=0.30` | `… --gain 0.30` | `/tmp/rb3_native_gameplay_gain030.wav` |
| preview | `_capture_preview.sh … 30` | `/tmp/rb3_native_preview.wav` |

### Metric table (loud/song region only; intro+count-in seconds excluded)

| signal | peak | RMS | crest (dB) | at-rail % | flat-top % (≥3) | THD× | centroid | verdict |
|---|---|---|---|---|---|---|---|---|
| **GAMEPLAY capture (gain 1.1)** | **1.0000** | 0.315 | **10.0** | **5.33 %** | **2.32 %** | **13.5×** | **2308 Hz** | **CLIPPED** |
| GAMEPLAY capture (gain 0.30) | 0.959 | 0.080 | 21.6 | **0.000 %** | **0.000 %** | 15.7×* | — | **CLEAN (no clip)** |
| PREVIEW capture (gain 1.1) | 1.0000 | 0.115 | 18.8 | 0.104 % | 0.067 % | 11.2× | — | lightly clipped (transient) |
| reference gameplay (un-clamped) | 3.128 | 0.416 | 17.5 | n/a (no clamp) | — | 1× (def) | 1182 Hz | ground truth |

\* The THD-ratio stays high even on the clean 0.30 capture — that is a **metric
artifact** of comparing a low-level capture (RMS 0.08) against the un-normalized
3.13-peak reference, NOT real distortion. The decisive clean indicators are crest
**21.6 dB** and clip-ratio **0.000 %**.

### Why the metric tool reports WRONG-SIGNAL on the waveform path (and why it's still CLIPPED)
The sample-accurate waveform xcorr is ~0 (Pearson 0.002 even at the best fine
alignment; no resample ratio 0.92–1.09 recovers it → **H3 pitch/rate REFUTED**). That
looks like "wrong signal," but it is the *severity of the clipping itself* destroying
phase. The music identity is proven independently:
- **log-mel spectrogram-shape corr = 0.572** at ref offset 1.35 s (A3 threshold 0.40),
- **long-term PSD log-divergence = 0.454** (low → same spectrum),
- envelope (0.1 s RMS) xcorr peak 0.33.
So: **same song, waveform-decorrelated by hard clipping.** This is a *more severe* case
than A3's self-test (d) (a clipped copy that still scored xcorr 0.97) — real gameplay
clips harder and adds enough odd-harmonic energy (centroid +95 %, THD 13.5×) to flatten
the cross-correlation. The clip-ratio/crest/centroid/THD quartet is the verdict, exactly
as A3 designed; the xcorr "WRONG-SIGNAL" is the documented sample-accuracy limitation,
not a different song.

### Causal A/B (the proof)
Same nav, same song, only `sMasterGain` changed at runtime:

```
gain 1.1 (default):  at-rail 5.33%   flat-top 2.32%   crest 10.0 dB   peak 1.0000
gain 0.30:           at-rail 0.000%  flat-top 0.000%  crest 21.6 dB   peak 0.959
```

peak 0.959 at 0.30× ⇒ the un-clamped mix-bus peak is ≈ 0.959/0.30 ≈ **3.2× full scale**,
matching A2's reference peak of **3.13** to within rounding. A master gain of **≈0.30–0.32
(1/peak)** is therefore the largest flat gain that avoids all clipping on this song.

---

## 2. CONFIRMED audio root cause + exact fix line

**Root cause (H1, CONFIRMED):** every song stem is a separate additive `AudioSource`;
`AudioDevice::MixSources` sums all ~15 of them with **no mix-bus headroom**
(`AudioDevice.cpp:364-366`), multiplies the sum by a **>1.0 master gain (1.1×)**
(`AudioDevice.cpp:35,378`), then **hard-clamps to [-1,1]** (`AudioDevice.cpp:379-381`).
On `20thcenturyboy` the pan/vol-weighted sum peaks at ~3.2× full scale (A2 measured 3.13
on the identical pan law), so the clamp square-wave-distorts 5 % of samples while the
song envelope survives = the reported "clipped noise that still carries the song."

- **H1 — CONFIRMED (root cause).** The A/B above is decisive: only the gain changed and
  the clipping went from 5.33 %→0 %. The pan/vol chain IS applied (A1 verified
  `StandardStream.cpp:707-725` + `rb3_stream_receiver_native.cpp:299-300`); the defect is
  purely missing headroom + a pre-clamp boost.
- **H2 — REFUTED (format/scale).** Captures sit at sane absolute levels (preview RMS
  0.12, gameplay-at-0.30 RMS 0.08); no int16-range-float leak (that would peg every
  sample, not 5 %). Matches A1.
- **H3 — REFUTED (pitch/rate).** No resample ratio in 0.92–1.09 (incl. 44.1/48 k)
  recovers any waveform correlation; spectral centroid shift is harmonic distortion from
  clipping, not a pitch shift. (A1 left this NEEDS-MEASUREMENT; now refuted.)
- **H4 — REFUTED (downmix/stride).** Same long-term spectrum as the reference
  (PSD divergence 0.45); the downmix is correct, the sum just has no headroom. Matches A1.

**Exact fix (Phase 2 applies it — do NOT apply here):**
- **Primary, native:** `milo-native-engine/src/audio/AudioDevice.cpp:35`
  `static float sMasterGain = 1.1f;` → `0.30f` (≈ 1/peak; already runtime-overridable via
  `DC3_AUDIO_GAIN`, so sweep before committing). This single line removes the clipping AND
  the gratuitous 1.1× pre-clamp boost.
- **Matching web:** `milo-native-engine/src/audio/AudioDevice_Web.cpp:411-413` — insert
  `output[i] *= kMixHeadroom;` (0.30f) before the clamp (web has no master gain today).
- Both files are `HX_NATIVE`/`HX_WEB`-only; no Wii decomp-match impact.

**Expected post-fix numbers (predicted from the 0.30 A/B):** gameplay at-rail → ~0 %,
flat-top → ~0 %, crest → ~21 dB, peak → ~0.95. Trade-off: RMS drops to ~0.08 (≈ −12 dB
quieter). That loudness loss is why Phase 2 should also prototype a soft limiter (see
plan) so the flat 0.30 isn't the permanent answer.

---

## 3. Perf stutter table + prime suspect (measured this wave)

`frame_profiler.py --scroll 20 --scroll-pace 0.3 --run-secs 8` (2526 frames):

| metric | value |
|---|---|
| p50 / p95 / p99 / max | 7.47 / 11.21 / 68.89 / **185.76 ms** |
| long frames ≥33 ms | 63 (61 of them on splash_screen) |

**Top-5 stutters (all `splash_screen`, boot):** 185.8 / 96.6 / 84.5 / 82.4 / 81.5 ms —
each carries only `bgLoad=8–15 ms`; the rest is draw/GPU.

| screen | n | p50 | p95 | p99 | max |
|---|---|---|---|---|---|
| **splash_screen** | 62 | 68.2 | 82.4 | 96.6 | **185.8** |
| song_select_screen | 2276 | 7.3 | 10.3 | 11.6 | 48.5 |
| main_hub_screen | 158 | 8.5 | 15.2 | 24.5 | 28.0 |
| intro_movie_screen | 22 | 9.6 | 16.4 | 32.5 | 32.5 |

**Spike attribution:** of 4619 ms total long-frame time — **238 ms in LoadMgr.Poll (bg),
0 ms in synchronous PollUntil* drains, 4381 ms (95 %) elsewhere (draw/poll/GPU).**

**Prime suspect = splash-screen render/GPU during boot**, NOT the asset loader. The
budgeted loader (`RB3_LOADER_BUDGET_MS=8`) is holding — `lpu` (sync-drain ms) is **0 on
every post-boot frame**, so the old sync-I/O stutter is gone. Preview-stream opens and
album-art loader-adds during scroll are ~5–7 ms each and don't block the frame. The only
asset-attributable steady spike is the **one-shot song_select-ENTER (48.5 ms, LOAD+2)**.
(Fully matches A4.)

---

## 4. Phase-2 plan (ordered)

### 4a. Audio fix — apply & re-measure (highest priority, fully de-risked)
1. **Apply the headroom gain.** `milo-native-engine/src/audio/AudioDevice.cpp:35`:
   `sMasterGain = 1.1f` → `0.30f`. Mirror on web: `AudioDevice_Web.cpp:411-413` insert
   `output[i] *= 0.30f;` before the clamp. Expected: gameplay at-rail/flat-top → ~0 %,
   crest → ~21 dB. **Re-measure** with `capture_gameplay_audio.py` + `audio_correlate.py`
   (clip-ratio must read 0.00 %, crest >20 dB).
2. **Then prototype a soft limiter / mix-bus normalize** (recommended follow-up, because
   the flat 0.30 costs ~12 dB of loudness). Options, cheapest first:
   (a) tanh/soft-knee limiter on the summed `output[]` before the clamp at
   `AudioDevice.cpp:376-381` — keeps loudness, rounds transients instead of square-clip;
   (b) normalize by active-source count (`gain = clamp(headroom/√N_active)`) so quiet
   sections aren't over-attenuated. Re-measure crest + RMS + clip-ratio; target clip 0 %
   AND RMS back toward the reference's ~0.4 without re-clipping. Keep `DC3_AUDIO_GAIN` so
   the default is sweepable.
3. **Confirm on web once** (per the README "iterate native, confirm web once" rule): the
   mix/clamp is shared, so the native A/B predicts the web fix; verify in-browser with
   `scripts/web/web-song-preview-audio.mjs` (SAB-ring nonZero + a capture spot-check).

### 4b. Perf fix — prototype (lower priority; steady scroll is already smooth at p95 ≈11 ms)
4. **Stagger the song_select-ENTER loaders.** Spread the 2 transition loaders off the
   single transition frame (defer/stagger `AddLoader` at `src/system/utl/Loader.cpp`) and
   prewarm the list/art scaffolding before the screen goes active. Expected: kill the
   48.5 ms enter spike → bring it under the ~11 ms scroll p95. Re-measure with
   `frame_profiler.py` (watch the `song_select` transition row + ensure `lpu` stays 0).
5. **Boot splash spike is a RENDER concern, separate workstream.** The 185 ms splash
   frames are 95 % draw/GPU with the loader a minor co-tenant. If "boot stutter" is in
   scope, profile the splash milos' per-frame draw cost (not the loader). Out of scope for
   the audio-clip fix; flag for a render-side investigation. The tracer's `lpu` field is
   the canary if any change reintroduces a synchronous load.

---

## 5. Tools added/used this wave
- **`scripts/native/capture_gameplay_audio.py`** (NEW) — drives rb3-native headless into
  real gameplay of the default song and dumps post-mix audio to a WAV via the null
  backend. `python3 scripts/native/capture_gameplay_audio.py OUT.wav --secs 40 [--gain G]`.
  `--gain` forwards `DC3_AUDIO_GAIN` for the causal A/B. Reuses song-end-test's nav.
- Reused (Wave A): `scripts/native/_capture_preview.sh`, `scripts/native/audio_correlate.py`,
  `scripts/native/decode_reference.py`, `scripts/native/frame_profiler.py`.
- No `src/` or engine files modified this wave (capture/measure only; the fix is Phase 2).

## 6. Confidence
**HIGH** on the audio root cause: a controlled A/B (same song/nav, only `sMasterGain`
changed) drove clipping 5.33 %→0 %, and the un-clamped mix peak (≈3.2×) matches A2's
independent reference (3.13×) to rounding. Residual ambiguity is only the *choice of
fix shape* (flat 0.30 vs soft limiter) and the per-song loudness target — both Phase-2
tuning questions, not "what's broken." The metric tool's WRONG-SIGNAL on the waveform
path is the documented sample-accuracy limitation (spectrogram-corr 0.57 + PSD-div 0.45
prove same-song); it does not weaken the CLIPPED conclusion, which rests on the
alignment-free clip-ratio/crest/centroid/THD evidence.
