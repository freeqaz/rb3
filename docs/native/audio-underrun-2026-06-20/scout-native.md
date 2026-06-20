# SCOUT-NATIVE — the native audio path & where a consumer under-run lives

**Scope:** map the NATIVE (non-web) audio pipeline end-to-end, locate where the
real-time consumer can starve, and specify the exact instrumentation to *count*
it. No fixes here — diagnosis + instrumentation plan only.

**Date:** 2026-06-20. Engine pinned `4087e92e` (rb3/native/CMakeLists.txt:74).

---

## 1. The pipeline, in one diagram

```
 MAIN THREAD  (App::RunOneFrame, once per rendered frame)
 ─────────────────────────────────────────────────────────────────────────
   TheSynth->Poll()                                  src/system/synth/Synth.cpp:175
     └ SynthPollable::PollAll()                      Synth.cpp:186
         └ StandardStream::SynthPoll() -> PollStream()   StandardStream.cpp:230/232
             ├ mRdr->Poll(budgetMs)  == VorbisReader::Poll   VorbisReader.cpp:253 (HX_NATIVE)
             │    └ SYNCHRONOUS decode loop, time-budgeted by `until` ms:
             │        while (CyclesToMs < until || first) { pcmout; ConsumeData; TryDecode; DoFileRead }
             │        └ StandardStream::ConsumeData()  StandardStream.cpp:377
             │            └ per channel: mChannels[ch]->WriteData(pcm, n*2)  -> base ring mBuffer
             └ for each channel: StreamReceiver::Poll()   StreamReceiver.cpp:122
                  └ cursor math + StartSendImpl/SendDoneImpl back-pressure
                                    │
                                    │  (mBuffer is the SHARED ring; producer writes,
                                    │   consumer reads — see §2)
                                    ▼
 AUDIO THREAD  (miniaudio real-time callback — SEPARATE OS thread on native)
 ─────────────────────────────────────────────────────────────────────────
   MaDataCallback(device, out, frameCount)           AudioDevice.cpp:146
     └ AudioDevice::MixSources(out, frameCount)       AudioDevice.cpp:367
         └ for each AudioSource: src->RenderAudio(mix, frameCount)  AudioDevice.cpp:392
              └ RB3StreamReceiverNative::RenderAudio()  rb3_stream_receiver_native.cpp:243
                   reads int16 mono from base mBuffer at mAudioReadPos,
                   converts -> stereo float, **ZERO-FILLS on starvation** (lines 305-309)
         └ additive mix + master one-pole peak limiter (SoftClip)  AudioDevice.cpp:408-444
```

**Producer = main thread inside `RunOneFrame` (once per rendered frame).**
**Consumer = miniaudio's own real-time thread, draining at the hardware clock.**
These are *genuinely concurrent* on native (unlike web, where both run on the
single JSPI main thread via `PumpAudio`). This is the key difference the brief
flags: **the native real-device case is the one that can exhibit the consumer
under-run; MILO_HEADLESS / `MILO_AUDIO_BACKEND=null` cannot (see §5).**

---

## 2. The ring buffer — who owns what

The ring is the base `StreamReceiver::mBuffer` (one per channel/stem). Native
enlarges it (StreamReceiver.h:55-69):

| field | native value | meaning |
|---|---|---|
| `mBuffer[]` | **0xC0000 = 16×0xC000 ≈ 9.1 s** | physical ring array (Wii was 0x18000 = 2 chunks ≈ 1.1 s) |
| `mRingSize` | `mNumBuffers*0xC000`, capped to array | realised ring depth |
| `mRingWritePos` | producer-owned | where WriteData appends |
| `mRingReadPos` | producer-owned | "free frontier" — chunk fully consumed + SendDone-acked |
| `mRingWrittenSpace` | producer-owned | valid buffered bytes ahead of free frontier |
| `mAudioReadPos` (native subclass) | **consumer-owned** `atomic<int>` | the play cursor |
| `mPlayedTotal` (native subclass) | consumer-owned `atomic<long long>` | monotonic bytes played (drives back-pressure) |

**Ring depth in seconds** is set by `mBufSecs` (StandardStream.cpp:150-172):
native floor **4.0 s** (env `RB3_STREAM_BUF_SECS` overrides), capped by the 9.1 s
array. So at steady state the consumer plays ~4 s behind the producer's decode
frontier. That deep buffer is *exactly* the prior mitigation for this bug class
(see StreamReceiver.h:56-65 comment, and StandardStream.cpp:154-172).

