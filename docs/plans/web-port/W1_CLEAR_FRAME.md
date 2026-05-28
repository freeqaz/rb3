# W1 — RB3 clear-color frame in browser

**Parent plan:** [`PLAN.md`](PLAN.md). Read it for context.
**Depends on:** [`W0_ENGINE_EXTRACTION.md`](W0_ENGINE_EXTRACTION.md) (engine `MILO_BUILD_WEB` must be a real switch).
**Blocks:** W2.

## Goal

The smallest target that proves the toolchain works end-to-end for RB3:

- `cd rb3/native && emcmake cmake -B build-web && cmake --build build-web` →
  produces `rb3-web.wasm` + `rb3-web.js`.
- `python3 native/web/server.py --port 8421` serves the page.
- Headed Chromium loads the page, downloads the asset bundle, runs the same
  `SystemPreInit("config/band_preinit_keep.dta")` →
  `SystemInit("config/band_keep.dta")` sequence that `RB3_BOOT=1 rb3-native`
  does natively (see `native/src/main_native.cpp:459,463`), brings up
  `BandRnd`, and clears the canvas via the `BandRnd::BeginFrame` /
  `EndFrame` cycle.
- The browser tab ticks ≥5 frames in `BOOT_RUNNING` with no WASM trap.
- A Playwright screenshot proves the canvas is the smoke clear-colour (not
  the default page background).

## Out of scope

- Rendering an actual `.milo`. That's W2.
- Audio. That's W3.
- Input. That's W3.
- Loading the full `App` / `RB3_GAME=1` flow. W1 uses the headless rb3-native
  boot harness adapted for the browser; W3 swaps in `App`.

## Files to create / touch

### `rb3/native/CMakeLists.txt`

1. At the top, before the existing `add_subdirectory(${MILO_ENGINE_PATH} …)`,
   add `set(MILO_BUILD_WEB ON CACHE BOOL "RB3 web target" FORCE)` **inside**
   an `if(EMSCRIPTEN)` block. The bare native build stays unaffected.
