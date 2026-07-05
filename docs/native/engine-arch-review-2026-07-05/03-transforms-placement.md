# Lane 3 — Transforms & Placement

**Reviewer:** Opus review agent (read-only). Date: 2026-07-05.
**Question:** Why do objects render at the wrong location — crowd characters all at
one point, drum kit at one point, only some band members correctly placed?

---

## Exec summary

There is **one systemic fault** plus **one independent data-flow gap**, not N
unrelated bugs.

**The systemic fault (MEASURED):** the live RB3 draw path forces the per-object
world matrix (`obj.world`) to **identity for every skinned mesh** and derives the
object's on-screen position **entirely from the bone palette**
(`BoneOffsetAt(b) * boneTrans->WorldXfm()`), read from `mesh->GeomOwner()`'s bones.
This is correct *only* when a skinned object's bone `WorldXfm`s already encode its
world placement. It breaks whenever placement is applied by `SetWorldXfm()` on a
parent `RndDir`/group/mesh whose transform is **not** the actual `TransParent` of
the bones — the bones never re-inherit that world, the palette resolves at the
**origin**, and the object collapses to screen-centre / one point. The engine
already contains **two hard-coded per-mesh-name patches** that inject the missing
translation back into `obj.world` (hub highlight bar, scrollbar thumb) — these are
the smoking gun that this is a general fault being papered over case-by-case, plus
a third latched workaround for the crowd (`RebindCrowdCharBonesToOwnSkeleton`).

- Crowd **3D characters** are skinned → identity `obj.world` → placement from a
  **shared `GeomOwner` skeleton** → every instance reads the last-posed bones →
  "all at one point, not animating." (`Rnd_Wgpu_RB3.cpp:4556`, `4849/4876`;
  `world/Crowd.cpp:333-416`, `860-1040`).
