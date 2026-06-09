# RB3 Native Port — Roadmap & Status

**Status**: Phase 0 **COMPLETE**. Phase 1 **COMPLETE**. Steps 2 (headless DTA boot) + 3 (rendering) **COMPLETE**. **Boot-to-song milestone ACHIEVED (2026-05-27).** **v1 ACHIEVED** — `RB3_GAME=1 rb3-native` plays "20th Century Boy" end-to-end on Linux: tv3 transition cinematic → in-song gameplay with venue + band + crowd rendering, `Game::mLoadState = kReady`, score HUD ticking. Tv3 cinematic landed at `a316502c` + `acdfc69f`; `InterstitialPanel::Exiting` HX_NATIVE wrapper deleted (retail parity) at `aab19da5`. Song-end native fix for long-run reliability at `6cfb0a7d` (real `NetSession::EndGame` + `IsInGame` semantics so `CanEndGame` no longer blocks `TrulyWinGame` / `GameEndedMsg`). All regression guards remain GREEN (`RB3_BOOT` 227, `RB3_RENDER_MESH` 129 meshes/27878 tris, `rb3-dta` 138 songs). See `docs/sessions/native/V1_ONE_SONG.md` and the session log below for the full per-layer fix list.
**v1 milestone**: Play one song end-to-end (audio + venue + HUD + scoring) on Linux x86_64. **DONE.**

