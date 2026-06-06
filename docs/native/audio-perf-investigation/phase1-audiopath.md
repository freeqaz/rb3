# Phase 1 — A1: Audio gain-staging / format path, end-to-end

Goal: follow ONE song channel's PCM from the decrypted mogg to the speaker and
pinpoint where "clipped noise" is introduced. All file:line below are verified
against the current tree (2026-06-06).

## TL;DR

The format/normalization is **correct** (refutes H2). The "clipped noise" is
**gain-staging clipping (H1)**: every song stem is a *separate* additive
`AudioSource`, each rendered near unity, and ~11–14 of them are summed before a
hard clamp to [-1,1]. Centered (`pan 0.0`) stems contribute at full `volume` to
**both** L and R. On a loud song section the per-channel sum routinely exceeds
1.0 → the `[-1,1]` clamp square-wave-distorts the peaks while the song envelope
survives (exactly the "real-music-but-clipped" symptom). The native path adds a
further **1.1× master gain** on top of the sum, *before* the clamp, which makes
it strictly worse.

**Most-likely root cause:** additive sum of all stems with no mix headroom / no
master attenuation, then hard clamp.
**Single most-impactful fix line:**
`milo-native-engine/src/audio/AudioDevice.cpp:35`
`static float sMasterGain = 1.1f;` → set to a **sub-unity headroom gain**
(e.g. `0.30f`, ≈ 1/√(numStems)) so the summed stems no longer clip. (Web has no
master gain at all — see "Fix" for the matching web change.)

---

## Per-channel data-flow table

One song channel (one mogg sub-stream), file → speaker:

| # | Stage | File:line | Numeric range in / out | Gain applied here |
|---|-------|-----------|------------------------|-------------------|
| 1 | Vorbis decode → planar float PCM | `src/system/synth/VorbisReader.cpp:285` `vorbis_synthesis_pcmout(mVorbisDsp, &pcm)` | out: **float [-1.0, 1.0]** (Xiph API contract) | none |
| 2 | Hand PCM up to stream | `VorbisReader.cpp:287` `ConsumeData((void**)pcm, pcmAvail, …)` → `:683` `mStream->ConsumeData(v,i1,i2)` | float [-1,1] | none |
| 3 | **float→int16 conversion + clamp** | `src/system/synth/StandardStream.cpp:313-318` `float f = 32767.0f * src[j]; clamp ±32767; *dst=(short)f;` | in: float [-1,1]; out: **int16 [-32767, 32767]** | ×32767 (format scale, not a volume) |
| 4 | Write int16 into per-channel ring | `StandardStream.cpp:322` `mChannels[chIdx]->WriteData(convBuf, …)` → `StreamReceiver::WriteData` (`StreamReceiver.cpp:32`) | int16 | none |
| 5 | dB→linear ratio, set per-channel vol | `StandardStream.cpp:719-721` `ratio = DbToRatio(mFaders.GetVal()); ClampEq(ratio,0,1); mChannels[i]->SetVolume(ratio)` | dB → linear **[0,1]** (attenuation only) | sets `mVolume` |
| 6 | set per-channel pan | `StandardStream.cpp:660-665` `mChannels[chan]->SetPan(pan)` | pan ∈ **[-1,1]** (from songs.dta) | sets `mPan` |
| 7 | **RenderAudio: int16→float, apply vol+pan, write stereo** | `native/src/rb3_stream_receiver_native.cpp:294-303` `fs = s*(1/32768); out[L]=fs*volL; out[R]=fs*volR;` with `ComputePanGains` (`:87-91`) | in int16; out: **float ≈[-mVolume, +mVolume]** per L/R | ×volL/volR (≤ mVolume) |
| 8 | **Additive mix of ALL stems + master gain + clamp** | `milo-native-engine/src/audio/AudioDevice.cpp:364-381` `output[i] += mMixBuffer[i]; … output[i]*=sMasterGain(1.1); clamp ±1` | sum of N stems (can be ≫1) → clamp **[-1,1]** | **×1.1, then HARD CLAMP** |
| 9 | int16 dump / speaker | `AudioDevice.cpp:108` (WAV) / miniaudio device | clamp ±1 → int16 | none |

