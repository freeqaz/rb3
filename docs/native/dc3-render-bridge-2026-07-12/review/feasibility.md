# Review — Feasibility / Correctness (SKEPTIC lens)

_Adversarial review of PLAN.md + SPEC.md for the DC3 Render Bridge study
(2026-07-12). Posture: refutation. Every claim below is checked against the
actual engine + decomp source, cited file:line. Read-only; no builds run._

## Verdict: GO_WITH_CHANGES

The plan **survives the feasibility attack.** Its central and most attackable
claim — that two engine lineages would collide at link time — is correctly
*dissolved*, not hand-waved, and I verified the mechanism that dissolves it. The
vertex-convergence, header-opacity, and rndobj-shape stories all check out
against source. **No blocking findings.** But the SPEC carries four factual
imprecisions about its own flagship spike's seam surface that an implementing
architect would trip on, and the go/no-go gate (SPIKE-TQ) is a weaker proxy for
the real risk than the plan claims. Fold the corrections below into SPEC §2/§3
and the C2 acceptance before dispatch.

---

## What I tried to break, and what held

### 1. Symbol collision between two engine lineages — DISSOLVED (verified)

The obvious feasibility kill: "you can't link RB3-Wii rndobj and DC3-Xbox rndobj
into one binary — same class names, divergent ABI (R3 finding 5)." The plan's
answer (SPEC §7): the engine is a **source-level library**, each consumer
`add_subdirectory`s it and compiles it against that consumer's own rndobj; one
binary = one fork. I verified the mechanism is real:

- `milo-native-engine/CMakeLists.txt:112-134` defines
  `MILO_ENGINE_GPU_BACKEND ∈ {off,dc3,rb3}`.
- `CMakeLists.txt:371-374`: `dc3` → `MILO_ENGINE_GPU_PLATFORM_SOURCES`;
  `rb3` → `MILO_ENGINE_GPU_PLATFORM_SOURCES_RB3`
  (`= src/platform/Rnd_Wgpu_RB3.cpp …`, :321-322).
- `CMakeLists.txt:392-393`: the `dc3` flavor **additionally** appends
  `MILO_ENGINE_GFX_RNDOBJ_SOURCES` (the 6 coupled TUs); the `rb3` flavor does
  **not** get them.
