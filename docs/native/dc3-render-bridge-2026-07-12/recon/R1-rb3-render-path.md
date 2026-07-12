# R1 — RB3 native render path today

_Recon lane R1 for the DC3 Render Bridge study (2026-07-12). Read-only; no builds._

## Summary

- RB3-native/-web **already renders through the shared `milo-native-engine`
  WebGPU renderer** — not through `rb3/src/system/rndwii` (Wii GX, excluded from
  the native build) and not through any renderer in `rb3/native/src`
  (`rb3_band_rnd.cpp` is a 0-byte placeholder).
- The active renderer is **`BandRnd : Rnd`** in
  `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp`, selected by the engine's
  `rb3` GPU-backend flavor. It reads RB3's own **2010-era matched-fork
  `rndobj/`** scene-graph objects (compiled into rb3-native) and submits them to
  Dawn/WebGPU.
- `milo-native-engine` is a **fork/shared-extraction of DC3's `native/src`**.
  Both decomps `add_subdirectory` it with a soft SHA pin. RB3 and DC3 **share
  the gfx CORE** (GpuDevice/PipelineManager/BloomPass/Screenshot/…) but use
  **different `Rnd` backend flavors** (`rb3`=BandRnd vs `dc3`=WgpuRnd) because
  RB3's older rndobj shapes can't compile DC3's rndobj-coupled TUs.
- Net: a "bridge from Wii scene graph to the DC3 renderer core" **substantially
  already exists**. The live gap is that BandRnd **reimplements several passes
  inline** (halo/postproc/quad/mesh-cache/material-bind/DXT/vert-layout) rather
  than sharing DC3's mature `gfx/` pass TUs.

## Findings

### 1. The per-frame render call path (game draw loop → WebGPU submit)

Driven by the game's own `App` loop under `HX_NATIVE`, one frame =
`App::RunOneFrame` (`rb3/src/App.cpp:533`):

1. `App.cpp:632` `TheRnd->BeginDrawing()` → `BandRnd::BeginDrawing`
   (`milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp:254`) → `BeginFrame(cam)`.
2. `App.cpp:642/649` `TheUI.Draw()` → matched-fork UI/World draw graph
   (`rb3/src/system/ui`, `world`, `track`, `bandobj`, `char`) → leaf
   `RndMesh::DrawShowing()`. The **engine supplies the strong definition** of
   `RndMesh::DrawShowing` at `Rnd_Wgpu_RB3.cpp:6133` (overriding the matched-fork
   weak stub) → `BandRnd::DrawMesh(mesh)` (`Rnd_Wgpu_RB3.cpp:2563`) →
   `BandRnd::SubmitDraw` (`:2552`) → `mPass.DrawIndexed` (`:2560`).
   Particles route via `RndParticleSys::DrawShowing` → BandRnd's own path
   (`:6551`, single-arg `DrawIndexed`).
3. `App.cpp:652` `TheRnd->EndDrawing()` → `BandRnd::EndDrawing`
   (`Rnd_Wgpu_RB3.cpp:269`) → `EndFrame()` → `mGpu.Queue().Submit(1,&cmd)`
   (`Rnd_Wgpu_RB3.cpp:2120`).

`TheRnd` is the single `gBandRnd` `BandRnd` instance stood up in
`rb3/native/src/main_native.cpp:685` (`gBandRnd.InitGpu(...)`); the boot spine
`SystemPreInit → TheRnd->PreInit/Init` is noted at `main_native.cpp:580`.
On web, `main_web.cpp` calls `App::RunOneFrame` directly (same body).

**Which implementation actually executes:**
- `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` (BandRnd) — **YES, the renderer**.
- `rb3/src/system/rndobj/*` — **YES, compiled** (matched fork; provides the
  RndMesh/RndMat/RndCam/RndTransformable/RndTex scene objects BandRnd reads).
  Confirmed in the `rb3` NATIVE_FORK_SOURCES set (`rb3/native/CMakeLists.txt:295`,
  `ENGINE_RNDOBJ` glob `:250`); the GFX exclusion regex `(Dx9|Wgpu|Gpu|D3D|GX)`
  drops only the platform-render TUs (`native/CMakeLists.txt:~330`).
