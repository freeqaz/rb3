# Wave 13 — Kickoff Design (coordinator draft, for Fable review before dispatch)

**Author:** coordinator. **Status:** DRAFT — under Fable pre-dispatch review, not yet dispatched.
Parent: `execution/README.md` (Wave 12 results + Wave 13 menu). Engine pin `44716f4`.
Note: the W4.3-C34 re-run (flipped hold-labels + ticker) is in flight as a side agent; its verdict
is folded into the acceptance section at dispatch time.

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
