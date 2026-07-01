# Wave 10 — pump + telemetry audit (code read, NO run)

**Probe:** wave-10 #4 (pump+telemetry-audit). Document the exact main-thread-stall →
audio-underrun causal chain on web, and verify whether the underrun TELEMETRY is
trustworthy (so a web-capture probe's `underruns` numbers can be believed).

**Method:** static read of the three hot-path files. No build, no run. Every number
below is derived from the source constants + the measured device rate (ctx = 48000 Hz,
mix = 44100 Hz — confirmed `RESAMPLING 0.9187x` in wave-08/09 logs).

**Files / lines read:**
- `../milo-native-engine/src/audio/AudioDevice_Web.cpp` (PumpAudio, MixSources, EM_JS init/ring/stats)
- `../milo-native-engine/src/platform/web/assets/audio-worklet.js` (the AudioWorkletProcessor)
- `../milo-native-engine/src/audio/AudioDevice.h:86-119` (members)
- `rb3/src/App.cpp:504-594` (RunOneFrame)
- `rb3/native/src/rb3_stream_receiver_native.cpp:243-332` (RB3 song-stream RenderAudio)
- `rb3/scripts/web/audio-stall-measure.mjs` (how a harness reads the counters)

---

## Constants (the load-bearing numbers)

| Constant | Value | Source | At ctx 48000 Hz |
|---|---|---|---|
| `RING_FRAMES` | 32768 frames (stereo) | AudioDevice_Web.cpp:42 | **682.7 ms** total ring |
| `MIX_BUF_FRAMES` | 8192 | AudioDevice_Web.cpp:50 | 170.7 ms / pump chunk cap |
| worklet quantum | 128 frames | audio-worklet.js:56 (`left.length`) | **2.667 ms** |
| underrun report cadence | `sampleRate>>1` = 24000 frames | audio-worklet.js:94 | exactly 0.5 s |
| adaptive START target | `kStartMs` = 120 ms | AudioDevice_Web.cpp:585 | 5760 frames |
| adaptive FLOOR | `RB3_AUDIO_LAT_MIN_MS` default 50 ms | :607 | 2400 frames |
| adaptive CEILING | min(`MAX_MS`=500ms, 80% ring=546ms) = **500 ms** | :608,:614 | 24000 frames |
| grow step | `kGrowMs` = 40 ms / sustained window | :586 | 1920 frames |
| `kResampleCarryMax` | 8 frames | AudioDevice.h:117 | — |

Device rate = 48000 Hz (browser ignores the requested 44100, AudioDevice_Web.cpp:130-139).

---

## (a) The exact main-thread-stall → underrun chain

**Each `App::RunOneFrame(frame)` the main thread does, in order (App.cpp:504-593):**
1. `SystemPoll(false)` — :507
2. `TheUI.Poll()` — :508
3. `TheBandDirector->Poll()` (native venue tick; web: not in this `#ifdef HX_NATIVE` block) — :537
4. `TheMusicLibrary->Poll()` (drives song-preview state machine; native block) — :556
5. `RB3GameInputPoll(frame)` — :560
6. `TheTaskMgr.Poll()` — :561  ← the song clock / task graph (the big one during gameplay)
7. `TheSynth->Poll()` — :562-563  ← **decode/refill of every stem** (StandardStream/VorbisReader fills `mBuffer`)
8. **`AudioDevice::GetInstance().PumpAudio()`** — :569 (web only, `#ifdef __EMSCRIPTEN__`)
9. `TheRnd->BeginDrawing()` / `TheUI.Draw()` / `TheRnd->EndDrawing()` — :572-593 ← **the heaviest item** (full 3D venue + HUD; wave-03b measured splash rAF gaps p50 100 ms, song_select-enter ~150 ms one-shot)

So between two consecutive `PumpAudio` refills the worklet must survive the WHOLE rest
of the frame: Draw (dominant), Poll, decode, plus **any V8 GC pause** that lands anywhere
on this thread. The audio thread (worklet) is pure-JS and only DRAINS the ring; it cannot
refill. This is the architectural single-thread coupling.

**PumpAudio refill math (AudioDevice_Web.cpp:542-789):**
- It queries `freeFrames = js_audio_ring_free_frames()` (:553), computes `queued = (RING_FRAMES-1) - freeFrames` (:671), and tops the ring up to the **adaptive target depth** `targetFrames` (:672-676) — NOT to the full ring. `writable = targetFrames - queued`; if `<=0` it returns without writing (:673).
- So the buffered headroom that protects against a stall is **`targetFrames`**, not `RING_FRAMES`. At the floor that is **50 ms**; at boot/start **120 ms**; under sustained underrun pressure it climbs to **500 ms** (then the worklet has ≥182.7 ms of ring slack left, by the 80% cap).

**The stall threshold:**
- The ring drains at real time (1 frame per 1/48000 s). A main-thread stall of duration **Δt** with the buffer at depth **D ms** leaves `D − Δt` ms in the ring when the next pump runs.
- **Underrun (silence pad) occurs when `Δt > D`** — i.e. a stall longer than the *current adaptive target* fully drains the queue.
  - At the **50 ms floor**, a stall **> 50 ms** underruns. (wave-09 noted real-browser p50 frame ~20 ms with one 85 ms blip — an 85 ms stall at the 50 ms floor WOULD underrun; at the 120 ms start target it would not.)
  - At the **120 ms start target**, a stall **> 120 ms** underruns.
  - At the **500 ms ceiling**, only a stall **> 500 ms** underruns.
- A single render quantum is 2.667 ms, so the worklet re-checks the ring every 2.667 ms; "underrun" is decided per-quantum (audio-worklet.js:65 `toRead = min(frames, available)`).

**Net:** the depth that matters for "does this stall cause a dropout?" is the *adaptive
target at that instant*, which floats 50–500 ms. The full 682.7 ms ring is never the live
guard — the pump deliberately refuses to fill past the target (the wave-09 SFX-latency
fix). Lowering latency (good for SFX) directly lowers the stall tolerance (bad for jitter):
**they trade off on the same knob.**

---

## (b) Is the underrun telemetry trustworthy?  ← DECISIVE

**Verdict: PARTIALLY. The counter is correctly wired and never OVER-counts, but it
materially UNDER-counts — `underruns == 0` does NOT prove "no stall stressed the buffer"
or "no jitter." It only proves "the ring never fully drained below 128 frames (2.67 ms)."**

### What is correct (no over-count, plumbing is sound)
- The worklet increments `underrunEvents`/`underrunFrames` **only when `padded = frames − toRead > 0`** (audio-worklet.js:76-90) — i.e. only when it actually wrote silence. A fully-served quantum never counts. No false positives from normal operation.
- **Idle/preview silence is NOT a false underrun.** When no source plays, `MixSources` still writes *zeros* into the ring (memset at :483, no early-out unless `mSources.empty()` — and even empty just leaves zeros), so the worklet reads valid (zero) frames and `available >= 128` → `padded = 0`. Silence-because-nothing-is-playing is correctly distinguished from silence-because-starved.
- **Boot is not over-counted.** Before the worklet receives its `init` message, `this.data` is null and `process()` returns at audio-worklet.js:49 *without touching the counters*. Counting starts only once the SAB is wired.
- Counters are **cumulative since worklet start**; posted every 0.5 s (audio-worklet.js:91-103) onto `window[key].underruns` (AudioDevice_Web.cpp:166-170). The C side reads the latest snapshot via `js_audio_underrun_stats` into `int[4]` (:236-247). The harness `audio-stall-measure.mjs:82-89` snapshots cumulative values and **computes per-window deltas** (:360-361) — correct interpretation.
- `js_audio_underrun_stats` returns the **latest** snapshot, so it is at most ~0.5 s stale. Acceptable for the 0.5 s reporting granularity. No staleness bug.

### Why it materially UNDER-counts (the trust limit)
1. **Threshold is a TOTAL drain, not a near-miss.** An underrun is counted only when the
   ring has **< 128 frames (< 2.667 ms)** left at the moment a quantum runs
   (audio-worklet.js:65). But the pump keeps the ring topped to **50–500 ms**. A stall that
   drops the buffer from 120 ms to, say, 5 ms — a severe, buffer-stressing near-miss that
   indicates real jitter — produces **zero** counted underruns. The telemetry is blind to
   everything above the 2.67 ms floor. **`underruns=0` is consistent with the buffer
   repeatedly dipping to within a hair of empty.** This is the decisive caveat: the wave-09
   "underruns = 0 over 30 s" result proves the ring never fully emptied; it does **not**
   prove the buffer wasn't being stressed (and thus does not by itself refute audible jitter
   — though wave-09 found the audible problem was HF decimation, a different bug).
