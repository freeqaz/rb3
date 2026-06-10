# Wave 10 — WEB capture: audio jitter / GC causation test

**Probe:** web-capture (the centerpiece causation test).
**Date:** 2026-06-09.
**Hypothesis under test:** "GC sweep (from heap growth / a memory leak) causes the web audio stalls."
**Verdict: REFUTED.** Jitter is real and reproduces, but it is caused by **steady
per-frame engine/render cost** (the page runs at ~25 fps effective, every frame is a
33-67ms long task), **not** by GC and **not** by a leak. Heap is flat during steady
playback; GC accounts for ~0.8% of wall time and only ~1.3% of the rAF lost-time.

## Method

- New script `scripts/web/audio-jitter-profile.mjs` (reuses navigation/instrumentation
  logic from `audio-stall-measure.mjs` + `_songlib-mem.mjs`). ONE Playwright session per
  run drives boot → splash → main_hub → song_select → **game_screen (real MOGG playback,
  ch=15 rate=44100)** and captures a unified, timestamp-aligned timeline of:
  - (a) rAF inter-frame gaps (every gap + its perf.now timestamp);
  - (b) PerformanceObserver `longtask` (>50ms) entries;
  - (c) JS heap / wasm heap (`HEAPU8.length`) / `__rb3IdbCache` size+bytes, ~every 300ms;
  - (d) per-0.5s underrun stats from `window._rb3Audio.underruns` (stateKey = `_rb3Audio`,
    confirmed from `AudioDevice_Web.cpp:387` `"_" MILO_WEB_AUDIO_NS_STR "Audio"`,
    `MILO_WEB_AUDIO_NS=rb3` from `rb3/native/CMakeLists.txt:820`);
  - (e) **V8 GC events via CDP Tracing** (`Tracing.start` categories `v8`,
    `disabled-by-default-v8.gc`, `devtools.timeline`, `blink.user_timing`; events drained
    via `Tracing.dataCollected`). GC trace ts (µs) aligned to the page perf.now timeline via
    a `blink.user_timing` mark captured at trace-start (offset + a 2nd end-mark for drift;
    measured drift < 0.005 ms/s, i.e. clocks are effectively locked).
- Steady window = 50s of gameplay audio, measured AFTER nav (boot/load excluded from all
  slopes/fractions). Run 3× (`w10-webcap-{1,2,3}`); report median + spread.
- **GC observer-effect bug found and fixed in-flight (decisive):** run 1 enabled
  `disabled-by-default-v8.gc_stats`, which makes V8 run `V8.GC_OBJECT_DUMP_STATISTICS`
  *inside* every MajorGC (60-140ms each) — pure tracing overhead. It inflated the measured
  MajorGC pause ~10-20x (run1 MajorGC p50=85ms; subtracting the nested dump → true net
  3-50ms). Runs 2&3 omit `gc_stats` → realistic GC. **Use runs 2&3 for GC numbers; all 3
  agree on jitter/heap.** This is why the GC numbers must be trusted from 2&3 only.
- Also: GC stats count ONLY the canonical top-level stop-the-world slices `MajorGC`
  (mark-compact) and `MinorGC` (scavenge). Every other `V8.GC_*` slice is a nested
  sub-phase of one of those and would multiply the total.

**Box load at capture (32-core):** loadavg run1 9.7→7.4, run2 7.7→6.4, run3 5.9→5.9
(~18-30% of 32 cores; not saturated). Build = current deploy (`native/web/build/release`,
2026-06-09 06:42). ctx sampleRate = 44100 Hz (no resample), ring = 32768 frames.

## Results (steady 50s of gameplay MOGG playback)

### 1. Does jitter reproduce? YES — and it is severe.
rAF gap distribution, steady region, 3 runs (all consistent):

| run | n | p50 | p95 | p99 | max | gaps>33ms | >50ms | >100ms | eff. FPS |
|---|---|---|---|---|---|---|---|---|---|
| 1 | 1263 | 33.33 | 66.66 | 83.33 | 200.0 | 1183 | 128 | 7 | ~25 |
| 2 | 1305 | 33.33 | 66.66 | 83.33 | 133.3 | 1184 | 126 | 2 | ~25.7 |
| 3 | 1236 | 33.33 | 66.67 | 83.33 | 1599.9* | 1151 | 139 | 3 | ~24.3 |

*run3 max = one isolated 1.6s hitch (a single outlier, not the steady pattern).

**Median p50 = 33.33 ms = exactly 2× the 16.67 ms vsync interval.** Gap histogram (run2):
only **9.3% of frames at ~16ms (60fps)**; **66.7% at 33-50ms (30fps); 22.8% at 50-67ms
(20fps)**. Bimodal at vsync multiples = the engine consistently misses the 16.67ms deadline
and the compositor pushes each frame to the next vsync. ~91% of frames are "dropped" vs 60.

### 2. Underruns — real and material (NOT zero like wave-09).
Steady-region (delta, boot backlog excluded):

| run | Δevents | events/s | Δpadded frames | steady underrun fraction |
|---|---|---|---|---|
| 1 | 1129 | 22.6/s | 138,712 | **6.33%** of audio frames |
| 2 |  879 | 17.5/s | 106,567 | **4.86%** |
| 3 | 1545 | 30.6/s | 191,323 | **8.55%** |

