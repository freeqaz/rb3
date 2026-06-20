# Web main-thread frame-stall PROFILING — methodology + prior-art survey

**Date:** 2026-06-20
**Engine pin:** `884ab17` (rb3 `native/CMakeLists.txt:74`)
**Role:** the *survey / methodology* agent in the frame-stall workflow. This doc is the
foundation the other two profiler agents + the prototype agent build on. It establishes
**how to profile a frame stall on this build trustworthily**, summarizes what is **already
known** (so nobody re-walks boot/load perf), and delivers a **do-not-repeat list**.

The mission: the audio under-runs ("clicks") are caused by main-thread longtasks emptying the
SAB audio ring (`PumpAudio` runs once per rAF on the single-threaded JSPI wasm build). A
parallel workflow makes audio *survive* stalls (off-main-thread producer). **This workflow
attacks the other end: stop the engine from stalling.** If the engine doesn't stall, the
producer never falls behind.

---

## TL;DR for the next agent

1. **DO NOT trust the CDP sampling CPU profiler for function-level attribution on this build.**
   It is JSPI-blind: **73% of all gameplay samples land on a childless `(program)` leaf** —
   V8 cannot unwind the asyncify/JSPI-suspended wasm stack. Measured: 163 549 / 225 124
   samples on `(program)` with **zero children** (proof in §2). The named functions it *does*
   resolve are only the ~27% sampled at a JS-glue boundary or a non-suspended instant. Use it
   only for the *coarse* category split and the steady-state JS-glue hotspots — never to
   attribute a song-start stall's internals.
2. **The trustworthy attribution comes from TWO JSPI-immune sources, used together:**
   - **`RB3_FRAME_TRACE` in-engine per-frame bucket tracer** (already built,
     `native/src/rb3_frame_trace.cpp`) — for **load/song-start stalls** (it buckets each
     frame's ms into loader-poll / sync-drain / per-object load / texture / mesh / unpack /
     pipeline / inflate). Runs in BOTH native and web (shared `src/App.cpp`).
   - **`perf record` on the NATIVE build** — for the **steady per-frame cost** that the
     bucket tracer lumps into "residue (draw/poll/gpu)". Native uses a real CPU sampler with
     no asyncify blindspot. One web `RunOneFrame` does the *same CPU work* as one native
     `RunOneFrame` (minus the JSPI I/O suspension), so native self-time ranks the shared
     engine hotspots faithfully.
3. **The two regimes are already root-caused** (this survey + the audio diagnosis agree 1:1):
   - **SONG-START burst (≈85% of all under-run frames):** synchronous loader drain
     (`PollUntil*`) + per-object char/venue asset load (`objMs`) + texture decode/upload
     (`texMs`), all **multiplied by the JSPI async-read suspension** on web. New, attributed.
   - **STEADY gameplay spikes:** `BandRnd::DrawMesh` (**#1, 18% native self-time / 22% of
     resolved web busy**) + per-draw RTTI `__dynamic_cast` (~4% native / ~22% resolved web) +
     `DirtyCache::SetDirty` churn (~2-7%) + `KeylessHash`/`ObjectDir` lookups. All in the
     **shared engine renderer** → a fix helps web AND native FPS.

---

## 1. What's ALREADY KNOWN (read these, don't re-measure) — the do-not-repeat list

### 1a. BOOT / first-screen load perf — SOLVED & DOCUMENTED. Do not re-profile boot.
`docs/native/web-loadperf-findings-2026-06-03.md` is authoritative. Established hard:
- Boot ≈ 13-20 s; the steady bottleneck is **`new App()` ≈ 12.45 s**, of which **~7 s is the
  wasm SUSPENDED (idle)** — the web **async-file-I/O / JSPI** round-trip per boot read.
- It is **GPU-independent** (real RTX 3090 == SwiftShader, both 12.3 s — `gpu-boot-probe.mjs`).
- WASM size is NOT the App-ctor bottleneck (28 MB → 3.2 MB brotli, ~42 ms on localhost). Size
  *does* drive the bimodal cold wasm-compile (~7.5 s cold vs ~0.5 s code-cache hit).
- Loader-yield tuning is neutral for total boot time.
- App-ctor CPU work (~5 s) = DTA lexer + DXT decompress + `vector<uint8>` ctors.

**DO NOT re-run boot CPU profiles or A/B loader-yield.** Boot is a separate axis from the
*per-frame gameplay* stalls this workflow targets.