**Starvation = `mAudioReadPos` (play cursor) catches up to `mRingWritePos`
(write frontier).** In `RenderAudio` the available span is
`available = mRingWrittenSpace - (mAudioReadPos - mRingReadPos)`
(rb3_stream_receiver_native.cpp:274-281). When `available < bytesNeeded`,
`framesToRender < frameCount` and the tail is **silently zero-filled**
(lines 305-309). That zero-fill IS the click/glitch — and **nothing counts it.**

---

## 3. Producer cadence — why it can fall behind (the root mechanism)

The decode is **synchronous and time-budgeted per frame**, not buffered ahead by
a worker thread:

- `PollStream` passes the decode budget `Max(mFrameTimer.GetLastMs() * mThrottle,
  1.0f)` ms (StandardStream.cpp:236). During steady playback that's roughly *the
  previous frame's duration × `throttle`* — typically only a **few ms**.
- `VorbisReader::Poll` then loops `while (CyclesToMs(timer) < until || first)`
  (VorbisReader.cpp:283), decoding Vorbis blocks for all stems and calling
  `ConsumeData -> WriteData` until either the budget expires or the ring is full
  (`consumed == 0` -> break, line 293-294).

So the producer only refills **once per rendered frame**, and only for a few ms
of CPU. The consumer drains continuously at the hardware clock. The buffer
survives *because* it's 4 s deep — BUT it under-runs whenever the **main-thread
frame cadence stalls longer than the live ring depth ahead of the play cursor**:

- A long render frame (GPU stutter, venue/HUD draw spike — wave-10 measured web
  rAF p50 33 ms with 100-150 ms one-shots on screen transitions).
- An asset/`.milo` load or DTA parse that blocks the frame.
- The frame budget feeding `until` is *derived from the previous frame*, so a
  burst of slow frames also *shrinks* the decode budget right when you need it
  bigger — a mild negative-feedback wrinkle, but the 4 s ring usually absorbs it.

This is the **identical root cause the web wave-10 already proved** ("main-thread
audio-pump starvation", docs/native/audio-perf-loop/wave-10-audio-jitter-gc.md
lines 72-89) — except native's consumer is a true separate thread, so it cannot
be papered over by running producer+consumer in lock-step.

**Device-side cushion (separate from the ring):** miniaudio uses
`periodSizeInFrames = 512` (AudioDevice.cpp:188) and defaults to
`MA_DEFAULT_PERIODS = 3` (miniaudio.h:12217) → ~1536 frames ≈ **32 ms** of
hardware buffering. That's a *second*, much smaller cushion below the 4 s ring;
it only matters if the audio thread itself is preempted, not for producer
starvation.

---

## 4. The master-bus limiter (read so you DON'T chase it)

`MixSources` (AudioDevice.cpp:408-444): after the additive stem mix, a one-pole
stereo-linked peak limiter (`mLimiterEnv`, instant attack / 80 ms release,
`kLimThreshold 0.90`) feeds `SoftClip` (tanh soft-knee above 0.95). Wave-09
HF-static A/B knobs: `RB3_LIM_BYPASS=1`, `RB3_LIM_ATTACK_MS=N`
(AudioDevice.cpp:46-50, 419-426). **This is the gain path, already worked over —
NOT the under-run.** An under-run is the consumer feeding the limiter literal
zeros (silence gaps), not the limiter clipping. The limiter can't manufacture or
hide a starvation gap; leave it alone.

---

## 5. Why headless / null-backend CANNOT reproduce this (critical)

`scripts/native/capture_gameplay_audio.py` runs with
`MILO_HEADLESS=1 MILO_AUDIO=1 MILO_AUDIO_BACKEND=null DC3_DUMP_AUDIO=…`
(capture_gameplay_audio.py:77-78).

- `MILO_AUDIO_BACKEND=null` selects `ma_backend_null` (AudioDevice.cpp:217-225):
  an always-openable dummy device on a synthetic real-time clock. Its data
  callback still runs `MixSources`, and `DumpFramesToWav` (AudioDevice.cpp:447-454)
  captures the **producer/mixer output deterministically**.
- But there is **no independent real-time consumer racing a separate producer
  thread** under the null clock the way a hardware device does, and headless runs
  don't have real frame-time stalls (no GPU present). So a producer-vs-consumer
  starvation **cannot manifest** in this dump. The WAV proves the mix is *clean*
  (useful as the negative control that isolates the bug to the ring/consumer
  side) — it cannot reveal the under-run itself.

