# Native Port Inventory — dc3-decomp/native/ → milo-native-engine vs per-decomp

**Source**: `/home/free/code/milohax/dc3-decomp/native/src/`
**Purpose**: Decide which dc3-decomp/native/ files move to the shared `milo-native-engine` repo vs stay per-game. Companion to [NATIVE_PORT_ROADMAP.md](NATIVE_PORT_ROADMAP.md).

This doc is the disposition catalog. The roadmap defines *when* moves happen and *why*; this doc defines *what* moves and *what shape* the moved file takes.

---

## Categorization framework

The cut isn't "native APIs vs engine code" — almost all of dc3's native/ is engine-level. The cut is **game coupling** AND **SDK coupling**.

| Disposition | Criteria |
|-------------|----------|
| **SHARED — engine, day 1** | No DC3-specific game headers. No Xbox/Win32 SDK header includes. No Kinect/XMA-only dependencies. Pure native-runtime glue or engine helpers that depend only on Milo's shared layer (`obj/`, `rndobj/`, `math/`, `synth/`, `char/`, etc.) and POSIX/clang. |
| **SHARED — engine, after factoring** | Engine-level intent, but the current DC3 source has one of: (a) game-specific includes (e.g. `Rnd_Wgpu.cpp` includes `hamobj/HamDirector.h`); (b) Xbox/Win32 SDK includes that need replacing with direct POSIX. Factor first, then move. The "factoring" step is concrete and called out per file. |
| **PER-DECOMP — native glue, stays in dc3-decomp/native/** | Depends on DC3 game classes (`hamobj/*`, `meta_ham/*`) or DC3 platform surface (XDK, Kinect, XMA, Xbox Live, Memcard, Dingo). RB3 gets its own equivalent if any is needed — usually smaller. |
| **N/A FOR RB3** | DC3-exclusive subsystem with no RB3 analog. Don't port at all (`xdk_shims.cpp` itself is DC3-only — RB3 needs `rvl_shims.cpp` instead; XMA codec, Kinect pose tracker). |

**One sentence rule**: if the file's includes go *only* through Milo's engine layer (`src/system/*` interface headers) and POSIX, and it doesn't pull in `hamobj/` or `meta_ham/` or `xdk/`, it ships in milo-native-engine. Otherwise it stays per-decomp or gets factored first.

---

## SHARED — engine, day 1

### Sub-directories that go wholesale

| Path | Notes |
|------|-------|
| `src/gfx/` | All WebGPU infra — device, pipeline manager, bloom, postproc, shadow, screenshot, vertex formats, mikktspace, ImGui backend, Metal surface, standard_wgsl.inc. **Zero game coupling, zero SDK coupling.** |
| `src/audio/` | AudioDevice (miniaudio integration) + AudioDevice_Web + miniaudio.h (vendored). |
| `src/char/` | CharTwistSolver — engine helper for outfit-only milo files. |
| `src/stl/` | STLport-shape header shims that redirect to host STL (`#include <utility>`, `#include <vector>`, etc.). **The STL seam.** Both decomps' matched stlport stays per-decomp. |
| `src/export/` | glTF/material/texture/bitmap exporters. Engine-clean. |
| `src/tools/` | milo2gltf, milo_mat_export, milo_tex_export CLI tools. Opt-in via `MILO_ENGINE_BUILD_TOOLS=ON`. |
| `src/viewer/` | Standalone milo viewer (separate CMake target). Opt-in via `MILO_ENGINE_BUILD_TOOLS=ON`. |
| `src/render_test/` | Programmatic render test (separate CMake target). |

### Top-level `src/` files

| File | Disposition | Why |
|------|-------------|-----|
| `main.cpp` | SHARED | wgpu-window-test — pure GPU smoke test, no engine deps. |
| `vectorintrinsics.h` | SHARED | Tiny math helper. |
| `msvc_compat.h` | PATTERN SHARED, CONTENT PER-DECOMP | Per-decomp file; the *concept* is the engine's contract ("each decomp provides a compat shim header that absorbs its compiler's quirks"). |
| `web_stubs.cpp` | PATTERN SHARED, CONTENT PER-DECOMP | Web build needs Itanium-mangled stubs; the symbol set is per-game. |
| `thunk_stubs.cpp` | PATTERN SHARED, CONTENT PER-DECOMP | Vtable thunks per-decomp; label addresses differ. |

### `platform/` — files that move into engine

These are file I/O, threading-via-pthread, input, FFmpeg, miniaudio, WebGPU glue, and stub implementations of features the native build doesn't need. The common thread: they implement the `src/system/os/*.h` and similar interface headers using only POSIX/clang/host libraries. No game classes, no Xbox/Wii SDK includes.

```
AsyncFile_Native.cpp           NativeSettings.h
BinkMovie_Stub.cpp             Net_Stub.cpp
BoneSetup.cpp/.h               NetworkSocket_Stub.cpp
Cache_Stub.cpp                 Part_Wgpu.cpp
Cam_Native.cpp                 PlatformMgr_Native.cpp
CDReader_Native.cpp            RenderState_Native.cpp
CDReader_Web.cpp               RndTex_Native.cpp
ChecksumData_Stub.cpp          Rnd_Stub.cpp
DataParser_Native.cpp          SampleInst_Native.cpp/.h
DebugPanel.cpp/.h              Skeleton_Native.cpp/.h
FFmpegAudioReader.cpp/.h       StreamReceiver_Native.cpp/.h
FFmpegMovieImpl.cpp/.h         Synth_Stub.cpp
File_Native.cpp                SynthCommon_Stub.cpp
File_Web.cpp                   TexGpu.h
FxSendNative.cpp/.h            Tex_Wgpu.cpp
GpuDevice_Web.cpp              TransformUtils.h
HttpServer.cpp/.h              TransparentQueue.cpp/.h
Joypad_Native.cpp              UiRenderHeuristics.h
Joypad_Stub.cpp                VirtualKeyboard_Stub.cpp
Keyboard_Native.cpp            WebAssets.cpp/.h
Keyboard_Stub.cpp              WebMovieImpl.cpp/.h
Keygen_Stub.cpp                WebSvcMgr_Stub.cpp
MapFile_Stub.cpp               
MaterialSetup.cpp/.h
MeshGpuCache.cpp/.h
Mesh_Wgpu.cpp
```

Per-file notes only where worth a comment:

| File | Note |
|------|------|
| `HttpServer.cpp/.h` | DC3-built embedded debug HTTP server. **Always built into `milo-engine`** — debug tooling is too painful to live without during bring-up. Both decomps enable. |
| `DebugPanel.cpp/.h` | ImGui debug panel. **Always built into `milo-engine`** — same reason. |
| `WebAssets.cpp/.h`, `WebMovieImpl.cpp/.h` | Web-build asset/movie loading. Game-agnostic. |
| `Keygen_Stub.cpp` | Saves-keygen stub. Verify on extraction that it doesn't reference DC3-specific save format. |
| `WebSvcMgr_Stub.cpp` | Web service manager — both games have one, native both no-op. |
| `BinkMovie_Stub.cpp` | Both games use Bink. Stub returns nothing. Shared. |
| `MaterialSetup.cpp/.h`, `TransparentQueue.cpp/.h`, `MeshGpuCache.cpp/.h`, `BoneSetup.cpp/.h` | Extracted helpers from `Mesh_Wgpu.cpp` to share between draw/shadow paths. Engine-clean. |
| `FxSendNative.cpp` | Synth FX chain via `synth/FxSendEQ.h` etc. — engine-clean. |

---

## SHARED — engine, after factoring

These files have engine-level intent but currently include game-specific or SDK-specific headers. Each gets a concrete factoring step before it moves.

| File | Coupling | Factoring step |
|------|----------|----------------|
| `platform/Rnd_Wgpu.cpp/.h` | Includes `hamobj/HamDirector.h`, `hamobj/HamCharacter.h`, `hamobj/HamGameData.h` for DC3-specific draw passes (HamDirector overlay, character render-to-texture loop). | Extract a `GameRenderHook` interface (engine-owned). DC3 supplies `HamRenderHook`, RB3 supplies `BandRenderHook`. The renderer calls into the hook for "draw your overlay" / "render impostors" without naming game types. The hook implementations stay per-decomp. **Done in Phase 0.2a.** |
| `platform/MeshFilter.cpp/.h` | Hardcoded skip-list for Kinect UI elements (player indicators, controller_mode.flow meshes). | Replace hardcoded `if (name == "kinect_player_indicator_X")` with a game-supplied skip list registered at startup. RB3 supplies its own (likely empty or band-specific). |
| `platform/System_Native.cpp` | Some inits assume DC3-shape (`TheLocale`, `TheHamUI` references; `Game::LoadSong` callbacks). | Audit `System_Native::Init`; factor any DC3-specific subsystem init calls into `dc3-decomp/native/src/dc3_system_init.cpp`. RB3 gets its own `rb3_system_init.cpp`. The engine version of `System_Native` only handles engine subsystems (Locale, Memory, ThreadCall, etc.). |
| `platform/Memory_Native.cpp` | `#include "xdk/XAPILIB.h"` for `PhysMemTypeTracker` type. | Replace the include with a forward declaration of the Memory.h type (`PhysMemTypeTracker` is defined in shared `src/system/os/Memory.h`). DC3's matched fork keeps the type definition; the engine version's `.cpp` provides the no-op POSIX impls of `PhysicalFree`, `PhysicalUsage`, etc. without dragging in the XDK header. |
| `platform/ThreadCall_Native.cpp` | `#include "xdk/XAPILIB.h"` for Win32 `HANDLE`, `WaitForSingleObject`, etc. The body uses these types to mimic Win32-shape thread synchronization. | Rewrite the body to use pthread directly without the Win32 type aliases. The engine version implements `os/ThreadCall.h` interface using `pthread_create` / `sem_t` / etc., no Win32 type names anywhere. DC3's matched fork's `os/ThreadCall_Xbox.cpp` still uses Win32 types and is excluded from the native link. |
| `platform/ContentMgr_Stub.cpp` | DLC catalog stub. Both games have DLC but different shapes (DC3 dance packs, RB3 song packs). | Inspect — if it's truly a no-op stub, share. If it references DC3 content types (`HamContent`, etc.), factor those out into per-decomp content stubs. |
| `platform/Achievements_Stub.cpp` | Both games have achievements; DC3 has Kinect-pose-specific ones. | Share as a no-op base; per-game achievement registration stays per-decomp. |

These get factored **only when RB3's port actually exercises them and we can see both call shapes**. Don't refactor speculatively — wait until the second concrete consumer is in front of you. The factoring milestones:

- **0.2a** (Rnd_Wgpu hook) — must happen before Phase 2 rendering work.
- **0.2b** (Win32 shim removal for Memory_Native + ThreadCall_Native) — must happen before Phase 0 closes; the engine cannot ship native impls of `os/` interfaces if they transitively pull in the XDK.
- **System_Native, ContentMgr, Achievements, MeshFilter** — factor as RB3 needs them in Phases 1–4.

---

## PER-DECOMP — stays in dc3-decomp/native/

DC3-specific files. RB3 either doesn't need them or needs a different file in `rb3/native/src/`.

| File | Why DC3-specific |
|------|------------------|
| `src/main_native.cpp` | DC3 desktop entry. RB3 needs its own (similar shape, different `App` ctor). |
| `src/main_web.cpp` | DC3 web entry. RB3 needs its own (Phase 6). |
| `src/native_link_glue.cpp` | DC3-typed `ObjRefConcrete<T>::CopyRef` instantiations across DC3's entire type set. RB3 has different types — it gets its own. |
| `src/native_job_stubs.cpp` | Xbox Live job marketplace stubs (`SingleItemEnumJob`, `_XMMATRIX`). Xbox-only API. |
| `src/engine_stubs_generated.cpp` | Auto-generated stubs for missing DC3 symbols. RB3 generates its own from its symbol table. |
| `src/msvc_compat.h` | MSVC compat shim. RB3 has `mwcc_compat.h` instead. |
| `src/web_stubs.cpp` | Itanium-mangled WASM stubs — DC3's symbol set. RB3 makes its own. |
| `src/thunk_stubs.cpp` | DC3 vtable thunks for label-named .rodata entries (`lbl_82066608`). RB3 has different label addresses. |
| `src/xdk_shims.cpp` | DC3-specific. RB3's analog is `rb3/native/src/rvl_shims.cpp`. |
| `platform/DingoSvr_Native.cpp` | Dingo = Xbox 360 dev server. Xbox-only. |
| `platform/GestureMgr_Native.cpp` | Kinect gesture manager. DC3-only. |
| `platform/KinectShare_Stub.cpp` | Kinect photo-share. DC3-only. |
| `platform/MemcardMgr_Stub.cpp`, `Memcard_Stub.cpp` | Xbox 360 memory card. Xbox-only — RB3 uses Wii NAND. |
| `platform/NetXbox_Stub.cpp` | Xbox Live networking. Xbox-only. |
| `platform/SongSortMgr_Native.cpp` | Includes `meta_ham/SongSortMgr.h`. DC3-specific. RB3 has its own SongSortMgr in `meta_band/`. |
| `telemetry/GameplayTelemetry.cpp/.h` | Includes `hamobj/HamDirector.h`, `hamobj/HamWardrobe.h`, `hamobj/HamCharacter.h`, `hamobj/HamDriver.h`, `hamobj/ClipPlayer.h`. Entirely DC3-shaped. RB3 will want its own against `band3/` classes — or skip telemetry in v1. |

---

## N/A — don't port at all

| Path | Why skip |
|------|----------|
| `src/pose/` (entire dir) + `third_party/ncnn/` | ncnn-based YOLO pose estimation for Kinect-replacement gesture input. Dance Central-only — RB3 has no body-tracking input. |
| `platform/XmaSampleDecoder.cpp/.h` | Xbox 360 XMA audio codec. RB3 uses MOGG/Vorbis only — already covered by `FFmpegAudioReader`. |

`src/xdk_shims.cpp` is listed under PER-DECOMP rather than N/A because RB3 needs the *analog* (`rvl_shims.cpp`), even though the file itself doesn't port.

---

## What RB3 must write from scratch (per-decomp)

When `rb3/native/` is created, these files won't exist in the dc3 source and need writing. Each is small and follows a known pattern.

| File | Surface to spec | Notes |
|------|-----------------|-------|
| `rb3/native/src/main_native.cpp` | Mirrors `dc3-decomp/native/src/main_native.cpp`. Constructs `App`, runs main loop. | Trivially small. |
| `rb3/native/src/main_web.cpp` | Mirrors `dc3-decomp/native/src/main_web.cpp`. Emscripten main-loop state machine. | Phase 6. |
| `rb3/native/src/mwcc_compat.h` | `__alloca` fallback (call `alloca()` directly); `#pragma` no-op stubs for clang where MWCC pragmas appear in headers reached by native build. | Smaller than DC3's `msvc_compat.h` because clang and MWCC agree on more than clang and MSVC do. Start small; grow as Phase 1 surfaces missing pieces. |
| `rb3/native/src/rvl_shims.cpp` | POSIX implementations for Wii SDK calls made by RB3 matched fork **outside `src/system/os/`**. Discovery: `grep -rln 'revolution/' src/system \| grep -v '/os/'`. Initial known surface: `Splash.cpp` (splash screen flow), `OutfitConfig.cpp`, `StorePackedMetadata.cpp`, `PostProc.h`. Map each call to POSIX: timer → `clock_gettime`, mutex → `pthread_mutex_t`, thread → `pthread_t`, memory → `malloc`. | Analog of DC3's `xdk_shims.cpp`. Surface should be much smaller (RB3 only directly calls into Wii SDK from a handful of non-`os/` files). |
| `rb3/native/src/native_link_glue.cpp` | Walk RB3's type set, instantiate `ObjRefConcrete<T>::CopyRef` for each. | Pattern from `dc3-decomp/native/src/native_link_glue.cpp` carries directly; just substitute RB3's types. |
| `rb3/native/src/engine_stubs_generated.cpp` | Weak stubs for symbols the engine link wants but RB3 hasn't decomp'd yet. | Auto-generated; port dc3's generator script. |
| `rb3/native/src/rb3_system_init.cpp` | RB3-side `System_Native::Init` extensions — TheLocale init, UI manager init, RB3-specific subsystem startup. | Mirror DC3's pattern after Phase 0.2 System_Native factoring. |
| `rb3/native/src/web_stubs.cpp` | Per-symbol Itanium stubs for Web build — RB3 symbol set. | Phase 6. |
| `rb3/native/src/thunk_stubs.cpp` | RB3 vtable thunks if any are needed (label addresses differ from DC3). | Trivially small; may not be needed at all. |
| `rb3/native/src/rb3_render_hook.cpp` | Implements engine's `GameRenderHook` interface: RB3 HUD overlay pass, RB3-specific render-to-texture (if any). | Created in Phase 0.4 as a no-op stub; grows during Phases 2 and 5. |
| `rb3/native/src/rb3_input_map.cpp` | Maps SDL/keyboard events to RB3 controller buttons (guitar fret/strum, drum pads + kick, vocal mic, keys, pro-guitar fret). | Phase 4a. |

---

## Engine layout

Preserve dc3's flat layout to minimize day-1 churn. Reshape later if pain points emerge.

```
milo-native-engine/
├── CMakeLists.txt              (engine-only; per-game source lists live in each decomp)
├── README.md
├── cmake/                      (helper modules)
├── include/                    (3rd-party single-headers: bits, cgltf, stb, httplib)
├── third_party/                (vendored libs: miniaudio.h) — NO ncnn
├── src/
│   ├── gfx/                    ← from dc3/native/src/gfx/        (verbatim)
│   ├── audio/                  ← from dc3/native/src/audio/      (verbatim)
│   ├── platform/               ← from dc3/native/src/platform/   (filtered: drop DC3-specific files, factor the 7 cleanup files)
│   ├── char/                   ← from dc3/native/src/char/       (verbatim)
│   ├── stl/                    ← from dc3/native/src/stl/        (verbatim — the host-STL shim layer)
│   ├── system/                 ← NEW. Clean LP64 impls of os/ThreadCall.h, os/CritSec.h, os/File.h, etc.
│   │                              that replace DC3's xdk-dependent native files.
│   ├── export/                 ← from dc3/native/src/export/     (verbatim, opt-in tool target)
│   ├── tools/                  ← from dc3/native/src/tools/      (verbatim, opt-in tool targets)
│   ├── viewer/                 ← from dc3/native/src/viewer/     (verbatim, opt-in tool target)
│   └── render_test/            ← from dc3/native/src/render_test/ (verbatim, opt-in test target)
├── tests/                      ← from dc3/native/tests/ (engine-only subset; see roadmap §0.2)
└── docs/
```

---

## Build-system extraction plan

DC3's existing `native/CMakeLists.txt` is heavily intertwined: per-game source lists, per-game include paths, per-game include-directory ordering all in one file. Extraction approach:

### Engine `CMakeLists.txt` (new, in milo-native-engine/)

Owns:
- Compiler detection (clang-only) and the MSVC compat flags for any matched-fork code that consumers compile against engine headers. (Note: the engine's own .cpp files don't need MSVC compat flags; only the per-decomp matched-fork sources do.)
- LP64 audit + ASan options.
- Third-party deps (Dawn/WebGPU, miniaudio, FFmpeg detection, ImGui).
- Engine source globs for `src/gfx/`, `src/audio/`, `src/platform/`, `src/char/`, `src/stl/`, `src/system/`.
- The `milo-engine` static lib target.
- Opt-in tool targets (`milo-viewer`, `milo2gltf`, `render-test`) behind `MILO_ENGINE_BUILD_TOOLS=ON`.
- Opt-in web target machinery behind `MILO_BUILD_WEB=ON`.
- The `milo-engine-tests` target with the engine-only test subset.

Does NOT own:
- Any reference to `lazer/`, `hamobj/`, `dance/`, `band3/`, `meta_ham/`, `meta_band/`, etc. The engine knows nothing about either game.
- Game executable targets. Each decomp's CMake adds those.

### Per-decomp top-level `CMakeLists.txt` (one per decomp)

Each decomp's `CMakeLists.txt` does roughly:

```cmake
# Engine is a sibling repo at a configurable path, not a submodule.
set(MILO_ENGINE_PATH "${CMAKE_SOURCE_DIR}/../milo-native-engine"
    CACHE PATH "Path to milo-native-engine checkout")
set(MILO_ENGINE_PIN  "<commit-sha>"
    CACHE STRING "Engine commit SHA verified against")

# Soft pin: warn on mismatch, don't fail.
if(EXISTS "${MILO_ENGINE_PATH}/.git")
    execute_process(COMMAND git rev-parse HEAD
                    WORKING_DIRECTORY ${MILO_ENGINE_PATH}
                    OUTPUT_VARIABLE engine_actual
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT engine_actual MATCHES "^${MILO_ENGINE_PIN}")
        message(WARNING
            "milo-native-engine HEAD is ${engine_actual}\n"
            "  but ${PROJECT_NAME} pins ${MILO_ENGINE_PIN}\n"
            "  (set MILO_ENGINE_PIN or update via scripts/bump-engine.sh)")
    endif()
endif()

add_subdirectory(${MILO_ENGINE_PATH} ${CMAKE_BINARY_DIR}/milo-engine)

# Per-game matched-fork source lists, native glue, link glue.
set(RB3_MATCHED_SOURCES ...)         # src/system/**, src/band3/**
set(RB3_NATIVE_GLUE_SOURCES ...)     # native/src/main_native.cpp, native/src/rvl_shims.cpp, etc.

add_executable(rb3-native
    ${RB3_NATIVE_GLUE_SOURCES}
    ${RB3_MATCHED_SOURCES})
target_link_libraries(rb3-native PRIVATE milo-engine)
target_include_directories(rb3-native PRIVATE
    ${CMAKE_SOURCE_DIR}/native/src        # native glue takes priority (STL shims, compat headers)
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/system)
target_compile_options(rb3-native PRIVATE
    ${RB3_MWCC_COMPAT_FLAGS})            # whatever clang needs to swallow the matched fork
target_compile_definitions(rb3-native PRIVATE HX_NATIVE=1)
```

The pattern: **engine knows nothing about games; each decomp's CMake assembles its own executable by linking the engine + its own matched fork + its own native glue**. There is no `MILO_NATIVE_GAME=DC3|RB3` switch inside the engine's CMake — the engine is game-agnostic.

### Helper: `scripts/bump-engine.sh` (one per decomp)

Re-runs the engine's tests against the local checkout, then updates `MILO_ENGINE_PIN` in this decomp's CMakeLists and commits. Engine tests run via `cmake --build build --target milo-engine-tests && ctest`.

### Why this shape

- **No submodule**: engine is at a known sibling path, but not embedded. Easy to swap to a branch checkout.
- **Reproducible**: pin SHA lives in each decomp's git history. Checking out an old decomp commit tells you which engine SHA was known to work.
- **Iteratable**: edit engine code, save, run `cmake --build` in the decomp — done. No install dance.
- **Debuggable**: source paths are real, breakpoints work, no path-mapping.
- **Soft pin**: a mismatched engine checkout produces a warning, not an error. Test engine branches by checking them out.
- **Game-agnostic engine**: the engine's CMake is reusable for a hypothetical third Milo decomp without touching engine code.
- **Reversible**: switching to FetchContent or a static-lib install package later is a small CMake diff, not a refactor.

### Engine library shape

Single `milo-engine` static library. All engine .cpp files compile into one `libmilo-engine.a`. No subsystem splits (`milo-engine-gfx`, etc.) initially. Revisit if link times degrade past acceptable.

Tool targets (`milo-viewer`, `milo2gltf`, `render-test`) are separate executables in the engine repo, opt-in via `MILO_ENGINE_BUILD_TOOLS=ON`.

---

## Phased rollout

This inventory drives the Phase 0 deliverables in [NATIVE_PORT_ROADMAP.md](NATIVE_PORT_ROADMAP.md). The phases are reproduced here for cross-reference; the roadmap is authoritative.

1. **Phase 0.1** — Stand up `milo-native-engine` repo. CMake scaffolding, empty `src/` tree, README. dc3-decomp **unchanged**.
2. **Phase 0.2** — Move SHARED files into engine. Land `0.2a` (Rnd_Wgpu hook factoring) + `0.2b` (Win32 shim removal in Memory_Native + ThreadCall_Native). Update dc3-decomp's CMake to consume the engine. **Convergence test**: dc3-decomp's engine-only test subset still passes via `milo-engine-tests` target in the engine repo, plus dc3-only tests still pass in dc3-decomp.
3. **Phase 0.3** — Bring up `rb3/native/` skeleton consuming engine. First link of `rb3-native` executable, even if it exits immediately on first frame.
4. **Phase 0.4** — CI smoke for all three repos.
5. **Phase 1+** — As RB3 brings up subsystems, the remaining "cleanup" files (System_Native, ContentMgr, MeshFilter, Achievements) get factored *when RB3 actually exercises them*, not speculatively.

---

## Resolved questions

| # | Question | Decision |
|---|----------|----------|
| 1 | Submodule vs sibling vs find_package? | **Sibling repo + `add_subdirectory()` + soft SHA pin.** Engine at `../milo-native-engine`, pin lives in each decomp's CMakeLists, mismatch is a warning. |
| 2 | Single static lib vs split? | **Single `milo-engine` static lib.** No subsystem splits; revisit if needed. |
| 3 | HttpServer + DebugPanel built-in or off? | **Built in by default.** Both decomps enable. Compiled into `milo-engine` unconditionally; no CMake flag to disable. |
| 4 | Tool targets default-on or opt-in? | **Opt-in via `MILO_ENGINE_BUILD_TOOLS=ON`** option. Off by default. |
| 5 | Does the engine know about per-game source lists? | **No.** The engine is game-agnostic. Each decomp's CMakeLists adds the engine via `add_subdirectory()` and assembles its own executable target with its own matched-fork + native-glue source lists. No `MILO_NATIVE_GAME` switch in engine. |
| 6 | How does the SDK shim story differ between decomps? | **Per-decomp.** DC3 ships `xdk_shims.cpp` for Win32 calls its matched fork makes; RB3 ships `rvl_shims.cpp` for Wii SDK calls. The engine sees only POSIX directly and never includes either SDK header. The engine's `Memory_Native.cpp`/`ThreadCall_Native.cpp` are rewritten in Phase 0.2b to drop their current `xdk/` includes. |
| 7 | What's the STL story for the engine? | **Engine ships the shim layer (`src/stl/_*.h`).** Both decomps keep `src/system/stlport/` forked for asm-match indefinitely. Native build's include-path priority routes to host clang STL via the shim. Convergence is automatic — no third copy. |
| 8 | What's the convergence test for Phase 0.2? | **The engine-only test subset.** Game-agnostic tests (binstream, chunkstream, dirloader, dta_parser, asset_loading, mesh_loading, archive_enumeration, charbones_serialization, charclipgroup, bone_ground_truth, object_lifetime, rndcam_projection, mogg_decode, mogg_v0xe, extract_bik) move into the engine repo as `milo-engine-tests` and run against `libmilo-engine.a`. Game-coupled tests stay per-decomp. Phase 0.2 passes when both `milo-engine-tests` (in engine) and dc3-decomp's remaining game-coupled tests pass after the move. |
| 9 | What MWCC patterns must `mwcc_compat.h` handle? | **`__alloca` intrinsic** (fallback to `alloca()`), and `#pragma` no-op gates for `pool_data`, `dont_inline`, `fp_contract`, `force_active`, `aux` that clang doesn't recognize. PowerPC inline asm blocks in `math/Vec.h`, `math/Mtx.h`, `math/Geo.cpp`, `math/Rot.cpp`, `bandobj/BandIKEffector.cpp`, `bandobj/InlineHelp.cpp`, `char/CharForeTwist.cpp`, `char/CharHair.cpp`, `rndobj/Part.cpp` need `#ifdef __MWERKS__ / #else <C++> / #endif` wraps in their respective files (not in mwcc_compat.h). |

When new questions arise or these are revisited, append to the Decision log in the roadmap doc.
