# wave-05 — band-character skinning faithful fix (char angles)

Coordinator: audio-perf-loop. Char agents are READ-ONLY this wave (no build,
no rebuild-requiring runs). Findings hand off via this doc + the canonical
`docs/native/CHAR_SKINNING_DEFORM_INVESTIGATION.md`. RB3 HEAD `fd11c360`,
engine HEAD `513dcd5` (dormant WIP uncommitted on top: BoneSetup.cpp +
Rnd_Wgpu_RB3.cpp + ~15 src/system/char + BandCharacter.cpp).

---

### angle-1 — the LOADER / NAME-RESOLUTION share magnet (CHAR-1)

**Mandate:** the loader/name-resolution layer. Break the shared-skeleton magnet so
each band member binds to ITS OWN skeleton at draw, keeping the 8 distinct per-member
skeletons that already exist at install time.

#### Root cause pinned to exact source (refined, confirmed)

The outfit skin meshes' per-bone ObjPtrs resolve to ONE shared
`char/main/skeleton.milo` instance. The magnet is **FilePath-string share keying**:

1. **Magnet created** — `src/system/obj/Dir.cpp:1021` `PreloadSharedSubdirs()` for the
   `char` group (`config/preload_subdirs.dta:24` lists `"char/main/skeleton.milo"`):
   `gPreloaded[gPreloadIdx++].LoadFile(FilePath("char/main/skeleton.milo"), async=false,
   share=true, …)` (Dir.cpp:1033). One DirLoader in `TheLoadMgr.mLoaders`.
2. **Shared by NAME** — `ObjDirPtr::LoadFile` (`src/system/obj/Dir.h:63`), `share==true`:
   `d = DirLoader::Find(p)` (Dir.h:67). `DirLoader::Find`
   (`src/system/obj/DirLoader.cpp:78`) returns the FIRST loader with `mFile == fp`
   (FilePath equality). Share key = on-disk path string → identical for every member.
3. **Two share entry points**, both → `DirLoader::Find`/`FindLast`:
   - `ObjectDir::LoadSubDir` (Dir.cpp:849 → `LoadFile(subdirpath, true, b, …)` :860)
     for non-inlined `share=true` subdirs (each `*_resource.milo` / outfit milo lists
     `char/main/skeleton.milo`).
   - `ObjectDir::PostLoad` shared-inlined fixup (Dir.cpp:402-411):
     `DirLoader::FindLast(fp)` → `iDir.dir = last->GetDir()`.
4. **Bone refs resolve at the resource milo's PARSE time** via `ObjectDir::FindObject`
   (Dir.cpp:531, descends `mSubDirs` 535-540) → finds `bone_R-upperArm.mesh` in the
   shared `skeleton.milo`. `RndBone::mBone` (`rndobj/Mesh.h:45`, `bs >> mBone >>
   mOffset` :50) points into the shared skeleton; `BoneTransAt` (Mesh.h:256) returns it.
