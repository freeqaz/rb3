# Lane SKEL — S-S1 — STATUS — VERDICT: **DIAGNOSED (prior "shared magnet" premise INVERTED by runtime pointers)**

Diagnosis-only. NO fix, no behavior change, no flag flips. Engine pin `44716f4`. Evidence from
source trace + a read-only runtime probe run (committed W2.8g binary `d016ce66`, gameplay band
render, `evidence/s1_mechanism_probe.py`, log `/tmp/wave13-skel-s1/gameplay.log`).

## Headline (this is the load-bearing result)

The Wave-9→12 mechanism said: *"the ANIMATING bone `Find(name)` resolves is the SHARED MAGNET
(invOff identical 106° across members); the member's authored rest lives only on a STATIC
per-member bone that does not animate."* **The runtime pointers are the OTHER WAY AROUND.**

For every one of the 205 sampled `hands_naked` finger-bone slots (`RB3_APD_DIAG`, distinct=1 for
all 205):
- **`own = Find(boneName)` is PER-MEMBER and ANIMATES.** Four distinct pointers, one per member,
  each in its own contiguous per-member allocation block; the same-name bone reads a different
  pointer for player0/1/2/3. `HEAD_REBIND_ANCHOR` fired 0× → every `own` is a trans-descendant of
  its BandCharacter ("mine"), not a foreign/shared root. When a member plays an animated body clip,
  its `own` finger bone MOVES 100–276 u/frame across `Character::Poll()` (player0 guitar 186u,
  player2 mic 276u); members on a vignette/`(none)` clip sit at rest (~0u — role, not a freeze).
- **`bound = mesh->BoneTransAt(b)` is the SHARED static.** ONE pointer per bone name across ALL
  members (e.g. `bone_R-middlefinger03` bound=`0x…730140` for player0/1/2/3 identically). 42
  distinct `bound` pointers = one shared skeleton copy; 116 distinct `own` = per-member (~1 per
  bone per member). This shared static copy is the mesh's embedded authored bind pose — exactly the
  "shared `char/main/skeleton_unshared.milo` root" the 2026-06-06 `FilterSubdir` note
  (`BandCharacter.cpp:3915-3937`) correctly identified as the BIND target.

So the earlier "invOff identical 106°" was the **shared authored offset the engine dual-skin probe
read on PRE-rebind meshes** (that probe gates on `!mNativeBonesRebound` + `wext>60`, so it only
ever sees meshes before `RebindHeadHandsAtRest` rebakes them). It labelled the shared BIND source
("own") and the per-member driven bone ("bound") backwards. The drawn `hands_naked` is post-rebind:
repointed to the per-member animating `own` and rebaked against `own`'s rest.

## Consequence for the kickoff's fix statement (A1)

The kickoff/A1 seam — *"un-share `char/main/skeleton.milo` at the name-resolution / share layer so
`Find` resolves the member's own bone"* — is **a NO-OP: `Find` ALREADY resolves the member's own
animating bone.** The un-share half is, in effect, already done at the resolution layer. Pouring S2
effort into a `Dir.cpp`/`LoadSubDir`/`DirLoader` share-layer change (the "broad, high-risk, would
also touch the crowd" work the source warns about) would change nothing at the draw path and would
needlessly assume the crowd risk. **The residual is a REST-BASIS / authored-bind reconciliation, not
a sharing problem.**

## The corrected mechanism (what actually smears)

`RebindHeadHandsAtRest` default path (all flags OFF) for `hands_naked`:
1. `bound = BoneTransAt` (shared authored bind copy) → `own = Find(name)` (per-member animating);
   `own != bound` for all 38/40 bones (distinct=1).
2. First clip-free distinct resolve captures `rest = NativeCharSpaceRestXfm(own)` (`:1656`),
   repoints `SetBone(b, own)` (`:1707`), bakes `off = meshWorld · inv(rest)` (`:1725`), sets
   `mNativeBonesRebound=true` (`:1752`). `RB3_APD_DIAG` confirms `bakedRest.ang == ownNow.ang` for
   every bone → the offset is coherent **w.r.t. `own`** at its captured rest.

The mesh VERTICES + their authored weights, however, were skinned against `bound`'s bind pose (the
shared static copy), NOT `own`'s rest. `own`'s rest differs from `bound`'s per bone, and the delta
is **gender-differentiated**: for the same bone, `ownNow.ang` clusters by gender while `boundNow.ang`
is a single shared value —

| bone | male own (p0/p2) | female own (p1/p3) | shared bound |
|---|---|---|---|
| `bone_R-index01` | 109.5° | **120.1°** | 103.0° |
| `bone_L-hand`    | 127.2° | 127.5° | 146.7° |
| `bone_L-middlefinger01` | 116.2° | 116.5° | 136.6° |

