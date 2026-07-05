# Lane 01 — Renderer Core Anatomy & Decomposition Plan

**Reviewer:** Opus review agent (renderer-core lane)
**Date:** 2026-07-05
**Scope:** `milo-native-engine/src/platform/Rnd_Wgpu_RB3.{cpp,h}` (the RB3 backend),
`Rnd_Wgpu.{cpp,h}` + `Mesh_Wgpu.cpp` (the DC3 backend), `src/gfx/*`, and the
`rb3/native/src/rb3_render_*.cpp` glue.
**Verdict for this subsystem: OVERHAUL** (structural, staged — see §5).

---

## Executive summary

The RB3 render backend is a **single 7,017-line translation unit** (`Rnd_Wgpu_RB3.cpp`)
that reimplements, inline and monolithically, everything the DC3 backend splits across
**eight** files. Its `DrawMesh` alone is **~2,670 lines** (lines 3837–6510). It carries
**113 `getenv` calls / ~90 distinct runtime flags**, and — most damaging architecturally —
it branches on **~30 hardcoded RB3 asset name strings** (`"scrollbar.mesh"`, `"prism_gem"`,
`"gem_smasher"`, `"highlight_main"`, `"goatee"`, `"fingernails"`, `"trackjacket"`,
`"peakstate"`, …) to apply per-asset placement, lighting, and skinning-shard workarounds.

This is the structural root of the bug family the review was convened over. The faithful
Milo engine is **data-driven** (behavior flows from `RndMat`/`RndEnviron`/`RndTransformable`
data); this backend is **content-coupled and heuristic** (behavior is bolted on per-asset-name).
Every "fix" that lands is another `strcmp` special-case, so the file grows without the
underlying pipeline (skinning, transform propagation, lighting) ever being made correct.
That is why *faithful* BandPatchMesh rewrites broke rendering twice: the faithful data path
runs headfirst into a renderer whose correctness depends on name-matched exceptions.

Concrete measurements below. The recommendation is a staged extraction: **mechanical
carve-out first** (mesh cache, material setup, uniform rings, postproc, shaders → files),
**seam introduction second** (a `GameRenderHook`-style boundary so game-specific behavior
leaves the engine), **behavioral correctness third** (retire name-string workarounds as the
general pipeline is fixed). Moves and behavior changes must never be mixed in one commit.

---

## 1. Responsibility map

### 1a. What each file actually does (MEASURED — `wc -l`, function maps)

**RB3 backend — one file does everything:**

| File | Lines | Role |
|---|---|---|
| `platform/Rnd_Wgpu_RB3.cpp` | **7,017** | The entire RB3 GPU backend. `class BandRnd : public Rnd`. |
| `platform/Rnd_Wgpu_RB3.h` | 660 | ~120 members: 4 uniform rings, pipeline mgr, halo/bloom/postproc/quad/compose/particle pipelines all as members. |
| `platform/RB3TexSharpen.cpp` | 415 | Progressive in-session texture sharpen (research/13); the only *other* file in the rb3 GPU source set. |

`Rnd_Wgpu_RB3.cpp` function map (MEASURED, line anchors):
`InitScreenshots`(180) · `BeginDrawing`(215) · `EndDrawing`(230) · `PreInitRender`(295) ·
`CreateDefaultTextures`(942) · `StartGpuInit`(992) · `InitGpuResources`(1015) · `InitGpu`(1068) ·
`Shutdown`(1086) · `WriteSceneUniforms`(1297) · `BeginFrame`(1590) · `EndFrame`(1746) ·
`MakeMaterialBindGroup{Raw,Cached,}`(1815/1863/1884) · `BeginDrawTarget`(2088) · `EndDrawTarget`(2151) ·
`FlushPostProcMidFrame`(2212) · **halo/bloom block** `HighwayBloomEnabled`(2275)…`CompositeHaloBloom`(2471) ·
`DoPostProcess`(2603) · `ClearDepthForOverlay`(2616) · `EnsureIntermediate/Depth`(2924/2952) ·
`RunPostProcComposite`(3025) · `EnsureQuadPipeline`(3327) · `DrawRect`(3507) ·
**`DrawMesh`(3837–6510, ~2,670 lines)** · `WarmGpuForDir`(6510) · weak-stub strong defs
`RndMesh::DrawShowing`(6543)/`RndMesh::OnSync`(6547)/`RndTex::*`(6579–6593) ·
`EnsureParticlePipeline`(6673)/`DrawParticles`(6706).

