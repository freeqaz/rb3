# C8 dark-face — FINDINGS UPDATE (2026-07-01, orchestrator)

Measurements this session **overturn** the prior campaign's "C8 pose-basis
rotation divergence → dark normals" conclusion. Chain of evidence:

## 1. Skinning basis is CORRECT (refutes dark-normals-from-wrong-basis)
`BONE_PROBE` on a skinned head mesh (`female_extra_head.mesh`, 33 bones) in
gameplay: **every bone `skinDet = 1.0000`**, `skinRot` is orthonormal (unit,
orthogonal rows), `offRot ≈ identity`. The composed skin matrix `BoneOffsetAt·boneWorld`
is a clean rigid rotation — NOT the det-0.53 scale/shear the V38 audit hypothesized.
⇒ Normals transformed by this matrix are fine. The "wrong rotation basis breaks
normals → dark face" mechanism does **not** hold for the measured mesh.
Drive: `BONE_PROBE=1 BONE_PROBE_NAME=head` via `scripts/native/band-closeup-capture.py`.

## 2. Band members DO render faces + skin in native gameplay
`SKIN_PROBE` + direct vocalist capture (`scripts/native/band-closeup-capture.py
--member vocals`, shots `coop_front_n0*`): band members `player0..player3` are
loaded (each ~17-19 skinned meshes, ~200 bones, restPose captured, rebind runs).
The vocalist renders a **head with a face** (blonde hair, two eyes, open singing
mouth) + bare **skin** on arms/midriff/legs. Screens: `/tmp/bc-voc/voc_coop_front_n0*_0.png`,
face crop `/tmp/voc-face-n00.png`. **Characters are NOT faceless in native.**

### Residual visual issues actually observed (native, blues-club venue)
- Face skin looks **flat/untextured** (no skin-detail: brows/lips/shading baked in),
  and the **eyes render over-bright** (glowing dots). Reads as a "blank/wrong" face,
  not an absent one.
- A blown-out **magenta/pink wash** over part of the frame (right side, over the
  drummer). Note `scripts/web/magenta-capture.mjs` exists ⇒ likely a known web/tier2
  artifact. Track separately.

## 3. The SKIN_PROBE name trap (why I initially thought flesh was missing)
Crowd EXTRAS draw `*_head.mesh` + `*_skin*_medium.mesh` (obvious names). Band
members draw their flesh/head under **body-type / merged-instance names** that do
NOT contain head/face/skin substrings, and their rebind targets are `*_resource.mesh`
templates vs drawn `*_skin.N` instances (GeomOwner split). Grepping SKIN_PROBE for
"head|face|skin" misses the band flesh — but it IS drawn (confirmed visually). Do
not conclude "missing" from the name grep alone.

## Consequence for the plan
- The **emulator ground-truth** is still wanted — but RETARGET it from "compare bone
  rotation basis" (now refuted) to **"what does a correct RB3 band FACE look like"**:
  boot the real game (Wii/Dolphin or Xbox/Xenia), get a band-member face closeup,
  compare face texture/skin-shading/eye-brightness vs native. Retail stills in
  `images/retail-screenshots/` are highway/HUD POV — they do NOT show band faces.
- **Native ≠ user's report.** User sees "without faces" on **web**; native shows
  faces present. Must capture the current web build (`native/web/build/`, debug wasm
  rebuilt 2026-07-01 18:30) in gameplay to see the actual web symptom — it may be a
  web-specific face-texture/asset-load gap, or a staler deployed build.

## Open threads / re-dispatched
- T-web: capture current web build gameplay character → `t-web-face.md`
- T2 Dolphin oracle (was cut off, empty) → re-dispatch, retargeted to face closeup
- T3 Xbox oracle (was cut off, empty) → re-dispatch, retargeted to face closeup
