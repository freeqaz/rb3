# WEB Audio Threading + Data-Flow Map (MAP-WEB)

Read-only trace, 2026-06-20. Foundation doc for the off-main-thread audio spikes
+ design. Every claim is cited `file:line`. Engine repo paths are relative to
`/home/free/code/milohax/milo-native-engine`; rb3 paths to `/home/free/code/milohax/rb3`.

---

## TL;DR (the load-bearing facts)

1. **Three threads exist on web today, not two.** Main thread (rAF), the
   AudioWorklet audio thread, and... that's it for audio. There is **no decode
   thread** and **no `-pthread`**. Decode + mix + resample + ring-write ALL run on
   the **main thread**, once per `requestAnimationFrame`.
2. **What crosses the SAB ring: final, fully-mixed, post-limiter, post-resample
   interleaved stereo Float32 at the AudioContext device rate** (commonly 48000).
   It is NOT per-stem. By the time anything reaches the SAB the mix is done.
   `AudioDevice_Web.cpp:885` / `:803` (`js_audio_ring_write`).
3. **The SAB is JS-allocated, OUTSIDE the wasm heap.** `js_audio_init` does
   `new SharedArrayBuffer(totalBytes)` in JS (`AudioDevice_Web.cpp:119`); the
   worklet reads it directly (`audio-worklet.js:82`). The wasm `HEAPF32` is a
   *separate, non-shared* buffer; `js_audio_ring_write` **copies** from `HEAPF32`
   into the SAB every pump (`AudioDevice_Web.cpp:277-278` source → `:275` dest).
   **This is why the worklet already works without `-pthread`/`-sSHARED_MEMORY`.**
   It also means the whole producer chain (decode → mix → resample) touches only
   the private wasm heap and the *final* copy crosses the boundary.
4. **The starvation cause:** the entire producer chain is gated behind the main
   thread's rAF cadence. Any main-thread longtask > buffered ring depth = the
   worklet drains the ring to empty and pads silence (`audio-worklet.js:166-170`).
   Band-aids in place: hold-last+ramp concealment, 140 ms adaptive latency floor,
   120 ms prime gate. None move the producer off-main.

---

## The two threads + what runs where

### Main thread — `App::RunOneFrame` (once per rAF)
`rb3/src/App.cpp:556-570`. Relevant ordered calls each frame:

```
TheTaskMgr.Poll();
if (TheSynth) TheSynth->Poll();          // <-- DECODE happens inside here
#ifdef __EMSCRIPTEN__
    AudioDevice::GetInstance().PumpAudio(); // <-- MIX + RESAMPLE + RING-WRITE here
#endif
... TheUI.Draw() ...                      // <-- the longtasks that starve the ring
```

Both the **producer (decode)** and the **pump (mix+resample+ring-write)** are
inline on this thread, sandwiched right before `Draw()`. When `Draw()` (or any
loader/GC/layout longtask) overruns the buffered depth, neither runs again until
the next rAF — and the audio thread has already drained the ring.

### Audio thread — `MiloAudioProcessor.process()`
`engine/src/platform/web/assets/audio-worklet.js:90-207`. Runs on the real-time
audio clock (128-frame quanta at `ctx.sampleRate`). Pure consumer: reads the SAB
ring, advances `readPos`, applies the prime-gate + fade concealment. It NEVER
decodes or mixes. It has zero access to wasm — only the JS SAB.

---

## What `PumpAudio()` does per rAF (exact)

`engine/src/audio/AudioDevice_Web.cpp:560-888`. Per call:

1. **Gates** (`:561-572`): bail if not initialized / worklet not started / ring
   full (`js_audio_ring_free_frames`, a JS Atomics read of writePos−readPos).
2. **Adaptive-latency law** (`:575-766`): computes `targetFrames` — the desired
   *queued* ring depth in **device-rate frames** — from the worklet's per-window
   underrun/low-water feedback. Pressure accumulator, grow-on-sustained-underrun,
   slow shrink. Floor 140 ms (`:646`), start 200 ms (`:603`), ceiling ≤80% ring.