**DC3 backend — the same responsibilities, decomposed across 8 files (the contrast):**

| File | Lines | Role that RB3 inlines into `Rnd_Wgpu_RB3.cpp` |
|---|---|---|
| `platform/Rnd_Wgpu.cpp` (`WgpuRnd : NgRnd`) | 1,861 | Frame/pass lifecycle, scene uniforms, bind-group factories, postproc-for-overlay, DrawRect. |
| `platform/Mesh_Wgpu.cpp` | 406 | `DrawMeshImmediate`(159–332, **~170 lines**) — the mesh draw. **RB3's equivalent is 2,670 lines.** |
| `platform/MeshGpuCache.cpp` | 365 | Per-mesh VB/IB upload cache. RB3 reimplements as inline `RB3MeshEntry` map. |
| `platform/MaterialSetup.cpp` | 304 | `BuildMaterialParams` → material uniforms. RB3 inlines (see refs at RB3 5766, 5974). |
| `platform/Tex_Wgpu.cpp` | 325 | Texture upload/format. RB3 inlines DXT decompress + upload. |
| `platform/Part_Wgpu.cpp` | 327 | Particle billboards. RB3 has its own `DrawParticles`/`EnsureParticlePipeline`. |
| `platform/TransparentQueue.cpp` | 143 | Depth-sorted transparent pass. **RB3 does NOT use it at all** (see §2f). |
| `platform/RndTex_Native.cpp` | 86 | RndTex glue. RB3 inlines `RndTex::SyncBitmap` etc. |

The two backends are selected mutually-exclusively at configure time
(`MILO_ENGINE_GPU_BACKEND={off,dc3,rb3}`, `CMakeLists.txt:112–131`). The rb3 source set
(`MILO_ENGINE_GPU_PLATFORM_SOURCES_RB3`, `CMakeLists.txt:322`) is deliberately
**self-contained** — the CMake comment (`CMakeLists.txt:315–320`) states it "owns its strong
defs … so it needs neither `MILO_ENGINE_GFX_RNDOBJ_SOURCES` nor the dc3 platform backends."
That self-containment is exactly the duplication.

### 1b. What `Rnd_Wgpu_RB3.cpp` does that should live elsewhere (MEASURED)

- **A per-mesh GPU upload cache** (`RB3MeshEntry`, fingerprint-keyed VB/IB reuse, DrawMesh
  ~4000–4410). Belongs in a `MeshGpuCache`-equivalent. Comment at RB3:389 even cites
  `MeshGpuCache::EnsureMeshUploaded`.
- **Material → uniform translation** (blend/zmode clamps, emissive slot, alpha handling,
  RB3 ~5760–6300). Belongs in a `MaterialSetup`-equivalent; comments at 5766 / 5974 point at
  `MaterialSetup::BuildMaterialParams`.
- **CPU vertex unpack / DXT decompress** (`RB3UnpackMeshVerts`, `Be*`/`Half2Float`,
  L1 unpack cache). Belongs in a vertex/format helper (mirrors `VertexFormats` +
  `Tex_Wgpu`).
- **Postproc composite** — RB3 inlines `kRB3PostProcShaderSource` + `RunPostProcComposite`
  (3025–3327) that the header comment (2665, 3033, 3060) says is "ported from
  `PostProcPass::Run`". `gfx/PostProcPass.cpp` already exists and is used by DC3.
- **Quad/rect 2D** — inline `kRB3QuadShaderSource` + `EnsureQuadPipeline` + `DrawRect`
  duplicate `gfx/DrawRect2D.cpp`.
- **Game-specific render behavior** — the ~30 asset-name branches (§1c). None of this
  belongs in an engine renderer; DC3 routes game-specific passes through `GameRenderHook`
  (see `rb3_render_hook.cpp:1–26`, which describes the seam — *but RB3 never links the hook,
  so the seam is unused and the logic lives inline instead*).

### 1c. Content-coupling: the renderer knows RB3 asset names (MEASURED — the core finding)

`grep` for name special-cases in `Rnd_Wgpu_RB3.cpp` returns **30** `strcmp/strstr` sites on
mesh/material/dir names. A representative sample (counts from `uniq -c`):