- `rb3/native/src/rb3_band_rnd.cpp` — **NOT used** (0 bytes; historical placeholder).
- `rb3/native/src/rb3_render_hook.cpp` — **used only for policy**, not drawing
  (see finding 4).
- `rb3/src/system/rndwii/*` (Wii GX renderer) — **NOT compiled** into native
  (no rndwii glob in `native/CMakeLists.txt`; GX TUs regex-excluded).

### 2. `milo-native-engine` ↔ DC3 `native/src`: shared fork, two backend flavors

`milo-native-engine` is freeqaz's standalone repo
(`git@github.com:freeqaz/milo-native-engine.git`) described by its own README as
the "shared, cross-platform LP64 runtime the decomp repos link against"
(`milo-native-engine/README.md:1-15`). Both decomps consume it identically:
- RB3: `add_subdirectory` + `MILO_ENGINE_PIN b36bcfcd…` (`rb3/native/CMakeLists.txt:74,224`).
- DC3: `add_subdirectory` + `MILO_ENGINE_PIN 77eb428b…` (`dc3-decomp/native/CMakeLists.txt:225-227`).
  (DC3 pins an **older** engine SHA than RB3.)

**File-list diff (structural, not line-level):**
- `gfx/`: engine == DC3 except engine adds `UniformRingBuffer.{cpp,h}`,
  `UniformStructs.h`, `GpuDevice.h`, `ImGuiBackend_Web.cpp`, `Shaders/`. So the
  gfx core is a near-verbatim shared copy.
- `platform/`: engine has RB3-only additions **absent from DC3's tree**:
  `Rnd_Wgpu_RB3.{cpp,h}`, `RB3HaloPass`, `RB3MaterialBinder`, `RB3MeshCache`,
  `RB3PostProc`, `RB3Quad`, `RB3TexSharpen`, `GameRenderHook.{cpp,h}`,
  `NativeCompatFlags.*`, `MeshGpuCache.h`, `RB3DrawLogDebug.h`, plus the Web
  layer (`*_Web.cpp`, `WebAssets`). DC3's tree has Xbox-only stubs the engine
  lacks (`Achievements/ContentMgr/DingoSvr/GestureMgr/KinectShare/Memcard/
  NetXbox/SongSortMgr/System_Native`, `Xma*`).
- **Caveat:** DC3 still keeps its own `native/src/{gfx,platform}` copies of the
  shared files (legacy duplicates that predate the extraction — see MEMORY
  `project_dc3_native_engine_masking.md`, "stale duplicates shadow engine").
  The canonical shared copy is now the engine's.

**Two backend flavors (engine `MILO_ENGINE_GPU_BACKEND`, CMakeLists.txt:91-139):**
- Shared gfx CORE (both flavors, `MILO_ENGINE_GFX_SOURCES`,
  `milo-native-engine/CMakeLists.txt:261-273`): `GpuDevice`, `Screenshot`,
  `PipelineManager`, `FrameCapture`, **`BloomPass`**, `GpuResourceRegistry`,
  `UniformRingBuffer`, `VideoEncoder`, `mikktspace`, `ImGuiBackend`.
- `dc3` flavor = `WgpuRnd : NgRnd` + **6 rndobj-coupled gfx TUs**
  (`DrawRect2D`, `DofPass`, `VertexFormats`, `ShadowPass`, `PostProcPass`,
  `TextureConvert`; `CMakeLists.txt:281-289`) + **8 Wgpu platform TUs**
  (`MaterialSetup`, `MeshGpuCache`, **`Mesh_Wgpu`**, **`Part_Wgpu`**,
  `RndTex_Native`, **`Rnd_Wgpu`**, **`Tex_Wgpu`**, **`TransparentQueue`**;
  `:298-315`).