Web variant of stage 8: `milo-native-engine/src/audio/AudioDevice_Web.cpp:399-413`
— identical additive sum + clamp, but **no master gain** (no `*= sMasterGain`).

### Pan law (stage 7) — the gain trap

`rb3_stream_receiver_native.cpp:87-91` `ComputePanGains`:
```
pan = clamp(pan, -1, 1);
left  = volume * (pan <= 0 ? 1.0       : 1.0 - pan);
right = volume * (pan >= 0 ? 1.0       : 1.0 + pan);
```
- `pan = 0.0` (centered stem): `volL = volR = volume` → full level into **both** channels.
- `pan = -1.0` (hard left): `volL = volume`, `volR = 0`.
- This is a linear *balance* law, capped at `volume` (max gain per source = `volume`,
  no 2× boost). That part is fine; the problem is the **sum across sources**.

> Note: the engine's own `StreamReceiver_Native.cpp:201-202` uses a *worse* law
> (`volL = volume*max(0,1-pan)` → 2.0× at pan=-1), but that TU is **excluded** from
> the rb3-native build (`MILO_ENGINE_DECOMP_PLATFORM_EXCLUDE`, see header of
> `rb3_stream_receiver_native.cpp`). RB3 uses the capped law above. So the 2× boost
> is NOT in play for RB3 — only the additive over-unity sum is.

### Empirical stem layout (real song `antibodies`, songs.dta)

```
pans (-1.0 1.0 0.0 -1.0 1.0 -1.0 1.0 -1.0 1.0 -1.0 1.0)   ← 11 channels
vols (-4.0 -4.0 -1.0 -3.7 -3.7 -4.0 -4.0 -4.0 -4.0 -4.0 -4.0) dB
```
- 11 separate `AudioSource`s, each its own ring + RenderAudio.
- DbToRatio: -4dB→0.63, -1dB→0.89, -3.7dB→0.65.
- **Left bus** = sum of all pan≤0 sources: ch0,2,3,5,7,9 →
  0.63+0.89+0.65+0.63+0.63+0.63 ≈ **4.07× full-scale peak headroom**.
- Even at typical music crest (stems not perfectly in phase), instantaneous L sum
  frequently exceeds 1.0 on loud sections → clamp → square-wave clip. Native then
  multiplies by 1.1 *first*, guaranteeing more clipping.
- Other songs in songs.dta show 12–14 channels with vols as low as -5.5dB; the
  channel **count** (not the per-stem level) is what blows the sum past unity.

---

## Hypothesis verdicts

### H1 — over-unity additive sum + hard clamp (gain staging) — **SUPPORTED (root cause)**
- Each stem is a distinct `AudioSource`; `AudioDevice::MixSources` sums them
  additively then clamps: `AudioDevice.cpp:364-366` (`output[i] += mMixBuffer[i]`)
  and `:377-381` (`*= sMasterGain; clamp ±1`). Web mirror `AudioDevice_Web.cpp:399-413`.
- vols/pans **are** applied (refutes the "bypassed" sub-hypothesis): the whole
  `SetVolume/SetPan → UpdateVolumes → DbToRatio → SetVolume(ratio)` chain is shared
  engine code with no HX_NATIVE bypass (`StandardStream.cpp:707-725, 660-666`), and
  RB3 RenderAudio multiplies `fs*volL/volR` (`rb3_stream_receiver_native.cpp:299-300`).
  The problem is not missing per-channel gain — it's that 11–14 near-unity sources
  with **no mix bus headroom** sum past 1.0, and native applies a **>1.0 master gain
  (1.1×) before the clamp** (`AudioDevice.cpp:35,378`).
- Real songs.dta confirms ~11–14 channels, multiple at pan 0.0 (full-level to both
  buses). Peak L/R headroom requirement ≈ 4× → clamp distortion correlated to the
  music. This is exactly the README's "native gameplay peaks at exactly 32767
  (full-scale clip)" observation.

### H2 — scale/format mismatch (int16-range floats instead of [-1,1]) — **REFUTED**
- `vorbis_synthesis_pcmout` returns normalized **[-1,1]** float (Xiph contract).
- `StandardStream::ConsumeData:313` multiplies by `32767.0f` and clamps to ±32767
  to produce int16 — this is the correct [-1,1]→int16 conversion and only makes
  sense for [-1,1] input.