```
prism_gem (3)  head.mesh (3)  highlight_main (3)  highlight_pattern (3)
surface (2)  trackjacket (2)  envNm "char" (2)  skeleton_unshared.milo
gem_smasher / gem_smasher_glow  peakstate  scrollbar.mesh / scrollbar_bg.mesh
goatee  fingernails  facehair  eyebrow  mohawk  hair  bedhead  blownback
plaidshirt  vestdenim  shirt  jacket  tongue  tail_  crowd  extras  _source.mesh
matName: label / icon / font   dt: skin_diffuse_output
```

Each is a workaround for a general-pipeline defect the backend never fixed. Examples with
line anchors:
- **Scrollbar thumb placement** (DrawMesh ~4536–4562): `strcmp(mesh->Name(),"scrollbar.mesh")`
  copies the *previous* draw's world xfm because the transform pipeline doesn't propagate the
  ScrollbarDisplay parent xfm to a shared skinned UI dir. `RB3_SCROLLBAR_THUMB_FIX_OFF`.
- **Hub highlight bar** (DrawMesh ~4505–4535): `highlight_main`/`highlight_pattern` forced to
  identity world because a skinned UI mesh's WorldXfm doesn't reach the bone parent chain.
  `RB3_NO_HUB_BAR_PLACEMENT_FIX`.
- **Character part shading** — `goatee`/`hair`/`fingernails`/`facehair`… (~5900–6250) gate the
  C8 face/skin lighting and fret-glow branches.
- **Highway halo** — `prism_gem`/`gem_smasher`/`surface` drive the capture-replay bloom
  (`IsHaloSourceMat`, 2304).

These are not bugs in the individual `strcmp`s; the *pattern* is the bug. A data-driven engine
would key all of this off material/transform data, not asset identity.

---

## 2. Frame data-flow (MEASURED trace of one frame)

`BeginFrame(cam)` (1590) → draws → `EndFrame()` (1746). Trace:

**2a. Scene/camera uniforms.** `BeginFrame` resets all 4 rings
(`mSceneRing/mMaterialRing/mObjectRing/mBoneRing.Reset()`, 1651–1655), bumps `sFrameSeq`
(1649), clears `mHaloDraws` (1665), acquires the frame view, sizes depth, then calls
`WriteSceneUniforms(cam)` (1673). `WriteSceneUniforms` (1297) fills a `SceneUniforms` (view/proj,
lights from `RndEnviron::sCurrent`), writes it into `mSceneRing` at a fresh offset (1570), and
**creates a brand-new `mSceneBindGroup`** (1581). The main pass opens and binds it once (1723).

**2b. Mid-frame scene re-writes (the leak surface).** `DrawMesh` re-checks the current camera
on *every mesh* (3895–3940):
- **V2/V13** (3912–3925): if `RndCam::sCurrent` changed *or* the same cam object was re-posed
  (compares eye+forward against `mLastSceneCamPose`), it calls `WriteSceneUniforms(sCurrent)`
  again — **allocating another scene bind group mid-frame** — and re-binds group 0. This exists
  because `TrackDir::DrawShowing` re-poses `game.cam` repeatedly to scroll the highway (comment
  3901–3910).
- **Per-environ** (3934–3940): under `world.cam` with venue lighting on, re-writes scene
  uniforms whenever `RndEnviron::sCurrent` changes, so venue meshes get their scoped environ.

  → **`mSceneBindGroup` is a single mutable member re-created an unbounded number of times per
  frame.** Every draw implicitly depends on "whatever the last `WriteSceneUniforms` left in the
  member." This is order-dependent global state: correctness hinges on the engine calling
  `Select()`/re-posing cams in exactly the sequence the heuristics anticipate. This is a
  first-class **state-leak vector** and a plausible contributor to lighting/placement bugs when
  draw order shifts (e.g. after a faithful mesh rewrite changes submission order).

**2c. Per-draw object/material/bone state.** Each draw claims a **persistent per-(mesh,instance)
uniform slot** keyed by `sFrameSeq` (DrawMesh ~4560–4760): an object-uniform buffer + bind group
per slot, a bone palette buffer + bind group for skinned meshes, and a material bind group. The
material bind group is built from `MakeMaterialBindGroup` (1884) / cached variant (1863). The
pipeline is chosen by a `PipelineKey` (6302–6353: blend/zmode/cull/layout/format/alphaCut) and
fetched from the shared `mPipelines` (`PipelineManager`, which owns `standard_wgsl.inc`). Final
binding (6376–6383): `SetPipeline` + 4 bind groups (0 scene / 1 mat / 2 obj / 3 bone) + VB/IB +
`DrawIndexed`. **The main mesh shader is the shared `standard_wgsl.inc` via `PipelineManager`** —
that part is *not* duplicated; only everything around it is.

