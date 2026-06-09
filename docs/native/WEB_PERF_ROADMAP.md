# Web Port Performance Roadmap

_Written 2026-06-09. Synthesizes docs/native/web-loadperf-findings-2026-06-03.md and
docs/native/audio-perf-loop/ (waves 09–10, STATE.md). Numbers below are measured, not
estimated, unless marked (est)._

> **Implementation handoffs:** every item below was deep-audited and Opus-verified the
> same day — see **[web-perf-handoffs/](web-perf-handoffs/README.md)** for per-item
> implementation specs and for status changes that supersede this file (notably: P2.2
> mesh-cache v2 is already landed+validated+pinned; P4.2 is resolved; the P2.4
> mechanism needed a redesign; P0.3 is largely built in a worktree awaiting merge).

## 1. Where the time goes today (measured baseline)

### Load time — cold boot 13–20 s, native is 5.2 s on the same box

| Phase | Cost | Status |
|---|---|---|
| wasm fetch+compile | 0.5 s warm code-cache / ~7.5 s cold (28 MB `-O0 -g2` dev wasm) | release build is HTTP-cached + code-cached; cold path is the dev/debug pain |
| engine init + GPU adapter | ~0.2 s | fine |
| **`new App()` ctor** | **~12.45 s** | the bottleneck. ~7 s **idle** (JSPI suspends per `ReadAsync` even when bytes are already in MEMFS) + ~5 s CPU (DTA lexer ~0.4 s, DXT decompress ~0.25 s, buffer ctors ~0.7 s, rest spread) |

**Proven GPU-independent** (gpu-boot-probe: RTX 3090 == SwiftShader == 12.3 s).
**Refuted**: shader compile, wasm-split, network waterfall, loader-yield tuning as boot
bottlenecks. The async-I/O model is the gap — native, same code, same GPU, boots in 5.2 s.

### Runtime — web gameplay runs at ~25 fps effective, and that *is* the audio jitter

Wave-10 capture (50 s steady gameplay): rAF p50 = 33.33 ms (exact 2× vsync), p95 = 66.7,
max 133–200 ms; 91 % of frames missed 16.67 ms. `PumpAudio()` runs inside
`App::RunOneFrame` on the single JSPI main thread → a 33–117 ms frame can't refill the
SAB ring → **~22 underrun events/s, 5–9 % of audio frames silence-padded**.

- **Confirmed NOT the cause**: GC (0.8 % of wall, 1.3 % of rAF lost-time), JS/wasm heap
  growth (0 MB/min during steady play), native engine leak (RSS flat ±36 MB band over 200 s).
- **Caveat**: the capture ran with concurrent agents loading the box (18–30 %). The exact
  2×-vsync pattern is ambiguous under contention — quiet-box re-measure is a P0 gate
  before investing in render-perf work.
- Splash screen: full 3D venue (823 meshes / 250k tris) drawn behind the 2D overlay at
  ~10 fps because the frustum cull is disabled (baked Xbox bounding spheres are
  wrong-centered, correct culling removes visible meshes).
- song_select ENTER: ~150 ms one-shot hitch (`UIScreen::LoadPanels` parses all 5 panels'
  milo Includes in one frame).
- Known per-draw waste: RB3's `BandRnd::DrawMesh` recreates vertex/index buffers every
  draw (RAII locals, no per-mesh GPU cache). The mesh-cache experiment
  (`/tmp/eng-meshcache` `a0f98ad`) targets exactly this but **failed its strict A/B gate**
  (multi-draw uniform collapse, darkened song rows — `d03f041e`); v2 in progress.

### Memory — steady-state is clean; transitions leak

- **L1 (real leak)**: `sTexGpu` (`milo-native-engine` `Rnd_Wgpu_RB3.cpp:340`) — GPU-texture
  cache inserted at :469/:1356, cleared only at Shutdown. Grows per screen/song
  transition (hundreds of textures per hub→select→song→results loop). Multi-song-session
  baseline climb; not within-song jitter. Fix: erase on `RndTex` destruction.
- **L2 (telemetry bug)**: worklet counts an underrun only when the ring drops below
  128 frames (<2.67 ms). A 120 ms→5 ms dip counts as zero. Wave-09's "underruns = 0" was
  a false all-clear. Fix: per-window ring low-water-mark.
- All JS-side caches bounded post-`b79cbafa`; native RSS flat.

