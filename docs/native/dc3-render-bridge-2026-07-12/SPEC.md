# SceneView Convergence Layer — Technical Spec (ARCHITECT draft, 2026-07-12)

_Companion to PLAN.md (Option C). Defines the layer that lets the shared
engine's DC3-flavor render TUs compile against EITHER consumer's rndobj fork.
Citations are to recon handoffs (`recon/R1..R4`) or direct reads made during
synthesis (marked "direct read"). Nothing here was built or executed._

## 1. Name and one-line definition

**`SceneView`** — a read-only, rndobj-shape-neutral accessor seam in
`milo-native-engine/src/gfx/SceneView.h`, with one per-flavor implementation TU
(`src/platform/SceneView_RB3.cpp`, `src/platform/SceneView_DC3.cpp`) compiled
against the consuming decomp's own rndobj headers. It is **not** a bridge
between two engines: there is only one engine, and only one rndobj fork per
binary (§7). SceneView exists so the six rndobj-coupled gfx TUs and
TransparentQueue stop depending on **divergent** rndobj shapes and therefore
stop being flavor-locked.

**Scope of the rndobj-free rule (revised per feasibility/integration review):**
the grep-enforced "no rndobj includes" invariant applies to `SceneView.h`
ONLY. Pass `.cpp` files MAY keep including rndobj headers for the
**fork-common virtual surface** — `Name()`, `Select()` exist identically in
both forks (`rb3 Cam.h:31`, `dc3 Cam.h:27`) and resolve per-flavor at compile
time. Only the **divergent** accesses must route through SceneView: packed
`RndMesh::Vert`, the `BaseMaterial` base/enum qualifiers RB3 lacks,
`GetViewProjectXfms`, member-vs-method light lists.

Explicitly rejected alternatives (from recon):
- **Object marshaling** (RB3 objects → DC3-shaped objects): impossible in one
  binary — same class names, divergent ABI (Transform 0x30 vs 0x40,
  RndDrawable base, vtables; R3 findings 1, 5, per-class table).
- **rndobj uplift** (give RB3 the DC3 shapes): forks the matched decomp's wire
  format (`RndMesh::Vert` is loaded from disk; R3 finding 1) — PLAN Option B
  NO-GO.

## 2. Boundary — exactly what is intercepted and what is targeted

### RB3-side (calls that already exist; unchanged)

The game side of the boundary is the standard Milo traversal, already in
production (R1 finding 1):

- `App::RunOneFrame` (`rb3/src/App.cpp:533`) → `TheRnd->BeginDrawing()`
  (`App.cpp:632` → `Rnd_Wgpu_RB3.cpp:254`) → `TheUI.Draw()` (`App.cpp:642`) →
  leaf `RndMesh::DrawShowing()` — engine strong def at `Rnd_Wgpu_RB3.cpp:6133`
  → `BandRnd::DrawMesh` (`:2563`) → `SubmitDraw` (`:2552`) →
  `mPass.DrawIndexed` (`:2560`) → `EndDrawing` (`App.cpp:652`) → queue submit
  (`Rnd_Wgpu_RB3.cpp:2120`).
- Per-draw game policy stays behind `GameRenderHook`
  (`rb3/native/src/rb3_render_hook.cpp:39`; hook surface
  `GameRenderHook.h:152-342` — R1 finding 4, R2 finding 4). **SceneView does
  not replace GameRenderHook**: GameRenderHook answers "what does the GAME
  want done with this draw"; SceneView answers "what shape does this rndobj
  FIELD have". New bridge behavior extends GameRenderHook; new shape access
  extends SceneView.

Interception point for converged passes: **inside `BandRnd`**, at the same
sites where it currently calls its `RB3*` twins — e.g. `BandRnd::DrawMesh`
gains a queue-or-draw fork calling `QueueTransparentDraw(...)`
(`TransparentQueue.h:10`, direct read) when `RB3_TRANSPARENT_SORT` is ON, and
BandRnd supplies the `extern void DrawMeshImmediate(RndMesh*)` that
TransparentQueue's flush requires (`TransparentQueue.cpp:17,140,209`, direct
read). `BandRnd` remains the frame-graph owner; converged passes are
subroutines it brackets, exactly as `WgpuRnd` brackets them for DC3
(`Rnd_Wgpu.cpp:834/1065` BeginDrawing/EndDrawing — R2 finding 2).