### 1b. Incremental-load-perf waves 0-5 — SHIPPED. Don't re-implement these.
`docs/native/incremental-load-perf/PLAN.md` + MEMORY `incremental-load-perf`. Already landed:
async File seam, Range moggs, BC textures, prefetch, **screen bundles**, **prewarm**,
pipeline-prewarm, read-ahead, byte reduction (181→115 MB wire), SFX PCM→ogg. First-frame is
at-baseline; L2/L3 deferred. These attacked the *download/bytes* and *boot* surface, NOT the
per-frame render cost.

### 1c. Audio under-run consumer side — DIAGNOSED TODAY. The complement to this workflow.
`docs/native/audio-underrun-2026-06-20/diagnose-rootcause.md` +
`docs/native/audio-thread-2026-06-20/STALL_BENCH_BASELINE.md`. Established:
- Web under-run = **4.05% of output frames silence-padded** over 45 s; **1:1 correlation**
  between each main-thread longtask and a proportional under-run burst (their correlation
  table is the canonical stall list — reproduced + extended below).
- `audio-worklet.js:88-92` zero-pads on empty ring = the click; the adaptive-latency law
  (`AudioDevice_Web.cpp:603-745`) oscillates and never converges.
- **Stall-resilience bench** (`scripts/web/audio-stall-bench.mjs`): the current architecture
  survives 0-400 ms *injected* stalls only by riding the adaptive law to its 500 ms latency
  ceiling; an **≥800 ms** stall → 38% dropout. That bench injects *synthetic* stalls to test
  the consumer; **this workflow measures the REAL stalls and removes them at the source.**

**Division of labor:** the audio workflow conceals/survives stalls (worklet hold-last ramp,
deeper buffer, off-main-thread producer). THIS workflow reduces the stalls. They are
complementary; don't duplicate the consumer-side fixes here.

---

## 2. THE METHODOLOGY — "how to profile a frame stall here"

### 2.0 The cardinal fact: the JSPI/asyncify CPU-profiler blindspot (PROVEN)

