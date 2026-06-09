# Wave 05 — Audio sample-RATE fix (chipmunk + "clipping") — LANDED, web-validated

Goal: kill the "music but TOO FAST (chipmunks) + clipping a lot" song-audio/preview
bug. Per STATE.md this is a SEPARATE sample-rate bug from the already-fixed gain
clipping (the instant-attack limiter handled that).

## TL;DR
- **Root cause = WEB-ONLY rate mismatch with NO resampler.** The engine mixes at the
  mogg/decode rate (44100), but the browser `AudioContext` commonly runs at the
  hardware rate (48000) — the requested 44100 is only a *hint* the browser may
  ignore. The code never read back the actual `ctx.sampleRate`; it pushed 44100-rate
  PCM straight into the SAB ring, and the AudioWorklet (which runs at the true
  `ctx.sampleRate`) replayed it with NO resampler → **48000/44100 = 1.0884x fast =
  chipmunks**.
- **Native is CORRECT (no fix needed).** miniaudio is given `config.sampleRate=44100`;
  its data callback then fires at 44100 and miniaudio internally resamples to the
  hardware rate. `mSampleRate = mDevice->sampleRate` reads back the callback rate
  (44100), which matches the PCM. Measured: native null-backend = 44100, RenderAudio
  consumes 44100 PCM 1:1 → ratio 1.000. (This sandbox has no ALSA device, so native
  real-output couldn't be captured here, but the rate path is provably 1:1.)
- **Fix (web): read back the real `ctx.sampleRate` + linear-resample the 44100 mix to
  it in `PumpAudio` before the SAB push.** Continuous-phase (carries sub-sample phase
  + the boundary sample across PumpAudio chunks → no clicks).
- **"Clipping a lot" is NOT a separate over-level and NOT ring drift.** The current
  (instant-attack-limiter) build's preview MixSources output measures **0 flat-top
  runs, 0 overflow-wraps, peak ~5% of full scale, crest 13.6–14.3 dB** = clean music.
  The reported clipping was (a) the chipmunk's pitch/tempo shift sounding harsh and/or
  (b) the previously-DEPLOYED build predating the user's uncommitted instant-attack
  limiter. A fresh web build (done this wave) picks up both fixes.

## Measured ratios (NOT theorized)
| path | how measured | rate seen | playback ratio |
|---|---|---|---|
| **web (headless test box)** | read `window._rb3Audio.ctx.sampleRate` | 44100 (browser honored hint) | 1.000 — no chipmunk *here* |
| **web (real hardware = 48000)** | forced `AudioContext` to 48000 in-browser | 48000 | **1.0884x (chipmunk)** before fix |
| **web (48000, AFTER fix)** | 440 Hz mix-rate tone → resampler → capture, FFT | 48000 | **440.00 Hz = 1.0000** |
| **native** | null backend init log + 1:1 RenderAudio | 44100 | 1.000 (correct, no fix) |

The headless Chromium here returns `ctx.sampleRate=44100` (so the bug does NOT repro
on this box) — that's why earlier waves didn't catch it. The user's real machine has
a 48000-locked device, which is the common case. Proven by forcing the context to
48000 (`scripts/web/web-audio-rate-forcetest.mjs`): the engine logged it would push
44100 data to a 48000 worklet = 1.0884x.

## Rate pipeline (where it diverged)
```
mogg 44100/15ch
  -> VorbisReader mSampleRate = mVorbisInfo->rate = 44100
  -> StandardStream::InitInfo(.., 44100, ..) -> StreamReceiver::New(.., 44100, ..)
  -> StreamReceiverNative ring: 1 frame == 1 sample, NO resampler (RenderAudio 1:1)
  -> AudioDevice::MixSources sums sources at the MIX rate (44100)
  NATIVE: -> miniaudio config.sampleRate=44100 => callback@44100 + internal resample
          to hw rate. mSampleRate=mDevice->sampleRate=44100. CORRECT.
  WEB:    -> PumpAudio pushed 44100 frames into SAB 1:1
          -> AudioWorklet process() runs at ctx.sampleRate (e.g. 48000), NO resampler
          -> plays 44100 data at 48000 = 1.0884x.   <-- THE BUG (AudioDevice_Web.cpp)
```
The exact divergent stage: `AudioDevice_Web.cpp` Init requested 44100 but stored
`mSampleRate=44100` regardless of the browser's actual `ctx.sampleRate`, and PumpAudio
wrote the mix to the SAB with no rate conversion.

## The fix (files — all native/web-only, zero Wii decomp impact)
`../milo-native-engine/src/audio/AudioDevice.h`
- web-only (`#ifdef __EMSCRIPTEN__`) members: `mDeviceSampleRate` (actual ctx rate),
  `mResamplePos` / `mResampleLastL/R` / `mResampleHavePrev` (continuous linear-resampler
  phase state) + `GetDeviceSampleRate()` accessor.

`../milo-native-engine/src/audio/AudioDevice_Web.cpp`
- `js_audio_init` now **returns the actual `ctx.sampleRate`** (browser may clamp).
- `Init` stores it as `mDeviceSampleRate`; logs `mix 44100 -> ctx 48000 (RESAMPLING
  0.9187x)` when they differ.
- **`PumpAudio` resamples the 44100 mix → `mDeviceSampleRate`** before `js_audio_ring_write`:
  - rates equal → unchanged fast path.
  - rates differ → for each output (device-rate) chunk, mix exactly the mix-rate frames
    needed, linear-interpolate to the device rate; carry the sub-sample phase + the last
    mix sample across PumpAudio calls (prepended at index 0) so it's click-free across
    chunk boundaries. The limiter still runs on the 44100 mix inside MixSources (its
    release coeff uses `mSampleRate`), correct.
- `sOutBuffer` added (device-rate scratch); capture/download tag the WAV at the device
  rate when resampling.
- Debug hook `rb3_debug_tone(hz)` (inert unless called): injects a pure mix-rate sine so
  the resampler can be verified deterministically in-browser.

Native `AudioDevice.cpp` UNCHANGED (the 21-line diff there is the user's pre-existing
instant-attack limiter — built on, not reverted).

## Post-fix re-measure (rebuilt web 18:47, redeployed)
- **Deterministic tone proof (real browser, ctx forced 48000):** 440 Hz mix-rate tone →
  resampler → SAB → captured @48000 → FFT = **440.00 Hz (ratio 1.0000)**. Chipmunk would
  be 478.9 Hz. Control @44100 fast path = 440.00 Hz too. → PITCH FIXED, both paths.
  Tool: `scripts/web/web-audio-tone-verify.mjs` + `/tmp/measure_tone.py`.
- **Resampler DSP unit test** (`/tmp/resampler_test.cpp`, exact PumpAudio logic, irregular
  chunk sizes): 1 kHz @44100 → **999.84 Hz @48000**; chunk-boundary curvature == interior
  curvature (ratio 0.99) → **no boundary clicks**.
- **No ring drift / underrun:** under the 48000 resampling path the SAB stays 31743–32767
  of 32768 frames filled (demand-driven PumpAudio keeps up). Refutes the "rate mismatch →
  ring drift → clipping" unified hypothesis: the chipmunk was PURE pitch, not glitches.
- **Clipping resolved:** preview MixSources output (48k resampled AND 44k control) =
  `clip_ratio=0.0000  flat_top_run=0  overflow_wraps=0  crest 13.6–14.3 dB peak ~5% FS` =
  COHERENT music (`scripts/native/audio_coherence.py`). The instant-attack limiter (now in
  the deployed build) holds it clean.

## Native status
Native ratio = 1.000 by construction (miniaudio resamples). No native fix required. The
fix is web-only because only the AudioContext path lacked a resampler. (If a native host
ever opens the device at a rate miniaudio can't honor, `mSampleRate=mDevice->sampleRate`
already reflects it and RenderAudio stays 1:1 with that callback rate — still correct.)

## Tools added this wave
- `scripts/web/web-audio-rate-probe.mjs` — reads `ctx.sampleRate` vs requested 44100.
- `scripts/web/web-audio-rate-forcetest.mjs` — forces ctx to 48000 to repro/confirm.
- `scripts/web/web-audio-tone-verify.mjs` — deterministic 440 Hz tone through the real
  pipeline; the decisive single-run pitch proof.
- `/tmp/measure_tone.py`, `/tmp/resampler_test.cpp` — FFT tone measure + DSP unit test.
- (cross-run song-content xcorr ratio search is UNRELIABLE — same caveat STATE.md already
  notes for audio_correlate; the tone test replaces it.)