- `rb3` flavor = `BandRnd : Rnd` + RB3-only passes
  (`Rnd_Wgpu_RB3`, `RB3MeshCache`, `RB3MaterialBinder`, `RB3HaloPass`,
  `RB3PostProc`, `RB3Quad`, `RB3TexSharpen`; `CMakeLists.txt:317-329`).
- **RB3 gets the gfx CORE but NOT the 6 coupled TUs nor DC3's Mesh/Part/Tex/
  TransparentQueue** — they read DC3's 2012-era `RndMat/RndCam/RndMesh::Vert/
  RndParticleSys` shapes that RB3's 2010-era rndobj lacks (documented
  `CMakeLists.txt:274-297`, and RB3 `native/CMakeLists.txt:63-69`, "Wave 2.3
  finding"). BandRnd therefore re-implements DXT decompress + static vertex
  layout + material binding + mesh caching **inline** (`Rnd_Wgpu_RB3.h:12-16`).

**Bridge implication is direct:** DC3's Bloom is already shared and running for
RB3; DC3's Shadow/PostProc/TransparentQueue/Mesh/Part are the passes BandRnd
reimplements. "Adopt DC3's renderer" = either bring RB3 rndobj shapes up so the
6 coupled TUs + Wgpu platform TUs compile, or author RB3 variants — not a
green-field bridge.

### 3. Where Wii-lineage assumptions leak into RB3's render path

- **The scene graph itself is Wii-era.** RB3's matched-fork `rndobj/` (RndMesh::
  Vert with packed `Color32`, DXT1/3/5 textures, the slimmer 2010 `RndMat`,
  `RndCam` without `GetViewProjectXfms`) is exactly what BandRnd was written to
  translate (`Rnd_Wgpu_RB3.h:11-16`). This is the fundamental leak: BandRnd
  exists because RB3's render objects are structurally older than DC3's.
- **CPU DXT decompression** lives in BandRnd (`Rnd_Wgpu_RB3.cpp:382+`,
  `~499/520/554` memcpy DXT blocks) because Wii textures ship DXT and there is no
  GX texture unit.
- **`rndwii/Rnd.h` (WiiRnd) header-type leak** in the crowd imposter:
  `rb3/native/src/rb3_crowd_imposter_native.cpp:40` includes `rndwii/Rnd.h` for
  the `WiiRnd::SharedTexType` enum (compiled into native — `native/CMakeLists.txt
  :593,909`). It deliberately does **not** use a real WiiRnd; the WiiRnd methods
  (`PrepareRenderAlley`, `RestoreRenderAlley`, `SetTriFrameRendering`,
  `GetSharedTex`) are weak-stubbed (`band3_link_stubs.s:3568-3580`,
  `band3_stub_table.inc:331-333`). So it is a type/enum leak, not a live GX path.
- **GX display-pipeline latency compensation** — a per-frame frame-index offset
  the Wii applied in `Game::Poll` to compensate GX latency is referenced/neutered
  natively (`main_native.cpp:663`; `rb3_synth_native.cpp:49` notes "the native/
  web WebGPU renderer has no such [GX pipeline latency]").
- The rndwii dir is otherwise fully excluded (no glob; `_GX_`/`Wgpu`/`Gpu` TU
  regex, `native/CMakeLists.txt`).

### 4. RB3-native-only render features / hacks

- **RB3-only engine passes** (no DC3 counterpart; `milo-native-engine/CMakeLists
  .txt:317-329`): `RB3HaloPass` (additive gem-bloom capture/replay),
  `RB3PostProc` (Stage-2 composite/grade — *not* the shared `gfx/PostProcPass`),
  `RB3Quad` (2D quad / DrawRect / outfit compose — *not* `gfx/DrawRect2D`),
  `RB3MaterialBinder`, `RB3MeshCache` (twin of DC3 `MeshGpuCache`),
  `RB3TexSharpen` (progressive in-session texture sharpen).
- **`GameRenderHook` per-draw policy seam.** RB3 supplies `BandRenderHook`
  (`rb3/native/src/rb3_render_hook.cpp:39`). Whole-frame overlay/impostor hooks
  are **no-ops** for RB3 (`:44,50`); what it actually provides is per-draw
  content-name policy relocated out of the engine renderer: hub highlight-bar
  world-xfm override, scrollbar-thumb xfm reuse, shard-guard exemption
  (`:66-105`), and facehair/hair skeleton-rebake selection (`:117-142`).
- **`NativeCompatFlags` env-flag system** (`milo-native-engine/src/platform/
  NativeCompatFlags.*`; **333 rows** in `NativeCompatFlags.gen.inc`, ~415 in the
  Wave-30 census per MEMORY). Many gate render behavior:
  `RB3_PROP_POSE_FULL`, `RB3_NO_HUB_BAR_PLACEMENT_FIX`,
  `RB3_SCROLLBAR_THUMB_FIX_OFF`, `RB3_NO_SKEL_REBAKE`, `RB3_CLEAR_COLOR`
  (`main_native.cpp:683`), `RB3_VENUE_POLL_OFF` (`App.cpp`), `RB3_CROWD_IMPOSTER_OFF`.
- **Crowd imposter RTT** (`rb3_crowd_imposter_native.cpp`) + the `HX_NATIVE`
  `kFastBillboardXYZ` camera-facing branch in `rndobj/MultiMesh.cpp`.
- **31-wave engine-arch-review campaign** fixes (point-radial shard meshes,
  emissive glow, depth occlusion, patch-mesh composites) — the render defect
  families that motivate this study (`docs/native/engine-arch-review-2026-07-05/`,
  `docs/native/NATIVE_HACK_AUDIT_2026-06-08.md`).

## Implications for the bridge

1. **The bridge is ~70% already built.** RB3's Wii scene graph already drives the
   shared engine's WebGPU core (GpuDevice/PipelineManager/BloomPass). The
   architect should frame the work as **backend convergence**, not new bridge
   construction: retire BandRnd's RB3-only reimplementations
   (RB3PostProc/RB3Quad/RB3HaloPass/RB3MeshCache/RB3MaterialBinder) in favor of
   the shared `gfx/` passes DC3 uses.
2. **The blocker is rndobj shape drift, not renderer architecture.** The 6
   rndobj-coupled gfx TUs + DC3's `Mesh_Wgpu/Part_Wgpu/Tex_Wgpu/TransparentQueue`
   don't compile against RB3's 2010-era rndobj. A bridge scoped to "scene
   graphics only" reduces to: (a) uplift RB3 rndobj to DC3-compatible shapes, or
   (b) write RB3-flavored variants of those 6+8 TUs. R3's rndobj divergence
   inventory is the gating input here.
3. **`GameRenderHook` is the clean seam.** The engine already isolates
   game-specific draw policy behind an abstract hook that names no RB3 type — the
   right extension point for any new bridge behavior, keeping the shared renderer
   game-agnostic.
4. **DC3's `TransparentQueue` is a concrete win to target.** RB3 currently has no
   shared transparent-sort queue; several defect families (glow/depth/patch
   composites) are ordering/transparency bugs BandRnd handles ad hoc.

## Confidence + what I could not verify

- **High confidence** on the static architecture (call path, backend flavors,
  file inventories, CMake gating) — all cited to source and build config.
- **Not verified at runtime.** Per the hard rules I ran no build and did not
  execute rb3-native, so I could not confirm the frame actually flows through
  these functions live (only that the source wires them and the engine strong
  defs override the weak stubs). `RndMesh::DrawShowing` strong-override is
  asserted from the file comment + symbol placement, not from a link map.
- **DC3-side duplication is inferred**, not fully audited: I confirmed DC3 keeps
  its own `native/src/{gfx,platform}` copies and consumes the engine, but did not
  verify which copy DC3's link line actually pulls (its CMake may exclude the
  local duplicates like RB3 does). Flagged for R2.
- Flag counts are from `NativeCompatFlags.gen.inc` (333) vs the Wave-30 census
  (~415 per MEMORY) — the discrepancy is real-vs-census bookkeeping, not
  load-bearing here.
