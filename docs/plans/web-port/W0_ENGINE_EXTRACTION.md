# W0 — Engine Extraction (lift DC3's web infra into milo-native-engine)

**STATUS: DONE — 2026-05-28.** DC3's web infra lifted into `milo-native-engine`
under `MILO_BUILD_WEB`, parameterised by `MILO_WEB_CANVAS_SELECTOR` /
`MILO_WEB_AUDIO_NS`. DC3 web build remains green; RB3 pins the new engine SHA.
Engine `cfaaa5b`, dc3 `c800138e`, rb3 `7f62c2af`.

**Parent plan:** [`PLAN.md`](PLAN.md). Read it for context.
**Depends on:** nothing.
**Blocks:** W1, W2, W3, W4.

## Goal

Move the engine-portable pieces of DC3's web port from `dc3-decomp/native/`
into `milo-native-engine/`, gated by `MILO_BUILD_WEB`. Both DC3 and RB3 then
consume the same shared layer. The engine's existing `MILO_BUILD_WEB` option
(`milo-native-engine/CMakeLists.txt:71`) was a Phase-6 placeholder; W0 makes
it real.

**Hard constraint:** DC3's `dc3-web` build must remain functionally
unchanged. The Playwright smoke (`dc3-decomp/scripts/web/smoke-test.mjs`)
must still pass against the post-extraction tree.

## Scope

In scope:

- Lift WebAssets fetcher, web platform stubs, web GpuDevice path, web
  AudioDevice, web ImGui backend, and the web JS glue into the engine.
- Move the link/compile-option block (JSPI, `-fwasm-exceptions`,
  `-sFETCH=1`, `-sUSE_ZLIB=1`, etc.) into an engine-side CMake helper.
- Rewire `dc3-decomp/native/CMakeLists.txt` to consume the extracted layer.
- Verify DC3 web still builds + boots + clears canvas + ticks frames.

Out of scope:

- Any RB3 code or RB3 target. W0 is pure refactor on the engine + DC3 side.
- Changes to DC3's `main_web.cpp` (the boot state machine stays
  DC3-specific until W3's `UNIFY_WITH_APP` work generalises it).
- WebMovieImpl extraction. Leave it in DC3 until W3 — RB3 has a different
  movie shape and the porting cost is W3 work.
- `scripts/web/smoke-test.mjs` extraction. Optional; can defer to W1.

## Files to touch

### Engine (`milo-native-engine/`)

| Source (DC3) | Destination (engine) | Adaptation |
|---|---|---|
| `dc3-decomp/native/src/platform/WebAssets.{cpp,h}` | `src/platform/WebAssets.{cpp,h}` | Near-verbatim. Strip "DC3 Web Port" header comment; `/data/` MEMFS root is constant and engine-neutral. |
| `dc3-decomp/native/src/platform/File_Web.cpp` | `src/platform/File_Web.cpp` | Verbatim. |
| `dc3-decomp/native/src/platform/CDReader_Web.cpp` | `src/platform/CDReader_Web.cpp` | Verbatim. |
| `dc3-decomp/native/src/platform/GpuDevice_Web.cpp` | `src/platform/GpuDevice_Web.cpp` | **Parameterise.** Replace 3 hardcoded `"#dc3-canvas"` selectors with a `MILO_WEB_CANVAS_SELECTOR` build macro (defaulted to `"#milo-canvas"`; DC3 sets `-DMILO_WEB_CANVAS_SELECTOR='"#dc3-canvas"'`, RB3 sets `'"#rb3-canvas"'`). |
| `dc3-decomp/native/src/audio/AudioDevice_Web.cpp` | `src/audio/AudioDevice_Web.cpp` | **Parameterise.** ~20 hardcoded `_dc3Audio` / `dc3-audio-processor` / `dc3CaptureAudio` / `dc3_*` JS state names + C `EMSCRIPTEN_KEEPALIVE` exports. Introduce a `MILO_WEB_AUDIO_NS` macro (default `"milo"`; DC3 → `"dc3"`, RB3 → `"rb3"`) and token-paste the prefix. Audit-checkpoint: every `dc3_` / `dc3CaptureAudio` symbol must be macro-driven. |
| `dc3-decomp/native/src/gfx/ImGuiBackend_Web.cpp` | `src/gfx/ImGuiBackend_Web.cpp` | **Parameterise.** 2 hardcoded `"#dc3-canvas"` selectors; reuse `MILO_WEB_CANVAS_SELECTOR`. |
| `dc3-decomp/native/web/audio-worklet.js` | `src/platform/web/assets/audio-worklet.js` | Verbatim (JS module is symbol-namespace-agnostic on the worklet side). |
| `dc3-decomp/native/web/missing_stubs.js` | `src/platform/web/assets/missing_stubs.js` | Verbatim. |

