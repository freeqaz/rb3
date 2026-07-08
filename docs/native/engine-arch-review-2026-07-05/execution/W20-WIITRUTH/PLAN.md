# Lane W — Wii load-truth (static, Dolphin patched-disc) — PLAN

**Wave 20, Lane W (KEY=W20-WIITRUTH). AUDIT-ONLY.** Charter: kickoff A5/A6/A7 +
review Q2. Produce the **Wii binding table**: for each band member's hand meshes,
which skeleton ObjectDir instance do the mesh's bone refs resolve to — OWN_MEMBER or
SHARED_ROOT — at **main_hub AND live gameplay**. STATIC pointer reads only; NO
animation claims (A11). The band CharBone skeleton is known-static on this substrate
(R5 V_findings) — that is the **bind pose = Wii basis B** (A7), irrelevant to binding
topology.

## The binding chain (from source, structurally Bank-8-faithful)

Hand-mesh → owning skeleton dir, pointer-deref only (A5, never name-NN):

1. **RndMesh** (`__vt__7RndMesh` @ 0x80c235a8) — the outfit skin mesh (`hands_naked.mesh`,
   `gloves*`, torso control). `RndMesh : RndDrawable, RndTransformable` (both virtual-base
   `Hmx::Object`). `mBones` = `ObjVector<RndBone>` @ src-annotated 0xe4 (empirical).
2. **RndBone** { `ObjPtr<RndTransformable,ObjectDir> mBone` @ 0x0; `Transform mOffset` }.
   `mBone.mPtr` derefs to the skeleton **bone transformable** — a `bone_X.mesh`
   `RndTransformable` living in the skeleton dir (CHAR_SKINNING ~:536-549: parse-time
   `FindObject("bone_R-upperArm.mesh")` descent). NOT the `.cb` CharBone (`CharBone :
   Hmx::Object`, distinct; its `mTrans` @0x40 points AT the `.mesh` trans).
3. **RndTransformable** (bone) → `Hmx::Object` vbase → `mName` (+0xc in Object subobject),
   `mDir` (ObjectDir*, +0x10) → the **owning skeleton ObjectDir**.
4. **ObjectDir** (`__vt__9ObjectDir` @ 0x80bb8ad4, subclasses BandCharacter 0x80be2db0 /
   Character 0x80bfd730 / WorldDir 0x80c1a460) → its `mName`/path → OWN_MEMBER vs
   SHARED_ROOT (`char/main/skeleton.milo` shared magnet vs member's own
   `skeleton_unshared.milo`). Exact-vt ObjectDir census MISSES subclass dirs → census by
   ALL known dir vtables + a structural fallback (mDir target's own name).

Key vbase caveat: RndMesh/RndTransformable have `Hmx::Object` as a VIRTUAL base, so the
Object subobject is NOT at concrete-offset 0. D2 established CharBone (non-virtual base)
name @ +12; for RndMesh/RndTransformable the Object-subobject offset is derived
EMPIRICALLY (G2 validation: name-ptr → plausible `bone_*`/`*.mesh` string; dir-ptr →
heap addr whose vtable is a known ObjectDir vtable).

## Steps

1. **Substrate check** — disc present (`/home/free/tmp/wave17-d2/disc`), own userdir
   `/tmp/dolphin-w20`, own Xvfb `:93`; never touch `/tmp/r1b-user` (concurrent lane).
2. **Offset derivation (calibration, G2)** — on a booted main_hub image: census RndMesh
   vt hits; for a mesh known-named (e.g. `hands_naked.mesh`), sweep candidate offsets for
   (a) the Object-subobject base (name @ base+0xc yields the mesh's own name), (b) `mBones`
   ObjVector begin/end/cap pointers (bone count 38/40 plausible; all `mBone` derefs →
   objects named `bone_*`). Validate: bone names all `bone_*`; count 38/40; deref targets'
   `mDir` → a heap ObjectDir. Document EVERY derived offset + its validation.
3. **RndTransformable Object-vbase offset** — derive where a bone RndTransformable's
   `mName`/`mDir` live (may differ from RndMesh's; both virtual-base but different
   concrete layout). Validate: name reads `bone_X.mesh`; mDir → ObjectDir vtable hit.
4. **main_hub census + per-member walk** — reach main_hub (visgame `run`-style nav or
   dirboot init), then per band member: locate hand meshes (verify RESIDENT — MESH_ABSENT
   loud), walk `mBones`, deref each `RndBone.mBone`, read owning dir. Torso control row.
   Count distinct skeleton-dir instances alive.
5. **Gameplay repeat** — nav to live gameplay (visgame proven), re-census (song load
   re-runs merge chain — the shipped bug is gameplay-state).
6. **A7 basis capture** — per bone slot dump the bound trans's world+local matrices
   (read_mat3x4 exists) + the RndBone.mOffset if reachable. This IS Wii basis B.
7. **A10 schema emit** — one row per (platform=wii, state, member, mesh, boneSlotIndex):
   boneName, status{RESOLVED|UNRESOLVED|MESH_ABSENT}, owningDirName,
   owningDirClass{OWN_MEMBER|SHARED_ROOT|OTHER}, owningDirInstanceId (guest addr, opaque),
   boneCount, memberGender (38-vs-40). JSON + md. Plus distinct-skeleton-dir count per state.
8. **Determinism check** — run twice on the same frozen state → identical tables.

## Fail-reds (mandatory, loud)
- **UNRESOLVED ≠ SHARED**: a failed ObjPtr deref gets status=UNRESOLVED, never a dirClass.
- **MESH_ABSENT loud**: verify hand meshes resident per member BEFORE reading.
- **GENDER-GAP row** if the guest-profile lineup has no female (38-vs-40 census).
- **No name-NN matching** (heap has ~2x instances per name; pointer-deref only).

## Deliverables
PLAN.md (this), STATUS.md (headline: OWN_MEMBER or SHARED_ROOT?), evidence/ (binding
tables JSON+md, offset-derivation log, state screenshots, tool commit SHA in milo-trace),
checkpoint. If boot/nav wall is impassable: exhaustion record with exact blockers, no fake rows.

## Process
Instance-scoped (own -u + own Xvfb), pgid-only teardown. NO default flips/pin bumps/engine
edits. New tool committed in milo-trace (own repo norms). Evidence committed under lane
evidence/. rb3 git under flock; stage only my files; never rb3_session_trace.cpp / engine
FxSendNative.cpp.