### DC3-flavor-side (entry points being made shape-neutral)

| Shared TU | Public entry (direct read / R2 finding 2) | rndobj coupling to remove |
|---|---|---|
| `platform/TransparentQueue.cpp` | `QueueTransparentDraw(RndMesh*,float,RndCam*,RndEnviron*)`, `FlushTransparentDraws()`, `IsTransparentBlend(int)` (`TransparentQueue.h:8-18`) | includes `rndobj/{Cam,Env,Mesh,BaseMaterial}.h` (`TransparentQueue.cpp:6-9`); `IsTransparentBlend` body qualifies enums as `BaseMaterial::kBlend*` (`:58-64`) — RB3 has no `BaseMaterial.h` (blend enum on `RndMat`, `rb3 Mat.h:129-142`), so the qualifier must be re-sourced; flush MUTATES via fork-common `cam->Select()`/`env->Select(nullptr)` (`:137-145,205-217`) — stays a direct call, see §3 |
| `gfx/ShadowPass.cpp` | pass render bracket (header fwd-decls `RndEnviron/RndCam/RndMesh` only, `ShadowPass.h:7-9`) | includes `rndobj/{Cam,Env,Lit,Mat,Mesh}.h`, `obj/Dir.h` (`ShadowPass.cpp:6-11`) **plus `platform/Rnd_Wgpu.h` (`ShadowPass.cpp:5`)** — a dc3 renderer-class coupling (WgpuRnd/NgRnd) SceneView does not abstract; C3 must strip/neutralize it. Also draws via a SECOND extern seam `DrawMeshShadow(RndMesh*)` (`ShadowPass.cpp:103`) and has the same `BaseMaterial::kBlend*` re-sourcing need (`IsShadowTransparentBlend`, `:106-110`). (Feasibility review C.) |
| `gfx/PostProcPass.cpp` | postproc bracket (consumes `RndPostProc` — engine CMake comment, CMakeLists.txt:276-280) | RndPostProc member reads |
| `gfx/DofPass.cpp` | DOF bracket (consumes `RndCam`) | RndCam member reads |
| `gfx/DrawRect2D.cpp` | 2D quad/rect (consumes `RndMat`) | RndMat member reads |
| `gfx/VertexFormats.cpp` | `UnpackStaticVertices/UnpackSkinnedVertices(const RndMesh&, GpuVertex*…)` (`VertexFormats.h:39-48`) | reads DC3 `RndMesh::Vert` layout — **stays flavor-side** (§5) |
| `gfx/TextureConvert.cpp` | `ByteSwapDXT/UntileMilo/SwapBGRAtoRGBA` (`TextureConvert.h:36-44` per R3 finding 6) | Xbox-specific texel transforms — **stays flavor-side** (§6) |

Already shared, no work: `GpuDevice`, `PipelineManager`, `BloomPass`,
`Screenshot`, `FrameCapture`, `GpuResourceRegistry`, `UniformRingBuffer`,
`VideoEncoder`, `mikktspace`, `ImGuiBackend`
(`milo-native-engine/CMakeLists.txt:261-273`; R1 finding 2).

## 3. The SceneView interface (initial surface)

Free functions in `namespace SceneView`, taking opaque rndobj pointers
(forward-declared only — the header includes **no** rndobj header; this is a
grep-enforced acceptance criterion, PLAN C1). Initial surface = the union of
what the four convergeable pass TUs read, per R2 finding 3's engine-side
contract inventory (`Rnd_Wgpu.cpp:1263-1593`, `Mesh_Wgpu.cpp`,
`Part_Wgpu.cpp:163`):

