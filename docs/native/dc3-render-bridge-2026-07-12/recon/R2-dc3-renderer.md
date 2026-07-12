# R2 — DC3 Native Renderer Inventory (as a candidate RB3 backend)

_Lane R2 recon handoff. Read-only study, no builds. Cites the **canonical**
source: the DC3 renderer is not a `dc3-decomp` artifact — it lives in the shared
`milo-native-engine` and is selected by a CMake backend flavor. Paths below are
engine paths unless prefixed. Engine HEAD `0083bad3`; the copies under
`dc3-decomp/native/src/{gfx,platform}` are stale duplicates (the
"native↔engine masking" that was de-duped) — do not cite those._

## Summary

The "DC3 native renderer" is the `dc3` **GPU-backend flavor** of the shared
`milo-native-engine`: `WgpuRnd : NgRnd` plus 6 rndobj-coupled gfx translation
units, all gated by `MILO_ENGINE_GPU_BACKEND=dc3`. RB3-native **already links the
same engine repo**, but selects the `rb3` flavor (`BandRnd : Rnd`,
`Rnd_Wgpu_RB3.cpp`) because RB3's 2010-era `rndobj/` cannot compile the DC3 TUs.
So a "bridge" is not a renderer transplant — both ports share one engine and one
gfx core (GpuDevice/PipelineManager/passes); what differs is which renderer class
consumes which `rndobj` shape. The DC3 flavor hard-requires an `NgRnd` base,
`RndCam::GetViewProjectXfms`, unpacked `RndMesh::Vert`, and
`RndParticleSys::NumTilesAcross/Down` — the exact surface RB3-Wii lacks. The
game-specific coupling is already abstracted behind a `GameRenderHook` seam that
**already carries RB3-shaped policy virtuals**.

## Findings

### 1. Two backend flavors of ONE engine (the central fact)
`milo-native-engine/CMakeLists.txt:96-136,371-394` defines `MILO_ENGINE_GPU_BACKEND ∈ {off,dc3,rb3}`:
- **`dc3`** = `WgpuRnd : NgRnd` + `MILO_ENGINE_GPU_PLATFORM_SOURCES`
  (`MaterialSetup/MeshGpuCache/Mesh_Wgpu/Part_Wgpu/RndTex_Native/Rnd_Wgpu/Tex_Wgpu/TransparentQueue`,
  CMake:304-313) + `MILO_ENGINE_GFX_RNDOBJ_SOURCES` (the 6 coupled TUs
  `DrawRect2D/DofPass/VertexFormats/ShadowPass/PostProcPass/TextureConvert`,
  CMake:281-288).
- **`rb3`** = `BandRnd : Rnd` (`Rnd_Wgpu_RB3.cpp` + `RB3MeshCache/RB3MaterialBinder/RB3HaloPass/RB3PostProc/RB3Quad/RB3TexSharpen`, CMake:321-329). Self-contained; owns its own strong defs for `RndMesh::DrawShowing`, `RndTex::SyncBitmap`, `VertexFormats::Static/SkinnedLayout` (inline CPU DXT + static vert layout).
- **`off`** (RB3 today at gfx-core level) / gfx CORE (`GpuDevice/PipelineManager/Screenshot/FrameCapture/BloomPass/GpuResourceRegistry/UniformRingBuffer/VideoEncoder/mikktspace/ImGuiBackend`, CMake:261-273) is **rndobj-free and shared by both flavors**.

RB3 selects `rb3` explicitly: `rb3/native/CMakeLists.txt:62-70` — "the `dc3` flavor … stays OFF for RB3: RB3-Wii's 2010-era rndobj/ is structurally older than DC3's and cannot compile those TUs (Wave 2.3 finding)."

### 2. Architecture / who owns the frame graph
Layered, top to bottom:
- **`GpuDevice`** (`gfx/GpuDevice.cpp`) — WebGPU/Dawn device+surface+headless readback; GLFW window; sampler cache; BC-compression feature probe. Backend-agnostic (`GpuDevice.cpp:23-43,297-380`).
- **`PipelineManager`** (`gfx/PipelineManager.cpp`, `.h`) — `PipelineKey` → render-pipeline cache; the varianting seam (`GRAPHICS_REFACTOR.md:38-40`).
- **`WgpuRnd : NgRnd`** (`platform/Rnd_Wgpu.{h,cpp}`, `Rnd_Wgpu.h:61`) — **the frame-graph owner**. Owns pass orchestration and scene state. Key overrides/entry points:
  `Init/Terminate/Clear` (`Rnd_Wgpu.cpp:254,403,470`), `BeginDrawing/EndDrawing`
  (`:834,1065`, virtual from `NgRnd`), `BeginFramePass/BeginTexturePass/EndActivePass`
  (`:617,724,474`), `ClearDepthForOverlay/FlushPostProcessingForOverlay` (`:484,538`),
  `SelectRenderTarget/FinishRenderTarget/MakeDrawTarget` (`:805,813,821`),
  `WriteSceneUniforms` (`:1275`), `EnsureSceneUniformsCurrent` (`:1038`),
  `CreateMaterial/Object/BoneBindGroup` (`:1707,1757,1772`), `DrawRect` (`:1851`).
  A global `gWgpuRnd` + `IsInPass()`/`CurrentPass()` gate every draw.
