# Wave 17 — CLOSE-OUT Review (Fable adversarial pass over RESULTS)

**Reviewer:** Fable subagent (2026-07-08), first run of the R6 standing close-out protocol
(`KICKOFF_TEMPLATE.md` §Close-out, rb3 `a55c53b4`). **Input:** all five lane hubs
(`R1-DOLPHIN`, `R2-FIXTURES`, `R3-UIDUMP`, `R4-DETERMINISM`, `R5-HANDS-ENDGAME`) + committed
evidence, `WAVE17_KICKOFF.md`/`WAVE17_REVIEW.md`, the close-out commits (rb3 `65c5092e` merge,
`2c5b30cf` pin bump + ledger regen; engine `3b08a88` regen 372), the ten OPTIONS §4 lints as
rubric, and direct re-verification: the R5 re-derivation `.out` re-read against its asserts,
good-body screenshot md5s, engine classjson rows at `3b08a88`, `git ls-files` vs the live tree,
`PLAN-R4` §M3 / `PLAN-R5` §2.4 against the verdicts that claim to apply them.

**Overall verdict: the wave's substance survives adversarial review.** All five lanes delivered
what they graded themselves as delivering, the R5 dissolution of the D4 headline HOLDS
(independently re-checked, §Q2 below), and the close-out mechanics (single pin bump, gates
re-run, collision handled fix-forward) were executed correctly. The findings below are almost
all **record-consistency debts**: the wave produced a major standing correction (R5 §8) and the
wave-end regen then re-emitted documents that still carry the corrected-away claims. Fix the
record before Wave 18 reads it.

---

## Findings (ranked by severity)

### F1 (HIGH — stale record; the regen re-emitted corrected-away claims)

**Claim under review:** close-out step 2 — "ROADMAP/plan docs amended where the wave changed
them; single classjson regen." The regen ran, but the wave's load-bearing correction (R5
VERDICT §8, `fd2f83ac`) was not propagated into the three standing documents that contradict
it, and two of them were re-emitted verbatim by the regen itself:

- `execution/R1-DOLPHIN/evidence/D4_findings.md:99-101` still contains the exact line §8.1
  bans: "This is the finger-specific divergence R5's pre-registered contract flags as
  **confirming the mechanism**." `execution/R1-DOLPHIN/STATUS.md:209-217` repeats the
  headline ("middle/ring divergence SURVIVES frame-matching … real skeleton/pose candidate")
  with no pointer to the dissolution. `git log` confirms neither file was touched after
  `176480e9` — the correction exists only inside `R5-HANDS-ENDGAME/VERDICT.md`.
- Engine classjson row `RB3_HANDS_AUTHORED_REPOINT` (re-emitted into
  `NATIVE_COMPAT_LEDGER.md:173` by the `2c5b30cf` regen) still asserts BOTH corrected claims:
  "the genuine fix is an engine per-member RESKIN of verts+weights … NOT any offset/repoint"
  (banned as a mandate by §8.4 — a native-only inference that under GT-A degenerates to the
  refuted W16 cell) and "RB3_NO_SKIN_CLAMP remains the shipped mitigation" (corrected by §8.3
  — the clamp skips rebound meshes, `Rnd_Wgpu_RB3.cpp:3777,3812` / `BandCharacter.cpp:1898`;
  the shipped hands state is the default rebake's ceiling-hand).
- `execution/RETROSPECTIVE/ROADMAP.md:28` (row R5) carries the same clamp shorthand —
  the VERDICT §8.3 explicitly named this row as needing the fix.

**Why HIGH:** Wave 18's D5-branch lane executes a mechanical table lookup and will read these
exact files. A lane that greps the flag registry per §4 lint 4 will find a default-relevant row
asserting "the genuine fix is a RESKIN" — the precise claim the wave adjudicated as unproven.

**Correction:** append correction blocks (do not rewrite history): (a) a 3-line "R5 §8.1
standing correction" note atop D4_findings.md §Interpretation and the D4 section of
R1-DOLPHIN/STATUS.md; (b) amend the engine classjson row's `faithfulStatus` tail ("W17 R5
correction: reskin-mandate unproven pending D5, see R5-HANDS-ENDGAME/VERDICT.md §8; shipped
hands residual = default-rebake ceiling-hand, NOT a NO_SKIN_CLAMP artifact") under the
classjson lock + one regen; (c) same one-liner on ROADMAP row R5.

### F2 (MEDIUM — D4's headline packaging outran its evidence; the honest caveat lived one level down)

**Claim:** "FRAME-MATCHED table DELIVERED — the R5-decisive table" (STATUS.md:178-184, commit
subject `176480e9`). **Evidence:** the Wii side has zero matched frames of anything —
`D4_wii_drivers.json` `active_player_clip_drivers: []`; the join is a driver-less settled pose
against the native vignette *envelope*, i.e. a FLOOR, not a frame match. D4's own findings
carry this precisely (D4_findings.md:105-109) and the R5 contract check states it flatly
(VERDICT §0 item 2: "Zero matched articulated frames exist"). The lane's *analysis* was honest;
its *headline and title* were not, and that headline is what STATUS/commit-log readers consume.
This is the §4-lint-2 failure shape one level up: the aggregate label ("frame-matched")
closed a cell the rows don't support. **Correction:** folded into F1's annotations; also worth
one line in the R6 template's close-out section: *"headline claims are gate-checked against the
lane's own caveats before the coordinator relays them."*

### F3 (MEDIUM — R2's known-GOOD arm has the weakest provenance of the three fixtures)

R2's metric result is solid (verified: `BlendSpreadSeparatesTornBlend` reads the REAL committed
goldens via `UseRealFixtures()`, test_skinning_oracle.cpp:325-342; goldens in `git ls-files`;
margin 6.95x good-vs-torn). But the known-GOOD reference itself:

