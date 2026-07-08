# T2-WORLDROI — M1 skinned-pose bbox + serialize (GREEN)

Binary: `native/build-native/rb3-native` (engine beb89e5 + T2 uncommitted edits), built OK.
Harness: `scripts/native/t2_worldroi_probe.py` (boots to band gameplay, RB3_DRAWLOG_PROV=1, dumps /api/drawlog?prov=1).

## Coverage on a band gameplay frame (GREEN arm, RB3_PROV_SKIN_SPHERE unset)
```
frame=1873 total_draws=424 skinned=306
  skinned rectKind: 3(pose-bbox)=306 1(sphere)=0 2(unavail)=0 0(verts)=0
  boneFallback histogram (N bones@bind -> #draws): 0:300, 9:4, 14:2
```
- **306/306 skinned draws → rectKind:3** positioned bboxes. ZERO fell to the sphere fallback → confirms B4 (no mesh-cache bypass: every skinned draw reaches the compose loop + sidecar).
- boneFallback path exercised: 6 draws carry clamped/null/nonfinite bones (4@9, 2@14) — B2's bind-pose union fires for exactly these.

## Example rectKind:3 rows (positioned, real bones, NOT near-full-viewport)
```
mesh='jp80_strings.mesh'            rect=[536,356,9,140]   bones=[bone_bridge, bone_vibrate_hi, bone_bend_string01..04, bone_nut, bone_vibrate_low]
mesh='motorcycleboots_resource'    rect=[918,483,74,69]   bones=[bone_L-knee, bone_L-toe, bone_R-ankle, bone_R-toe, bone_L-ankle, bone_R-knee]
mesh='greaserjacket_resource.1'    rect=[962,349,12,54]   bones=[bone_spine1, bone_spine3, bone_spine2, bone_L-deltoid]
mesh='tightdistressedpants_resource' rect=[918,383,74,169] bones=[bone_spine2, bone_pelvis, bone_L-knee, bone_R-ankle, ...]
```
Rects are small + placed (9x140, 74x69, 12x54) vs the R3 v1 sphere blindness — the killer-query localization works.
(owner='' at M1 — the Character::DrawShowing owner-scope hook is M2/E3.)

## G4 flag-OFF golden regression net (BINDING contract)
```
drawlog-golden.py --fixed-clock --scene splash_screen --canonical-order
  -> PASS (canonical-order): live capture matches golden (792 draws) (281 known-residual within bound, non-blocking)
drawlog-golden.py --fixed-clock --scene splash_screen --fail-red-audit
  -> FAIL-RED AUDIT OK: perturbed golden correctly compared as FAIL (draw 0 world[12])
```
Flag-OFF byte-identical (792 canonical): prov off => mDrawProv empty => RecordDrawProv never called => T2 edits inert. Comparator still reads RED on perturbation.
