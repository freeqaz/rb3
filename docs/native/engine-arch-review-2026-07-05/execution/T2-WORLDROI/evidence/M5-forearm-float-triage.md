# T2-WORLDROI — M5 FOREARM-FLOAT production smoke (TRIAGE ONLY, no fix)

**Backlog key** (R5-HANDS-ENDGAME/CLOSURE.md:98-101): *persistent top-center floating
flesh-colored structure in the burst_08/12 frames, unchanged OFF->ON, forearm/prop-level
(NOT finger-level), Wave-9 "disconnected floating forearm" lineage.* T2's job = NAME it.

**Protocol:** `t2_worldroi_burst.py` boots to band gameplay (RB3_DRAWLOG_PROV=1) and captures
paired (screenshot, skinned-prov drawlog) per burst step. On `shot_00` (frame 1701; wide band
shot with the exploded flesh geometry — see `M5-frame-shot00.png` + `M5-forearm-float-crop.png`),
the top-center flesh structure floats ABOVE the band members' heads (heads start y~307).

**Query** (top-center floating strip ROI [620,278,140,34], above the heads) — `M5-forearm-float-query.json`:
```
y=284  mesh='gloves_resource.mesh'   owner='player3'  bones=[bone_R-foreArm, bone_R-foreTwist1/2, bone_R-hand, R+L fingers]
y=288  mesh='gloves_skin.2.mesh'     owner='player3'  bones=[bone_R-foreTwist1/2, bone_R-hand, bone_L-hand, ...]
y=297  mesh='clearcoat_resource.mesh' owner='player3' bones=[bone_R-foreArm, bone_R-foreTwist1/2, bone_R-hand, bone_L-foreArm]
        (+ clearcoat_skin.1 / clearcoat_resource.1 — the sleeve over the same arm)
--- band heads begin below here ---
y=307+ head/eyes/eyebrows (player2)  <- ordinary in-place faces, NOT the float
```

## ANSWER (the one-query replacement for the eyeball-lineage triage)
- **mesh:** `gloves_resource.mesh` + `clearcoat_resource.mesh` (sleeve) [player3's right arm]
- **bone(s):** `bone_R-foreArm`, `bone_R-foreTwist1`, `bone_R-foreTwist2`, `bone_R-hand`
- **owner:** `player3` (a band member — consistent with the "male guitarist / right" lineage)

## Triage finding (for the future fix charter, NOT fixed here)
`boneFallback=0` on every FOREARM-FLOAT draw → the forearm is NOT bind-clamped/nonfinite; its
bone WORLDS are genuinely POSED at an elevated screen position (the R-foreArm/R-hand skeleton
sits above the head). So this is a POSE/placement float (the arm bone is driven high), DISTINCT
from the finger-level mitten/clamp class R5 closed. A fix should look at what poses player3's
right-arm chain (bone_R-foreArm) up-and-detached, not at bind-clamp/skin fallback.

(Non-deterministic band frame -> triage on ONE captured frame, as the charter specifies. No fix.)