- `evidence/good-body/provenance.txt` is the **legacy format** — no `arm_env`, no
  `palette_dump_env` (engine_head `04651e1`), i.e. exactly the gap STATUS.md:67-69 declares
  "fixed" without noting good-body was not re-captured under the fix.
- All four committed good-body screenshots are **byte-identical**
  (md5 `3153e740…`, 36,970 B each — vs ~1 MB per shot for the bad arms): the committed visual
  evidence for the good arm is one repeated (likely stale/pre-render) frame. "Coherent even
  under articulation" (STATUS.md:19) is therefore grounded ONLY in the 12 palette dumps +
  the metric, not in any committed pixels.
- The M1 Tier-1 table (87.3/42.6 vs 3.1/3.1) exists only in STATUS prose + one provenance
  `note=` line (bad-ceiling/provenance.txt:6); no computed artifact committed (lint 7
  borderline — the raw dumps ARE committed and recomputable, and the 3.1 is pinned in-test at
  test_skinning_oracle.cpp:445).

**Why it matters beyond hygiene:** R5's G5′ gate (VERDICT §6) names these exact dumps as the
palette-level known-good/known-bad separation surface for the D5 fallout lane. **Correction:**
re-capture good-body under the new provenance format with real per-frame shots (cheap — the
harness exists) before any Wave-18 lane leans on it; commit the M1 table as a small JSON.

### F4 (MEDIUM — R4 evidence in untracked limbo violates lint 7's letter)

Committed evidence covers the headline (M1 table, M2/M3 gate JSONs, ledger, G2/G3). Untracked
in the live tree, however: `execution/R4-DETERMINISM/evidence/attrib-off-boot{0..3}.log`
(7.2/3.6/5.8/7.0 MB — the M1 raw arms) and `execution/R4-DETERMINISM/loaddet_attrib.py`
(12.6 KB, referenced by no committed doc; the promoted harness is
`scripts/native/loaddet_gate.py`). Lint 7: committed or it doesn't exist. The right call for
24 MB of raw logs is probably deletion (the derived `attrib-off-boot.json`/`attrib-off-gate.json`
ARE committed and the run is regenerable), but the disposition must be explicit. Same decision
needed for: `execution/BOOTRNG/measure/smoke.json`, `execution/REBASELINE/harness/` (whole dir),
`execution/WASH-fix/measure/probe_s2pin.json`, and `/tmp/r4-status-stale-draft.md` (parking a
draft in `/tmp` is the worst of both — commit under the lane if unique, else delete). The
`M native/src/rb3_session_trace.cpp` in `git status` appears to be another agent's live work —
do not fold it into close-out commits (standing hazard-note pattern).

### F5 (LOW — R4 ledger axes were re-semantized mid-run; disclosed, but unmarked in the artifact)

STATUS.md:71-74 discloses the count/order/clock axes were "corrected to anchor-relative /
completion-only / span==k semantics" and this run's ledger "recomputed offline from the saved
ON-boot logs." The rationale is coherent (the residues are non-gating by the seam's
re-base-at-anchor design), the pre-registered PRIMARY was never touched, and OFF-arm fail-red
reproduced at N=10 (spread 7179) — so this is NOT a redefine-until-green move. But
`LEDGER.md:15-17` embeds the revised definitions inline with no marker that they differ from
plan §3.4; a future reader will take "10/10" as passing the original axis definitions.
**Correction:** one line in LEDGER.md: "axes v2 (2026-07-07), revised from PLAN-R4 §3.4 —
see STATUS M3 note." Also note `callerOrder 1/10` stays prominently informational (it is).