**Consumer override syntax** (DC3 keeps current behaviour, RB3 picks its own
namespace):
```cmake
# DC3
target_compile_definitions(dc3-web PRIVATE
    MILO_WEB_CANVAS_SELECTOR="#dc3-canvas"
    MILO_WEB_AUDIO_NS="dc3")
# RB3
target_compile_definitions(rb3-web PRIVATE
    MILO_WEB_CANVAS_SELECTOR="#rb3-canvas"
    MILO_WEB_AUDIO_NS="rb3")
```

The JS-side debug hooks (`window.dc3CaptureAudio()`, `dc3_start_capture`,
etc.) are public ABI for DC3's HTML/JS. After the lift, DC3's HTML must
update to call `window.milo_<ns>CaptureAudio()` or whatever scheme the
macro generates. Track this as a follow-up DC3 task; it does **not** break
the headless Playwright smoke (which doesn't exercise the debug hooks).

CMake changes in `milo-native-engine/CMakeLists.txt`:

1. When `MILO_BUILD_WEB=ON` AND `EMSCRIPTEN`, append the lifted web sources
   to `milo-engine`'s target sources. Gate with both `if(MILO_BUILD_WEB AND
   EMSCRIPTEN)`.
2. Expose a helper function `milo_engine_apply_web_target_options(<tgt>)`
   that applies the link/compile options DC3's `dc3-web` target currently
   applies inline. Actual line ranges in `dc3-decomp/native/CMakeLists.txt`:
   web target block `1202-1382`; `target_link_options(dc3-web …)` block
   `1342-1366`; JSPI block `1376-1382`; `DC3_WEB_ASYNC` option at line 1340.
   - `target_compile_options(<tgt> PRIVATE "SHELL:--use-port=emdawnwebgpu" -fwasm-exceptions)`
   - `target_link_options(<tgt> PRIVATE …)` — the full DC3 list:
     `--use-port=emdawnwebgpu`, `-sALLOW_MEMORY_GROWTH=1`,
     `-sMAXIMUM_MEMORY=512MB`, `-sSTACK_SIZE=4194304`,
     `-sENVIRONMENT=web,node`, `-sERROR_ON_UNDEFINED_SYMBOLS=0`,
     `-sUSE_ZLIB=1`, `-sUSE_OGG=1`, `-sUSE_VORBIS=1`, `-sFETCH=1`,
     `-sASSERTIONS=1`, `-sSTACK_OVERFLOW_CHECK=2`,
     `-sEXPORTED_FUNCTIONS=["_main"]`,
     `-sEXPORTED_RUNTIME_METHODS=["ccall","cwrap"]`,
     `-fwasm-exceptions`, `-g2`.
     Consumer extends `EXPORTED_FUNCTIONS` (DC3 adds `_dc3MainLoopTick`,
     RB3 W1 adds `_rb3MainLoopTick`).
   - `--pre-js ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/web/assets/missing_stubs.js`
   - JSPI block (when `MILO_WEB_ASYNC=ON`, default ON, gated on Chrome
     137+):
     - `target_compile_definitions(<tgt> PRIVATE MILO_WEB_ASYNCIFY=1)`
     - `target_link_options(<tgt> PRIVATE -sJSPI -sJSPI_EXPORTS=["_main"])`
       (consumer extends the JSPI exports list to match its tick symbol).
   - A `add_custom_command(POST_BUILD …)` that copies
     `audio-worklet.js` next to the linked WASM. Consumer overrides the
     destination dir.
3. Add an option `MILO_WEB_ASYNC` (default ON), mirroring DC3's
   `DC3_WEB_ASYNC`. The C-side gate `DC3_WEB_ASYNCIFY` is referenced
   inside `dc3-decomp/native/src/main_web.cpp:150`; rename it to
   `MILO_WEB_ASYNCIFY` as part of the helper migration. **This requires
   the one-line edit to `main_web.cpp`** — overriding the "don't touch
   `main_web.cpp`" rule narrowly for this `#ifdef` rename (no behaviour
   change). All other DC3-specific logic in `main_web.cpp` stays untouched.
4. Add the engine-side **EMSCRIPTEN imgui target**. Today's engine imgui
   block at `milo-native-engine/CMakeLists.txt` is gated
   `if(MILO_ENGINE_HAVE_CONTEXT AND MILO_ENGINE_BUILD_GFX AND NOT EMSCRIPTEN)`
   — under EMSCRIPTEN the engine creates no `imgui` target. DC3's
   `target_link_libraries(dc3-web PRIVATE imgui)` is currently satisfied
   by a DC3-side block (trace it before lifting). W0 stands up an
   EMSCRIPTEN sibling that mirrors the non-EMSCRIPTEN block but links
   against emdawnwebgpu / browser-GL instead of Dawn/glfw, exposing
   `imgui` as a CMake target either way.
