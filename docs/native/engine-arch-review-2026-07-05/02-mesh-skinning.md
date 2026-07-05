# Lane 02 — Mesh Skinning

Architectural review of the native/web engine skinning path vs. the Wii decomp ground
truth. Why hands/fingers deform wrong, and why "faithful" BandPatchMesh rewrites keep
breaking rendering.

**Verdict: REFACTOR** (targeted overhaul of the *bind* + *BandPatchMesh* + *test harness*;
keep the GPU blend math — it is faithful). The per-vertex skinning arithmetic is correct.
Everything *around* it is the problem: a load-time bind bug papered over by four stacked
render-side hacks, a half-stubbed CPU geometry subsystem (BandPatchMesh) that triggers a
revert cycle, two duplicated unpack paths that disagree on weight decoding, and a 1005-line
test suite that exercises the one layer already proven to work while leaving the actual
divergence surface untested.

---

## Executive summary

1. **The blend math is faithful.** Our GPU skin matrix `skinMat_i = BoneOffsetAt(i) ·
   boneWorld_i`, weighted sum `Σ w_i·skinMat_i·pos`, with the object transform forced to
   identity, is algebraically identical to the Wii CPU reference `RndMesh::SkinVertex`
   (`rb3/src/system/rndobj/Mesh.cpp:1368`). The row-vector-vs-column-vector matrix
   convention is correctly transposed (`TransformUtils.h:54`). `MAX_BONES = 40` matches on
   both sides (`rb3/src/system/rndobj/Mesh.h:17` == engine `UniformStructs.h:98`).

2. **Hands/fingers are wrong because of the BIND, not the blend.** Character outfit/appendage
   skin meshes have their per-bone `RndBone.mBone` pointers resolved at load time onto the
   **shared static** `char/main/skeleton.milo` magnet, not the member's own animated
   `skeleton_unshared.milo` (proven, `CHAR_SKINNING_DEFORM_INVESTIGATION.md` §wave-07/08). The
   shipped workaround (`RebindOutfitBonesToOwnSkeleton`) rebinds only *torso* clothing;
   head/hands/fingers are **deliberately excluded** because rebinding thin geometry shards it
   — the animated bone's rotation *basis* differs from the magnet basis the invBind offset was
   baked against, so a finger vertex at radius R flings by ~R·sin(θ). Hands therefore render
   either frozen-but-coherent (rebake path) or exploded (force-rebind path). Both read as
   "deform wrong."

3. **BandPatchMesh is the smoking gun that the mesh *data layer* is structurally wrong.** It
   is CPU geometry surgery (raw-pointer manipulation of `RndMesh::Vert` arrays via
   exact-offset `MeshVert` structs). It is compiled but its heavy methods are weak-stubbed.
   **Two independent asm-faithful rewrites of its `WorkVerts`/`FindXfm` functions each broke
   character rendering and were reverted** (`82f390b1` reverting `4a49b1a4`; `f0a95910`
   reverting `30c51bad`) — even though both rewrites are *semantically correct for the Wii*
   (they reproduce the shipping binary that renders on Dolphin). A change that is faithful to
   the Wii breaks native ⇒ the native mesh-data layout the algorithm consumes is **not the
   same shape** as the Wii's, and the "working" partial-match code works only by accident. The
   visual gate is **blind** to this (PASS 34/34 on exploded frames).

4. **Skinned `WorldXfm == IDENTITY` is a load-bearing invariant, not a bug** — but it is
   undocumented and untooled, which manufactures phantom bugs. All placement of a skinned mesh
   lives in the bone palette (world-space skin matrices); the mesh node transform is identity.
   Any code that reads `mesh->WorldXfm()` for a skinned char gets the origin. Count-in shards
   are the same mechanism: an empty/frozen driver holds a degenerate pose → thin geo flings →
   the V24 shard guard drops it or it renders as slivers.

5. **The test harness validates the wrong layer.** `test_bone_ground_truth.cpp` covers the
   *skeleton pose pipeline* (CharBones/CharClip → bone LocalXfm/WorldXfm) — exactly the layer
   the investigation proved *works*. It never tests the GPU blend, the invBind composition,
   the outfit→skeleton bind, weight/index unpack, or BandPatchMesh topology. A faithful
   BandPatchMesh rewrite passes 100% of it and still explodes characters.

---

