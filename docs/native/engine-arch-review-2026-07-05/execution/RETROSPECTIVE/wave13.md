# Wave 13 — Retrospective (hindsight review)

**Reviewer:** retro subagent (read-only). **Run:** `wf_6092bda5-74c`, 6 agents + 2 side agents.
Engine pin `44716f4` → `3b5af48` (regen 354). Sources: `WAVE13_KICKOFF.md`, `WAVE13_REVIEW.md`,
README Wave 12/13/14/15 results, `SKEL/STATUS.md`, `UIGRADE/STATUS.md`, `HANDS-ADJUDICATION/VERDICT.md`.

## What Wave 13 set out to do

Three lanes, all named as "in-reach fixes" from Wave 12's two measured failures:

- **Lane S (SKEL, Opus):** per-member skeleton instancing — make the animating `Find(name)`
  resolution return the member's own bone (authored 129° rest) instead of the "shared magnet."
  The named root fix for the hands/finger shard family; renderer read-only.
- **Lane G (UIGRADE, Opus, coordinator range grant):** draw/composite UI AFTER the postproc grade
  so focused menu text isn't washed pale-on-gold. `RB3_UI_POST_GRADE` flag-first.
- **Lane C (Sonnet):** song_select panel-bg (C2a), SYS-5 Y-anchor layout family (C2b art / C4
  ticker), flipped hold-labels (C3).

The pre-dispatch Fable review (10 amendments, all adopted) was unusually strong and caught real
errors before dispatch: it flagged that the kickoff's Lane-S fix statement conflated two rest bases
(A1), that Lane-G shape (b) was unimplementable and shape (a) carried a hidden `venueGrade`/
chroma-preserve trap (A5), and that Lane C's `~:5040-5090` grant range was stale (A8). The gate
did its job.

## What shipped / flipped (durable)

- **C4 hub ticker `RB3_HUB_TICKER_YFIX` → default-ON** (rb3; E1 PASS, retail-matching stacking,
  drawlog-792 clean). Still a default today. A clean, real win.
- **UIGRADE machinery landed** (`RB3_UI_POST_GRADE` + `FlushPostProcMidFrame` venueGrade
  parameterization via a menu-flush latch, engine `f677871`; trigger wired through the public
  `ClearDepthForOverlay()` seam, engine `a5cf8d3`, rb3 `82aa81c7`). Flag stayed **default-OFF /
  E1 HELD** — correctly, because song_select flag-ON showed a visible red-band artifact.
- **C2a closed as an asset difference, not a bug** (360-ARK `song_select.milo` has no quick-view
  sidebar backing; the `difficulty_bg*/raitings_bg` quads belong to the drill-in page). Honest
  not-a-bug.