5. Update `milo-native-engine/README.md` so `MILO_BUILD_WEB` is documented
   as a real switch (no "Phase 6 placeholder" wording).

### DC3 (`dc3-decomp/`)

| Action |
|---|
| `dc3-decomp/native/CMakeLists.txt`: remove the web sources lifted to the engine (verbatim + parameterised). Set `MILO_BUILD_WEB=ON` before `add_subdirectory(${MILO_ENGINE_PATH})`. Replace the inline link-options block with a call to `milo_engine_apply_web_target_options(dc3-web)`. Add DC3-only exports (`_dc3MainLoopTick`) to the consumer's `-sEXPORTED_FUNCTIONS` and `-sJSPI_EXPORTS`. Set `target_compile_definitions(dc3-web PRIVATE MILO_WEB_CANVAS_SELECTOR="#dc3-canvas" MILO_WEB_AUDIO_NS="dc3")`. |
| **Verify `DC3_WEB_GFX_SOURCES` (lines 1269-1283) survivors.** Only `GpuDevice_Web.cpp` moves to the engine; the other ~11 sources (Screenshot, TextureConvert, VertexFormats, PipelineManager, FrameCapture, ShadowPass, BloomPass, DofPass, PostProcPass, DrawRect2D, GpuResourceRegistry, mikktspace.c) must remain wired — confirm they already live in engine `src/gfx/` (lifted in a prior phase) so DC3's CMake doesn't orphan them. If any is still DC3-only, leave it in `dc3-decomp/native/src/gfx/`. |
| Delete the now-engine-side source files from `dc3-decomp/native/src/`. **Do NOT delete `WebMovieImpl.cpp`** — it's in `DC3_WEB_CORE_SOURCES` (line 1252) and is explicitly out of scope for W0 (RB3 has a different movie shape). |
| Delete `dc3-decomp/native/web/audio-worklet.js` and `missing_stubs.js` (or leave a thin "moved to engine" stub if any tool still points at the old path). |
| Update `dc3-decomp/native/web/build.sh` to deploy the engine-side `audio-worklet.js` (via the engine `POST_BUILD` copy step) instead of the in-repo path. |
| Update DC3's `index.html` JS-side debug-hook calls if any use the `dc3_*` symbol names — after the parameterisation, the symbols still resolve under the `dc3` namespace, so this should be a no-op, but verify. |
| Bump `MILO_ENGINE_PIN` in `dc3-decomp/native/CMakeLists.txt` to the engine commit landing W0. |
| Run `scripts/build/web.sh` followed by `node scripts/web/smoke-test.mjs` from `dc3-decomp/`. Expect: build succeeds, browser tab boots, no regression in `dc3-decomp/scripts/web/results/<timestamp>/summary.json`. |

### RB3 (`rb3/`)

| Action |
|---|
| Bump `MILO_ENGINE_PIN` in `rb3/native/CMakeLists.txt` to match DC3's, so the soft-pin warning stays quiet. **Do not** set `MILO_BUILD_WEB=ON` yet — that lands in W1. |

### Pin-bump ordering

Strict sequence (the engine commit must land first):

1. Land engine PR with the lift + helper + parameterisation + EMSCRIPTEN imgui target.
2. Capture the merged engine SHA.
3. In the **same change set**, bump `MILO_ENGINE_PIN` in both `dc3-decomp/native/CMakeLists.txt` and `rb3/native/CMakeLists.txt` to that SHA, plus DC3's CMake rewire.
4. Verify `cmake --build build` in `rb3/native/` produces `rb3-native` and `rb3-dta` cleanly (the pin bump can land before any RB3 web work).
5. Verify DC3's acceptance test (below).

## Acceptance test

1. From `dc3-decomp/`:
   ```sh
   bash scripts/build/web.sh
   python3 native/web/server.py --port 8420 &
   node scripts/web/smoke-test.mjs
   ```
   Expect:
   - Build succeeds (no missing-source errors).
   - Frontend loads in headed Chromium.
   - `summary.json` reports `result: pass`, no WASM trap.
   - `canvas.png` shows a non-default canvas colour (the post-clear frame).