```cpp
// gfx/SceneView.h  (sketch — names load-bearing, signatures indicative)
class RndMesh; class RndCam; class RndEnviron; class RndLight; class RndPostProc;

namespace SceneView {
  // Mesh (TransparentQueue, ShadowPass)
  int         MeshBlend(const RndMesh*);        // numerically shared enum (R3 f.3);
        // NOTE source qualifiers differ per fork (DC3 BaseMaterial::kBlend*,
        // RB3 RndMat::kBlend* — Mat.h:129-142); blend CLASSIFICATION
        // (IsTransparentBlend) must be re-sourced fork-neutrally (review)
  bool        MeshShowing(const RndMesh*);
  const char* MeshName(const RndMesh*);
  float       MeshDistSqTo(const RndMesh*, const float eye[3]);

  // Camera (ShadowPass, DofPass, TransparentQueue)
  void  CamWorldPos(const RndCam*, float out[3]);
  void  CamViewProj(const RndCam*, float out[16]);  // RB3 impl composes it;
        // DC3 impl may use GetViewProjectXfms (R2 f.3 — RB3 lacks that method)
  float CamNearPlane(const RndCam*);  float CamFarPlane(const RndCam*);

  // Environment / lights (ShadowPass)
  void  EnvAmbient(const RndEnviron*, float rgba[4]);
  bool  EnvFog(const RndEnviron*, float* start, float* end, float color[4]);
  int   EnvLightCount(const RndEnviron*);
  const RndLight* EnvLightAt(const RndEnviron*, int i);
  int   LightType(const RndLight*);   // kDirectional/kPoint/kFakeSpot
  void  LightColor(const RndLight*, float rgba[4]);
  void  LightWorldPosDir(const RndLight*, float pos[3], float dir[3]);
  float LightRange(const RndLight*);

  // PostProc (PostProcPass) — added at PLAN C4 after the twin audit
}
```

Rules:
- **Read-only accessors; mutation never goes through SceneView.** SceneView
  functions never mutate scene objects. Shared pass TUs DO mutate render
  state — TransparentQueue's flush re-selects camera/environment via
  `cam->Select()` / `env->Select(nullptr)`
  (`TransparentQueue.cpp:137-145,205-217`) — but those are **fork-common
  virtuals** (present in both forks' `Cam.h`/`Env.h`) and stay as direct
  rndobj calls in the pass `.cpp` under §1's revised scope rule. (Revised per
  feasibility review A / integration review A2: the draft's blanket
  "read-only" claim was false for the flagship spike TU.)
- **Grow-on-demand.** A pass TU may only add an accessor when its convergence
  node lands; no speculative surface.
- **No new rndobj methods.** Each impl reads existing fields the flavor's
  renderer already reads today (LP64-proven surface — PLAN risk table). Where
  DC3 has a precomputed helper RB3 lacks (`GetViewProjectXfms`,
  `mViewProjMatrix` — R3 finding 8), the RB3 impl computes it in
  `SceneView_RB3.cpp`, NOT by adding members to the matched fork.
- Draw dispatch stays the existing `extern` seams — there are **two**:
  `DrawMeshImmediate(RndMesh*)` (TransparentQueue flush,
  `TransparentQueue.cpp:17`) and `DrawMeshShadow(RndMesh*)` (ShadowPass,
  `ShadowPass.cpp:103`). DC3 strong-defines both (`Mesh_Wgpu.cpp:159,332`);
  the rb3 flavor currently defines **neither** (grep-verified — feasibility
  review D), so these are named new-work acceptance lines: PLAN C2 adds the
  rb3 `DrawMeshImmediate` (thin forwarder to `BandRnd::DrawMesh`-immediate),
  PLAN C3 adds the rb3 `DrawMeshShadow`.

## 4. Data-flow diagrams

### Today (two parallel stacks in one engine; R1 finding 2)

```
RB3 game (matched fork, Wii-lineage rndobj 2010)          DC3 game (Xbox-lineage rndobj 2012)
  App::RunOneFrame → TheUI.Draw                              game loop → RndDir::Draw
        │ RndMesh::DrawShowing (strong def :6133)                 │ RndMesh::DrawShowing (Mesh_Wgpu.cpp:123)
        ▼                                                         ▼
  BandRnd : Rnd  (Rnd_Wgpu_RB3.cpp)                        WgpuRnd : NgRnd  (Rnd_Wgpu.cpp)
   ├ RB3MeshCache ──► GpuVertex ─┐                          ├ MeshGpuCache ──► GpuVertex ─┐
   ├ RB3MaterialBinder           │  SAME struct             ├ MaterialSetup                │
   ├ RB3HaloPass / RB3PostProc   │  (VertexFormats.h:9)     ├ ShadowPass/DofPass/PostProc  │
   ├ RB3Quad / RB3TexSharpen     │                          ├ DrawRect2D / TransparentQueue│
   ▼                             ▼                          ▼                              ▼
  ──────────────── shared gfx CORE: GpuDevice · PipelineManager · BloomPass · UniformRingBuffer ───────────────
                                        │  Dawn / WebGPU (native + emdawnwebgpu)
                                        ▼
```

