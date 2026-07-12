# W31-HUBWALKER-SHARDS — VERDICT

**Verdict: (iii) = SKEL-FAMILY → STOP. No fix lands this wave.**
Binds to `R5-HANDS-ENDGAME/CLOSURE.md` (band seed-R / static-skeleton family) and the
W23-29 CROWD-chain closure (README: "CROWD CHAIN CLOSED — measurement artifact, DO NOT
REOPEN"). Diagnosis-only mandate honored: no fix code, no engine writes, no flags, no
class.json.

Base SHA fd119705 | engine pin b36bcfc | probe TU `native/src/rb3_shardprobe_native.cpp`
(read-only, distinct TU, A2-compliant) + the engine's existing read-only draw-path probes
(`SKIN_CLAMP_PROBE`, `SKEL_REBAKE_PROBE`, `BONE_PROBE`).

## Mechanism (named, matrix-relative + pointer-verified)

Every hub-walker shard — forehead flesh cone, waist stick-fans, crumpled boots/dangling
foot — is a **bone-skinning-basis fling**: for a skinned mesh's bone `b`, the composed
skin `S_b = BoneOffset(b) · boneWorldXfm(b)` lands vertices far off bind in mesh space
(`|S_b · inverse(meshWorld)|`, the engine's own SKIN_CLAMP metric, 18u–780u vs the 12u
shard threshold). The outfit/skin/face/hair meshes are vertex-skinned to a skeleton whose
LIVE pose does not match the mesh inverse-bind offsets:
- band outfit (`hippyfringe_resource.mesh`) → `char/main/skeleton_unshared.milo` (the
  STATIC shared magnet — `SKEL_REBAKE` probe: `boneDir='char/main/skeleton_unshared.milo'`);
- street crowd/extras (`*_extras_*`, `*_extra_head*`, `goatee_resource`) → char/extras &
  char/crowd per-character skeletons.

### seed-R signature (live-bone probe, A12) — `female_extra_head.mesh`, 33 bones
`BONE_PROBE` on the actual rendered head mesh:
- `skinDet = 1.0000` on every bone — the fling is an **orthonormal rotation basis**, NOT a
  scale/degeneracy bug.
- `skinRot ≈ [0.745 -0.665 -0.047 / 0.666 0.738 0.107 / -0.037 -0.111 0.993]` — a coherent
  ~42° rotation (trace→42.4°) off identity (identity = bind). Same rotation-basis class as
  the R5 87.2° band-hand seed-R; the specific angle varies by chain, the class is one.
- **All face bones collapse to a shared apex** `skinPos ≈ (-287, 56, 123)` while the
  inverse-bind `offPos ≈ (0..-2, -4, -64)` — every face vertex pulled ~290u to ONE point.
  That shared-apex, orthonormal-rotation fling IS the point-radial R·sin(θ) cone the user
  sees over the forehead.

## Discriminator resolution

- **(i) shard meshes + bound bones** — draw-path SKIN_CLAMP census (per-mesh worst bone,
  mesh-local extent), `evidence/shard_mesh_table.tsv`. Forehead cone is directly named:
  `male_extras_eyebrows11.mesh`→`bone_forehead.mesh` (650u), `male_extra_head03.mesh`→
  `bone_L-brow1.mesh` (780u), `goatee_resource.mesh`→`bone_L-lipcorner.mesh` (650u),
  `female_extra_hair02.mesh`→`bone_hair_R-front03.mesh` (648u). Waist/boot fans =
  `bone_spine*`, `bone_torso_hippyfringe-*`, `bone_*-toe/knee/thigh/upperArm`, finger bones
  on the body/skin meshes. Bones pointer-identified via `BoneTransAt` in the probe;
  boneDir named by `SKEL_REBAKE_PROBE`.
- **(ii) driven by the walk clips?** — YES, the bones are DRIVEN. Player0-3 body driver
  `main.drv` plays the `playerN_{m,f}` walk clips (70 tracks each). The crowd/extras servo
  skeletons animate (that is WHY they fling — a moving bone with a mismatched bind). The
  large extents (250–780u) with coherent live rotations are the proof of drive: an undriven
  bone sits at bind (extent≈0, skinRot≈I, skinPos≈offPos). Refutes the "undriven walk-clip
  prop-track gap" hypothesis. (Note: the player0-3 `expression.drv` FACE driver carries no
  clip (0 tracks), but that is a separate observation — the face bones still inherit the
  static-skeleton bind mismatch; a driven face clip would not change which skeleton the
  face mesh is bound to.)
