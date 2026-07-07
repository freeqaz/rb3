# Lane RESKIN — R1 — STATUS — VERDICT: **FEASIBLE**

Feasibility + design, diagnosis-only. The synthesized-deform fix is **feasible with ZERO
engine TU edits and ZERO extra memory** (per-member meshes are distinct + self-owned, so
verts are mutated in place). All four R1 tasks answered from source + a headless probe run.
Probe committed (default-OFF, Wii byte-identical); no fix applied.

Engine pin `3b5af48`. Probe built + run in isolated worktree `.claude/worktrees/RESKIN`
(the main build-native was blocked by Lane A's uncommitted `SongSelectPanel.cpp` compile
error — NOT my file; left untouched). Probe source synced verbatim into the main tree for
commit; both trees +50 lines identical.

---

## TL;DR of the R2 recipe

At `BandCharacter::SetDeformation()`, right after the existing `unk610` Reskin loop
(`:3111-3115`, skeleton posed at the weighted gender-bind rest), for each per-member hands
mesh do a **synthesized per-vertex weighted multi-bone blend** that moves verts from the
authored (shared-male) bind basis to the member's OWN gender rest:

```
per bone b in mesh->mBones:
    own_b  = Find<RndTransformable>(mesh->BoneTransAt(b)->Name())   // gender-posed at this point
    A_b    = mesh->BoneOffsetAt(b)          // AUTHORED invBind, still intact pre-Poll-rebake
    T_b    = A_b · CharSpaceRest(own_b)      // char-space (placement divided out — see RISK-1)
per vert v (mesh->Verts(), native 16-bit weights):
    W = ( Σ_b weight_b · T_b ) / Σ_b weight_b        // ALL bones — dodges Seam-B's single-bone wall
    v.pos  = v.pos · W ;   rotate+renorm v.norm       // mirror MeshDeform.cpp:366-395
then mesh->Sync(0x1F)  (or mgr->SyncMesh) to bump the vert fingerprint
latch once-per-mesh (std::set<RndMesh*>) so a SyncObjects re-run cannot compound
```

The **default-ON `RebindHeadHandsAtRest`** Poll-time rebake is LEFT ON and COMPOSES with
this: reskin fixes the rest *shape* (verts), the rebake keeps the GPU palette basis-consistent
(`off = meshWorld·inv(rest_own)` ⇒ at own-rest skin = meshWorld ⇒ renders the re-posed verts,
then animates them in the correct basis). Ordering is correct by construction: SetDeformation
runs BEFORE the first Poll rebake, so the reskin consumes the authored `A_b` before it is
overwritten.

---

## Task 1 — the synthesized-deform math, verified from source (not assumption)

