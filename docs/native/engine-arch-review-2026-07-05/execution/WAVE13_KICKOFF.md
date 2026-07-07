# Wave 13 — Kickoff Design (coordinator draft, for Fable review before dispatch)

**Author:** coordinator. **Status:** REVIEWED (Fable, `WAVE13_REVIEW.md` rb3 `b2eaf12b`) — **all 10
amendments adopted**; dispatched with the corrected shape below.
Parent: `execution/README.md` (Wave 12 results + Wave 13 menu). Engine pin `44716f4`.

## COORDINATOR ACCEPTANCE (2026-07-07) — final dispatched shape

Fable review: **dispatch-with-amendments** (10). Adopted in full:

- **A1 (CRITICAL, Lane S) — the kickoff's fix statement was internally inconsistent:**
  `skeleton_unshared.milo` is the MALE-BIND skeleton (`BandCharacter.cpp:3934`); the authored
  129.9°/119.3° rests are the outfit GENDER binds. The in-source 2026-06-06 investigation block
  (`BandCharacter.cpp:3932-3935`) names the REAL two-half fix: **un-share AND gender-pose**. S1
  must answer three existence questions before any seam is picked: (i) do the finger bones even
  exist in the per-member skeleton instance? (ii) does the clip driver follow the un-shared
  instance or freeze it? (iii) does SetDeformation/gender-pose reach it before rest capture?
  STOP-TRIPWIRE extended: "resolves-but-doesn't-ANIMATE" is a failure, not a fix.
- **A2 (HIGH, Lane S) — crowd protection made concrete:** the source itself warns the un-share
  "would also touch the crowd" (`:3933`). Fix must be BAND-SIDE SCOPED (no global Dir.cpp/
  DirLoader changes); named crowd gates replace the vague "clamp byte-identical": W2.1 crowd
  placement oracle GREEN both arms, `RB3_NO_CROWD_REBIND` 24× shard-drop fail-red reproduction,
  per-dir guard-DROP census unchanged.
- **A3 (HIGH, Lane S) — name the actual offset writer:** W2.8e's rule-7 note says `hands_naked`
  is rebound via a path OTHER than `RebindHeadHandsAtRest`; S1 must name the final writer of its
  offsets (GeomOwner propagation `:1438-1448`, engine SKEL_REBAKE ~:3330 candidates) or the fix
  silently no-ops on the real path.
- **A4 (MEDIUM, Lane S gates):** wext ≤60u stands; ADD the ungameable mechanism gate: per-member
  **invOff provenance** — post-fix the offsets are no longer identical 106° across members and
  equal each member's gender-bind rest.
- **A5 (HIGH, Lane G) — shape decided by source:** menus are Tier-1 (UI interleaved into the
  graded intermediate, composite `venueGrade=false`); gameplay ALREADY draws UI post-grade via the
  Tier-2 mid-frame flush. Shape (a) = **generalize the existing `FlushPostProcMidFrame` to the
  menu venue→UI boundary** (machinery exists, faithful); shape (b) is unimplementable as stated
  (no per-draw identity at composite time). **TRAP:** the flush hardcodes `venueGrade=true` and
  chroma-preserve (default-ON) gates on `venueGrade>0.5` → unparameterized reuse regresses the
  authored B+W menu look, INVISIBLE to drawlog. Parameterize venueGrade at the flush + pin a B+W
  menu-backdrop ROI gate.
- **A6 (MEDIUM, Lane G):** range grant corrected: `Rnd_Wgpu_RB3.cpp:1973-2010` (EndFrame starts
  :1973) + `RB3PostProc.{h,cpp}` + `rb3_postproc.wgsl.inc`. The TRIGGER site (menu venue→UI
  boundary) may be game-side OUTSIDE the grant — S1 declares it and the coordinator signs off
  before S2 edits. First S1 question: why don't menus flush today?
- **A7 (MEDIUM, Lane G gates):** hub PP_OFF reaches only 2.20 (10% margin over 2.0); song-select +
  partdiff have NO baselines — S1 captures default + PP_OFF per screen FIRST and pre-registers
  PP_OFF-parity as the gate where absolute 2.0 is unreachable. Run all arms with
  `RB3_HUB_TEXT_CONTRAST` OFF (it WORSENED the metric 1.95→1.81; orthogonal, do not co-flip).
- **A8 (MEDIUM, Lane C):** the C34 re-run verdict does not exist yet — C3 starts from the
  committed C34 PLAN (or the side-agent STATUS if landed by then; coordinator folds it into the
  lane prompt at dispatch). The `~:5040-5090` grant reference was STALE (that region is the band
  shard guard today; obj.world composition = `:3241-3273`) — any C3 escalation re-derives ranges
  BY SYMBOL, never inherits line numbers.