3. **Top-up clamp** (`:769-774`): `writable = targetFrames − queued`. If already
   at/above target this pump, **return without producing anything**. Otherwise
   only fill up to the target depth (so SFX don't queue behind a full ring).
4. **Mix + resample + write loop** (`:784-887`), in `MIX_BUF_FRAMES`=8192-frame
   (`:48`) device-rate chunks until `writable` is satisfied:
   - **No-resample fast path** (`:787-805`, when ctx rate == 44100): call
     `MixSources(sMixBuffer, outChunk)` → optional capture → `js_audio_ring_write`.
   - **Resample path** (`:808-886`, ctx rate ≠ mix rate, the common case): carry
     unconsumed mix frames, call `MixSources(sMixBuffer+carry, newMix)` to pull
     mix-rate frames, then linear-interpolate mix-rate→device-rate into
     `sOutBuffer`, carry the leftover tail, `js_audio_ring_write(sOutBuffer,...)`.

### Does PumpAudio decode? **No.**
`MixSources` (`:499-556`) loops `mSources` and calls `src->RenderAudio(...)` on
each, then runs the master limiter (`:536-555`). `RenderAudio` for the song path
**does not decode** — see next section.

### How many frames written per rAF?
**Variable, capped by the adaptive target, not fixed.** It writes
`targetFrames − queued` device-rate frames (clamped to `freeFrames`), in ≤8192-
frame chunks. At steady state with a 140 ms floor @ 48000 that's ~6720 frames of
*depth*; per-pump it writes only enough to top the ring back to target (often a
few hundred to a couple-thousand frames at a steady ~60 fps; a whole 200 ms
burst at song start). At 1–3 fps (the comment's worst case, `:41`) it writes much
larger chunks per pump because more drained between pumps.

---

## What crosses the SAB ring boundary

**Final mixed stereo Float32 PCM at the device rate.** Layout
(`audio-worklet.js:5-8`, `AudioDevice_Web.cpp:44`):

```
Int32[0] = writePos (frames)   written by main thread (Atomics.store)
Int32[1] = readPos  (frames)   written by worklet     (Atomics.store)
Float32[2..] = interleaved L,R,L,R...   RING_FRAMES=32768 (~743ms @44.1k / ~683ms @48k)
```

The data put there has already been: per-stem rendered → additively summed
(`AudioDevice_Web.cpp:519-523`) → stereo-linked peak-limited + soft-clipped
(`:536-555`) → linear-resampled to device rate (`:847-858`). **The SAB carries one
fully-finished stereo bus. No per-source state, no decode buffers, no volumes.**

---

## Where each stem is decoded, and on which thread

### Song stems (the mogg) — the volume case
The mogg path uses the **matched-fork `VorbisReader`**, NOT FFmpeg, NOT stb_vorbis
for the song. Chain, all on the **main thread** via `TheSynth->Poll()`:

```
App::RunOneFrame  (main, per rAF)        rb3/src/App.cpp:563
  → Synth::Poll                          rb3/src/system/synth/Synth.cpp:186 (TheSynth->Poll @:295)
    → SynthPollable::PollAll()           rb3/src/system/synth/Synth.cpp:187(-area)
      → StandardStream::SynthPoll()       rb3/src/system/synth/StandardStream.cpp:230
        → StandardStream::PollStream()    rb3/src/system/synth/StandardStream.cpp:232
          → mRdr->Poll(timeBudgetMs)      StandardStream.cpp:235   == VorbisReader::Poll
            → vorbis_synthesis_pcmout     rb3/src/system/synth/VorbisReader.cpp:288  (HX_NATIVE path)
            → ConsumeData → StandardStream::ConsumeData → channel->WriteData(...)
                                          VorbisReader.cpp:290; StandardStream.cpp:465/470
          → for_each(mChannels, StreamReceiver::Poll)  StandardStream.cpp:238-240
```

**`VorbisReader::Poll` is the decoder, and it is time-budgeted** (`StandardStream.cpp:236`
passes `Max(lastFrameMs*throttle, 1or8)` ms; the loop runs until that budget
elapses, `VorbisReader.cpp:283`). It decodes Vorbis blocks and writes **decoded
int16 mono PCM** into each `StreamReceiver`'s `mBuffer` ring via `WriteData`
(`StreamReceiver.cpp:48`). The HX_NATIVE Poll explicitly notes "**on native
there's no background decode thread**" (`VorbisReader.cpp:299`) and breaks when
the ring is full to "wait for the audio callback to drain" (`:294`).

There is one `StreamReceiver` **per channel** (drums L/R, bass, guitar, vox,
backing — ~6 per song); each is a separate `AudioSource` in the mixer
(`rb3_stream_receiver_native.cpp:71-72, 123`).

### `vorbis_synthesis_poll` shim
`rb3/native/src/rb3_vorbis_poll_shim.cpp:11` — Harmonix's incremental Xbox
decoder is replaced by a one-shot `vorbis_synthesis`. So decode is **fully
synchronous within the time budget**, no incremental yielding.

### `RenderAudio` does NOT decode — it reads pre-decoded PCM
`rb3/native/src/rb3_stream_receiver_native.cpp:283-414`. The consumer side
(called from `MixSources` inside `PumpAudio` on web) just reads int16 mono out of
the **already-decoded** base `mBuffer` ring at its own play cursor, converts to
stereo float, applies vol/pan + fade concealment (`:363-378`), advances the
cursor. **No vorbis calls.** This is the producer/consumer split that matters:

- **PRODUCER (decode):** `VorbisReader::Poll` → `WriteData` into `StreamReceiver.mBuffer`.
  Driven by `TheSynth->Poll()`. On web = main thread.
- **CONSUMER (read decoded PCM + mix):** `RenderAudio` → `MixSources` → resample →
  SAB. Driven by `PumpAudio()`. On web = main thread.
- On **native**, the consumer side runs on the miniaudio audio thread instead
  (`AudioDevice.cpp` data callback), so native is already largely stall-immune for
  the consumer half — but native's *producer* (`VorbisReader::Poll`) is still on
  the main/synth thread. (`rb3_stream_receiver_native.cpp:14-69` describes both.)

### SFX / one-shots — `SampleInst`
`rb3/native/src/rb3_sampleinst_native.cpp:85,267` (`RB3SampleInstNative`, an
`AudioSource`). Menu/hit samples. These are decoded/loaded up front (sample data
resident), so their `RenderAudio` also just reads resident PCM — not a decode
hotspot. Not the song-starvation path, but they share the same mixer + pump.

### FFmpegAudioReader / StandardStream / stb_vorbis — which is live?
- **`VorbisReader` (matched fork)** = the live song-stem decoder on both
  native+web (mogg). This is the one that matters.
- **`FFmpegAudioReader`** (`engine/src/platform/FFmpegAudioReader.cpp`) exists in
  the engine but is the engine's generic reader; **not on the rb3 mogg path**
  (rb3 routes through its own `VorbisReader`/`StandardStream`). Confirmed: the rb3
  song chain above never touches it.
- **`stb_vorbis.h`** (`rb3/native/src/stb_vorbis.h`) is bundled glue; the live
  decode goes through the Emscripten `-sUSE_VORBIS=1` port
  (`engine/CMakeLists.txt:580`) via `vorbis_synthesis*`, not stb, for the song.
- `StandardStream` is the container/clock layer that owns the channels and the
  reader; it does NOT itself decode (it forwards `ConsumeData`).

---

## Ring sizes (so the spike can reason about depth)

- **SAB output ring** (mixed bus → worklet): `RING_FRAMES=32768` stereo float
  (`AudioDevice_Web.cpp:42`), ~683 ms @48k. Adaptive target keeps it ≤80% full.
- **Per-channel decoded-PCM ring** (`StreamReceiver.mBuffer`): on native/web
  `0xC0000` bytes = 16 chunks of `0xC000`, ~9.1 s of decoded int16 mono per
  channel (`StreamReceiver.h:65-66`, ctor `StreamReceiver.cpp:28-34`). The Wii
  build keeps the 2-chunk `0x18000` ring (`StreamReceiver.h:68`).

**Critical implication for the spike:** the decoded-PCM ring is HUGE (~9 s) and
already decoupled from the SAB. The decoder only has to keep ~9 s of int16 ahead.
**The fragile link is NOT decode-ahead of decoded PCM — it's the
mix+resample+ring-write step that must run on the main thread to move bytes from
the decoded-PCM ring into the SAB.** That is the smallest thing that must move
off-main.

---

## The smallest set of work that must move off the main thread

Ranked by necessity to keep the SAB fed during a main-thread stall:

### MUST move off-main (the actual fix): **mix + resample + ring-write**
i.e. the body of `PumpAudio` → `MixSources`(→`RenderAudio` per source)→ resample →
SAB write. This is what tops up the SAB. If a worklet (or worker) ran this on the
audio/worker clock instead of rAF, a stalled main thread could NOT starve the SAB,
because the consumer would refill itself from the already-decoded-and-resident
per-channel rings.

What this off-main mixer needs ACCESS to (today all in the wasm heap, main-owned):
- **The decoded-PCM per-channel rings** (`StreamReceiver.mBuffer`, ~9 s int16
  mono each) + each channel's **play cursor / write frontier / written-space**
  (`mAudioReadPos`, `mRingWritePos`, `mRingWrittenSpace`, `mRingReadPos`).
- **Per-source vol/pan/paused/finished** (`mVolume`, `mPan`, `mPaused`,
  `mPlayStarted` in `RB3StreamReceiverNative`; vol/pan also live in the engine
  `StreamReceiver`).
- **The mixer source LIST** (`AudioDevice::mSources`) + the limiter envelope
  (`mLimiterEnv`) + the resampler phase/carry (`mResamplePos`, `mResampleCarry*`).

Could that state live in a SAB the main thread fills less-often (decode-ahead)?
**Yes — and it largely already is decode-ahead.** Two shapes:
- **(A) Move only the mixer off-main, share the existing decoded-PCM rings.**
  Put `StreamReceiver.mBuffer` + its cursors + per-source vol/pan into shared
  memory the worklet/worker can read. The main thread keeps DECODING into those
  rings at its leisure (~9 s lead); the off-main mixer drains them on the audio
  clock. The cursor protocol (`mAudioReadPos` published by consumer,
  `mRingWritePos`/`mRingWrittenSpace` by producer) is ALREADY an
  atomics-style SPSC handshake (`rb3_stream_receiver_native.cpp:62-69, 314-319`),
  designed for exactly this cross-thread split — it's just running on one thread
  today.
- **(B) Move decode off-main too.** Only needed if the main thread can't keep ~9 s
  of decoded PCM ahead under load. Decode is the more expensive op, but it has a
  9-second cushion, so it tolerates main-thread jank far better than the
  millisecond-tight SAB top-up does. **Likely NOT required for v1** — decode-ahead
  on the main thread + off-main mixer (shape A) should suffice. Keep (B) as a
  fallback if profiling shows the decode budget itself starves under load.

### Does NOT need to move (can stay on main):
- **Vorbis decode** (shape A) — protected by the ~9 s decoded-PCM ring.
- The adaptive-latency control law (it's a slow, per-0.5s policy; can stay main-
  side and command the off-main mixer's target depth via a shared int).
- Capture/debug paths.

---

## Build-flag baseline (for the coexistence spike)

`rb3/native/CMakeLists.txt:957-961` + `engine/CMakeLists.txt:592-597`:

- **SET:** `-sJSPI`, `-sJSPI_EXPORTS=[_main,_rb3MainLoopTick,...]`,
  `-sALLOW_MEMORY_GROWTH=1`, `-sMAXIMUM_MEMORY=512MB`, `--use-port=emdawnwebgpu`,
  `-fwasm-exceptions`, `-sUSE_VORBIS/OGG/ZLIB`, `-sFETCH=1`.
- **NOT set (the coexistence question):** NO `-pthread`, NO `-sSHARED_MEMORY`, NO
  `-sAUDIO_WORKLET`, NO `-sWASM_WORKERS`, NO `-sPROXY_TO_PTHREAD`.
- **Today's SAB needs none of those** — it's a plain JS `SharedArrayBuffer`
  (`AudioDevice_Web.cpp:119`) the worklet reads via `Atomics`, with the wasm heap
  copied INTO it each pump. The wasm module itself is single-threaded, non-shared
  memory.

**The open spike** (next doc): can `-sAUDIO_WORKLET` / `-sWASM_WORKERS` /
`-pthread`+`-sPROXY_TO_PTHREAD` (any of which would let an off-main context run
*wasm code* — the mixer — against shared memory) **coexist with `-sJSPI`** in one
link? That must be answered with a real compile. The data-flow above says the
off-main context needs to (1) read the decoded-PCM rings + cursors + vol/pan from
shared memory and (2) run the mix+resample+ring-write loop — which is wasm code,
so it needs a wasm execution context off-main (worklet/worker), not just the JS
SAB the worklet already has.

---

## File:line index (the load-bearing cites)

| What | Where |
|---|---|
| Main-loop pump site (Synth::Poll then PumpAudio) | `rb3/src/App.cpp:556-570` |
| PumpAudio (gates, latency law, mix/resample/write) | `engine/src/audio/AudioDevice_Web.cpp:560-888` |
| MixSources (sum sources + limiter) | `engine/src/audio/AudioDevice_Web.cpp:499-556` |
| SAB ring write (HEAPF32 → SAB copy) | `engine/src/audio/AudioDevice_Web.cpp:267-294` |
| SAB allocation (JS, outside wasm heap) | `engine/src/audio/AudioDevice_Web.cpp:112-206` (`:119` new SAB) |
| Worklet consumer process() | `engine/src/platform/web/assets/audio-worklet.js:90-207` |
| Worklet reads SAB / prime-gate / fade | `audio-worklet.js:82, 113-134, 166-170` |
| Song decode (VorbisReader::Poll, HX_NATIVE) | `rb3/src/system/synth/VorbisReader.cpp:253-312` |
| Decode driver (PollStream → mRdr->Poll, budgeted) | `rb3/src/system/synth/StandardStream.cpp:232-296` |
| Synth::Poll → SynthPollable::PollAll | `rb3/src/system/synth/Synth.cpp:186-187, 295` |
| Decoded-PCM into channel ring (WriteData) | `rb3/src/system/synth/StreamReceiver.cpp:48-64` |
| Per-channel ring size (~9s) | `rb3/src/system/synth/StreamReceiver.h:65-66`; ctor `StreamReceiver.cpp:28-34` |
| Consumer reads decoded PCM (no decode) | `rb3/native/src/rb3_stream_receiver_native.cpp:283-414` |
| Producer/consumer cursor protocol (SPSC) | `rb3/native/src/rb3_stream_receiver_native.cpp:14-69, 314-319` |
| vorbis_synthesis_poll one-shot shim | `rb3/native/src/rb3_vorbis_poll_shim.cpp:11` |
| AudioSource interface (RenderAudio) | `engine/src/audio/AudioDevice.h:19-34` |
| Web build flags (JSPI, no -pthread) | `rb3/native/CMakeLists.txt:957-961`; `engine/CMakeLists.txt:567-597` |
