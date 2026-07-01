# Web main-thread frame-stall — STEADY-STATE jitter attribution (2026-06-20)

Empirical CPU-profile + per-frame-trace + tick-split measurement of the RB3 web
build's main-thread frame timing during gameplay, to attribute the stalls that
empty the SAB audio ring and cause under-runs ("clicks"). Companion to the audio
under-run effort (which makes the *producer* survive stalls); this workflow asks
whether the *engine* can be made not to stall.

**Platform:** single-threaded JSPI wasm (`-sJSPI`, `native/CMakeLists.txt:959`),
DEBUG build (`-O0 -g2`, the only build whose wasm carries C++ function names).
The JS frame loop is `requestAnimationFrame(async ()=> { await
Module._rb3MainLoopTick(); requestAnimationFrame(tick); })` (`main_web.cpp:983`).

## TL;DR — the premise was partly a measurement artifact; the real stalls are load-in, not steady per-frame

1. **Steady gameplay is NOT over budget.** When measured *without* a CPU profiler
   running, gameplay holds **~60 fps** with engine tick **p50 = 8.7–9.2 ms, p99
   = 15.6–18 ms, max ≈ 30–35 ms** (`RunOneFrame` wall time). The mission's
   "~50 ms every gameplay frame" was a **CDP-CPU-Profiler observer effect**: at
   1 kHz sampling the profiler inflates `-O0` wasm frame time ~5–6× (8.7 ms →
   ~55 ms). Two independent clean signals agree: the JS `await
   Module._rb3MainLoopTick()` duration (tick-probe) AND the wasm-side
   `RB3_FRAME_TRACE` `dt` (no profiler) both show p50 ≈ 9 ms, 60 fps.

2. **The real steady-state stalls are sporadic ASSET-STREAMING longtasks**, not
   per-frame compute. Over a 50 s gameplay window the passive
   `PerformanceObserver('longtask')` saw **13 longtasks (sum 1245 ms, max
   194–219 ms)** — and **11 of 13 land in the first ~8 s** (the gameplay
   load-in / "song-start burst"). After t≈8 s, true steady state has only
   ~2 sporadic asset-stream longtasks per ~40 s.

3. **The single worst stall is a synchronous texture drain:** one frame at
   **dt = 612 ms**, fully attributed by the frame-trace to **`lpu` = 530 ms
   (`LoadMgr::PollUntilLoaded`) + obj = 160 ms (`RndTex:floor_wood02_NORM.tex`) +
   tex = 87 ms**. Cause = `RndTex::SetBitmap(FileLoader*)` →
   `PollUntilLoaded(fl,0)` (`src/system/rndobj/Tex.cpp:181`): a venue normal-map
   lazily loaded **during** gameplay, draining synchronously. For its whole
   duration the `await rb3MainLoopTick()` stays pending → **`PumpAudio` is never
   called → the audio ring empties → under-run burst.** This is the audio team's
   t=62/71 s 114–127 ms stalls, same mechanism.

So: the engine *already* fits the 16.7 ms budget in steady play. The eliminable
prize is the **gameplay load-in burst of synchronous asset streams**, dominated
by `PollUntilLoaded` texture/mesh/clip drains.

## Method (reproducible)

All against the running dev server (`python3 native/web/server.py --port 8421`),
DEBUG build (`?debug=true`), headless bundled-chromium + real GPU (run the Bash
tool with `dangerouslyDisableSandbox` for `/dev/dri`). Env flags injected via the
`?env=RB3_*=...` bridge (`rb3_pre.js` → `ApplyUrlLoaderEnv`).

- **`scripts/web/_framestall-tickprobe.mjs`** — the authoritative steady-state
  tool. Monkeypatches `Module._rb3MainLoopTick` from the page to time the actual
  `await` per frame, splitting the frame period into **tickMs** (engine, CPU +
  any JSPI suspend) vs **rafWaitMs** (rAF pacing idle). Adds passive
  `longtask` PO + reads back the wasm-side `RB3_FRAME_TRACE` JSONL. **No CPU
  profiler → no observer effect.** This is what proved 60 fps.
