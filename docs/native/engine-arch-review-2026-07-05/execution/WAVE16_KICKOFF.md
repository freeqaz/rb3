# Wave 16 — Kickoff Design (coordinator draft, for Fable review before dispatch)

**Author:** coordinator. **Status:** DRAFT — under Fable pre-dispatch review.
Parent: `execution/README.md` (Wave 15 results + Wave 16 menu). Engine pin `84ccb9e`. Ten
defaults ON.

## Where we are

Wave 15's adjudication (`HANDS-ADJUDICATION/VERDICT.md`) produced a proof-level derivation and a
never-measured fix cell with pre-registered gates (§5). The rowfix lane isolated a real engine
gap (RndText ignores font-material color) blocking the focused-text flip. Both are now
implementation waves with unusually well-specified briefs.

## Proposed Wave 16 lanes

**Lane F — HANDS-FIX (Opus; rb3 `BandCharacter.cpp` only):**
Implement the adjudicated cell: in `RebindHeadHandsAtRest`'s apdMesh scope, KEEP the authored
per-mesh offsets (no rebake) and REPOINT the appendage meshes to the per-member animating bone
(`SetBone(b, own, calcOffset=false)` — the torso pattern), flag-first default-OFF.
GATES = VERDICT §5 verbatim (the lane must re-read them from the VERDICT, they are BINDING):
gender-split everything (male AND female measured separately — the confound that killed prior
readings); Tier-1 palette xcheck count>5° = 0 on male; female per §5 (authored female offsets are
28.9° off `bound` — the VERDICT's female-specific gate applies); Tier-2 joint-attachment ≤1u;
the ceiling-hand/forearm E1 sightings GONE (the A.S3 protocol frames, my eyes at close-out);
drawlog 792 flag-OFF byte-identical; crowd oracles + guard-DROP census unchanged; gloves/nails
regression checks (the arm-S data showed those populations behave differently — measure them).
wext is DESCRIPTIVE ONLY (not a pass/fail oracle — legit two-hand extents reach 104u).
FALLBACK (pre-registered, §5): if the male visual gate misses, run the Dolphin + ../milo-trace
single-bone capture (bone_R-middlefinger03 / bone_R-hand WorldXfm at matched clip time vs native
`own`) before any redesign.

**Lane T — RndText font-material color (Opus; engine text/glyph path + WGSL):**
Make the native glyph shader honor font-material color so `UILabel::Draw`'s focus-state
`fontMat->SetColor(dark)` (UILabel.cpp:269) reaches pixels. Diagnose first: where the glyph
pipeline drops the material color (vertex color? uniform never bound? shader ignores it?) —
then fix flag-first default-OFF. MUST NOT regress: the W4.2 relaxed text floor (default-ON), the
Wave-7-rescued labels, hub post-grade contrast (≥2.0), and menu text everywhere (a text-color
change is global — the no-regression net is a screen sweep of hub/song_select/partdiff/overshell
captures both arms + drawlog 792 + DC3 zero-blast incl. Dawn WGSL gtest).
THEN (same lane, if the engine fix lands): enable ROWFIX Part B alongside Part A
(`RB3_ROWFIX`), re-run the directional two-region gate on the song_select focused row + partdiff
GUITAR, and deliver a READY_FOR_FLIP package (coordinator flips ROWFIX + the text-color flag
together after E1).

## Process rules (carried)

Locks, checkpoints (`/tmp/wave16-checkpoints/`), commit-per-review-cycle, PLAN/STATUS per item,
append-only classjson + coordinator regen, own build dirs, no flips/pin bumps by lanes, refuted
flags UNSET, TEN defaults stay ON, pgid-only cleanup, frame-count settling.

## Risks / open questions for the reviewer

- **R-A (Lane F):** the adjudication says the prior "torso pattern" repoint is exactly what the
  DEFAULT path already does for hands PLUS a rebake — verify from BandCharacter.cpp what
  `calcOffset=false` + no-rebake concretely changes vs today's default (which offsets survive?),
  and that the cell is actually distinct from the 7 dead artifacts (especially the shipped
  default and RB3_HANDS_SHELL_FIX). The VERDICT claims it is — re-derive, don't trust.
- **R-B (Lane F):** female gate — the authored female offsets are 28.9° off `bound`; under the
  new cell they pair with female `own`. Does the VERDICT's arm-W data actually predict the female
  outcome, or is female-specific residual expected (and what gate value)?
- **R-C (Lane T):** is honoring font-material color safe for ALL text (fonts may rely on white
  multiply today — a global change could tint every label)? Should the fix be scoped (multiply
  by material color only when != white, or only for UILabel-driven materials)?
- **R-D:** lane collision — Lane T touches the engine text/mesh path; Lane F is rb3-only. Verify
  empty matrix.