- **S.S1 premise inversion (the single most valuable thing this wave produced):** runtime pointer
  probes overturned the Wave-9→12 framing. `own = Find(name)` is PER-MEMBER, ANIMATES (4 distinct
  ptrs, moves 186-276u/Poll) and IS gender-posed; `bound = BoneTransAt` is the SHARED static bind.
  The old "shared magnet animates" reading was the dual-skin probe sampling PRE-rebind state with
  **own/bound labelled backwards**. This also EVAPORATED the crowd-regression risk that the whole
  Lane-S charter (and Fable's A2) had been built to guard against. A ~4-wave false premise, caught
  here by one cheap pointer/identity dump.

## What Wave 13 got WRONG (hindsight from Waves 14–15)

Wave 13's two headline conclusions on the hands lane were both wrong, and it propagated a false
"exhausted" certificate from Wave 12:

1. **"Faithful fix = per-member RESKIN of verts+weights onto `own`" (SKEL S.S2 headline → Wave-14
   headline).** Wave 14 implemented it exactly per recipe and it was **REFUTED by its own gate**:
   wext REGRESSED (flag-OFF 74.8u → ON 87.7u → ON+no-rebake 136u). Root cause reframed as an
   ANIMATION-BASIS problem: with the default rebake, `skinPos(t) = v'·meshWorld·inv(own_rest)·own_live(t)`,
   so the live-vs-rest delta is IDENTICAL flag-ON/OFF **by construction** — a one-time vertex
   re-pose only moves verts to a larger radius and AMPLIFIES the smear. **This algebra was fully
   available at Wave 13** (S.S2 already wrote the palette formula `skin[b] = BoneOffsetAt(b)·own->WorldXfm()`
   and had the R·sinθ far-vert model). Wave 13 recommended reskin without running that substitution;
   the refutation was a paper exercise, not a build-and-measure exercise.

2. **"The offset-bake fix class is EXHAUSTED (6 dead cells + degenerate 7th)."** Wave 15's
   HANDS-ADJUDICATION found the Wave-12 "6th dead cell death certificate was CONFOUNDED" — the
   shared-B bake had killed female/gloves/nails, NOT the composition Wave 13 attributed the death
   to. Wave 15 then named the actual fix: **keep AUTHORED per-mesh offsets + repoint appendages to
   `own` (SetBone calcOffset=false — the existing torso pattern), NEVER MEASURED.** That is an
   offset-repoint cell, i.e. inside the class Wave 13 declared closed. Wave 13 accepted Wave 12's
   exhaustion claim wholesale and added its own "degenerate 7th" to it, hardening a false wall.

3. **"Seam B tears for everyone; per-bone own-vs-bound gaps are mixed-sign up to ~35°, irreducible."**
   This drove the "no offset bake can work → must reskin" logic. Wave 15's gender-split, arm-W
   baseline run showed **male authored offsets match `bound`'s basis EXACTLY (Tier-1 0.1°, all 38
   bones; palette Tier-1 3.1°, 0/1038 blocks >5°)** — the male hands were essentially fine. The
   defect was specifically female/gloves/nails (28.9° off). Wave 13's APD_DIAG measured `own` vs
   `bound` per bone and reported an aggregate "mixed-sign up to 35°" — but that is not the
   palette-relevant comparison (authored-offset vs `bound`), and it was not gender-split in the
   analysis that fed the conclusion, so it over-generalized "everyone tears" from a confounded metric.

4. **UIGRADE artifact mis-attribution.** Wave 13 held the flip (correct) but attributed the
   song_select red band to "the `ClearDepthForOverlay` depth-clear side effect." Wave 14 found the
   minimal flush-only shim STILL showed it — the real cause was `FlushPostProcMidFrame`'s OWN
   depth-clear-on-resume revealing a z-occluded SETLISTS quad, fixed with a `LoadOp::Load` on the
   menu flush re-open. The HOLD was right; the mechanism named was wrong, and the wrong mechanism
   was carried into the Wave-14 menu ("clean flush-only seam, no depth-clear") which then had to
   re-diagnose.

## Premise failures — entering, created, and how they were eventually caught

- **CAUGHT by this wave (credit):** own/bound backwards-labelling (Waves 9-12). Killed by S.S1's
  runtime pointer dump. This is the wave's best moment and shows the fix: cheap identity probes
  beat elaborate symptom metrics.
- **ENTERED and NOT caught:** Wave-12's "6 dead cells, offset-bake class exhausted." Wave 13
  accepted it and built on it. Caught two waves later (Wave 15) only when someone re-ran the death
  certificate gender-split and found it confounded.
- **CREATED by this wave:** (a) "reskin is the faithful fix" — refuted Wave 14 by algebra that was
  available Wave 13; (b) "the residual tears for everyone / irreducible up to 35°" — refuted
  Wave 15 by gender-split; (c) UIGRADE red band = ClearDepthForOverlay — refuted Wave 14.

The through-line: Wave 13 correctly inverted ONE bad premise (own/bound) with a direct probe, then
immediately manufactured three more by reasoning from confounded/aggregate metrics (own-vs-bound
gaps, the inherited exhaustion claim) instead of continuing to probe directly.

## Tooling gaps (the load-bearing section)

Every wrong conclusion above traces to a missing measurement that arrived one-to-two waves too
late. Named concretely:

