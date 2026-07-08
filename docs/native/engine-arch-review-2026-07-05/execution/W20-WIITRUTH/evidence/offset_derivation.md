# Bank-8 offset derivation log — Lane W (W20-WIITRUTH)

All offsets derived EMPIRICALLY on the running Bank-8 patched-disc image (D2 boot;
`/home/free/tmp/wave17-d2/disc`), G2-validated (rigidity/topology/name checks) —
Bank-5 DWARF is known-divergent and was NOT trusted for absolute offsets. Source
headers (`src/system/rndobj/Mesh.h`, `obj/Object.h`, `obj/Dir.h`, `char/CharBone.h`,
`rndobj/Trans.h`) gave FIELD ORDER (structurally Bank-8-faithful); absolute offsets
were verified against live memory. Reproduce: `tools/wii_mesh_binding.py derive`.

## The binding chain (source-derived, pointer-deref only)

`WiiMesh`(skin mesh) `.mBones[ObjVector<RndBone>]` → `RndBone.mBone[ObjPtr<RndTransformable>]`
→ deref `.mPtr` → the bound skeleton bone (`bone_X.mesh` `RndTransformable`) → its
`Hmx::Object::mDir` → the owning skeleton `ObjectDir` → `mPathName`. Classify
`skeleton_unshared.milo` = OWN_MEMBER vs `skeleton.milo` = SHARED_ROOT.

## Concrete class surprise: WiiMesh, not RndMesh

The map's `__vt__7RndMesh` (0x80c235a8) has **0 resident instances**. The Wii build
instantiates the Wii-specific subclass **`WiiMesh`** (`__vt__7WiiMesh` @ 0x80c33ec0,
`rndwii/Mesh.o`) for every skin mesh. The decomp/native port matches bare `RndMesh`,
but the shipped binary uses `WiiMesh` — the census MUST key on WiiMesh vtables.
(This is a D2-class Bank-8 vs source layout divergence, caught by the map anchor.)

## Derived offsets (all validated)

| symbol / field | offset | derivation + validation |
|---|---|---|
| `__vt__7WiiMesh` (primary) | 0x80c33ec0 | map; sits at objsub_base+0x34 in the object |
| WiiMesh Object-subobj vtable | 0x80c33f7c | anchored on the `hands_naked.mesh` name string → object whose `+0xc` = that name; 1278 instances, 865 read a `*.mesh` name at `+0xc` |
| `Hmx::Object::mName` (in Object subobj) | +0x0c | source Object.h `mName@0xc`; reads the mesh's own name |
| `Hmx::Object::mDir` | +0x10 | source `mDir@0x10`; reads the mesh's containing dir (`char/main/outfit.milo`) |
| `WiiMesh.mBones` ObjVector | objsub+0x114 | scan; layout is `[begin ptr][size:u16 \| cap:u16][owner]` — this build packs the ObjVector count as two equal u16s (NOT a std::vector end ptr). size==cap==actual run of ObjPtr-vtable'd elements (e.g. gloves 0x260026 = 38, matches 38-bone run) |
| `RndBone` stride | 0x3c | ObjPtr(0xc) + Transform(0x30 packed); every `+i*0x3c` holds the ObjPtr vtable |
| `RndBone.mBone.mPtr` (bound trans*) | RndBone+0x08 | ObjRef vtable(0)+mOwner(4)+mPtr(8); deref yields a `bone_*.mesh` trans |
| `RndBone.mOffset` (authored inv-bind) | RndBone+0x0c | source; packed Transform after the ObjPtr |
| `__vt__37ObjPtr<16RndTransformable,9ObjectDir>` | 0x80bdfde0 | map; first word of every RndBone element |
| `__vt__16RndTransformable` | 0x80c2a1b4 | map |
| bone trans `mName` | (RndTransformable-vt addr) − 0x28 | dominant hit (534/600 bones name `bone_*` at −0x28); the RndTransformable vtable is stored at (Object-subobj name + 0x28) |
| bone trans `mDir` | (RndTransformable-vt addr) − 0x24 | mName+4 = Object mDir@+0x10; validated 2626 bones → dir with a `*.milo` name |
| `ObjectDir::mPathName` | dir+0x5c | source Dir.h `mPathName@0x5c`; reads `char/main/skeleton_unshared.milo` (MEM1-interned string) |
| bone trans `mLocalXfm` (packed Transform) | boneObjPtr+0x1c | source RndTransformable `mLocalXfm@0x1c`; rigid packed Transform (A7 basis) |
| bone trans `mWorldXfm` (packed Transform) | boneObjPtr+0x4c | mLocalXfm + 48-byte packed Transform; rigid, plausible bone world pos (A7 basis B) |
| packed Transform | Matrix3 3×12-byte rows, then Vec3 | reused from `wii_bone_dirboot.read_mat3x4` (D2-derived) |

## G2 self-check (`tools/wii_mesh_binding.py derive`)

Over 1278 WiiMesh-objsub instances at a main_hub boot:
- mName reads `*.mesh`: 865
- mBones@+0x114 valid vec (begin=ObjPtr array, size:u16==cap==run): 15 (rest are
  non-skinned meshes with empty mBones — correct)
- RndBone[0] derefs a real bone: 13
- bone.mDir names a dir: 13
- VERDICT: offsets VALID (G2 pass)

Samples (mesh → bone[0] → owning dir):
- `drivinggloves_resource.1.mesh` (38 bones) → `bone_R-thumb01.mesh` → `char/main/skeleton_unshared.milo`
- `hands_naked.mesh` → `bone_L-breast.mesh` → `char/main/skeleton_unshared.milo`
- `male_hands_naked.mesh` → `bone_L-foreArm.mesh` → `char/main/skeleton_unshared.milo`

## Fail-red validation (instruments shown red AND green)

- **UNRESOLVED ≠ SHARED**: `mesh.1.mesh` (a generic-named 40-count mesh, NOT hand/torso)
  has 40 ObjPtr elements whose `.mPtr` targets carry no `bone_*` RndTransformable at the
  standard offset → the tool returns **all 40 UNRESOLVED**, never fabricating a dir class.
  (Not in the hand/torso census filter; surfaced only in an exploratory scan.)
- **MESH_ABSENT loud**: meshes present-by-name whose mBones vector is empty/unbound at a
  state (e.g. `male_placement_torso_*`, `gloves_resource.mesh` at some boots) are reported
  MESH_ABSENT, distinct from RESOLVED-to-shared.
- **Determinism**: census run twice on the same frozen state → byte-identical class counts
  and dir-instance sets (`tools/wii_mesh_binding.py determinism` → `identical=True`).