### End-state (Option C converged)

```
RB3: BandRnd (frame-graph owner, keeps 31-wave fixes)     DC3: WgpuRnd (frame-graph owner)
        │ queue/bracket calls                                     │ queue/bracket calls
        ▼                                                         ▼
  ┌──────────────── SHARED, shape-neutral pass TUs ────────────────┐
  │ TransparentQueue · ShadowPass · DofPass · PostProcPass ·       │
  │ DrawRect2D · (one MaterialParams binder, PLAN C5)              │
  │        every rndobj read goes through ↓                        │
  │            SceneView.h  (no rndobj includes)                   │
  └───────┬──────────────────────────────────────┬─────────────────┘
   SceneView_RB3.cpp                        SceneView_DC3.cpp
   (compiled vs rb3/src/system/rndobj)      (compiled vs dc3-decomp/src/system/rndobj)
          │                                        │
   flavor-side unpack/decode                flavor-side unpack/decode
   RB3MeshCache (GpuVertex, Wii-GX/Xbox     MeshGpuCache + VertexFormats
   decode stays here)                       (+ Xbox ByteSwapDXT/UntileMilo stays here)
          └──────────────► shared gfx CORE ◄───────┘
```

### Per-draw sequence with TransparentQueue enabled (SPIKE-TQ)

```
RndMesh::DrawShowing ─► BandRnd::DrawMesh(mesh)
    │ flag RB3_TRANSPARENT_SORT && IsTransparentBlend(SceneView::MeshBlend(mesh))?
    ├─ no ──► SubmitDraw ─► DrawIndexed                (today's path, bit-identical)
    └─ yes ─► QueueTransparentDraw(mesh, MeshDistSqTo(mesh, camEye), cam, env)
                       …end of opaque pass (BandRnd bracket)…
              FlushTransparentDraws() ─ sorts back-to-front ─► DrawMeshImmediate(mesh)
                                                                   └► BandRnd immediate submit
```

## 5. Vertex-format strategy

**Already converged — formalize, don't rebuild.** Both flavors emit the
engine's single `GpuVertex` (64B: pos/norm/color/uv/tangent) and
`GpuVertexSkinned` (88B) defined at `gfx/VertexFormats.h:9-29` (direct read).
RB3's unpacker is `RB3UnpackMeshVerts` (`RB3MeshCache.cpp:102-170`, direct
read), which handles BOTH source encodings RB3-native actually loads:

- **Uncompressed Wii-lineage `RndMesh::Vert`** (48B packed: `Vector4_16_01`
  bone weights, `Color32` — R3 finding 1): expands weights/color to float,
  copies uv/pos/norm, and writes a **placeholder tangent (1,0,0,1)**
  (`RB3MeshCache.cpp:123,135`, direct read) — see OPEN QUESTION 1.
- **Xbox-compressed blobs** (`XboxCVert`, `BeFloat/BeDec4n/BeUDec4n` decode
  incl. real tangents, `RB3MeshCache.cpp:141-168`, direct read) — present
  because RB3-native prefers 360 ARK assets (MEMORY
  `feedback_prefer_xbox_assets`).

Rules going forward:
1. Shared pass TUs consume **only** `GpuVertex`/`GpuVertexSkinned` (+ the
   `VertexFormats::StaticLayout/SkinnedLayout` wgpu layouts, of which the rb3
   flavor already owns strong defs — engine CMakeLists.txt:317-322 comment,
   direct read). They never see `RndMesh::Vert`.
2. Unpacking stays flavor-side: `VertexFormats.cpp` (DC3 shapes) is NOT
   seam-ported; `RB3UnpackMeshVerts` is its rb3 twin and stays. This is R3
   implication 2's "GpuVertex seam", already realized.