1. **Reference-emulator single-bone ground-truth capture (Dolphin + milo-trace).** Wave 15 named
   this as the "fallback ground truth" and its gender-split arm-W run produced the NUMERIC CLOSURE
   (`angle(B·inv(R)) = 87.2°` = the authored-bind-vs-SetDeformation-seed rotation) that finally
   pinned the fix. **One run of "dump each hand bone's authored bind and runtime pose from the real
   Wii binary" answers the entire hands question** — is the runtime bone the authored basis, and by
   how much does it differ, per bone, per gender. The campaign had no such capture through 14 waves
   and instead measured ~7 offset-bake cells on native-only surrogates. Wave 13 specifically would
   have had "reskin amplifies, offset-repoint-to-own with authored offsets closes it" handed to it.
   Cost of the absence: the reskin dead-end (Wave 14, ~2 agent stages + engine impl) and the
   confounded-exhaustion propagation.

2. **A skinning-pipeline algebra/symbolic checker.** Given a candidate transform edit, symbolically
   evaluate whether it changes `skinPos(t)`'s live-vs-rest delta. Wave 14's refutation of reskin was
   pure substitution into a formula Wave 13 already had written down. A tiny "does this candidate
   move the delta or just the radius?" evaluator would have refuted RESKIN on paper at Wave 13 and
   kept it off the Wave-14 menu entirely. What the campaign did instead: named it the headline,
   implemented it, built it, measured it, refuted it.

3. **Metric-validation harness for `wext` (and dual-skin/Instrument-B).** No one validated that
   `wext > 60u` actually discriminates shard-vs-clean until Wave 15 declared it **NOT a hands-shard
   oracle** (legitimate two-hand extents reach 104u). Wave 13's S.S2 gate list led with "wext ≤60
   without freeze." A harness that fires each proposed oracle on known-GOOD and known-BAD reference
   frames and reports its separation would have retired wext before it gated ~10 waves of pass/fail
   decisions. This is the single most expensive missing tool in the saga: the primary quantitative
   gate was invalid the whole time.

4. **Gender-split-by-default in the appendage probes.** Wave 13's APD_DIAG had the data (its own
   table shows p0/p1 columns) but the conclusion aggregated across gender into "mixed-sign, tears
   for everyone." Wave 15's identical-shape run, reported gender-split, showed male=fine /
   female=broken and demolished the reskin premise. Making the probe REPORT the male/female split
   (and compare authored-offset-vs-bound, the palette-relevant pair, not own-vs-bound) is a
   reporting change, not a new instrument — it would have flipped the S.S2 conclusion in place.

5. **Depth/occlusion draw-inspector for the compositing lanes.** For UIGRADE, a tool that, given a
   pixel-diff ROI (the red band), reports which draw occupies it and its z-state would have named
   `FlushPostProcMidFrame`'s resume depth-clear directly. Instead Wave 13 guessed ClearDepthForOverlay
   and Wave 14 re-diagnosed (a side agent + a full lane stage).

## Wasted effort

**Moderate, and almost all downstream-triggered rather than spent in-wave.** Wave 13 itself was
disciplined: S.S2 honored its STOP-TRIPWIRE, changed no source, registered no fake flag on a
degenerate seam — that is exactly right and should be preserved as the model. The waste is that it
exported a high-confidence WRONG next-fix ("reskin is the faithful fix, Wave-14 headline") that
was analytically refutable from formulas already in the S.S2 STATUS. That directly cost Wave 14's
RESKIN R1+R2 (2 agents + an engine implementation and measurement) to reach a NEGATIVE result, and
it hardened the false "offset-bake class exhausted" wall that Wave 15 had to demolish before naming
the real (offset-repoint) fix. Net: ~2–3 agent-stages of downstream work that a single ground-truth
bone capture (tool #1) or the paper algebra check (tool #2) would have avoided. In-wave direct
waste: small.

## One-line verdict

Wave 13's process was good (strong pre-review, honest STOP-TRIPWIRE, real ticker flip, correct
UIGRADE hold) and its one direct probe (S.S1 own/bound inversion) was excellent — but it then
reasoned three new false premises out of confounded/aggregate metrics and an inherited exhaustion
claim, exporting the wrong headline to Wave 14. The fix was not more agent-reasoning; it was one
reference-emulator per-bone ground-truth capture, which the campaign didn't build until Wave 15.
