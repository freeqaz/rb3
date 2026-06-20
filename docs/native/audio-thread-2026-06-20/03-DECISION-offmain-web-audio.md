# ARCHITECTURE DECISION — feeding web audio off the main thread

**Date:** 2026-06-20
**Author:** architect (design synthesis over the MAP + SPIKE + BASELINE wave)
**Engine pin context:** `884ab17` (rb3 `native/CMakeLists.txt:MILO_ENGINE_PIN`)
**Status:** DECIDED. Build path chosen, MVP scoped, success metric pinned.

---

## The problem, one paragraph

On web, the audio PRODUCER (`AudioDevice::PumpAudio` → `MixSources` → 44100→ctx
resample → SAB ring-write) runs **once per `requestAnimationFrame` on the main
thread** (`rb3/src/App.cpp:569`). The CONSUMER (`audio-worklet.js`) drains the SAB
on the real-time audio clock. Any main-thread longtask (GC, WebGPU pipeline
compile, asset decode, layout) longer than the buffered ring depth empties the
ring → under-run → click. The measured break point of the current band-aid build
(140 ms adaptive floor, 743 ms ring) is **800 ms: 38% dropout, 880 ms audible gap**
(`STALL_BENCH_BASELINE.md`). Surviving even a 200 ms stall today costs **~500 ms of
latency** (the adaptive law rides its ceiling) — the exact thing the user rejects
for a rhythm game. The fix is structural: **produce audio from a context that
survives a stalled main thread**, not a bigger ring.

The native build already does this — it mixes on the miniaudio audio thread, off
the main thread, pulling from ~9 s per-channel rings, and is verified MATCH on a
real device (`aloop-native-realdevice-capture-verify.md`: chroma 0.95, 0% clip,
1.001× speed). **Native is the reference the web fix converges to.**

---

## The one insight that decides everything

The off-main mixer does NOT need to move decode. The expensive op (Vorbis decode,
`VorbisReader::Poll` driven by `TheSynth->Poll()`) writes into a **per-channel
decoded-PCM ring that is already ~9 s deep** (`StreamReceiver.mBuffer[0xC0000]`,
16 chunks × `0xC000`, int16 mono — `StreamReceiver.h:65-66`, confirmed in source).
That ring is **already an SPSC handshake**: `mRingWritePos`/`mRingWrittenSpace` are
producer-owned, `mAudioReadPos` is consumer-owned with acquire/release loads, and
the consumer read is documented racy-benign (`rb3_stream_receiver_native.cpp:60-72`).
It runs on one thread today **only by accident of where `PumpAudio` is called.**

So the smallest thing that must move off-main is the **millisecond-tight
mix+resample+ring-write loop** — NOT decode. Keep decode on the rAF/JSPI thread
behind the 9 s cushion ("shape A"); move only the mixer to the audio clock. A
main-thread freeze then cannot starve audio until it exceeds the **decode-ahead
depth** (seconds), which is decoupled from output latency. **This is exactly why
native is stall-immune, and it is reproducible on web.** Both spikes confirmed it
empirically (below).

---

## Candidate evaluation

Four candidates. For each: feasibility (from the spikes), stall-resilience (from
the bench), A/V latency, build-system/JSPI risk, effort.

### A. Pure-JS AudioWorklet mixes per-stem SAB rings (compat-matrix option d2)

The AudioWorklet (plain JS, the same drainer we already ship) reads N per-stem SAB
rings + a small control block and **mixes on the audio thread** (additive sum ×
gain/pan + soft-clip + the linear resampler — the same shape as `MixSources`). The
main thread only *tops up* the deep stem rings on a relaxed cadence (decode stays
on rAF/JSPI).

- **FEASIBILITY — PROVEN (real headless-Chromium A/B).** `SPIKE_WORKLET_SIDE_MIX.md`
  built the exact architecture standalone (no wasm, no engine boot), injected
  controlled main-thread stalls, and read under-runs from the *same* worklet
  instrumentation the production build uses, plus an FFT correctness tap. The
  off-main mix is the RIGHT signal: all 6 stem sines present, amplitudes within
  **0.6 dB** of the main-thread mix, and correct **through** the stall.