3. **Wii `mCompressedVerts` blobs must never reach DC3's
   `UnpackCompressedVertices`** — that decoder is Xbox-360-specific
   (`VertexFormats.h:43-49`; R3 finding 2). If Wii-extracted assets are ever
   loaded natively, extend `RB3UnpackMeshVerts` with a Wii-blob branch or
   pre-expand at load. (OPEN QUESTION 2 covers whether this path is live.)
4. Tangent policy: passes requiring real tangents (normal-mapped material via
   C5) must either accept the placeholder (flat TBN) or wire the engine's
   vendored `mikktspace` (`CMakeLists.txt:270`) into the uncompressed path —
   decision deferred to the C5 charter (R3 implication 5 predicts acceptable
   fallback).

## 6. Texture-format strategy

Per R3 findings 4/6 and implication 3, texel decode is platform-specific and
**stays consumer-side**; shared code sees only linear RGBA (or pre-decoded
DXT the GPU accepts, where the BC feature probe allows — `GpuDevice.cpp`
BC probe, R2 finding 2):

- RB3 flavor: existing inline decode in `Rnd_Wgpu_RB3.cpp` (CPU DXT at
  `:382+` — R1 finding 3) + `RB3TexSharpen`. Wii GX-tiled formats
  (CMPR/RGB5A3 etc., `rndwii/Tex.h:33`) decode with RB3's own logic if/when
  Wii-sourced textures are loaded.
- DC3 flavor: `Tex_Wgpu`/`RndTex_Native` + `TextureConvert`
  (`ByteSwapDXT`/`UntileMilo` are **Xbox-only** transforms and are never
  applied to Wii data — R3 finding 6).
- The `RndBitmap` mOrder/DXT *scheme* is shared across forks (byte-identical
  header comment — R3 finding 4), so `TextureConvert`'s format-classification
  helpers MAY converge later; the byte-transform functions do not.

## 7. Compile/link strategy — how the "two engines'" symbols coexist

**They never coexist.** The engine is a **source-level library**: each
consumer `add_subdirectory`'s the same engine tree and compiles it against
that consumer's injected matched-fork include roots (RB3 injects its roots +
MWCC compat flags — R4 finding 2, `rb3/native/CMakeLists.txt:92-100,224`; DC3
likewise at its pin). One binary therefore contains exactly ONE rndobj fork,
ONE flavor's renderer, and ONE `SceneView_*.cpp`. Consequences:

- **No namespaces, no shims-for-symbol-collision, no static-lib tricks are
  needed** — the "how do both engines' symbols coexist" question dissolves:
  `class RndMesh` always resolves to the consumer's own fork at compile time.
  (This is also why PLAN Option A is structurally blocked: it is the only
  option that would require both forks in one binary.)
- Flavor selection stays `MILO_ENGINE_GPU_BACKEND ∈ {off,dc3,rb3}`
  (`milo-native-engine/CMakeLists.txt:96-136` — R2 finding 1). Convergence
  moves TUs from the flavor-specific lists (`MILO_ENGINE_GFX_RNDOBJ_SOURCES`
  :281-288, `MILO_ENGINE_GPU_PLATFORM_SOURCES` :304-313,
  `..._RB3` :317-329 — direct read) into a new shared
  `MILO_ENGINE_GFX_SCENE_SOURCES` list compiled for **both** flavors, plus
  the one per-flavor `SceneView_*.cpp`.
- Weak/strong def pattern is preserved: the engine weak-stubs
  `RndMesh::DrawShowing` etc.; the flavor renderer strong-defines it
  (`Rnd_Wgpu_RB3.cpp:6133` / `Mesh_Wgpu.cpp:123` — R1 f.1, R2 f.2).
  Converged TUs must NOT introduce new strong defs of rndobj methods.
- Repo/pin mechanics: engine commits land first in `../milo-native-engine`,
  then each consumer bumps `MILO_ENGINE_PIN` (soft, warn-only —
  `rb3/native/CMakeLists.txt:74-85`; CLAUDE.md git rules). PLAN C0
  reconciles the current three-SHA skew (RB3 `b36bcfc`, DC3 `77eb428b`, HEAD
  `0083bad3` — R2 implication 5). The stale `dc3-decomp/native/src/{gfx,
  platform}` duplicates must stay off DC3's link line (R1 finding 2 caveat;
  MEMORY `project_dc3_native_engine_masking.md`).