The web build is single-threaded `-sJSPI` (`native/CMakeLists.txt:959`). The CDP `Profiler`
(and equally CDP Tracing's `disabled-by-default-v8.cpu_profiler`, same sampler) **cannot
unwind a wasm stack that is suspended/resumed by JSPI**. Measured on a 45 s gameplay profile
(`/tmp/rb3-frame-stall/profile.cpuprofile`, 225 124 samples @200 µs):

```
(program) node: hitCount=163549  children=0      ← 73% of ALL samples, opaque leaf
samples on (program): 163550 / 225124            ← V8 sees "in wasm" but no stack
wasm-named nodes: 140    js/url nodes: 28154
```

So every per-stall slice is dominated by `(program)` (e.g. the 186 ms song-start longtask
sampled **99.6 ms as `(program)`**, only ~30 ms resolved to names). **The CPU profiler tells
you a stall happened and which JS-glue/boundary functions ran, but NOT which C++ function
spent the time inside the wasm.** This is THE reason to use the two JSPI-immune tools below.

> Corollary: the loadperf doc's `analyze-cpuprofile.mjs` numbers for *boot* are similarly
> partial in the wasm-busy regions; they were directionally right because boot is I/O-idle
> (78% idle) so the resolved minority is representative. Per-frame gameplay is CPU-busy, so
> the blindspot bites much harder here.

### 2.1 Tool A — `RB3_FRAME_TRACE` in-engine bucket tracer (for LOAD / song-start stalls)

Already built: `native/src/rb3_frame_trace.cpp`, wired in `src/App.cpp:764-812`, web-mapped
via `?frameTrace=` → `RB3_FRAME_TRACE` env. Records EVERY frame as one JSONL object with
sub-frame attribution buckets compiled INTO the wasm (so JSPI-immune):

```
dt  whole-RunOneFrame ms        lp   LoadMgr.Poll (bg loader)   lpu  PollUntil* SYNC drain
objMs per-object PreLoad+PostLoad   objWNm slowest object this frame
texMs texture decode+upload      meshMs VB/IB write   unpackMs CPU vertex unpack
pipeMs pipeline compile          inflMs ChunkStream inflate     primeMs Vorbis prime
st  streams opened   ld loaders added   pend backlog depth
```

**Run it (native, fast):**
```bash
python3 scripts/native/frame_profiler.py --into-song --run-secs 40 \
        --trace /tmp/ft.jsonl --worst 25
# then analyze the rich buckets (the built-in report only shows dt/lp/lpu):
#   sum buckets over long frames; the worst-frame breakdown names the slowObj.
```
The `frame_profiler.py` summary surfaces dt/lp/lpu + spike clustering; for the per-bucket
attribution (texMs/objMs/etc.) parse the JSONL directly (see §3 numbers — I did this inline).

**Run it on web** (to confirm the JSPI multiplier on the SAME buckets): load with
`?frameTrace=…` is native-path only; on web the trace writes via the same App.cpp path but the
file lands in MEMFS — easier to read the buckets from the `RB3_FRAME_INSTRUMENT` LONG log in
the console, or extend the recorder to `postMessage` the JSONL. (Left for the prototype agent
if a web-side bucket confirm is wanted; native + the CPU-profile correlation already prove the
mechanism.)

### 2.2 Tool B — `perf record` on the NATIVE build (for STEADY per-frame cost)

The bucket tracer lumps the steady render/poll cost into "residue (draw/poll/gpu)" (97.7% of
all native frame time — §3). To attribute THAT, profile the native binary with `perf` (real
unwinder, no JSPI blindspot). `perf_event_paranoid=2` here allows user-space `perf record -p`.

```bash
# boot rb3-native to gameplay over the HTTP API, get its PID, then:
perf record -g -F 999 -o /tmp/rb3-perf.data -p <PID> -- sleep 12
perf report -i /tmp/rb3-perf.data --stdio --no-children --sort=symbol   # flat self-time
```
A reusable driver is at `/tmp/perf_gameplay.py` (boots, navs, attaches perf). NOTE: the nav in
that scratch script is brittle (reached song_select, not gameplay, on one run) — prefer
driving via `scripts/native/frame_profiler.py`'s proven `--into-song` nav, or add a
`game_screen` wait-gate before the `perf record` window. Self-time ranking is valid either way
for the shared render hotspots (DrawMesh dominates even at song_select).

### 2.3 Tool C — the CDP attribution harness I built (for the COARSE picture + correlation)

`scripts/web/frame-stall-attribute.mjs` (NEW, mine). Drives boot→game_screen, runs a CDP
`Profiler` for the whole gameplay window, captures PerformanceObserver `longtask` + per-rAF
ts on the same timeline, reconciles clocks, and for the worst N longtasks slices the profile
samples inside `[start,start+dur]` and aggregates self-time. **Use it for:** the under-run
rate, the longtask timeline, the steady JS-glue hotspots, and a loadable
`profile.cpuprofile`. **Do NOT use its per-stall `top` lists as ground truth** — they are
`(program)`-dominated (the blindspot). It is the right tool to *prove a fix moves the
under-run rate* end-to-end, and to see GPU-glue cost (`writeBuffer`/`setBindGroup`).

```bash
node scripts/web/frame-stall-attribute.mjs --port 8421 --play-secs 45 --debug-build \
     --out /tmp/rb3-frame-stall --top-stalls 12
```
Use `--debug-build` (`?debug=true`): the `-g2` debug wasm carries demangled C++ names (the
`-O2` release build inlines+strips them, so even the resolved minority loses names). Caveat:
debug is `-O0` so absolute self-times are inflated and the debug console `log` spam shows up
(~12% — a measurement artifact, ignore it; release has no such log).

### 2.4 Clock reconciliation (so longtask ↔ CPU-sample alignment is real)

- Page side: `longtask.startTime` and rAF ts are `performance.now()` ms. Capture
  `Date.now() - performance.now()` once at init → wall ms.
- CDP side: `Profiler.stop().profile` gives `startTime`/`timeDeltas` in **monotonic µs**;
  anchor `profile.startTime` to the wall `Date.now()` captured around the `Profiler.start`
  call. My harness does both (`cdpToWall` / `ltToWall`). This is needed because the per-stall
  slice intersects two different clocks.

---

## 3. THE MEASURED ATTRIBUTION (empirical, this survey)

### 3a. Native bucket tracer — 8089 frames, 40 s, boot→gameplay
```
ALL FRAMES            n=8089  dt=52623ms   attributed=2.3%  residue(draw/poll/gpu)=97.7%
GAME_SCREEN ALL       n=3766  dt=33030ms   attributed=1.5%  residue=98.5%
GAME_SCREEN LONG>16ms n=36    dt=870ms     attributed=54.9% residue=45.1%
   lp(bgLoad)=234ms(27%)  objMs=164ms(19%)  lpu(syncDrain)=53ms(6%)  texMs=21ms(2%)
WORST game frame: f4323 dt=102ms = lpu53 + objMs39 + texMs19 (+mesh4+unpack2)
   → a CHAR/VENUE LOAD BURST at gameplay start (f4323-4346): CharClip anim loads,
     venue meshes (jaguar03/ludvista/shell_3band), wall_*_norm.tex textures.
```
**Reading:** native gameplay is *mostly clean* (game_screen p99 = 15.9 ms). The spikes are
**asset load at song-start** — sync drain + per-object char/venue load + texture upload. On
web each of those file reads suspends the wasm stack (JSPI), so the same 71 ms native burst
becomes the 381/201 ms web longtasks the audio agent measured.

### 3b. Native `perf` flat self-time — steady gameplay rendering (the 98% residue, resolved)
```
18.25%  BandRnd::DrawMesh(RndMesh*)              ← #1, the steady per-frame render cost
 2.32%  __vmi_class_type_info::__do_dyncast       ┐
 0.86%  __dynamic_cast                            ├ per-draw RTTI dynamic_cast (~4% total)
 0.86%  __si_class_type_info::__do_dyncast        ┘
 1.85%  vector<CharBones::Bone>::size()           ← char skinning per-frame
 1.41%  getenv                                     ← hot getenv in a loop (cheap to cache!)
 1.13%  DirtyCache::SetDirty_Force() + SetDirty() ← dirty-flag churn (~2.3%)
 1.11%  RndTransformable::WorldXfm()              ← transform recompute
 ~3%    dawn::* (RefCount/BufferBarrier/Transition) ← GPU driver-side (Vulkan), not eliminable
 0.51%  CharBones::FindOffset(Symbol) / Symbol::operator== / HashString  ← bone lookups
```

### 3c. Web CDP profile — resolved-only steady busy (the ~27% the sampler caught)
```
22.2%  BandRnd::DrawMesh           ← matches native #1
~22%   __dynamic_cast + RTTI search_*_dst functions  ← matches native RTTI cost
11.9%  log @?debug=true            ← DEBUG-BUILD ARTIFACT (console spam; release-free)
 7.8%  writeBuffer / 2.5% setBindGroup ← WebGPU upload glue (per-draw)
 6.8%  DirtyCache::SetDirty(_Force)
 5.4%  KeylessHash<…ObjectDir::Entry>::FirstFrom/Find ← ObjectDir name lookups in the draw walk
 2.8%  WebAssetsFetchDone          ← the JSPI async-read completion callback (web I/O tax)
```
**The two independent tools AGREE:** steady cost = `DrawMesh` + per-draw `dynamic_cast` +
`DirtyCache::SetDirty` + `ObjectDir`/`KeylessHash` lookups, all in the shared engine renderer.

---

## 4. RANKED stall attribution (cause → magnitude → when → ELIMINABLE?)

| # | cause | magnitude (web) | when | eliminable? how |
|---|---|---|---|---|
| 1 | **Sync loader drain `PollUntil*` + per-object char/venue load + texture upload, ×JSPI** | 381 ms + 201 ms (≈85% of all under-run frames) | song-start / first gameplay seconds (t≈0.8-7 s) | **YES.** Native is 71 ms; web tax is the JSPI per-read suspension. (a) make the song-start loads async/streamed off the first frames (defer char-clip + venue mesh + wall-tex load past the first audio-critical frames), (b) prime the ring before the worklet drains (audio agent's fix C), (c) the loadperf "sync read when bytes already in MEMFS" lever (doc 02-boot-sync-read) applied to the gameplay load path. Prototype agent should target this — biggest single win. |
| 2 | **`BandRnd::DrawMesh` steady self-time** | #1 self-time, ~18% native / ~22% resolved web; recurs every frame | steady gameplay (the 114-154 ms mid-play spikes when draw count peaks) | **PARTIALLY.** It's the real render workload (can't delete draws), but the per-draw overhead is reducible: the L1 vertex-unpack cache already exists (`unpackMs≈0` steady). Next levers: cut per-draw `dynamic_cast` (#3), cache the per-draw uniform/bind-group writes (`writeBuffer`/`setBindGroup` churn), frustum-cull earlier (a cull wave shipped — `web-perf-handoffs/05-frustum-cull.md`). |
| 3 | **Per-draw RTTI `__dynamic_cast`** (in the Draw walk + DrawMesh type checks) | ~4% native / ~22% of *resolved* web busy | every frame, scales with drawable count | **YES, high-ROI + match-neutral on native.** Replace hot `dynamic_cast<T*>` in the draw traversal / `ObjDirItr<T>::Advance` with the engine's existing `Hmx::Object` class-id / `DynamicCast` vtable check (no libcxxabi `__do_dyncast` tree walk). Native-only path or engine-side; classic eliminable RTTI hotspot. |
| 4 | **`DirtyCache::SetDirty` / `SetDirty_Force` churn + `RndTransformable::WorldXfm`** | ~2-7% | every frame (transform/dirty propagation) | **PARTIALLY.** Dirty-flag storms from redundant SetDirty; investigate whether the per-frame char/transform updates over-dirty. Lower priority than 1-3. |
| 5 | **`ObjectDir` `KeylessHash::Find/FirstFrom` name lookups** | ~5% resolved web | every frame (draw walk resolves entries by name) | **MAYBE.** Cache resolved entry pointers instead of by-name lookups in the per-frame path. |
| 6 | **GC pauses** | 2/3 of under-run bumps coincide with a GC (audio diag §2c); CDP shows gc≈0.2% self-time | steady | **PARTIALLY** — the wasm heap GC is JS-engine driven; reducing per-frame JS-glue allocations (the `writeBuffer`/typed-array churn) lowers GC pressure. The 0.2% direct cost is small; the *correlation* is because GC lands on an already-tight frame. |
| 7 | `getenv` in a hot loop | 1.4% native | steady | **YES, trivial** — cache the env lookups (several `getenv` calls are per-frame; the frame-trace/instrument gates already cache theirs — find the un-cached ones). Tiny but free. |

---

## 5. ALREADY-UNDERSTOOD vs GENUINELY-NEW (the flag the brief asked for)

**Already understood / addressed (do not re-investigate):**
- Boot App-ctor async-I/O (§1a) — the song-start stall is the SAME mechanism (JSPI per-read
  suspension) applied to the gameplay load path. The *root cause class* is known; what's new
  is its expression as the song-start *frame* stall.
- The under-run consumer mechanism + the stall-resilience curve (§1c) — owned by the audio
  workflow. Don't touch `audio-worklet.js` / the adaptive law here.
- Load bytes / prefetch / screen-bundle / prewarm (§1b) — shipped.
- Frustum-cull, mesh-cache, texGPU-leak — shipped (`web-perf-handoffs/`).

**Genuinely NEW from this survey:**
- The **JSPI CPU-profiler blindspot quantified** (73% `(program)`, childless) — and the
  resulting **two-tool methodology** (frame-trace buckets + native `perf`) that sidesteps it.
- **Song-start stall internals attributed** to the char/venue load burst (CharClip + venue
  mesh + wall-tex) via the bucket tracer — not just "asset decode".
- **Steady-gameplay stall attributed** to `DrawMesh` + per-draw `dynamic_cast` RTTI +
  `DirtyCache`/`ObjectDir` lookups, **cross-validated by native `perf` AND the resolved web
  samples agreeing** — these are shared-engine, so a fix helps web AND native FPS (the brief's
  explicit goal).
- The **per-draw `dynamic_cast` RTTI** (#3) as the highest-ROI *steady-state* eliminable
  target, distinct from the song-start load target (#1).

---

## 6. Recommended division for the rest of the workflow

- **Profiler agent 2 (song-start):** drive the `RB3_FRAME_TRACE` bucket tracer on web (extend
  the recorder to postMessage the JSONL, or read the LONG log) to confirm the JSPI multiplier
  per bucket; quantify how many frames the char/venue load burst spans and whether deferring
  it past the first ~30 audio-critical frames is feasible. Target stall #1.
- **Profiler agent 3 (steady):** deepen the native `perf` profile *inside* `DrawMesh` and the
  Draw walk; locate the exact `dynamic_cast` call sites (likely `ObjDirItr<T>::Advance` + the
  per-mesh material/skin type checks) and the redundant `DirtyCache::SetDirty`. Target #2/#3/#4.
- **Prototype agent:** stall #1 (song-start load deferral / async) is the biggest single win
  and the cleanest to prototype + prove with `frame-stall-attribute.mjs`'s under-run rate.
  Stall #3 (RTTI elimination) is the cleanest *steady* win and is match-neutral on the native
  engine path. Build in a `framestall-<unique>` worktree; do not bump the pin.

## Artifacts
- `scripts/web/frame-stall-attribute.mjs` — NEW CDP attribution + under-run harness (mine).
- `/tmp/rb3-frame-stall/profile.cpuprofile` + `attribution.json` — the web profile (load in
  speedscope; remember the `(program)` blindspot).
- `/tmp/rb3-native-frametrace.jsonl` — 8089-frame native bucket trace (the ground truth for
  load stalls).
- `/tmp/rb3-perf.data` — native `perf` steady-gameplay self-time.
- `/tmp/perf_gameplay.py` — scratch native perf driver (nav is brittle; gate on game_screen).