**Reskin's blend semantics** (`src/system/rndobj/MeshDeform.cpp:298-399`, rb3 TU):
- `:308-317` builds `xfms[i] = mBones[i].offset · ExportWorldXfm(bone_i)` per bone.
- `:337-357` per vertex reads the deform's OWN weight table (`mVerts`: `[count]{boneIdx,
  weightByte}...`), and **accumulates ALL its (bone,weight) pairs** into one `weighted`
  transform, `:358-362` normalizes by `totalWeight`, `:363-365` optional `mMeshInverse`
  premult, `:366-368` `v.pos = v.pos · weighted`, `:369-395` rotates+renormalizes `v.norm`.

**Why the blend dodges the Seam-B mixed-sign wall (CONFIRMED from code, not claimed):**
Seam B (SKEL/STATUS) applied ONE dominant bone's delta per vertex; the per-bone `own`-vs-
`bound` deltas are mixed-sign up to ~35°, so one bone's delta tears the other's blend
contribution at the knuckles. Reskin's inner loop (`:337-357`) instead applies the
**weighted average of ALL the vertex's bones' transforms** — the exact LBS model the GPU
palette uses (`Rnd_Wgpu_RB3.cpp:3299-3305`) — so a knuckle vert weighted 50/50 across two
bones gets a smooth interpolation of both deltas. No single-bone tearing. Residual =
ordinary LBS blend error (candy-wrapper class) ⇒ the wext gate stays **quantitative** (≤60u),
not binary-zero.

**What the per-bone transform must be, and that it produces the right motion:**
`A_b` = the mesh's authored `RndBone::mOffset` = `meshWorld · inv(boundBindWorld_b)` (the
inverse bind against the shared authored bind). At SetDeformation the member's `own` skeleton
is posed at gender-rest, so pairing `T_b = A_b · ownRestWorld_b = meshWorld · inv(boundBind_b)
· ownRest_b` maps each vert from the bound-bind basis into bone-b-local and out to the
member's own gender rest — a per-vertex weighted **re-pose from bound-bind rest to own-basis
rest at the gender-bind pose**, exactly the target. (`mMeshInverse`/meshWorld cancel per the
`mSkipInverse` path; skinned char meshes have meshWorld ≈ I.)

**KEY R1 FINDING — do NOT route through `RndMeshDeform::Reskin` verbatim.**
`BoneDesc::ExportWorldXfm` (`MeshDeform.cpp:181-189`) returns the live pose ONLY for
`"exo_"`-prefixed helper bones (`IsExoBone`, `:170-180`); for a **regular** skeleton bone
(hands are `bone_R-index01.mesh` etc., NOT exo) the while-loop is skipped and it returns just
the authored `BoneDesc.parent` (identity on a synthesized deform) — NOT `own->WorldXfm()`. So
a synthesized RndMeshDeform pointed at hand bones would blend the wrong transforms. R2 must
compute `own->WorldXfm()` (char-space) **directly** and do the blend in BandCharacter
HX_NATIVE code. This also matches WAVE14 A1's independent recommendation (avoids
`VertArray::AppendWeights`'s 8-bit weight requantization, `MeshDeform.cpp:124-135`, and any
edit to the match-sensitive `MeshDeform.cpp`).

## Task 2 — A4: authored `mOffset` per gender (PRE-Poll probe, `RB3_RESKIN_PROBE`)

Probe fires at the top of `RebindHeadHandsAtRest` on the first call, before the two-pass
rebake overwrites `mOffset` (`rebound=0` rows). Full log:
`evidence/reskin-probe-gameplay.log`. Comparing bones that resolve to the **same shared
`boundPtr`** object across a male and a female member:

| bone (shared boundPtr) | male player0 `offV` | female player1 `offV` | |Δtrans| |
|---|---|---|---|
| `bone_R-middlefinger01` (0x…efdf00) | (27.84, 39.15, -2.17) | (31.14, 28.39, -1.89) | ~11.2u |
| `bone_R-hand` (0x…8c060) | (21.26, 44.46, -2.31) | (28.69, 32.74, -4.59) | ~14.0u |
| `bone_R-middlefinger02` (0x…4e780) | (39.51, 25.08, -2.17) | (31.57, 26.13, -1.89) | ~8.0u |

Rotation rows differ too (e.g. `middlefinger01` male M-row0 `[0.419,-0.908,-0.014]` vs female
`[0.312,-0.939,-0.143]`).

**A4 ANSWER: `female_hands_naked` carries genuinely FEMALE-AUTHORED offsets, not male
copies.** The reskin source for the female is her mesh's OWN `mOffset` array + her own
skeleton — no cross-gender derivation is needed. Corroborating: the gendered meshes are
structurally different assets — male `hands_naked` = **1876 verts / 38 bones**, female =
**1256 verts / 40 bones**, with different bone *ordering* (`middlefinger01` at male b=1 /
female b=4) and female-only bones (`bone_R-ringfinger-base`). ⇒ R2 must build `mBones`/weights
**per mesh from that mesh's own RndBone array**, never a shared template. The S2 "double
mismatch" is confirmed at the data level: female verts + female-authored `mOffset` + shared
male-bind `bound` bone OBJECTS (same pointers) + female-posed `own` draw bones.

## Task 3 — data availability + invalidation

- **CPU verts are resident and mutable at SetDeformation time.** Probe: `mesh->Verts().size()`
  = 1876 (male) / 1256 (female), a live mutable `RndMesh::Vert` array (float pos/norm,
  `Vector4_16_01` 16-bit weights, `short boneIndices[4]`; `Mesh.h:60-86`). No compressed
  round-trip — "V24" is the engine's shard-guard VERSION tag, not a vert format (WAVE14 A3).
- **GPU re-upload is automatic.** The native renderer creates the GPU VB lazily at first draw
  (`Rnd_Wgpu_RB3.cpp:3013`) and invalidates on vert fingerprints + `RndMesh::OnSync`
  `uploaded=false` (`:2694-2711`); the V24 guard re-reads bind-pose verts every frame. A
  load-time reskin at SetDeformation lands BEFORE the first upload, so it is picked up with no
  special action. Defensive parity with Reskin: call `mesh->Sync(0x1F)` (Reskin does
  `cb->SyncMesh(mMesh, 0x1f)`, `:301`) to bump the fingerprint. **ZERO engine TU edits
  expected.**

## Task 4 — cost + risks + mesh-instance sharing

- **Mesh instances are DISTINCT and SELF-OWNED per member (the pivotal de-risk).** Probe:
  hands_naked `meshPtr` = 0x…996600 (player0 male) / 0x…ded180 (player1 female) / 0x…9c0280
  (player2 male) — all distinct, each `geomOwner == meshPtr` (verts are the mesh's own, not
  shared via GeomOwner). **⇒ per-member reskin mutates each member's verts in place with NO
  cross-member corruption and NO mesh cloning.** (The `bound` BONE objects are shared, but the
  verts/meshes are not — that is the whole native-port split.)
- **Cost:** one-time, latched, at load. Per member ≈ 38 transform composes + ~1876 verts ×
  ~4-weight blends ≈ well under 0.1 ms; ×4 members ≈ negligible. Zero extra memory.
- **RISK-1 (space): use CHAR space, not world.** Pairing `A_b` (placement-free authored
  invBind) with `own->WorldXfm()` (carries live placement) would reintroduce the
  R·sin(θ) placement-lever fling that killed `RB3_APPENDAGE_REST_ROT` (W2.8d). R2 must use
  `NativeCharSpaceRestXfm(own_b)` (placement divided out) so the re-pose is a pure local shape
  change. Pre-registered constraint.
- **RISK-2 (latch): SyncObjects re-arms.** A mid-song merge re-runs SetDeformation and resets
  the head-rebind latch; a naive re-run would compound the transform. Latch by
  `std::set<RndMesh*>` (a re-stuffed mesh is a NEW pointer ⇒ correctly re-reskinned; outside
  the closet `CharMeshCacheMgr` is Disabled so verts persist ⇒ pointer latch is safe).
- **RISK-3 (rest-free invariants):** the reskin changes the REST shape (moves verts) → must
  clear the Instrument-B `isoDistort` non-rigid invariant and show no gloves/torso regression
  (the reskin is scoped to hands_naked + fingernails_resource only; nailboots/torso excluded).
- **RISK-4 (fingernails):** `fingernails_resource.mesh` probes `verts=0` (geometry lives
  elsewhere / proxy) — R2 should confirm whether it needs reskinning at all or rides on
  hands_naked; do not blind-reskin a 0-vert mesh.

## Deliverable verdict

**FEASIBLE.** The primitive is correct and code-verified to dodge the Seam-B wall; the
authored source data exists, is gender-distinct, and is intact at the pinned call site; verts
are CPU-resident/mutable/per-member with automatic GPU re-upload and zero engine edits. R2
recipe is above with 4 pre-registered risks. No offset-bake variant, no clamp — STOP-TRIPWIRE
not tripped.

## Gates (R1)
Diagnosis-only; no behavior change to gate. Probe is default-OFF (`RB3_RESKIN_PROBE`,
getenv-cached), flag-OFF inert, Wii byte-identical. Refuted flags left UNSET. Seven defaults
untouched. No pin bump / default flip / classjson change. `FxSendNative.cpp` untouched. Staged
only my own file (`BandCharacter.cpp`) + these docs under flock.

## Evidence
- `evidence/reskin-probe-gameplay.log` — the `RB3_RESKIN_PROBE` dump (male p0/p2 + female p1;
  mesh ptrs, geomOwners, vert/bone counts, per-bone authored `mOffset`). Regenerable:
  `RB3_RESKIN_PROBE=1 RB3_FIXED_CLOCK=1 python3 scripts/native/keyboard-to-gameplay.py --bin
  .claude/worktrees/RESKIN/native/build-native/rb3-native --song-downs 4`.
- Source: `MeshDeform.cpp:170-399`, `Mesh.h:33-86,240-257`, `BandCharacter.cpp:3064-3145`
  (SetDeformation), `:2974-2978` (unk610 population), `:1253+` (RebindHeadHandsAtRest, probe
  site), `:2420-2456` (SyncObjects → SetDeformation → rest-capture), `BandPatchMesh.cpp:1408-
  1420` (runtime RndMeshDeform synth precedent).
- Checkpoint: `/tmp/wave14-checkpoints/R1.json`.