2. From `dc3-decomp/native/build-web/`: `ls dc3-web.wasm` exists.
3. Diff the pre/post WASM sizes — they should be within ±2% (extraction
   shouldn't materially change codegen).
4. From `rb3/native/`: `cmake --build build` still produces `rb3-dta` and
   `rb3-native` cleanly (the pin bump didn't break anything).

## Known gotchas (DC3 source survey)

- `dc3-decomp/native/CMakeLists.txt:1202-1382` is the entire web target
  block — read all of it before deleting anything. There are subtleties:
  filtered ENGINE sources (remove `HttpReqCurl`, `WebSvcMgrCurl`,
  ogg/vorbis), DC3-only sources (`main_web.cpp`, `WebSvcMgr_Stub.cpp`,
  `DebugPanel.cpp`) that **stay in DC3**.
- DC3's web target uses `DC3_WEB_CORE_SOURCES` (lines 1209-1266), most of
  which are DC3-platform implementations (Memory/PlatformMgr/Synth_Stub
  etc.) that **belong to DC3 only** — only the web-specific subset moves
  to the engine (the table above).
- `DC3_WEB_GFX_SOURCES` (lines 1269-1283) has 11 sources besides
  `GpuDevice_Web.cpp`. Verify they already live in the engine before
  removing them from DC3's list; if any is still DC3-only it stays.
- `--use-port=emdawnwebgpu` is a compile-AND-link option in DC3
  (applied both at lines ~1343 and ~1369); carry that into the helper.
- **Engine has no `imgui` target under EMSCRIPTEN.** The current
  `if(MILO_ENGINE_HAVE_CONTEXT AND MILO_ENGINE_BUILD_GFX AND NOT EMSCRIPTEN)`
  block in `milo-native-engine/CMakeLists.txt` skips imgui entirely
  under EMSCRIPTEN. Either DC3 supplies one in its own CMake (trace
  before lifting), or this is a real gap. W0 adds an EMSCRIPTEN sibling
  to the engine.
- The engine's existing `MILO_BUILD_WEB` option string today reads "Build
  the Emscripten/web target machinery" — no "Phase 6" wording in the
  option itself, but the file's narrative comments still describe it as
  a placeholder. Update README to match.
- File path: DC3's webasset bundle endpoint uses `/data/` as the MEMFS
  mount root (`WebAssets.cpp` mkdirs `/data`). Keep that constant in the
  engine; RB3 will reuse it.
- **JS-side `dc3_*` debug exports** (`window.dc3CaptureAudio()`,
  `dc3_start_capture`, `dc3_download_capture`, `dc3_dump_sab`,
  `dc3_audio_stats`) are public ABI for any HTML/JS DC3 ships. After
  parameterising via `MILO_WEB_AUDIO_NS`, DC3 still gets `dc3_*`
  symbols (because DC3 sets the macro to `"dc3"`). No functional change
  if the macro is correctly set per-consumer.

## Open questions

- Move `dc3-decomp/native/web/server.py` and `index.html` to the engine
  too? Recommendation: **no** in W0 — they're frontends, parameterised
  by canvas id / module name / bundle root. W1 writes RB3's variant
  freshly and W4 can decide whether to factor a shared template.
- Move `dc3-decomp/scripts/web/smoke-test.mjs` into the engine?
  Recommendation: **no** in W0. Defer to W1 — RB3 will need its own
  variant and we'll see what parameterisation makes sense once two
  consumers exist.

## Suggested subagent prompt

> Execute Phase W0 of the RB3 web port plan
> (`rb3/docs/plans/web-port/W0_ENGINE_EXTRACTION.md`). Lift the engine-portable
> web infra from `dc3-decomp/native/` into `milo-native-engine/` under
> `MILO_BUILD_WEB`. Three of the lifted files (`GpuDevice_Web.cpp`,
> `AudioDevice_Web.cpp`, `ImGuiBackend_Web.cpp`) hardcode `dc3-canvas` /
> `dc3_*` strings — parameterise with `MILO_WEB_CANVAS_SELECTOR` and
> `MILO_WEB_AUDIO_NS` macros so DC3 sets `"dc3"` / `"#dc3-canvas"` and RB3
> can set its own. Stand up an EMSCRIPTEN sibling for the engine's `imgui`
> target (currently `NOT EMSCRIPTEN`-gated). Rename the C-side
> `DC3_WEB_ASYNCIFY` macro to `MILO_WEB_ASYNCIFY` (one-line edit to
> `main_web.cpp:150` — the only authorised change to that file). Acceptance
> test: `bash dc3-decomp/scripts/build/web.sh && python3
> dc3-decomp/native/web/server.py --port 8420 &` then `node
> dc3-decomp/scripts/web/smoke-test.mjs` from the DC3 repo — must report
> `result: pass` and a non-default canvas screenshot. Bump `MILO_ENGINE_PIN`
> in both DC3 and RB3 CMakeLists in the same change set after the engine PR
> merges; do not set `MILO_BUILD_WEB=ON` in the RB3 CMakeLists in this
> phase. Read the linked DC3 files before deleting anything to confirm
> DC3-specific sources stay in DC3 (`main_web.cpp`, `WebSvcMgr_Stub.cpp`,
> `DebugPanel.cpp`, `WebMovieImpl.cpp`, `DC3_WEB_CORE_SOURCES`).
