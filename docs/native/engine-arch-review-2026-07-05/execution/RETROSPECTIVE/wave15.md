# Wave 15 — Retrospective (written with Wave-16 hindsight)

**Run:** `wf_06c0e645-e15`, 3 agents. **Pin:** `fdf0ad9 → 84ccb9e` (regen 360 clean).
**Close-out commit:** `a5ed784b`. **Shape:** one deep synthesis lane (H) + two small
diagnostic/fix lanes (B=ROWFIX, N=gamertag). Reviewed pre-dispatch by Fable
(`WAVE15_REVIEW.md`, all 9 amendments adopted).

## Goals
1. **Lane H (HANDS-ADJUDICATION):** synthesis-only, NO fix — adjudicate the 7-artifact /
   3-premise-inversion hands saga and either derive the correct fix or declare the option
   set closed.
2. **Lane B (W4.4-ROWFIX):** song_select focused-row polarity (white-on-navy vs retail
   black-on-white); diagnose + fix flag-first.
3. **Lane N (W4.5-GAMERTAG):** the "(null)" gamertag revealed by Wave-14's art fix.

## What shipped
- **W4.5-GAMERTAG — FIXED + FLIPPED default-ON (`3fdf482b`).** Clean win. Strong native
  `PlatformMgr::GetName` override ports the Wii `PlatformMgr_Wii.cpp:489-496` "not signed
  in → localized Player N" fallback verbatim; one provider fixes header + overshell + all
  consumers. Coordinator E1 PASS both places → default-ON, opt-out
  `RB3_PLAYER_NAME_FALLBACK_OFF`. **Defaults now TEN.** Survives Wave 16 unrefuted.
- **W4.4-ROWFIX Part A — landed behind `RB3_ROWFIX` default-OFF (`51ae685a`).** Focused
  fill quad `ml_highlight_glasstopp` repainted solid opaque (fill p60 94→179). Correct and
  reusable. Flag correctly held OFF (fill-only-with-white-text would worsen legibility).
- **HANDS-ADJUDICATION VERDICT — disposition (a), proof-level (`834ecd2d`).** Named a
  never-measured fix cell (keep authored per-mesh offsets + repoint appendages to `own` via
  `SetBone(b,own,false)`), with numeric closure `angle(B·inv(R))=87.2°` and pre-registered
  §5 gates. Fed Wave-16 Lane F.

## What was measured dead / refuted / corrected THIS wave (Lane H's real value)
- **The z-occluded selection-quad prime suspect (Wave-15 acceptance A3) was REFUTED** at
  the renderer: both highlight meshes are `zmode=0` (no depth test) → a depth `LoadOp`
  cannot occlude them. The kickoff's imported "bar bleeds through AA text" hub-mechanism
  was already killed at pixel level in the review; ROWFIX then killed the depth suspect too.
- **Lane H caught a false premise CREATED by Wave 14:** the W2.8g "6th dead cell death
  certificate" (`RB3_HANDS_SHELL_FIX` regresses) was shown **CONFOUNDED** — the shared-B
  bake was correct only for male `hands_naked` (Tier-1 3.1°) and killed female (28.9°),
  gloves (60–69°), nails (~170°); the aggregate metric was dominated by the wrongly
  re-anchored meshes. The mechanism that caught it: **gender/mesh-split measurement (arm S)
  — the first time the saga split by gender.** Named as "the single biggest instrument
  lesson of this saga."
- **The 87.3° Tier-1 angle was re-attributed:** not a mysterious "magnet conjugation" but
  the authored-bind-vs-SetDeformation-seed relative rotation, living in the native rebake
  (a workaround), not the animation. Reproduced offline from committed matrices.

## What Wave 15 GOT WRONG (caught by Wave 16)
**W4.4-ROWFIX Part B — "the native RndText/glyph shader ignores the font-material color."**
This claim (STATUS §3B → README Wave-15 table → Wave-16 kickoff draft) is **REFUTED at
source** by Wave-16 review A1 (CRITICAL):
- `standard_wgsl.inc:764` multiplies `material.color.rgb` into EVERY draw incl. text;
  `RB3MaterialBinder.cpp:132-134` binds `mat->GetColor()` per draw.
- **A within-campaign contradiction the lane missed:** the campaign's OWN shipped, measured
  W4.2 result (`RB3_UI_TEXT_FLOOR_RELAXED`, default-ON) proved dark focus-state `fontMat`
  colors reach text pixels through this exact path on the hub. If the shader ignored
  font-material color, that flip could not exist.
- Real cause (source-visible, rb3-side): `UILabel::DrawShowing` propagates `mColorOverride`
  only to the MAIN font's material (`UILabel.cpp:266-270`); the ALT font's mat always gets
  `GetStateColor` (`:279-292`), and RndText assigns per-font materials to submeshes
  (`Text.cpp:1402`) — the darkened material isn't the one the visible glyphs draw with.

