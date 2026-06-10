# Wave 10 — audio jitter / buffer stalls during playback (hypothesis: memory leak → GC sweep → underrun)

**User report (2026-06-09):** web song audio jitters slightly during playback; "some
stalls in the audio buffer." Hypothesis offered: a memory leak triggers a GC sweep that
pauses the main thread long enough to starve the audio ring. Wants the web port profiled,
notes in docs, native build also checked for leaks. Orchestrator stays zoomed out.

## Why this hypothesis is architecturally plausible (pre-wave facts)
- **Web audio is MAIN-THREAD pumped.** `AudioDevice::PumpAudio()` runs from
  `App::RunOneFrame` (single-threaded JSPI build, NO `-pthread`/SHARED_MEMORY — confirmed
  native/CMakeLists.txt). A pure-JS AudioWorklet only *drains* the SAB ring. So **any**
  main-thread pause — V8 GC, a long render frame, a milo parse — directly stops refills →
  the worklet runs the ring dry → audible underrun/jitter. The buffer exists *because* of
  exactly this.
- **There was a real unbounded leak, just fixed (`b79cbafa`).** `__rb3CachePut` retained a
  full second copy of every fetched asset in `window.__rb3IdbCache` (~187MB / 475 entries
  by song_select). Fixed. Question: **are there OTHER leaks** (JS glue or shared engine)
  still growing the heap during steady playback?
- **Underrun telemetry already exists.** Worklet posts `underrun-stats` every 0.5s →
  `window[<stateKey>].underruns = {underrunEvents, underrunFrames, ...}`; C side reads via
  `js_audio_underrun_stats(int*4)` → `rb3AudioStats()`. We can poll it from CDP.
- **Adaptive output-latency buffer exists (engine 58901477, pin d2e5d86c).** Pressure
  accumulator grows ring depth on ≥2 consecutive underrun windows, decays on clean windows,
  capped ≤80% ring. Mitigation, not a fix for the root cause — and growth = added latency.
- **Prior wave-09 measured underruns=0** on the user's *earlier* (HF-static) capture. That
  symptom was a different bug (decimation-by-2, fixed `08ec1442`). The jitter may now be
  visible because the static that masked it is gone — or it may be a fresh/intermittent
  thing. **The wave must honestly report whether jitter even reproduces.**

## Existing tooling to REUSE (do not reinvent)
- `scripts/web/audio-stall-measure.mjs` — rAF gaps + longtask + per-0.5s underrun + ring
  health, drives boot→gameplay. **The stall quantifier.**
- `scripts/web/_songlib-mem.mjs` — JS heap / wasm heap (HEAPU8.length) / idbCache size +
  bytes per 400ms (boot→song_select). **The heap-over-time probe** — needs extending THROUGH
  a full song.
- `scripts/web/lib/core.mjs` — Playwright launch/nav harness. `native/web/server.py` serves.
- Native capture: `MILO_AUDIO=1 MILO_AUDIO_BACKEND=null` + `RB3_HTTP=1`; `/proc/self/statm`
  / getrusage for RSS-over-time.

## Wave-10 probes (DIAGNOSE layer — measure-only, no fixes yet)
1. **web-capture (ONE comprehensive browser run):** single instrumented playthrough to
   sustained song audio capturing in one timeline — rAF gaps, longtasks, JS+wasm heap +
   idbCache over time, per-0.5s underruns, AND **CDP Tracing v8.gc GC events** (the missing
   causation signal). Output: does jitter reproduce? underruns/sec? heap slope during steady
   playback? are GC pauses time-correlated with rAF gaps / underruns?
2. **native-mem (control + engine-leak finder):** native 60s+ steady playback, sample RSS /
   heap over time (and a valgrind/massif or heaptrack pass if cheap). Native has NO JS GC —
   if it ALSO leaks/stalls, the leak is shared-engine; if clean, the issue is web/JS-glue.
3. **web-leak-audit (code read, no run):** audit JS glue + engine web path for remaining
   unbounded retentions after `b79cbafa` — Maps/arrays/event-listeners/closures, retained
   ArrayBuffers, per-frame allocations in the hot path. Rank suspects.
4. **pump+telemetry-audit (code read, no run):** document the exact main-thread-stall →
   underrun chain; verify the underrun telemetry is actually wired + trustworthy (is the
   worklet really detecting/reporting underruns, or silent?); characterize the adaptive
   buffer's behavior + failure modes; note per-frame allocation in PumpAudio/MixSources.

Each probe: read STATE.md first; append a `### <label>` section here with raw numbers;
return a structured verdict. Decisive "confirmed" claims get adversarially verified.

---
## VERIFICATION (orchestrator, 2026-06-10) — jitter FIXED end-to-end ✅

A parallel session consumed this diagnosis and shipped the fixes (mesh-cache v2 + `-O2` release
→ render fps; low-water telemetry + edge-triggered adaptive buffer → audio resilience; CleanupGpuTex
→ leak). I independently re-measured the deployed `-O2` release on a QUIET box (load 2.2) ×2:
**60fps** (rAF p50/p95/p99 = 16.67ms; 0 gaps >50ms), **0 underruns/s**, ring low-water ~33ms,
**latency low (~33–62ms steady)**, heap flat, gameplay renders coherently with no crash. The 25fps
→ 60fps + 0 underruns confirms the root cause (render starving the main-thread pump) is resolved.
Detail in STATE.md (WAVE 10 — VERIFICATION block); baselines `baselines/w10verify-rel-{1,2}/`.

---
## SYNTHESIS (orchestrator, 2026-06-09)

**The user's hypothesis (leak → GC → underrun) is REFUTED, adversarially verified.** Detail in
`wave-10-web-capture.md`, `wave-10-native-mem.md`, `wave-10-web-leak-audit.md`,
`wave-10-pump-telemetry-audit.md`; canonical summary in `STATE.md` (WAVE 10 block).

- **No leak during playback / GC negligible.** Web heap flat (JS/wasm/idbCache 0 MB/min), native
  RSS flat (net −10/−12 MB over 120/200s), GC = 0.8% of wall. A tracing observer-effect
  (`v8.gc_stats`) initially faked 85ms MajorGCs — caught and removed; real GC is ~4-5ms p50.
- **Real cause = main-thread audio-pump starvation.** Web gameplay runs ~25fps (rAF p50 33.33ms);
  `PumpAudio` runs inside `RunOneFrame` on the single JSPI main thread, so a 33-117ms frame can't
  refill the SAB ring → underrun. Underruns track longtasks (77-86%), not GC (38-41%, mostly
  nested inside a longtask).
- **Two real but UNRELATED bugs:** (L1) `sTexGpu` GPU-texture leak (per-transition baseline climb,
  not within-song jitter); (L2) underrun telemetry undercounts (only flags ring <2.67ms while the
  buffer target is 50-500ms) → "underruns=0" is misleading + the adaptive buffer barely engages.
- **Fix fork (see STATE):** (A) buffer resilience [low-risk, +latency], (B) render perf [bigger,
  must re-confirm fps on a quiet box — capture had concurrent GPU load], (C) off-thread mix [risky].

---
## Findings (per-probe detail in wave-10-<key>.md; raw verdicts in the wave-10 task result)