5. **Merge does NOT redo it** — `BandCharacter::Filter` `sBoneMergeDir` path
   (`BandCharacter.cpp:1844-1851`) only `ReplaceRefs` for bones in `sBoneMergeDir`
   (the outfit's own pelvis-bone dir, set at OnInstallFilter `BandCharacter.cpp:1981`).
   Shared `skeleton.milo` bones aren't in `sBoneMergeDir` → fall to `kIgnore` (:1852).

Wave-06 runtime ground truth (probes since removed, doc is authoritative): 8 distinct
per-member `main.milo` skeletons load fresh (`kInlineCached`, `shared=0`); install-time
`FindObject("bone_R-upperArm.mesh")` returns distinct per-member bones; yet at DRAW all
four outfits' `BoneTransAt` return the SAME shared bone (male bind). Female trackjacket
(female-baked invBind) → skinPos (19.8,3.8,0.4) FLUNG; males (0,0,0) clean.

#### KEY PRECEDENT (de-risks the recommended fix): the engine already rebinds mesh bones post-load

`Character::SyncShadow` (`src/system/char/Character.cpp:651-652`) iterates a skin mesh's
bones and re-points each via `mesh->SetBone(i, AddShadowBone(mesh->BoneTransAt(i)),
false)` — rebind to a DIFFERENT transform, `calcOffset=false` (keep the authored
offset). `RndMesh::SetBone` (`Mesh.cpp:317`) is the sanctioned API:
`mBones[idx].mBone = bone` (+ optional offset recompute). So a post-load per-member
rebind of the outfit meshes' shared-skeleton bones to the member's OWN skeleton bones is
a pattern the engine itself uses — NOT a hack.

#### RECOMMENDED fix — per-member rebind in BandCharacter::SyncObjects (RB3 DECOMP, HX_NATIVE)

After the per-member skeleton/outfit are merged into the BandCharacter dir (the moment
install-time `FindObject(boneName)` already returns the member's own bones), walk every
outfit/resource skin mesh and rebind each bone whose owner-dir is NOT this character to
the same-named bone resolved in THIS character dir, keeping the authored offset:
```
for each skin mesh m in this character's dirs:
  for b in [0, m->NumBones()):
    RndTransformable* shared = m->BoneTransAt(b);
    if (shared && shared->Dir() != this-subtree) {
      RndTransformable* own = Find<RndTransformable>(shared->Name(), false);  // member's own
      if (own && own != shared) m->SetBone(b, own, /*calcOffset=*/false);
    }
```
Entry point: a new `#ifdef HX_NATIVE` pass at the END of
`BandCharacter::SyncObjects` (`src/system/bandobj/BandCharacter.cpp:593`, after
`Character::SyncObjects()` at :648 — the per-member skeleton is fully hooked into the
char pipeline by then). This is exactly the `SyncShadow` pattern, scoped to the band.

Why this is the FAITHFUL fix and not the documented dead-ends:
- The per-member skeleton is the one the CHAR PIPELINE poses (it's the member's
  `main.milo` skeleton hooked by `Character::SyncObjects` / `CharBones`), so unlike the
  draw-time rebind to a dormant static skeleton (CHAR-2's case 1), `own->WorldXfm()` is
  LIVE-animated. It tracks the idle clip + IK.
- It keeps the OUTFIT's own (female-baked) `mOffset` (`calcOffset=false`), so at the
  female member's own female bind, `off * world == I` → no fling, AND the female
  animation is correct (the member skeleton is posed to the female deform clip via
  `SetDeformation`, `BandCharacter.cpp:1086`).
- No loader un-share (avoids the crowd/extras blast radius — they keep sharing
  `skeleton.milo`, untouched). No `CharServoBone` ref-remap crash (we rebind the MESH's
  ObjPtrs, the proven-safe `SetBone` path, NOT a dir Copy).

Composes with the shipped clamp: after rebind the band arm bones compose to ~0u so the
clamp (`Rnd_Wgpu_RB3.cpp` skin loop, >12u → identity, `RB3_NO_SKIN_CLAMP`) never fires
on them; it stays as a pure backstop for crowd/extras servo flings.

#### Alternative (deeper, loader-level scoped un-share)
Break the share at both entry points scoped to band-member loads: in `LoadFile`
(Dir.h:66-72) force `d=0` (new instance) when a band resource/outfit references
`char/main/skeleton.milo`, and skip the `FindLast` repoint in PostLoad (Dir.cpp:402-411)
under the same scope. Gives each member a real own skeleton from parse — but (a) must be
scoped to the band only (crowd/extras have no per-member main.milo, must keep sharing),
and (b) the fresh instance is still male-bind until that member's deform clip poses it,
so it ALSO needs the member's gender pose to drive it. More moving parts than the rebind;
kept as fallback if the rebind proves insufficient.

#### A/B verification (same harness as CHAR-2)
- `XBONE=bone_R-upperArm RB3_NO_CLIP=1 RB3_NO_IK=1 --shots 70+`: after the SyncObjects
  rebind, trackjacket_resource `bonePtr` must DIFFER from vestdenim/plaidshirt/shred
  (no longer the one shared root) and trackjacket skinPos (19.8,3.8,0.4)→(0,0,0).
- WITH clip+IK (playback): `SHARD_CATCH` shows 0 lines for trackjacket arm/foreTwist
  across a full burst (the per-member skeleton is LIVE-posed female → no re-fling, the
  recompute-trap that broke the runtime-recompute path does NOT apply here).
- Venue: `scripts/native/char-burst-capture.py --shots 40`, grep singer-at-mic frames;
  female upper body coherent, males unchanged.
- Clamp: `SKIN_CLAMP_PROBE=1` → clamp fires 0× on trackjacket (fired before), still
  fires on crowd/extras.

#### Risk
- Edits ONE RB3 DECOMP file (`BandCharacter.cpp:SyncObjects`), HX_NATIVE-gated → Wii
  byte-identical. Uses only the existing `SetBone`/`Find`/`BoneTransAt` engine API.
- Male members: `Find(boneName)` returns the member's own bone == the (formerly shared)
  bone for males too, but `calcOffset=false` keeps their already-correct offset and
  their pose is unchanged → no male regression.
- Crowd/extras/venue chars: NOT BandCharacters through this path's band scope; untouched.
- DC3: DC3 dancers are fixed-gender matched outfits, never the mixed-gender shared path;
  the pass is in RB3 `BandCharacter` only → DC3-inert.
- Shipped clamp unchanged → even if a bone isn't covered, the clamp backstops it; zero
  regression vs current shipped state.
- Coordination: the implementing agent edits `BandCharacter.cpp` (RB3, no concurrent
  char agent owns it now) — NO engine `Rnd_Wgpu_RB3.cpp` change needed, so it does NOT
  collide with the audio agent's build ownership of the engine tree (it's a DECOMP-file
  edit + the single coordinated build).

---

### angle-2 — DRAW-TIME / inverse-bind rebind (CHAR-2)

**Mandate:** evaluate the fix at the renderer/skinning layer, NOT the loader
name-resolution layer (that's CHAR-1's angle and it's the documented blocker).
Specifically: (a) at draw, resolve each outfit mesh's bones via ITS member's
own skeleton; (b) recompute the inverse-bind offset at bind time so
`offRot == transpose(world)` for ALL outfits; (c) how the shipped fling-clamp
composes with the real fix.

#### What the draw loop actually does (exact code)

`BandRnd::DrawMesh` in `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp`:
- `2705`  `RndMesh* owner = mesh->GeomOwner(); if (!owner) owner = mesh;`
- `3037`  `int numBones = owner->NumBones();`
- `3086`  `RndTransformable* bt = owner->BoneTransAt(b);`  ← the bone POINTER
- `3137`  `Multiply(owner->BoneOffsetAt(b), wt, skin);`  ← `skin = invBind * boneWorld`
- `3200-3230` shipped fling-clamp (skin\*inverse(meshWorld) > 12u → identity)
- `3290`  `MiloXfmToColMajor(skin, dst);`  → GPU palette slot `b`

The bone pointer and the inverse-bind offset are BOTH read out of the OUTFIT
MESH's own `mBones` array — `RndMesh::mBones[b] = { ObjPtr<RndTransformable>
mBone; Transform mOffset }` (`src/system/rndobj/Mesh.h:33-46`). `BoneTransAt`
(Mesh.h:256) just returns `mBones[idx].mBone`; `BoneOffsetAt` (Mesh.h:257)
returns `mBones[idx].mOffset`. The renderer NEVER does a name lookup — it
follows the ObjPtr the LOADER already resolved. So at draw the skeleton choice
is already baked into `mBone`, and it points at the shared male-bind
`skeleton.milo` root (BAND_DRAW_PROBE: identical `bonePtr` across all four
outfits, `parent==nil`).

#### Why fix-(a) "rebind to the member's own skeleton at draw" does NOT stand alone

The per-member `main.milo` skeleton instances DO exist and ARE findable by
name (INSTALL_PROBE: `FindObject("bone_R-upperArm.mesh")` returns distinct
bones per member at install). The rebind mechanism is trivially available:
`RndMesh::SetBone(idx, bone, calcOffset)` (`Mesh.cpp:317`) does
`mBones[idx].mBone = bone;` and, if `calcOffset`, recomputes
`mOffset = WorldXfm() * inverse(bone->WorldXfm())`.

BUT the **gender bind is not in the skeleton geometry** — it is applied by a
per-member deform CLIP at pose time:
`BandCharacter::SetDeformation` (`BandCharacter.cpp:1086`) →
`clip = BandCharDesc::GetDeformClip(mGender)` → `clip->StuffBones / ScaleAdd /
PoseMeshes`. `skeleton_unshared.milo` is itself MALE-bind
(`BandCharDesc::mGender` default `"male"`, ctor `BandCharDesc.cpp:355`). So a
per-member COPY of `skeleton_unshared` is *still male-bind until the member's
deform clip poses it female*. Two cases:

1. **Rebind to the dormant per-member `main.milo` skeleton (NOT driven by the
   char pipeline).** Then `bt->WorldXfm()` is a STATIC male-bind pose → the
   trackjacket invBind (female) still doesn't cancel → still flung, AND now
   the bone never animates. = the recompute-at-bind trap (already proven to
   fail during playback, see investigation doc "recompute B'").
2. **Rebind to a per-member skeleton that the char IK/clip pipeline actually
   poses to the FEMALE bind.** This is the correct target — but the female
   bind only materializes once player1's deform clip + IK run. There is no
   such second, female-posed, live skeleton on native today: the char
   pipeline drives the ONE shared root (`CharBoneDir::FindResource` resolves
   bones through the shared `sResources` dir, `CharBoneDir.cpp:36/60`), and
   the shared root is male-bind because 3 of 4 members are male and overwrite
   it.

**Conclusion on fix-(a):** a draw-time rebind is only correct if there is a
LIVE, char-pipeline-posed, FEMALE-bind skeleton to rebind to. Creating that is
exactly CHAR-1's loader problem (per-member skeleton + per-member deform-clip
pose). The draw layer can REBIND cheaply, but it cannot MANUFACTURE the female
pose. So a pure draw-side rebind does not stand alone.

#### Why fix-(b) "recompute the inverse-bind offset at draw" does NOT stand alone

`SetBone(i, bone, true)` makes `skin = invBind*world = I` BY CONSTRUCTION at
the moment you snapshot. Under `RB3_NO_CLIP RB3_NO_IK` it collapses
trackjacket skinPos 19.8→0 (PROVES the mismatch). But during real playback the
only frame the outfit mesh first becomes reachable is already POST-IK
(`mOutfitDir` reachable at first Poll), so the recompute bakes a mid-idle pose;
later IK/twist frames then re-fling the female arm. Same wall as the documented
`RB3_RECOMPUTE_OFFSETS` diagnostic. **Not shippable as-is** — there is no clean
bind frame at draw time.

#### The ONE draw-side variant that IS faithful + low-risk

The correct *draw-side* contribution is NOT to rebind and NOT to recompute the
offset against a posed frame. It is to **harvest the female bind ONCE, at the
true bind moment, from the female member's own asset, and recompute only the
female-flung bones' offsets against that bind** — then leave the live posing to
the existing shared skeleton. Concretely:

The female mismatch is a fixed delta in BONE-LOCAL space: trackjacket's invBind
was baked for orientation `R_female`, the shared runtime bone is at `R_male`.
For a given bone, `skin = invBind_female * world_male`. If we know the constant
per-bone correction `C_b = R_female_bind⁻¹ * R_male_bind` (the bind-to-bind
delta — a CONSTANT, independent of the live animation), then
`invBind_female_corrected = invBind_female * C_b` makes
`skin = invBind_female * C_b * world_live` cancel to identity at bind AND track
the live animation correctly (because `C_b * world_live` is just the female
member's pose expressed on the shared skeleton). This is a ONE-TIME offset
fixup, applied to the FEMALE outfit meshes' `mOffset` only, computed at the
genuine bind moment (load-time, before any clip/IK runs — captured from the
female `GetDeformClip` pose, NOT the live runtime pose).

Where to source `C_b`: the female bind world for each bone is exactly what
`BandCharDesc::GetDeformClip("female")` poses (`SetDeformation`,
`BandCharacter.cpp:1086`); the male bind is the loaded `skeleton_unshared`
LocalXfm. Both are available at LOAD time, before the idle clip animates — so
the harvest happens in the bind frame the recompute-at-runtime path could never
catch. This keeps the SINGLE shared live skeleton (no loader un-share, no crash
risk from CharServoBone refs), touches ONLY player1's outfit meshes' offsets,
and composes additively with the shipped clamp.

#### Composition with the shipped fling-clamp (keep as safety)

The shipped clamp (`Rnd_Wgpu_RB3.cpp:3200-3230`, `RB3_NO_SKIN_CLAMP` opt-out)
rejects any bone whose mesh-local skin translation > 12u, falling it to
identity (vertex stays at authored model-space bind). After the offset-fixup
above, the female arm bones compose to ~0u and the clamp NEVER fires on them
(it fires at >12u; a correctly-bound + animated arm is <8u). So the clamp stays
in as a pure backstop for: (i) the crowd/extras servo-skeleton flings it
already catches, (ii) any residual hair/face bone the fixup doesn't cover. The
fixup REPLACES the clamp's role for the band arms (perfectly posed, not
T-pose-frozen) while the clamp remains a no-op safety for everything else. No
conflict — the fixup runs at load (offset edit), the clamp runs at draw
(palette reject); the fixup just stops the clamp from ever needing to act on
the band.

#### Recommended draw-side fix (single)

**Load-time per-bone offset fixup for the female member's mismatched outfit
meshes, harvesting the bind-to-bind delta from `GetDeformClip(mGender)`.**
Entry point: a new HX_NATIVE post-`SetDeformation` pass in
`BandCharacter::SetDeformation` (`BandCharacter.cpp:1086`), after
`clip->PoseMeshes()` poses the member's own bones to its gender bind, snapshot
each outfit-skin mesh's `BoneTransAt(b)->WorldXfm()` (now in the member's
gender bind) and call `owner->SetBone(b, BoneTransAt(b), true)` to rebake
`mOffset` against THAT bind. Because this fires inside the deform pass (bind
frame, pre-idle-clip, pre-IK), it captures the clean gender bind the runtime
recompute could not. Mechanism detail + A/B + risk in the structured return.

This is the draw/skinning-layer fix that does NOT need CHAR-1's loader
un-share: it accepts the single shared skeleton and corrects the female
offsets to it at the one frame the female bind is valid.

#### A/B verification

- `XBONE=bone_R-upperArm RB3_NO_CLIP=1 RB3_NO_IK=1` per member, `--shots 70+`
  (band meshes only enter DrawMesh on a band-closeup cut). Assert
  trackjacket_resource skinPos (19.8,3.8,0.4)→(0,0,0); the 3 males stay (0,0,0).
- Then WITH clip+IK (real playback): `SHARD_CATCH` must show 0 lines for
  trackjacket arm/foreTwist bones across a full burst (the runtime recompute
  trap re-flung here; the bind-frame fixup must NOT).
- Venue screenshots: `scripts/native/char-burst-capture.py --shots 40`,
  grep the singer-at-mic frames; female upper body coherent, no shards;
  males unchanged.
- Clamp interaction: with `SKIN_CLAMP_PROBE=1`, confirm the clamp now fires 0×
  on trackjacket (it fired before the fixup) and still fires on crowd/extras.

#### Risk

- Touches ONLY player1's (female) outfit-mesh offsets, at load, under
  HX_NATIVE; male members never enter the fixup (their offset already cancels).
- The shipped clamp is unchanged and stays as backstop — zero regression to the
  current shipped behavior even if the fixup mis-fires (clamp catches a fling).
- Wii match: the change is HX_NATIVE-gated inside `SetDeformation`; Wii path
  byte-identical.
- DC3: DC3 uses fixed-gender named dancers with matched outfits → never
  exercises the mixed-gender shared-skeleton path; the fixup is scoped to the
  band deform pass and is DC3-inert.
- Main remaining unknown: whether `SetDeformation`'s pose frame is the SAME
  bind the outfit invBinds were authored against (the deform clip vs the DCC
  bind). If they differ by a member-global root delta it cancels; if per-bone,
  the fixup still produces identity-at-that-frame and the clamp covers any
  residual. Verify with the XBONE A/B before landing.
