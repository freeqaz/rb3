# rb3-viewer v2 + hair follow-ups — plan (2026-07-02, phase 2)

Continuation of PLAN.md after `81f38f3a` (CharHair CFG fix) + `5b8e0d05`
(viewer v1) landed. Read first: laneV-viewer.md ("Important note on --sim
visual result"), land-report.md §3 + Follow-ups, scout-dc3-viewer.md §5 + §7,
scout-wig-bug.md §4 (H2/H4).

## What v1 cannot do yet

The viewer draws the strand mesh **un-skinned**: `--sim N` runs
`CharHair::Poll()` and moves `bone_hair_*` transforms, but the rendered
`*_resource.mesh` stays at rest pose — no skinning pass runs, so hair physics
(and most char-anim bugs) are invisible in the viewer. That defeats the tool's
purpose for this bug class. v2 = make bone motion visible, plus the
scout-ranked features (--draw-dir, --pose-dump, --test-bone).

## Lane S — viewer v2: visible skinning + inspection features (Opus, sequential — owns native build dir + rb3_viewer.cpp)

### S1. Skinned draw (the core)

Leads (verify, don't assume):
- The hair milo CONTAINS its ~30 `bone_hair_*` Trans objects (scout-rb3-infra
  §4.1) — CharHair::Poll moves those. The unresolved `bone_hair.mesh` NOTIFYs
  are bone *meshes* (visual bone geometry), likely irrelevant.
- `NOTIFY: Skinned mesh needs to be re-exported: <hair>.mesh` fires from
  `src/system/rndobj/Mesh.cpp:903` on every hair load — read that branch: does
  it drop/clear the mesh's bone list (falling back to rigid draw)? That would
  explain rest-pose rendering. Compare how the same asset gets skinned
  IN-GAME (it does — the collapsed wig was visible in-game), i.e. what the
  BandCharacter/CharCache path does differently at load (e.g. bones re-bound
  post-merge, PostLoad ordering, or the mesh's bones resolve against the
  merged skeleton rather than local Trans).
- Engine `BandRnd::DrawMesh` (milo-native-engine src/platform/Rnd_Wgpu_RB3.cpp)
  already implements the skinned path in-game — find what condition it keys on
  (mesh->Bones() non-empty with live RndTransformable*?) and make the viewer
  satisfy it. DC3's viewer alternative: pose meshes on the CPU via
  `CharBonesMeshes` + `PoseMeshes()` (scout-dc3-viewer §5 "DirectPose") —
  acceptable fallback if the DrawMesh skin path can't be satisfied standalone.
- If an engine-side change is needed, commit engine-first, bump
  MILO_ENGINE_PIN in a matching rb3 commit (CLAUDE.md rule). Prefer rb3-side
  solutions; the engine draw path is shared with DC3 — do not disturb it.

Acceptance S1: `--viewer male_hair_crazyhawk_resource.milo_xbox --out a.png`
vs `... --sim 30 --out b.png` produce VISIBLY DIFFERENT PNGs; b shows strands
settled under gravity while keeping the hawk-fan shape (post-CFG-fix physics).
Save both to /tmp/rb3-viewer-v2/. A `--sim 1` vs `--sim 120` progression that
converges (no explosion) is a bonus check.

### S2. Inspection features (scout-dc3-viewer §4/§7 patterns)

- `--draw-dir`: draw via the dir's own `DrawShowing()` (draw-order /
  transparency parity) instead of the mesh walk; default stays mesh-walk.
- `--pose-dump out.json`: every RndTransformable's local+world xfm as JSON
  (DC3 ViewerPoseDump pattern), `--pose-dump-bones csv` filter. Dump AFTER
  --sim steps so it captures simulated pose (this becomes the numeric A/B tool
  the wig saga lacked).
- `--test-bone <name> <deg> [x|y|z]`: rotate one bone from rest before draw.
- Update VIEWER.md + render-asset.py passthrough. Keep RB3_RENDER_MESH mode
  and all v1 flags byte-identical in behavior.

Commit (own files only): rb3_viewer.cpp, possibly rb3_render_mesh.{cpp,h},
VIEWER.md, render-asset.py, + engine commit/pin if truly needed. Handoff doc
laneS-viewer-v2.md (what condition gates skinning, what was changed, evidence
PNGs).

## Lane P — H2 collide-hookup probe + in-game skull-clip check (Opus, runs AFTER Lane S — shares native build dir)

1. Env-gated probe (`RB3_HAIR_DBG=1`, HX_NATIVE-gated, zero Wii impact) in
   `CharHair::Hookup(collides)` (src/system/char/CharHair.cpp:758): per strand
   log `hair-name strand-i points-with-collides/points hookup-flags`, plus a
   one-line summary per CharHair. Also log total CharCollide count reachable
   from Dir() at hookup time.
2. Rebuild rb3-native; run band-closeup (RB3_HAIR_DBG=1, re-roll until a
   long-hair lineup — crazyhawk/ziggymullet/long* in HEADMAT) + viewer --sim.
   Questions to answer: (a) do flagged strands get nonzero collides in-game on
   native? (b) does post-fix long hair visibly clip through skull/shoulders?
   Capture closeups to /tmp/hair-h2/.
3. H4 evidence: put a post-fix native closeup of a long-hair member next to a
   Dolphin ground-truth shot (docs/native/c8-ground-truth-2026-07-01/
   dolphin-shots/ if it has hair; else images/retail-screenshots/) and note
   color/brightness delta for the Fable review. No fix — evidence only.
4. Verify `SimulateInternal__8CharHairFf` objdiff unchanged (99.6%) with the
   probe in (probe must be #ifdef HX_NATIVE — confirm with run_objdiff).
5. Commit the probe only if it's clean + useful (env-gated, silent by
   default); otherwise report-only. Handoff doc laneP-hair-verify.md with the
   coverage numbers + verdict on H2 (real gap vs fine) + H4 notes.

## Sequencing

Strictly sequential (S then P): both rebuild native/build-native and P's
in-game runs must include S's skinning work anyway. Fable (orchestrator)
reviews all evidence PNGs at the end.

Standing rules: stage only own files; no stash/revert/checkout; Wii builds
only via tools/ninja-locked (run_objdiff handles it); no Co-Authored-By;
engine changes commit engine-first + pin bump.
