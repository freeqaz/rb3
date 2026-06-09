# Wave 09 — web audio: broadband HF static (NOT clicks, NOT chipmunk) + SFX latency

**Date:** 2026-06-09
**Trigger:** User: web song audio "static mixed with chipmunks, wayyy too high frequency."
Plus a second ask: SFX keydown→audio latency is a few hundred ms.

## Orchestrator brief (decisive facts established inline — read before investigating)

User gave us the first real browser capture we've ever had: `fucked-audio1.m4a`
(11.3s, "Beast and the Harlot", song shortname `beastandtheharlot`).

- **underruns = 0** over 30s (user console log), frame p50 ~20ms → NOT buffering.
- `audio_verify.py fucked-audio1.m4a --song beastandtheharlot`: **MATCH** — right song
  (chroma 0.69, fp BER 0.34), **right pitch/speed (1.003x, not a chipmunk)**, **not clipped**
  (0.00% clip, crest 19dB), but **HF/THD 6.08x** (thr<2.0) and **flatness 0.62** = broadband
  HF static on top of correct music.
- Noise char: **0 clicks/discontinuities** (seam-click theory dead), steady HF hiss,
  **spectral peak at 11025 Hz = 44100/4**.
- carry-all resampler fix IS deployed (pin bd695fb=HEAD, build 21:09); resampler is clean
  linear interp 44100→ctx (upsampling). Linear upsample shouldn't add 6x HF.

**The job:** localize where the HF static enters (upstream mix/limiter/decode vs the web
resampler vs worklet), quantify it with a number, and (separately) find + propose a fix for
the SFX latency. Native-first where possible. Every claim needs a number.

Reference numbers to beat/compare: web capture **HF/THD 6.08x, flatness 0.62, peak 11025Hz**.

---

<!-- Investigators append `### <label>` sections below with raw numbers + verdict. -->

### probe:sfx-latency

**Verdict: CONFIRMED.** The dominant SFX (and song) output-latency term is the **SAB ring fill
depth**, which `PumpAudio()` keeps at ~FULL (`RING_FRAMES-1 = 32767` frames) every frame.
At ctx=48000 that is a **~673 ms** output-latency floor; the header even advertises ~743 ms
@ 44100 (`RING_FRAMES/44100*1000 = 743 ms`). This is a READ-ONLY code/path trace — no build.

**Path traced (keydown -> audio):**
1. JS `keydown` listener sets `window._rb3Keys` bitmask (rb3_game_input.cpp:20). Drained once
   per frame by `RB3GameInputPoll(frame)` (App.cpp:551), edge-detected -> `ExecButton`.
2. SFX trigger -> `SynthSample::NewInst` -> `RB3SampleInstNative::StartImpl()`
   (native/src/rb3_sampleinst_native.cpp:165) -> `AudioDevice::AddSource(this)` (line 213/247).
3. Same frame, `AudioDevice::PumpAudio()` (App.cpp:560, runs ~9 lines after the input poll) ->
   `MixSources()` mixes the new source into the ring at `writePos` and `js_audio_ring_write`
   advances `writePos` (AudioDevice_Web.cpp:541-667).
4. AudioWorklet `process()` drains 128 frames/quantum from `readPos` (audio-worklet.js:48).

**Latency budget (per stage, ctx=48000, frame_dt p50~20 ms per user log):**

| stage | term | ms |
|---|---|---|
| (1) input poll wait (once/frame, <=1 frame) | edge-detect on next `RB3GameInputPoll` | ~10 avg / ~20 worst |
| (2) source-add -> next `PumpAudio` | SAME frame (poll@551, pump@560) | ~0 |
| (3) **SAB ring queue depth ahead of read head** | `js_audio_ring_free_frames` queried once, then `while(freeFrames>0)` fills the ENTIRE free space -> ring sits at ~`RING_FRAMES-1`=32767 frames; new source appended at `writePos` waits the whole queue | **~673** (DOMINANT) |
| (4) worklet quantum | 128 frames @ 48000 | 2.7 |
| **TOTAL keydown->audio** | | **avg ~685 ms, worst ~705 ms** |