- **(iii) SKEL 87° family vs distinct undriven-track gap** — **SKEL-FAMILY.** The error is a
  skinning-basis (offset vs live-pose) mismatch on a bound-to-the-wrong-skeleton mesh, not a
  missing track on the same skeleton. Adding/scoping a walk-clip prop track cannot repoint an
  outfit/face mesh's bind skeleton. The seed-R orthonormal-rotation shared-apex signature
  seals it.

## Census-trap / attribution (E7, W29 Q(b) — BINDING)

The kickoff bound the target to "player0-3." Pointer-keyed measurement REFINES that name-key:
- CharCache `player0..3` (band-preview walkers, distinct char_addr
  0x…c8e0/…afe0/…e700/…c080) report **0 skinned meshes** in their own dir; they are NOT the
  shard bearers in this shell.
- The VISIBLE hub shards (incl. the forehead cone) are on the **street CROWD/EXTRAS**
  characters (`*_extras_*`, `*_extra_head*`, `*_crowd_*`, `goatee_resource`) + the band
  outfit fringe (`hippyfringe_resource` → the static magnet). This is exactly the name-key
  census trap the kickoff flagged: "hub street walkers" resolves to crowd/extras, whose
  shard chain W23-29 already CLOSED, plus the R5-closed band seed-R class.

## Existing mitigations already in the tree (why some shards still show)

The engine layers three responses to this family, all present + default-ON:
1. `BandCharacter::RebindOutfitBonesToOwnSkeleton` (decomp faithful fix) — band outfit skin
   repointed to the member's own gender-posed live skeleton.
2. `SKEL_REBAKE` — static rebake of arm/twist/torso/leg outfit bones (>12u) to the live pose.
3. V24 per-frame fling clamp / `SKIN_CLAMP` (>12u → bind) — the backstop.
Face/hair/goatee/finger meshes are DELIBERATELY excluded from rebake (their bones are driven
by CharHair/CharFaceServo/CharIKFingers, so a static rebake would freeze them) and left to
the clamp, which clamps to bind — the engine comment records the residual verbatim: **"the
650u goatee/hair flings stay exactly as the shipped clamp left them."** The forehead cone /
brow / goatee shards the user sees are precisely that accepted clamp residual on the face/
hair chains — the FOREARM-FLOAT-class residual R5-HANDS-ENDGAME/CLOSURE.md §POST-FLIP
ADDENDUM already recorded ("persistent top-center floating flesh-colored structure … NOT
finger-level").

## Lint-4 (RB3_PROP_POSE_FULL, 15th default)

No contradiction. `RB3_PROP_POSE_FULL` governs perf-clip prop-pose fullness; the hub shards
are a skinning-basis fling on crowd/extras/outfit meshes — an orthogonal mechanism. W30's own
flip retest already reported hub-walker fans SURVIVE the flip; that is consistent (the flag
does not touch the static-skeleton bind mismatch), not a contradiction.

## Lint-6 (family option table)

Bound to the SKEL family coverage table (R5 five dead fix classes). Verdict SKEL-family ⇒
STOP + memo; no 7th cell opened.

## Bottom line

A scoped walk-clip prop-track fix is NOT legal (the (iii)-undriven branch did not obtain).
The hub-walker shard family is the already-closed SKEL static-skeleton seed-R class (band:
R5-HANDS-ENDGAME; crowd/extras: W23-29). Any real fix is per-character skeleton instancing /
bind-offset recompute on the crowd/extras + face/hair chains — an engine-side change to a
twice-closed family, out of scope for a diagnosis lane and gated behind those closures'
reopen conditions.