- **Per-primitive backends**: `Mesh_Wgpu.cpp` provides the strong def of
  `RndMesh::DrawShowing` (`:123`) and `DrawMeshImmediate/DrawMeshShadow`
  (`:159,332`); `Tex_Wgpu.cpp`, `RndTex_Native.cpp`, `Part_Wgpu.cpp`
  (RndParticleSys billboards), `MaterialSetup.cpp` (RndMat → GPU uniforms/bind
  group), `MeshGpuCache.cpp` (per-mesh upload cache), `TransparentQueue.cpp`
  (back-to-front sort).
- **Dedicated passes** (`gfx/`): `ShadowPass`, `BloomPass`, `PostProcPass`,
  `DofPass`. Consume `RndCam/Env/Lit/Mat/Mesh/PostProc` (CMake:275-280).

**Entry a foreign scene graph calls**: the *standard Milo traversal* — the game
walks `RndDir`/`ObjectDir` and calls `RndDrawable::Draw` → `RndMesh::DrawShowing`,
which the backend strong-defines. The renderer itself is driven per frame via
`WgpuRnd::BeginDrawing()`/`EndDrawing()` bracketing plus `GameRenderHook`
callbacks (finding 5). There is **no bespoke "submit scene" API** — the contract
is virtual `rndobj` method overrides, so a foreign scene graph integrates by
presenting the same `rndobj` object shapes, not by calling a new interface.

### 3. Engine-side contract (what `rndobj` shape the `dc3` backend demands)
All in `platform/Rnd_Wgpu.cpp::WriteSceneUniforms` and `Mesh_Wgpu.cpp` unless noted:
- **`RndCam`**: `WorldXfm` (`:1004,1244,1320`), `NearPlane/FarPlane` (`:1331`),
  `YFov` (`:1330`), `ZRange` (`:1334`), `GetScreenRect` (`:676,750`), `TargetTex`
  (`:674,748`), `GetViewProjMatrix` (`:1253`), **`GetViewProjectXfms`** (`:1263-1270`),
  `UpdatedWorldXfm` (`:1266`).
- **`RndEnviron`**: `AmbientFogOwner` (`:1346`), `AmbientColor` (`:1348`),
  `FogEnable/FogStart/FogEnd/FogColor` (`:1356-1360`), `LightsApprox` (`:1409`),
  `LightsReal` (`:1530`) — both `ObjPtrList<RndLight>`.
- **`RndLight`**: `GetColor` (`:1379,1543`), `WorldXfm` (`:1382,1537`), `GetType`
  (`:1383,1414,1535,1562` — `kDirectional/kPoint/kFakeSpot`), `Range` (`:1549`),
  `GetTexture` (`:1563,1593`), `Projection` (`:1580`), `Showing/Name`.
- **`RndMesh`**: `Mat/Name/Showing` + geometry via `EnsureMeshUploaded`
  (`Mesh_Wgpu.cpp:149,129,177`); vert layout via `VertexFormats.cpp`
  (`RndMesh::Vert`, CMake:276).
- **`RndMat`**: 9 texture slots + ~30 properties resolved in `MaterialSetup.cpp`
  (documented `RENDERING_SYSTEM.md:19-105`).
- **`RndParticleSys`**: `NumTilesAcross/NumTilesDown` (`Part_Wgpu.cpp:163-164`).

**RB3-Wii gaps that block the `dc3` flavor** (CMake:298-303, `rb3/native/CMakeLists.txt:62-70`):
no `NgRnd` base (RB3 `Rnd` is older/unrelated); no `RndCam::GetViewProjectXfms`;
`RndMesh::Vert` is packed `Color32` not the DC3 unpacked layout; no
`RndParticleSys::NumTilesAcross/Down`. These are the concrete "won't compile"
edges, not vague version drift.

### 4. DC3-game-specific vs reusable scene rendering
**Reusable (game-agnostic scene rendering):** GpuDevice, PipelineManager, scene
uniforms, mesh/tex/material upload, TransparentQueue, ShadowPass/BloomPass/
PostProcPass/DofPass, fog + directional/point/fakespot lights, skinning (40-bone),
RTT infra.

**DC3-game-specific:** Kinect depth-grid mesh skip (`Mesh_Wgpu.cpp:143`
`grid_80by60`), WorldCrowd billboard-impostor RTT (dance-crowd; hook
`RenderCharacterImpostors`, `Part_Wgpu` + `NATIVE_PORT_STATUS.md:42` S77),
Kinect NUI/gesture (`Skeleton_Native.cpp` → `xdk/NUI.h`, CMake:365, DC3-only),
dance-move choreo camera.