## 2. Measurement toolbox (what we can diagnose with today)

| Question | Tool | Maturity |
|---|---|---|
| Where did boot time go? | `scripts/web/loadperf-profile.mjs` (CPU profile + longtasks + boot milestones, `--nav` to profile into gameplay) → `analyze-cpuprofile.mjs` (flame aggregation) | polished |
| Is it the GPU? | `scripts/web/gpu-boot-probe.mjs` (adapter identity, real vs SwiftShader A/B) | polished |
| Network-conditioned load cost | `scripts/web/netperf-suite.mjs` (throttle profiles, per-transition ms-blocked + waterfall) | polished |
| Frame gaps / main-thread stalls | `scripts/web/web-stutter-probe.mjs` (per-screen rAF p50/p95/p99) | polished |
| Audio jitter, unified timeline | `scripts/web/audio-jitter-profile.mjs` (rAF + longtasks + heap + GC + underruns on one timeline, the wave-10 instrument) | polished |
| Is the audio *correct*? | `scripts/native/audio_verify.py` + `/audio-verify` skill (identity, rate, clipping) | polished |
| Native frame attribution | `scripts/native/frame_profiler.py`, `loadperf-frametail.py`; `RB3_FRAME_TRACE` JSONL, `RB3_FRAME_INSTRUMENT` | polished |
| Memory growth / OOM | `scripts/web/_songlib-mem.mjs`; heap sampling inside audio-jitter-profile | ad-hoc |
| Regression guard | `scripts/web/smoke-test.mjs`; `_framepin_capture.py` for pixel-identical A/B | polished |
| Harness plumbing | `scripts/web/lib/core.mjs` (nav/console/screenshot), `native/web/server.py` debug API (`/api/health`, `/api/screenshot`, `/api/input`, `/api/dta/eval`, `/api/version`) | mature |