## Q1 — The faithful Wii skinning model, and where our engine diverges

### Faithful model (decomp ground truth)

- **Weights & indices per vertex** — `RndMesh::Vert` (`rb3/src/system/rndobj/Mesh.h:62-87`):
  4 influences. Weights are `Vector4_16_01 boneWeights` — the "hate format": four `u16`
  fixed-point values in [0,1], `weight = raw/65535` via `GetX()`/`FloatAt()`
  (`rb3/src/system/math/Vec.h:179-217`). Indices are `short boneIndices[4]`.
- **Palette cap** — `MAX_BONES = 40` (`rb3/src/system/rndobj/Mesh.h:17`); `RndMesh::MaxBones()`
  returns it (`Mesh.cpp:85`). `Load` clamps `mBones.resize(MaxBones())` (`Mesh.cpp:949-951`).
  Meshes with more influences are split at author time so each mesh's palette ≤ 40.
- **Where skinning runs on Wii** — GX hardware matrix-palette skinning. `rndwii/Mesh.cpp`
  stages bone weights/indices/transforms into GX matrix memory
  (`gBoneWeightCache`/`gBoneIndexCache`/`gBoneTransformCache` at `0xe0000000`,
  `rndwii/Mesh.cpp:30-32,239-244`). The CPU reference (used for collision + our port model) is
  `RndMesh::SkinVertex` (`rb3/src/system/rndobj/Mesh.cpp:1368-1410`):

  ```
  tf60 = 0
  for i in 0..3:
    boneIdx = vert.boneIndices[i]
    if boneIdx < NumBones() and vert.boneWeights[i] != 0 and BoneTransAt(boneIdx):
      Multiply(BoneOffsetAt(boneIdx), BoneTransAt(boneIdx)->WorldXfm(), tf90)   // skinMat_i
      ScaleAddEq(tf60, tf90, vert.boneWeights.FloatAt(i))                        // += w_i·skinMat_i
  ret = vert.pos · tf60                                                          // row-vector
  ```

  Note three faithful properties: (a) **NO weight normalization** — `tf60` is the raw weighted
  sum; (b) out-of-range indices are **skipped**, not clamped (`if (boneIdx < NumBones())`); (c)
  `skinMat = BoneOffset · boneWorld` (invBind on the left, in Milo row-vector convention).

### Our engine model (the LIVE RB3 path)

RB3 compiles **only** `Rnd_Wgpu_RB3.cpp` (+ `RB3TexSharpen.cpp`) for the GPU backend
(`milo-native-engine/CMakeLists.txt:321-324`; the DC3 files `Mesh_Wgpu.cpp` /
`MeshGpuCache.cpp` / `BoneSetup.cpp` / `VertexFormats.cpp` are the **dc3 flavor** and are NOT
in the RB3 build). Live path:

- `RndMesh::DrawShowing` (`Rnd_Wgpu_RB3.cpp:6543`) → `BandRnd::DrawMesh` (`:3837`).
- **Unpack** `RB3UnpackMeshVerts` (`:1999-2016`): weights via `v.boneWeights.GetX()` (correct
  normalized float), indices `(uint8_t)v.boneIndices[k]`, into the 88-byte `GpuVertexSkinned`.
- **Palette fill** (`:4594-4620`): for each bone, `Multiply(BoneOffsetAt(i), boneTrans->
  WorldXfm(), skinMatrix)` → `TransformToMat4` (column-major store for WGSL). Slots ≥ NumBones
  filled with identity (`:5112`). Object transform forced identity for skinned meshes.
- **Blend** in `vs_skinned` (`gfx/standard_wgsl.inc:669-704`, compiled via
  `PipelineManager.cpp:39`): `Σ w_i·(bones[idx_i]·pos)`, then `object.world` (identity) ×
  result. Matrix convention: `TransformToMat4` stores Milo rows as WGSL columns, so WGSL
  `M·v == v·Milo_M` — correct.

### Divergences (MEASURED)