Median ≈ **22 underrun events/s, ~6.3% of audio frames silence-padded during playback.**
(Cumulative fraction reported by the worklet is ~19%, but that is dominated by the boot/load
backlog — the steady in-playback figure is 5-9%.) This is audible jitter; the user is right.

### 3. Heap slope during STEADY region — FLAT. No leak.

| pool | run1 | run2 | run3 |
|---|---|---|---|
| JS heap | 0 MB/min (9.5→9.5 MB) | 0 (9.5→9.5) | 0 (9.5→9.5) |
| wasm heap | 0 MB/min (190.5→190.5 MB) | 0 (190.5→190.5) | 0 (190.5→190.5) |
| `__rb3IdbCache` | 0 MB/min (0 MB, n=0) | 0 (0, n=0) | 0 (0, n=0) |

The IDB warm-cache is **empty during gameplay (0 bytes, 0 entries)** — the `b79cbafa` fix
holds. JS heap pinned at 9.5 MB, wasm heap pinned at 190.5 MB, over the full 50s. **There is
no heap growth during steady playback, so there is no GC pressure source to begin with.**

### 4. THE CRUX — GC vs stalls.
GC events in the steady window (runs 2&3, observer-effect-free):

| run | total GC | major | minor | MajorGC dur p50/max | MinorGC dur p50/max | GC pause total |
|---|---|---|---|---|---|---|
| 2 | 37 | 10 | 27 | 4.07 / 29.33 ms | 11.7 / 16.11 ms | 410 ms = 0.81% of wall |
| 3 | 34 |  9 | 25 | 4.76 / 28.98 ms | 13.24 / 16.37 ms | 403 ms = 0.79% of wall |

- **GC total pause ≈ 0.8% of wall time.** Of that, ~80% is minor (scavenge) at ~12ms each,
  ~20% major at ~4ms each.
- **rAF lost-time (Σ(gap−16.67) for gaps>20ms) = 57-59% of wall.** GC is only **1.3-1.4% of
  that lost time.** GC cannot explain the jitter.
- **Coincidence (tight ±50ms): only ~9-11% of big gaps (>33ms) coincide with a GC event.**
- **Longtask coincidence: 24-30% of wall time is in >50ms longtasks** (n=205-227, durations
  53-117ms). These ARE the cause of the long frames.
- **Underrun-increment attribution** (per 0.5s underrun window):
  - near a longtask ≥33ms: **77-86%**
  - near a GC pause: **38-41%** — BUT of those, ~75-90% are ALSO near a longtask (near-BOTH
    14/18 run2, 18/20 run3), and ~half of all GC pauses fall *inside* a longtask. So the GC
    "coincidence" is mostly GC nested inside an already-long render frame, not GC causing the
    stall. near-NEITHER = 10-15%.

**Conclusion of the crux:** the stall/underrun cause is **long main-thread frames (engine +
render: each RunOneFrame ~33-117ms)**. The main-thread audio pump (`PumpAudio` in
`RunOneFrame`) cannot refill the SAB ring while the frame is busy → the worklet drains it
dry → underrun. This is exactly the architectural failure mode documented in the wave plan
(main-thread-pumped audio), but the trigger is the *steady* per-frame engine cost, not GC and
not a leak.

## Honest caveats
- Captured headless (Chromium + ANGLE/Vulkan) under a ~6-8 box load. A real user's GPU/CPU
  would render frames faster, so the absolute FPS/underrun% here is a worst-ish case. But the
  *structure* is load/GPU-independent: GC is a tiny constant (~0.8% of wall) that only shrinks
  on faster hardware, while the long-frame cost is the dominant term — the conclusion (not GC,
  not leak) holds across the spread, and all 3 runs agree.
- The 0.5s underrun reporting cadence widens the GC-coincidence window; with the tight ±50ms
  test the GC fraction drops to ~10%. Both framings agree GC is not the driver.

## Artifacts
- `scripts/web/audio-jitter-profile.mjs` (the new profiler; CDP GC tracing + unified timeline).
- `docs/native/audio-perf-loop/baselines/w10-webcap-{1,2,3}/{summary,timeline,console}.json`
  - `summary.json`: all decisive numbers. `timeline.json`: raw rAF gaps, longtasks, heap
    samples, underrun snapshots, `gcEvents` (canonical MajorGC/MinorGC) + `gcEventsAll`
    (every nested V8.GC_* slice). `console.json`: last 400 page console lines.
- (run1's GC durations are inflated by the gc_stats observer effect — documented above; its
  jitter/heap numbers are valid.)

## Implication for the next wave (measure-only here, but the direction is clear)
The fix is NOT a leak hunt or GC tuning (negligible). It is one of:
1. **Reduce per-frame main-thread cost** so RunOneFrame fits in 16.67ms (the engine/render is
   the 30fps ceiling) — the real cure, but heavy.
2. **Decouple the audio pump from the render frame** — the OFF-MAIN-THREAD MIX option from
   STATE.md WAVE-09 (move MixSources into the AudioWorklet or a wasm-worker pump), which needs
   `-pthread`/SHARED_MEMORY (currently blocked by the JSPI single-thread build). This is the
   only thing that makes audio immune to long render frames.
3. **Grow the adaptive output-latency buffer** (engine 58901477) further to ride the ~57%
   lost-time — a mitigation that trades latency for fewer underruns; the buffer already grew
   to 90ms during boot in every run, which is below the per-frame stall depth.
