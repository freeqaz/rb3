# wave-06 — band-character skinning (faithful per-member skeleton)

Per-wave detail. Canonical investigation: `docs/native/CHAR_SKINNING_DEFORM_INVESTIGATION.md`.
Confirmed root cause (do not re-litigate): all 4 band outfit skin meshes bind by NAME to
ONE shared `char/main/skeleton.milo` (the magnet) at the MALE bind; per-member
`main.milo` skeletons load (8-9 distinct, kInlineCached) but the outfit meshes don't read
them; the female (player1, trackjacket) outfit's female-baked invBind × shared male-bind
world ≠ I → ~20u arm/hand/hair fling. Shipped backstop = renderer fling clamp.

## B — DRAW-FLOW analysis + the per-member-CORRECTION alternative (READ-ONLY, DESIGN-B)

Goal: settle on paper whether a CONSTANT per-bone correction can fix the female outfit,
or whether only a true per-member live skeleton (DESIGN-A) can. The deciding axis is the
band draw flow (pose-per-member vs pose-all-then-draw-all) crossed with WHAT skeleton the
outfit meshes are bound to.

### B.1 — The band draw flow: POSE-ALL-then-DRAW-ALL (confirmed from source)

The frame is two GLOBAL passes, NOT an interleaved per-member pose+draw:

- `App::RunOneFrame` (`src/App.cpp`): `TheBandDirector->Poll()` (L506) runs to COMPLETION
  before `TheUI.Draw()` (L551/558). No frame-level pose→draw interleave.
- POSE pass: `BandDirector::Poll` (`BandDirector.cpp:242`) → `mCurWorld->Poll()` (:245) →
  `WorldDir::Poll` (`world/Dir.cpp:124`, gated on `b`/`mFirstPoll`/kProcessPost) →
  `RndDir::Poll` (`rndobj/Dir.cpp:162`) iterates `mPolls` and Polls EVERY band member +
  world graph in one sweep. Each `BandCharacter::Poll` (`BandCharacter.cpp:315`) →
  `Character::Poll` (`char/Character.cpp:217`) → `RndDir::Poll` drives that member's
  CharDriver/CharClip/CharBones/IK/twist (writes bone Local/World Xfms).
- DRAW pass: `WorldDir::DrawShowing` (`world/Dir.cpp:393`) → HX_NATIVE band-draw bridge
  (`Dir.cpp:448-461`) loops `bi=0..3` → `bandChar->DrawShowing()` →
  `Character::DrawShowing`/`DrawLod`/`mOutfitDir->DrawLodOrShadow` → outfit skin meshes →
  `BandRnd::DrawMesh` (`Rnd_Wgpu_RB3.cpp:3086`) reads each bone's CURRENT WorldXfm ×
  baked `BoneOffsetAt`. All 4 drawn AFTER all 4 posed.

=> **POSE-ALL-then-DRAW-ALL.** RndDir keeps SEPARATE `mPolls`/`mDraws` vectors
(`rndobj/Dir.h:83/89`); poll and draw are distinct passes, never interleaved per member.

### B.2 — BIND vs POSE distinction

- `skeleton_unshared.milo` (and the `skeleton.milo` magnet) is MALE-bind. The FEMALE bind
  is NOT in any skeleton — it enters via `BandCharDesc::GetDeformClip(mGender)` at
  `BandCharacter::SetDeformation` (`BandCharacter.cpp:1087`). And `SetDeformation` is
  called ONCE from `SyncObjects` (`BandCharacter.cpp:613`), NOT from Poll — it is a
  load-time body-MORPH (StuffBones/ScaleAdd into a LOCAL `CharBonesMeshes` temp, then
  `CharMeshCacheMgr`/`RndMeshDeform::Reskin`/`DeformHead`/cuffs reskin the MESH vertices),
  not a per-frame skeleton animation.
- At the female member's `SetDeformation`, her temp `CharBonesMeshes` is momentarily at
  the female bind — but that is a transient local, and it poses the MEMBER's own bones,
  not the shared magnet the outfit meshes read.