- Web: both consumers mirror the same Emscripten seam
  (`rb3/native/CMakeLists.txt:204-223` "Mirrors DC3's pattern" — R4
  finding 2); converged TUs are ordinary engine sources and inherit it. Any
  new TU must survive the `File_Web`/`GpuDevice_Web` exclusion dance and the
  28 MB wasm budget (R4 finding 2).

## 8. Lifetime & ownership rules

1. **Scene objects are game-owned.** rndobj instances belong to the decomp's
   ObjectDir world; the renderer and all converged passes hold **borrowed
   pointers only**, valid strictly within the `BeginDrawing()..EndDrawing()`
   bracket of the owning flavor renderer (R1 finding 1 / R2 finding 2
   brackets).
2. **Queues must drain within the frame.** TransparentQueue-style deferral
   stores raw `RndMesh*` + captured cam/env pointers
   (`TransparentQueue.h:10`); `FlushTransparentDraws()` MUST run before
   `EndDrawing` returns (BandRnd adds the flush to its pass-end bracket —
   §4 sequence). No render-side pointer survives the frame.
3. **GPU resources are engine-owned**, keyed per mesh in the flavor cache
   (`RB3MeshCache` / `MeshGpuCache` — R1 finding 2) and registered with
   `GpuResourceRegistry` (shared CORE). Cache invalidation on mesh edit
   remains a flavor responsibility; converged passes never create or destroy
   per-mesh GPU state.
4. **Uniforms** ride the shared `UniformRingBuffer` (per-frame transient;
   CORE). The 656-byte SceneUniforms "DC3-zero-blast" layout constraint
   applies wherever a converged pass shares the scene uniform block (R4
   table, wash/glow row).
5. **SceneView holds no state.** It is pure functions over borrowed pointers;
   anything cached (e.g. a computed view-proj) is cached by the caller inside
   the frame bracket.
6. **Flags own behavior deltas.** Every RB3-visible behavior change ships as
   a NativeCompatFlags row, default-OFF until its charter flips it (W30
   census discipline; preserve the DECOMP_FORCEACTIVE `__LINE__` trap —
   MEMORY).

## 9. Open questions (carried to review)

1. **Tangent quality on the uncompressed path** — placeholder (1,0,0,1)
   tangents (`RB3MeshCache.cpp:123,135`) are fine for today's RB3 shading
   (no normal maps on RB3 `RndMat` — R3 finding 3), but C5's material
   convergence exposes normal-map slots. Wire mikktspace, or accept flat TBN
   for RB3 content? (Defer to C5 charter.)
2. **Is any Wii-encoded `.milo` geometry/texture actually loaded by
   RB3-native today?** The port prefers 360 assets (MEMORY) and
   `RB3UnpackMeshVerts` has an Xbox-blob branch (direct read), which
   suggests the Wii-decode risk (R3 findings 2/6) may be mostly latent. A
   5-minute asset census (which ARK the native build mounts) would size §5
   rule 3 / §6 precisely. R1-R3 were silent on this.
3. **PostProcPass twin audit depth** — RB3PostProc embodies the
   chroma-preserve / UI-post-grade fixes (R4); whether `gfx/PostProcPass`
   can absorb them without forking its DC3 behavior is unknown until C4's
   audit. If not, C4 downgrades to "share subfunctions only".
4. **Does the dc3 flavor's `WgpuRnd` bracket tolerate a SceneView-ported
   ShadowPass byte-identically?** Compile-and-tests proof only (no builds run
   this wave); C3's acceptance covers it, but a surprise there re-scopes C3.
5. **Pin-reconciliation ownership** — C0 needs a named owner with commit
   rights on all three repos. LARGELY DE-FANGED post-review: integration
   review measured the delta contents (linear fast-forward, DC3 pin ⊂ RB3
   pin ⊂ HEAD; ~130 commits with exactly 1 touching the shared gfx CORE and
   dc3-flavor touches self-labeled byte-identical MOVEs / default-OFF flags;
   ancestry re-verified by finalizer via `git merge-base --is-ancestor`),
   and PLAN's DAG now makes C0 a LANDING gate only (ROI review B1) — the
   spike does not wait on it. Remaining open: who owns the actual bump.
6. **License** — no LICENSE file in any tree (R4); same-org inference only.
