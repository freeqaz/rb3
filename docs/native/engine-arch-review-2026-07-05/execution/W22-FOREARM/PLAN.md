# W22-FOREARM — PLAN

**Lane:** fix the player3 right-forearm float. **Charter:** WAVE22_KICKOFF.md + A1–A9 (WAVE22_REVIEW.md, binding).

## The bug (already NAMED, T2 M5)
Persistent top-center floating flesh above the band = `gloves_resource.mesh` +
`clearcoat_resource.mesh` (sleeve), owner **player3**, bones `bone_R-foreArm`/
`foreTwist1`/`foreTwist2`/`bone_R-hand`. `boneFallback=0` (not clamp).

## Step 0 — A1 discriminator (ONE boot, ZERO fix code, checkpoint verdict EARLY per A9)
Boot band gameplay `RB3_LOADBIND_PROBE=1 SKEL_REBIND_PROBE=1`. For player3 ×
{gloves_resource.mesh, clearcoat_resource.mesh} × slots {bone_R-foreArm,
foreTwist1, foreTwist2, bone_R-hand}: read `mesh->BoneTransAt(b)` (ptr + owning
dir identity + world) vs `Find<RndTransformable>(name,false)` on player3's dir.

Existing `[LOADBIND_SLOT]` probe (BandCharacter.cpp:224) already logs boundPtr,
owningDir, owningDirClass, distinct(own!=bound) — but (a) scopes to
hand/glove/torso only (clearcoat NOT matched), (b) does NOT log world xfm.
→ May need a scoped probe extension (instrument, not fix) to add world + clearcoat.

VERDICT:
- bound≠own AND bound-world elevated AND own-world at-arm ⇒ **BINDING BUG** →
  arm-scoped rebind (A2 guardrail: clearcoat mesh-level OK; gloves BONE-SLOT
  scoped R-foreArm/foreTwist1/2/hand ONLY, NEVER finger slots).
- bound==own ⇒ **exonerated** → POSE investigation (R-vs-L bilateral world
  compare; clip driver / IK / attachment / transposed ObjPair like d988a301).

## Fix (flag-first, default-OFF, HX_NATIVE)
- Binding: extend rebind to player3 R-arm outfit slots per A2 guardrail.
- Pose: match-neutral if possible, else HX_NATIVE flag-gated.

## Gates (A2/A3)
- PRIMARY: burst-ROI numeric (t2_worldroi_burst.py, RB3_FIXED_CLOCK=1
  RB3_DRAWLOG_PROV=1): flag-ON → ZERO player3 gloves/clearcoat draws in
  above-heads strip (y<~300) WHILE those meshes STILL draw nonzero on body
  (anti-"fixed by disappearing").
- SECONDARY: R-foreArm vs L-foreArm world symmetry.
- E1: human burst-pair review + FINGER-region crop UNCHANGED vs flag-OFF (A2.iv).
- flag-OFF drawlog-golden 792 byte-identical.
- batch_objdiff==baseline on touched src/system unit.
- rb3-tests 116/0 on BandCharacter.cpp touch.

## Deliverables
PLAN.md (this), STATUS.md (verdict headline), evidence/ (discriminator log,
burst before/after, finger no-change crop, gate results), code default-OFF,
checkpoint w/ discriminator verdict EARLY.
