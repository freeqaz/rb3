# Web boot/load performance — measured findings (2026-06-03)

Hard data on the rb3-web boot, captured with the new profiler tooling, plus what
the data says to optimize (and what it says NOT to bother with).

## Tooling (use these to measure, not guess)

- **`scripts/web/loadperf-profile.mjs`** — the main profiler. Boots rb3-web
  headless (no xvfb) and captures, on one timeline: a V8 **CPU profile**
  (`boot.cpuprofile`, load in Chrome DevTools › Performance or speedscope.app),
  **Long Tasks** (>50ms main-thread blocks) attributed to boot phase, **RAF
  gaps** (user-visible freezes), **Resource Timing** (every fetch + size +
  duration), and **boot-phase milestones**. Writes `profile.json` + a summary.
  - `--nav` also drives splash→main_hub→song_select→game (profiles song-load).
  - `--loader-yield N` / `--loader-budget N` A/B the loader knobs (no rebuild).
  - `--no-cpuprofile` for a faster pass.
- **`scripts/web/analyze-cpuprofile.mjs <boot.cpuprofile>`** — self-time by
  function + by category (wasm / js-glue / gc / **idle**), no GUI needed.
- **Boot-phase markers** — `native/src/main_web.cpp` `BootMark()` emits
  `performance.mark('rb3:boot:<phase>')` + `window.rb3BootPhaseLog` at each
  BootState transition (fetch_start, fetch_done, engine_init_done, gpu_ready,
  appctor_start, appctor_done). The profiler prints the per-phase deltas.
- **URL-param knobs** — `?loaderYieldMs=`, `?loaderBudgetMs=`, `?frameInstrument=1`
  (`main_web.cpp` `ApplyUrlLoaderEnv` → setenv before the first Poll).

## The numbers (headless, localhost, cold cache)

Boot to first screen ≈ **13–20 s**. The phase timeline splits it cleanly:

| phase | typical | notes |
|---|---|---|
| nav → `fetch_start` (wasm dl + compile + runtime init) | **0.5 s OR ~7.5 s** | **bimodal** — wasm code-cache hit (0.5 s) vs cold compile of the 28 MB -O0 -g2 dev wasm (~7.5 s) |
| `fetch_start` → `gpu_ready` (bundle, engine init, GPU adapter) | ~0.2 s | fast + consistent |
| `appctor_start` → `appctor_done` (`new App()`) | **~12.45 s** | **rock-solid, the steady bottleneck** |

CPU profile of the boot window: **~78% idle, only ~5 s of actual CPU work.**
Busy time breakdown (self-time): DTA lexer (`yylex`/`yy_*`/`DataInput`, ~0.4 s),
DXT texture decompress (`DecompressDXT1/5*`, ~0.25 s), `std::vector<uint8>`
buffer ctors (~0.7 s), `BinStream::Read` + `DataArray::FindArray` +
`NativeStdioFile::Read`. The other **~7 s of the App ctor is idle (async wait)**.

## What the data REFUTES (don't chase these)

- **WASM size is not the *App-ctor* bottleneck.** The 28 MB wasm is 3.2 MB brotli
  over the wire and downloads in ~42 ms on localhost; splitting/shrinking it would
  not move the 12.45 s App ctor. BUT size *does* drive the bimodal pre-fetch: a
  **cold** V8 wasm-compile of the 28 MB **-O0 -g2 dev build** costs ~7.5 s (a warm
  code-cache hit is ~0.5 s). So the user's "wasm" intuition applies to the
  *pre-fetch cold-compile*, addressable by **shrinking the wasm** (release/-Oz,
  strip `-g`) — not by code-splitting, and not for the App ctor.
- **Network is not the bottleneck on localhost.** 96 sync XHRs total ~609 ms;
  the bundle is 7.8 MB / ~39 ms. (On a *remote* host this changes — see caveats.)
- **The per-frame loader yield interval does not reliably change boot time.**
  A/B of `loaderYieldMs` ∈ {2,4,8,16,50,100} produced only the bimodal 13/20 s
  split with no correlation. The App-ctor 12.45 s is constant across all of them
  → the ~7 s App-ctor idle is **not** loader-yield overhead. (This refuted the
  initial "yield-every-8ms-slice is the cost" hypothesis. The loader code was
  still refactored to decouple slice-granularity from yield-frequency — a clarity
  + spin-overhead win + the `RB3_LOADER_YIELD_MS` tunable — but it is honestly
  neutral for total boot time.)

## What the data points AT (ranked)

1. **App-ctor async wait (~7 s, the biggest single chunk).** `new App()` runs the
   boot spine + `PollUntilEmpty` boot loads synchronously, and ~7 s of it is the
   wasm suspended (idle in the CPU profile), independent of yield tuning. Leading
   suspects, in order: **(a) WebGPU pipeline/shader compilation** for the boot
   UI/font/track milos — under headless this is **SwiftShader software compile**,
   which is slow and almost certainly inflated vs a real GPU; **(b)** genuinely
   async boot-character loads (`FileMerger`). Next step: instrument the App-ctor
   sub-phases (shader-compile count/time vs file-load time) to split (a) from (b).
   **Caveat: re-measure on a real GPU before optimizing — much of (a) may vanish.**
2. **wasm cold-compile latency (bimodal pre-fetch, ~7.5 s when cold).** The
   28 MB **-O0 -g2 dev build** is the cost. `native/web/rb3_pre.js` was switched to
   **`WebAssembly.instantiateStreaming`** (compile-while-download; server already
   sends `application/wasm` + br/gz, arrayBuffer fallback) — best-practice and a
   real win on *remote* networks (overlaps download+compile), but a 6-run A/B
   showed it does NOT remove the localhost cold-compile bimodality (the compile
   cost is inherent to the wasm size). The actual lever is **shrinking the wasm**:
   a `--release` (-O0 -g0) or size-optimized build, which also drops the 71 MB→
   smaller download. Worth a dedicated pass; the dev build keeps -g2 for debugging.
3. **App-ctor CPU work (~5 s).** DTA lexing + DXT decompress + buffer ctors. DXT
   decompress on the CPU is a candidate to move to the GPU (sample compressed
   textures directly) — a real-GPU win independent of headless.

## Caveats

- All numbers are **headless SwiftShader** (software Vulkan via ANGLE). Real users
  on real GPUs will see a much faster App ctor if (1a) dominates. **Re-measure on
  hardware** (display + `--use-angle` default, or a non-headless run) before
  investing in shader-compile reduction.
- Ephemeral browser context per run ⇒ **cold IDB cache** (worst case). Warm-cache
  boots (returning users) skip the sync XHRs entirely.
- The intro cinematic now holds `intro_movie_screen` for 68 s, so the passive
  profiler stops at first-screen; use `--nav` (which skips/rides it) to profile
  past it.