**Known measurement gaps** (the diagnose-better half of this roadmap):
1. Underrun → cause attribution (worklet→`__rb3Trace` wiring missing; can't ask "which
   long frame caused this underrun?").
2. Underrun undercounting (L2 above).
3. Native boot/nav milestones are web-only (`BootMark` lives in `main_web.cpp`).
4. No unified session identity / replay — every tool is a point solution.
   **Session telemetry** (`docs/native/SESSION_TELEMETRY_DESIGN.md`, v1 contract locked,
   M0 not yet implemented) is the designed fix for 1/3/4.

## 3. Roadmap

Ordered by leverage ÷ risk. Each item names its acceptance measurement — nothing lands
without a before/after number from the tools above.

### P0 — Fix the instruments first (days, low risk)

| # | Item | Why first | Accept |
|---|---|---|---|
| P0.1 | **L2: ring low-water-mark telemetry** in the audio worklet (per-window min depth, posted alongside underrun count) | Every audio decision downstream is currently judged by an undercounting metric | audio-jitter-profile shows low-water series; induced 100 ms stall registers as a dip |
| P0.2 | **Quiet-box gameplay re-measure** — rerun `audio-jitter-profile.mjs --play-secs 50` with no concurrent agents/builds | The 25 fps / 2×-vsync finding gates the entire render-perf track; it's ambiguous under the 18–30 % box load it was captured at | clean capture; decide whether P2 is "render is slow" or "render is fine, contention artifact" |
| P0.3 | **Session telemetry M0** (already an active project — frame+input+nav+boot NDJSON, frame tap moved to `App::RunOneFrame` so web+native share it) | Unifies the point tools; gives native boot milestones; prereq for underrun→frame attribution (M2) | one trace per session ingested; native emits boot/nav events |

### P1 — Boot time: 13 s → ~6 s (the single biggest win)

| # | Item | Expected | Accept |
|---|---|---|---|
| P1.1 | **Sync-read fast path in `native_file.cpp`**: when the requested bytes are already resident in MEMFS, satisfy `ReadAsync` synchronously instead of suspending the JSPI stack. Confirm boot files are in the eager `/api/bundle/boot` bundle first. | −~7 s of App-ctor idle (proof: native = 5.2 s with sync I/O, identical code) | loadperf-profile: App ctor ≤ ~5.5 s, idle share collapses |
| P1.2 | **Optimized release wasm**: fix `--opt`/`--closure` in `scripts/web/build.sh` (BROKEN as of W4a), ship release at -O2/-Os with `-g0`. Shrinks the 28 MB binary → faster cold compile (~7.5 s today), faster download, and likely faster runtime frame times (helps P2 for free). | cold compile well under 7.5 s; smaller brotli payload; (est) runtime speedup from -O0→-O2 is potentially large | gpu-boot-probe cold-cache run; smoke-test green; bytes on the wire |
| P1.3 | Boot-CPU micro-passes (DTA lexer 0.4 s, DXT 0.25 s, buffer ctors 0.7 s) — only after P1.1/P1.2, and only if a fresh loadperf-profile still shows them above noise | −0.5–1 s (est) | flame diff |

### P2 — Frame time: 25 fps → 60 fps (root cause of audio jitter) — gated on P0.2

| # | Item | Expected | Accept |
|---|---|---|---|
| P2.1 | **Quiet-box frame profile** (loadperf-profile `--nav` into gameplay) → rank top per-frame costs. Prime suspect: per-draw vbuf/ibuf recreation in `BandRnd::DrawMesh`. | attribution, not a fix | top-5 frame costs named with % |
| P2.2 | **Mesh-cache v2** (persistent per-mesh GPU buffers) — redo of `/tmp/eng-meshcache` `a0f98ad` with the multi-draw uniform-collapse bug fixed; gate with `_framepin_capture.py` strict pixel A/B like v1 | (est) large — it's the obvious per-draw waste | pixel-identical A/B passes this time; rAF p50 moves |
| P2.3 | **Splash/venue frustum cull (A1, spec ready)**: recompute tight local spheres from unpacked verts at upload, `SetSphere`, re-enable world.cam cull. Was blocked on concurrent char-skinning edits in `Rnd_Wgpu_RB3.cpp` — that work committed 2026-06-06 (`acd9c19a`/engine `12455b0`), so **re-check: likely unblocked**. | ~30 % of splash-phase draw cost; fixes the 10 fps splash | splash rAF p50 ≪ 100 ms; no missing geometry vs framepin baseline |
| P2.4 | **song_select prewarm (A2, spec ready)**: parse song_select.milo during main_hub idle behind `RB3_PREWARM_SCREENS`, optional GPU prewarm | 150 ms ENTER hitch → <30 ms | netperf/stutter-probe transition gap |

### P3 — Audio resilience (independent of P2; ship regardless)

| # | Item | Notes |
|---|---|---|
| P3.1 | **Adaptive buffer growth on low-water dips** (wave-10 option A): feed P0.1's low-water-mark into the existing adaptive-latency law (engine `58901477`) so near-misses grow the buffer, not just full drains. Cost = output latency; song clock must account for it (A/V sync). | Accept: underruns/s → ~0 in a wave-10-style capture, even before P2 lands |
| P3.2 | **Off-main-thread mix** (option C) — AudioWorklet/worker-side `MixSources`. Structural fix, but **blocked**: `-pthread`/SHARED_MEMORY conflicts with the JSPI single-thread build. Park until P2+P3.1 results are in; revisit only if 60 fps + adaptive buffer still underruns. | |

### P4 — Memory hygiene (small, do alongside)

| # | Item | Notes |
|---|---|---|
| P4.1 | **L1: `sTexGpu` erase-on-`RndTex`-destruction** in `Rnd_Wgpu_RB3.cpp` | Bounds the multi-song baseline climb. Accept: `_songlib-mem.mjs` across a 3-song loop shows flat GPU-tex count |
| P4.2 | **Link trap**: the working-tree edit to `native/src/rndobj_synth_link_stubs.s` removes the `CleanupGpuMesh` weak alias without defining it anywhere — will break the link if committed as-is. Resolve before any of P2.2 ships. | |

### Sequencing

```
P0.1 ──→ P3.1                      (telemetry → adaptive buffer)
P0.2 ──→ P2.1 ──→ P2.2             (re-measure → attribute → mesh cache v2)
P1.1, P1.2                          (boot track, independent, start now)
P2.3, P2.4, P4.1                    (spec-ready, parallelizable)
P0.3 (session telemetry M0)         (ongoing project, unifies everything)
P3.2                                (only if P2+P3.1 insufficient)
```

**Headline targets**: cold boot 13 s → ~6 s (P1.1+P1.2); gameplay 25 → 60 fps (P2);
audible underruns 22/s → 0 (P3.1, even before P2 lands); flat GPU memory across a
multi-song session (P4.1).