2. **The adaptive buffer actively masks underruns by trading latency.** On ≥2 consecutive
   underrun windows the target grows +40 ms/window up to 500 ms (:639-644). So a *sustained*
   stall pattern stops producing counted underruns once the buffer has grown past the stall
   depth — the count goes quiet precisely when the latency cost is highest. A flat
   `underruns` line can mean "fixed" OR "buffer grew to hide it." You must read
   `targetFrames` / the `latency GROW` logs alongside the counter, not the counter alone.
3. **Min-resolution = one 128-frame quantum.** The smallest detectable underrun is 128
   frames = 2.667 ms of silence. Sub-quantum gaps cannot exist (the worklet always outputs
   whole quanta), so that's a hard floor, not a bug — but it means each `underrunEvents++`
   is worth ≥ 2.667 ms of audible silence, and `underrunFrames` (not `underrunEvents`) is
   the better severity metric.
4. **No "low-water-mark" telemetry exists.** There is no counter for "minimum ring depth
   observed" — the single most useful jitter signal. To actually trust "no jitter," the
   probe should ALSO sample `js_audio_ring_free_frames` (→ queued depth) continuously and
   report its *minimum*, because the underrun counter only trips at the very bottom.

**Bottom line for the web-capture probe:** treat `underruns > 0` as *definitely real*
(trustworthy positive — every count is genuine silence). Treat `underruns == 0` as
*necessary but not sufficient* for "no stall jitter" — pair it with (i) the min of
`ring_free_frames` over the run, and (ii) the adaptive `targetFrames` / `latency GROW`
logs. Do not conclude "no stalls" from a zero counter alone.

