# Audio UNDER-RUN scout-history — "what's already been tried, do not repeat"

**Date:** 2026-06-20
**Scope:** the residual the user still hears is a CONSUMER-SIDE UNDER-RUN (buffer
starvation → clicks/static), NOT a mix-gain or decode bug. This doc inventories every
prior audio pass so the diagnose/implement agents don't re-walk old ground.

**TL;DR for the next agent:** the under-run is a *consumer starvation* symptom whose
root cause history says is **main-thread starvation of the producer pump** (web) and the
analogous single-thread starvation on native real-device. Three separate gain/decode/
resampler bugs were already found and fixed (do NOT re-attack them). The most likely
STILL-OPEN cause is below in §6. The fastest correctness lever that has NOT been tried is
**changing the under-run FILL behavior** (both consumers currently emit raw silence/zeros
on starvation — the audible "click/static" — when a hold-last-sample or short fade would
be inaudible). See §5.

---

## 1. The pipeline + where under-run is HANDLED (read this first)

All native-only (none in the Wii .o set → edit freely, no #ifdef gating):

| layer | file | role |
|---|---|---|
| native device | `engine/src/audio/AudioDevice.cpp` (~455 L) | miniaudio real-time callback → `MixSources()`; master one-pole peak limiter + SoftClip |
| native song stream | `rb3/native/src/rb3_stream_receiver_native.cpp` (~399 L) | RB3StreamReceiverNative: consumer `RenderAudio()` reads decoded PCM out of the base ring `mBuffer` |
| web producer | `engine/src/audio/AudioDevice_Web.cpp` (~942 L) | `PumpAudio()` (called from main loop) mixes + resamples + writes the SAB ring; adaptive output-latency law |
| web consumer | `engine/src/platform/web/assets/audio-worklet.js` (128 L) | real-time AudioWorklet drains the SAB ring |
| base ring | `rb3/src/system/synth/StreamReceiver.{h,cpp}` | shared decode ring `mBuffer`; **HX_NATIVE sizes it to `mNumBuffers*0xC000` up to `0xC0000` (~9.1 s)**; Wii stays 0x18000 |
| decode | `src/system/synth/StandardStream.cpp` `ConsumeData`/`PollStream` | fills the base ring; driven by `TheSynth->Poll()` on the MAIN loop |

**Under-run handling TODAY (the audible glitch):**
- **Web consumer** `audio-worklet.js:88-92` — on `available < frames` it **pads with raw
  silence (zeros)**. The comment literally calls this "the audible 'static'".
- **Native consumer** `rb3_stream_receiver_native.cpp:305-309` — on starvation
  (`framesToRender < frameCount`) it **zero-fills the remainder**.
- **Native device** `AudioDevice.cpp:367-369` — `MixSources` memsets output to 0 first;
  if the source produced fewer frames, that tail stays silent.

Neither consumer does hold-last / fade / interpolation. A single dropped frame =
discontinuity = click; a run of them = the "static."

**CRITICAL repro fact (from the brief, re-confirmed here):**
`scripts/native/capture_gameplay_audio.py` and `capture_song_gameplay.py` both run with
`MILO_AUDIO_BACKEND=null` + `MILO_HEADLESS=1` (verified: `capture_gameplay_audio.py:77`,
`capture_song_gameplay.py:98`). The null backend has **no real-time consumer thread** — it
pulls `MixSources` deterministically as fast as the producer can feed it. So these dumps
capture the **PRODUCER (mixer) output** and **cannot reveal a consumer under-run**. Use
them to PROVE the producer output is clean (isolating the bug to the ring/consumer), not to
reproduce the starvation. Consumer under-run only shows on (a) the WEB worklet draining the
SAB, or (b) native with a REAL (non-null, non-headless) miniaudio device.

---

## 2. Every audio commit, one-line "what it changed"

### Engine (`milo-native-engine`), newest→oldest
- `5ac9501` audio(web): **edge-trigger** adaptive-latency law on each NEW worklet window —
  fixes per-pump thrash (a stale window was re-firing the grow/decay ~15-30×/window, walking
  the target 50↔500 ms in <0.4 s). Gates the law on `u[2]=totalQuanta` changing. **HEAD audio commit.**
- `53fb203` audio(web): ring **low-water telemetry** (worklet posts per-window `minRingDepthFrames`)
  + **soft-pressure** adaptive growth (grows on near-misses, not just hard underruns).
- `5890147` audio(web): rework adaptive output-latency law — **pressure gate** (grow only ≥2
  consecutive underrun windows) + **multiplicative decay** + **80% ring headroom cap**.
- `7e5b87c` audio(web): first **adaptive output-latency buffer** (self-tunes vs worklet underruns).
- `5a810c2` audio(web): SFX output-latency cap + **carry-all resampler** + hardware-rate ctx +
  limiter diag knobs (`RB3_LIM_BYPASS`, `RB3_LIM_ATTACK_MS`).
- `b458b18` audio(web): fix **chipmunk** pitch — resample mix to the actual AudioContext rate (44100→ctx).
- `513dcd5` audio: fix **clipped-noise** via master-bus peak limiter + soft-knee (the gain fix).
- `9289e59` audio: add reachable miniaudio **null backend** (`MILO_AUDIO_BACKEND=null`) — the producer-dump path.
- `1abd595` gfx(standard): GX-faithful soft-clip of the lighting **sum** — *not audio* (lighting; name collision only).

### rb3 (game repo) — audio-relevant
- `08ec1442` (referenced in STATE/wave-09) **decimation-by-2 fix** in `StandardStream.cpp`
  `ConsumeData` — dropped a stray `src++` that read `src[2j]` → 2× chipmunk + broadband HF
  static. Gated `#ifndef HX_NATIVE` (Wii unaffected). **THE HF-static root cause.**
- `5ac9501` is the engine pin currently in `native/CMakeLists.txt:74` =
  **`4087e92e76c64830d9d7d23cfdaa1398312d6f38`** (engine HEAD; verified clean except an
  unrelated dirty `src/platform/FxSendNative.cpp` — FX send, not the under-run path).

**No audio work has landed since `5ac9501` (2026-06-09).** The under-run track was declared
"verified fixed end-to-end" on a quiet -O2 box (wave-10 verify) — but the user still reports it,
so that verification did not capture the user's real condition (see §6).

---

## 3. The three ALREADY-FIXED bugs — DO NOT RE-ATTACK

1. **Clipped-noise / gain (W2, `513dcd5`).** 11-15 stems summed ~3.2× full scale; the old 1.1×
   master gain + hard clamp square-wave-clipped. Fixed by a content-adaptive one-pole peak
   limiter (T=0.90, instant attack, 80 ms release) + soft-knee saturator. flat-top runs
   1124→0. The limiter was later EXONERATED for the HF static (wave-09 A/B: `RB3_LIM_BYPASS`
   leaves fs/4 HF unchanged). Do not touch the limiter for under-run.
2. **Chipmunk pitch (W5, `b458b18`).** Web AudioContext clamps 44100→48000 with no resampler →
   1.088× fast. Fixed by resampling the mix to the real ctx rate. Verified (440 Hz→440.00 Hz).
3. **HF broadband static / "fs/4 = 11025 Hz line" (W8/W9, `08ec1442`).** A stray `src++` in
   `StandardStream::ConsumeData` decimated-by-2 → 2× pitch + period-4 aliasing on the
   `mFloatSamples=true` (libvorbis/native) path. Wii uses int16 Tremor (other branch) so it
   matched. Fixed `#ifndef HX_NATIVE`. E>11k 13.4%→0.0%. **This was the "static" of waves
   05/08/09 — it is GONE. The residual under-run is a DIFFERENT mechanism.**

### Resampler carry saga (waves 05/08) — RESOLVED, do not re-open
- W5 (`b458b18`) added the 44100→ctx linear resampler (fixed chipmunk).
- W8 found that resampler carried only ONE sample between `PumpAudio` chunks while
  `MixSources` destructively pulls `newMix` frames → on ~8% of *jittered* chunks it silently
  DROPPED one input frame = a 1-sample click (`~4.7 ticks/s @ 60fps`). Fixed by carrying ALL
  unconsumed frames (`mResampleCarry[]`/`mResampleCarryN`, `5a810c2`). Host test: jittered
  seam-anomalies 139→0.
- W9 host-test EXONERATED the resampler entirely (0.919× HF vs a polyphase reference — it is
  transparent, cannot manufacture HF). The resampler is clean linear interp + full carry.
  **Do not replace or re-audit the resampler for the under-run.**

---

## 4. The limiter, succinctly
`AudioDevice.cpp:43-62, 408-444` (native) + the mirror in `AudioDevice_Web.cpp` (~510-535).
One-pole stereo-LINKED peak limiter, threshold 0.90, INSTANT attack, 80 ms release, + a
`SoftClip` soft-knee (0.95) backstop. `sPreGain` default 1.0 (`DC3_AUDIO_GAIN` override).
Reset in `Resume()`. A/B knobs `RB3_LIM_BYPASS=1`, `RB3_LIM_ATTACK_MS=N`. **Exonerated for
both HF static and (by construction — a per-sample envelope can't create silence) under-run.**
Leave it alone.

---

## 5. The HF-static probe (wave-09) — what it found + what's STILL open
Symptom (wave-09): web "static mixed with chipmunks, wayyy too high frequency." Decisive
measurements localized a narrowband **fs/4 = 11025 Hz** line + broadband HF present on BOTH
native and web captures but ABSENT in the clean ffmpeg mogg decode → engine-injected at the
44100 mix rate, in the per-stem decode. **Root-caused + fixed = the `08ec1442` decimation
bug (§3.3).** STILL OPEN from wave-09 that the next agent should NOT conflate with under-run:
- web HF/THD measured 6.08× vs native 3.31× — a residual ~1.8× web-vs-native HF delta that
  was never fully attributed (could be capture-chain lossy AAC, or a real worklet/SRC stage
  effect). Low priority; it is HF coloration, not a dropout.

---

## 6. Single most-likely STILL-OPEN under-run hypothesis (rank-ordered)

The wave-10 "FIXED" verification ran on a **QUIET -O2 release box (load 2.2)** and saw 0
underruns / 60 fps. The user hears it on a **real, loaded machine**. The root cause wave-10
nailed — **`PumpAudio()` runs INSIDE `App::RunOneFrame` on the single JSPI main thread (no
-pthread); any 33-117 ms render/GC/parse frame can't refill the ring → the worklet drains it
dry → silence-pad = click/static** — is *mitigated* by the adaptive buffer + 60 fps but NOT
eliminated. So:

> **H-PRIMARY (web): residual main-thread pump starvation under real frame-time variance.**
> The fix that LANDED is a bandage (adaptive latency grows the ring depth) gated on the
> worklet's per-window underrun/low-water feedback. Its known weak points, none retired:
> 1. **Floor is 50 ms** (`kStartMs=120`, `RB3_AUDIO_LAT_MIN_MS` default 50). A single render
>    frame longer than the CURRENT target depth still under-runs before the per-0.5 s law can
>    react (the law is per-window, the stall is per-frame). On a slower/contended box steady
>    fps may be <60 → frames routinely exceed 33 ms → recurring dips the bandage can't outrun.
> 2. The adaptive law only GROWS after **≥2 consecutive** underrun windows (`kPressureGrow`),
>    so isolated-but-frequent single-window stalls (the real-world pattern) never trigger
>    growth and keep clicking at the floor.
> 3. **Consumer fill is raw ZEROS** (`audio-worklet.js:88-92`) — every dip is maximally
>    audible. A hold-last-sample or 1-2 ms fade would make the SAME dip inaudible. **This is
>    the cheapest, never-tried correctness lever and it is consumer-side = exactly where the
>    user's symptom lives.** (Same applies to native `rb3_stream_receiver_native.cpp:305-309`.)
> 4. Structural fix (off-main-thread mix into the worklet / wasm-worker) is BLOCKED on
>    -pthread/SHARED_MEMORY vs the JSPI single-thread build — repeatedly deferred, still open.

> **H-SECONDARY (native real device): producer decode starvation.** Native `SynthPoll`/
> `PollStream` decode ALSO runs on the main loop (`App.cpp:858 TheSynth->Poll()`), while the
> miniaudio callback consumes on a real-time thread (`AudioDevice.cpp:146-149`). A long main
> frame → the base ring (deep, ~9.1 s, so resilient) is fine for sustained play, but at
> **song-start / seek / screen-transition** the producer hasn't pre-filled → `RenderAudio`
> zero-fills (`:305-309`) = the click. miniaudio period is 512 frames / ~10 ms — small. The
> deep `0xC0000` ring (`StreamReceiver.h:55-66`) should cover steady state; suspect the
> TRANSITION moments and the pre-fill depth at `kInit→kPlaying`.

**Stale-comment note (not a bug, but will mislead):** `rb3_stream_receiver_native.cpp:255`
says `const int ringSize = mRingSize; // base, fixed at 0x18000`. The comment is wrong —
`mRingSize` is set to `mNumBuffers*0xC000` (up to 0xC0000) in the ctor
(`StreamReceiver.cpp:21-36`). The CODE is correct (reads the live `mRingSize`); only the
comment lies. Don't let it send you down a "ring is too small" rabbit hole.

---

## 7. Repro / verify tooling — EXACT invocations + which SIDE each measures

| tool | side | invocation |
|---|---|---|
| `scripts/web/audio-stall-measure.mjs` | **CONSUMER** (worklet under-run + rAF gaps + ring health) | `node scripts/web/audio-stall-measure.mjs --port 8421 --play-secs 40` |
| `scripts/web/audio-jitter-profile.mjs` | **CONSUMER+producer cadence** (unified rAF/longtask/heap/GC/per-0.5 s underruns timeline) | `node scripts/web/audio-jitter-profile.mjs --port 8421 --play-secs 45` |
| `scripts/web/web-audio-capture.mjs` | **CONSUMER OUTPUT** (worklet's actual output incl. dropouts, Playwright) | `node scripts/web/web-audio-capture.mjs --port 8421 --phase song` (or `--phase menu` for SFX) |
| `scripts/native/audio_verify.py` | correctness of any WAV vs source mogg | `python3 scripts/native/audio_verify.py <capture.wav> --song <id> --section gameplay` ; selftest: `--selftest` |
| `scripts/native/capture_gameplay_audio.py` | **PRODUCER** (null backend, deterministic — CANNOT show consumer under-run) | `python3 scripts/native/capture_gameplay_audio.py /tmp/out.wav --secs 30` |
| `scripts/native/capture_song_gameplay.py` | **PRODUCER** (null backend, token-scroll to a named song) | `python3 scripts/native/capture_song_gameplay.py --song <id> ...` |

- Web requires the deployed build served: `python3 native/web/server.py` (http://localhost:8421);
  for a consumer-side repro you MUST use the browser/worklet path (the two native `capture_*`
  scripts are producer-only by the null backend).
- For a **native real-device** consumer repro (H-SECONDARY): run `rb3-native` WITHOUT
  `MILO_HEADLESS`/`MILO_AUDIO_BACKEND=null` so a real miniaudio device + its real-time
  callback are live (ALSA default on Linux). The producer dumps are the clean-reference control.
- Only 3 songs have moggs for `audio_verify`: `20thcenturyboy`, `25or6to4`, `antibodies`
  (+ `beastandtheharlot` reference was used in wave-09).

### `audio_verify.py --selftest` — RAN, PASSES (6/6)
Verdict columns: `case | expect | got | chroma | speed | clip% | align | ok`.
Verdicts: **MATCH** (same song, right rate, not clipped → exit 0) / **DEGRADED** (wrong-rate /
pitch / clipped → 1) / **WRONG-SIGNAL** (different song/noise → 2) / **SILENT/ERROR** (3).
Calibration: `CHROMA_SAME=0.65`, `RATE_TOL=0.005`, `CLIP_RATIO_BAD=0.005`, `FLATTOP_BAD=8`.
NOTE (from wave-09): chroma identity is so robust it returned **MATCH through heavy
corruption** (a false positive on the decimation static) — `audio_verify` confirms "right song,
right rate, not clipped" but is NOT a sensitive under-run/dropout detector. For under-run use
the worklet under-run counters / low-water-mark from the `.mjs` tools, or measure silence-run
length on a real-device capture.

---

## 8. Recommended next-agent path
1. **Reproduce on the mechanism, not the null backend** (the brief's core insight): run
   `audio-stall-measure.mjs` / `audio-jitter-profile.mjs` in-browser on a **loaded** box (the
   user's condition), OR `rb3-native` with a REAL audio device, and read the worklet
   under-run counters / low-water-mark + the silence-run lengths. Confirm the residual is a
   genuine consumer dropout, characterize its rate.
2. **Prove the producer is clean** with `capture_gameplay_audio.py` + `audio_verify.py` (it
   should be MATCH/clean — that isolates the bug to the ring/consumer side, as expected).
3. **Cheapest correctness fix first (never tried):** change consumer under-run FILL from raw
   zeros to hold-last-sample / short fade in `audio-worklet.js:88-92` AND
   `rb3_stream_receiver_native.cpp:305-309`. Makes the SAME dip inaudible without changing the
   buffer math. Then re-measure perceived static.
4. Only then consider the deeper levers (lower the adaptive floor's reactivity per-frame, or
   the blocked off-main-thread mix) if the fill-fix isn't enough.
