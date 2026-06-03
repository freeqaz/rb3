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
- **`scripts/web/gpu-boot-probe.mjs`** — reports the actual WebGPU adapter (real
  GPU vs SwiftShader) + App-ctor time per backend (`--backend bundled|real|swift`).
  Used to prove the App ctor is GPU-independent. Needs Bash `dangerouslyDisableSandbox`.

## The numbers (localhost, cold cache, real GPU)

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

1. **App-ctor async I/O / JSPI overhead (~7 s, the biggest single chunk).**
   `new App()` runs the boot spine + `PollUntilEmpty` boot loads synchronously, and
   ~7 s of it is the wasm **suspended** (idle in the CPU profile) — independent of
   yield tuning AND **independent of the GPU**. This is the key validated result
   (`scripts/web/gpu-boot-probe.mjs`, run with the Bash sandbox disabled):

   | WebGPU adapter | App-ctor time |
   |---|---|
   | **real GPU** — `vendor=nvidia arch=ampere` (RTX 3090), `isFallback=false` | **12.26–12.30 s** |
   | **SwiftShader** — `vendor=google arch=swiftshader` (software) | **12.29–12.30 s** |

   **Identical.** The App ctor is *not* GPU-bound — not shader compile, not pipeline
   creation. (The earlier "headless SwiftShader inflates it" hypothesis was wrong:
   the bundled-chromium web scripts already get the real GPU with no DISPLAY, via
   Vulkan + the `/dev/dri` render node.) The ~7 s is the **web async-file-I/O / JSPI
   model**: the boot loads issue async reads (`ReadAsync`/`ReadDone`) and the loader
   `emscripten_sleep(0)`s (JSPI suspends the wasm stack) waiting for each to land —
   thousands of event-loop round-trips. Native does the same reads as a synchronous
   memcpy from disk, with no suspension (see cross-check below). **The fix is to
   make boot reads synchronous when the bytes are already in MEMFS** (bundle / IDB
   cache), avoiding the async round-trip; next step is to confirm the boot files are
   actually in the eager bundle and trace the `ReadAsync` path in `native_file.cpp`.
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
   textures directly). This is the *floor* once the async-I/O wait (#1) is removed.

## Native cross-check (both on the real GPU)

`rb3-native` shares the exact same App ctor but runs with **synchronous** file
I/O. It boots to the first screen in **~5.2 s** total (process start + GPU init +
full App ctor) — vs the web App ctor *alone* at ~12.3 s. Since web and native both
use the **same real RTX 3090** (validated above), and web's App ctor is
GPU-independent, the entire native↔web gap is the **synchronous-vs-async I/O
model**: native reads are a memcpy with no suspension; web reads suspend the wasm
stack (JSPI) per async read. That ~7 s is the addressable prize.

## Running the GPU probe

```bash
python3 native/web/server.py --port 8421 &
# Bash tool: dangerouslyDisableSandbox (Chromium needs the /dev/dri GPU device)
node scripts/web/gpu-boot-probe.mjs --backend bundled  # real GPU, the default config
node scripts/web/gpu-boot-probe.mjs --backend swift     # forced software, for contrast
```
No `DISPLAY` / xvfb needed — bundled chromium + `--use-angle=vulkan
--enable-features=Vulkan` reaches the NVIDIA GPU through the render node headless.

## Caveats

- **Real GPU is already in use** for all numbers here (validated: `nvidia`/`ampere`,
  `isFallback=false`). Real users on real GPUs will *not* boot faster than this —
  the App ctor is not GPU-bound. (A weaker GPU won't help; a faster CPU + sync I/O
  will.)
- Ephemeral browser context per run ⇒ **cold IDB cache** (worst case). Warm-cache
  boots (returning users) skip the sync XHRs entirely.
- The intro cinematic now holds `intro_movie_screen` for 68 s, so the passive
  profiler stops at first-screen; use `--nav` (which skips/rides it) to profile
  past it.