Steady-state ring fill (measured from the code's fill logic, not a capture): drains
`ctx*dt = 960` frames/frame @ 20 ms, then `PumpAudio` tops back to full -> oscillates
31807..32767 frames = **662..683 ms** queued. There is **no prefill/target-fill cap anywhere**
(grep `prefill|prebuffer|targetFill|minFill|keepAhead` in src/audio + worklet = 0 hits) — the
greedy fill is the bug. This matches the user's "few hundred ms" report (a perceptual
underestimate of ~0.67 s; the song-audio path has the same floor but no discrete onset to
notice it against). The deep ring was justified by the (now-disproven) underrun fear — the
header literally says "Enough headroom for 1-3 FPS WASM frame rates." underruns=0 over 30s
(frame p50~20 ms, one 85 ms blip) means that headroom is ~8x more than needed.

**(4) StreamReceiver mBuffer enlargement (0x18000->0xC0000, ~9.1 s, uncommitted in
src/system/synth/StreamReceiver.{h,cpp}) does NOT add SFX latency.** It is the SONG stream
source's PRODUCER-side decode buffer (`mBuffer` played directly as `mRingSize =
mNumBuffers*0xC000`; RB3StreamReceiverNative::RenderAudio reads decoded PCM from it). SFX is a
SEPARATE `AudioSource` (RB3SampleInstNative) — both AddSource into the same mixer, but SFX
never touches StreamReceiver's mBuffer. That buffer governs SONG-START latency + multitrack
underrun resilience only. (Note: RenderAudio there still consumes `mRingSize` fixed at 0x18000
per its comment at line 255 — orthogonal to SFX latency either way.)

**Proposed fix (concrete, web-only, since underruns=0):** cap how much `PumpAudio` writes so
the ring holds only a TARGET queue depth instead of filling all 32767 free frames. Keep
`RING_FRAMES=32768` (allocation unchanged) and add a target:

```cpp
// In PumpAudio(), after `int freeFrames = js_audio_ring_free_frames(...)`:
static const int kRingTargetFrames = (mDeviceSampleRate ? mDeviceSampleRate : 48000) / 10; // ~100ms
int used = (RING_FRAMES - 1) - freeFrames;          // currently queued frames
int writable = kRingTargetFrames - used;            // only top up to the target
if (writable <= 0) return;
freeFrames = std::min(freeFrames, writable);        // cap the greedy fill
```

`kRingTargetFrames = ctx/10 = 4800` frames @ 48000 = **100 ms** queue depth. Margin: the worst
observed frame gap was 85 ms (one blip), so a 100 ms cushion stays underrun-free with headroom;
80 ms (3840 frames) is the floor, 120 ms (5760 frames) the conservative choice.

**Numbers — current vs post-fix (100 ms target):**
- Current keydown->audio: **avg ~685 ms / worst ~705 ms** (ring queue ~673 ms dominates).
- Post-fix (100 ms target): **avg ~63 ms / worst ~123 ms** (poll ~10 + queue 50 + quantum 2.7).
- = **~10.8x reduction** in SFX latency; ring still 32768 frames so a transient frame stall up
  to (target) ms is absorbed, and underruns stay 0 for gaps < target.

**Note:** this is INDEPENDENT of the HF-static bug (this probe did not investigate the 6.08x
HF/THD; shrinking the ring does not touch the resampler/mix path). Artifacts: none (read-only).

### probe:resampler-hosttest

**Verdict: resampler EXONERATED.** The `PumpAudio` linear-interp + carry resampler
ALONE does NOT reproduce the 6.08x HF / 11025Hz-peak static. On real music it is
essentially transparent (HF *below* a high-quality resampler). The static is
UPSTREAM of the resampler (mix / limiter / decode) or in the worklet/browser stage.

**What I ran** (host-only, no browser; venv = `rb3/venv`, numpy 2.2.6 / scipy 1.15.3):
- Replicated the EXACT `AudioDevice_Web.cpp` PumpAudio resampling path (lines
  ~560-666: `step=mSampleRate/mDeviceSampleRate`, `sStart/sEnd/needTotal`, the
  `mResampleCarry[]`/`carryN`/`leftover`/`mResamplePos` carry-all logic, scalar
  linear interp `sOut=l0+(l1-l0)*t`) as a standalone numpy harness
  (`/tmp/w09res/resampler_hosttest.py`). MIX_BUF_FRAMES=8192, kResampleCarryMax=64.
- Drove it **44100 -> 48000** (the real web case: ctx=48000 => UPSAMPLING) with
  **realistic JITTERED chunk sizes** (base 800 dev-frames = 48000/60fps, +/-50%
  jitter; min chunk 245), exercising the carry path on 99.67% of chunks (below).
- HF/THD definition = **EXACT copy of `audio_verify.py spectral_div()`**:
  `hf_frac = PSD(4000..min(8000,nyq)) / PSD(>=50)`, Welch nperseg<=4096; thd_ratio
  = hf_frac(cap)/hf_frac(ref). Reference resampler = `scipy.signal.resample_poly`
  44100->48000 (polyphase, "good") on the SAME input.
- Decoded reference = `decode_reference.py beastandtheharlot --section full`
  (`/tmp/w09res/rb3_ref_beastandtheharlot_full.wav`, 44100/2ch), 12s loud window
  ~30s in, peak-normalized to 0.95 so both resamplers see identical input.

**Raw numbers** (`/tmp/w09res/run_hosttest.py`):

| signal | metric | LINEAR (PumpAudio) | resample_poly (good) | web capture (target) |
|---|---|---|---|---|
| **real song** | HF/THD ratio (cap vs poly) | **0.919x** | 1.000x (control) | **6.08x** |
| real song | HF/THD [audio_verify geometry: lin@48k->clean@44100 vs 44100 src] | **0.920x** | 1.001x | 6.08x |
| real song | aliasing >22050Hz | **0.0002%** | 0.0000% | -- |
| real song | global spectral peak | 105.5Hz (musical bass) | -- | 11025Hz |
| real song | energy in 11-13kHz band | 0.134% | 0.212% | strong peak |
| 20kHz **sweep** (torture) | HF/THD ratio vs poly | 1.256x | 1.000x | -- |
| 20kHz sweep | aliasing >22050Hz | 1.195% | 0.012% | -- |

- **On real music the linear interp has LESS HF than the good resampler (0.92x).**
  Linear upsampling is a mild low-pass -> it cannot manufacture 6x HF. Nowhere near 6.08x.
- The full-scale 20kHz sweep is the ONLY case where linear imaging shows at all
  (1.26x HF, 1.2% aliasing) -- and even that worst case is ~5x BELOW the observed
  6.08x, and real music has no sustained full-scale 20kHz content.
- No 11025Hz peak on the song (peak is the 105Hz bassline). The 11039Hz blip in the
  *sweep* row is just where the chirp's instantaneous freq sits in that window + its
  mild image -- NOT a fixed 44100/4 concentration.

**Carry path verified genuinely exercised** (`/tmp/w09res/carryproof.py`, jitter50%):
carryN per chunk = {0:1, 1:285, 2:20} -> **99.67% of chunks carry >=1 frame, 6.5%
carry 2 frames** (exactly the ~8% fractional-landing case wave-08 fixed). The old
single-sample carry dropped frames here; carry-all handles all of them.

**Click / pitch sanity** (`/tmp/w09res/sanity.py`, 1kHz pure tone, 4 jitter regimes
incl. tiny 128-frame chunks -> 1871 chunks, min chunk 25):
- pitch = **1002Hz** in all regimes (= 1000Hz at Welch bin res) -- NO chipmunk.
- **0 clicks** in all regimes; max sample-to-sample delta 0.11748 <= tone's own max
  slope 0.11781 -> literally no discontinuities.
- **HF>3kHz = 0.0000%** on the pure tone -> resampler adds ZERO broadband HF static.

**Artifacts:** `/tmp/w09res/{resampler_hosttest,run_hosttest,sanity,carryproof}.py`,
`/tmp/w09res/rb3_ref_beastandtheharlot_full.wav`.

**Implication for the loop:** the 6.08x HF / 11025Hz static is NOT created by the
44100->48000 linear-interp+carry resampler. Look UPSTREAM (the mix sum / SoftClip
limiter / vorbis decode at the engine 44100 rate) or in the SAB->worklet->browser
output stage (probe C). The 11025Hz = 44100/4 peak strongly suggests a quarter-rate
artifact AT the 44100 mix rate (e.g. a per-4-sample pattern in mix/limiter or a
decode-block boundary) that exists BEFORE this resampler runs -- consistent with the
host result that the resampler input->output is clean. A better resampler (polyphase/
windowed-sinc) would buy ~nothing here (already at 0.92x); spend effort on probe C.

### probe:web-bisect

**Verdict: headless web does NOT reproduce the user's static, because headless
picks a DIFFERENT runtime audio config.** Two independent gates make the headless
capture unrepresentative of the user's browser:
1. **Headless ctx.sampleRate = 44100 Hz (NOT 48000).** Engine log line
   `AudioDevice: initialized (web) -- 44100 Hz, ring 32768 frames` +
   `AudioWorklet connected (ctx 44100 Hz)` + `window._rb3Audio.ctx.sampleRate = 44100`.
   Because `mDeviceSampleRate (44100) == mSampleRate (44100)`, `PumpAudio`'s
   `resample` flag is **FALSE** -> the whole linear-interp+carry resampler is
   BYPASSED; `MixSources()` is pushed straight to the SAB. The user's real browser
   reported ctx=48000 (resample path ACTIVE). **Headless cannot exercise the
   resample path at all** -- a second, harder gate on top of the wave-05/08
   "steady-cadence headless misses the jitter-gated bug" pattern.
2. The capture (`rb3CaptureAudio`/`sCaptureBuffer`) taps the **fast-path mix
   `sMixBuffer` BEFORE the SAB/worklet** -> worklet underruns do NOT enter the WAV
   (verified: max zero-run = 3 samples = 0.07 ms over 30 s; 0.40% zeros, no silence
   gaps). So this WAV is a clean tap of the live **44100 mix** itself.

**What I ran:**
- `scripts/web/w9-web-bisect-capture.mjs --phase song --song beastandtheharlot
  --capsecs 33` (NEW; the stale `web-song-preview-audio.mjs`/`web-audio-capture.mjs`
  wait only 4.2 s but the engine `CAPTURE_SECONDS` is now **30 s** -> they never get
  a ready capture). Navigated headless to `game_screen` on beastandtheharlot, armed
  the 30 s engine capture, polled `rb3AudioStats` to ready, downloaded the WAV.
- `audio_verify.py /tmp/rb3_w9_web_song.wav --song beastandtheharlot --section full`.
- Direct numpy Welch spectral analysis (4096/8192-pt, 6x 5-s windows).

**Raw numbers** (`/tmp/rb3_w9_web_song.wav`, 30.0 s, 44100 Hz x2, peak 29490,
nonZero 97.5%, RMS 5632):

| metric | HEADLESS web capture (44100 mix, no resample) | USER real browser (ctx=48000, post-resample) |
|---|---|---|
| **HF/THD ratio** | **2.19x** (thr<2.0) | **6.08x** |
| **flatness** | **0.467** | **0.62** |
| **global spectral peak** | **DC / 105-242 Hz bass** (NO line) | **11025 Hz = 44100/4** |
| chroma corr (identity) | 0.78 (same song) | 0.69 |
| fp BER | 0.40 | 0.34 |
| speed ratio | ~1.0 (1.032x, conf 0.16, not a chipmunk) | 1.003x |
| clip-ratio / crest | 0.00% / 14.7 dB | 0.00% / 19 dB |
| HF>8k energy frac | 0.16-0.21 steady across all 6 windows | -- |
| per-window peak | 151/172/178/188/194/242 Hz (bassline) | -- |
| worklet underruns (headless) | **4757 events / 608769 padded frames / 4.0M total (~15%)** | **0** (user) |

**Reading:**
- The headless 44100-mix capture shows only **2.19x HF / 0.467 flatness** and **NO
  11025 Hz peak** (dominant peak is the 105-242 Hz bassline of this metal song; HF is
  the song's own cymbals/distorted-guitar content). It is **nowhere near** the user's
  6.08x / 0.62 / 11025 Hz signature. **Headless does NOT reproduce the static.**
- This **REFUTES probe-B's speculation** that the 11025 line is a quarter-rate
  artifact "AT the 44100 mix rate, BEFORE the resampler." If it lived in the 44100
  mix, this direct 44100-mix tap would show it -- it does not. The 11025 line is
  therefore introduced **specifically by the ctx=48000 resample path on the LIVE
  game mix, or by the SAB->worklet->browser output stage** -- neither reachable
  headless (headless = 44100, no resample). (Probe B host-tested the resampler on a
  CLEAN decoded reference and also saw no 11025 line; the open gap is the resampler
  on the *live multi-source jittered* 48000 path, which neither probe B nor this
  probe exercised end-to-end.)
- The 2.19x (vs 6.08x) elevated-but-not-static reading is just this song's intrinsic
  HF content (steady 0.16-0.21 HF>8k, musical flatness 0.24-0.28 per window) plus the
  count-in / star-power crowd+clap+achieve SFX bursting throughout (active source
  count 12-16, many `@22050Hz` crowd/clap one-shots). Not the user's broadband hiss.
- **Headless underruns = 4757 / ~15% padded** -- the OPPOSITE of the user's 0
  underruns. Headless render-quantum scheduling starves the ring; but since the
  capture tap is pre-SAB, this does not corrupt the WAV (confirmed: no silence runs).
  It does mean the headless audio *graph* is in a wholly different regime than the
  user's smooth-clocked real browser.

**Probe (3) pre-resampler tap:** the existing capture ALREADY taps pre-resampler in
the fast path (`sMixBuffer`), but only when ctx==mix (44100). There is **no headless
way to tap the live 48000 `sOutBuffer` post-resampler without forcing ctx=48000**,
which requires editing the compiled `new AudioContext()` (EM_JS, line ~138) to
`new AudioContext({sampleRate:48000})` and a full web rebuild (minutes) -- NOT done
here per the "no slow rebuild" instruction. probe `native-control` provides the
pre-web-path reference instead.

**Recommendation for the converge:** to reproduce the user's 11025/6x static headless,
the ONLY faithful path is to **force the AudioContext to 48000** (one-line EM_JS edit
+ one web rebuild) so headless takes the resample branch on the live game mix, then
re-capture `sOutBuffer`. If that reproduces 6x/11025, the resampler-on-live-mix is the
culprit (probe B's clean-reference test was too gentle); if it stays clean, the static
is real-browser-output-stage-specific (worklet/AudioContext SRC/driver). Until then:
**headless steady-cadence + 44100-no-resample is structurally blind to this bug.**

**Artifacts:** `/tmp/rb3_w9_web_song.wav` (30 s, 44100, the live mix tap),
`scripts/web/w9-web-bisect-capture.mjs` (NEW; waits the full 30 s capture window).

### probe:native-control

**Question:** does the NATIVE build (MixSources -> miniaudio; NO web resampler / SAB /
worklet / 48000 ctx) ALSO produce the fs/4 = 11025 Hz HF static, or is it web-path-only?

**What I captured (both 44100 Hz x2 raw PCM, null backend, real-time pace, post-mix
`DC3_DUMP_AUDIO` tap = verbatim copy of the MixSources `output` buffer — no resample in
the dump path, confirmed AudioDevice.cpp:358-455):**
- `beastandtheharlot` PREVIEW (30 s) — the SAME song as the user's `fucked-audio1.m4a`.
  Targeted via NEW `scripts/native/capture_song_gameplay.py` token-scroll
  (`{{music_library get_highlighted_node} get_token}` -> 'beastandtheharlot' at down-ix 7);
  preview path used because beast GAMEPLAY aborts on a `SongData::TrackInfo*` vector
  OOB during part-commit (separate bug, mogg decoded fine: `kBuffering->kReady` then abort).
- default first song GAMEPLAY (22 s) — negative-control on a DIFFERENT song.

**audio_verify (capture vs the SAME beastandtheharlot reference, apples-to-apples with
the web 6.08x):**

| metric | NATIVE beast preview | WEB user real-browser | web 44100 mix-tap (probe-B/bisect) |
|---|---|---|---|
| chroma corr (identity) | 0.83 (13 s ovl) — IS beast | 0.69 | 0.78 |
| speed ratio | 1.000x (not chipmunk) | 1.003x | ~1.0 |
| clip-ratio / crest | 0.00% / 19.8 dB | 0.00% / 19 dB | 0.00% / 14.7 dB |
| **HF/THD ratio** | **3.31x** (thr<2.0) | **6.08x** | 2.19x |
| **flatness 50-8k** (ref-free) | **0.558** | 0.62 | 0.467 |

**Direct spectral analysis (ffmpeg->f32 mono, Welch, loud middle region):**

| source (all 44100) | HF>8k frac | flatness | loudest line >6kHz | **11025-bin spike vs local (16384-pt)** | bp(10.8-11.25k)/total RMS |
|---|---|---|---|---|---|
| REF beast preview (clean mogg) | **0.012** | 0.249 | 6304-10923 Hz | **1.3x (NONE)** | 0.0285 |
| **NATIVE beast preview** | **0.301** | 0.558 | **11047 Hz = fs/4** | **50.2x** | 0.1845 |
| **NATIVE default gameplay** (diff song) | **0.277** | 0.440 | 11.0 kHz region | **10.2x** | 0.0996 |
| **WEB 44100 mix-tap** (probe-B's own WAV) | 0.16-0.21 | 0.467 | (bassline globally) | **12.9x** | 0.0711 |

**VERDICT — CONFIRMED: the static is SHARED-ENGINE, NOT web-path-only.**
1. The NATIVE binary — which has NO web resampler/SAB/worklet and runs at 44100 with no
   resample at all — produces a **strong narrowband spike at exactly 11025 Hz = 44100/4**
   (50.2x above local floor on beast preview; 10.2x on a *different* song's gameplay, so
   NOT song-specific) plus broadband HF (HF>8k frac 0.30 vs the clean mogg's 0.012 — **25x**;
   ~14-21% of total RMS sits in the 10.8-11.25 kHz band vs the reference's 2.8%).
2. The fs/4 line lives in the **44100 domain BEFORE any resample**: it lands at 11025 Hz on
   BOTH the 44100 native mix and the user's 48000-ctx browser capture. A 48000-domain
   resampler artifact would image at 12000 Hz, not 11025 — so the web 44100->48000 resampler
   is NOT the origin of the line.
3. **Reconciles probe-B/web-bisect's "headless does NOT reproduce it / 11025 not in the
   44100 mix" claim:** that probe looked at the *global* spectral peak (the loud 105-242 Hz
   bassline) and concluded "no 11025 line." But re-analyzing **its own** `/tmp/rb3_w9_web_song.wav`
   44100 mix-tap with a narrowband fs/4 probe shows a **12.9x spike at 11025 Hz** — comparable
   to native gameplay's 10.2x. The line WAS in the web 44100 mix all along; it was masked by
   the bassline in a global-peak view. So probe-B's "introduced by the ctx=48000 resample/output
   stage" conclusion is **REFUTED** for the *narrowband fs/4 seed* (the resample/output stage may
   still amplify the broadband floor: user 6.08x vs headless 2.19x).
4. The prior "native clean" claim (only ever on 20thcenturyboy) was song-dependent /
   global-peak-blind; on beast + the default song the native engine is clearly NOT clean.

**Localization for converge:** fs/4 = a **period-4 sample artifact** in the 44100-domain
MixSources output, present on every song, engine-side. The mogg is 15-channel; the prime
suspect is the multi-channel decode / N-ch->stereo downmix in the shared synth stream path
(`src/system/synth/StandardStream.cpp` SetPan/channel mix + VorbisReader), NOT the web
resampler and NOT the master limiter (limiter is a per-sample env that can't synthesize a
new spectral line; it runs identically native+web). Next fix wave should instrument
`AudioSource::RenderAudio` / the downmix to find the 4-sample-periodic write.

**Numbers to carry forward:** native beast preview **HF/THD 3.31x, flatness 0.558,
11025-bin spike 50.2x**; native default gameplay 11025-bin spike 10.2x (universal);
clean mogg ref 11025-bin spike 1.3x (none). Web-vs-native HF/THD delta = 6.08x vs 3.31x
(web ~1.8x worse, but BOTH have the fs/4 seed).

**Artifacts:** `/tmp/native_beast_preview.wav`, `/tmp/native_default_gameplay.wav`,
`/tmp/rb3_ref_beastandtheharlot_preview.wav` (clean ground truth),
NEW `scripts/native/capture_song_gameplay.py` (token-scroll to a named song -> gameplay),
analysis scripts `/tmp/spec_analysis.py` `/tmp/spec_ref.py` `/tmp/period_analysis.py`
`/tmp/highres_fs4.py`. Engine logs `/tmp/rb3-preview-52653.log`,
`/tmp/rb3-song-cap-60615.log` (the beast-gameplay TrackInfo OOB abort).