**2d. Transparent queue / draw ordering.** **There is none.** RB3 draws in engine submission
order. `TransparentQueue` is linked only for the DC3 backend (grep: users are `TransparentQueue.cpp`,
`Rnd_Wgpu.cpp`, `Mesh_Wgpu.cpp` — *not* `Rnd_Wgpu_RB3.cpp`). Alpha blending correctness therefore
depends entirely on the engine's authored draw order plus `zMode` per material. HYPOTHESIS: some
transparency/z-fighting artifacts and the text-over-3D handling (forced `zMode=Disable` for text,
6318) are consequences of having no sorted transparent pass.

**2e. Bloom capture-replay + postproc interleave.** During the main pass, halo-source draws
(`IsHaloSourceMat`, name-gated) are **captured** into `mHaloDraws` — recording the *live*
`mSceneBindGroup` handle plus mat/obj/bone groups and buffers (6363–6371). Because the scene
bind group is re-created (not overwritten in place) per re-pose, and the ring is only `Reset` at
the next `BeginFrame`, the captured handle stays valid for this frame. At `EndFrame`,
`CompositeHaloBloom` (2471) opens a fresh pass, **replays** the captured draws into a transparent
target (2530–2542), blooms it, and additive-blits only the halo. Postproc: if
`RndPostProc::Current()` is set (and `RB3_PP_OFF` unset), `BeginFrame` redirects the main pass to
`mIntermediateView` (1695–1700); `EndFrame`/`RunPostProcComposite` (3025) grades it onto the
framebuffer with the inline `kRB3PostProcShaderSource`. `FlushPostProcMidFrame` (2212) and
`ClearDepthForOverlay` (2616) exist to punch overlays through mid-frame.

**2f. Where state leaks between draws (summary of suspected root causes):**
1. **`mSceneBindGroup` mutable member** re-created mid-frame; draws depend on last write (§2b).
2. **`mLastSceneCamPose` / `mLastSceneEnv` / `mLastSceneCam`** heuristic dirty-tracking — a
   6-float pose compare (3915–3919) decides whether to re-project. Miss it and geometry projects
   against the wrong view (the exact V13 bug the code documents).
3. **`mRtActiveTex` / RTT suspend-resume** (`BeginDrawTarget`/`EndDrawTarget`, 2088/2151) mutate
   pass/target members; a mis-paired begin/end leaks the RT pipeline variant into subsequent
   draws (the code has a defensive close in `EndFrame`).
4. **`mComposeHaveDiff` / `mComposeColor2`** (header 340–342) — outfit-tint compose state carried
   across `DrawRect` calls; "reset each base layer" by convention, not by structure.
5. **No transparent sort** (§2d) — ordering is implicit global state owned by the caller.

---

## 3. Shader census (MEASURED)

**No `.wgsl` files exist.** All shaders are inline C++ string literals or the one shared
`.inc`. Two classes:

**Shared, file-backed (good):** `src/gfx/standard_wgsl.inc` (38.6 KB) holds the main
mesh shader (`vs_main`/`vs_skinned`/`fs_main`, structs `SceneUniforms`/`MaterialUniforms`/
`ObjectUniforms`/`LightingTerms`). Both backends reach it through `PipelineManager` (the only
`#include` of it, `PipelineManager.cpp:39`; RB3 uses it via `mPipelines`). This is the right
model — one file, hot-reloadable (`PipelineManager::ReloadShaders`, 208–222).

**Inline C++ raw-string shaders (the problem)** — `@vertex`/`@fragment` counts by file:

| File | entry pts | Inline modules |
|---|---|---|
| `Rnd_Wgpu_RB3.cpp` | 11 | **5 inline modules:** halo-blit (`kRB3HaloBlitShaderSource`, 2350), postproc (`kRB3PostProcShaderSource`, 2703), quad (`kRB3QuadShaderSource`: `vs_rect`/`fs_rect`/`fs_rect_notex`, 3267), compose-interp (`kRB3ComposeShaderSource`, 3376), particle (`kRB3ParticleShaderSource`, 6631). |
| `gfx/BloomPass.cpp` | 6 | shared |
| `gfx/DofPass.cpp` | 4 | shared (DC3 only) |
| `gfx/DrawRect2D.cpp` | 3 | shared (DC3 only) — **duplicates RB3's inline quad shader** |
| `gfx/PostProcPass.cpp` | 2 | shared (DC3 only) — **duplicates RB3's inline postproc shader** |
| `gfx/ShadowPass.cpp` | 2 | shared (DC3 only) |
| `standard_wgsl.inc` | 3 | shared (both) |

