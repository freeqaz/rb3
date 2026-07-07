# Lane SKEL — S-S1 (mechanism study, diagnosis-only) — PLAN

Wave 13, KEY=SKEL, STAGE=S-S1. Engine pin `44716f4`. Diagnosis ONLY — no fix, no behavior
change, no flag flips. Charter: WAVE13_KICKOFF ACCEPTANCE A1-A4 + STOP-TRIPWIRE.

## Objective
Answer, from source + runtime probes, the six binding questions:
(1) EXISTENCE of per-member finger bones, (2) ANIMATION of the un-shared instance,
(3) GENDER-POSE reaching it before rest capture, (4) the ACTUAL final offset writer for
`hands_naked`, (5) BAKE-TIME magnet provenance, (6) CROWD-interaction scoping. Then name the
exact seam for the two-half fix. NO fix this stage.

## Files READ (source trace) — line ranges re-derived BY SYMBOL on current tree
- `src/system/bandobj/BandCharacter.cpp`:
  - `Poll()` rebind ordering — `:369`; `RebindHeadHandsAtRest()` call pre-Poll `:526`;
    `Character::Poll()` `:529`; `RebindOutfitBonesToOwnSkeleton()` post-Poll `:574`.
  - `NativeCaptureRestPoseAfterDeform()` load-time seed `:973`.
  - `RebindOutfitBonesToOwnSkeleton()` `:1101` (torso-scoped default).
  - `RebindHeadHandsAtRest()` `:1253`: GeomOwner propagation `:1438-1448`; first-distinct
    clip-free rest capture `NativeCharSpaceRestXfm(own)` `:1656`; pass-B bake
    `Multiply(mesh->WorldXfm(), invRest, mesh->BoneOffsetAt(b))` `:1725`; rebound-flag `:1752`;
    `RB3_APD_DIAG` provenance log `:1676-1693`.
  - `FilterSubdir()` 2026-06-06 investigation + two-half fix + crowd warning `:3915-3937`.
- `src/system/char/CharBonesMeshes.cpp`: `ReallocateInternal()` driver pose-target bind
  `CharUtlFindBoneTrans(mBones[i].name, Dir())` `:54` (captured into `mMeshes`).
- `src/system/char/CharUtl.cpp`: `CharUtlFindBoneTrans` `:183` (= `dir->Find<CharBone/Trans>`).
- engine `src/platform/Rnd_Wgpu_RB3.cpp`: SKEL_REBAKE pre-pass `:3442-3549`, SKIN_CLAMP `:3753+`
  (both skip `mNativeBonesRebound` meshes) — READ-ONLY (renderer not touched this lane).

## Runtime probe (read-only, existing committed probes; NO new source)
Harness `evidence/s1_mechanism_probe.py` launches the committed W2.8g binary
(`native/build-agent-W2.8g/rb3-native`, commit `d016ce66`, matches HEAD BandCharacter.cpp) to a
live gameplay band render with render-inert getenv probes: `RB3_APD_DIAG`, `HEAD_REBIND_PROBE`,
`SKEL_REBIND_PROBE`, `SKEL_REBAKE_PROBE`, `RELOAD_PROBE`, `BAND_ANIM_PROBE`. All only print.
Cleanup by PGID only. Log: `/tmp/wave13-skel-s1/gameplay.log`.

## Deliverable
STATUS.md with (1)-(6) answered + probe evidence, the corrected mechanism, and the seam for the
two-half fix (files + shape). Interaction analysis vs both default-ON rebinds. NO fix.