- **STALL-RESILIENCE — best measured.** Sweep `0,100,400,800,1500 ms`:
  **0.00% under-run across the WHOLE sweep including 1500 ms**, where the baseline
  breaks at 800 ms (6.75%) and is catastrophic at 1500 ms (47.26%). The hard sweep
  proved the model falsifiably: the spike survives a **2000 ms** freeze at 0%
  (minRing → 1003 ms = 3000−2000, the predicted residual) and breaks only at
  3500 ms — *precisely when the stall exceeds the stem-ring depth*. Stall budget =
  decode-ahead depth (a capacity we control: the real ring is ~9 s, 3× the
  prototype's 3 s), **decoupled from output latency.**
- **A/V LATENCY — best.** Output SAB shrinks to a fixed **60–80 ms floor** because
  it no longer absorbs stalls (that's now the upstream stem ring's job). Wins on
  dropout AND latency simultaneously — the opposite of the rejected adaptive-ceiling
  trade.
- **BUILD-SYSTEM / JSPI RISK — ZERO.** No `-pthread`, no `-sAUDIO_WORKLET`, no
  `-sWASM_WORKERS`, no shared wasm memory, no new emcc flags. The wasm module stays
  single-threaded + unshared, so it **cannot conflict with `-sJSPI`** — it never
  touches the wasm threading model. COOP/COEP + SAB are already in production
  (`server.py`). This is the only path with no build spike at all.
- **EFFORT — medium.** ~150 lines of JS (port `MixSources` additive-sum + limiter +
  the 44100→ctx linear resampler into the worklet) + marshalling the existing
  StreamReceiver rings/cursors/vol-pan into SABs + moving the adaptive law to
  "command a target depth via one shared int." No toolchain risk; cost is pure
  engineering, and the DSP is small and already wait-free.

### B. Wasm Audio-Worklet DSP on the audio thread (compat-matrix option a)

`-sAUDIO_WORKLET=1 -sWASM_WORKERS=1` flips the wasm module to **shared memory** and
runs a wait-free C `Process()` callback on the audio thread
(`emscripten_create_wasm_audio_worklet_processor_async`). The callback drains the
per-channel rings (which now live in the *shared* wasm heap, directly readable) and
writes output — reusing the exact C++ `MixSources`/limiter/resampler bytes.

- **FEASIBILITY — toolchain PROVEN, mixer NOT yet ported.** `02-spike-offmain-jspi-
  RESULT.md` answered the deferred make-or-break question with a real compile at four
  escalating levels: (1) isolated link of `-sJSPI -sAUDIO_WORKLET -sWASM_WORKERS
  -sALLOW_MEMORY_GROWTH -sMAXIMUM_MEMORY=2GB` → **EXIT 0**, emits growable *shared*
  memory (`flags=0x03`) with no warning; (2) a **real JSPI suspend/resume worked
  inside the shared-memory module** (the #24302 failure surface — it didn't bite);
  (3) the **full rb3-web bundle** compiled+linked+deployed a 29 MB shared+JSPI wasm,
  emdawnwebgpu recompiled cleanly for shared memory; (4) it **BOOTED in headless
  Chromium** to `intro_movie_screen` (`booted=true frame=32`, `sharedMem=true`,
  0 fatals) — meaning the JSPI async `.milo` fetch path ran on the shared-memory
  module. **The build-flag spike is closed: not a dead end.** What it did NOT do:
  port `MixSources` into the callback, move the SAB write, or measure under-runs.
- **STALL-RESILIENCE — expected equal to A (not yet measured).** Same shape-A
  architecture (audio-thread mix off deep rings); resilience again equals the
  decode-ahead depth. No bench numbers exist for this path yet — the spike answered
  "can the toolchain do it," not "is dropout fixed."
- **A/V LATENCY — equal to A** (same fixed-floor output ring design).
- **BUILD-SYSTEM / JSPI RISK — LOW but non-zero.** The headline risk (shared memory
  × JSPI × growth × 2GB) is **retired** by the spike on emcc 5.0.2. Residual: (i)
  boot/song-load memory under the shared *growable* heap at **full heavy song load**
  was not driven (boot only reached intro cleanly); (ii) every static ctor now runs
  per-worker-thread, multiplying the #24302 suspend-in-ctor surface (didn't bite at
  boot, but full-load is unverified); (iii) the whole bundle recompiles to shared
  memory — a heavier artifact and a flag that touches every TU. Do **not** add
  `-pthread`/`PROXY_TO_PTHREAD` (re-introduces #19287 entry-rewiring for zero audio
  benefit; the spike deliberately avoided it).
- **EFFORT — medium-high.** Toolchain is free now, but: port the mixer + decoders'
  consumer side to the C worklet callback (must call NO JSPI/Asyncify import — only
  shared memory), pick ONE drainer (replace the JS `audio-worklet.js` with the wasm
  callback), make capture/debug taps shared-memory-safe, and verify full-load memory.

### C. pthread / Worker off-main mix (compat-matrix options b/c)

A Wasm Worker (`-sWASM_WORKERS`) or pthread (`-pthread [+ -sPROXY_TO_PTHREAD]`) runs
the mixer on a background thread, pushing to the SAB the JS worklet drains.

- **FEASIBILITY — possible, strictly dominated.** Same shared-memory module as B
  (so it inherits B's now-retired build risk), but a Worker/pthread is **NOT the
  real-time audio thread** — it pushes on a `setInterval`/`Atomics.wait` cadence and
  races the OS scheduler instead of the audio callback. You'd still need a worklet
  (JS or wasm) as the actual drainer, so this is *additive* to A or B, never a
  replacement.
- **STALL-RESILIENCE — worse than A/B in principle** (the producer thread can itself
  be descheduled; it doesn't run on the audio clock). Not measured; not worth
  measuring.
- **LATENCY — worse** (timer cadence adds jitter the audio-clock paths avoid).
- **BUILD RISK — highest.** Option c (`PROXY_TO_PTHREAD`) hits #19287's proxied-entry
  `_main_thread` JSPI-export rewiring — the spike deliberately avoided it. Option b
  (Wasm Worker) is shared-memory like B but with no realtime thread payoff.
- **EFFORT — highest, lowest payoff.** Out.

### D. Deeper decode-ahead only — the do-nothing baseline

Leave production on the main thread; make the output ring (and the adaptive ceiling)
even bigger so a longer stall is absorbed.

- **FEASIBILITY — trivial, already maxed.** This is what the band-aid build does.
- **STALL-RESILIENCE — fundamentally capped.** The baseline already rides the
  adaptive law to its **500 ms ceiling** to survive 400 ms and STILL breaks at
  800 ms (`STALL_BENCH_BASELINE.md`). The producer is gated by rAF; a stall longer
  than the ring empties it regardless of ring size, and the resilience is
  history-dependent (only resilient *after* a recent glitch inflated the buffer).
- **A/V LATENCY — unacceptable, and the explicit user rejection.** Every ms of stall
  tolerance is bought as a ms of output latency. "180 ms blanket is lazy and bad for
  a rhythm game."
- **BUILD RISK — none. EFFORT — none.**
- **VERDICT — REJECTED.** It is the problem statement, not a solution. Listed only as
  the A/B floor every other candidate must beat.

---

## Ranking

| Rank | Candidate | Feasibility | Stall-resilience | A/V latency | JSPI/build risk | Effort |
|---|---|---|---|---|---|---|
| **1** | **A — pure-JS worklet mix from SAB stems** | PROVEN (real browser A/B + FFT) | **0% through 1500 ms; budget = ~9 s decode-ahead** | **best (60–80 ms fixed floor)** | **ZERO (no new flags, no shared wasm)** | medium |
| **2** | **B — wasm Audio-Worklet DSP** | toolchain PROVEN; mixer port pending | expected = A (unmeasured) | = A | LOW (shared-mem×JSPI retired; full-load mem open) | medium-high |
| 3 | C — pthread/Worker off-main mix | possible | worse (not the audio clock) | worse (timer jitter) | highest (option c #19287) | highest |
| 4 | D — deeper decode-ahead only | trivial | capped; breaks ≥800 ms | **rejected (the user's complaint)** | none | none |

**Decision: build A. It is the only path with proven end-to-end stall-resilience AND
zero build-system risk.** It wins on dropout and latency simultaneously, needs no new
emcc flags, and cannot collide with `-sJSPI`. B is a viable, now-de-risked *upgrade*
to keep on the bench — pursue it only if A's pure-JS mix proves insufficient (it
won't for 6–15 stems × 128-frame quanta — the spike ran at 0 under-run with headroom)
or if exact C++ DSP byte-reuse becomes a hard requirement. C and D are out.

---

## MVPs to build next loop

### MVP-1 (PRIMARY) — Worklet-side mix from per-stem SABs (path A, real engine)

**Scope (buildable):**
1. **Publish the stem rings to SABs.** For each `RB3StreamReceiverNative`, expose its
   decoded-PCM ring (`StreamReceiver.mBuffer`, int16 mono @ mix rate) + its cursors
   (`mRingWritePos`/`mRingWrittenSpace` producer-owned, `mAudioReadPos` consumer-
   owned) + per-source vol/pan/paused/finished into a `SharedArrayBuffer` per stem.
   Either back `mBuffer` with a SAB directly, or copy-into-SAB on `WriteData`
   (`StreamReceiver.cpp:48`). Engine/native-only edit (`engine/src/audio/*`,
   `rb3/native/src/rb3_stream_receiver_native.cpp`) — match-neutral.
2. **Port the mixer into `audio-worklet.js`.** Move `MixSources`' additive-sum + the
   stereo-linked peak limiter + the 44100→ctx linear resampler into the worklet JS
   (~150 lines; the prototype `spike-worklet.js` is the template). The worklet reads
   N stem rings on the audio clock, mixes, and outputs directly — replacing the
   current "drain one pre-mixed output ring" consumer.
3. **Demote `PumpAudio` to a decode/top-up pump.** On the rAF/JSPI thread it now only
   keeps the stem SABs filled from decode (decode already happens in `TheSynth->Poll`)
   — no mix, no resample, no output-ring write. Decode stays on main behind the ~9 s
   cushion (shape A).
4. **Move the adaptive law to command a target depth via one shared int**; shrink the
   output to a **fixed 60–80 ms floor**.

**Success metric (pinned, measured by the existing bench):**
Re-run `scripts/web/audio-stall-bench.mjs` (the *full-engine* bench, not the spike's
synthetic harness) on the rb3-web build:
- **~0% under-run across the whole sweep `0,50,100,200,400,800` ms** — the baseline
  breaks at 800 ms (38%). **Primary gate: 0% at the 400 ms stall** (the task's
  explicit target vs the 140 ms-floor baseline), and **0% at 800 ms** (beat the wall).
- Achieved at a **fixed ≤80 ms output latency floor**, NOT by riding the adaptive law
  to 500 ms (verify the latency log stays at the floor through the sweep).
- Compare `curve.csv` directly against `STALL_BENCH_BASELINE.md`.
- Correctness gate: `audio_verify.py`/`web-audio-capture.mjs` on a real captured song
  → still MATCH (chroma ≥ 0.65, speed 1.0×, 0% clip), converging to the native
  reference (`aloop-native-realdevice-capture-verify.md`).

**Why this is the right first build:** zero toolchain risk, the data-flow is already
proven in a real browser at 0% under-run through 1500 ms, and it directly reproduces
native's stall-immunity. If it lands, the mission is done with no build spike.

### MVP-2 (CONTINGENT / parallel de-risk) — full-load memory + drainer check for path B

Only schedule if MVP-1 reveals a need for wasm DSP byte-reuse, OR run a cheap parallel
de-risk now to keep B ready:
- Take the proven `wt-jspi-audio-spike` shared+JSPI bundle and **drive it through a
  full heavy song load** (not just boot-to-intro) with `RB3_AUDIO_UNDERRUN_LOG`-style
  instrumentation, watching for `Cannot enlarge memory` / SuspendError / Atomics
  faults under the 2 GB growable-shared heap. This closes the one open risk from
  `02-spike-offmain-jspi-RESULT.md` (§"did NOT do": heaviest load unverified).
- **Success metric:** full song load completes + plays with shared memory ON, memory
  stays under the browser/2 GB ceiling, 0 fatals. If green, B is a fully-cleared
  fallback; if MVP-1 already hits the gate, B stays parked.

---

## Open questions the MVP build must resolve

1. **Resampler phase ownership across the producer/consumer split.** Today the
   44100→ctx linear resampler carries `mResamplePos`/`mResampleCarry*` in the pump
   (main thread). Moving the resample into the worklet means the *phase/carry state
   moves to the audio thread* — clean (it becomes consumer-owned), but confirm no
   main-side code reads it. (Prototype resampled at ctx rate directly; the real path
   must resample per-stem or post-mix on the audio thread.)
2. **Per-stem vs post-mix resample.** Cheaper to mix at 44100 then resample the single
   bus once (matches `MixSources` today) vs resampling each stem. Pick post-mix (one
   resampler instance on the audio thread) — verify it fits the 128-frame quantum
   budget for 6–15 stems (the spike says yes with headroom for pure-JS).
3. **SAB backing vs copy-into-SAB for `mBuffer`.** Backing the ring with a SAB array
   avoids a per-`WriteData` copy but touches the StreamReceiver allocation; copy-into-
   SAB is simpler and isolates the change. Decide by measuring the copy cost vs the
   allocation-refactor blast radius (the ring is ~9 s × N stems = a few MB of SAB).
4. **Limiter envelope continuity.** `mLimiterEnv` is a one-pole state; moving the
   limiter to the worklet moves that state too. Confirm SFX/`SampleInst` sources
   (which also feed the mixer) are published into the SAB set or mixed in a second
   pass — they share the bus (`rb3_sampleinst_native.cpp`).
5. **Output-floor tuning.** Pick the fixed floor (60 vs 80 ms) by the worst observed
   audio-callback jitter in headless Chromium; the spike used the existing low-water
   instrumentation — reuse it to set the floor empirically, not by guess.
6. **SFX latency.** With a low fixed output floor + audio-thread mix, confirm one-shot
   SFX (menu/hit) still fire promptly — the top-up clamp logic that kept SFX from
   queueing behind a full ring (`AudioDevice_Web.cpp:769-774`) must be preserved in
   the new model.

---

## Source-of-truth index (load-bearing cites, all verified in-tree)

- Main-thread pump site: `rb3/src/App.cpp:569`
- PumpAudio / MixSources / SAB write / adaptive law: `engine/src/audio/AudioDevice_Web.cpp:560-888, 499-556, 267-294, 575-766` (RING_FRAMES=32768 @ `:42`, MIX_BUF_FRAMES=8192 @ `:48`)
- Per-channel ~9 s decoded-PCM ring: `src/system/synth/StreamReceiver.h:65-66` (`mBuffer[0xC0000]`)
- SPSC cursor protocol (producer/consumer ownership, racy-benign): `native/src/rb3_stream_receiver_native.cpp:60-72, 290-320`
- Native off-main mix (the reference): `engine/src/audio/AudioDevice.cpp:146,186,367,376`
- Worklet drainer (consumer to be upgraded): `engine/src/platform/web/assets/audio-worklet.js`
- Build flags (JSPI, no -pthread today): `native/CMakeLists.txt:957-961`; `engine/CMakeLists.txt:592-597`

## Wave docs this decision rests on

- `docs/native/audio-thread-2026-06-20/01-map-web.md` — web data-flow map
- `docs/native/audio-thread-2026-06-20/MAP_NATIVE_AUDIO_THREADING.md` — native reference + ring depth
- `docs/native/audio-thread-2026-06-20/00-emscripten-offmain-audio-compat-matrix.md` — option matrix
- `docs/native/audio-thread-2026-06-20/STALL_BENCH_BASELINE.md` — the curve to beat
- `.claude/worktrees/worklet-mix/docs/native/audio-thread-2026-06-20/SPIKE_WORKLET_SIDE_MIX.md` — path A proof (0% through 1500 ms)
- `.claude/worktrees/jspi-audio-spike/docs/native/audio-thread-2026-06-20/02-spike-offmain-jspi-RESULT.md` — path B toolchain VIABLE
- `.claude/worktrees/aloop-capture/docs/native/audio-thread-2026-06-20/aloop-native-realdevice-capture-verify.md` — native real-device MATCH benchmark