- **`scripts/web/_framestall-steady-profile.mjs`** — CDP `Profiler` (1 kHz) over a
  warmed steady window + frame-trace readback. Use its profile for *proportional*
  hot-spot attribution only; its absolute dt is inflated (see #1).
- **`scripts/web/_framestall-calltree.mjs`** — rolls a `.cpuprofile` up by
  *inclusive* time + does parent/child attribution (`--focus FN`). Essential
  because `-O0` wasm collapses deep frames into `(program)`; inclusive time +
  call-tree recovers the real owner.
- **`scripts/web/_framestall-ab.mjs`** — fresh-session A/B of `RB3_*` flags vs
  gameplay `dt` (frame-trace, no profiler). Run-to-run variance is ~±6 ms (~15 %),
  so only trust it for *large* effects.
- Trace readback gotcha: use **`window.FS.readFile(...)`**, NOT `window.Module.FS`
  (the latter is a trap-on-access stub that `abort()`s → "unreachable").

Key knob used: `RB3_FRAME_TRACE=/trace.jsonl` (per-frame `dt` bucketed into
`lp`/`lpu`/`fetchMs`/`objMs`/`primeMs`/`texMs`/`meshMs`/`unpackMs`/`pipeMs`/`inflMs`
+ residue; recorder in `native/src/rb3_frame_trace.cpp`).

## Numbers

### Clean steady gameplay (no profiler) — `_framestall-tickprobe.mjs`, 50 s, 3 runs
| metric | value |
|---|---|
| wasm fps | **59.7–59.9** |
| tickMs (engine `RunOneFrame`) | p50 **8.7–9.2**, p90 13.3–14.3, p99 **15.6–18.2**, max 29.7–35.5 |
| rafWait (pacing idle) | p50 7.5–7.9, p99 10.3–11.0, max 61–75 (1 hiccup) |
| period (tick+raf) | p50 **16.6**, mean 16.7 → 60 fps |
| FRAME_TRACE `dt` (game frames) | p50 **9.5**, p99 26.9, max 612; **>33 ms: 5, >50 ms: 2, >100 ms: 1** out of 3348 frames |
| longtasks > 50 ms (passive PO) | **13 in 50 s** (sum 1245 ms, p50 88, max 219, 4 over 100) |

### Longtask timeline (the burst is front-loaded at gameplay load-in)
```
t=0.6s 54ms   t=0.8s 194ms  t=1.1s 142ms  t=2.8s 92ms   t=3.3s 88ms
t=5.6s 112ms  t=6.2s 54ms   t=6.3s 85ms   t=7.7s 163ms  t=7.9s 63ms  t=8.0s 50ms
t=32.0s 50ms  t=46.7s 98ms        <-- only 2 after the load-in burst
```

### Worst gameplay frames (FRAME_TRACE, no profiler) — cause attribution
| frame | dt ms | dominant cause |
|---|---|---|
| f2179 | **612.5** | `lpu`=530 (`PollUntilLoaded`) + obj=160 (`RndTex:floor_wood02_NORM.tex`) + tex=87 — **sync texture drain mid-gameplay** |
| f2449 | 66 | `fetch`=33 (sync XHR for an asset) + residue |
| f2323 | 37.4 | `lp`=16.5 (loader budget) + obj=4.6 (`Mesh:ludclassic_small_club_resource.1.mesh`) |
| f2320 | 36 | `lp`=18.3 + obj=5.9 (`RndTex:bolt_norm.tex`) |
| f2329 | 33.2 | `lp`=15 + obj=1.6 (`RndTex:bonesandspikes_mic_norm.tex`) |
| f2222–2332 | 32–33 | `lp`≈15–16 (budgeted loader) + obj (CharClip streaming: `stand_rhythm_*`, `chord_7th`, `trio_bass_13`, `crowd_ext_*`) |

The 32–37 ms cluster = the **background loader budget (`lp`≈8–16 ms)** plus a
per-object `PostLoad` (`obj`) that pushes the frame over budget when a CharClip /
normal-map / venue mesh streams in. These are partly mitigated already by
`RB3_LOADER_BUDGET_MS`; the `obj` PostLoad spike is the uncapped part.

## Steady per-frame CPU breakdown (proportional; from the profiled run)

`_framestall-calltree.mjs` on `cap/steady-run2/profile.cpuprofile`. Absolute ms
are profiler-inflated; **the proportions are valid**. Of the real per-frame engine
CPU (everything under `rb3MainLoopTick`, ≈ 9 ms/frame clean):

- **`BandUI::Draw` ≈ 42 %** — the render. Inside: `BandRnd::DrawMesh` (top wasm
  self-time), transform churn, GPU JS calls (`writeBuffer`/`submit`/`setBindGroup`
  total only ~1.2 s/40 s — GPU submit is fire-and-forget, **not** main-thread
  blocking; `BandRnd::EndDrawing` is 13 ms total).
- **`BandUI::Poll` ≈ 28 %** + **`BandDirector::Poll` ≈ 18 %** — venue + character
  poll (the HX_NATIVE venue-poll + char-anim).
- **Transform dirty-cascade** `DirtyCache::SetDirty` ↔ `SetDirty_Force` (mutually
  recursive) ≈ **32 % of frame CPU** — driven by char-skeleton animation dirtying
  every dependent transform each frame. Inherent graph propagation.
- **`ObjDirItr<RndMesh>::Advance` → `__dynamic_cast` ≈ 13 %** — the typed
  object-directory iterator does a `dynamic_cast` per entry. Engine-wide.
- **Char rebind** (`RebindHeadHandsAtRest` + `RebindOutfitBonesToOwnSkeleton` →
  `NativeCollectSkinnedMeshes`, the 1.1 s `ObjDirItr::operator++` scan) is
  **front-loaded / transient** — it self-latches (`mNativeReboundOnce`,
  `mNativeHeadReboundOnce`) after ~1–10 s and drops to ~0. A clean A/B
  (`RB3_NO_SKEL_REBIND=1;RB3_NO_HEAD_REBIND=1`) did **not** improve steady fps
  (it got slightly worse, within variance) → **NOT a steady-state culprit**; it
  is a contributor to the load-in burst only.

None of these steady-CPU items individually blow the 16.7 ms budget at 60 fps —
they sum to ~9 ms. They matter for *native* fps headroom (all shared engine code)
but are not the web stall source.

## Ranked stalls (cause → magnitude → when → eliminable?)

1. **`RndTex::SetBitmap` sync `PollUntilLoaded` texture drain — ~530 ms (frame
   612 ms) — at gameplay load-in (and any mid-song venue/LOD texture reveal) —
   ELIMINABLE.** `src/system/rndobj/Tex.cpp:181`. The texture's bitmap is set
   lazily from its `FileLoader` the first time it's needed; on web that forces a
   sync drain (which cooperatively yields but keeps the `await rb3MainLoopTick`
   pending the whole time → no `PumpAudio`). Fixes: (a) **preload venue/LOD
   normal-maps before the song starts** so no `SetBitmap` fires during play;
   (b) HX_NATIVE-guard the lazy path to defer to the budgeted background loader
   instead of `PollUntilLoaded`; (c) cap a single PumpAudio-keepalive call inside
   the drain loop. Shared decomp file → any change must be `#ifdef HX_NATIVE`.

2. **Background-loader budget + per-object PostLoad overspill — 32–37 ms/frame —
   recurring through load-in (CharClips, normal-maps, venue meshes streaming) —
   PARTLY ELIMINABLE.** `lp`≈15–18 ms is the `RB3_LOADER_BUDGET_MS` cap working;
   the `obj` PostLoad on top is uncapped. Tighten the budget (already a knob) and
   split the heaviest single-object PostLoad across frames. These are the
   ~50–194 ms longtasks that hold the bulk of load-in under-runs.

3. **Sync XHR fetch mid-frame — ~33 ms — sporadic during load-in —
   ELIMINABLE.** `fetchMs` on f2449: an asset fetched with a *blocking* XHR
   instead of the async fetch seam. Route through the existing async-open path /
   prefetch so it doesn't block the producer frame.

4. **Char skinning rebind scan (`NativeCollectSkinnedMeshes` full ObjDir walk +
   `dynamic_cast`) — ~2 s spread over first ~1–10 s — at load-in only, self-
   latches — REDUCIBLE (native-only, helps native fps too).** Make the
   collect/scan incremental (skip when nothing changed) so the load-in window is
   cheaper. HX_NATIVE-only functions → free to edit. NOT a steady-state win.

5. **Transform dirty-cascade + `ObjDirItr` `dynamic_cast` — ~45 % of steady frame
   CPU combined — every frame — HARD (engine-structural).** Inherent to the
   transform graph + typed-iterator design. Only worth it for native fps; does
   not cause web stalls (engine already fits 60 fps).

## What is NOT the cause (refuted)

- **Not per-frame engine compute over budget** — clean tick p50 = 9 ms, 60 fps.
- **Not GPU submit/readback blocking** — `EndDrawing` 13 ms total; WebGPU JS
  calls ~1.2 s/40 s; `submit` is fire-and-forget, no `mapAsync`/readback in the
  per-frame path; no `emscripten_sleep` in the renderer.
- **Not GC** — prior jitter profiling + this profile show GC ~45 ms/40 s (0.1 %).
- **Not char rebind in steady state** — self-latches; A/B neutral.
- **The "50 ms/frame" figure** — CPU-profiler observer effect on `-O0` wasm.

## Recommended prototype (next step, not yet built)

Highest-ROI + safest: **eliminate the gameplay-load-in synchronous asset
streams**, starting with stall #1. Concretely, add a venue/LOD-texture
**prewarm before `game_screen` goes live** (so `RndTex::SetBitmap`/`PollUntilLoaded`
never fires during play), measured by the same `_framestall-tickprobe.mjs`
longtask count over the first 8 s (target: drop the 11-longtask load-in burst).
Build the web bundle in a `framestall-*` worktree (`tools/setup-worktree.sh
framestall-prewarm --engine`); keep shared-decomp edits `#ifdef HX_NATIVE`.

## Artifacts

- `cap/tickprobe/tickprobe.json` — clean 60 fps split + longtask timeline + worst
  frames (the authoritative steady-state evidence).
- `cap/steady-run2/{summary,profile.cpuprofile,frametrace,timeline}.json` —
  profiled run (proportional hot-spot attribution; absolute dt inflated).
- Harnesses: `scripts/web/_framestall-{tickprobe,steady-profile,calltree,ab,fsprobe}.mjs`.