- At the female member's DRAW, the outfit meshes read the SHARED `skeleton.milo` magnet,
  which (XBONE, this file's §ROOT CAUSE) sits at the STATIC male bind,
  `worldPos=(7.4,-0.8,57.5)`, "never moves at bind AND during playback." No member's Poll
  animates that magnet; the per-member live skeletons (which ARE female-posed for her) are
  the `main.milo` instances the outfit meshes do NOT bind to.

### B.3 — Can a CONSTANT per-bone correction work? NO (honest)

Two cases, both lose:

1. **Bound magnet is STATIC (as XBONE claims).** Then `off_female × world_staticMaleBind`
   is one constant matrix; a constant per-bone delta would null the fling. BUT this is
   exactly `SetBone(calcOffset=true)` / `RB3_RECOMPUTE_OFFSETS`, the ALREADY-REFUTED
   recompute. It yields a FROZEN female (the magnet doesn't animate → the corrected arm
   is posed-but-immobile, no better than the shipped clamp's coherent model-space pose).
   Fails the "animated not frozen" bar. And per the recompute trap (this file L306), if
   the magnet isn't perfectly static in every venue/closeup state, the bake captures a
   mid-idle/post-IK pose and re-flings later frames.
2. **Bound skeleton is ANIMATED + shared (the classic pose-all-draw-all hazard).** Then it
   holds the LAST-posed member's pose at draw; a constant offset can't track a per-frame
   changing world. Reject.

So pose-all-draw-all does NOT, by itself, doom a constant correction here — because the
bound pose is constant (static magnet). What dooms it is that the bound skeleton is one
**NO MEMBER ANIMATES**: a constant correction can only ever reproduce that static
skeleton's pose (now female-shaped but frozen). There is no clean constant-correction
shortcut to a LIVE female.

### B.4 — Recommended path: DESIGN-A (per-member LIVE skeleton)

To get a live-animated female arm, her outfit meshes must bind to a skeleton HER OWN Poll
animates to HER gender bind — her per-member `main.milo` instance (verified distinct +
live, 2026-06-06 §1-2), NOT the dormant magnet. Concrete landing = the wave-05 CHAR-1
per-member rebind, but explicitly targeting the LIVE instance:

- In `BandCharacter::SyncObjects` (`BandCharacter.cpp:593`, after `Character::SyncObjects()`
  :648), HX_NATIVE-gated: for each outfit/resource skin mesh `m`, each bone `b`,
  `own = Find<RndTransformable>(m->BoneTransAt(b)->Name(), false)`; if `own && own != shared`
  → `m->SetBone(b, own, /*calcOffset=*/false)` (keep the authored gender-correct invBind).
  Pattern precedent: `Character::SyncShadow` (`char/Character.cpp:651-652`) already rebinds
  skin bones post-load via `SetBone(...,false)`.
- Mechanism: `BoneTransAt(b)->WorldXfm()` then reads the member's OWN live skeleton, posed
  to her gender bind (deform-clip morph + live clip/IK on her own bones). `off_female ×
  world_female == I` → skinPos→0 AND the arm ANIMATES. Males: `own` is their own
  counterpart, offset unchanged → still clean.
- Because the frame is pose-ALL-then-draw-ALL, this is correct by construction ONLY because
  each member's OWN instance holds its OWN pose at draw (they are distinct objects, not one
  shared object that last-writer-wins). That is the precise reason DESIGN-A works where the
  constant correction can't.

### B.5 — The make-or-break risk (feasibility: HARD)

The orchestrator's prior refutation ("rebind = no-op `own==shared`, OR binds a dormant
static male-bind skeleton, still flung + un-animated") is avoided IF AND ONLY IF `Find`
from `SyncObjects` resolves to the member's LIVE `main.milo` instance, not the shared
`skeleton.milo` magnet. The 2026-06-06 probes are SPLIT on this:
- §2: at `OnInstallFilter`, each member's `FindObject("bone_R-upperArm.mesh")` returns a
  DISTINCT per-member bone (encouraging).
- §3/§4: at DRAW, the outfit meshes' bone ObjPtrs are the SHARED magnet (the outfit's
  resource bones `ReplaceRefs`-consolidated onto it at resource-parse).