**Critically, the game-specific policy is already externalized** behind
`platform/GameRenderHook.{h,cpp}` (`GameRenderHook.h:152-342`; registered via
static `SetGameRenderHook` in a per-game glue TU, e.g.
`dc3-decomp/native/src/dc3_render_hook.cpp:45-86`). Its virtuals include
`DrawGameOverlay`, `RenderCharacterImpostors`, `QueryDrawGeom/Material/HaloPolicy`
— **and already RB3-shaped hooks**: `IsGemMesh` (`:313`), `IsTrackjacketMesh`
(`:327`), `IsBandHandMesh`/`HandBoneRole`/`HandBoneSide` (`:244-255`),
`IsCamDbgHighwayMesh` (`:288`), `IsHubBarMesh` (`:296`), `IsHeadMesh` (`:302`),
`IsSkinDiffuseOutputTex` (`:308`), `IsBandMemberSkeletonFile` (`:226`). Strong
evidence the seam was designed to serve **both** games from one renderer.

### 5. Maturity evidence
- **Stability**: `NATIVE_PORT_STATUS.md:41` (S76) — **21/21 venue×song combos
  stable, zero crashes** (7 venues × 3 songs); `:40` (S75) 6-venue draw-call
  census (416–2011 calls/frame). Phase-0 engine extraction kept `milo-tests`
  **371/371** green (`:43`).
- **Feature coverage** (`RENDERING_SYSTEM.md:187-211`, S69–S77): normal/spec/
  emissive/env/detail-normal/rim maps, 4 dir + 4 point + ambient, linear fog,
  full post-proc (bloom/contrast/saturation/levels/vignette/chromatic-ab/
  posterization/DOF), shadow mapping, particles, RndLine, RndFlare, text/glyph,
  TexMovie RTT, RTT infra. Roadmap Phase 6 ~80% (`NATIVE_PORT_STATUS.md:53`).
- **Known open gfx issues** (`RENDERING_SYSTEM.md:212-220`): motion-blur/
  gradient-map/kaleidoscope/flicker/noise/video-feedback postproc missing; no GPU
  occlusion queries (flares bypass visibility); **WorldCrowd impostor pipeline
  buggy (chars stack at origin)**; projected light cookies unimplemented;
  FakeSpot light type; SpotlightDrawer + reflection-cam RTT consumers untested.
- Architecture is explicitly a **compatibility shape mid-refactor**, not a frozen
  design (`GRAPHICS_REFACTOR.md:31-57` — monolithic shader, implicit material
  contract, informal pass boundaries; Phases 1-6 planned).

## Implications for the bridge

1. **Reframe the mission.** "Let RB3 drive the DC3 renderer" ≈ flip RB3 from the
   `rb3` engine backend to the `dc3` one (or selectively enable dc3 passes on top
   of `BandRnd`). It is a **rndobj-shape adaptation** problem, not a renderer
   port. Both ports already `add_subdirectory` the same engine.
2. **The blocker set is small and enumerable** (finding 3): `NgRnd` base,
   `RndCam::GetViewProjectXfms`, unpacked `RndMesh::Vert` (RB3 packs Color32),
   `RndParticleSys::NumTilesAcross/Down`. R3 (rndobj divergence) should size each.
3. **The game-coupling seam is already solved** (finding 4): `GameRenderHook`
   carries RB3 policy virtuals today; the bridge inherits an integration point,
   not a rewrite.
4. **Two viable scopings**: (a) full `dc3` backend after RB3 rndobj is retrofitted
   to the NgRnd contract; (b) "passes only" — keep `BandRnd`, but feed the shared
   `ShadowPass/BloomPass/PostProcPass` (needs the same RndCam/Env/Lit shapes the
   passes consume). Option (b) is the smaller cut but still trips the same
   `RndCam`/vert-format edges.
5. **Coordination hazard**: engine pins diverge — RB3 pins `b36bcfc`, DC3 pins
   `77eb428b`, engine HEAD is `0083bad3`. Any bridge work must reconcile pins so
   both consumers build the same engine state.

## Confidence + what I could not verify
- **High** on architecture, backend split, and rndobj contract — all
  code/CMake-verified with citations.
- **Medium-high** on DC3-specific vs reusable — inferred from hook + skip lists;
  the full DC3 game glue (crowd/Kinect) was sampled, not exhaustively enumerated.
- **Could not verify (no builds allowed)**: runtime behavior, whether option (b)
  "passes-only" actually links against `BandRnd` without the 6 coupled TUs, and
  the true effort to add `GetViewProjectXfms`/unpacked verts to RB3 rndobj (that
  is R3's job). I cited the canonical engine copies; if the build actually
  compiles the stale `dc3-decomp/native/src` duplicates for any target, line
  numbers there may differ (they should not — masking was de-duped).