| # | Divergence | File | Impact |
|---|---|---|---|
| D1 | **Shader normalizes weights** (`normalizeBoneWeights`, `standard_wgsl.inc:235-244`) — faithful `SkinVertex` does NOT. | shader | For RB3 near-inert (GetX weights already sum ≈1). But it is a real semantic change: a vertex intentionally weighted to sum ≠ 1 (partial bind) collapses differently. It also **papers over D2**. |
| D2 | **Duplicate unpack path reads RAW u16 weights.** `VertexFormats.cpp:141` does `gv.boneWeights[0] = v.boneWeights.x` — the raw 0–65535 fixed-point, NOT `.GetX()`. Only "works" because the shader then normalizes. | `gfx/VertexFormats.cpp:141-144` | Latent catastrophic bug; **masked** by D1. Not compiled for RB3, but it is the *same class* driving the shared shader-normalize hack, and it is exactly the duplication that will bite when someone routes RB3 through the generic path. |
| D3 | **No `boneIdx < NumBones` skip in the shader.** `bones[boneIndexAt(...)]` reads whatever slot the index names; slots past NumBones are identity-filled (≈ contributing the un-skinned bind vertex) rather than skipped. WGSL out-of-`array<40>` indexing is implementation-clamped (undefined; may differ native vs. browser). | `standard_wgsl.inc:683` | Minor for well-split meshes; a corrupt index reads a wrong palette matrix instead of dropping the influence. |
| D4 | **CPU-unpack + GPU-blend split.** Wii skins on the GPU (GX). We unpack per vertex on the CPU into a side buffer, cache it, and blend on the WebGPU GPU. Functionally equivalent, but it forces the whole cache/`OnSync`/shard-guard machinery that does not exist on Wii. | `Rnd_Wgpu_RB3.cpp:1999+`, `MeshGpuCache.cpp` | Structural surface area, not a correctness bug. |

**Conclusion Q1:** the arithmetic is faithful; the divergences are (a) a normalize step that
masks a duplicate-path bug, and (b) a CPU/GPU split that manufactures caching machinery. None
of these is the hands/fingers cause.

---

## Q2 — Hands/fingers root cause, ranked

The blend is correct, so a wrong hand means a wrong **input** to the blend: either the wrong
bone `WorldXfm` (bind points at the wrong skeleton) or the wrong `BoneOffsetAt` (invBind baked
against the wrong basis). Both are true here.

**H1 (PROVEN, primary) — Outfit/appendage meshes bind the shared STATIC skeleton, not the
per-member animated one.** `CHAR_SKINNING_DEFORM_INVESTIGATION.md` §wave-07/08 measured it:
the animated per-member `bone_R-upperArm` pointer (`0x..ca28700`, moves 100–165u/frame) never
appears in the outfit meshes' bound-bone list; all outfit meshes share one static magnet
(`0x..924ec0`, worldPos `(7.4,-0.8,57.5)`, byte-identical across 1424 draws). The bind is fixed
at resource-parse time via `FindObject` name resolution onto the preloaded shared
`skeleton.milo` (`obj/Dir.cpp:531`), before the per-member `skeleton_unshared.milo` exists as a
name winner. Confirm trail: `XBONE=bone_R-upperArm` (outfit bonePtr == static magnet);
`BAND_ANIM_PROBE` (per-member bone moves).