- **A9 (LOW):** if C3 escalates into `Rnd_Wgpu_RB3.cpp`, it is SEQUENCED AFTER Lane G completes
  (single-writer-per-TU is campaign law; "disjoint ranges" is not the standard).
- **A10 (LOW, C2a):** pre-register the z/compositing guard: draw-order evidence + grid-glyph ROI
  intact + a sub-panel content census (the details milo may carry its own natively-opaque
  playnow.lsw-style quads — hide-list them, don't blanket-show).
- **R-A answered:** post-load rebind to the unshared STATIC bone is exactly the dead 5th class —
  NOT an alternative seam; the invOff-identity evidence pins the magnet at offset-BAKE time,
  with the honest caveat that 106°-identity alone is also consistent with per-member instances of
  the same male-bind file — which is why A1's gender-pose question is load-bearing.
- **R-D answered:** cheapest global net = gameplay-frame pixel-invariance (gameplay already runs
  the post-grade path, so Lane G done right is a no-op there) + the A5 B+W menu ROI gate.

---

_(Original draft below, retained for provenance; superseded where the acceptance above differs.)_

## Where we are (entering Wave 13)

Wave 12's two "failures" each named a concrete, in-reach fix:

1. **Hands root fix named and re-laned (W2.8g B.S2):** the 87° basis gap is IRREDUCIBLE with any
   single live bone — six composition cells now measured dead. The defect is upstream of the
   renderer: the ANIMATING bone that `Find(name)` resolves for outfit/appendage meshes is the
   SHARED MAGNET (invOff identical 106° across members with distinct 38/40-bone skeletons), while
   the member's authored 129° rest basis lives only on a STATIC per-member bone. Fix = **skeleton
   instancing / bone resolution**: make the mesh bind to the member's own ANIMATING bone carrying
   `skeleton_unshared.milo`'s authored rest. Renderer untouched; gates already built.
2. **Focused-text root fix named (W4.3-C1):** the authored focus color IS dark and reaches the
   shader; the pale-on-gold wash is the **postproc grade lifting UI glyphs** (+ semi-transparent AA
   text over the bright bar). PP_OFF passes the pre-registered contrast gate (2.20; retail 4.17;
   default 1.95 FAIL). Fix = UI drawn/composited AFTER the grade (or grade-exempt UI path) —
   requires the coordinator's declared-range grant into `Rnd_Wgpu_RB3.cpp`/`RB3PostProc.*`, which
   this kickoff GRANTS to Lane G below.
3. **UI parity residuals:** C2a panel background (lives in `song_select_details.milo`, not
   compositing natively), C2b album-art-over-header + C4 ticker overlap (both SYS-5
   Y-anchor/panel-origin layout family), C3 flipped hold-labels (C34 re-run pending).

Deferred consciously: loader-determinism sufficient fix (staged design exists in
`W0.3d-b/STATUS.md`; partial reducer landed opt-in; low user-visibility), WHITE real-lever,
4→8 lights, W2.4.

## Proposed Wave 13 lanes

**Lane S — SKEL: per-member bone resolution for appendage meshes (Opus; char/loader side —
`BandCharacter.cpp`, skeleton merge/Find paths; engine renderer READ-ONLY):**
- **S1 (mechanism study):** map today's resolution chain for `hands_naked.mesh` bones: which
  `Find(name)` (or `BoneTransAt`/rebind) produces the magnet-owned animating bone; where the
  per-member `skeleton_unshared.milo` bone with the authored 129° rest lives at that moment; and
  what the existing default-ON rebind machinery (`RebindHeadHandsAtRest`,
  `RebindOutfitBonesToOwnSkeleton` — the LOAD-BEARING crowd path, W2.3) already does in this chain.
  Deliverable: the exact seam where resolution can return the member's own animating bone, with an
  interaction analysis against both default-ON rebinds (do NOT regress them).
- **S2 (fix, flag-first default-OFF):** implement the resolution fix. GATES (pre-registered, all
  instruments already landed): rest-free Instrument-B invariants stay ~0 (orthoResid ≤0.001,
  isoDistort ≤0.01); Tier-2 joint-attachment GREEN (≤1u); wext on the A.S3 sighting protocol
  95-106u → ≤60u WITHOUT freezing (worldExt varies and tracks pose); guard-DROP census unchanged;
  crowd clamp/spread byte-identical (the W2.3 load-bearing path); RealPathFixture + skin-golden
  gtests; lineup-gate PASS; flag-OFF drawlog 792 byte-identical. Before/after band screenshots for
  coordinator E1.
- **STOP-TRIPWIRE:** all SIX composition/bake cells are dead (incl. Wave-12's
  own-live+bound-rest). If S1's seam degenerates into re-baking offsets against any static basis,
  STOP → report BLOCKED. The fix class here is "which BONE animates", not "which OFFSET is baked".

**Lane G — C1-grant: grade-exempt UI compositing (Opus; **COORDINATOR RANGE GRANT**: the
end-of-frame composite/postproc call region of `Rnd_Wgpu_RB3.cpp` (~:1990-2010, verify + declare
exact lines in PLAN.md) + `RB3PostProc.{h,cpp}` + `gfx/Shaders/rb3_postproc.wgsl.inc`; disjoint
from Lane S which is renderer-read-only):**
- **S1 (design + flag-first):** make UI/overlay content composite AFTER (or exempt from) the
  postproc grade, `RB3_UI_POST_GRADE` default-OFF. Candidate shapes (planner picks with evidence):
  (a) split the frame into scene-pass → grade → UI-pass render order; (b) tag UI draws and have
  the grade shader pass them through; (c) opaque text rendering over the highlight bar. Constraint:
  venue/gameplay visuals byte-identical when no UI is graded differently — the venue grade path
  (chroma-preserve, venueGrade uniform) must not change.
- **S2 (verify):** GATES: the A11 percentile contrast gate (p60/p5 ≥2.0) PASSES on hub focused
  item + song-select highlighted row + partdiff GUITAR (retail calibration 4.17; current default
  1.95 = fail-red); Wave-7-rescued labels legible; venue wash arms unchanged (wash_score on the
  pinned shot, ON vs OFF within noise); drawlog flag-OFF 792 byte-identical; DC3 zero-blast
  (milo-engine-tests incl. Dawn WGSL gtest); before/after captures for coordinator E1.

**Lane C — W4.3 residuals (Sonnet impl, Opus if diagnosis deepens; game-side only, fenced OUT of
Lane G's files):**
- **C2a:** walk the `song_select_details.milo` subdir (ObjDirItr) → is the panel backing
  (difficulty_bg*/raitings_bg/details_background) never-submitted or submitted-and-dropped?
  Game-side fix flag-first (show/attach the sub-panel where the 360-ARK expects it).
- **C2b + C4 (one family):** SYS-5 Y-anchor/panel-origin offsets — authored milo xfm vs drawn
  rect for the album-art panel (header overlap) and the hub ticker label/body stacking. Fix
  game-side per-panel (MainHubPanel.cpp:130 precedent) flag-first.
- **C3:** per the C34 side-agent verdict (folded at acceptance): game-side fix if the negative-Y
  scale is game-side; if it needs the DrawMesh xfm region, the coordinator grants ~:5040-5090 to
  Lane C explicitly at acceptance time (Lane G's region is the composite call site, disjoint —
  verify at dispatch).
- Gate: before/after captures vs retail refs for E1; drawlog 792 flag-OFF unchanged; settle-frame
  pinning.

## Process rules (carried)

Locks, checkpoints (`/tmp/wave13-checkpoints/<stage>.json`, check-first/write-before-return),
commit-per-review-cycle, PLAN.md/STATUS.md per item, append-only classification.json + single
coordinator regen, own build dirs, NO pin bumps/default flips by lanes. Six defaults ON; all
refuted flags UNSET (now incl. `RB3_HANDS_SHELL_FIX`, `RB3_LOAD_DETERMINISM` stays opt-in).
**NEW (Wave-12 lesson): process cleanup by pgid ONLY — never `pkill -f rb3-native`** (suspected
cause of the C34 stall; concurrent agents share the process namespace).

## Risks / open questions for the reviewer

- **R-A (Lane S):** the resolution seam sits next to TWO default-ON load-bearing rebinds. Is the
  interaction analysis in S1 sufficient protection, or does S2 need an explicit
  crowd-regression gate beyond byte-identical clamp (e.g. the crowd-bone gate + placement oracle
  re-run)? Also: is "make Find return the member's own bone" even the right seam vs "rebind the
  outfit mesh to the unshared skeleton bone post-load" (closer to the existing
  RebindOutfitBonesToOwnSkeleton pattern — W2.2's `RebindHeadHandsAtRest` already does a rest
  CAPTURE; why doesn't it already fix this, and does that answer change the seam)?
- **R-B (Lane G):** which candidate shape is actually implementable without breaking the
  venue-grade path? Verify against the real composite order in source (mid-frame venue flush +
  end-of-frame composite) — is UI already in a separate submission that just needs reordering, or
  is it interleaved with scene draws in one pass (making (b) tagging the only option)?
- **R-C (Lane C):** C2a's "show the sub-panel" could double-draw or z-fight if the 360-ARK expects
  a different compositing order — what's the guard? And is the C2b/C4 "same family" premise
  actually verified, or two coincidentally-similar symptoms?
- **R-D:** Lane G changes what EVERY screen looks like post-grade if done wrong — what's the
  cheapest global no-regression net beyond drawlog (e.g. a small screen-sweep SSIM check vs
  pre-change captures on non-UI screens)?