The female's `own` rest sits ~17°/bone off the shared `bound` (males ~6°). So the authored shell
(baked in `bound`'s frame) is composed on `own` (per-member, gender-posed) → a per-bone conjugation
`ownRest · inv(boundRest)` rotates each finger sub-shell → the R·sinθ far-vert fling — worst for the
female. This is exactly the W2.8g "SPACE/composition axis" (isoDistort≈0, orthoResid≈0, rigid
per-bone shell rotation), now with the pointers assigned correctly: the animating bone is per-member
(good), but the mesh's authored bind (`bound`) is shared and single-basis, so it disagrees with
`own`'s gender-posed rest.

## The six questions, answered with probe evidence

**(1) EXISTENCE — YES.** Per-member finger bones exist and are name-resolvable. `Find(fingerName)`
returns 4 distinct per-member pointers (one per member), all trans-descendants of their
BandCharacter (`HEAD_REBIND_ANCHOR` 0 foreign). There is nothing to "extend"; the A1 fallback
("extend the skeleton + drive new bones") is not needed.

**(2) ANIMATION — YES, the driver follows the per-member instance.** Root cause proven at source:
`CharBonesMeshes::ReallocateInternal` (`CharBonesMeshes.cpp:54`) binds each pose target by NAME —
`CharUtlFindBoneTrans(mBones[i].name, Dir())` = `dir->Find<CharBone/.trans/.mesh>` (`CharUtl.cpp:183`)
— captured into `mMeshes`, then `PoseMeshes()` writes those captured pointers each frame. Because
the driver uses the SAME per-dir `Find` the mesh bind uses, both resolve the SAME per-member `own`.
Runtime confirms: `own` moves 186u (guitar)/276u (mic) per frame. **The un-shared instance is
DRIVEN, not frozen** — provided the resolution stays a per-dir name lookup (it does). NB the
STOP-TRIPWIRE's "post-load rebind to the unshared STATIC bone = 5th dead class" is exactly the
freeze that would occur if a fix swapped instances AFTER `ReallocateInternal` captured `mMeshes`;
the current binding does not, and any fix must likewise land before/through name resolution.

**(3) GENDER-POSE — reaches `own`, NOT `bound`.** `own`'s rest is gender-differentiated (index01
male 109.5° vs female 120.1°), so `SetDeformation`/the deform clip DO pose the per-member `own`
skeleton. The SHARED `bound` is a single gender-neutral basis (103.0° for all). So the gender pose
reaches the animating bone but NOT the mesh's authored bind copy — the reconciliation gap is
`own`(gender-posed) vs `bound`(shared) exactly because gender-pose reaches only one of them.

**(4) OFFSET WRITER — `RebindHeadHandsAtRest`, its default distinct-resolve bake (`:1656/1707/1725/
1752`).** `hands_naked` is self-owned (`CHAR_MESH shared=0`), so the GeomOwner-propagation branch
(`:1438-1448`) is NOT taken; it is not torso-named so `RebindOutfitBonesToOwnSkeleton` skips it; the
engine SKEL_REBAKE (`Rnd_Wgpu_RB3.cpp:3474`) skips it because `mNativeBonesRebound=1`. `RB3_APD_DIAG`
fires for all 205 of its finger slots inside `RebindHeadHandsAtRest`'s default branch; `CHAR_MESH`
shows `rebound=0→1`; it never appears in `HEAD_REBIND_PENDING` (miss==0 → pass B runs). The W2.8e
rule-7 "never fired" note was about a *different, removed* probe (`RB3_APD_RESTSRC`), not this path.
**Any seam that does not sit on `RebindHeadHandsAtRest`'s rest-capture/bake (or on the mesh verts it
composes) will no-op.**