**Variant selection:** the main mesh shader has **no string concatenation / no `#define`
permutation** — variation is entirely runtime via `PipelineKey` (blend/zmode/cull/vertex-layout/
target-format/alpha-cut/depth) selecting a pre-created pipeline over the *same* WGSL. That is a
sound design. The RB3 inline modules are fixed strings, not permuted.

**Costs (MEASURED/structural):**
- **No shader files, no external validation.** Every WGSL is a C++ string; a syntax error
  surfaces only at runtime `CreateShaderModule`. No editor tooling, no offline `naga`/`tint`
  validation, no diff-review of shader logic as shader code.
- **Combinatorial drift between backends.** RB3's inline postproc (2703) is a hand-port of
  `gfx/PostProcPass.cpp` (comments 2665/3033/3060 cite exact line ranges it was copied from);
  RB3's inline quad shader duplicates `gfx/DrawRect2D.cpp`. Two copies of the same effect drift
  independently — a fix to the grade curve in one is invisible to the other.
- **Lighting logic is trapped in `fs_main` inside the `.inc`** with no unit-testability; the
  "synthetic lighting" hacks live half in `WriteSceneUniforms` (CPU) and half in the shared
  fragment shader, so a faithful-lighting rewrite touches both the C++ heuristics and the WGSL.

---

## 4. Flag census (MEASURED — `getenv` in the RB3 render path)

`grep -c getenv Rnd_Wgpu_RB3.cpp` = **113 calls**, **~90 distinct names**. Classified
(D = debug probe / one-shot diagnostic, W = workaround toggle for an unfixed bug, F = legit
feature/perf toggle). Defaults inferred from the `getenv(...) ? … : …` latch at each site.

