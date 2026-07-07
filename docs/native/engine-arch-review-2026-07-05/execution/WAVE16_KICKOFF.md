# Wave 16 — Kickoff Design (coordinator draft, for Fable review before dispatch)

**Author:** coordinator. **Status:** REVIEWED (Fable, `WAVE16_REVIEW.md` rb3 `d122c830`) — **all 7
amendments adopted**; dispatched with the corrected shape below.
Parent: `execution/README.md` (Wave 15 results + Wave 16 menu). Engine pin `84ccb9e`. Ten
defaults ON.

## COORDINATOR ACCEPTANCE (2026-07-07) — final dispatched shape

- **A1 (CRITICAL, Lane T) — the "RndText glyph shader ignores font-material color" premise is
  REFUTED at source:** `standard_wgsl.inc:764` multiplies material.color into EVERY draw (text
  included), `RB3MaterialBinder.cpp:132-134` binds `mat->GetColor()`, and the shipped W4.2 hub
  result proves dark fontMat colors reach pixels on this exact path. The drop is LIST-PATH
  SPECIFIC; prime suspect (source-grounded): `UILabel::DrawShowing` propagates `mColorOverride`
  only to the MAIN font's material (`UILabel.cpp:266-270`) while the ALT font's mat always gets
  `GetStateColor` (`:279-292`), and RndText assigns per-font materials to submeshes
  (`Text.cpp:1402`) — exactly reproducing "line 269 fires dark, pixels stay 167". Lane T =
  observation-first (existing `RB3_UI_FLOOR_DBG` probe: which submesh/material renders the
  focused-row glyphs), grant EXTENDED to rb3 `src/system/ui/`; the fix may be entirely rb3-side.
- **A2 (Lane T / R-C):** no new multiply is introduced by any candidate fix → the global-tint
  worry dissolves; skip "!= white" scoping. Keep the screen-sweep no-regression net anyway.
- **A3 (Lane F / R-A) — the cell is CONFIRMED distinct and never-measured:** today's default
  repoints (`:1756-1757`) AND unconditionally overwrites offsets (`:1775`); SHELL_FIX baked
  against bound-rest. (authored offsets, own bone) has never run. Dispatch stands.
- **A4 (Lane F, BINDING) — "pristine authored offsets" has a real mutation window:** engine
  SKEL_REBAKE (`Rnd_Wgpu_RB3.cpp:3545`, `SetBone(b,bt,true)`) excludes finger bones but NOT
  `bone_?-hand`/wrist — arm W shows it didn't fire there, but the lane must VERIFY offset
  provenance at its own runtime (probe pre/post), not inherit. Pass-A semantics (own==bound
  cases, clipPlaying misses) must be handled explicitly and documented.
- **A5 (Lane F):** Tier-1 xcheck has no perturb fail-red — the flag-OFF arm IS Tier-1's red
  control (87.3° signature must reproduce flag-OFF in the same session as the flag-ON reading).
- **A6 (Lane F, BINDING) — female gate pinned:** 28.9° is the FAILURE signature, not a pass
  bound. Female PASS = count(>5°)==0, same as male. Before any Dolphin fallback, extend
  `evidence/offset_basis_derivation.py` (male-only today) with the female axis offline.
- **A7 (housekeeping):** fix the inverted bound/own comment block at `BandCharacter.cpp:1558-1572`
  in Lane F's commit (it sits exactly where the lane edits).

---

_(Original draft below, retained for provenance; superseded where the acceptance above differs.)_

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