This doc is the durable tracking artifact for the RB3 native port. Sessions append to the Status Log at the bottom and adjust phase tables as items move. Companion: [NATIVE_PORT_INVENTORY.md](NATIVE_PORT_INVENTORY.md) (per-area disposition of DC3's existing native code).

---

## Critical path

**Where we are (2026-05-29):** Steps 1-5 of the original critical path are
**COMPLETE**. The real game `App` boots end-to-end on Linux and plays
"20th Century Boy" through to gameplay with rendering + audio + scoring.
The **tv3 transition cinematic** plays the full 3-sub-shot authored montage
before the in-song view, and the `InterstitialPanel::Exiting` HX_NATIVE
wrapper was unwrapped (retail parity) once tv3 default-on was established.
See the [Status log](#status-log) for the full per-session fix list, and
[`docs/sessions/native/V1_ONE_SONG.md`](../sessions/native/V1_ONE_SONG.md)
for the v1 task graph + per-layer detail.

| Step | Milestone | Status |
|------|-----------|--------|
| **1** | Full object-graph load (Phase 1) — rndobj/synth clang-LP64-clean, factories register, real `DirLoader::LoadObjects` instantiates the live tree | **DONE** (2026-05-27) |
| **2** | Headless DTA boot — `SystemPreInit`+`SystemInit` load the real config (227 obj-cfg + ui) | **DONE** (2026-05-27) |
| **3** | Rendering bring-up (Phase 2) — `BandRnd : Rnd` Strategy B renders RB3 geometry headless (`RB3_RENDER_MESH` = 129 meshes/27878 tris/PNG) | **DONE** (2026-05-27) |
| **4** | **Boot-to-song** — real `App`, synthetic input, full screen flow: splash → main_hub → song_select → select song → `Game::LoadSong()` | **DONE** (2026-05-27) |
| **5** | **v1 — one song end-to-end** — audio playback + gem track + scoring on Linux x86_64 | **DONE** — assets at `orig-assets/extracted-xbox-full/`, audio backend via `rb3_synth_native.cpp` + `rb3_stream_receiver_native.cpp`, tv3 cinematic + retail-parity `InterstitialPanel`, song-end fix `6cfb0a7d`. Tracker: [`docs/sessions/native/V1_ONE_SONG.md`](../sessions/native/V1_ONE_SONG.md). |

Per-phase acceptance criteria are below; open regressions are tracked under
[Known issues](#known-issues--tracked-regressions).

### Critical path to v1 (one song end-to-end) — HISTORICAL

v1 was achieved by extracting the Xbox 360 full ARK to
`orig-assets/extracted-xbox-full/` (85 songs, 83 .mogg, 635 .mid) plus
`SetUsingCD(true)` in `os/System.cpp` (HX_NATIVE) so `config/foo.dta` rewrites
to `config/gen/foo.dtb`. Audio backend resolved by writing an RB3-shaped
`StreamReceiverNative` (derives from both RB3's `StreamReceiver` AND the
engine's `AudioSource`) + a `NativeSynth : Synth`. See
[`V1_ONE_SONG.md`](../sessions/native/V1_ONE_SONG.md) for the X1-X6 task list
and the V2-V9 status table.

Post-v1 polish (NOT v1 gates):

| Task | Description |
|------|-------------|
| V3 | Bring up residual long-tail gameplay TUs clang-LP64-clean (`Singer`, `VocalPlayer`/`VocalNoteList`, `BandPatchMesh`, `TourPerformerLocal`, …) for fuller band animation / results screens |
| V4 | Venue-character `Draw()` crash fix (`Character::DrawLodOrShadow`→`RndMesh::SetUpdateApproxLight` — Phase 2 RB3-specific render path; currently sigsetjmp-absorbed) |
| Visual | Texture sampling in some paths still diffuse=white; skinning uses identity bone palette |

---

## Goal & non-goals

**Goal**: A native, cross-platform port of Rock Band 3 (Wii build SZBE69_B8 as the source of truth) that runs on Linux x86_64, macOS (arm64 + x86_64), and Web (Emscripten). Single-player only for v1.

**Non-goals**:
- 100% asm-match in the *native* runtime. Asm-match remains the verification standard inside the decomp repo, but the native build is a clean LP64 modern-C++ target. Match% proves the source is faithful; it is not the deliverable.
- Online multiplayer in v1. `src/network/` and Wii DWC/WFC stacks are out of scope. Local multiplayer is deferred too — focus on single-player polish first.
- Windows in v1. Add later if needed.
- Rewriting decomp work in `src/sdk/`, `src/system/rndwii/`, or `src/system/os/`. These are replaced wholesale by the host platform; matching them teaches nothing about porting.

---

## Architecture

Three repos. The deliverable lives in **milo-native-engine**; the matched decomps remain authoritative for verification.

```
/home/free/code/milohax/
├── milo-native-engine/    ← NEW. Shared LP64 modern-C++ engine + native glue.
│                            Both decomps depend on it for the runtime build.
│                            Owns: gfx (WebGPU), audio (miniaudio/FFmpeg), input,
│                            CMake scaffolding, host-STL shim layer, native impls
│                            of os/ interfaces, and engine-only unit tests.
│
├── dc3-decomp/            ← DC3 source of truth. Keeps its matched src/system/
│                            fork for asm-match verification against Xbox 360.
│                            Native build links against milo-native-engine and
│                            supplies its own MSVC compat shim (msvc_compat.h)
│                            plus Win32-API shim (xdk_shims.cpp).
│
└── rb3/                   ← THIS REPO. Keeps its matched src/system/ fork for
                             asm-match verification against Wii MWCC. Native
                             build (new: rb3/native/) links against
                             milo-native-engine. Supplies MWCC compat shim
                             (mwcc_compat.h) plus Wii-SDK shim (rvl_shims.cpp).
                             Owns src/band3/ (RB3 game layer).
```

### Three layers of source ownership

The plan has three categories of source, not two. Be explicit about which is which when discussing any file:

| Layer | Owned by | Compiles under | Purpose |
|-------|----------|----------------|---------|
| **Matched fork** (`src/system/*.cpp` in each decomp) | Per-decomp | MWCC (RB3) / MSVC PPC (DC3) | Asm-match verification. Off the native link path. |
| **Engine runtime** (`milo-native-engine/src/**`) | Shared repo | Clang LP64 only | The deliverable. Linked by both decomps' native builds. |
| **Per-decomp native glue** (`rb3/native/src/**`, `dc3-decomp/native/src/**`) | Per-decomp | Clang LP64 only | Compat shims, link glue, game-specific stubs that depend on per-game types. |

The matched fork and the engine runtime are *separate copies* that drift independently. The matched fork must compile under its original toolchain; the engine runtime is freed from that constraint and stays clean LP64 modern C++. They are never merged into one shared file — see the next section for why the previous "hybrid src/system" idea was wrong.

### Why `src/system` does not converge across the three layers

Earlier drafts proposed "hoisting" a matched `src/system/foo.cpp` into the engine once both decomps reached AT_LIMIT. After auditing DC3's existing native work this turns out to be the wrong shape:

- DC3 has ~865 `HX_NATIVE` ifdef blocks across ~295 source files in its matched fork. The blocks are the patches that make the MSVC PPC source compile and behave correctly under clang LP64. They are not a transitional state — they are the matched fork's permanent native compatibility layer.
- RB3 today has zero `HX_NATIVE` blocks. Phase 1 lands them.
- An asm-matched `.cpp` and its clang-LP64-clean cousin are two different files with different correctness criteria. Merging them creates a third hybrid that satisfies neither.

So the engine's `src/system/` (when it grows beyond glue) is a *new* clean LP64 implementation written against the same `os/`, `obj/`, `rndobj/`, `math/` interfaces as the decomp forks. The decomp `src/system/` files compile under their compiler for asm-match and **do not appear on the native link line at all**. The native build link rule:

- Native link = `milo-native-engine/src/**` (engine) + `rb3/native/src/**` (per-game glue) + `rb3/src/band3/**` (RB3 game logic) + selected `rb3/src/system/**` files that already work under clang via `HX_NATIVE` gating.
- Decomp link = `rb3/src/**` only (no engine, no native glue).

The two builds share *header* interfaces (e.g. `os/ThreadCall.h`) but compile separate implementation .cpp files. New engine `.cpp` files only appear in `milo-native-engine/src/system/` when there's a concrete reason — almost always "the matched fork's HX_NATIVE branch became larger and more cross-game-relevant than the matched logic, so move the native logic to the engine and let the matched fork keep only its asm-match path."

### Compiler-quirk gating model

The shared engine targets **vanilla Clang LP64 C++17**. The matched forks carry per-compiler quirks. We gate explicitly so both can coexist:

- `#ifdef HX_NATIVE` — code that runs on the clang LP64 native build. Lives in the matched fork's headers (e.g. `Data.h` ctor zero-init for the 8-byte union) and source. Both decomps adopt the same convention.
- `#ifdef __MWERKS__` — MWCC-only code. Inline `asm { psq_l ... }` blocks in `src/system/math/Vec.h`, `#pragma dont_inline`, `#pragma pool_data`, `__alloca`. Lives in RB3's matched fork. Native build must compile when this gate is off.
- `#ifdef _MSC_VER` — MSVC-only code in DC3's matched fork. Native build must compile when this gate is off.
- `#ifdef __EMSCRIPTEN__` — web-build branch within native code.

Per-decomp compat shims (`mwcc_compat.h`, `msvc_compat.h`) absorb the rest. RB3's `mwcc_compat.h` is expected to be much thinner than DC3's `msvc_compat.h` because clang and MWCC agree on more than clang and MSVC do.

### STL ABI seam (resolved)

Both decomps use **STLport** for their matched STL. The native build never sees the stlport headers. Mechanism, copied from DC3:

1. Each decomp ships `src/system/stlport/` for its matched build (`<vector>`, `<map>`, etc. resolve to STLport).
2. The engine ships a shim layer (e.g. `milo-native-engine/src/stl/_vector.h` containing only `#include <vector>`) that maps STLport-shape internal-header includes to host STL.
3. The native build's include path puts the engine shim layer *before* `src/system/stlport/`, so host clang STL wins. The matched build doesn't see the shim layer.
4. Common Milo container helpers in `utl/Symbol.h`, `utl/Str.h`, etc. that reach into STL internals get gated with `#ifdef HX_NATIVE` to use the host-STL shape instead.

There is no third copy of stlport in the engine. Both decomps' stlport stays forked for asm-match indefinitely.

### Per-decomp OS-API shim (the xdk/rvl story)

The matched forks call into their platform's SDK directly:
- DC3 source uses Win32 types from `xdk/XAPILIB.h`: `HANDLE`, `WaitForSingleObject`, `RTL_CRITICAL_SECTION`, etc.
- RB3 source uses Wii types from `revolution/OS.h`: `OSMutex`, `OSThread`, `OSCreateThread`, `OSGetCurrentThread`, etc.

For the native build, each decomp ships an SDK shim that maps its platform's calls to POSIX:
- DC3's `native/src/xdk_shims.cpp` (exists) — POSIX implementations of the Win32 API surface DC3's matched fork happens to call.
- RB3's `native/src/rvl_shims.cpp` (new) — POSIX implementations of the Wii SDK surface RB3's matched fork happens to call. Wii thread/mutex/event calls map onto pthread; Wii filesystem calls map onto stdio; Wii timer calls map onto `clock_gettime`.

The shims are per-decomp because the *which* of the API surface is per-decomp. They are not engine code. The engine sees only the higher-level interfaces (`os/ThreadCall.h`, `os/CritSec.h`, `os/File.h`) and ships *its own* POSIX implementations of those interfaces — `ThreadCall_Native.cpp` etc. — which never call into the SDK shim.

Concretely: the matched fork's `src/system/os/ThreadCall_Wii.cpp` (which calls `OSCreateThread`) is excluded from the native link. The engine's `ThreadCall_Native.cpp` (which calls `pthread_create`) takes its place. The `rvl_shims.cpp` exists for the much smaller surface that *non-os* matched fork code happens to call directly into Wii SDK headers for — Splash, OutfitConfig, StorePackedMetadata, etc.

---

## DC3 → RB3 differences to handle

DC3's native port is mature (boot-to-gameplay, audio playing, post-processing, crowd billboards). RB3 differs in ways that matter:

| Area | DC3 (source) | RB3 (this port) | Impact |
|------|--------------|-----------------|--------|
| Original platform | Xbox 360 (PPC Xenon, big-endian, ILP32) | Wii (PPC Gekko/Broadway, big-endian, ILP32) | Endianness + word-size assumptions match — most LP64 lessons transfer 1:1 |
| Original compiler | MSVC PPC | MetroWorks CodeWarrior 4.3.172 | Different mangling, different STL (MSVC vs STLport), different `__declspec` vs MWCC pragmas. Per-decomp shims, not engine concern |
| Original STL | MSVC STL | STLport on Wii | Matched-fork ABI differs; native build resolves to host STL via shim path priority (see STL seam above) |
| SDK shim | `xdk_shims.cpp` (Win32 → POSIX) exists | `rvl_shims.cpp` (Wii SDK → POSIX) to be written | Same pattern, different surface. Per-decomp |
| MWCC inline assembly | None | PowerPC `asm { psq_l ... }` blocks in `math/Vec.h`, `math/Mtx.h`, `math/Geo.cpp`, `math/Rot.cpp`, `bandobj/BandIKEffector.cpp`, `char/CharForeTwist.cpp`, `char/CharHair.cpp`, `bandobj/InlineHelp.cpp`, `rndobj/Part.cpp` | Each asm block needs a C++ fallback gated by `#ifndef __MWERKS__`. List is small and known |
| MWCC pragmas | None | `#pragma dont_inline`, `#pragma pool_data`, `#pragma fp_contract`, `#pragma force_active` scattered across matched fork | No runtime semantic; gate with `#ifdef __MWERKS__` or `#pragma` no-ops under clang |
| MWCC intrinsics | None | `__alloca` (rare) | Provide clang `alloca` fallback in `mwcc_compat.h` |
| Symbol info | None (Ghidra-inferred) | Full DWARF in debug ELF | RB3 has much richer type info; field layouts more reliable |
| Milo version | Newer (DC3 ≈ 2012) | Older (RB3 ≈ 2010) | DC3 patterns generally apply forward; RB3 may lack newer APIs. Carry-forward is one-way DC3 → RB3 |
| Game layer | `src/dance/` + `lazer/meta_ham/` | `src/band3/` (bandtrack, game, meta_band, tour) | Entirely separate. RB3 game logic — GemPlayer, scoring, instruments, song HUD — has no DC3 equivalent. Decomp-matched source already exists; needs native bring-up |
| Renderer source | Xbox D3D9 abstractions in `rndxbox/` | Wii GX in `system/rndwii/` | Both replaced by WebGPU. DC3's `Rnd_Wgpu.cpp`/`Tex_Wgpu.cpp`/`Mesh_Wgpu.cpp`/`Part_Wgpu.cpp` carry over directly |
| Audio source | XMA (Xbox proprietary) + MOGG/Vorbis | Wii audio paths + MOGG/Vorbis | Both replaced by miniaudio + FFmpeg/Vorbis. DC3's VorbisReader native impl carries directly. XMA codec dropped — RB3 doesn't need it |
| Multiplayer | Disabled in DC3 native | Out of scope for v1 | No cross-port concern |
| Asset packaging | Xbox `.ark`/`.hdr` extracted to `orig-assets/` | Wii `.ark` + `band_r_wii.sel` in `orig/SZBE69_B8/files/` | Asset loader (`ArkFile`) is mostly shared; the per-game extract step differs |
| Songs/DLC | DC3 has on-disc + DLC validation hooks | RB3 has its own on-disc setlist + DLC system | RB3-specific. Hook into `src/band3/meta_band/` |

### What RB3 gets for free from DC3

In priority order:

1. **All of `native/src/gfx/`** — WebGPU device, pipeline manager, post-processing (bloom, contrast, levels, vignette, chromatic, posterization), shadow pass, screenshot, video encode. Direct copy into engine.
2. **All of `native/src/audio/`** — miniaudio device, web audio adapter. Direct copy into engine.
3. **Renderer glue in `native/src/platform/`** — `Mesh_Wgpu.cpp`, `Tex_Wgpu.cpp`, `Rnd_Wgpu.cpp`, `Part_Wgpu.cpp`, RenderState, TransparentQueue, MeshGpuCache. Direct copy after Rnd_Wgpu's game-specific draw-pass hooks are factored out.
4. **File I/O stack** — `AsyncFile_Native.cpp`, `File_Native.cpp`, `CDReader_Native.cpp`, `DataParser_Native.cpp`. Direct copy into engine.
5. **Audio stack** — `FFmpegAudioReader`, `StreamReceiver_Native`, `Synth_Stub`. Direct copy into engine. XMA decoder dropped.
6. **Input stack** — `Joypad_Native`, `Keyboard_Native`. Direct copy. Button → action mapping is per-game (RB3 needs guitar/drum/vocal mappings; DC3 has dance pad).
7. **Threading & system** — `ThreadCall_Native`, `Memory_Native`, `System_Native`, `PlatformMgr_Native`. Move into engine after the SDK-shim dependency is factored out (DC3's current versions `#include "xdk/XAPILIB.h"`; engine versions must not).
8. **LP64 lessons** — the entire HX_NATIVE catalog from DC3 sessions 1-77. Many apply identically since both decomps were ILP32 PPC. Phase 1 work is largely "port HX_NATIVE branches from DC3 sister files to RB3 sister files."
9. **STL shim layer** — `native/src/stl/_*.h` shims. Direct copy into engine.
10. **CMake scaffolding** — DC3's CMake handles macOS + Linux + Web. Engine adopts the structure; per-game source lists move to each decomp's CMakeLists.
11. **Documentation patterns** — `docs/native/`, `docs/sessions/` layout. Mirror it.

### What RB3 must build itself

1. **`src/band3/` game-layer port** — GemPlayer, BandTrack, scoring, instrument-specific tracks (guitar/drum/vocal/keys/proGuitar/proKeys). DC3 has no analog. Lives in rb3/, not the engine.
2. **MWCC compatibility shim** (`rb3/native/src/mwcc_compat.h`) — fallback for MWCC intrinsics (`__alloca`), and gates for `#pragma` directives that clang doesn't recognize. Small.
3. **Wii SDK shim** (`rb3/native/src/rvl_shims.cpp`) — POSIX implementations of the Wii SDK calls RB3's matched fork happens to make outside `src/system/os/`. Surface to spec: scan `grep -rln 'revolution/' src/system | grep -v '/os/'` and shim each header's call surface.
4. **PowerPC asm fallbacks** in matched-fork `math/` and a handful of perf-critical files. Each `asm { ... }` block gets a `#ifdef __MWERKS__ / #else <C++ equivalent> / #endif` wrap. List enumerated in the differences table above.
5. **Wii asset extraction pipeline** — equivalent of DC3's `orig-assets/`. Tools (Mackiloha, MiloLib) exist; integration script is new.
6. **RB3-specific DTA handlers** — Phase 4 work. RB3's DTA flow differs from DC3's in content but not in shape. See DTA section below.
7. **HUD & gem-track rendering** — RB3's note highway, lane visualizations, scoring overlays. Phase 5 work; needs decomp completion in `src/band3/bandtrack/`.
8. **Crowd / venue specifics** — RB3 venues differ from DC3 (band stage vs dance floor). Some venue rendering logic needs adaptation even though `WorldCrowd` is shared engine code.

---

## Phases & acceptance criteria

Phase boundaries are deliberately the same as DC3's so cross-pollination stays legible. Each phase has a concrete acceptance signal — usually a test-runnable command or a measurable runtime behavior.

### Phase 0 — Setup & bootstrapping

**Goal**: `milo-native-engine` exists as a buildable repo; both decomps consume it; `rb3-native` executable links and runs to its first crash.

This phase splits into 0.1 → 0.2 → 0.3 → 0.4. Each substep has its own acceptance gate. Do not flag-day the transition — each substep keeps the prior state working until the next is proven.

#### 0.1 — Engine repo skeleton

- Create `/home/free/code/milohax/milo-native-engine/` with CMake scaffolding, README, .gitignore.
- Engine CMake defines targets that *will* be populated in 0.2; for now they hold zero source files but produce a valid `libmilo-engine.a` (empty archive is fine).
- dc3-decomp's CMake is **unchanged** at this point. dc3-native still builds via its current inline layout.

**Acceptance**: `cd milo-native-engine && cmake -B build && cmake --build build` produces `libmilo-engine.a` (empty or near-empty). dc3-native build untouched, unchanged.

#### 0.2 — Move engine-clean code into engine; dc3-decomp keeps building

Move into `milo-native-engine/src/`:
- All of `gfx/` (WebGPU infra).
- All of `audio/` (miniaudio integration).
- All of `stl/` (host-STL shim).
- `platform/` files with no Win32-API surface and no game-class dependencies — file I/O, input, threading-via-pthread, renderer glue (after the Rnd_Wgpu game-hook factoring, see 0.2a).
- `char/CharTwistSolver.cpp` (engine helper).

Move into engine `src/system/` *only if it currently has a meaningful HX_NATIVE branch and that branch is engine-owned*:
- This is initially empty. The engine `src/system/` starts as a clean LP64 implementation of `os/ThreadCall.h`, `os/CritSec.h`, `os/File.h` interfaces — the new versions that replace DC3's `xdk`-dependent native files. dc3-decomp's matched `src/system/os/ThreadCall_Xbox.cpp` stays in dc3-decomp for asm-match; the native build no longer compiles it.

**0.2a — Rnd_Wgpu game-hook factoring** (prereq for moving Rnd_Wgpu): DC3's `Rnd_Wgpu.cpp` currently `#include "hamobj/HamDirector.h"` for an overlay draw pass and a character-impostor render-to-texture loop. Refactor into a `GameRenderHook` interface owned by the engine. DC3's existing per-game hook becomes `dc3-decomp/native/src/dc3_render_hook.cpp` providing a `HamRenderHook` implementation; RB3 will add `rb3/native/src/rb3_render_hook.cpp` in 0.4.

**0.2b — Win32 shim factoring**: DC3's `Memory_Native.cpp` and `ThreadCall_Native.cpp` `#include "xdk/XAPILIB.h"`. Rewrite the engine versions to depend only on POSIX directly (no SDK include). DC3's matched fork still needs `xdk_shims.cpp` for the *non-os* Win32 calls its decomp source makes, so xdk_shims stays per-decomp.

dc3-decomp's CMake gets a new `add_subdirectory(${MILO_ENGINE_PATH})` block and drops the source paths that moved. The matched fork's `src/system/os/*_Xbox.cpp` files are removed from the native link list; the engine's POSIX implementations take their place.

**Convergence test**: dc3-decomp's engine-only test set passes. This is the subset of `dc3-decomp/native/tests/` that touches no `hamobj/`, `meta_ham/`, `lazer/` classes:
- `test_binstream`, `test_chunkstream`, `test_dirloader`, `test_dta_parser`
- `test_asset_loading`, `test_mesh_loading`, `test_archive_enumeration`
- `test_charbones_serialization`, `test_charclipgroup`, `test_bone_ground_truth`
- `test_object_lifetime`, `test_rndcam_projection`
- `test_mogg_decode`, `test_mogg_v0xe`, `test_extract_bik`

These tests move into the engine repo as `milo-engine-tests` and run against `libmilo-engine.a` only. They become the cross-decomp convergence gate. (Game-coupled DC3 tests — `test_loading_panel`, `test_ham_provider`, `test_subsystems`, `test_headless_boot`, `test_dta_flow`, `test_movegraph`, `test_gameplay_telemetry` — stay in dc3-decomp/native/tests/ and link against dc3-native game sources.)

#### 0.3 — Stand up `rb3/native/` skeleton

This substep has two milestones:

- **Milestone (a) — `rb3-dta` (DONE).** A headless DTA parser
  (`rb3/native/build/rb3-dta`) that links the engine *without* injected decomp
  context (`MILO_ENGINE_HAVE_CONTEXT=OFF`, engine builds only the placeholder
  lib — no gfx/Dawn). It parses **138 real RB3 songs** from
  arkhelper-extracted assets (`orig-assets/extracted/songs/songs.dta`), proving
  the matched-fork DTA path runs clean under clang LP64. See
  [`../../native/README.md`](../../native/README.md).
- **Milestone (b) — full-engine link (IN PROGRESS, this session).** Flip
  `rb3-native` to inject decomp context (`MILO_ENGINE_HAVE_CONTEXT=ON`, full
  MWCC context injection), link the whole engine, and run to a controlled exit.
  Then Phase 1's `.milo` scene-tree dump.

- Create `rb3/native/` with CMakeLists that adds the engine via `add_subdirectory(${MILO_ENGINE_PATH})` and produces an empty `rb3-native` executable.
- Create `rb3/native/src/mwcc_compat.h` — initially with `__alloca` fallback and `#pragma` no-ops for the directives clang doesn't recognize.
- Create `rb3/native/src/rvl_shims.cpp` — initial surface: enumerate `revolution/OS.h` and `revolution/IPC.h` calls made by RB3 matched fork outside `src/system/os/`. Provide POSIX impls.
- Create `rb3/native/src/rb3_render_hook.cpp` — initially a no-op `BandRenderHook` that satisfies the engine's `GameRenderHook` interface.
- Create `rb3/native/src/main_native.cpp` — mirrors DC3's; constructs the `App`, runs a no-op main loop.

**Acceptance**: `rb3-native` links. Running it produces a controlled crash or exit ("data dir not found", "App init failed"). No game logic runs yet.

#### 0.4 — CI smoke

- Add a CI job that builds `rb3-native` on Linux on each PR.
- Add a CI job that runs the engine test set in milo-native-engine on each PR.
- dc3-decomp gains a CI job that builds dc3-native against the current engine pin and runs both engine tests and DC3-only tests.

**Acceptance**: All three CI jobs green.

### Phase 1 — Engine foundation

**Goal**: `rb3-native` boots far enough to load a `.milo` file and print its scene tree.

Phase 1 is mostly **landing HX_NATIVE branches in RB3's matched fork to mirror what DC3 already did**. Categories of HX_NATIVE blocks to port from DC3 sister files:

1. **LP64 type fixes** — `src/types.h` (`u32 = unsigned int` instead of `unsigned long`), `src/system/utl/BinStream.h`/`.cpp` (operator overloads, intptr_t casts), `src/system/obj/Data.h` (8-byte union zero-init in DataNode ctors). Largely a direct port; the underlying ABI issue is identical.
2. **CRT shims** — `src/types.h` (`stricmp` → `strcasecmp`), `src/system/utl/Str.h`. Trivially small for MWCC since clang and MWCC share more headers than clang and MSVC.
3. **Native impls** — full function bodies gated `#ifdef HX_NATIVE` where the matched implementation calls into the SDK directly. E.g. `VorbisReader::Poll` single-threaded decode (DC3 Session 62+), `StreamReceiver::WriteData` ring-buffer write, `Synth::Init` skip Xbox audio HW. Port the body; the upstream interface is the same.
4. **PPC-only guards** — `#ifndef HX_NATIVE` around inline asm fallbacks, around code that touches Wii GX hardware registers directly, around `__alloca` calls that the mwcc_compat.h fallback handles instead.
5. **Debug logging** — `fprintf(stderr, ...)` calls that exist only on native. Optional; add as needed.

Engine-side bring-up — **status after the 2026-05-27 session:**
- **DONE** — DataNode 8-byte union zero-init, DataArray symbol-pointer LP64 truncation, and Task liveness landed in RB3's `obj/` matched fork (DC3→RB3 carry-forward). The `ObjRef`/`ObjDirPtr` double-link ring fix was **deliberately not ported**: RB3's 2010-era `Hmx::Object` tracks refs in a `std::vector<ObjRef*>`, not DC3's intrusive next/prev ring, so DC3's session-61 bug cannot occur in RB3.
- **DONE** — `Memory_Native`/`ThreadCall_Native` POSIX impls live in the engine; RB3's `os/` matched fork already compiles clean under clang LP64 for the DTA/header-dump path (no edits needed there yet — but see Step 2: booting the App will need the deferred `os/System.cpp` runtime blocks).
- **DONE (header level)** — Asset pipeline: `ArkFile`/`ChunkStream`/`ObjDir` read a `.milo`'s directory header (`world.milo_xbox` → `world [WorldDir]`, `cowbell_bank.milo_xbox` → 8 `Sfx`/`SynthSample` objects, etc.). RB3's `.ark`+`.sel` bootstrap differs from DC3's `.ark`+`.hdr` only at the entry point. Assets are `.milo_xbox` (Xbox-format, big-endian PPC like Wii — the scene-graph structure is shared).
- **REMAINING (Step 1 of the critical path)** — Object **factory** + full graph load: needs RB3's `rndobj`/`synth` matched-fork TUs clang-LP64-clean so their classes register and instantiate. Until then `main_native` dumps the directory header, not the live object tree.

**Acceptance — two tiers:**
- **(b1) Header dump — DONE:** `rb3-native <path.milo_xbox>` prints the directory's object names + class types without crashing. Verified across 60 milos / 7 root dir classes.
- **(b2) Full object-graph load — REMAINING:** `rb3-native` instantiates the `.milo`'s objects via the registered factories and dumps the live `ObjDir` tree. Engine test set still passes.

### Phase 2 — Rendering bring-up

**Goal**: A static RB3 venue (or the main menu) renders. No animation, no UI, no audio. Just a frame.

**Blocker found 2026-05-27 (Waves 2.3 + 3):** the shared engine's WebGPU layer is **DC3-wired** and does **not** compile against RB3's older (2010) `rndobj`. Concretely: engine `src/platform/Part_Wgpu.cpp` calls `RndParticleSys::NumTilesAcross()`/`NumTilesDown()` (RB3 has no sprite-atlas tiling); the engine renderer is `WgpuRnd : NgRnd` but RB3 has no `NgRnd` (its `Rnd` is a different, older class); RB3 also lacks `FontMap`/2012-shape `RndText`, `MetaMaterial`, `RndAmbientOcclusion`, `Hmx::Matrix4`, `TheHiResScreen`. That is why RB3 currently builds the engine **GFX-off** — the `MILO_ENGINE_BUILD_GFX` option (default ON) plus the consumer `MILO_ENGINE_DECOMP_PLATFORM_EXCLUDE` seam, engine commit `54b9fa0`. DC3 sets neither → its full GPU build is byte-identical.

**Prerequisite:** Step 1 of the critical path (clang-clean `rndobj` TUs) — the GPU backends can't link against object classes that don't compile.

**Reconciliation strategy (decision deferred to Phase 2 start, pick one):**
- **(A) Generalize the engine gfx layer** to abstract over both DC3's `NgRnd`-era and RB3's older `Rnd` shapes — a thin renderer interface the `Rnd_Wgpu`/`Tex_Wgpu`/`Mesh_Wgpu`/`Part_Wgpu` backends target, with per-decomp adapters for the divergent class members. Highest cross-pollination value; most up-front work.
- **(B) RB3-specific GPU backends** in `rb3/native/` (its own `*_Wgpu`-equivalent TUs against RB3's `rndobj`/`rndwii`), reusing the engine's `GpuDevice`/`PipelineManager`/post-proc but not its Rnd/Mesh/Tex/Part backends. Faster to a first RB3 frame; some duplication.

Then, once a backend compiles+links for RB3:
- Wire RB3's draw paths through `BandRenderHook` (the no-op `rb3_render_hook.cpp` slot already exists).
- Material + shader pipeline — `standard.wgsl` carries over; validate RB3 texture formats / mesh vertex layouts (expect a CMPR/BC*-format issue analogous to DC3 Session 71).
- First milestone: clear-color frame → triangle → a single textured RB3 mesh. Second: render an RB3 venue/main-menu `.milo` statically.

**Acceptance**: Screenshot of an RB3 venue or main menu rendering, even without lighting accuracy.

### Phase 3 — Audio

**Goal**: A `.mogg` plays through the speakers at correct pitch and speed.

**Status (2026-05-29): DONE.** `rb3_synth_native.cpp` now ships a real
`NativeSynth : Synth`, and `rb3_stream_receiver_native.cpp` bridges RB3's
`StreamReceiver` to the engine's `AudioSource` (dual `StartSendImpl` overloads
+ 96KB ring + miniaudio integration). v1 reaches `Game::mLoadState = kReady`
with mixer/decode running. Audible output requires a headed environment; the
headless CI/test run by design does not open a device.

- VorbisReader: port DC3's Session 62-67 native impl. The decoder is in the
  engine; the matched-fork glue lives in `rb3/src/system/oggvorbis/` with
  `#ifdef HX_NATIVE` branches mirroring DC3's.
- `StreamReceiver_Native`, `AudioDevice` ring buffer — already in engine;
  verify RB3's `Synth` initialization wires them (RB3's `StreamReceiver` lacks
  `IsOutputDrained`/`SetSlip*` per the engine-exclude comments, so the engine
  versions need RB3-shape adaptation OR an RB3-native equivalent).
- Validate `songMs` clock from audio playback drives engine timing. The boot-to-
  song fix landed a native `std::chrono` UI clock so `UISeconds()` advances on
  native (previously the PPC `mftb` expansion was a no-op under clang) — verify
  the same fix powers `TheTaskMgr.SongMs()` for audio synchronization.
- **Asset gate:** a `.mogg` for at least one song must exist on disk (V1).

**Acceptance**: A tutorial-song `.mogg` plays at real-time speed and `songMs`
advances correctly.

### Phase 4 — Input & UI menu navigation

**Goal**: Navigate boot → main menu → song select with a keyboard, headless GPU.

**Status (2026-05-27): DONE for the headless/synthetic-input path.** Boot-to-song
delivered: `splash → main_hub_screen (settles) → song_select_screen (83 real
songs) → song selected → meta→game transition → Game::LoadSong()`. UI manager
identity resolved: RB3 uses `BandUI : UIManager` bound at static-init
(`band3/meta_band/BandUI.cpp:44-45` `UIManager &TheUI = TheBandUI`). The
synthetic-input mechanism lives in `native/src/rb3_game_input.cpp` (frame-
scheduled `ButtonDownMsg`/`select:`/`msg:` via `Automator`/`TheUI.Handle`).
**Open follow-ups for full Phase 4** (post-v1 polish): real `Joypad_Native`/
`Keyboard_Native` (currently excluded for RB3 due to vtable shape differences);
RB3 controller mappings (guitar/drum face buttons, vocal mic) for actual
keyboard play.

**Real keyboard/gamepad menu nav (2026-06-02):** The native+web build now drives
the *real* engine input path — `JoypadPoll` → `SendButtonMessages(0, btns)` →
`ButtonToAction` → `TheUI`'s `JoypadClient`. Pad 0 is pinned to the `wii_guitar`
breed so the same keyboard plays guitar in-song. The loaded Xbox
`config/joypad.dta` ships **no** `button_meanings` block (the only variant with
one is the non-loaded Wii `system/run/config/joypad.dta`), so `gButtonMeanings`
was NULL and `ButtonToAction` returned `kAction_None` for every menu key →
menus never advanced off `splash_screen`. Fix: we append a `button_meanings`
block (`wii_guitar` + `analog`/`digital`/`dualshock`/`none`, **raw integer keys
and values** — no `kPad_`/`kAction_` macros exist in the loaded config) to
`orig-assets/extracted/config/joypad.dta` so the real path resolves menu actions
(Confirm/Cancel/Option/Start/Up/Down/Left/Right/PageUp/PageDown). The
`wii_guitar` map mirrors `rb3_game_input.cpp`'s `kWebKeyMap` (the proven menu
baseline). **Gameplay is unaffected**: `GuitarController` resolves frets by
physical button→slot (`beatmatch_controller.dta`) and ignores `button_meanings`.
This is a zero-C++ data fix that works for both native and web (same config
dir). **This edit must be re-applied if extracted assets are regenerated from
the ARK** (it lives in a gitignored extraction tree). Note that `gButtonMeanings`
is a `Joypad.cpp` file-static cached at `JoypadInitCommon` (before
`EnsureWiiGuitarMapped`), so it cannot be re-pointed from native glue — the data
must exist in the file at parse time.

Phase 4 is structured around the DTA-loading work because that is the recurring blocker:

#### 4a — Input pipeline

- `Joypad_Native` + `Keyboard_Native` adapted from engine. RB3 controller mapping (guitar/drum face buttons, vocal mic) goes in `rb3/native/src/rb3_input_map.cpp`.
- `MsgSinks::Export` (already engine) + RB3 UI manager identity (investigate first task: does RB3 use `TheHamUI` like DC3 or a different two-pass draw stack?).

#### 4b — DTA execution baseline

DC3 hit a deep DTA-loading blocker that took multiple sessions of root-causing across the boot path. RB3 will hit something analogous. The work is *not* "fix the parser" — DC3's parser works. The work is reaching enough state that the DTA-driven boot script can advance:

- DTA parser + DataArray: already engine, verified by `test_dta_parser`.
- `SystemPreInit` + `SystemInit` load configs from `.ark`: same code path as DC3.
- `UIManager::Init` broadcasts `"init"` message → DTA handlers fire.
- Specific DTA commands fail silently when they reference Xbox-specific objects (DC3) / Wii-specific objects (RB3). For RB3, the parallel set: `$content_mgr`, `$net_mgr`, `$disc_mgr`, plus RB3-specific managers like `$band_user_mgr`, `$song_mgr`.

For each missing object, the work is: provide a no-op stub object that satisfies enough of the DTA-callable surface to let the boot continue. See DC3's `docs/native/DTA_LOADING_BLOCKER.md` for the pattern.

#### 4c — Screen flow + first interactive milestone

- `UIScreen::Enter` auto-skip pattern (DC3 Session 24).
- Animation lifecycle: `AnimTask::Poll` self-deletes when `mAnimTarget` nulled (DC3 Session 39).
- `mSink` button routing fallback (DC3 Session 39).

**Acceptance**: First milestone: attract → title → main_screen renders with text. Second milestone: full menu flow to song_select with keyboard input working.

### Phase 5 — Gameplay (v1 milestone)

**Goal**: Pick a song, load venue, play audio, render gem track, advance score. Single instrument (guitar) is fine.

**Status (2026-05-29): DONE.** v1 plays "20th Century Boy" end-to-end on Linux:
synthetic input → menu → song_select → part_difficulty → tv3 transition
cinematic → in-song gameplay with venue + band + crowd rendering,
`Game::mLoadState = kReady`, gem highway + smasher rendering, score HUD
ticking. Per-layer detail and the X1-X6 + V2-V9 status table live in
[`docs/sessions/native/V1_ONE_SONG.md`](../sessions/native/V1_ONE_SONG.md).

Key landings:
- Audio backend: `rb3_synth_native.cpp` `NativeSynth : Synth` +
  `rb3_stream_receiver_native.cpp` bridging RB3's `StreamReceiver` to the
  engine's `AudioSource` (X5, X6).
- Assets: Xbox 360 full ARK → `orig-assets/extracted-xbox-full/` (X1);
  `SetUsingCD(true)` in `os/System.cpp` rewrites `config/foo.dta` →
  `config/gen/foo.dtb` to match the extract layout.
- MOGG decode: `ByteGrinder::GrindArray` + `magicNumberGeneratorNative` ported
  from DC3 (X3); `VorbisReader` HX_NATIVE Poll/DoFileRead/Decrypt + magicHash
  ported (X4).
- Tv3 transition cinematic: default-on at `a316502c` + `acdfc69f` (force-poll
  vignette_transition + `CamShotFrame::Interp` fix + hold gate); the
  `InterstitialPanel::Exiting` HX_NATIVE wrapper deleted at `aab19da5` once
  retail-parity was restored.
- Song-end reliability: `6cfb0a7d` (real `NetSession::EndGame` + `IsInGame`
  semantics so `CanEndGame` no longer blocks `TrulyWinGame` / `GameEndedMsg`).

**Post-v1 polish** (NOT v1 gates):
- Residual long-tail gameplay TUs (`Singer`, `VocalPlayer`/`VocalNoteList`,
  `BandPatchMesh`, `TourPerformerLocal`, …) still stubbed in
  `band3_link_stubs.s`; bringing them up improves band animation / results
  screens.
- `Character::DrawLodOrShadow` → `RndMesh::SetUpdateApproxLight` crash still
  sigsetjmp-absorbed (animated characters draw with a guard); cosmetic.
- Texture sampling in some paths still diffuse=white; skinning uses identity
  bone palette.
- Inlined-proxy dir parent wiring (`Character::PostLoad` → `RndDir::PostLoad`
  `p->from->Dir()==null` on native for inlined sub-dirs) — cosmetic main_hub
  venue band preview is currently deferred at its call site; DC3 has the same
  check passing with no HX_NATIVE.

**Acceptance**: One full song completed end-to-end on Linux x86_64. **MET.**

### Phase 6 — Polish & multi-platform (post-v1)

- Multi-instrument support (drums, vocals, keys, pro-guitar, pro-keys).
- All venues.
- Post-processing fidelity pass.
- **macOS reactivation**: engine's CMake already handles macOS; verify rb3-native links and runs on arm64 + x86_64. Acceptance: one song plays end-to-end on macOS.
- **Web reactivation**: engine's `DC3_WEB_*` source lists are renamed `MILO_WEB_*` during 0.2 extraction. RB3 opts in via `cmake -DMILO_BUILD_WEB=ON`. Acceptance: one song plays end-to-end in browser via Emscripten.
- Multi-song stability sweep (DC3 Session 75-76 model).

---

## Known issues & tracked regressions

| # | Issue | Status / notes |
|---|-------|----------------|
| K1 | **`milo-engine-tests`: 4 `RndCamProjectionTest` failures** (`PerspectiveIdentityProjectionMatchesExpectedMatrix`, `PerspectiveWorldToScreenMatchesExpectedFrustumEdges`, `ChooseModeDebugUiCamHackPushesApproximateLayoutOffscreen`, `OrthographicProjectionMatchesExpectedMatrix`); suite is **191/195**. | **Pre-existing** — not introduced by the 2026-05-27 session (reverting that session's engine change did not fix them; the engine source on the projection path was unchanged). Prime suspect: clang-22.1.5 toolchain drift in projection-matrix float codegen. Test file `milo-native-engine/tests/test_rndcam_projection.cpp:178`. **OPEN** — diagnose (compare expected/actual matrices; check `-ffp-contract`/FMA codegen) before relying on RndCam projection in Phase 2. |
| K2 | **RB3 `rndobj`/`synth` matched-fork TUs not clang-LP64-clean** (was ~22/64 rndobj, ~7/47 synth). | **RESOLVED (2026-05-27).** All **64/64 rndobj + 47/47 synth** TUs now compile clean under clang LP64 and `rb3-native` links them. Fixes (additive `HX_NATIVE` blocks): dependent-base `using Base::size/back/…` in `Keys`/`ObjVector` (the "`Symbol` has no call operator" cluster, 9 TUs); `Symbol("…")`-literal use sites for the POSIX-colliding symbol names (`wait`/`select`/`random`/`pause` in Anim/EventTrigger/Env/PostProc/Wind/Song) + gated `extern Symbol` decls for `pause`/`environ`/`sync`; MWCC `MSL_Common/extras.h` → `<cstring>`; Wii GX `GXPixel.h`/`rndwii/Mat.h` includes + `GXSetPixelFmt` calls gated out (TexRenderer); `&ref = ptr` reference-rebind hacks rewritten with pointers (Mesh/Tex); STLport `__copy_ptrs`/`stlpmtx_std` block gated (Line); switch jump-over-init braces (Trans); `this->insert(begin()+n,…)` (PropKeys); address-of-temporary fixes (PropAnim); missing `Multiply(Matrix3³)` via not defining `CHARHAIR_LOCAL_MULTIPLY` on native (Part); explicit `&Class::member` ptr-to-member (MetaMusic); tomcrypt `<angled>` include path added to the CMake include dirs. Off-path link deps (Wii GX backend / Bink / libogg / tomcrypt / Striper / engine-GPU / game globals, 83 syms) satisfied by `native/src/rndobj_synth_link_stubs.s` weak no-op aliases. |
| K3 | **RB3 rendering** | **RESOLVED (2026-05-27).** Strategy B: RB3 builds the engine's rndobj-FREE WebGPU **gfx core** (`MILO_ENGINE_BUILD_GFX` ON / `MILO_ENGINE_BUILD_GPU_BACKENDS` OFF) and supplies its own thin backend (`rb3/native/src/rb3_band_rnd.cpp` `BandRnd : Rnd`, `rb3_render_mesh.cpp`) providing the real bodies for the weak-stubbed `RndMesh::DrawShowing`/`RndTex::*`. `RB3_RENDER_MESH=1 rb3-native <milo>` renders real RB3 geometry headless (Vulkan via `/dev/dri`, no DISPLAY) → PNG. RndCam projection built directly from the camera world basis + a [0,1]-z clip matrix (sidesteps the K1 `GetViewProjectXfms` path). Verified: tracksystem (129 meshes/27878 tris), gem-smasher, beveled text. **Open:** textures not yet sampled (diffuse=white); skinning uses identity bone palette; a latent matched-fork heap over-write (guard-padded in MemMgr) still trips on some char milos. |
| K4 | **`dc3_runtime_sources.cmake` lists two engine-graduated files** (`Memory_Native`, `ThreadCall_Native`) → benign duplicate symbols in `milo-engine-tests`. | Cosmetic. A safe per-line removal is identified (verified not to change behavior or the K1 failures) but left un-applied. **LOW PRIORITY.** |

---

## Convergence tracker — hack bring-online (post-v1)

The native port is **"mostly faithful with isolated disables," not broadly
stubbed** (2026-06-08 multi-agent audit: 73 unique hacks → 10 fixable, 7–8
blocked, 9 correct platform glue, 47 benign default-faithful opt-outs). The real
convergence debt is a small **char-customize + vocals** cluster. Source of truth:
[`NATIVE_HACK_AUDIT_2026-06-08.md`](NATIVE_HACK_AUDIT_2026-06-08.md) +
[`BLOCKER_VALIDATION_2026-06-08.md`](BLOCKER_VALIDATION_2026-06-08.md) (validated
verdicts). The **`rb3-tests`** gtest suite (`native/tests/`, 11/11) is the durable
guard — every bring-online lands with a test.

| # | Item | Status | Next action |
|---|------|--------|-------------|
| C1 | PostLoadVocals / GameMicManager | ✅ done (`082bcea4`/`8d300cd7`) | covered by boot-to-gameplay |
| C2 | gDeforms gender-morph + IsExoBone LP64 | ✅ done (`a5999979`) | — |
| C3 | VocalPlayer RTTI-cast guard delete | ✅ done (`579e7416`) | — |
| C4 | BandHeadShaper `gHeadMale` typo | ✅ done (`4e49ef34`, match-positive) | — |
| C5 | BandFaceDeform BE decode | ✅ verified-correct + gtest (`15e3c048`) | false alarm — no reader needed |
| C6 | **char-customize previews** (chars.milo + CharSync Poll) | 🚧 **blocked on body-source** (see C13) | reaching+populating the closet is designed (C11+C12); the preview chars are bodyless shells → C13 is the real blocker |
| C7 | BandHeadShaper head-milo load | 🚧 blocked | `CharClip`/`CharBonesSamples`/`CharBones` `Load` native byte-correctness |
| C8 | char head/hands/face rebind residual | 🚧 blocked, concrete lead | per-member bind-frame basis correction (no Xbox ref needed) — see char-skinning doc |
| C9 | worldcenter occluder | ✅ KEEP | depth-occluder, orthogonal to the (fixed) RTT; A/B recipe in validation doc |
| C10 | SP scoreboard X-neutralization | ✅ KEEP | correct stopgap; diagnostic-only convergence |
| C11 | **native sign-in / profile gate** (customize requires `CanSaveData`; sign-in is a stubbed redirect-back) | 🟡 foundation landed (`2bb6d944`), cascade open | Landed: faithful offline `TheServer` (`rb3_server_native.cpp` — was a null-vtable crash) + **opt-in** guest-profile (`rb3_guestprofile_native.cpp`, `RB3_GUEST_PROFILE=1`: real `WiiProfileMgr`/`ProfileMgr`/signin for pad 0). **DEFAULT-OFF** because enabling it makes a *primary* profile exist → cascade of profile-driven crash paths (negative-control confirmed). Dominoes: ①`MainHubPanel`→`TheServer` ✅fixed; ②`RndMat::SyncProperty`/`PropSync<RndTex>` (patch-texture sync) ⬜ + more. **Deeper work**: resolve the cascade (faithful offline stubs), then real sign-in via decomp (`PlatformMgr_Wii`/`WiiProfileMgr`) or reimpl. |
| C12 | customize closet reachability | 📋 designed | `PrefabMgr::PrefabIsCustomizable()`→true (HX_NATIVE default-on, HACK/TODO) routes `customize_character`→closet; needs C11 (else `CustomizePanel::Load` null-profile asserts) + C13 |
| C13 | **band-char preview BODY source** — route IDENTIFIED, gate pending | 🟡 route known, Stage-0 gate written (verification blocked) | **Corrected**: `player0..3` are NOT bodyless — they are milo **proxies** (`mProxyFile="../../char/main/main.milo"`); `Dir::PostLoad` loads `main.milo` into each → binds `mFileMerger` (main.milo has FileMerger.fm + outfit + IK + body_clips) → FileMerger loads the 13 bodyparts. **Same path gameplay uses** (`acd9c19a`). Xbox/Wii asset mismatch = red herring. **Stage-0 gate** written (`native/tests/test_char_preview.cpp`, untracked): load chars.milo, assert each player has a `FileMerger.fm` child. **Blocked**: shared build red from a concurrent uncommitted `App.cpp`+`BandOffline.h` (STLPORT) WIP — run the gate once green, then the full enable plan. See [`CUSTOMIZE_PREVIEW_FINDINGS_2026-06-09.md`](CUSTOMIZE_PREVIEW_FINDINGS_2026-06-09.md). |
| — | whammy slip / intro cinematic | 🚧 blocked | themes D/C (effort HIGH / low-value) |

---

## Cross-pollination workflow

The whole point of `milo-native-engine` is that fixes in either game's port land in both. Workflow when a native bug is found:

1. **Diagnose in one decomp's native build.** Capture stack, repro.
2. **Locate the file.** Is it engine code (`milo-native-engine/src/**`), per-decomp glue (`*-decomp/native/src/**`), or matched-fork code under `HX_NATIVE` gating (`*-decomp/src/system/**`)?
3. **Engine code** → fix in milo-native-engine. Bump the engine pin in both decomps. Both pick up the fix.
4. **Per-decomp glue** → fix locally. If the bug is structurally identical to something the sister repo will hit (it usually is), open a tracking issue in the sister repo's docs/sessions/ so it lands before the sister repo's bring-up trips on it.
5. **Matched-fork HX_NATIVE branch** → fix in the affected decomp's matched fork. The same logical bug almost certainly exists in the sister repo's matched fork; cross-port the HX_NATIVE branch (not the matched code itself — that stays per-compiler-quirk).

### What we expect cross-pollination to look like in practice

- **Engine code**: changes here happen most often during Phase 0-3. Once stable, churn drops.
- **Per-decomp glue**: each decomp owns its own. Cross-pollination is "you'll hit this; here's our diff."
- **Matched-fork HX_NATIVE**: every Phase 1 RB3 session is essentially "find the DC3 sister file, copy the HX_NATIVE branches into the RB3 matched file, adapt for STLport vs MSVC STL and MWCC vs MSVC quirks."

---

## Open questions / risks

| # | Item | Mitigation |
|---|------|-----------|
| 1 | **DTA loading depth** — DC3 found that specific DTA commands fail silently when referencing platform-specific manager objects. RB3 will hit the same class of issue with a different set of missing objects. | Treat Phase 4b as a discovery phase: each missing object gets a no-op stub until boot advances past the next milestone. Reference DC3's `docs/native/DTA_LOADING_BLOCKER.md` for the pattern. |
| 2 | **RB3 UI manager identity** — does RB3 use `TheHamUI` like DC3, or a `TheBandUI` or different two-pass stack? | First investigation task of Phase 4. Source of truth: `src/band3/meta_band/`. |
| 3 | **Wii asset extraction path** — RB3 uses `.ark` + `.sel`; existing tools (Mackiloha, MiloLib) cover this but the extracted layout may not match what `ArkFile` expects at runtime. | Validate early in Phase 1 with a single milo load test. The ArkFile loader is shared; only the bootstrap entry point (`.sel` vs DC3's `.hdr`) is per-game. |
| 4 | **Endian handling** — both Xbox 360 and Wii are big-endian, but DC3's native sometimes assumes little-endian host RAM (BinStream paths). Verify byte-swap toggles for RB3. | Carry DC3's `ChunkStream` endianness fix (Session 31) and test against an RB3 asset early. |
| 5 | **MWCC paired-singles inline asm fallbacks** — the 9 matched-fork files with `asm { psq_l ... }` blocks need C++ fallbacks for the native build. The math fallbacks are well-understood (cross product, dot product, normalize). The perf-critical fallbacks (Part.cpp particle inner loop, CharHair `StrandMultiply`) need testing for correctness, not just compilation. | Land math fallbacks in Phase 1. Test perf-critical fallbacks against the matched build's known-correct output before Phase 2 rendering work depends on them. |
| 6 | **DLC / additional content** — RB3 has a deep DLC catalog. v1 ignores DLC; v2+ may want it. | Out of scope for v1; design `SongMetadata` validation to be DLC-extensible. |
| 7 | **Engine pin bootstrap** — the very first engine SHA pin can't pre-exist before the engine is created. | Phase 0.1 lands the engine repo *and* the initial pin in dc3-decomp's CMakeLists in the same change-set. Subsequent pins use `scripts/bump-engine.sh`. |

---

## Decision log

Decisions made up-front and during planning. Append when a phase boundary or scope decision changes.

| Decision | Rationale |
|----------|-----------|
| **New repo `milo-native-engine`** (sibling to dc3-decomp and rb3) | Highest cross-pollination value. Avoids one-way coupling. |
| **Copy-first, extract-later** bootstrap | Faster path to a working RB3 native. Extraction happens once code shape stabilizes. |
| **Three-layer source model** (matched fork / engine runtime / per-decomp glue) | Earlier "hybrid src/system" plan conflated layers. Three distinct categories with explicit ownership avoids the trap. |
| **Engine `src/system/` is a clean LP64 rewrite, not a hoisted matched fork** | The matched forks' `HX_NATIVE` branches are *the* native compatibility layer; they don't graduate out. Engine grows its own clean implementations against the same headers. |
| **Per-decomp SDK shims** (`xdk_shims.cpp`, `rvl_shims.cpp`) | The which-API-surface is per-decomp; the shim layer can't be shared because the matched forks call into different SDKs. |
| **STL: stlport stays per-decomp, native uses host STL via shim path priority** | DC3's existing pattern works; replicate verbatim. Engine ships the shim headers. |
| **Convergence test = engine-only test subset** | Game-coupled tests stay per-decomp. The cross-decomp gate is the engine tests passing on both repos' native build pipelines. |
| **Platforms v1**: Linux x86_64 + macOS + Web | Mirrors DC3. Skip Windows for now. macOS + Web reactivate in Phase 6. |
| **Single-player v1** | Defer all `src/network/` + Wii online. Match DC3 scope. |
| **v1 milestone**: one song end-to-end | Concrete, demoable, captures most of the engine's surface area. |
| **Engine packaging: sibling repo + `add_subdirectory()` + pin file** | Source-level build keeps iteration and debugging fast during heavy Phase 0–5 engine churn. SHA pinned via `MILO_ENGINE_PIN` in each decomp's CMakeLists; bumped explicitly. Reversible to FetchContent or static-lib install later. |
| **Engine SHA pin: warning, not error** | CMake compares engine HEAD vs pin and prints `message(WARNING ...)` on mismatch. Lets us test against engine branches without editing CMakeLists. Bump the pin when changes land canonical. |
| **Engine library shape: single `milo-engine` static lib** | All engine .cpp files compile into one `libmilo-engine.a` target. No subsystem splits initially; revisit if link time degrades. Tool targets (`milo-viewer`, `milo2gltf`, `render-test`) are separate executables, opt-in via `MILO_ENGINE_BUILD_TOOLS=ON`. |
| **Debug tooling (HttpServer + DebugPanel) built in by default** | Both included in `milo-engine` unconditionally — no CMake flag to disable. Bring-up without an HTTP eval endpoint and ImGui debug panel is too painful. |
| **Compiler-quirk gating macros**: `HX_NATIVE`, `__MWERKS__`, `_MSC_VER`, `__EMSCRIPTEN__` | Explicit four-way gating. `HX_NATIVE` means "clang LP64 native build"; the others identify the matched-fork compiler. |
| **Engine serves all three decomps; Wii `rb3` is brought up first** | `rb3` (Wii, SZBE69_B8) is a *debug* build with full DWARF → faster, more reliable bring-up than the `rb3-xenon` *retail* XEX. The harder retail-Xenon port proceeds in parallel. `rb3-xenon/native/` is a proof-of-concept (headless `songs.dta` parse → 138 songs, no GPU/audio) and is largely **redundant** with the shared-engine effort: mine it for its RB3 type set (`native_link_glue.cpp`) and dual-target `types.h` pattern, not its copied `platform/`/`stl/` shims (the engine owns those). |

---

## Status log

Append a one-line entry per session. Detailed session notes go in `docs/sessions/native/` (mirror DC3's `docs/sessions/` layout).

| Date | Phase | Summary |
|------|-------|---------|
| 2026-05-25 | 0 | Roadmap drafted. No code yet. Decisions: new milo-native-engine repo, copy-first bootstrap, hybrid src/system, clean LP64 in shared engine, Linux+macOS+Web, single-player v1, one-song-e2e v1 target. |
| 2026-05-25 | 0.1 | Per-file inventory of dc3-decomp/native/ complete — see [NATIVE_PORT_INVENTORY.md](NATIVE_PORT_INVENTORY.md). Dividing line is **game coupling** (not "native APIs vs engine"): most files ship to milo-native-engine, a small set needs cleanup, DC3-specific files (Kinect, XDK, DC3 game classes) stay put. |
| 2026-05-25 | 0.1 | Engine packaging decisions locked: sibling repo + `add_subdirectory()` + soft SHA pin (warning), single `milo-engine` static lib. Reversible to FetchContent/install-package later. |
| 2026-05-25 | 0.1 | Debug tooling (HttpServer + DebugPanel) built into `milo-engine` by default for both decomps — no opt-out flag. |
| 2026-05-25 | 0.1 | Staff-engineer design review pass: tightened the three-layer source model, replaced "hybrid src/system convergence" with the cleaner "engine is its own clean LP64 fork" model, added explicit SDK-shim spec (xdk for DC3, rvl for RB3), enumerated MWCC inline-asm files, added STL-seam spec, defined convergence test as the engine-only test subset, split Phase 0 into 4 substeps with per-step acceptance, split Phase 4 into 4a/4b/4c around the DTA work. |
| 2026-05-26 | 0.1 | **`milo-native-engine` repo skeleton stood up — Phase 0.1 acceptance met.** Game-agnostic `CMakeLists.txt` builds the `milo-engine` static lib (near-empty `libmilo-engine.a` from an `EngineVersion` placeholder TU); README, `.gitignore`, `src/` subsystem tree (gfx/audio/platform/char/stl/system/export/tools/viewer/render_test), `docs/native/`+`docs/sessions/`. Soft dependency detection (Threads/glfw3/FFmpeg found locally, Dawn optional/non-fatal) ready to promote to REQUIRED in 0.2. Opt-in tool/test/web targets gated. dc3-decomp untouched. Decision recorded: engine serves all 3 decomps, **Wii `rb3` prioritized**; `rb3-xenon/native/` is a POC to mine, not a base. |
| 2026-05-26 | 0.2 | **Engine extracted + DC3 fully converged onto it.** Copied the day-1 SHARED engine-clean source (gfx/audio/char/clean-platform = 47 TUs) + vendored headers into the engine. Engine CMake gained a **consumer-injected context** model (`MILO_ENGINE_DECOMP_INCLUDE_DIRS`/`_COMPAT_FLAGS`/`_PCH`): the engine compiles against the consuming decomp's Milo headers + matched-fork compat flags, so the same engine source builds under MSVC-PPC (DC3) or MWCC (RB3). `cmake/dc3-reference.cmake` lets the engine compile-check standalone against DC3. **All four DC3 consumers now link `libmilo-engine.a`** — dc3-native, milo-viewer, render-test, milo-tests — via a shared filtered source list + soft SHA pin (engine `291a70f`). **`milo-tests`: 371/371 pass** (telemetry-automation skipped); dc3-native boots through archive/file-IO/input/DTA from the engine. Deferred to per-decomp glue pending factoring: PlatformMgr_Native (xdk/XSOCIAL), RenderState_Native (xdk/D3D9), Skeleton_Native (Kinect), HttpServer+DebugPanel (telemetry/DC3_HTTP_SERVER). Remaining 0.2: 0.2a GameRenderHook (Rnd_Wgpu), 0.2b xdk removal (Memory/ThreadCall), move engine-only tests into engine as `milo-engine-tests`. |
| 2026-05-27 | 0.2 | **`milo-engine-tests` convergence gate landed in the engine repo: 161/162 pass** (1 intentional skip; `test_asset_loading` dropped — game-coupled + a separate GpuDevice/Wgpu atexit-teardown segfault to fix engine-side later). Transitional link model: until the Milo CORE (utl/obj/math/rndobj/os/synth) moves into the engine, the test target links `libmilo-engine.a` + dc3's reference core via a generated `tests/dc3_runtime_sources.cmake` (read-only consumption of the dc3 checkout). That list shrinks toward empty as Phase 0.2 factors the core into the engine. Engine pin now `7b5adf5`. |
| 2026-05-27 | 0.3 | **rb3 Wii native: headless `rb3-dta` milestone (a) achieved.** rb3/native/ stood up consuming `milo-engine` via `add_subdirectory` (no decomp context → engine builds only the placeholder lib; gfx/Dawn deferred to Phase 2). Matched-fork core (obj/utl/os/math + DataFlex) + native_link_glue + rvl_shims + Wii SDK shim headers (`native/src/revolution/*`) compile under clang LP64. Load-bearing HX_NATIVE-gated edits to the matched fork (MWCC ninja build unaffected — `HX_NATIVE` never defined there): `types.h` LP64 dual-target split; `compiler_macros.h` clang-`__attribute__` gate; `math/Vec.h` `__MWERKS__`-gated C++ fallbacks for `Distance`/`Length`/`Normalize` paired-singles asm (asm preserved in `#else`); HX_NATIVE shims across `utl/{Symbols*,MemMgr,PoolAlloc,Str,Option}`, `os/{File,Debug}`, `obj/Task.cpp`, `math/SHA1.cpp`. Verified: `rb3-dta` parses a real Milo DTA, prints id/name/artist rows, exit 0. |
| 2026-05-27 | 0.3 | **rb3 ark extraction tooling + full milestone-(a) proof.** `scripts/milo/extract_ark.sh` (modeled on dc3's) drives Mackiloha's `arkhelper` against the RB3 archive; auto-detects Wii `.hdr`/`.sel` and falls back to the RB3 360 retail ARK when a usable Wii ark isn't present (RB3-Wii `.ark` data parts aren't in this repo's `orig/` — only a zero-magic `.sel` placeholder + CodeWarrior linker `.map`). Extracted ~4.0 GB / 4729 files (`songs/`, `config/`, `world/`, `ui/`, etc., 274 `.dta` text) to `orig-assets/extracted/` (gitignored). **`rb3-dta orig-assets/extracted/songs/songs.dta 138` parses all 138 top-level nodes** (on-disc setlist: "20th Century Boy", "Bohemian Rhapsody", "Crazy Train", … + pro-keys tutorial entries) cleanly, exit 0 — full proof the matched-fork DTA path handles real RB3 input under clang LP64. |
| 2026-05-27 | 0.2/0.4 | **Phase 0 closed: 0.2a, 0.2b, 0.4 CI, asset-loading fix.** (i) **0.2b** Memory_Native + ThreadCall_Native graduate into the engine with POSIX impls (pthread + sem_t; no `xdk/`, no Win32 type names); engine sources 47→49. (ii) **0.2a** Rnd_Wgpu graduates into the engine behind a clean `GameRenderHook` interface (`DrawGameOverlay` / `RenderCharacterImpostors`, opaque `void*` cookie); DC3 supplies `HamRenderHook` via static auto-registration. Surprising finding: the historical hamobj call sites were already removed in dc3 commit `a97fbac6` leaving orphan includes — the seam codifies the now-clean state and pre-shapes the slot for RB3's `BandRenderHook`. (iii) **Asset-loading rejoin**: the previously-dropped `test_asset_loading` re-enters `milo-engine-tests` (162→195 tests). The "GpuDevice/Wgpu atexit teardown" hypothesis turned out wrong; real bug was `nullptr TheUI` deref in `PanelDir::SendTransition` reached via `HamDirector::OnFileMerged` during a song-milo load. Fix: install a default-constructed `UIManager` stub as `TheUI` in `EnsureEngineInit()`. **3× consecutive runs at 195/195 + 1 intentional skip.** (iv) **0.4 CI**: per-repo GitHub Actions workflows (engine + dc3 + rb3) for native build + test, with pinned Dawn release + asset-skip behavior — actionlint-clean. Blocker: `milo-native-engine` needs a GitHub remote pushed for cross-repo checkouts to fully resolve. (v) Side finding for follow-up: engine standalone binary reaches `OnFileMerged` reliably; dc3's milo-tests binary doesn't — a real Phase-0.2 link-order divergence under `-Wl,--allow-multiple-definition`. **Both decomps pinned to engine `9a58e86`. milo-tests: 371/371. milo-engine-tests: 195/195. rb3-dta: 138 songs.** |
| 2026-05-27 | 2+3 | **Steps 2 & 3 — headless DTA boot + native rendering (RB3 renders real geometry).** **Boot:** `os/System.cpp` `SystemPreInit`/`SystemInit`/`SystemPoll` HX_NATIVE-gated (skip Wii RSO/0x91000000 asserts, WiiNetworkSocket, TheMC/CacheMgr/PlatformMgr/ContentMgr, CheatsInit); ported DC3's flex hold-char fix (`DataFlex.c/.h` `yyGetHoldChar`/`yySetHoldChar` + `ReadEmbeddedFile`) so `#include`-inside-array parses; `chdir` RB3_DATA + `rb3/system` symlink for the `(..)` extraction layout. `RB3_BOOT=1` → `gSystemConfig` = **227 object type-defs + ui config** (also completes Phase-1 full object-graph load). **Engine gfx split (commit `9ad4e13`):** `MILO_ENGINE_BUILD_GPU_BACKENDS` separates the rndobj-free WebGPU core (RB3 builds it) from the NgRnd-coupled backends (off for RB3); DC3 unchanged. **Rendering (Strategy B, commit `7fac3184`):** `BandRnd : Rnd` + `rb3_band_rnd.cpp`/`rb3_render_mesh.cpp`/`rb3_render_tri.cpp` reuse the engine `GpuDevice`/`PipelineManager` and provide the real bodies for the weak-stubbed `RndMesh::DrawShowing`/`RndTex::*`; RndCam→clip matrix built from the camera world basis (sidesteps K1). **`RB3_GPU_SMOKE=1`** clear-color frame; **`RB3_RENDER_MESH=1 rb3-native <milo>`** boots+loads+renders real RB3 geometry headless (Vulkan/`/dev/dri`, no DISPLAY) → PNG: **tracksystem 129 meshes/27878 tris**, gem-smasher strike pads, beveled text. Matched-fork render-path fixes: `Mesh.cpp` Xbox-compressed vert blobs, `MemMgr.cpp` guard pad. Render-ready subsystems pre-cleaned (committed): rndobj 64/64, synth 47/47, ui 37/37, world 16/16, bandobj 47/60 (menu widgets). Regressions green: rb3-dta 138, header dump, boot, smoke, triangle, mesh render. **Open:** main-menu/venue/character render needs per-class `Load` native-correctness for ui/world/bandobj/char (each class like the rndobj/synth bring-up) + UI screen flow (Phase 4c) + texture sampling. Both decomps pin engine `9ad4e13`. |
| 2026-05-27 | 1 | **Critical-path Step 1 — rndobj/synth clang-LP64-clean (K2 resolved) + live object instantiation.** Made all **64/64 rndobj + 47/47 synth** matched-fork TUs compile clean under clang LP64 via additive `HX_NATIVE` blocks (see K2 row for the per-cause list: dependent-base `using` in Keys/ObjVector, `Symbol("…")` literals for POSIX-colliding names + gated decls for `pause`/`environ`/`sync`, MSL→`<cstring>`, Wii-GX include/call gating, `&ref=ptr`→pointer rewrites, STLport `__copy_ptrs` gating, switch-init braces, vector `insert` iterator, ptr-to-member `&Class::member`, `CHARHAIR_LOCAL_MULTIPLY` not-on-native). Re-added rndobj/synth to `NATIVE_FORK_SOURCES`; 83 off-path link deps satisfied by a generated weak-stub file (`rndobj_synth_link_stubs.s`). `main_native.cpp` registers the obj/rndobj/synth factories; `RB3_LIVE_LOAD=1` runs real `DirLoader::LoadObjects` and **instantiates objects** (Sfx ctor runs after a minimal `TheSynth` bring-up). Next blocker (→ Step 2): property-sync `MILO_FAIL`s without `gSystemConfig` `objects` type-defs (needs the real config load). Regression-safe default path unchanged: header dump **4453/4455 milos**, `rb3-dta` still **138 songs**. Engine pin unchanged (`54b9fa0`). |
| 2026-05-27 | 1 | **Phase 1 — rb3-native milestone (b) + scene-tree dump achieved (3-wave parallel-agent session).** Engine published to GitHub (`freeqaz/milo-native-engine`, public). **Wave 1**: ported DC3→RB3 HX_NATIVE matched-fork blocks for `math/` (Key.h STL-insert iterator + fixed 2 latent quat/transform fallback bugs) and `utl/` (BinStream endian for LE-host/BE-file reads, MemMgr exception-specs, Str/MemStream/Locale LP64 — Wave-2 gating headers Symbol/BinStream/MemMgr/Str confirmed clean); docs synced for Phase 0 closure; **link-order divergence root-caused — the premise was false**: `milo-tests` and `milo-engine-tests` both reach `OnFileMerged`→`TheUI->WentBack()` identically; the only difference is the engine harness installs a `TheUI` stub. Duplicate `Memory_Native`/`ThreadCall_Native` symbols are benign (byte-equivalent); no engine remediation needed. **Wave 2**: HX_NATIVE ports for `obj/` (DataNode 8-byte union zero-init, DataArray symbol-pointer LP64 truncation, Task liveness; **ObjRef/ObjDirPtr ring correctly NOT ported** — RB3's 2010-era `Hmx::Object` uses `std::vector<ObjRef*>` not DC3's intrusive ring, so the session-61 double-link bug cannot occur), `rndobj/` (GPU-lifecycle handoff blocks), `synth/` (headless wall-clock audio timer, mem_fn shims, VorbisReader masterKey LP64 bypass); `os/` already clean (no edits). **Wave 3**: added engine `MILO_ENGINE_BUILD_GFX` option (default ON; DC3 unaffected) + consumer `MILO_ENGINE_DECOMP_PLATFORM_EXCLUDE` seam — RB3 builds the engine **GFX-off** because the DC3-wired WebGPU layer can't compile against RB3's older `rndobj` (engine `Part_Wgpu.cpp` needs `RndParticleSys::NumTilesAcross` which RB3 lacks; `WgpuRnd : NgRnd` vs RB3's older `Rnd`). **`rb3-native` links the GFX-off engine + RB3 matched fork and runs to clean exit (FLOOR / §0.3 milestone b)**; **`rb3-native <path.milo_xbox>` dumps the scene tree — 60/60 milos, 0 crashes** across 7 root dir classes (`world [WorldDir]`→FileMerger+CrowdAudio; `cowbell_bank [ObjectDir]`→8 Sfx/SynthSample/RandomGroupSeq) (STRETCH / §1). The dump reads the `ObjectDir` header from the `ChunkStream` (names+types, like `test_dirloader`) — needs zero object factories, so it works before rndobj/synth are native-clean. **Both decomps pinned to engine `54b9fa0`. rb3-dta: still 138 songs. milo-engine-tests: 191/195** (4 `RndCamProjectionTest` failures confirmed **pre-existing**, unrelated to this session — likely clang-22 toolchain drift). Follow-ups: full object-graph load (vs header dump) is blocked by 22/64 `rndobj` + 7/47 `synth` matched-fork TUs not yet clang-LP64-clean (POSIX `wait`/`select` clashes, MWCC MSL headers, switch jump-over-init, `ObjVector.h` template) — Wave-1/2-style matched-fork bring-up; once clean it's a one-line glob re-add + factory re-enable. The 4 RndCamProjection failures and the 5 deferred-glue files (PlatformMgr/RenderState/Skeleton/HttpServer+DebugPanel) remain open. |
| 2026-06-08 | 6 | **Convergence: hack audit → validation sweep → bring-onlines.** Multi-agent audit classified 73 hacks (port is "mostly faithful, isolated disables"); stood up the `rb3-tests` gtest suite (`f51ab466`, now 11/11). Landed bring-onlines: PostLoadVocals (`082bcea4`), GameMicManager (`8d300cd7`), gDeforms+IsExoBone (`a5999979`). A 17-agent **validation sweep** then re-checked every blocked item: **VocalPlayer RTTI-cast guard deleted** (`579e7416`, RTTI now real); **BandHeadShaper female `gHeadMale` typo fixed** (`4e49ef34`, match-positive, proven vs asm); **BandFaceDeform "BE reader blocker" proven a FALSE ALARM** (already byte-correct, 38/38 vs real asset; locked by gtest `15e3c048`). char-customize previews (theme B) **re-designed** with the true crash surface (menu-wide `UpdateCharCache` + unconditional `mFileMerger` hard-crash; audit's "world_chars proves it" premise was false) — runtime-gated, next-session exec. worldcenter occluder + SP scoreboard = documented KEEP. See `NATIVE_HACK_AUDIT_2026-06-08.md` + `BLOCKER_VALIDATION_2026-06-08.md` + the Convergence tracker above. |
| 2026-05-27 | 4–5 | **BOOT-TO-SONG MILESTONE ACHIEVED — the real game boots, navigates menus, and loads a song.** `RB3_GAME=1 rb3-native` (with `RB3_GAME_INPUT="@10:start,@30:confirm,@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn,@320:down,@350:msg:music_library:select_highlighted_node,@450:msg:overshell:end_override_flow:1:0"`) drives the **full real sequence end-to-end**: real `App::App` ctor → `SystemPreInit`+`SystemInit` (227 obj-cfg) → all `*Init` (Char/World/Track/BeatMatch/Band/Song/MetaPanel/Game/etc.) → `BandUI::Init` (= `TheUI.Init()`) → native frame loop → DTA `init_msg` screen-include chain → `intro_movie` auto-skip → `splash_screen` → overshell `attempt_to_add_user` → `kSplashScreen_StartOvershell` → `{ui goto_screen main_hub_screen}` → **`main_hub_screen` settles** → `song_select_enter_screen` → **`song_select_screen` (83 real songs from `songs.dta`)** → SelectNode(kNodeSong) for `20thcenturyboy` → `PlaySetlist`/`move_on_quickplay` → `SyncScreen(part_difficulty_screen)`/`tv3_a_screen` → `GamePanel::CreateGame` → **`Game::Game()` ctor → `Game::LoadSong()` ENTERED → `SongData::Load`** → graceful stop at the absent `.mid` chart (documented asset bound — 360-ARK extract has no `.mogg`/`.mid`). **Decisive root-causes fixed across ~9 parallel-orchestration waves** (all additive `#ifdef HX_NATIVE`, matched `#else` byte-identical): (1) **`Hmx::Object::PreLoad` was weak-stubbed to a no-op** → every leaf `Load()` silently skipped (RndText/RndFont/RndMat/RndTex/CharClip…) — strong def in `obj/Object.cpp`; (2) **headless UI clock was frozen** (PPC `mftb` no-op under clang → `UISeconds()=0` → every timed `UITrigger` hung every screen transition) — native `std::chrono` clock; (3) **DC3's 12 `DirLoader` `HX_NATIVE` native-loader blocks ported** (RB3 had 0) — sync-load state machine, `EofType`/`TempEof`, shared/proxy-dir, `NewObject`-null tolerance; (4) **`OBJ_SET_TYPE` config lookup made tolerant** — class absent from extracted objects.dta → no type-def vs hard-fail; (5) **`#pragma pack(1)` ODR bug** (clang ignores MWCC `pack push/pop`) mis-packed `Synth`; (6) **`ChunkStream::mFail` detects failed binary open** so missing milos fail cleanly (without breaking DTA `#include` leniency); (7) **`CharBonesSamples::LoadData` reads cached Xbox 16-byte-padded sample layout** (vs unpadded Wii); (8) **Wii/net manager-globals brought up as real native objects** (`ThePlatformMgr`/`TheNetSession`/`TheContentMgr` had ctors trapped in excluded Wii TUs → zeroed `MsgSource` → `AddSink` faults); (9) **per-class clang-LP64 bring-up** of MetaPanel + 60-odd menu/panel/manager factories, plus targeted bring-ups for `CharForeTwist`/`CharHair`/`CharacterTest`/`GemTrackDir`/`ChordShapeGenerator`; (10) **rndobj legacy class-name aliases** (`Tex`/`Text`/`Dir` → `RndTex`/`RndText`/`RndDir`) on the game path. **Native infrastructure added** (per-decomp glue, `rb3/native/src/`): `rb3_game_input.cpp` (synthetic headless input — frame-scheduled `ButtonDownMsg`/`select:`/`msg:` directives via the real `Automator`/`TheUI.Handle` path), `rb3_platform_native.cpp`/`rb3_netsession_native.cpp`/`rb3_synth_native.cpp` (real ctors + offline-default impls for typed `TheX` globals whose ctors lived in excluded Wii TUs), `band3_link_stubs.s` (weak no-op stubs for the residual ~20 not-yet-clang-clean band3/bandobj/char gameplay TUs — Wave 1/2 bring-up territory; their symbols satisfied so the boot links). All regression guards GREEN throughout: `RB3_BOOT` 227-cfg/boot-complete, `RB3_RENDER_MESH` 129 meshes/27878 tris/PNG, `rb3-dta` 138 songs. Engine pin unchanged (`9ad4e13`). **Out of scope (next phases):** Phase 3 audio playback (needs `.mogg` assets) + Phase 5 gameplay (needs `.mid` charts + the ~20 residual clang-LP64-gap gameplay TUs — `Singer`/`VocalPlayer`/`GameGemList`/`BandPatchMesh`/etc.) — the **v1 milestone** (one song end-to-end). |

---

## Reference

- **DC3 native port (model)**: `/home/free/code/milohax/dc3-decomp/native/`
- **DC3 native status & sessions**: `/home/free/code/milohax/dc3-decomp/docs/native/NATIVE_PORT_STATUS.md`
- **DC3 convergence plan (Phase 1→2 transition example)**: `/home/free/code/milohax/dc3-decomp/docs/native/CONVERGENCE_PLAN.md`
- **DC3 DTA blocker** (expect equivalent in RB3): `/home/free/code/milohax/dc3-decomp/docs/native/DTA_LOADING_BLOCKER.md`
- **RB3 ↔ DC3 sync plan** (decomp tooling, not native): [SYNC_WITH_DC3.md](../SYNC_WITH_DC3.md)
- **RB3 decomp scope (skip lists carry over for port)**: see "Skip / deprioritize" in [CLAUDE.md](../../CLAUDE.md)