| Flag | Class | Default | Note |
|---|---|---|---|
| `RB3_RENDER_DBG` (8×), `RENDER_DBG`, `CAM_DBG`, `RB3_TIER2_DBG` (5×), `RB3_HEADMAT_DBG` (5×) | D | off | Draw/cam/material trace spam. |
| `SKIN_PROBE`, `VERT_PROBE`, `BONE_PROBE`/`_NAME`/`_MINFRAME`, `SLOT_PROBE`, `MESH_DUMP`, `GEM_VTX`, `HUB_BAR_PROBE`, `C8_PROBE`/`C8_EVERY`, `PART_PROBE`, `RB3_LIGHT_PROBE` (2×), `RB3_VENUE_PROBE`, `CHAR_DBG` | D | off | One-shot geometry/bone/skin diagnostics. |
| `SHARD_DBG`/`_CATCH`/`_GUARD_OFF`(2×)/`_RATIO_DBG`(2×)/`_BONE_DBG`, `IK_SHARD_VERT`(2×), `SKEW_PROBE`, `SKIN_CLAMP_PROBE`, `SKEL_REBAKE_PROBE`, `REBIND_DRAW_SKINPOS`/`_FLING`, `XBONE`/`XBONE_TRACK`, `SMASH_DBG`, `CHAIN_*`(6×) | D | off | Skinning-shard investigation probes (the deform saga). |
| `RB3_SKIP_SKINNED`, `RB3_SKIP_STATIC`, `RB3_ISOLATE_MESH`, `GEM_FORCE`, `RB3_BONES_IDENTITY`, `RB3_RECOMPUTE_OFFSETS` | D | off | Render-bisection switches. |
| `RB3_NO_PRECLEAR`, `RB3_NO_MESH_CACHE`, `RB3_UNPACK_CACHE_OFF`, `RB3_PP_OFF`, `RB3_BC_TEX_OFF`, `RB3_PIPELINE_PREWARM_OFF`/`_NOCHUNK`/`_PER_FRAME`, `RB3_PREWARM_DBG` | F(A/B) | feature ON | Perf features with an A/B opt-out canary (house style). |
| `RB3_BLOOM_OFF`/`_SCALE`/`_THRESH`/`_BLEND`, `RB3_HIGHWAY_BLOOM_OFF`/`_THRESH`/`_BLEND`, `RB3_NOISE_OFF`, `RB3_COMPOSE_MULT_OFF` | F | ON | Bloom/postproc tuning. |
| `RB3_VENUE_LIGHT_OFF`, `RB3_TRACK_LIGHT_OFF`, `RB3_CHAR_REAL_LIGHT_OFF`, `RB3_VENUE_POINT_FALLOFF_LEGACY`, `RB3_CROWD_DIM`/`_OFF`, `RB3_FRET_GLOW_OFF` | **W** | ON | **Synthetic-lighting workarounds** — each toggles a hack that stands in for real lighting. |
| `RB3_SCROLLBAR_THUMB_FIX_OFF`, `RB3_NO_HUB_BAR_PLACEMENT_FIX`, `RB3_NO_HUB_HIGHLIGHT_FIX`, `RB3_NO_HUB_BAR_SHARD_EXEMPT`, `RB3_GUARD_EXEMPT_REBOUND` | **W** | ON | **Placement/UI workarounds** keyed off asset names (§1c). |
| `RB3_BAND_SHARD_WORLDFLOOR`/`_WORLDCAP`/`_RATIOCAP`, `SHARD_GUARD_OFF`, `RB3_NO_SKIN_CLAMP`, `RB3_NO_SKEL_WORLDFIX`, `RB3_NO_SKEL_REBAKE` | **W** | ON | **Skinning-shard clamps/guards** — backstops for the deform bug, not fixes. |
| `RB3_SMASHER_HALO`, `RB3_HIGHWAY_WATERMARK_OFF`/`_DIM`, `RB3_PART_NEARFADE_OFF`/`_MATCOLOR_OFF`/`_HAZE_OFF`/`_HAZE_SCALE`, `RB3_SCREENMASK_FALLBACK_OFF`/`_DBG`, `RB3_RTT_OFF`, `RB3_DRAWRECT_DBG` | F/W | mixed | Effect toggles; some (screenmask fallback) are workarounds. |
| `MILO_SCREENSHOT_DIR`/`_FRAMES`/`_NAMES` | F | off | Screenshot harness. |

**Reading:** roughly **half** the flags are one-shot debug probes that should not ship in the
render hot path (they cost a `getenv` per draw at worst — `getenv("CAM_DBG")` etc. are called
*inside* `DrawMesh`, though most latch to a `static`). The **~20 `W`-class flags are the alarming
set**: each is a permanent runtime toggle guarding a workaround for an unfixed bug (lighting,
placement, skinning shards). A healthy engine has feature toggles, not bug-workaround toggles;
the density of `W`-flags is a direct measure of accumulated structural debt.

---

## 5. Decomposition proposal

**Target: break the 7,017-line monolith into a layered set of files mirroring the DC3
decomposition, then converge the two backends onto shared modules where the data shapes allow.**
The RB3 and DC3 rndobj eras differ (CMake note 297–303), so a *full* merge is not the near-term
goal — a *shared-core + thin backend adapters* structure is.

### 5a. Target module structure (files, ownership, interfaces)

```
src/gfx/                         (shared, backend-agnostic — mostly already exists)
  PipelineManager   pipeline cache keyed by PipelineKey; owns standard_wgsl.inc   [exists]
  VertexFormats     static/skinned layouts + CPU unpack/DXT helpers              [exists, extend]
  BloomPass / PostProcPass / DrawRect2D / ShadowPass / DofPass                    [exists]
  UniformRing       ONE ring impl (retire BandUniformRing vs UniformRingBuffer)   [NEW, dedupe]
  Shaders/*.wgsl    move the 5 RB3 inline modules + inc to real .wgsl files       [NEW]

src/render/                      (NEW — shared render-core, backend-neutral)
  RenderFrame       BeginFrame/EndFrame/pass lifecycle, depth/intermediate mgmt
  SceneUniforms     WriteSceneUniforms; owns an IMMUTABLE-per-write scene bind
                    group keyed by (cam-pose, environ) — no mutable member reuse
  MaterialBinder    RndMat -> MaterialUniforms + bind group (from MaterialSetup)
  MeshUploadCache   per-mesh VB/IB fingerprint cache (from MeshGpuCache)
  DrawList          per-draw record; optional transparent sort (adopt TransparentQueue)
  Postproc/Halo     capture-replay + composite, driven by data not names

src/platform/
  Rnd_Wgpu_RB3.cpp  THIN BandRnd adapter: wires the src/render core to RB3 rndobj
  Rnd_Wgpu.cpp      THIN WgpuRnd adapter: wires the same core to DC3 rndobj
  GameRenderHook    the seam for game-specific passes (already defined; WIRE IT for RB3)

rb3/native/src/
  rb3_render_hook.cpp   MOVE the ~30 asset-name behaviors here, behind GameRenderHook
                        (the file already exists and documents this exact intent)
```