---

## (c) Per-call allocation in the hot path (heap churn that could itself drive GC)

**WASM/C++ side — essentially allocation-free in steady state:**
- `PumpAudio` allocates **nothing** per call. `sMixBuffer`/`sOutBuffer` are allocated ONCE in `Init` (`new float[MIX_BUF_FRAMES*2]`, :383-384) and reused. The carry buffer `mResampleCarry[16]` is a fixed member array (AudioDevice.h:118).
- `MixSources` resizes `mMixBuffer` (a `std::vector<float>`) only if too small (:491-493). Steady-state chunk size is constant (≤ MIX_BUF_FRAMES) so after the first frame this `resize` is a **no-op — no per-call growth.** No `new`/`malloc` in the mix or limiter loops.
- `mSources.erase(it)` runs only when a source `IsFinished()` (:507-508) — once per song-stem teardown, not per frame.
- RB3 song-stream `RenderAudio` (rb3_stream_receiver_native.cpp:243-332) is **allocation-free**: it reads int16 from the fixed `mBuffer` ring and writes the caller's `output`. No heap traffic.
- ⇒ **The C++ audio hot path does not feed the JS GC.** Any heap growth driving GC is NOT coming from PumpAudio/MixSources/RenderAudio.

**JS side — per-call typed-array VIEW allocation (the real GC pressure, but small + transient):**
Every EM_JS call constructs short-lived JS objects on the V8 heap:
- `js_audio_ring_free_frames` (:220-231): `new Int32Array(sab,0,2)` — 1 view / pump.
- `js_audio_underrun_stats` (:236-247): `UTF8ToString(stateKey)` allocates a JS string / call — called once per pump in the adaptive read (:630) **plus** every `audio_stats` print.
- `js_audio_ring_write` (:249-276): per call constructs `new Int32Array(sab,0,2)` + `new Float32Array(sab,8)` + `HEAPF32.subarray(...)` (a view) + up to two `.subarray` views on wrap. **Called once per chunk** (1–N chunks/pump; typically 1).
- `js_audio_worklet_started` (:215-218) + each EM_JS does `UTF8ToString(stateKey)` → a fresh JS string per call.

Per frame ≈ **4–6 EM_JS calls**, each minting ~1–3 small typed-array views / strings =
on the order of **a few hundred bytes of short-lived V8 garbage per frame** (~tens of KB/s
at 60 fps). These are all young-generation, immediately-dead allocations → collected by
cheap V8 scavenges, **not** a growing retention. This is normal steady churn, **not** a
leak, and not a plausible cause of a *long* main-thread GC pause by itself. It is, however,
non-zero and could be eliminated by caching the `Int32Array`/`Float32Array` views on
`window[key]` once at init (they alias the same SAB every call) and passing the stateKey as
an interned constant — a cheap, safe optimization if the web-capture probe shows young-GC
scavenge frequency correlating with rAF gaps.

**Conclusion (c):** No per-call leak; small constant young-gen churn on the JS glue only.
The hot path is not the GC driver. A GC pause large enough to underrun (>50 ms at the floor)
would have to come from a RETENTION growing elsewhere (the b79cbafa IDB-cache class of bug,
or another unbounded Map/array) promoting to old-gen and triggering a major GC — that's the
web-leak-audit probe's job, not this hot path.

---

## (d) Adaptive buffer under bursty (periodic-GC) vs sustained stalls