If the consolidation already redirected the outfit meshes' bone refs to the magnet, a
`SetBone` rebind onto `Find(name)` only helps if `Find` from the member dir returns the
member's own instance. If `Find` ALSO returns the magnet (because name resolution within
the member's dir tree reaches the shared subdir first), the rebind is a no-op and the
band-scoped UN-SHARE of `char/main/skeleton.milo` (2026-06-06 blocker #1) must land FIRST.

### B.6 — The single build that retires the constant-correction option

`XBONE=bone_R-upperArm` with clip+IK ON (NO `RB3_NO_CLIP`/`RB3_NO_IK`),
`scripts/native/char-burst-capture.py --shots 40`: watch whether the shared magnet
`worldPos` is byte-constant across frames.
- Constant → constant correction = FROZEN female (reject, no win over clamp).
- Moves → last-writer-wins, constant correction can't track (reject).
Either way: proceed to DESIGN-A. DESIGN-A itself does not depend on this answer; this build
just closes the constant-correction branch on evidence so the Implement agent doesn't
re-chase the recompute trap.

### B.7 — Bar for the Implement agent

Do NOT claim a fix without a BUILD+PROBE showing, for the female trackjacket arm bones:
(1) `own != shared` (rebound to a distinct object), AND (2) `own` is FEMALE-posed LIVE —
skinPos 19.8→0 WITH clip+IK, no re-fling across a 40-shot burst, arm ANIMATED (not frozen
T-pose / not frozen male bind). Males stay skinPos 0; `SKIN_CLAMP_PROBE=1` shows 0 clamps
on trackjacket (still fires on crowd/extras). Shipped clamp untouched (backstop). All edits
HX_NATIVE (Wii byte-identical) or in the native-only engine files
(`Rnd_Wgpu_RB3.cpp`/`BoneSetup.cpp` — build on the dormant WIP, keep the clamp).

## A — the surgical band-only LOADER UN-SHARE (READ-ONLY, DESIGN-A)

DESIGN-B's make-or-break (B.5) is: does `Find(boneName)` from the member's dir resolve to
the member's OWN skeleton, or to the shared `skeleton.milo` magnet? DESIGN-A's job is to
make the answer "the member's own" — by un-sharing the magnet for band members ONLY, so a
plain rebind (B.4) is unnecessary: the outfit bones bind correctly by NAME at parse time.

### A.1 — WHEN the bind is established (the decisive trace, new this wave)

The outfit skin mesh's bone pointer is NOT chosen at the band-member merge; it is fixed at
the **resource/char_shared milo's PARSE time**, before any band-merge:

- `RndMesh::Load` parses `bs >> mBones` (`rndobj/Mesh.cpp:750`); each `RndBone::mBone` is an
  `ObjPtr<RndTransformable>` whose `operator>>` (`obj/ObjPtr_p.h:98`) → `ObjPtr::Load`
  (`ObjPtr_p.h:536`) resolves the bone NAME via `mOwner->Dir()->FindObject(buf, false)`
  (:542). `mOwner->Dir()` = the dir the mesh is parsing INTO; `FindObject`
  (`obj/Dir.cpp:531`) descends `mSubDirs` (:535-540).
- The female torso `trackjacket_solid.milo` references `../../../shared/char_shared.milo`
  as a subdir; `char_shared.milo` references `../skeleton.milo` as a subdir; `skeleton.milo`
  inlines `skeleton_unshared.milo` (316 bones) — verified by `strings` on the `_xbox` milos.
  So at torso-parse, `FindObject("bone_R-upperArm.mesh")` descends
  torso → char_shared → skeleton.milo → the bone. **That `skeleton.milo` is the SHARED
  preloaded instance**, because `char_shared.milo` (band-group preload, `share=true`) and
  every `*_resource.milo`'s `../main/skeleton.milo` subdir resolve through
  `DirLoader::Find` (`DirLoader.cpp:78`) to the one preloaded loader.
- => The outfit bones are male-bind-bound the moment the shared `skeleton.milo` answers the
  parse-time `FindObject`. This is why the prior "prune after merge" dead-end failed (bones
  already resolved) and why a post-merge `SetBone` rebind only helps if `Find` returns the
  per-member instance (B.5).

### A.2 — WHY the per-member `skeleton_unshared.milo` is UNUSED

`char/main/main.milo` (the band outfit container) references TWO skeletons:
`skeleton_unshared.milo` directly as **kInlineCached** (→ a fresh per-member copy, verified
8-9 distinct, `shared=0`) AND, transitively through the resource/char_shared subdirs, the
shared `skeleton.milo` root. After load the member's dir tree contains BOTH instances under
the same bone names. The pose pipeline AND the outfit bones both resolve by name via
`FindObject`, and the shared `skeleton.milo` subdir wins the descent (it is appended as a
subdir of the member by `FilterSubdir`→`kReplace`; the inline-cached copy is buried under
`main.milo`'s own inline subtree). So the per-member copy exists but never answers the
lookup — exactly the orchestrator's empirical finding.

### A.3 — The band-only scope boundary (blast-radius containment)

The share key is a FilePath STRING (`char/main/skeleton.milo`), referenced by 10 files:
`char_shared.milo` + 9 `*_resource.milo` (deform/shell/vignette/extras/viseme/keyboard/
vocal/guitar/drum). vignette/extras/shell resources are used by the CROWD + vignette cast,
NOT just the band. A global un-share at `DirLoader::Find`/`ObjDirPtr::LoadFile` or
`PreloadSharedSubdirs` would hit the whole cast. The CONTAINMENT is that crowd/extras
characters do NOT merge through a `BandCharacter` filter:
- `FileMerger::FilterSubdir` (`char/FileMerger.cpp:317`) dispatches to `mFilter` if set,
  else `DefaultSubdirAction`. `BandCharacter` sets `mFileMerger->mFilter = this`
  (`BandCharacter.cpp:1961`, in `OnInstallFilter`). The crowd uses `crowd_clips.fm` with NO
  BandCharacter filter. => `BandCharacter::FilterSubdir`/`Filter` run ONLY for the 4 band
  members. This is the band-only seam, the SAME seam the existing white-texture shim and the
  `sCharSharedDir`/`sBoneMergeDir` remaps already use.

### A.4 — The fix (surgical, at the band-only `FilterSubdir` seam)

Make the shared `skeleton.milo` (and its parent `char_shared.milo` only insofar as it
re-exposes the skeleton) NOT be the instance the band member's outfit bones resolve against,
so the member's own `skeleton_unshared.milo` answers the name. Two viable shapes, both
HX_NATIVE-gated inside `BandCharacter::FilterSubdir` (`BandCharacter.cpp:1876`):

- **A-redirect (preferred, no new instances).** When `FilterSubdir`'s `o1` is the shared
  `skeleton.milo` root (detect by `o1->mStoredFile` basename == `skeleton.milo` AND
  `o1->FindObject("bone_R-upperArm.mesh")` present), DROP it for the band member (return
  `kKeep`/skip the `kReplace`-append) AND ensure the member's own inline-cached
  `skeleton_unshared.milo` is the dir that answers the bone name. The outfit bones then
  parse-resolve to the per-member copy. Because the member's `main.milo` already loaded its
  own `skeleton_unshared.milo` (kInlineCached), this needs the per-member skeleton to be
  reachable by `FindObject` from the outfit mesh's dir at parse — i.e. the per-member
  skeleton must be appended as the member's skeleton subdir in place of the dropped shared
  one. Risk: parse-order — the outfit/resource milo must parse AFTER the per-member
  skeleton is in the dir tree (see A.5).

- **A-unshare (cleaner semantically, one copy).** Force the band member's
  `char/main/skeleton.milo` subdir reference to NOT share: in `FilterSubdir`, when `o1` is
  the shared skeleton root, replace the appended subdir with a fresh per-member COPY
  (`o1->Copy(kCopyDeep)` into a member-owned dir) OR — simpler and avoiding the
  CharServoBone ref-remap crash that killed the prior dir-copy — REUSE the member's already
  per-member `skeleton_unshared.milo` (which `main.milo` loaded kInlineCached) as the
  appended subdir, so no new copy and no servo-bone remap. This collapses A-unshare into
  A-redirect: "append the member's OWN skeleton_unshared, not the shared skeleton.milo."

Both shapes converge on: **for band members, the skeleton subdir exposed to the outfit
parse must be the member's own kInlineCached `skeleton_unshared.milo`, not the shared
`skeleton.milo` magnet.** The fix is one branch in `FilterSubdir`, HX_NATIVE, scoped by the
`mStoredFile`-basename == `skeleton.milo` string test, contained to the band by the
`BandCharacter` mFilter seam.

### A.5 — The two load-order subtleties the Implement agent MUST verify

1. **Parse-order.** The outfit/torso/resource milos must `FindObject`-resolve their bones
   AFTER the member's per-member `skeleton_unshared.milo` is reachable in the member's dir
   tree. If `char_shared.milo`/`_resource.milo` (which carry the shared skeleton subdir) are
   merged/parsed BEFORE the per-member skeleton is appended, the bones still resolve to the
   shared one. Probe with `XBONE` post-fix: the bonePtr for trackjacket must DIFFER from
   vestdenim/plaidshirt/shred (distinct per-member instances), not be identical.
2. **The pose pipeline follows automatically.** `CharBonesMeshes::PoseMeshes`
   (`char/CharBonesMeshes.cpp:98`) resolves bone transforms via
   `CharUtlFindBoneTrans(name, Dir())` (`char/CharUtl.cpp:183` → `dir->Find<CharBone>` →
   `bone->mTrans`) — the SAME name-resolution as the outfit bones. So once the per-member
   `skeleton_unshared` is the name winner, BOTH the pose pipeline and the outfit bind to it
   → the member poses its OWN skeleton (to its gender bind via the deform clip + idle clip +
   IK) AND the outfit reads that live pose. No separate "drive the skeleton" step is needed;
   it falls out of making the per-member instance the name winner. THIS is the difference
   from the refuted SetBone-rebind: the rebind only moved the outfit's read pointer; the
   un-share also moves the POSE pipeline's pointer, so the skeleton is actually animated.

### A.6 — Why this beats DESIGN-B's rebind and the prior dead-ends

- vs B's `SetBone` rebind: B's open risk (B.5) is `Find` returning the shared magnet → no-op.
  A removes the magnet from the band member's name resolution, so `Find` CAN'T return it.
- vs the prior "prune char_shared's skeleton subtree" dead-end: that pruned the shared
  instance AFTER consolidation (bones already null-resolved → numBones=0). A swaps the
  per-member skeleton IN at the same seam, BEFORE/INSTEAD-OF the shared one, so bones resolve
  to a valid (per-member) skeleton, never to none.
- vs the prior dir-`Copy(kCopyDeep)` crash (`CharServoBone.cpp:179 mFacingPos && mPelvis`):
  A-redirect REUSES the existing kInlineCached `skeleton_unshared.milo` (already correctly
  hooked by `main.milo`'s own inline load — CharServoBone/CharBones already wired to it), so
  there is NO new copy and NO servo-bone ref remap.

### A.7 — Entry points (exact)

- Share magnet created: `PreloadSharedSubdirs` (`obj/Dir.cpp:1021-1033`) → `gPreloaded[..]
  .LoadFile("char/main/skeleton.milo", async=false, share=true)`; list
  `config/preload_subdirs.dta:24` (`char` group). Also `char_shared.milo`
  (`preload_subdirs.dta:29`, band group) re-exposes it via its `../skeleton.milo` subdir.
- Shared by NAME: `ObjDirPtr::LoadFile` (`obj/Dir.h:63`), `share==true` →
  `DirLoader::Find(p)` (:67) → `DirLoader::Find` (`obj/DirLoader.cpp:78`). Subdir entry:
  `ObjectDir::LoadSubDir` (`obj/Dir.cpp:849-861`, `LoadFile(subdirpath, true, b, ...)`) and
  `ObjectDir::PostLoad` shared-inlined fixup (`Dir.cpp:402-411`, `DirLoader::FindLast`).
- Parse-time bind: `RndMesh::Load` `bs >> mBones` (`rndobj/Mesh.cpp:750`) →
  `ObjPtr::Load` `dir->FindObject` (`obj/ObjPtr_p.h:542`); `FindObject` descends mSubDirs
  (`obj/Dir.cpp:531-540`).
- THE FIX SEAM (band-only): `BandCharacter::FilterSubdir` (`BandCharacter.cpp:1876`,
  already HX_NATIVE-shimmed) — add the skeleton-root branch here. Pose pipeline follows via
  `CharBonesMeshes::PoseMeshes`→`CharUtlFindBoneTrans` (`char/CharUtl.cpp:183`).
- Diagnostics: `XBONE=bone_R-upperArm` (distinct bonePtr per member = fix working),
  `SHARD_CATCH` (0 trackjacket arm lines = no fling), `SKIN_CLAMP_PROBE` (0 clamps on
  trackjacket), `char-burst-capture.py --shots 40+`.

### A.8 — Risk & bar (feasibility: MODERATE)

The fix is one HX_NATIVE branch at a band-only seam reusing an already-loaded per-member
skeleton — no new instances, no servo-bone remap, no crowd/extras/DC3 reach. The make-or-break
is A.5.1 (parse-order): if the resource/char_shared milos parse their bones before the
per-member skeleton is the name winner, the bind doesn't move. The Implement agent must probe
`XBONE` (per-member distinct bonePtr) FIRST under `RB3_NO_CLIP=1 RB3_NO_IK=1` to confirm the
bind moved, THEN with clip+IK to confirm live female pose (skinPos 19.8→0, animated, no
re-fling). If parse-order can't be satisfied at `FilterSubdir`, fall back to DESIGN-B's
`SetBone` rebind in `SyncObjects` LAYERED ON TOP of the un-share (un-share fixes the pose
pipeline's pointer; rebind fixes any outfit bone refs that already consolidated). Shipped
fling-clamp stays as backstop throughout. Wii path byte-identical (all edits HX_NATIVE).