**(5) BAKE-TIME provenance — CONFIRMED, with the honest correction.** The shared object at bake time
is `bound` (the mesh's authored bind copy, 1 instance across members), read by
`mesh->BoneTransAt(b)`; it is what the verts + inverse-bind were authored against. It is NOT the
`Find`/driven bone (per-member). The A1 caveat ("106°-identity is also consistent with per-member
instances of the same male-bind file") is resolved: `own` IS per-member (4 pointers) AND
gender-posed (male/female cluster) — instancing is already present; the identity the old probe saw
was the shared `bound`, not `own`.

**(6) CROWD — the risk EVAPORATES; the fix is inherently band-side.** Because no share-layer
un-share is required (Find already per-member), the seam does NOT touch `Dir.cpp`/`LoadSubDir`/
`DirLoader` and therefore cannot reach crowd load
(`Crowd.cpp:930 RebindCrowdCharBonesToOwnSkeleton`, W2.3 load-bearing). The reconciliation lives
entirely inside `BandCharacter` (`RebindHeadHandsAtRest`, band-dir-scoped — crowd/extras are never
in a BandCharacter dir, `:1252`) or in the per-band-member resource-milo bind instancing. Crowd
inputs are provably untouched (no code on the crowd path changes). If S2 wants an explicit gate:
W2.1 placement oracle GREEN both arms + `RB3_NO_CROWD_REBIND` 24× shard-drop fail-red still
reproduces + per-dir guard-DROP census unchanged (all instruments already landed, W2.3).

## The seam for the two-half fix (named; NO fix built here)

The offset-bake classes are EXHAUSTED (W2.8e/W2.8g: own-live+own-rest = shell shard; own-live+
bound-rest = shard-at-rest; bound-live+bound-rest = freeze — 5th/6th dead cells). With `own` already
per-member+animating+gender-posed, the STOP-TRIPWIRE's "which BONE animates" is already satisfied.
The remaining gap is **the mesh's AUTHORED BIND (`bound`, shared, single-basis) vs `own`'s
per-member gender rest.** Two live seam shapes (S2/coordinator to choose; both are band-side,
crowd-free):

- **Seam A — per-member gender-correct authored bind (`bound`).** Un-share the mesh's EMBEDDED bind
  skeleton copy (`bound = BoneTransAt`, today one shared instance) per band member and gender-pose
  it so `boundRest == ownRest`. Then the default `off = meshWorld·inv(ownRest)` composes the
  authored shell coherently (males already ~coherent because their `own`≈shared `bound`). This is
  the source's "un-share AND gender-pose" applied to the CORRECT object — the resource-milo embedded
  bind, NOT `char/main/skeleton.milo`. Location: `BandCharacter` install/merge + rest-capture path
  (`RebindHeadHandsAtRest` `:1595-1725`), band-dir-scoped.
- **Seam B — per-vertex shell re-pose.** Transform the authored verts from `bound`'s frame into
  `own`'s rest frame (`v' = ownRest·inv(boundRest)·v` per dominant bone) once at load, so the shell
  is authored for the bone it is drawn on. This is a MESH-DATA edit (NOT an off_b bake → outside the
  dead class), and is the W2.8g "Instrument-B shell" gate's natural fix. Renderer stays read-only;
  the re-pose is a `BandCharacter`/skin-mesh load step. Higher complexity; the trustworthy gate
  (shell invariant + wext-collapse-without-freeze) already exists.

Both keep `own` (animating, per-member) as the draw bone — so neither regresses the two default-ON
rebinds: `RebindOutfitBonesToOwnSkeleton` (torso-scoped, already repoints torso→own) and
`RebindHeadHandsAtRest` (this writer). Seam A refines what `RebindHeadHandsAtRest` bakes against;
Seam B refines the verts it composes. Neither touches the share/crowd layer.

## Interaction vs the two default-ON rebinds
- `RebindOutfitBonesToOwnSkeleton` (`:1101`, torso-only, DEFAULT-ON): skips `hands_naked` (not
  torso-named) and skips any `mNativeBonesRebound` mesh. Its own repoint (bound→own) is the SAME
  per-member-`own` target this diagnosis confirms is correct for torso arms (`Find(upperArm)` →
  per-member animating). No conflict; a hands seam must not claim torso meshes.
- `RebindHeadHandsAtRest` (`:1253`, DEFAULT-ON): the writer for hands. Seam A/B are refinements of
  its capture/compose, not new owners — single-writer preserved.

## Gates / process
- NO source changed (used only committed getenv probes). Binary is the committed W2.8g
  (`d016ce66`), render-inert probes → flag-OFF byte-identical by construction; no drawlog re-run
  needed (no code delta). No engine edit, no classification.json touch, no pin bump, no flag flip.
- Refuted flags UNSET in the run (no `RB3_HANDS_*_FIX`/`RB3_APPENDAGE_*`/`RB3_SKEL_REBIND_FULL`);
  only diagnostic print flags set. Six shipped defaults untouched. Left the uncommitted engine
  `FxSendNative.cpp` audio edit intact. Process cleanup by PGID only.
- Staged only my own files under `flock /tmp/rb3-git.lock`.

## Evidence
- `evidence/s1_mechanism_probe.py` (harness), `/tmp/wave13-skel-s1/gameplay.log` (1.3MB, not
  committed — regenerable, boot-nondeterministic). Key greps reproduced in this STATUS.
- Checkpoint: `/tmp/wave13-checkpoints/S-S1.json`.