Control law: AudioDevice_Web.cpp:557-668. Pressure accumulator `sPressure` (fixed-point /256):
+1.0 (256) per window WITH new underruns (:637), ×0.5 decay per clean window (:647). Grow
fires only at `pressure >= 2.0` = 512 (`kPressureGrow`, :591, :639) = **≥2 *consecutive*
underrun windows**. Floor = 50 ms, ceiling = 500 ms (80%-ring capped), grow +40 ms/window,
shrink = 25% of (target−floor) per clean window, min 10 ms (:649).

- **Floor / ceiling in ms:** floor **50 ms** (2400 frames @48k), start **120 ms**, ceiling
  **500 ms** (24000 frames; hard 80%-ring cap = 546 ms so 500 ms wins). Env: pin fixed with
  `RB3_AUDIO_LATENCY_MS`; bounds via `RB3_AUDIO_LAT_MIN_MS` / `_MAX_MS`.

- **SUSTAINED stall (e.g. a steady heavy-render or repeated major GC every window):** the
  adaptive buffer **works as intended** — pressure builds past 2.0 and the target climbs
  +40 ms/window until it sits above the stall depth, after which underruns stop. Cost =
  added output latency (up to +450 ms over the floor). This is the design case and it
  genuinely helps — at the price of latency.

- **BURSTY / periodic-GC stall (one big GC pause every few seconds, clean windows between):**
  the adaptive buffer is **largely ineffective by design, and that is the relevant failure
  mode for this wave's hypothesis.** Reasons, with numbers:
  1. **Transient rejection swallows the spike.** A *lone* underrun window gives pressure
     1.0 (256), which is `< kPressureGrow` (512) → **zero growth** (:639), then ×0.5 decay
     on the next clean window (:647). So an *isolated* periodic GC pause never grows the
     buffer at all — by intent (it's the wave-09-v2 "don't let one load stutter balloon the
     buffer" rule). The dropout it caused is simply taken.
  2. **For growth, two GC pauses must land in CONSECUTIVE 0.5 s windows.** A GC cadence
     slower than ~2 Hz (one pause every >1 s) tends to land each spike in its own window
     with a clean window between → pressure oscillates 256→128→… and **never reaches 512**.
     The buffer stays at/near the floor and **every** GC pause that exceeds the floor depth
     keeps producing an underrun, indefinitely. The adaptive law cannot catch a slow
     periodic burst; it only catches a *sustained* run.
  3. **Even when it does grow, shrink pulls it back down between bursts.** Each clean window
     shrinks 25% toward the floor (:649). For a GC every ~2–3 s there are ~4–6 clean windows
     between bursts → the target decays most of the way back to 50 ms before the next GC
     hits, so it re-underruns. The buffer chases its own tail.

  ⇒ **For a periodic-GC stall pattern the adaptive buffer mostly just adds latency without
  preventing the dropout** unless the GC is frequent enough (≥ every other 0.5 s window) to
  read as "sustained." A leak that triggers a major GC every few seconds is therefore the
  *worst* case: too rare to grow the buffer, frequent enough to keep clicking. This is fully
  consistent with the user's "jitters slightly / some stalls" (intermittent, not constant)
  report.

**Implication for the fix direction (not implemented — diagnosis only):** the adaptive
buffer is the wrong tool for a periodic-GC root cause; eliminating the leak (so no major GC)
is the real fix. If a residual GC must be tolerated, either (i) raise the *floor*
(`RB3_AUDIO_LAT_MIN_MS`) above the worst GC pause — pure latency-for-safety trade — or (ii)
make pressure grow on a *single* large `underrunFrames` spike (not just consecutive
windows), accepting more eager growth. But the architecturally correct fix remains
off-main-thread mix (STATE.md "OFF-MAIN-THREAD MIX" note) so a main-thread GC can't starve
the ring at all.

---

## Artifacts
- This doc: `rb3/docs/native/audio-perf-loop/wave-10-pump-telemetry-audit.md`
- Source of truth (read, unmodified):
  `../milo-native-engine/src/audio/AudioDevice_Web.cpp`,
  `../milo-native-engine/src/platform/web/assets/audio-worklet.js`,
  `../milo-native-engine/src/audio/AudioDevice.h`,
  `rb3/src/App.cpp:504-594`,
  `rb3/native/src/rb3_stream_receiver_native.cpp:243-332`,
  `rb3/scripts/web/audio-stall-measure.mjs`.

## One-line verdict
The stall→underrun chain is real and the buffered guard is the *adaptive target* (50–500 ms),
not the full 682.7 ms ring; the underrun telemetry is correctly wired and never over-counts,
but it under-counts — it fires only on a TOTAL ring drain (< 2.67 ms left), so `underruns=0`
means "ring never fully emptied," NOT "no jitter / no buffer stress." Trust positives;
corroborate zeros with ring-low-water-mark + the `latency GROW` logs.