**H2 (PROVEN, why hands specifically can't be fixed by the H1 workaround) — Rotation-basis
mismatch of the baked invBind offset.** The shipped fix rebinds only *torso* clothing. Full-body
rebind shards head/hands/fingers because `BoneOffsetAt` was baked against the magnet's rotation
basis, and the animated per-member bone has a *different* basis (documented sign-flip:
`bone_R-upperArm` worldRot.x `(0.73,-0.07,-0.68)` magnet vs `(-0.73,0.09,-0.68)` animated,
investigation L149-158). A vertex at radius R with rotation error θ flings by ~R·sin(θ): compact
torso survives, long-thin fingers/hair shard. So finger meshes stay bound to the static magnet
(frozen, and if the magnet's static pose ≠ the character's intended hand pose, visibly wrong) OR
get force-rebound and explode. Confirm trail: engine `IK_SHARD_VERT` (`Rnd_Wgpu_RB3.cpp:5557+`)
attributes the worst-flung vertex to its dominant bone's basis error; `RB3_SKEL_REBIND_FULL=1`
reproduces the shard.

**H3 (PROVEN contributor) — Finger bones are IK/servo-driven on top, and are excluded from every
static correction.** The SKEL_REBAKE pre-pass and the fling-clamp both exclude
face/hair/fingernail/finger meshes (`Rnd_Wgpu_RB3.cpp:4678-4681`) because they are driven every
frame by `CharIKFingers`/`CharFaceServo`/`CharHair` (a static rebake would not stick). So the
finger bones' *WorldXfm* is live/correct, but it is composed with an invBind baked against the
static magnet basis (H2) → the IK-correct finger pose is re-projected through a wrong offset.

**H4 (NOT the cause, ruled out) — Palette >40 truncation.** `MAX_BONES=40` is faithful and
meshes are pre-split ≤40; the shader's missing skip (D3) is a minor edge, not the finger bug.

**The invBind fix (root cause, from the investigation's own FIX PATH #1):** capture each outfit
bone's offset against the per-member **bind** pose — `offset' = meshBindWorld ·
inverse(perMemberBoneBindWorld)` — at skeleton load / first-pose (before any clip animates),
then rebind onto the animated per-member skeleton. This removes the basis mismatch and unlocks
full-body (incl. hands/fingers) rebind. The blocker is capturing that bind frame at the right
seam (the skeleton is already animating by the time the current rebind site runs).

---

## Q3 — BandPatchMesh: the structural probe

### What it does on Wii (per decomp)

`BandPatchMesh` (`rb3/src/system/bandobj/BandPatchMesh.{h,cpp}`, 1511 lines) is **CPU geometry
surgery** that builds subdivided "patch" meshes to project skin decals (tattoos, band-logo
patches, makeup) into a composite skin texture via render-to-texture. `WorkVerts`
(`.h:57-91`) constructs an edge/face/twin topology from a source `RndMesh` (`AddEdge`,
`AddFace`, `TryAddFace`, `ExtendTwin`, `SetVertsAndFaces`); `FindXfm`/`ProjectPatches` compute
the UV projection transform; `Render(RndTex*, RndMat*)` composites into `OutputTex()`. It
operates directly on `RndMesh::Vert` arrays through a raw `MeshVert` struct with **exact byte
offsets** (`unk4`@0x4, `unk10`@0x10, `unk28`@0x28, `unk2c`@0x2c, `unk30`@0x30 — `.h:37-55`) and
a `kMVFaceList` offset constant.

### What our engine does instead

The heavy methods are **weak-stubbed no-ops** (`band3_link_stubs.s:339-340,1094-1098`:
`ConstructQuad`, ctor, copy-ctor, `operator=` → `__hmx_band3_noop_stub`; the
`rndobj_synth_link_stubs.s:126` note says `ProjectPatches` would "write into .text and
SIGSEGV"). But the **class is compiled** — its destructor is reached via
`ObjVector<BandPatchMesh>::resize` from `OutfitConfig::Load` (`rb3/native/CMakeLists.txt:334-339`,
"V20/V33 un-excluded"), and the `WorkVerts`/`FindXfm` functions run when not stubbed. So skin
decals do **not** actually composite on native — the character's patch textures are absent
(this connects to the C8/skin-RTT lane).

### Why two faithful rewrites broke rendering (the invariant)

| Commit | Change | Reverted by | Symptom |
|---|---|---|---|
| `4a49b1a4` | wave-3 BandPatchMesh shared-code rewrites | `82f390b1` | "deformed characters" |
| `30c51bad` | WorkVerts trio + FindXfm faithful re-match (AddEdge 76→99.8%, ExtendTwin 72→94.5%, TryAddFace 89→93%, FindXfm 58→79%) | `f0a95910` | "exploded characters" |

Both rewrites are **semantically correct for the Wii** — `30c51bad`'s own message: "semantics =
the shipping binary that renders correctly on Dolphin … portable C++ … no ASM_BLOCK." A change
that is faithful to the Wii and yet explodes native characters means **the native mesh data the
algorithm consumes is not the same shape as the Wii's.** Evidence: the LP64 layout fix that had
to precede these attempts (`3d00d1dd` "correct BandPatchMesh MeshVert LP64 arena layout
(char-composite heap corruption)") — the `MeshVert` struct's raw-offset/pointer arithmetic
(`mVert` is 4 bytes on Wii, 8 on LP64; `kMVFaceList` offset; `unk2c` linked-list indices) is
inherently 32-bit-ABI-shaped. The partial-match versions happened to use portable typed
accessors (`mv->faceList[i]`, `std::vector`) that the LP64 compiler lays out correctly *and*
that produce tolerable/degenerate-but-quiet geometry; the asm-faithful reconstruction reproduces
the Wii's exact index/pointer/float-scheduling behavior, which under LP64 struct layout + IEEE
determinism yields different (degenerate) topology.

**The invariant the unfaithful code satisfies:** it stays within portable, layout-neutral C++
whose LP64 lowering happens to keep the patch topology non-degenerate; the faithful code
reintroduces the Wii's exact memory/index behavior that the 64-bit port does not honor.

**And the gate cannot catch it.** `30c51bad` claims "band-closeup-capture drums 6/6 + guitar
10/10 PASS, 0 mesh drops, coherent geometry (no spike hands)" — yet it was reverted for exploded
characters. The drop/ratio visual gate is **blind to patch-shard corruption** (MEMORY:
"PASS 34/34 on exploded frames"). This is the single best evidence that the mesh layer has a
structural fault: a semantics-preserving-on-Wii change breaks native and the automated gate
says green.

---

## Q4 — Count-in shards & the `WorldXfm == IDENTITY` trap

**The trap (MEASURED).** For skinned meshes the render path forces the object/world transform to
**identity** (`Mesh_Wgpu.cpp:245-249` "bone matrices already produce world-space positions, so
object transform must be identity to avoid double-transform"; the RB3 path fills object identity
for skinned draws). The bone palette carries the full world-space skin matrices. Therefore:

- A skinned mesh's own `mesh->WorldXfm()` is the **identity** — it holds NO position.
- Any placement/culling/metric that reads `WorldXfm()` for a skinned char reads the **origin**,
  and any "mesh-local" metric `skin · inverse(meshWorld)` reads back the character's
  far-from-origin world position (~300–500u) and **falsely looks like a fling**
  (`CHAR_SKINNING_DEFORM_INVESTIGATION.md` L160-163). The authoritative metric is
  `|skinWorld − boneWorld|` (limb extent, ~50–65u clean).

**What it tells us:** transform handling in the skinning path is **bimodal and undocumented** —
static meshes carry placement in the node transform; skinned meshes carry it in the palette and
zero out the node transform. Code and diagnostics that don't know which mode they're in produce
phantom bugs. This intersects the placement lane: crowd/drum-kit "all at the same point" is
consistent with skinned placement collapsing to the palette while something reads the (identity)
node transform, or the palette itself resolving to a shared instance.

**Count-in thin-geo shards (MEASURED mechanism).** At count-in the character driver is
empty/frozen (holds the loading-vignette pose — MEMORY walk-on note), so bones hold a degenerate
pose. Thin geometry (fingers, hair) then flings (H2 basis error amplifies any pose error at
radius). The **V24 shard guard** (`Rnd_Wgpu_RB3.cpp:5464-5556`) recomputes the exact 4-bone
blended world AABB per skinned mesh every frame and **drops** the draw if the blended extent
exceeds ~2× the bind extent. So during count-in these meshes are either dropped (blank hands) or,
with the guard off, render as the "thin teal/green/yellow slivers." The guard is **pure symptom
suppression** and is now riddled with per-feature exemptions (rebound meshes `:5491`, UI
highlight bars `:5514-5522`) — each exemption is a place the real pose is wrong and the guard
was fighting a false positive. That exemption sprawl is itself a structural smell.

---

## Q5 — Recommendations

### A correct-by-construction skinning layer

1. **One skinning path.** Collapse the DC3 (`VertexFormats.cpp`/`Mesh_Wgpu.cpp`/`BoneSetup.cpp`)
   and RB3 (`Rnd_Wgpu_RB3.cpp` inline) unpack+palette duplication into a single
   `UnpackSkinnedVertex` (always `GetX()`/`FloatAt()`, never raw `.x`) and a single
   `FillBonePalette`. This kills D2 outright.
2. **Make the blend faithful, not compensating.** Either drop `normalizeBoneWeights` (match
   `SkinVertex`, which does not normalize) or keep it only as an explicit, documented,
   correctness-neutral guard for the all-zero case — not as the thing that hides raw-u16
   weights. Add the `idx < NumBones` skip (D3) to match `SkinVertex` and avoid
   implementation-defined WGSL OOB indexing.
3. **Fix the BIND at load, retire the render-side hacks.** Implement the investigation's FIX
   PATH #1: band-scoped un-share / rebind so outfit + appendage skin meshes resolve to the
   per-member animated `skeleton_unshared.milo`, **and** rebake invBind against the per-member
   *bind* pose (`offset' = meshBindWorld · inverse(perMemberBoneBindWorld)`) captured at skeleton
   load/first-pose. This is the single highest-value change: it removes the H2 basis mismatch,
   enabling full-body (hands/fingers) rebind, and makes the four stacked render-side hacks
   (`RebindOutfitBonesToOwnSkeleton` torso-only workaround, SKEL_REBAKE pre-pass, per-frame
   fling-clamp, V24 shard guard) obsolete. Delete them once it lands — they are all masking this
   one bug.
4. **Resolve BandPatchMesh's half-state.** The worst option is the current one (compiled class,
   stubbed methods, partial-match `WorkVerts` that "work by accident" and invite the revert
   cycle). Pick one: **(a)** port it faithfully with an explicitly LP64-correct `MeshVert`
   layout (replace every `kMVFaceList`/`unkNN` raw offset with typed members) plus a topology
   golden test, then *compile it* so skin decals composite; or **(b)** formally accept the stub,
   document that skin decals are absent, and stop attempting faithful re-matches on native.
5. **Codify the skinned-transform invariant.** Document "skinned mesh node transform is identity;
   placement lives in the palette" and provide one `SkinnedWorldAABB(mesh)` accessor so no code
   ever reads `mesh->WorldXfm()` for a skinned mesh. This defuses the trap and the phantom-fling
   class of bugs.

### Test harness — coverage assessment & gaps

`tests/test_bone_ground_truth.cpp` (1005 lines) is well-built but **covers only the skeleton
pose pipeline** — the layer the investigation proved *works*:

- Bone existence/hierarchy/child-count/symmetry/limb-distance/head-above-pelvis (`:115-241`).
- Clip evaluation: `PoseMeshesDoesNotCrash`, `PoseChangesTransforms`, `PoseDeterminism`,
  `ChannelEvaluationIsFiniteAtKeyBeats`, `BoneWorldMatchesLocalComposedWithParent`,
  foot orientation (`:349-975`).

It does **NOT** test any of the actual divergence surface: the GPU blend (`FillBonePalette` +
weighted sum → world vertex position), the `BoneOffset·boneWorld` invBind composition, the
**outfit→skeleton bind** (which skeleton a mesh's bones point to), vertex weight/index unpack
(the D2 raw-u16 bug), BandPatchMesh topology, or >40-bone handling. **A faithful BandPatchMesh
rewrite passes 100% of this suite and still explodes characters — which is exactly what
happened.**

To let faithful rewrites land safely, add:

- **(a) Golden skinned-vertex test** — load a real character + clip, compute the final 4-bone
  blended *world* positions for a fixed vertex set, assert against a captured baseline within
  tolerance. This is the test that would have caught both BandPatchMesh reverts *and* the
  count-in shard. It is the missing counterpart to the pose tests.
- **(b) Bind-integrity test** — assert each outfit skin mesh's `RndBone.mBone` resolves to the
  per-member animated skeleton instance, not the shared magnet (directly encodes H1).
- **(c) BandPatchMesh topology test** — run `WorkVerts` on a known patch input; assert
  vertex/face counts + AABB stay within tolerance across a change. Makes the revert cycle
  impossible to trigger silently.
- **(d) Weight-unpack test** — assert `Vector4_16_01` round-trips through the GPU unpack with no
  raw-u16 leak (locks D2 shut).
- **(e) Skinned-AABB CI gate** — replace/augment the drop-ratio visual gate (blind to shards)
  with the per-mesh blended-extent check wired to a golden, so "exploded characters" fails red.

---

## Verdict

**REFACTOR**, with a *targeted overhaul* of three things: the load-time **bind** (recommendation
3), **BandPatchMesh**'s half-stubbed state (rec 4), and the **test harness** (rec 5a–e). The GPU
blend math itself is sound and should be preserved — the problem is never the arithmetic, it is
(i) the wrong bone the blend is fed, (ii) a CPU geometry subsystem that is 32-bit-ABI-shaped and
un-gate-able, and (iii) a test suite pointed at the one layer that already works. The four
stacked render-side skinning hacks are the tell: they are a symptom-suppression layer standing in
for the one load-time fix that would let them all be deleted.