- Drum kit / instruments are **bone-attached prop meshes** ("instruments attach to
  specific hand/prop bones" — `BandCharacter.cpp:775`), i.e. skinned → same
  identity-`obj.world` collapse when the prop bones resolve to origin.
- Band members are placed by `Character::Teleport()` via **`SetLocalXfm()` on the
  character root** (`char/Character.cpp:485-490`), which *does* propagate down the
  bone chain correctly — so most members place right. The ones that don't are the
  ones whose waypoint/`Teleport` never fired (a `BandConfiguration`/`BandWardrobe`
  data gap, `bandobj/BandConfiguration.cpp:43-54`), **not** the transform engine.

**The crowd 2D-imposter path is NOT the bug** — it is a *static* billboard mesh
drawn once per instance, and the per-instance object-uniform slot machinery is
correct (each draw gets its own GPU buffer; the old multi-draw collapse regression
a0f98ad is genuinely fixed — `Rnd_Wgpu_RB3.cpp:4433-4471`).

**Verdict: refactor.** The matrix math, per-draw slotting, and static path are
sound. The skinned-placement contract is the single structural defect; it should
be fixed once, centrally, and the three point-hacks retired.

---

## Q1 — Every draw path that submits a world matrix, and where the matrix comes from

### Ground truth (Wii / decomp)

- `RndTransformable::WorldXfm()` lazily composes `mWorldXfm = mLocalXfm *
  mParent->WorldXfm()` up the parent chain, with a dirty-bit cache
  (`rndobj/Trans.cpp:127-145`). `SetWorldXfm(tf)` writes `mWorldXfm` directly,
  clears the dirty bit, and **dirties all children** (`Trans.cpp:110-117`) so they
  recompose against the new parent world.
- `RndMultiMesh::DrawShowing()` places instanced geometry by looping the instance
  list, calling `mMesh->SetWorldXfm(it->mXfm)` then `mMesh->DrawShowing()` per
  instance — the single shared child mesh is re-posed and re-drawn N times
  (`rndobj/MultiMesh.cpp:206-212`). Billboards additionally re-apply the
  `kFastBillboardXYZ` constraint (`ApplyDynamicConstraint`, `Trans.cpp:161-200`).
- `WorldInstance::DrawShowing()` draws its shared group at `WorldXfm()`
  (`world/Instance.cpp:104-108`); `SharedGroup::Draw(tf)` does
  `mGroup->SetWorldXfm(tf); mGroup->Draw()` (`Instance.cpp:43-46`).

### Native engine (RB3 live path)

**Selected build:** `MILO_ENGINE_GPU_BACKEND=rb3` compiles **only**
`Rnd_Wgpu_RB3.cpp` (+ `RB3TexSharpen.cpp`) as the GPU platform backend
(`milo-native-engine/CMakeLists.txt:318-323`). `Mesh_Wgpu.cpp` /
`MeshGpuCache.cpp` / `Rnd_Wgpu.cpp` (the `DrawMeshImmediate` path) are the **DC3
flavor** (`CMakeLists.txt:300-314`) and are **not** used by RB3. All RB3 mesh
draws funnel through:

```
RndMesh::DrawShowing()            Rnd_Wgpu_RB3.cpp:6543  → gBandRnd.DrawMesh(this)
BandRnd::DrawMesh(RndMesh*)       Rnd_Wgpu_RB3.cpp:3837  (the one true draw path)
```

`obj.world` is chosen in a 4-way branch (`Rnd_Wgpu_RB3.cpp:4549-4560`):

| Case | `obj.world` source | line |
|---|---|---|
| static mesh | `MiloXfmToColMajor(mesh->WorldXfm())` (full world) | 4559 |
| **skinned mesh (general)** | **IDENTITY** — placement 100% from bone palette | 4557 |
| skinned + `highlight_main/pattern` (hub bar hack) | identity rotation + `mesh->WorldXfm().v` translation injected | 4553-4555 |
| skinned + `scrollbar.mesh` (thumb hack) | stashed `scrollbar_bg.mesh` world | 4550 |

- `MiloXfmToColMajor` (`:157-163`) is a correct row→column-major repack (no
  transpose bug). WGSL consumes `M*v` column-major; consistent.
- Skinned bone palette: for each bone, `Multiply(owner->BoneOffsetAt(b),
  boneTrans->WorldXfm())` → column-major (`:4876`, stored `:5032`). `owner =
  mesh->GeomOwner()` (`:3849`).
- **Per-instance object buffer:** each draw of the *same* mesh this frame claims a
  distinct `UniformSlot` (`:4433-4471`) with its own `objUB` + bind group
  (`:4571-4586`). This is the correct fix for the multi-draw collapse — verified:
  a slot is never shared between two draws in a frame.

**Net:** every world matrix that reaches the GPU is either (a) the mesh's own
`WorldXfm()` for static meshes, or (b) implicit-via-bones for skinned meshes with
`obj.world = I`. There is **no path** by which a skinned mesh's own `WorldXfm()`
translation reaches the GPU except the two name-scoped hacks.

---

## Q2 — CROWD-AT-ONE-POINT, ranked

The crowd has two render modes (`world/Crowd.cpp`):

1. **2D bowl imposter** — `mmesh->DrawShowing()` (`Crowd.cpp:571`) draws a 4-vert
   **static** billboard once per instance via `RndMultiMesh::DrawShowing`
   (`MultiMesh.cpp:206`), each instance's `it->mXfm.v` set as the world
   translation. Static path → distinct per-draw slot → **places correctly.** The
   `kFastBillboardXYZ` native branch (`MultiMesh.cpp:191-204`) handles camera
   facing. **This path is not the bug.**

2. **3D characters** — `Draw3DChars()` (`Crowd.cpp:323-422`): per instance,
   `curChar->SetWorldXfm(spXfm)` (`:403`) then `drawable.DrawShowing()`. These are
   full **skinned** `Character`s.

### Ranked hypotheses

**H1 (MEASURED, primary): 3D-char crowd collapses because skinned placement reads
a shared `GeomOwner` skeleton, not the per-instance pose.**
`obj.world` is identity for the skinned char meshes; placement comes from
`mesh->GeomOwner()->BoneTransAt(b)->WorldXfm()`. The crowd's body/outfit meshes are
**shared master geometry with one shared `mBones` array** — the engine reads the
*owner's* bones, never the drawn instance's (`Rnd_Wgpu_RB3.cpp:3849`, palette
`:4849/4876`; documented at length in `Crowd.cpp:860-1040`). Whichever world/instance
posed the shared owner last wins → all instances render at that one pose, and
appear static. This is exactly what the in-tree workaround
`RebindCrowdCharBonesToOwnSkeleton` (`Crowd.cpp:906-1040`, called at draw time
`:409`) exists to fix — and it is **latched once per owner mesh**
(`owner->mNativeBonesRebound = true`, `:1026`), so it binds the shared owner to
**the first archetype that draws** and cannot re-diverge per instance. Fragile by
construction.
*Confirm:* run headless with `CROWD_REBIND_PROBE=1` and the per-bone probe
(`RB3_RENDER_DBG` / bone-probe block `:4834`) on an arena/festival venue; compare
`skinPos`/`worldPos` across instances — identical positions confirm the shared-owner
collapse. A/B with `RB3_NO_CROWD_REBIND=1`.

**H2 (HYPOTHESIS): even with correct per-instance bones, the latched rebake freezes
the crowd at the first captured rest pose**, so instances that share the owner after
the latch inherit the wrong world. Same probe; look for `reboundMeshes` climbing to
1 then sticking while positions stay fixed.

**H3 (rule-out): 2D-imposter multi-draw collapse.** Would show as all *billboards*
at the last instance's spot. Rule out via `SLOT_PROBE=1` (`:4465`): if `scrollbar`-
style per-frame `slotIdx` climbs for the billboard mesh, slotting is working and
this is not the cause. Given the code at `:4433-4471`, this is **already fixed**;
listed only for completeness.

**H4 (rule-out): `WorldXfm()` dirty-cache staleness.** `Character::SetWorldXfm` on a
child of a still-dirty parent could be overwritten on next `WorldXfm()` recompute.
The engine has a defensive per-bone `DirtyLocalXfm()+WorldXfm_Force()` re-walk
(`:4810-4821`) suggesting this has bitten before. Confirm by dumping the char root
`WorldXfm().v` right after `SetWorldXfm` vs at bone-palette time.

---

## Q3 — Drum kit & band members

### Band members — waypoint/`Teleport`, mostly working

- `BandConfiguration::SyncPlayMode()` sets each of 4 waypoints' local xfm from the
  play-mode config and calls `bchar->Teleport(way)`
  (`bandobj/BandConfiguration.cpp:43-54`).
- `Character::Teleport(way)` sets **`SetLocalXfm(way->WorldXfm())`** on the
  character root (`char/Character.cpp:485-490`). Because this goes through the
  *normal* `LocalXfm → WorldXfm` chain, the skeleton bones (descendants of the
  root) recompose with the placement, and the skinned palette lands correctly.
  **This is why placed band members render right.**
- **"Only some correct" root cause (HYPOTHESIS, game-side):** members whose
  `TheBandWardrobe->FindTarget(targName, mVenueNames)` returns null, or whose
  `Teleport` never fires (missing `TargTransform.targName`, wrong `ConfigIndex()`
  play-mode, or `BandConfiguration` not loaded), keep their default `LocalXfm` and
  overlap at the origin/default. This is a `BandConfiguration`/`BandWardrobe`
  data-flow gap, **not** the transform engine. Confirm by `/api/dta/eval` dumping
  each `BandCharacter` `WorldXfm` and its `unk594` (current waypoint) —
  null-waypoint members are the mislocated ones.

### Drum kit — skinned prop, same systemic collapse as crowd

- Instruments/props "attach to specific hand/prop bones, not the gender skeleton"
  (`bandobj/BandCharacter.cpp:775-776`; prop bone names `bone_prop0..3`,
  `bone_mic_stand_bottom` at `:1659-1662`). The kit is therefore a **skinned/bone-
  bound** mesh whose `obj.world` is forced identity and whose position comes from
  its prop-bone palette. If those prop bones resolve to the origin (e.g. the kit
  mesh is placed via a Dir `SetWorldXfm` that doesn't reach the bones, or a shared
  prop skeleton like the crowd's shared owner), the whole kit collapses to one
  point — the exact "drum kit at same point" symptom.
  *Confirm:* `CAM_DBG=1` (`:3858`) filtered to the kit mesh names, or the
  bone-probe, to see whether the kit's bone `worldPos` sits at ~origin while the
  drummer is elsewhere.

### `WorldInstance::SyncDir` transposed-ObjPair check

Prior art (memory: venue-env was a transposed `ObjPair` in `WorldInstance::SyncDir`).
Re-reviewed `world/Instance.cpp:304-394`: the surviving `ObjPair`s are built
`ObjPair(from=mDir/it, to=this/foundObj)` (`:323`, `:353`) and consumed as
`(*it)->Replace(p->from, p->to)` (`:367`) — **from→to ordering is consistent** in
the current tree; no transposition remains here. `SharedGroup::Draw` correctly
re-poses at `WorldXfm()` (`:43-46`). WorldInstance placement itself looks sound;
its risk is only that its shared group is drawn at the instance `WorldXfm()` — a
*static* group path, which is fine.

---

## Q4 — Systemic fault vs N independent bugs

**One systemic fault, with several manifestations, plus one independent data gap.**

**Systemic:** *Skinned meshes ignore the `RndTransformable` parent chain for
placement.* `obj.world` is hard-set to identity for skinned meshes
(`Rnd_Wgpu_RB3.cpp:4556-4557`); the only placement signal is the bone palette read
from `mesh->GeomOwner()`'s bones. When an object is placed by `SetWorldXfm()` on a
parent `Dir`/group/mesh that is **not** the bones' `TransParent`, the placement is
dropped and the object renders at the origin. The engine already acknowledges this
in-code three times:

- Hub highlight bar — "SetWorldXfm lands on the mesh/labelDir but NOT on the bones'
  parent chain … places the bar at the ORIGIN" (`:4486-4492`), patched by injecting
  translation (`:4553-4555`).
- Scrollbar thumb — "the thumb's bones do NOT re-inherit that world … places the
  thumb at the ORIGIN → screen centre" (`:4516-4521`), patched by stashing the
  sibling bg-track world (`:4550`).
- Crowd — shared-owner bone collapse, patched by the latched draw-time rebind
  (`Crowd.cpp:906-1040`).

Each is the **same bug** at a different call site, fixed with a different one-off
name-scoped hack. The drum kit is a fourth, un-patched instance.

**Independent (game-side data):** the "only some band members" symptom is a
`BandConfiguration`/`BandWardrobe` waypoint-assignment gap, unrelated to the engine
transform path (band uses `SetLocalXfm`, which propagates correctly).

Contributing structural amplifier: **shared `GeomOwner` / shared skeletons** across
co-resident worlds (crowd) and across props (kit) mean "placement via a parent" and
"place per-instance" fundamentally cannot both work through a single owner's one
`mBones` array without the per-draw rebind hack.

---

## Q5 — Recommendations + headless test recipe

### Recommendations (priority order)

1. **Fix the skinned-placement contract centrally (retire the 3 hacks).** Decide one
   invariant and enforce it in `BandRnd::DrawMesh`:
   - *Option A (preferred, matches Wii semantics):* keep bones origin-relative and
     always compose `obj.world = mesh->WorldXfm()` for skinned meshes too, with the
     bone palette using **local/bind-relative** bone matrices (`boneLocalToBind`),
     not world. This makes placement flow through the transform chain exactly like
     the Wii, and both the hub bar and scrollbar thumb work with zero name-scoping.
   - *Option B (least churn):* when a skinned mesh's bone palette resolves near the
     origin but `mesh->WorldXfm().v` is non-origin, inject the mesh translation —
     i.e. generalize the hub-bar hack into a rule keyed on geometry, not mesh name.
   Either removes the `highlight_main/pattern`, `scrollbar.mesh`, and (for props)
   the ad-hoc collapse.
2. **Kill shared-`GeomOwner` cross-world skeleton aliasing** for crowd and props:
   give each drawn instance/archetype its own bone-owner (or make the palette read
   `mesh->BoneTransAt` of the *drawn* mesh when it has its own bones), so the
   latched rebind (`RebindCrowdCharBonesToOwnSkeleton`) becomes unnecessary.
3. **Close the band-waypoint data gap** separately: assert in `SyncPlayMode` that
   every `TargTransform.targName` resolves to a `BandCharacter`, and log the misses.
4. Add a **placement regression gate** (below) so any future skinned-placement
   change is caught before it ships.

### Cheap headless placement test recipe

Uses the existing rb3-native HTTP debug API (pattern:
`scripts/native/song-select-capture.py`, `song-end-test.py`).

```bash
# 1) Boot headless into a gameplay/venue frame
RB3_HTTP=1 rb3-native &            # /api/health, /api/screenshot, /api/dta/eval, /api/input

# 2) Per-draw-path placement assertions via /api/dta/eval (no pixels needed):
#    - band members: dump each BandCharacter WorldXfm().v + its waypoint; assert
#      distinct positions and non-null waypoints.
#    - drum kit / prop bones: dump the prop-bone WorldXfm().v; assert != origin
#      while drummer WorldXfm != origin.
#    - crowd: with CROWD_REBIND_PROBE=1, assert reboundMeshes>0 and instance
#      positions span > (bind extent), i.e. NOT all equal.

# 3) Pixel backstop: /api/screenshot → PNG; compare vs images/retail-screenshots/
#    for the venue. Assert band silhouettes occupy distinct screen regions
#    (centroid spread), and crowd fills the bowl rather than a single blob.

# 4) A/B the systemic fix with the existing opt-outs to prove attribution:
#    RB3_NO_CROWD_REBIND=1, RB3_NO_HUB_BAR_PLACEMENT_FIX=1,
#    RB3_SCROLLBAR_THUMB_FIX_OFF=1  — a correct central fix should make all three
#    opt-outs no-ops.
```

Lock one JSON assertion per draw path (band char positions, prop-bone positions,
crowd position spread, UI-widget centroid) so the gate is per-path and a regression
names the exact path that moved.

---

## Evidence index (file:line)

- Skinned `obj.world = identity`: `Rnd_Wgpu_RB3.cpp:4556-4557`; static:
  `:4559`; hub-bar hack: `:4504-4511, 4551-4555`; scrollbar hack:
  `:4512-4548, 4549-4550`.
- Bone palette from `GeomOwner`: `:3849`, compose `:4876`, store `:5032`.
- Per-instance uniform slot (collapse fix): `:4433-4471`, buffer `:4571-4586`.
- Col-major repack (no transpose bug): `:157-163`.
- Live draw path selection: `RndMesh::DrawShowing` `:6543` → `BandRnd::DrawMesh`
  `:3837`; CMake backend split `CMakeLists.txt:300-323`.
- `RndMultiMesh::DrawShowing` (per-instance SetWorldXfm): `MultiMesh.cpp:206-212`,
  native billboard branch `:176-204`.
- `RndTransformable::WorldXfm/SetWorldXfm/ApplyDynamicConstraint`:
  `Trans.cpp:110-200`.
- Crowd 3D chars: `Crowd.cpp:323-422` (place `:403`); shared-owner rebind
  workaround `:906-1040` (latch `:1026`, call `:409`).
- Band placement: `BandConfiguration.cpp:43-54`; `Character::Teleport`
  `Character.cpp:485-495`; prop/instrument bones `BandCharacter.cpp:775-776,
  1659-1662`.
- WorldInstance SyncDir ObjPair ordering (no transposition now):
  `Instance.cpp:322-370`.