### F6 (LOW — classjson PASS-PRIMARY flip: precondition verified SATISFIABLE-BY-VACUITY, correctly carried forward)

Checked, not a defect: PLAN-R4 §M3 (plans/PLAN-R4-loader-determinism.md:267-272) authorizes the
coordinator flip "with the required flag-ON drawlog re-golden for any gameplay-scene goldens."
No gameplay-scene drawlog golden exists (`native/tests/goldens/drawlog/` = splash_screen +
synthetic only), so there was nothing to re-golden, and the regenerated row correctly carries
the precondition forward as a blocking note (classjson:1435: "flag-ON gameplay-scene drawlog
re-golden required before any gate adopts the ON-arm as its baseline"). **Requirement it
creates:** the Wave-18 WHITE re-grade charter MUST carry both preconditions verbatim — ledger
PASS 10/10 on the exact re-grade boots (PLAN-R4 §M4.1's validity lint) + the re-golden if any
gameplay golden adopts the ON arm.

### F7 (LOW — R5's GT-A *lean* borrows support from a confounded cell; the DECISION is insulated)

VERDICT §1 F6 and §2 cite slot1 (`player2_f`) covering the Wii magnitudes at 0.12–0.65° as
clip-state support. slot1 is the cross-gender cell class D4 itself de-weighted ("confounded …
must not carry the verdict," D4_findings.md:110-113), and the committed `.out` shows the Wii
value inside slot1's envelope on only **2 of 6** surviving pairs
(r5_adversarial_rederivation.out:16-21). The VERDICT's wording is accurate (it says "mid-segment
pairs," acknowledges the confound) and decision C does not rest on the lean — the branch table +
falsifier #1 make it harmless. Recorded so the D5 lane doesn't inherit "slot1 proved coverage"
as stronger than it is.

### F8 (LOW — orphaned process state)

The paused wedged workflow `wf_a6d64a07-446` is referenced nowhere under `docs/` — record its
disposition (killed / resumed / superseded-by-continuation-agents) in the README wave-17
section, or it becomes an archaeology item. `/tmp/wave17-checkpoints/` still holds scratch
(`m1_probe.cpp`, `loader_inc.h`, six lane JSONs incl. an unexplained `T.json`) — fine as scratch,
but the checkpoints that back lane verdicts are already superseded by committed STATUS docs;
nothing to rescue.

---

## Q2 — Does the R5 dissolution of D4 hold? **YES** (re-verified)

1. The re-derivation consumes only committed evidence (`D4_native_sweep_raw.tar.gz`,
   `D4_delta_table.json`) and its committed `.out` matches every number quoted in the VERDICT;
   I re-read script + output line-by-line. F2 (the script cross-validating the join script's
   envelopes to <0.05°) makes this a genuine independent check, not a re-run.
2. The hard-coded Wii magnitudes match the D2 primary evidence
   (`D2_interbone_table.md`: mf12 24.366, rf12 26.126, th01 123.644 …) — no transcription smuggle.
3. The logic is used in the correct direction: F1–F6 show D4's FLOOR is contaminated by
   clip-curl-interval coverage, hence **cannot confirm** the mechanism — and the VERDICT
   refuses BOTH letters (GT-B unproven, GT-A only a stated prior) and buys the discriminator.
   Falsifier #2 names exactly the surviving loophole (native family systematically more curled
   *for basis reasons*) that D5 closes. The one place the argument is weaker than it reads is
   F7 above (slot1), and the decision is insulated from it.
4. §8 self-compliance: the VERDICT cites neither banned line; its own affirmative claims
   ("anchors and thumbs exonerated," "native static skeleton externally Wii-faithful") rest on
   FLOOR < 0.06° statics — and a FLOOR near ZERO is meaningful even under coverage
   contamination (only large FLOORs are ambiguous), so the asymmetry is legitimate.
5. The §0 contract check applies PLAN-R5 §2.4 as written (item 2's "R5 formally requests R1-M4
   and pauses" trigger verified at PLAN-R5 §2.4 lines 148-154). The pre-registered branch table
   inherits §3.1 thresholds verbatim, including the indeterminate fail-safe.

## Q3 — Cross-lane contradictions (R2 "both arms bad" vs the shipped record)

R2's reframing (STATUS.md:28-30: default = ceiling, repoint = torn, known-good = body meshes) is
**consistent** with R5 §3 and with the Wave-15/16 record — no contradiction between the wave's
lanes. The contradictions are all with the PRE-wave record, catalogued in F1 (classjson
REPOINT row, ROADMAP R5 row, README Wave-16 rows "genuine remaining option: engine per-member
TRUE reskin … RB3_NO_SKIN_CLAMP as the shipped mitigation"). The eleven-defaults ledger itself
is clean: `RB3_NO_HEAD_REBIND` (ledger:234) claims only "W2.2 net win," not "hands fixed" —
but the README wave-17 section should carry R2's reframing verbatim so the defaults narrative
cannot be read as "hands are good."

## Lane scorecard (Q1, one line each)

| Lane | Self-grade | Review grade |
|---|---|---|
| R1 D2 | M1 GO, apploader patch | **Accurate** — boot log + 989/992 rigidity + bilateral symmetry committed; red AND green shown |
| R1 D3 | table + priced caveat | **Accurate** — conjugation caveat was load-bearing and correct |
| R1 D4 | "frame-matched, R5-decisive" | **Overstated headline** (F2); analysis honest; machinery vindicated by R5's F2 |
| R5 | verdict C, D4 dissolved | **Holds** (Q2); minor lean on a confounded cell (F7) |
| R2 | M1 GO + M2 green | **Substantially accurate**; known-good arm provenance weakest link (F3); SKIP dispositions verified honest in-code |
| R3 | DONE, all gates GREEN | **Accurate** — G4 knob shows a real LoadOp flip (draw idx 98 Clear↔Load), G1 fail-red real, v1 caveats honestly carried into the usage doc |
| R4 | PRIMARY PASS 10/10 | **Accurate** — with the F5 semantics marker owed; G2/G3 verified |

## Coordinator MUST-FIX before writing the README wave-17 section

1. **F1 annotations**: D4_findings.md + R1-DOLPHIN/STATUS.md correction blocks; engine
   classjson `RB3_HANDS_AUTHORED_REPOINT` row amendment + single regen; ROADMAP.md:28 row R5.
2. **F3**: re-capture good-body (new provenance format, real shots); commit the M1 Tier-1 JSON.
3. **F4/F8 dispositions**: commit-or-delete every untracked item listed; record
   `wf_a6d64a07-446`'s fate; empty `/tmp/r4-status-stale-draft.md` limbo.
4. **F5**: "axes v2" marker in R4 LEDGER.md.
5. README wave-17 results table carries: R2's both-arms-bad reframing verbatim; D4 described as
   "settled-pose FLOOR table (not frame-matched); headline dissolved by R5 §1"; the R4
   PASS-PRIMARY row with its forward precondition; the D5 branch table as the standing next step.
6. R6 template amendment (one line each): wave-end regen must sweep the wave's standing
   corrections into contradicted classjson rows in the SAME regen; coordinator gate-checks lane
   headlines against the lane's own caveats before relaying.

## Recommended Wave 18 menu

- **Lane H′ — D5 branch execution (flagship; lane already in flight).** When the articulated
  Wii capture lands, run the pre-registered branch table MECHANICALLY (VERDICT §4; gates
  G-D5-1..4 + G5′) and fund the named follow-on in the same wave: GT-A → the hard-boxed palette
  forensics lane (substrate = R2 dumps + D4/D5 worlds + item-5 binds; fix F3 first so the
  good-body substrate is trustworthy), GT-B → B1 anim-basis. **Precondition: F1's record
  corrections land first.** GT-D closure is pre-authorized if the box exhausts.
- **Lane L′ — R4 M4 cash-in: WHITE re-grade + wash co-sampling charter.** Now genuinely
  unblocked; highest info-per-cost item on the backlog. Charter must embed PLAN-R4 §M4.1's
  validity precondition (ledger PASS 10/10 on the exact re-grade boots) + the F6 re-golden rule.
- **Lane S′ — R2 net-tightening (small, one lane):** engine-emitted Tier-1 field in
  RB3_PALETTE_DUMP (flips the VerdictTable SKIP into a real reproduction), good-body re-capture
  (F3), and register the D4 middle/ring curl assertion as the regression net for whatever fix
  the D5 branch names.
- **Lane U′ (optional) — R3 v1 walk gap:** instanced list-widget rows + overshell root into the
  authored walk (closes the per-song-row A/B and the "names the fill quad" gap); natural pairing
  with the sidebar-quad authoring question now that the instrument exists. Fund only if a UI
  lane fits; otherwise carry.
- **Defer:** 4→8 lights; W2.4 BandPatchMesh — attractive under R2's new oracle net, but it is
  twice-burned (deformed-chars record) and touches the same blend machinery as the hands
  endgame; do not run it concurrently with a hands fix lane. Web-build confirmation pass rides
  as a cheap tail gate on whatever lands, not as a lane.
