# T2-WORLDROI — M4 gates G1-G4 (ALL GREEN)

Runner: `scripts/native/t2_worldroi_gates.py` (two boots of the SAME binary — RB3_PROV_SKIN_SPHERE
is env-cached per B3). Evidence: `gates/gates_result.json`, `gates/{green,red}_drawlog.json`,
`gates/{green,red}_frame.png`. Headless RB3_HTTP=1 RB3_FIXED_CLOCK=1, free port, pgid-only cleanup.

## G1 — RED baseline / known-answer localization contrast (fail-red 1, review B3) — GREEN
Same band ROI [540,310,60,160] (upper-center band, player0 guitar neck), two arms:
- **GREEN** (default): whole frame 303/303 skinned draws rectKind:3; **117** intersect the ROI,
  **106 named mesh+bone** (e.g. hippyfringe_resource / player1 / [bone_L-foreTwist1, bone_L-foreArm, ...]).
- **RED** (RB3_PROV_SKIN_SPHERE=1): whole frame 302 skinned rectKind:1 + 3 rectKind:2; **0** intersect
  the ROI (`red_rectKinds=[]`), **0 named**.
**PASS**: GREEN names >=1 mesh+bone the RED arm cannot (106 vs 0). The legacy sphere for skinned
character meshes is bind-pose mesh-local projected -> MISLOCATED (it doesn't even reach the band
ROI) — this is exactly why B3 replaced the cardinality criterion with a localization contrast: a
`count(green) < count(red)` test would have FALSE-RED'd (RED returns FEWER, not more). Cardinalities
are committed as evidence, not the pass gate.

## G2 — disjoint-ROI negative control (fail-red 2) — GREEN
Same GREEN boot. Background top-left corner ROI [0,0,80,80] vs the band ROI.
- band-ROI skinned owners = {player1, player2, player3, crowd_male01-04, crowd_female02-04} (10)
- corner-ROI skinned owners = {} (empty)
- leaked band owners into corner = **none**
**PASS**: skinned owner sets DISJOINT — no "everything matches everywhere." (The full-viewport
STATIC venue meshes room_ceiling/stage still carry the legacy rectKind:1 [0,0,1280,720] sphere and
owner='' — a PRE-EXISTING, non-skinned, out-of-T2-scope behavior; T2 fixes SKINNED draws.)

## G3 — known-answer positive control (hand mesh + finger/wrist bones, review B5) — GREEN
Hand ROI [560,300,220,80] over the fretting/instrument hands band (coherent frame, mitten no-op):
```
hands_naked.mesh   / player2 / [bone_R-middlefinger01, bone_R-index01, bone_R-foreArm, bone_R-foreTwist1/2]
gloves_resource.mesh / player3 / [bone_R-middlefinger02, bone_R-thumb02, bone_R-pinky01, bone_R-foreArm]
cast_resource.mesh  / player2 / [bone_L-upperArm, bone_L-foreArm, bone_L-foreTwist1]
```
**PASS**: named meshes are hand/glove meshes and their intersecting boneRects name finger/wrist bones
consistent with the hand-bone taxonomy — the world-cam projection + bone naming is CORRECT, not just
bounded. B5 caveat honored: wrist/forearm-level naming accepted; a mitten-triggered frame (rendered
hand at wrist-rigid vs reported finger bones) would not be misread as failure.

## G4 — flag-off golden invariance (regression net) — GREEN (see M1-coverage.md)
`drawlog-golden.py --fixed-clock --scene splash_screen --canonical-order` -> PASS (792 canonical);
`--fail-red-audit` -> comparator reads RED on perturbation. Flag-OFF byte-identical.

## OVERALL: ALL GREEN (G1, G2, G3, G4).
