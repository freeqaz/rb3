# R3-UIDUMP — DUPLICATE LANE U COLLISION (detected 2026-07-07)

## Situation
Two live Lane-U agents are running concurrently against the SAME engine worktree,
rb3 build dir, and shared game source. The harness restart mentioned in the resume
context appears to have spawned a second Lane U WITHOUT reaping the first.

Evidence (from `ps`/`pgrep` during my session):
- PID 3216439 (not issued by me): `flock /tmp/rb3-native-build.lock cmake --build
  native/build-agent-W17-UIDUMP ... > /tmp/w17u-build5.log` then
  `_uidump_m1_probe.py --out /tmp/uidump_m1` — the other Lane U's M1 rect probe.
- PIDs 3216517/3216519/3216520: that probe + its `rb3-native` child, live.
- `src/platform/Rnd_Wgpu_RB3.cpp` mtime advanced 3x during my session
  (…608 → …944 → …1034) with edits I did NOT make — notably the RecordDrawProv
  sphere-fallback was rewritten from the 8-corner/full-viewport-bail form to a
  center-anchored "no full-viewport bail" form by the other instance.
- The other instance writes checkpoints/probes to `/tmp/uidump_m1` and
  `/tmp/w17u-build5.log`; I was writing to `execution/R3-UIDUMP/evidence/m1_probe`.

## Why I stood down (did not continue M2)
Two agents co-editing `Rnd_Wgpu_RB3.cpp`, the engine header, and shared game TUs
(Text.cpp / UILabel.cpp / PanelDir.cpp) + both creating `native/src/rb3_uidump.cpp`
cannot be made safe — last-writer-wins clobbers and interleaved builds corrupt the
shared build dir. The other instance is marginally AHEAD on M1 (it made the sphere
refinement) and neither instance had started M2 yet. Racing would destroy work.
Per multi-agent hygiene (CLAUDE.md: never fold concurrent agents' work; avoid
shared-dir corruption), the correct action is to yield to a single instance and let
the coordinator resolve the double-spawn.

## Verified state at hand-off (independently confirmed by BOTH instances)
- Engine sidecar callsites WIRED and the tree BUILDS + LINKS clean
  (`RecordDrawProv`/`ProvNotePassOpen`/`RB3DebugGetDrawProv` symbols present in the
  compiled object; `rb3-native` links).
- **G1 GREEN** (I ran it): `drawlog-golden.py --fixed-clock --canonical-order
  --scene splash_screen` → PASS, 792 draws, with prov compiled-in but OFF (only the
  documented CharEyes world-jitter residual, within bound). Fail-red control
  (`--fail-red-audit`) → 4 drift classes RED + order-permutation GREEN, proving the
  comparator sees serialization drift. Evidence: `evidence/G1-golden-prov-off.txt`,
  `evidence/G1-failred-audit.txt`.
- **M1 rect go/no-go = GO** (both instances): `provAvailable:true`,
  `ml_highlight_glasstopp.mesh` present (matColor [0.82,0.76,0.02,0.08] — additive
  yellow highlight, α=0.08), `highlight_yellow.mesh` ABSENT ("0 draws for free"),
  degenerate (rectKind=2) fraction 0.0% (<20%). Evidence:
  `evidence/m1_probe/{m1_summary.json,drawlog_prov.json,song_select.png}`.

## Known rect-fidelity caveat (carry into M2/M3, matches plan Risk #2)
- The 61 exact-vert (rectKind=0) draws are all RndText GLYPH meshes (precise rects,
  e.g. [828,246,58,27]).
- Every NAMED UI quad (bg.mesh, header_list_bg.mesh, ml_highlight_glasstopp.mesh,
  highlight_main.mesh) falls to rectKind=1 sphere-fallback because static RB3 meshes
  retain NO CPU verts past GPU upload (`Verts().empty()`), and their loose local
  sphere still projects to a near-full-viewport box ([0,0,1046..1280,720]) even after
  the center-anchored refinement. This limits ROI discrimination for named UI quads;
  G4's "last writer" query must lean on submission order + pass state, not tight
  rects, exactly as the plan's ROI design intends. The glyph-precise rects + the
  game-side scope stack (M2) are how text/labels get attributed.

## Requested coordinator action
Reap one of the two Lane U instances and let a single instance carry M2→M4. All
evidence above is on disk and additive; no rework needed regardless of which
instance continues.
