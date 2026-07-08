# T2-WORLDROI — M3 query script + coverage count (GREEN)

## E5 — uidump_query.py --roi now names bones + owner
`print_roi` prints per matched draw: `mesh / owner / mat / pass / rectKind / skinned` and, for
rectKind==3 rows, a `bones=[…] boneFallback=N` line (via the new `bones_in_roi(prov, roi)` helper
that intersects each per-bone screen sub-rect with the ROI). Live run in `M3-uidump_query-roi.txt`.

Example (ROI [540,310,60,160] over player0's guitar neck):
```
mesh='jp80_strings.mesh' owner='player0' rectKind=3  rect=[539,357,8,125]
    bones=[bone_bend_string01..05, bone_nut]  boneFallback=0
mesh='gloves_resource.mesh' owner='player3' rectKind=3  rect=[592,305,169,47]
    bones=[bone_R-index01..03, bone_R-middlefinger01..03, bone_R-ringfinger01..03, ...] boneFallback=0
mesh='hippyfringe_resource.mesh' owner='player1' rectKind=3 boneFallback=14  # bind-sphere union fired
```
Contrast: static non-skinned meshes (room_ceiling, stage) STILL report the legacy rectKind=1
`[0,0,1280,720]` full-viewport sphere — those are out of T2 scope (T2 fixes SKINNED draws).

## Coverage count (band gameplay frame; RB3_DRAWLOG_PROV=1, GREEN arm)
```
skinned draws = 304
  rectKind:3 (skinned-pose bbox)     = 304   (N)
  rectKind:1 (disclosed sphere fallback) = 0   (M)
  boneFallback>0 draws               = 6  (4@9-bone, 2@14-bone; bind-sphere union path)
```
**N=304 localized, M=0 fell back.** Every skinned character draw gets a positioned pose bbox — no
silent sphere. (The disclosed-bypass path is exercised & correct per M1's fallback-forced check.)
