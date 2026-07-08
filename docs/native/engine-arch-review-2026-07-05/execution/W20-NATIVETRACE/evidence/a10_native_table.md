# Wave-20 Lane N — A10 native binding table (hand meshes)

Join-ready per the A10 schema (row per platform/state/member/mesh/boneSlot). Native side
for the coordinator's W×N join. Full per-slot rows in `*_a10.json` (205 rows each arm).
Aggregated here by (member, mesh) since within a mesh every slot shares the same
owningDir class + instance (verified: 0 intra-mesh variation).

Source: `keyboard-to-gameplay.py --diff hard`, RB3_FIXED_CLOCK=1, dump at
`RebindHeadHandsAtRest` ENTRY (first-touch, pre-mutation — A2). State reached
main_hub→gameplay in a single boot. `owningDirClass` uses the parent==nil heuristic;
**the authoritative join field is `owningDirInstanceId`** (SHARED across members ⇒ shared
root; DISTINCT per member ⇒ per-member instance). memberGender came back "other" from the
probe's gender read (mGender not populated at the collect used here — a probe limitation,
not a data fact; Lane W supplies the 38/40 census).

## CONTROL — shim ON (shipped default)

| member | mesh | boneCount | owningDirInstanceId | owningDirClass | distinctFromOwnFind |
|---|---|---|---|---|---|
| player0 | fingernails_resource.mesh | 10 | inst0 | SHARED_ROOT | True |
| player0 | hands_naked.mesh | 38 | inst0 | SHARED_ROOT | True |
| player1 | fingernails_resource.mesh | 10 | inst0 | SHARED_ROOT | True |
| player1 | hands_naked.mesh | 40 | inst0 | SHARED_ROOT | True |
| player2 | fingernails_resource.mesh | 10 | inst0 | SHARED_ROOT | True |
| player2 | hands_naked.mesh | 38 | inst0 | SHARED_ROOT | True |
| player2 | nailboots_resource.mesh | 6 | inst0 | SHARED_ROOT | True |
| player3 | gloves_resource.mesh | 40 | inst0 | SHARED_ROOT | True |
| player3 | gloves_resource.1.mesh | 4 | inst0 | SHARED_ROOT | True |
| player3 | gloves_skin.2.mesh | 9 | inst0 | SHARED_ROOT | True |

**All 205 hand-bone slots across all 4 members → ONE shared instance `inst0`**
(raw ptr `0x55c62d5a3d00`, anon preloaded skeleton root). `distinctFromOwnFind=True`
everywhere: the bound bone is NOT the instance the member's own `Find(name)` returns — the
per-member animated skeleton exists but is unused by the hand meshes. This is the
2026-06-06 record re-confirmed on today's build (pointer-evidenced).

## NOSHIM — shim OFF (retail kMerge, reconciliation arm)

| member | mesh(es) | owningDirInstanceId | owningDirClass | distinctFromOwnFind |
|---|---|---|---|---|
| player0 | hands/nails | inst0 (0x…c8d60) | SHARED_ROOT* | False |
| player1 | hands/nails | inst1 (0x…ae100) | SHARED_ROOT* | False |
| player2 | hands/nails | inst2 (0x…d6820) | SHARED_ROOT* | False |
| player3 | gloves/nails | inst3 (0x…5a020) | SHARED_ROOT* | False |

*classifier labels these parent==nil (SHARED_ROOT) but the `owningDirInstanceId` proves
they are **DISTINCT per member** (inst0..inst3, one each). `distinctFromOwnFind=False`
everywhere: bound == the per-member `Find` — i.e. each hand mesh binds THE member's own
instance. Topology CHANGED vs control. **But the hands still visually fling (see
`noshim_shimOFF_gameplay.png`): per-member binding ≠ gender-posed bind.**

## Parse-time name-resolution (Probe E, A4) — `RndMesh::Load` `bs >> mBones` (Mesh.cpp:947)
- CONTROL: 405/405 hand-mesh bone names resolve to ONE root `0x55c62d5a3d00` (dirIsRoot=1)
  = the same shared instance the SLOT rows bind. **Binding IS decided at parse-time name
  resolution** (mechanism (b)), exactly as CHAR_SKINNING doc ~:536-556 states.
- NOSHIM: 405/405 still resolve to one parse-time root `0x55a7e7880620`, but the
  subsequent kMerge + `:4202` ReplaceRefs (31,488×) re-points the bones onto the
  per-member merged dir → the distinct per-member instances above.