- Engine HEAD confirmed `0083bad` (PLAN's `0083bad3`).

So the "two engines' symbols coexist" question genuinely does not arise: only
one flavor's TUs and one consumer's rndobj compile into any binary. Option A
(the only scoping that would demand both forks in one binary) is correctly
rejected NO-GO. **This is the strongest part of the plan and it holds.** The
proposed new shared `MILO_ENGINE_GFX_SCENE_SOURCES` list is structurally
identical to the existing `MILO_ENGINE_GFX_SOURCES` (`:261`, added for both
flavors at `:388`), so the plumbing is proven-viable.

### 2. Vertex-conversion story for Wii-platform assets — HOLDS (verified)

SPEC §5 claims the vertex seam is already converged (both flavors emit one
`GpuVertex`). Verified directly:

- `gfx/VertexFormats.h:9-16`: `struct GpuVertex` = 64B (pos/norm/color/uv/tangent),
  `static_assert(sizeof==64)`. `GpuVertexSkinned` = 88B (`:19-29`). Matches SPEC §5.
- `platform/Rnd_Wgpu_RB3.h:43`: `using GpuVertexRB3 = GpuVertex;` — RB3 uses the
  **same** engine struct, not a parallel one.
- `platform/RB3MeshCache.cpp:102-170` (`RB3UnpackMeshVerts`): emits
  `GpuVertex`/`GpuVertexSkinned` from BOTH source encodings RB3-native loads —
  uncompressed `RndMesh::Vert` (`:110-136`, placeholder tangent `(1,0,0,1)` at
  `:123,135`) and Xbox-compressed `XboxCVert` (`:138-166`, real tangents via
  `BeDec4n`). Matches SPEC §5 rules and OQ1/OQ2 verbatim.
- The Xbox-decoder trap is real and correctly walled off: `VertexFormats.h:42-48`
  is labeled "Unpack **Xbox 360** compressed vertices"; SPEC §5 rule 3 forbids
  routing Wii blobs there. Consistent.

**Correctness of the "scoped to scene graphics" framing:** confirmed that scene
meshes *are* the rndobj-coupled path and RB3 already does its own Wii/Xbox decode
CPU-side, so "GPU-intermediate Option A collapses into Option C" (PLAN:98-99) is
accurate — `RB3UnpackMeshVerts` already produces exactly the `GpuVertex` a shared
pass would consume.

### 3. Header opacity — the crux that makes Option C cheap — HOLDS (verified)

Option C's entire effort estimate rests on "the shared pass **headers** are
already rndobj-opaque; coupling is confined to `.cpp` includes." Verified:

- `platform/TransparentQueue.h:3-5`: forward-decls `RndMesh/RndCam/RndEnviron`
  only; entry points `QueueTransparentDraw`/`FlushTransparentDraws`/
  `IsTransparentBlend(int)` at `:8-20`. Zero rndobj includes. Matches SPEC §2.
- `gfx/ShadowPass.h:4-9`: forward-decls only (`RndEnviron/RndCam/RndMesh`).
  Public surface `Init/Render/Terminate` at `:13-16`. Zero rndobj includes.
- Coupling is indeed `.cpp`-confined: `TransparentQueue.cpp:6-9`,
  `ShadowPass.cpp:6-11` carry the `rndobj/*.h` includes.

### 4. rndobj-shape divergence — SceneView is genuinely needed AND sufficient — HOLDS

Spot-checked the exact shapes SceneView (SPEC §3) must bridge:

- **RB3 lacks `BaseMaterial.h`** (`ls rb3/src/system/rndobj/` — has `Mat.h`, no
  `BaseMaterial.h`; DC3 has both). RB3's blend enum lives on `RndMat` with
  matching values (`rb3/.../Mat.h:129-139`: `kBlendAdd=2, kBlendSrcAlpha=3,
  kBlendSrcAlphaAdd=4`). Confirms R3 finding 3.
- **RB3 exposes lights as a member, DC3 as a method**:
  `rb3/.../Env.h:94-95` `ObjPtrList<RndLight> mLightsReal/mLightsApprox`
  (fields), vs DC3 ShadowPass calling `env->LightsApprox()`
  (`ShadowPass.cpp:263`). SceneView's `EnvLightCount/EnvLightAt` (SPEC §3)
  correctly abstracts exactly this member-vs-method split.
- **RB3 RndCam genuinely lacks `GetViewProjectXfms`** (grep empty in
  `rb3/.../Cam.h`); DC3 has it (`dc3-decomp/.../Cam.h:41,76`). SPEC §3's
  "RB3 impl computes it in SceneView_RB3.cpp" is the right call.
- Header-compile is proven for the rb3 flavor: `Rnd_Wgpu_RB3.cpp:6-15` already
  includes `rndobj/{Cam,Mesh,Mat,Env,Lit}.h`, so those headers compile under the
  rb3 native build today. Only `BaseMaterial.h` is the RB3-absent blocker.

**Net:** the seam is not only feasible, it is *smaller* than recon feared. This
strengthens the plan.

### 5. DC3 entry points callable from a foreign scene graph — HOLDS

There is no bespoke "submit scene" API to be foreign to (R2 finding 2): the
contract is virtual `rndobj` method overrides. Verified the strong-def pattern:
`RndMesh::DrawShowing` is strong-defined per flavor — `Rnd_Wgpu_RB3.cpp:6133`
(rb3) and `Mesh_Wgpu.cpp:123` (dc3); DC3 also strong-defines
`DrawMeshImmediate`(`:159`) / `DrawMeshShadow`(`:332`). BandRnd already owns the
frame-graph bracket (R1 finding 1), so "converged passes are subroutines BandRnd
brackets" is architecturally sound.

---

## Corrections to fold in (advisory — none blocks chartering)

### A. SPEC §3's "read-only" invariant is contradicted by its own flagship spike

SPEC §3 states as a hard rule: *"Read-only. SceneView never mutates scene
objects."* But the SPIKE-TQ target's flush **mutates render state through rndobj
virtuals**: `TransparentQueue.cpp:137-145,205-217` call `td.cam->Select()` and
`td.env->Select(nullptr)` to rebind the active camera/environment before each
deferred draw. These are not field reads. They are common to both forks (RB3
`Cam.h`/`Env.h` have `Select`), so they can stay as direct calls or a
non-read-only seam — but the SPEC must either (a) narrow the "read-only" claim to
"SceneView the accessor is read-only; cam/env selection stays a direct rndobj
call the shared TU makes," or (b) add explicit `SceneView::SelectCam/SelectEnv`
mutators. As written, the invariant is false for the first TU the plan touches.

### B. PLAN:160 "IsTransparentBlend(int) ports as-is" is not true at source level

`TransparentQueue.cpp:58-64` — the body references `BaseMaterial::kBlendSrcAlpha`
etc., a scoped enum from the RB3-absent `BaseMaterial.h`. Numeric values match
`RndMat::Blend` (finding 4), but the TU will **not** compile in the rb3 flavor
until those enum references are re-sourced (numeric literals, an `RndMat::`
qualifier under the rb3 flavor, or a SceneView `MeshBlend`-side classification).
`ShadowPass.cpp:106-110` has the identical `IsShadowTransparentBlend` issue.
Trivial to fix, but "ports as-is" understates it and would mislead the C2 agent.

### C. SPEC §2's coupling table under-states ShadowPass; a second draw seam is missing

Two gaps in the §2 "rndobj coupling to remove" column for ShadowPass:
1. `ShadowPass.cpp:5` includes `platform/Rnd_Wgpu.h` — a coupling to the **dc3
   renderer class** (`WgpuRnd`/`NgRnd`), not just rndobj. SceneView abstracts
   rndobj shapes but not renderer-class references; C3 must also strip/neutralize
   this include or the rb3 flavor cannot compile ShadowPass. Add it to the table.
2. ShadowPass draws via `extern void DrawMeshShadow(RndMesh*)`
   (`ShadowPass.cpp:103`), a **second** extern seam beyond `DrawMeshImmediate`.
   SPEC §2/§3/§4 name only `DrawMeshImmediate`. C3's acceptance must include the
   rb3 flavor supplying `DrawMeshShadow` too.

### D. The rb3 flavor supplies NEITHER extern draw seam today

Grep for `void DrawMeshImmediate` / `DrawMeshShadow` in `Rnd_Wgpu_RB3.cpp` and
the `RB3*.cpp` TUs returns empty — DC3 defines both in `Mesh_Wgpu.cpp`, RB3
defines neither. SPEC §3 anticipates the `DrawMeshImmediate` forwarder as new rb3
work (`:128`), which is correct; but the DAG should make explicit that C2 adds a
new rb3 strong-def for `DrawMeshImmediate` and C3 adds one for `DrawMeshShadow`.
Not a hidden blocker — just make it a named acceptance line so it isn't
discovered mid-spike.

### E. SPIKE-TQ is a weak proxy for the risk it gates (gate-validity)

This is the sharpest methodological finding. C2 (SPIKE-TQ) is chosen for
*smallness* — but finding 4 shows TransparentQueue is **nearly shape-neutral
already**: its only RB3-absent include is `BaseMaterial.h` (one enum), and its
cam/env/mesh calls (`Select`, `Name`) exist in both forks. So a green SPIKE-TQ
proves the extern-draw seam + one enum abstraction, and exercises almost **none**
of the harder SceneView accessor surface (`EnvLightCount/EnvLightAt/LightType`,
`CamViewProj` recomputation) that C1/C3/ShadowPass actually depend on — precisely
the member-vs-method (`mLightsApprox`) and missing-method (`GetViewProjectXfms`)
gaps verified in finding 4. The gate can PASS while giving false confidence about
the M-sized seam that carries the real cost.

Recommendation: extend C2 acceptance to exercise a **minimal ShadowPass-shaped
read** through `SceneView_RB3` (enumerate approx lights via
`EnvLightCount/EnvLightAt`, read one `LightType/LightColor`, and compute
`CamViewProj` on the RB3 side) — even if unused by the draw — so the gate
actually de-risks C1/C3 rather than only the extern-draw plumbing. This keeps the
spike S-M but makes its PASS load-bearing.

---

## Loose ends (low-severity, note-only)

- **DC3 bit-preservation at -O0** (go/no-go criterion 3): routing DC3's
  `mesh->Mat()->GetBlend()` through `SceneView::MeshBlend` may not inline at -O0,
  changing frame-time but not output. Output bit-preservation (milo-tests
  371/371) is the right acceptance; the perf delta is covered by the risk table's
  accessor-indirection row. No action.
- **License** (OQ6): confirmed no `LICENSE` in any of the three trees. Same-org
  inference only; flag to owner, low risk. Correctly carried.
- **OQ2 (live Wii asset census)**: still open and worth the 5-minute check the
  SPEC proposes — `RB3UnpackMeshVerts`'s Xbox branch (`:138-166`) strongly
  suggests the Wii-decode risk is latent, but nothing here confirms which ARK the
  native build mounts. Non-blocking for chartering Option C (the spike touches no
  vertex/texture decode).

## Bottom line

The plan's feasibility spine is sound and independently verified: no
symbol-collision problem (one fork per binary, CMake-proven), vertex seam already
converged, pass headers already opaque, SceneView genuinely necessary and
sufficient for the shape gaps. Option A/B NO-GO calls are well-founded. The
corrections above are factual precision fixes to the SPEC and one gate-validity
improvement — address them before dispatching C2, but they do not gate the
charter. **GO_WITH_CHANGES.**