**To reproduce a consumer under-run you need a REAL device:**
(a) **native non-headless** — `MILO_AUDIO=1` with the default ALSA backend (NOT
`MILO_AUDIO_BACKEND=null`, NOT `MILO_HEADLESS` unless paired with `MILO_AUDIO=1`
and a real backend), running actual gameplay frames; or
(b) **web** — the AudioWorklet draining the SAB ring (already instrumented, §6).

---

## 6. The instrumentation that already exists (web) — mirror it on native

**Web already counts under-runs** and it's the template:

- `audio-worklet.js:31-41,96-122` — per-render-quantum: `underrunEvents`
  (quanta with a silence pad), `underrunFrames` (total padded), plus
  `minRingDepthThisWindow` (low-water early-warning). Posts `underrun-stats`
  every 0.5 s.
- `AudioDevice_Web.cpp:237-265` — `js_audio_underrun_stats(int*4)` +
  `js_audio_min_ring_depth` C bridges → `rb3AudioStats()`.
- Harness: `scripts/web/audio-stall-measure.mjs`, `audio-jitter-profile.mjs`,
  `web-audio-capture.mjs`.

**Native has NO equivalent.** The zero-fill at
rb3_stream_receiver_native.cpp:305-309 and the `RenderAudio` returning
`frameCount` regardless (line 331) mean starvation is completely silent to any
observer. `MixSources` (AudioDevice.cpp) also has no aggregate starvation signal.

---

## 7. Exact native instrumentation approach (recommended)

A starvation counter belongs at **two layers** (per-source AND aggregate),
because a single stem starving (one decode falling behind) and the *whole* mix
starving (frame stall) are different failure shapes.

### (A) Per-source counter — `RB3StreamReceiverNative::RenderAudio`
This is the most precise probe: it already computes `framesToRender` vs
`frameCount` (rb3_stream_receiver_native.cpp:283-285). When
`framesToRender < frameCount` the source under-ran by `frameCount - framesToRender`
frames. Add (all native-only, no match concern):

```cpp
// members (audio-thread-written, main-thread-read -> atomic)
std::atomic<uint64_t> mUnderrunEvents{0};   // callbacks with a silence pad
std::atomic<uint64_t> mUnderrunFrames{0};   // total zero-filled frames
std::atomic<int>      mMinAvailFrames{INT_MAX}; // low-water ring depth (frames)

// in RenderAudio, right after computing framesToRender:
int availFrames = available / bytesPerFrame;
{   // low-water mark (relaxed; observer-only)
    int prev = mMinAvailFrames.load(std::memory_order_relaxed);
    while (availFrames < prev &&
           !mMinAvailFrames.compare_exchange_weak(prev, availFrames,
                                                  std::memory_order_relaxed)) {}
}
if (framesToRender < frameCount) {
    mUnderrunEvents.fetch_add(1, std::memory_order_relaxed);
    mUnderrunFrames.fetch_add(frameCount - framesToRender, std::memory_order_relaxed);
}
```

### (B) Aggregate counter — `AudioDevice::MixSources`
Mirror the web worklet exactly so one number describes the whole bus. The cleanest
seam: have `AudioSource::RenderAudio`'s shortfall bubble up, OR sum the per-source
counters. Simplest: in `MixSources`, after the per-source loop, track whether ANY
source returned a short render. (Note `RenderAudio` currently always returns
`frameCount`; the device-layer aggregate is better fed by reading the per-source
atomics in (A), or by adding a parallel `int* shortfall` out-param. Per-source (A)
is sufficient and least invasive — prefer it.)

