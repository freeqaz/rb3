# Wave 17 — Kickoff (instrument wave; executes the Fable-authored roadmap plans)

**Author:** coordinator. **Status:** REVIEWED (Fable, `WAVE17_REVIEW.md` rb3 `c5f1a925`) —
**dispatch-with-amendments, all adopted; notably the first wave in the campaign's recent record
with NO false premise found** (both engine ranges resolve exactly by symbol at pin `51640ff`;
R1's M1 claims all verified true: wbfs 4.1GB on disk, dolphin-emu-nogui built with the W7 hook
`bc3b1f5`, milo-trace RSP client present, map anchors match, `/api/call` exists gated
`RB3_REPLAY_API=1`).

## COORDINATOR ACCEPTANCE (2026-07-07)

- **A1 (HIGH, sequencing shape):** lanes stay PARALLEL; only the LANDING is sequenced. Lane S
  commits its engine probe block at M1-exit (M1's only code artifact, landable regardless of the
  metric verdict). Lane U develops its engine sidecar in an ISOLATED ENGINE WORKTREE from wave
  start (its M1 IS the engine spike), rebases when S's commit lands, escalates to the
  coordinator at the mid-wave checkpoint if it hasn't landed. No hard-sequenced stage.
- **A2 (MEDIUM, grants):** Lane D's writable surfaces are OUT-OF-REPO and granted explicitly:
  `/home/free/code/milohax/milo-trace` (its own repo — commit there per its norms) and the
  dolphin fork. rb3/engine source stays read-only for Lane D beyond declared probe additions.
- **A3 (MEDIUM, carve-out):** the carried "refuted flags UNSET" rule gains a CAPTURE-ARM
  carve-out: Lane S's known-bad arm runs `RB3_HANDS_AUTHORED_REPOINT=1` deliberately (verified
  in-tree default-OFF at `BandCharacter.cpp:1372-1374`, Wave-16 tear evidence real) — allowed
  ONLY inside the fixture-capture harness, never in gate/verify arms of other items.
- **A4 (LOW, M1 box):** corrected reading — the ~1-day box covers M1 TOTAL; switch route A→B
  (retail DOL) on the FIRST decisive route-A failure, not after a day of troubleshooting.
- **A5 (LOW):** Lane U records its FULL engine edit-site list in PLAN.md (DrawMesh call site
  `:5283`, `RecordDrawLog` `:5324`, `WriteSceneUniforms` ~`:1380`, four BeginRenderPass sites,
  +3 other TUs — all verified disjoint from Lane S's `:4736` region).
- **A6:** rb3-tests fixture suites are boot/asset-free by design; `orig-assets/extracted` is
  present for capture arms. Eleven-defaults tally reconciled; all ten §4 lints carried.
- **Hazard note:** the engine tree carries the long-standing uncommitted `M FxSendNative.cpp`
  (concurrent audio work) — as always, never stage it.
Parent: `RETROSPECTIVE/ROADMAP.md` + `RETROSPECTIVE/plans/PLAN-R{1,2,3}-*.md` (Fable-authored,
cross-checked by `plans/INDEX.md` incl. the COORDINATOR RESOLUTIONS section, which is BINDING).
Engine pin `51640ff`. Eleven defaults ON.

## Shape

This wave is unusual: the lane briefs are not designed here — they are the three
already-adversarially-authored plans, executed as written, with the INDEX.md resolutions
(M-1 sequencing/ranges, M-2 forearm→wrist row, M-3 bone-world comparand) applied. NO gameplay
fixes this wave. The reviewer's job is therefore narrower than usual: check THIS document's lane
scoping/sequencing against the plans, not re-review the plans themselves.

## Lanes

**Lane D — R1 Dolphin ground-truth probe (Opus; executes `PLAN-R1-dolphin-probe.md`):**
M1 go/no-go first (headless Dolphin boots the game + one scripted, map-named memory read),
then the inter-bone delta pipeline per the plan (§3 design; pair list INCLUDES forearm→wrist
per M-2; comparand = bone worlds per M-3). Exit: the per-pair Wii-vs-native delta table for one
member's hand chain at a matched clip time, or a priced NO-GO at M1. Nothing in this lane
touches rb3/engine source beyond what the plan declares (probe-side additions only).

**Lane S — R2 skinning fixtures + oracle-validation harness (Opus; executes
`PLAN-R2-skinning-fixtures.md`):**
Fixtures reproduce the Wave-15 arm-W verdict table from a clean build (male 0.1°/3.1° PASS,
female 28.9° FAIL-pre-fix) AND encode the Wave-16 failure as a permanent red test
(`BlendSpreadSeparatesTornBlend` at animated frames with `RB3_HANDS_AUTHORED_REPOINT=1` as the
known-bad arm). Adds the bone-world comparand variant per M-3. Its engine block
(~`Rnd_Wgpu_RB3.cpp:4736` declared range) lands FIRST among the two engine writers (M-1).
Oracle-validation harness per the plan.

**Lane U — R3 uidump + drawlog provenance (Opus; executes `PLAN-R3-uidump.md`):**
Game-side work proceeds immediately; its engine sidecar edit (~`Rnd_Wgpu_RB3.cpp:5324` declared
range) REBASES AFTER Lane S's engine commit exists (M-1 sequencing — poll for the commit, don't
block other work on it). Validation milestones = reproduce the W14 red-band LoadOp diagnosis and
the ROWFIX main-vs-alt font-material split from the shipped build.

**R6 (this document):** the ten process lints from `OPTIONS.md` §4 are BINDING on all three
lanes (notably: matrix-relative + pointer-verified bone claims; committed evidence under
`execution/<KEY>/evidence/`; no unvalidated oracles; instruments validated on known-good AND
known-bad before use).

## Process rules (carried)

Locks, checkpoints (`/tmp/wave17-checkpoints/`), commit-per-review-cycle, PLAN/STATUS per lane
under `execution/R{1,2,3}-*/`, append-only classjson + single coordinator regen, own build dirs,
NO default flips or pin bumps by lanes (coordinator bumps ONCE at close-out per M-1), refuted
flags UNSET, ELEVEN defaults stay ON, pgid-only cleanup, frame-count settling.

## Risks / open questions for the reviewer

- **R-A:** Lane D's M1 is the campaign's first out-of-repo dependency (Dolphin). Is the go/no-go
  budget (one day-equivalent of agent effort before retail-DOL fallback, per ROADMAP) encoded
  clearly enough that the lane won't burn its wave on boot troubleshooting?
- **R-B:** the M-1 sequencing (S's engine commit before U's engine edit) — is a simple
  poll-for-commit adequate, or should U's engine half be split into a separate follow-up stage?
- **R-C:** anything in the three plans that contradicts the CURRENT tree state (they were
  written against pin `51640ff` same-day, but verify the two declared engine ranges still
  resolve to the named regions by symbol).