2. After the existing `rb3-native` block, add an `if(EMSCRIPTEN)` block
   declaring an `rb3-web` executable. Source set:
   - `${CMAKE_SOURCE_DIR}/src/main_web.cpp` (new — see below).
   - The full RB3 rendering glue used by `rb3-native`:
     `rb3_band_rnd.cpp`, `rb3_render_tri.cpp`, `rb3_render_mesh.cpp`,
     `rb3_render_hook.cpp`.
   - `rb3_platform_native.cpp`, `rb3_netsession_native.cpp`,
     `rb3_waitinguser_gate_native.cpp`, `rb3_game_input.cpp`,
     `rb3_keychain_native.cpp` (mirror `rb3-native`).
   - `${NATIVE_FORK_SOURCES}`, `${DTA_LEXER}`, `${NATIVE_SHIMS}` minus
     `native_file.cpp` (replaced by the engine's `File_Web.cpp`).
   - The two link-stub assembly files (`band3_link_stubs.s`,
     `rndobj_synth_link_stubs.s`).
   - The MOGG decryption C sources
     (`${REPO_ROOT}/src/system/synth/tomcrypt/{aes,crypt,ctr}.c`) — wasm-clean.
3. Apply `rb3_configure_target(rb3-web)` (existing helper).
4. Call the engine-side helper for web link options:
   `milo_engine_apply_web_target_options(rb3-web)` (lands in W0).
5. Add the RB3-specific exports:
   - `target_link_options(rb3-web PRIVATE
     -sEXPORTED_FUNCTIONS=["_main","_rb3MainLoopTick"]
     -sJSPI_EXPORTS=["_main","_rb3MainLoopTick"])`.
6. Add the canvas / module / asset path defines:
   - `target_compile_definitions(rb3-web PRIVATE HX_NATIVE=1 HX_WEB=1
     MILO_DEBUG=1 _DEBUG=1
     MILO_WEB_CANVAS_SELECTOR="#rb3-canvas"
     MILO_WEB_AUDIO_NS="rb3")`.
   - The two `MILO_WEB_*` macros are the engine-side parameterisation
     added in W0 (replacing hardcoded `dc3-canvas` / `_dc3Audio`
     symbols). If those macros aren't actually exposed by the engine
     after W0 lands, treat that as a W0 regression — stop and fix
     before continuing W1.

### `rb3/native/src/main_web.cpp` (new)

Mirror DC3's `dc3-decomp/native/src/main_web.cpp` (170 LOC). State machine:

```
BOOT_INIT       → WebAssetsInit(), WebAssetsFetchBundle()
BOOT_FETCHING   → poll WebAssetsAllDone()
BOOT_ENGINE_INIT → minimal RB3 boot:
                     chdir("/data")  (MEMFS layout is whatever the bundle ships)
                     run the same init sequence rb3-native's main_native.cpp
                     does for `RB3_BOOT=1` mode (main_native.cpp:436-499):
                       SystemPreInit("config/band_preinit_keep.dta")
                       SystemInit("config/band_keep.dta")
                       register object factories (rndobj + synth)
                     instantiate gBandRnd, call BandRnd::InitForCanvas() (new
                       helper — see below)
                     call gBandRnd.SetClearColor(Hmx::Color(0.2, 0.4, 0.7))
                       (non-black so the smoke clearly differs from page bg)
BOOT_GPU_WAIT   → poll gBandRnd.IsReady() (engine GpuDevice async path)
BOOT_GPU_READY  → one BeginFrame(nullptr) / EndFrame() cycle to commit
                    the initial clear
BOOT_RUNNING    → per-frame: BeginFrame(nullptr) / EndFrame()
                  EM_ASM emits window.rb3FrameCount = N each frame
```

The boot harness used inside `BOOT_ENGINE_INIT` is the simplified
`RB3_BOOT=1` path from `main_native.cpp`, not the full `RB3_GAME=1` App
flow. Reuse the same call sequence; just wrap it so it can be invoked
without the headless main loop driver.

Key implementation notes for the agent:

- Call `emscripten_set_main_loop(mainLoop, 0, /*sim_infinite_loop=*/0)` so
  the browser drives the loop, not a `while(true)`.
- Emit `window.rb3FrameCount = N` via `EM_ASM_({ window.rb3FrameCount = $0; }, sFrameCount)`
  each frame so Playwright can poll readiness (DC3 pattern).
- Wrap the boot in `try { … } catch (...) { sBootState = BOOT_ERROR; printf("RB3 Web: boot error\n"); }`.
- Use `printf` for logs; Emscripten routes to console.

### `rb3/native/src/rb3_band_rnd.{cpp,h}` (extend, not replace)

Add two web-aware members:

```cpp
class BandRnd : public Rnd {
public:
    ...
#ifdef HX_WEB
    // Async init for browser GpuDevice (the engine's GpuDevice already
    // exposes async adapter/device under HX_WEB).
    void InitForCanvas();            // selector comes from MILO_WEB_CANVAS_SELECTOR
    // bool IsReady() const;         // already exists on BandRnd; reuse it.
#endif
    ...
};
```

`BandRnd::IsReady()` already exists for the native async-adapter path —
reuse it under `HX_WEB`; no new method needed. `InitForCanvas()`
delegates to the engine's web GpuDevice init. The W0 parameterisation
exposes `MILO_WEB_CANVAS_SELECTOR` as a build macro; `InitForCanvas` does
**not** take a runtime selector argument — the selector is baked at
compile time per consumer.

There is no separate `PresentFrame()`. Under WebGPU the surface
auto-composites at the end of each `requestAnimationFrame`; `EndFrame()`
ends the render pass and that's enough. The native readback path in
`rb3_render_mesh.cpp` is W2 territory; W1 just clears.

The native build is untouched. The existing native `BeginFrame` /
`EndFrame` / `SetClearColor` code paths work under `HX_WEB` once the
engine GpuDevice is initialised for the canvas.

### `rb3/native/web/index.html` (new)

Copy `dc3-decomp/native/web/index.html` (314 LOC) verbatim, then:

- Replace every `dc3` with `rb3` (canvas id `rb3-canvas`, module
  `rb3-web.js`/`.wasm`, status messages "RB3 — WebGPU", etc.).
- Drop DC3-specific controls (`controls span` lists DC3 key bindings — RB3
  has none yet in W1; leave the slot, say "(input wires up in W3)").
- Keep the WebGPU detection + COOP/COEP banner code unchanged.

### `rb3/native/web/server.py` (new)

Copy `dc3-decomp/native/web/server.py` (324 LOC) verbatim, then:

- `ASSETS_DIR` default: `<repo>/orig-assets/extracted/` (RB3 layout).
- Port: 8421 (DC3 uses 8420, keep them disjoint so both can run
  simultaneously).
- `(..)` parent-up path normalisation — RB3's extraction uses the same
  convention as DC3 (`rb3/orig-assets/extracted/(..)/` exists), so the
  logic is unchanged. Verify by listing the dir during agent prep.
- Update help / docstrings to say RB3 instead of DC3.

### `rb3/scripts/web/` (new)

- `rb3/scripts/web/smoke-test.mjs` — copy from
  `dc3-decomp/scripts/web/smoke-test.mjs`, edit:
  - Server port 8421, canvas id `rb3-canvas`, module name `rb3-web.js`.
  - Expect `window.rb3FrameCount` (not `dc3FrameCount`).
  - Results path: `rb3/scripts/web/results/<timestamp>/`.
- `rb3/scripts/web/package.json` (new, minimal): declare `playwright` and
  `pixelmatch` as dependencies (W2 will use `pixelmatch`; install both
  here so the npm-install step lands once).
- `rb3/scripts/web/.gitignore`: ignore `results/` and `node_modules/`.
- `rb3/scripts/web/build.sh` (matches `dc3-decomp/scripts/build/web.sh`):
  ```sh
  #!/usr/bin/env bash
  set -euo pipefail
  NATIVE_DIR="$(cd "$(dirname "$0")/../../native" && pwd)"
  BUILD_DIR="$NATIVE_DIR/build-web"
  DEPLOY_DIR="$NATIVE_DIR/web/build"
  mkdir -p "$DEPLOY_DIR"
  if [ ! -d "$BUILD_DIR" ]; then
      emcmake cmake -S "$NATIVE_DIR" -B "$BUILD_DIR"
  fi
  cmake --build "$BUILD_DIR" -- -j"$(nproc)" rb3-web
  cp "$BUILD_DIR/rb3-web.js" "$BUILD_DIR/rb3-web.wasm" "$DEPLOY_DIR/"
  cp "$NATIVE_DIR/web/index.html" "$DEPLOY_DIR/"
  # audio-worklet.js is copied by the engine POST_BUILD step
  echo "Deployed to $DEPLOY_DIR"
  ```

## Asset bundle for W1

The W1 bundle just needs what `SystemPreInit`/`SystemInit` reads — the cfg
DTAs (79 files in `orig-assets/extracted/config/`, ~916KB raw) and their
`#include` targets. From observation
(`docs/native/NATIVE_PORT_ROADMAP.md:548`), the native `RB3_BOOT=1` set is:

- `config/` (the 79 `.dta` cfg files for the boot path)
- Any `(..)` parent-walk indirections those configs use

The server's auto-bundle path includes everything under
`orig-assets/extracted/` — for W1, prune via a `--filter config/` flag (or
hardcode for now). Realistic W1 bundle: well under 5MB gzipped.

Do not include song or venue milos in the W1 bundle — they're not read by
`SystemPreInit` and they'd inflate the bundle.

## Acceptance test

1. After W0 has landed and engine pin is bumped:
   ```sh
   cd rb3/native
   source ~/emsdk/emsdk_env.sh
   bash ../scripts/web/build.sh
   python3 web/server.py --port 8421 &
   ```
2. From `rb3/`: `node scripts/web/smoke-test.mjs`.
3. Expect, in `rb3/scripts/web/results/<timestamp>/summary.json`:
   - `result: pass`
   - Frame counter advanced ≥5 (`window.rb3FrameCount`)
   - No WASM trap, no JS exception
4. Inspect `canvas.png`:
   - Canvas pixels are the RB3 smoke clear-colour (`rgb(51,102,178)` for
     `Hmx::Color(0.2, 0.4, 0.7)`), not the page background `#0a0a0a`.
5. Inspect `console.jsonl` for the expected printf trail:
   - "RB3 Web: downloading assets..."
   - "RB3 Web: assets ready (N files, 0 errors)"
   - "RB3 Web: SystemPreInit complete"
   - "RB3 Web: SystemInit complete"
   - "RB3 Web: BandRnd ready"
   - "RB3 Web: BOOT_RUNNING"
6. The DC3 web build remains green (regression check):
   ```sh
   cd ../dc3-decomp && node scripts/web/smoke-test.mjs
   ```

## Known gotchas

- `chdir(RB3_DATA)` in the native main becomes `chdir("/data")` under the
  browser — same convention DC3 uses.
- `RB3_BOOT=1` / `RB3_GAME=1` / `RB3_GPU_SMOKE=1` are **runtime env vars**
  in `main_native.cpp`, not compile-time `#ifdef`s. W1's `main_web.cpp`
  doesn't read env vars; it just hardcodes the equivalent of `RB3_BOOT=1`
  inline (the SystemPreInit/SystemInit pair). W3 swaps in the App boot.
- The `(..)` parent-walk dir under `orig-assets/extracted/` exists for
  RB3 (verified) — same convention as DC3. `WebAssets.cpp`'s path
  resolver should handle it identically.
- `BandRnd` under WebGPU may surface alignment or pipeline-state issues
  not seen on native Vulkan. If the canvas stays page-background-coloured
  past 5 frames: log the `wgpu::Device` validation messages via the
  engine's existing callback and bisect by stubbing each pipeline.
- The native `RB3_GPU_SMOKE=1` path returns from `main()` after one
  readback. The browser path loops via `emscripten_set_main_loop`; do
  **not** copy any one-shot exit logic from the smoke path.
- `tomcrypt` C sources (`aes.c`, `crypt.c`, `ctr.c`) need `LANGUAGE C` set
  in CMake — mirror the existing `rb3-native` target wiring at
  `native/CMakeLists.txt:430-432`.

## Suggested subagent prompt

> Execute Phase W1 of the RB3 web port plan
> (`rb3/docs/plans/web-port/W1_CLEAR_FRAME.md`). W0 has already landed:
> `milo-native-engine` ships web platform sources under `MILO_BUILD_WEB`,
> parameterised by `MILO_WEB_CANVAS_SELECTOR` and `MILO_WEB_AUDIO_NS`. Add
> an `rb3-web` Emscripten target to `rb3/native/CMakeLists.txt` (setting
> the two macros to `"#rb3-canvas"` and `"rb3"`), write
> `rb3/native/src/main_web.cpp` (boot state machine modelled on DC3's
> `dc3-decomp/native/src/main_web.cpp`), extend `BandRnd` with an
> `InitForCanvas()` member under `HX_WEB` (reuse existing `IsReady` /
> `BeginFrame` / `EndFrame` / `SetClearColor`), and stand up
> `rb3/native/web/{index.html, server.py}` plus
> `rb3/scripts/web/{build.sh, smoke-test.mjs, package.json}` by copying and
> adapting DC3's `smoke-test.mjs` (the actual file name in DC3 — not
> `test.mjs`). Acceptance: `node rb3/scripts/web/smoke-test.mjs` reports
> `result: pass` and a non-default canvas screenshot. Do not break DC3's
> web build — run `node dc3-decomp/scripts/web/smoke-test.mjs` as a
> regression check before declaring done.