The lane mislabeled an **rb3-side list-label plumbing bug as a global engine shader gap**
and escalated it to a full engine wave item ("make the glyph shader honor material color,
engine text/glyph path + WGSL"). Wave 16 dispatched with that wrong framing in its draft;
it was corrected only by Fable's pre-dispatch source re-derivation, which then had to widen
the grant to rb3 `src/system/ui/` (engine/WGSL edits "may be ZERO").

**Why it happened — structural, not carelessness:** Lane B's grant was scoped to
`RB3PostProc.cpp` (the depth-occlusion suspect). When the diagnosis crossed into the
label-color route (`src/system/ui/`, engine text TUs), the agent could not test the
alt-font hypothesis, stopped at "`SetColor` fires (`UILabel.cpp:269`), pixel stays luma
167," and concluded "engine gap, outside my scope." A diagnosis lane was given a fix lane's
narrow grant.

## Premise failures (entering / created, and how caught)
- **Entering:** Wave-14's "vert/offset-bake class CLOSED + animation-basis reframe" and the
  W2.8g confounded death certificate. **Caught** by Lane H's gender-split arm-S run — the
  right instrument for the right lane.
- **Entering (Lane B):** the hub's "bright bar compositing through AA glyph alpha" mechanism
  imported onto song_select. **Caught** pre-dispatch by Fable's pixel re-measurement (native
  row is white text on ~15-luma navy fill, no bright bar) and again by ROWFIX (zmode=0).
- **Created by Wave 15:** the "engine shader ignores font-material color" mislabel. **Caught
  one wave later** by Wave-16 review source re-derivation. Would have been caught in-wave by
  (a) checking the claim against the shipped W4.2 result, or (b) a system/ui grant + one
  RB3_UI_FLOOR_DBG trace of the focused glyph's material.

## Tooling gaps (concrete)
1. **No "does any shipped flag contradict this diagnosis?" cross-check.** The ROWFIX
   "engine shader ignores font-material color" conclusion directly contradicts the
   campaign's own default-ON `RB3_UI_TEXT_FLOOR_RELAXED` (W4.2). A tiny index of *what each
   shipped default-ON flag PROVES* (one line each), consulted before escalating a diagnosis
   to a new engine item, answers "is this claim already falsified by something we shipped?"
   in one lookup. The campaign instead spent a full Fable review stage in Wave 16
   re-deriving it from `standard_wgsl.inc` + `RB3MaterialBinder.cpp` + `UILabel.cpp` +
   `Text.cpp`.
2. **Per-glyph-submesh material-color trace at the binder was available but unused for the
   route in question.** `RB3_UI_FLOOR_DBG` dumps mesh/mat/pre-floor RGB per UI-text draw;
   run on the focused-row label it would have shown whether the darkened color reached the
   binder for the *visible* glyph submesh (main vs alt font), naming the drop point in ONE
   run and distinguishing "shader ignores color" from "wrong material darkened." The lane
   used the probe for the fill quad but not to resolve the text-color drop, because the
   grant excluded `src/system/ui/`.
3. **Scalar-angle probes instead of matrix-relative-rotation probes.** The hands saga's
   instruments dumped angle-to-identity scalars (±6–35°); the true operative error is a
   matrix product `angle(B·inv(R))=87.2°`. "Scalars do not subtract" — several premise
   inversions across the saga trace to this. A probe that emits per-bone *relative rotation
   between the two candidate bases* (not each to identity) would have been decisive many
   waves earlier. Lane H had to reconstruct it offline (`offset_basis_derivation.py`).
4. **Gender/mesh-split measurement was not the default.** Aggregating male + female + gloves
   + nails masked male `hands_naked` coherence (3.1°) under female/glove/nail authored-basis
   errors and produced a confounded refutation (W2.8g). A harness that always splits by
   member gender + mesh population would have prevented ~1 dead bake attempt and the
   confound; the saga self-diagnoses this as "the single biggest instrument lesson."

## Wasted effort
**Small-to-moderate.** The ROWFIX Part-B mislabel cost one Wave-16 Fable review stage of
source re-derivation and delayed the ROWFIX flip by ~a wave; but Part A is solid/reusable
and nothing was thrown away — the correction is a grant-widening, not a rework. Lane H's
"synthesis-not-fix" was the *right* use of a wave (it caught a confound and closed the
vert/offset class with a proof), so its cost is not waste. The larger latent waste is
upstream of Wave 15: the ~6-wave, 7-artifact hands bake-hunt that gender-split measurement
(gap #4) and matrix-relative probes (gap #3) would have collapsed — Wave 15 is where that
finally surfaced, to its credit.

## Recurring bug families touched
- **Hands / skinning shard:** adjudicated (not fixed) — vert/offset-bake class CLOSED with a
  proof; a distinct never-measured cell named + handed to Wave-16 Lane F (which CONFIRMED it
  distinct). Still open pending the Wave-16 fix outcome.
- **UI text color / polarity:** ROWFIX Part A landed (flag OFF); Part B mis-diagnosed as an
  engine shader gap → corrected to an rb3 alt-font plumbing bug in Wave 16.
- **UI text / gamertag:** "(null)" FIXED + flipped (Player-N fallback). Clean.
- Wash / BOOTRNG: not touched this wave.