### (C) Expose over the existing HTTP debug API
Add an `/api/audio/underruns` handler (or extend an existing one in
`rb3/native/src/rb3_http_handlers.cpp`) that sums each registered source's
`mUnderrunEvents/Frames` + reports the min `mMinAvailFrames`. This gives the same
pollable contract as web's `rb3AudioStats()`, drivable from the existing native
HTTP harness (`scripts/native/*-capture.py` pattern, CLAUDE.md "Debug with the
native build").

### (D) Optional: a stderr one-liner under an env gate
Gate behind `RB3_AUDIO_UNDERRUN_LOG=1` (cf. existing `RB3_STREAM_AUDIO_DBG`,
rb3_stream_receiver_native.cpp:121): on each under-run event, `fprintf(stderr,
"UNDERRUN ch=%d short=%d avail=%d\n", …)` (rate-limited like the existing
`sCounter % 200` debug at line 320). Cheap, no harness needed, immediately
visible when running native with a real device.

**Repro driver:** run `rb3-native` with `MILO_AUDIO=1` and the DEFAULT (ALSA)
backend — NOT null, NOT bare headless — drive to gameplay over `RB3_HTTP`, let a
song play for 30-60 s, then read the counters. Under-runs > 0 with a low
`mMinAvailFrames` confirms the consumer starves; correlate spikes with slow
frames via the existing `RB3_FRAME_TRACE` recorder (rb3_frame_trace.cpp /
FrameTraceCounters.h) to attribute each gap to its frame-cost class.

---

## 8. Ranked suspects (most → least likely root of the residual under-run)

1. **Main-thread frame stalls starving the per-frame producer refill.** The
   producer only decodes a few ms once per `RunOneFrame`; a render/load/parse
   spike longer than the live ring-ahead depth drains `mAudioReadPos` into
   `mRingWritePos` → zero-fill. *Same root cause web wave-10 proved.* The 4 s
   ring usually absorbs it, so the residual is the TAIL of long stalls (screen
   transitions, mesh/texture/pipeline uploads — see FrameTraceCounters.h classes).
   **This is the #1 candidate and the thing the §7 counter + frame-trace
   correlation will confirm or refute first.**

2. **Decode budget starvation under sustained slow frames.** `until =
   GetLastMs()*mThrottle` shrinks exactly when frames are slow
   (StandardStream.cpp:236), so the producer decodes LESS per frame right when it
   needs MORE. With many stems (11-15) the per-frame budget may not refill what
   was drained, slowly bleeding the ring down over a run of heavy frames even if
   no single frame exceeds the ring depth. Check `throttle` config value and
   whether the budget floor (`1.0f`) is enough for full-band decode.

3. **Back-pressure / send-loop edge cases freeing chunks under the play cursor.**
   The `available` guard (`if (available < 0) available = 0;`,
   rb3_stream_receiver_native.cpp:280) explicitly handles "cursor lapped a
   just-freed chunk." If `SendDoneImpl`'s monotonic gate
   (lines 233-237) ever releases a chunk fractionally early relative to
   `mAudioReadPos`, a frame's worth could be zero-filled. Lower probability (the
   gate is monotonic-byte exact) but worth a hard look once #1/#2 are measured.

4. **miniaudio device-period preemption (3×512 ≈ 32 ms).** If the OS preempts the
   audio thread itself longer than ~32 ms, the device under-runs below the ring.
   Unlikely on a desktop with a real-time-ish ALSA thread, but the 512-frame
   period + only 3 periods is a small hardware cushion; bump `periods`/period size
   only if measurement points here (it won't help a producer starvation).

5. **First-frame / kInit handoff transient.** `PlayImpl` arms `mAudioReadPos =
   mRingReadPos` at the kInit→kPlaying transition
   (rb3_stream_receiver_native.cpp:150-175). A mis-timed first Play (ring not yet
   primed to 4 s) could under-run at song start only. One-shot, not the
   sustained-playback residual — lowest priority.

---

## 9. Key files / anchors

- `engine: src/audio/AudioDevice.cpp:146` — `MaDataCallback` (consumer entry)
- `engine: src/audio/AudioDevice.cpp:367-455` — `MixSources` (additive mix + limiter + WAV dump; **no starvation counter**)
- `engine: src/audio/AudioDevice.cpp:188` — period 512 frames; `miniaudio.h:12217` MA_DEFAULT_PERIODS=3
- `rb3: native/src/rb3_stream_receiver_native.cpp:243-332` — `RenderAudio` (the ring read; **zero-fill starvation at 305-309, uncounted**)
- `rb3: native/src/rb3_stream_receiver_native.cpp:274-281` — the `available` computation (starvation predicate)
- `rb3: src/system/synth/StreamReceiver.cpp:48-65` — `WriteData` (producer append)
- `rb3: src/system/synth/StreamReceiver.h:55-85` — ring members + native 9.1 s array
- `rb3: src/system/synth/StandardStream.cpp:232-297` — `PollStream` (per-frame producer drive; decode budget line 236)
- `rb3: src/system/synth/StandardStream.cpp:150-172` — `mBufSecs` native 4 s ring depth
- `rb3: src/system/synth/VorbisReader.cpp:253-312` — native synchronous time-budgeted decode loop
- `engine: src/audio/AudioDevice_Web.cpp:237-265` + `platform/web/assets/audio-worklet.js:31-122` — the under-run telemetry to MIRROR natively
- `rb3: scripts/native/capture_gameplay_audio.py:77-78` — null-backend dump (producer-only; can't show consumer under-run)
- prior art: `docs/native/audio-perf-loop/wave-10-audio-jitter-gc.md` + `wave-10-pump-telemetry-audit.md` (web under-run root cause = main-thread pump starvation, FIXED for web)
