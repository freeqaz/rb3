# SCOUT-WEB — End-to-end map of the WEB audio under-run path

Date: 2026-06-20
Engine pin: `4087e92e76c64830d9d7d23cfdaa1398312d6f38` (rb3/native/CMakeLists.txt:74).
Pinned tree == working tree for both audio files (verified `git diff --stat`).
Deployed `native/web/build/audio-worklet.js` is byte-identical to the engine source (verified).

## TL;DR

The web audio pipeline is a **single-producer / single-consumer lock-free ring** in a
SharedArrayBuffer. The producer is `AudioDevice::PumpAudio()` on the WASM main thread,
driven once per `App::RunOneFrame` (rb3/src/App.cpp:569, under `#ifdef __EMSCRIPTEN__`).
The consumer is the AudioWorklet `process()` on the real-time audio thread.

**On under-run (ring empty), the consumer emits literal ZEROS** (audio-worklet.js:88-92).
That is the audible discontinuity: every starved render quantum drops to 0 and snaps back
when data returns — a click on entry AND on exit of each starved span. There is no
fade/hold-last/cross-fade. So the residual "clipping/clicks/static" the user hears is
**consumer starvation → zero-pad steps**, exactly the under-run signature.

The whole adaptive-latency control law (5 prior commits) only changes *how much* the
producer keeps queued ahead; it does **not** change the empty-ring behavior, and it has at
least one structural hole (suspect #1) that lets the queue collapse to the floor right
before a stall.

## The pipeline, end to end

### Producer — `AudioDevice::PumpAudio()` (engine/src/audio/AudioDevice_Web.cpp:560-859)

WHEN: called every frame from `App::RunOneFrame` (App.cpp:569). The WASM main loop
(`main_web.cpp:835/843`) calls `sApp->RunOneFrame(frame)` from `requestAnimationFrame`, so
the producer cadence == the rAF/frame cadence (~16–33 ms when healthy, but **arbitrarily
long during a main-thread stall** — GC, WebGPU pipeline compile, asset decode, JSPI fetch).
This is the crux: the *producer cadence is not real-time*; only the consumer is.

Ring geometry (lines 40-44):
- `RING_FRAMES = 32768` stereo-float frames (~743 ms @44100, ~682 ms @48000).
- SAB layout: `Int32[0]=writePos`, `Int32[1]=readPos` (8-byte header), then interleaved
  stereo Float32 PCM.
- One slot is always reserved: free = `bufFrames - used - 1` (js_audio_ring_free_frames,
  line 231), so usable depth is `RING_FRAMES-1 = 32767` frames.

Per-pump sequence:
1. Bail if worklet not started, or `freeFrames <= 0` (lines 567-573).
2. Compute an **adaptive target latency** `targetFrames` (device-rate frames to keep
   queued ahead of the read cursor). Detail below.
3. `queued = (RING_FRAMES-1) - freeFrames`; `writable = targetFrames - queued`. If already
   at/above target, **return without writing** (lines 740-743). Otherwise clamp
   `freeFrames` down to `writable` — i.e. top up only to the target depth.
4. Loop `while (freeFrames > 0)`: mix `outChunk = min(freeFrames, MIX_BUF_FRAMES=8192)`
   frames via `MixSources()`, optionally resample mix-rate→device-rate, and push to the
   ring via `js_audio_ring_write()`.

Write granularity: up to `MIX_BUF_FRAMES = 8192` device-rate frames per inner iteration,
multiple iterations per pump until the target depth is reached.

Resampler (lines 749-857): linear interpolation, mix rate (`mSampleRate`, 44100) →
device/ctx rate (`mDeviceSampleRate`, usually 48000). Carry-all state
(`mResampleCarry[]`, `mResampleCarryN`, `mResamplePos`) carries every unconsumed mix frame
across pump chunks (this is the wave-08 single-sample-carry fix — already landed, do NOT
redo). Master-bus one-pole peak limiter + SoftClip runs inside `MixSources()` (lines
536-555) — also already landed; not the under-run.

Ring-FULL handling: there is **no overwrite**. The producer only ever writes into
`freeFrames` (≤ free space, further capped to the target), and `js_audio_ring_write`
advances `writePos` by exactly the frames written. So the producer can never clobber
unread data → ring-full is benign (it just stops writing). The bug is the *opposite* end:
ring-EMPTY.

### Consumer — AudioWorklet `process()` (engine/src/platform/web/assets/audio-worklet.js:58-125)

WHEN: the browser audio render thread calls `process()` once per render quantum =
**128 frames**, locked to the real audio clock (ctx.sampleRate). This never stalls; if the
ring has no data the quantum still must be filled.

Read sequence per quantum:
1. `readPos = Atomics.load(cursors,1)`, `writePos = Atomics.load(cursors,0)` (lines 68-69).
2. `available = writePos - readPos` (+bufFrames if negative) (lines 72-73).
3. `toRead = min(frames=128, available)` (line 78).
4. Copy `toRead` frames from `data[(rp % bufFrames)*2 ...]` into L/R (lines 81-86).
5. **Pad `[toRead .. frames)` with literal `0`** (lines 88-92) ← the under-run click source.
6. `Atomics.store(cursors,1, rp % bufFrames)` (line 94).
7. Underrun instrumentation: `underrunEvents`, `underrunFrames`, `minRingDepthThisWindow`
   low-water mark; postMessage a summary to the main thread every ~0.5 s
   (`sampleRate>>1` frames) (lines 96-122).

Sample-rate: the worklet runs at `sampleRate` (the worklet-global == ctx.sampleRate,
commonly 48000). The producer's resampler converts 44100→ctx rate before the SAB push, so
the ring already holds ctx-rate frames; the worklet does **no** rate conversion. This is
correct (chipmunk fix, b458b18). 1:1 frame read. **No rate mismatch at the worklet** — the
only rate risk is if `mDeviceSampleRate` is read back wrong at init (see suspect #5).

### Atomics protocol

- Single producer writes `writePos`; single consumer writes `readPos`. Each side only
  loads the other's cursor. This is the standard SPSC ring — correct in principle.
- Reserved slot (`-1`) keeps writePos==readPos meaning "empty" (never ambiguous with
  "full"). Correct.

## Every place a discontinuity / click can be injected on (or near) under-run

1. **Empty-ring zero-pad** (audio-worklet.js:88-92) — the primary, by design. Hard step to
   0 and back. Each starved quantum = up to two clicks (fall + recover).
2. **Partial-quantum zero-pad** (same code, `toRead < 128`) — when `available` is between 1
   and 127, the quantum plays `toRead` real samples then zeros mid-quantum: a click *inside*
   an otherwise-voiced quantum. This is the near-miss the low-water telemetry tracks.
3. **Producer write / writePos-publish ordering** (suspect #2) — `js_audio_ring_write`
   (lines 267-294) does `data.set(src, …)` (plain, non-atomic Float32 store) then
   `Atomics.store(cursors,0,newWritePos)`. There is **no release barrier** between the PCM
   payload write and the writePos publish, and the consumer does a plain `Atomics.load` with
   no matching acquire-then-data-read fence. On most engines `Atomics.store/load` are
   seq-cst and the heap is a single SAB so this *usually* holds, but it is not guaranteed by
   spec that the non-atomic `data.set` is visible before the atomic writePos bump → the
   worklet could read a freshly-"available" frame whose PCM bytes are still stale/zero →
   transient garbage/click at the leading edge of a just-written region. Low probability,
   high-severity-if-real.
4. **Resampler seam at chunk boundary** — the carry-all logic (lines 791-843) is the fixed
   path, but if `mResampleCarryN`/`mResamplePos` ever desync from the actual frames pushed,
   the interpolation reads a discontinuous pair across the seam → periodic click. Worth a
   sanity check but lower priority (prior wave fixed the obvious case).
5. **`MixSources` destructive pull vs. cap** (lines 801-810): when `needTotal > MIX_BUF_FRAMES`
   the chunk is shrunk and `newMix` recomputed; the `if (newMix < 1)` clamp pulls a minimum
   of 1 frame. Edge arithmetic here, if wrong, drops/duplicates a mix frame on large chunks
   → click. Lower priority.

## Ranked suspects for the residual UNDER-RUN

### #1 (highest) — Adaptive law shrinks the queue to the floor, leaving no slack for the next stall

`PumpAudio` keeps only `targetFrames` queued (lines 740-745); on every **clean** ~0.5 s
window it multiplicatively shrinks the target back toward the floor (lines 714-722,
`kShrinkPctNum=25`, floor `RB3_AUDIO_LAT_MIN_MS` default **50 ms**). The grower only reacts
*after* underruns/near-misses are already observed (lines 686-713), and only once "pressure"
reaches 2.0 (≥2 consecutive bad windows). So the steady-state queue sits near 50–120 ms.

A single main-thread stall longer than the current target depth (a WebGPU pipeline compile,
a GC pause, an asset-decode spike, a JSPI fetch — all documented elsewhere as multi-hundred-ms
on this build) starves the ring **before** the law can react, producing a burst of zero-pad
quanta. The control loop is *reactive* (grows after the fact) and aggressively *shrinks* in
between, so it is structurally prone to "shrink → get caught by the next stall → click →
grow → shrink again." This matches "occasional clicks during gameplay," not constant static.

Tests/levers: pin a large fixed latency `RB3_AUDIO_LATENCY_MS=400` (disables adaptation) and
listen — if clicks vanish, the adaptive law's floor/shrink is the cause. Also try raising the
floor `RB3_AUDIO_LAT_MIN_MS=200`. The real fix is likely: raise the floor, slow the shrink,
and/or make the grow proactive (the producer knows when a long frame `dt` just happened — see
rb3_frame_trace.cpp — and could pre-grow on a detected stall rather than waiting for the
worklet's 0.5 s report).

### #2 — Missing release/acquire fence around the PCM payload vs. writePos publish

`js_audio_ring_write` (AudioDevice_Web.cpp:280-293): non-atomic `data.set()` of the PCM,
then `Atomics.store(writePos)`. Worklet reads `Atomics.load(writePos)` then plain-reads the
PCM. No fence pairs the data write with the index publish. Spec-legal reordering could let
the worklet read not-yet-published samples → leading-edge click on a just-written span. The
fix is cheap and correct regardless: write PCM, then `Atomics.store` is already a seq-cst
fence on the cursor — but the *data* store is plain. Safest is to keep the store order and
add an explicit `Atomics.store`-fenced publish (it already is) AND ensure the consumer's data
read happens-after the `Atomics.load(writePos)` (it does textually, but add an
`Atomics.load`-based acquire or a `Atomics.fence`-equivalent if available). Verify whether
this is real by stress (suspect, not yet proven).

### #3 — Producer cadence is the rAF frame loop; any long frame = guaranteed starvation source

The producer only runs inside `RunOneFrame`. If a frame takes longer than the queued depth
(targetFrames at device rate), the ring drains to empty regardless of the law. The web build
has documented multi-hundred-ms frame spikes (pipeline pre-warm, venue build, asset decode).
This is the *mechanism* behind suspect #1 — even a perfectly-tuned law can't beat a stall
longer than the buffer. Mitigations: bigger floor, or decouple the pump from rAF (e.g. a
`setInterval`/timer pump, or pumping more than once per long frame). Note `RING_FRAMES` caps
the absolute max queue at ~682 ms, so the deepest possible buffer can't ride out a >680 ms
stall.

### #4 — Partial-quantum mid-frame zeroing amplifies small dips into clicks

Because the worklet zero-pads from `toRead` to 128 *within* a quantum (lines 89-92), a ring
that dips to e.g. 60 frames produces a click mid-quantum even though it's not a "full"
underrun. A hold-last-sample or short linear ramp-to-zero (instead of a hard 0) in the
worklet would convert these audible steps into inaudible smears — a robustness fix that helps
even if the producer side is improved. (Consumer-side under-run *concealment*.)

### #5 — `mDeviceSampleRate` mis-readback → slow rate mismatch → eventual periodic starvation

If `js_audio_init` returns the wrong `actualRate` (or the ctx later changes rate), the
producer resamples to the wrong rate; the worklet then drains the ring at the true ctx rate
faster (or slower) than the producer fills it, causing a *slow systematic* drift that
periodically empties the ring → regularly-spaced clicks. Lower likelihood (the chipmunk fix
addressed the obvious case) but worth confirming `mDeviceSampleRate == ctx.sampleRate` at
runtime via `rb3AudioStats()`/console. A drift would also show as a steady minRingDepth
decline in audio-jitter-profile.mjs.

## Why headless can't see this (confirms the brief)

`MILO_HEADLESS` / `capture_gameplay_audio.py` use the **null backend**: there is no
real-time consumer thread draining the ring on a fixed clock. The mixer (producer) is pumped
deterministically and dumped. So the producer WAV is the right tool to PROVE the producer
output is clean (isolate the bug to the consumer/ring side), but it can NEVER reproduce a
consumer starvation. Repro must be: (a) the WEB AudioWorklet draining the SAB ring under a
real `AudioContext`, or (b) native with a real (non-headless) miniaudio device.

## Repro / measurement tooling to use (already exists)

- `scripts/web/audio-stall-measure.mjs` — rAF gaps + longtasks + per-0.5s underrun stats +
  ring health; correlates main-thread stalls with worklet underruns. **Primary repro.**
- `scripts/web/audio-jitter-profile.mjs` — producer/consumer cadence jitter, latency
  GROW/SHRINK log, minRingDepth series (will show the queue collapsing to the floor — direct
  evidence for suspect #1).
- `scripts/web/web-audio-capture.mjs` — captures the worklet's ACTUAL output incl. the
  zero-pad glitches, via Playwright (the only capture that contains the consumer-side clicks).
- In-browser console: `rb3AudioStats()`, `rb3DumpSAB(n)`, `window._rb3Audio.underruns`.
- Levers: `RB3_AUDIO_LATENCY_MS` (pin fixed target), `RB3_AUDIO_LAT_MIN_MS` /
  `RB3_AUDIO_LAT_MAX_MS` (adaptive bounds), `RB3_STREAM_BUF_SECS` (producer-side stream
  depth, StandardStream.cpp:159 — already deepened to 4 s default for native/web).

## What is ALREADY done (do NOT redo)

- Carry-all resampler (single-sample-carry click fix) — 5a810c2 / wave-08.
- Master-bus one-pole peak limiter + SoftClip — 513dcd5 (mix-gain, not under-run).
- Chipmunk pitch fix (hardware-rate ctx + 44100→ctx resample) — b458b18.
- Adaptive output-latency law (pressure gate, edge-trigger per worklet window, soft-pressure
  low-water growth, 80% ring cap) — 7e5b87c, 5890147, 53fb203, 5ac9501. **The residual lives
  on TOP of this law — focus on its floor/shrink aggressiveness and the reactive-only growth
  (suspect #1), and the consumer-side concealment gap (suspect #4).**

## Key files / anchors

- engine/src/audio/AudioDevice_Web.cpp:560 — `PumpAudio` (producer entry).
- engine/src/audio/AudioDevice_Web.cpp:740-745 — target-depth cap (queue kept shallow).
- engine/src/audio/AudioDevice_Web.cpp:714-722 — clean-window multiplicative SHRINK to floor.
- engine/src/audio/AudioDevice_Web.cpp:267-294 — `js_audio_ring_write` (no data/index fence).
- engine/src/platform/web/assets/audio-worklet.js:78 — `toRead = min(frames, available)`.
- engine/src/platform/web/assets/audio-worklet.js:88-92 — **empty/partial → ZERO pad** (click).
- engine/src/platform/web/assets/audio-worklet.js:94 — `Atomics.store(readPos)`.
- rb3/src/App.cpp:569 — `PumpAudio()` driven from RunOneFrame (producer cadence == frame loop).
- rb3/src/system/synth/StandardStream.cpp:159 — producer-side stream buffer depth (4 s).