- `RenderAudio:298` divides the int16 back by `32768.0f` → back to [-1,1] per source.
  No double-scale, no int16-range float leak. The single-source level is correct;
  there is no format error.

### H3 — sample-rate mismatch / pitch shift (web + native) — **NEEDS-MEASUREMENT**
- Mogg native rate comes from `mVorbisInfo->rate` (`VorbisReader.cpp:270`,
  RB3 moggs are authored at 44100). Native requests `AudioDevice::Init(44100)`
  (`rb3_synth_native.cpp:40`; `config.sampleRate=44100` `AudioDevice.cpp:158`).
- The StreamReceiver ring carries **1 frame = 1 sample** with **no resampler**
  (`rb3_stream_receiver_native.cpp` advances `mAudioReadPos` by exactly the
  callback's frameCount). If miniaudio opens the device at 48000 (hardware default
  when 44100 is unavailable), miniaudio inserts its *own* internal resampler
  (device-side), so playback rate should be correct — but if the song rate ≠ 44100,
  or the web `AudioContext` rate is clamped by the browser to 48000 while the SAB
  ring is filled at 44100, you get pitch shift + ring drift (README H3). This is a
  *glitch/pitch* failure mode, not "clipped noise," so it is secondary. Measure:
  log `mDevice->sampleRate` vs `mVorbisInfo->rate` and FFT a captured tone.

### H4 — channel/stride/pan-law downmix error — **REFUTED (as the clip cause)**
- Interleave is correct: planar per-channel float from vorbis (`pcm[ch][sample]`),
  each channel a separate source, written stereo-interleaved
  (`output[i*2+0]=L, output[i*2+1]=R`, `rb3_stream_receiver_native.cpp:299-300`).
- Pan law is a sane balance law capped at `volume` (no 2× over-unity per source).
- All channels do NOT go to both L+R at unity — only `pan==0` ones do, which is
  correct mono-center behavior. The downmix math is right; it's the *count* of
  summed sources without headroom (H1), not a stride/pan bug.

---

## Ranking (most→least likely cause of "clipped noise")

1. **H1 — additive over-unity sum + hard clamp (+1.1× native master gain).** SUPPORTED.
2. H3 — sample-rate/pitch (could add glitch/warble on top, but not the clip). NEEDS-MEASUREMENT.
3. H4 — downmix/stride. REFUTED as clip cause.
4. H2 — format/scale. REFUTED.

---

## The fix (single most-likely line + the matching web line)

**Primary (native):** give the mix bus headroom by making the master gain
**sub-unity** instead of a boost.

- File: `milo-native-engine/src/audio/AudioDevice.cpp:35`
- Change: `static float sMasterGain = 1.1f;` → `static float sMasterGain = 0.30f;`
  (≈ 1/√(11); tune per measured peak. Already overridable via `DC3_AUDIO_GAIN`,
  so it can be swept empirically without a rebuild.)
- This alone converts the clipping (sum 4× → 1.2× after 0.30 gain, only the rare
  true peaks clip) and removes the gratuitous 1.1× pre-clamp boost.

**Matching web change (web has NO master gain today):** add the same headroom
scale before the clamp.
- File: `milo-native-engine/src/audio/AudioDevice_Web.cpp:411-413` (the clamp loop)
- Insert `output[i] *= kMixHeadroom;` (e.g. 0.30f) before the clamp, mirroring native.

**Stronger, click-free alternative (recommended follow-up, not required for the
first fix):** replace the hard clamp with a single shared mix-bus soft limiter
/ normalize-to-active-source-count so transients compress instead of square-clip.
But the one-line sub-unity master gain is the minimal, measurable first move and
directly tests H1.

> Both files are engine files and the audio paths here are already `HX_NATIVE`/
> `HX_WEB`-only (no Wii decomp match impact). `sMasterGain` is runtime-overridable
> via `DC3_AUDIO_GAIN`, so the H1 fix can be A/B-measured against a reference decode
> (Phase 1 metric work) before committing a default.