Key interface: a `DrawContext` value object carrying the *per-draw* state (world xfm, material
params, bone palette handle, pipeline key) so that state is **passed, not left in members**. This
kills the §2f leak class by construction.

### 5b. Safe migration order (mechanical first; behavior never mixed with moves)

**Phase A — mechanical extraction (zero behavior change, byte-identical output; each step its
own commit, verified by the existing visual-diff gate):**
1. Move the 5 inline WGSL strings to `src/gfx/Shaders/*.wgsl` loaded like `standard_wgsl.inc`.
   No logic change — just externalize strings and add offline `naga` validation in CI.
2. Extract the per-mesh upload cache (`RB3MeshEntry` + unpack) into a `MeshUploadCache` TU.
3. Extract material→uniform translation into a `MaterialBinder` TU.
4. Extract postproc/halo/quad into TUs; where the RB3 port and `gfx/PostProcPass`/`DrawRect2D`
   are provably identical, converge onto the shared file and delete the RB3 copy.
5. Dedupe the two uniform-ring classes into one.
   → After Phase A, `Rnd_Wgpu_RB3.cpp` should be a few hundred lines of `BandRnd` wiring +
   `DrawMesh` reduced to a thin dispatcher.

**Phase B — seam introduction (still no rendering behavior change):**
6. Wire `GameRenderHook` for the RB3 backend (it is currently registered but never called —
   `rb3_render_hook.cpp:14–20`). Move the ~30 asset-name branches out of engine `DrawMesh` and
   behind the hook in `rb3/native/src`, one behavior per commit, each gated by its existing
   `RB3_*` flag so A/B stays possible. The engine stops knowing RB3 asset names; behavior is
   identical, just relocated.
7. Replace the mutable `mSceneBindGroup` member with an immutable-per-write scene bind group
   returned from `SceneUniforms` and bound explicitly per draw via `DrawContext`. Pure
   refactor — same bytes, but the leak vector is gone.

**Phase C — behavioral correctness (now safe to attempt, because the pipeline is legible):**
8. Introduce a real transparent/depth-sorted pass (adopt `TransparentQueue` for RB3) — retire
   the ad-hoc text `zMode=Disable` special-case.
9. Fix the *general* skinning/transform/lighting pipelines (lanes 02/03/04) and delete the
   corresponding `W`-class workaround flags and name-branches as each is subsumed. This is where
   the faithful-BandPatchMesh rewrites finally have a stable renderer to land against.

**Rule (from the review protocol and hard-won here):** a commit either *moves* code or *changes*
behavior, never both. Every Phase-A/B step must pass the visual-diff gate byte-for-byte before
the next. This is precisely how the memory notes say the C8/patch-shard regressions slipped in
(a "faithful" change that also moved behavior); the staged order prevents recurrence.

---

## Verdict

**OVERHAUL** — but a *staged, mostly-mechanical* one, not a rewrite. The subsystem is not
salvageable as-is: a 2,670-line `DrawMesh`, a mutable scene bind group re-created mid-frame,
~90 env flags of which ~20 guard unfixed-bug workarounds, ~30 hardcoded asset-name branches,
5 inline shader modules duplicating existing `gfx/` passes, and no transparent-sort path. But
the *pieces to converge onto already exist* in the DC3 backend and `gfx/` (`PipelineManager`,
`MeshGpuCache`, `MaterialSetup`, `PostProcPass`, `DrawRect2D`, `TransparentQueue`,
`GameRenderHook`). The overhaul is therefore extraction + convergence + seam-wiring, which can
be done incrementally behind the existing visual gate without a big-bang rewrite. The bug family
(placement, deform, lighting) will not be durably fixable until Phases A/B make the pipeline
legible and move game-specific behavior out of the engine.
