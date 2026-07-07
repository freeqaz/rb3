# Wave NN — Kickoff (STANDING TEMPLATE — fill in the blanks)

> **How to use this file.** Copy it to `execution/WAVE<NN>_KICKOFF.md`, replace every
> `<...>` placeholder, and delete this blockquote. The **Pre-dispatch checklist** (§ below)
> is MANDATORY — every box must be checked (or explicitly marked N/A with a one-line reason)
> before the wave is dispatched. It carries the ten process lints from
> `RETROSPECTIVE/OPTIONS.md` §4 verbatim; do not paraphrase away their teeth.
> Provenance: R6 of `RETROSPECTIVE/ROADMAP.md`; shape mirrors `WAVE16_KICKOFF.md` /
> `WAVE17_KICKOFF.md`.

**Author:** <coordinator>. **Status:** <DRAFT | REVIEWED (Fable, `WAVE<NN>_REVIEW.md` rb3 `<sha>`) —
amendments adopted | DISPATCHED>.
Parent: `<RETROSPECTIVE/ROADMAP.md + plans/PLAN-*.md | execution/README.md>`. Engine pin `<sha>`.
`<N>` defaults ON.

## COORDINATOR ACCEPTANCE (<date>)

_The reviewer's amendments, adopted, as a BINDING list. Each entry: severity + one-line shape,
then the concrete change. Anything here overrides the original draft below where they differ._

- **A1 (<severity>, <shape>):** <accepted amendment, concrete>.
- **A2 (<severity>, <shape>):** <accepted amendment, concrete>.
- **A<n> …**
- **Hazard note:** <standing tree hazards — e.g. the engine tree's long-standing uncommitted
  `M FxSendNative.cpp` (concurrent audio work): never stage it>.

## Shape

<One paragraph: what KIND of wave this is (instrument / fix / diagnosis / gameplay), what the
reviewer's job is this cycle, and any unusual structure — e.g. "lane briefs ARE the
already-authored plans, executed as written">.

## Lanes

**Lane <X> — <KEY> <title> (<model>; executes `<PLAN-file | inline brief>`):**
<Scope, milestones, exit criterion. State exactly which source trees the lane may WRITE
(grants) and which stay read-only. Diagnosis lanes get WIDE read grants; fix lanes stay
narrow (§4 lint 5). Name the engine edit sites if the lane touches the engine, and confirm
they are disjoint from other lanes' edit regions.>
Exit: <the artifact or priced NO-GO that closes the lane>.

**Lane <Y> — …**

_(Repeat per lane. Note any landing sequencing between engine writers; keep lanes PARALLEL and
sequence only the LANDING where two lanes touch the same TU region.)_

## Process rules (carried) — VERBATIM, do not edit per-wave

Locks (add ONLY your own files under flock; NEVER stage another lane's or the engine's
uncommitted edits):
- rb3 repo commits — flock `/tmp/rb3-git.lock`
- milo-native-engine commits — flock `/tmp/milo-engine-git.lock`
- engine `class.json` append (new flags) — flock `/tmp/milo-engine-classjson.lock`
  (append-only; a SINGLE coordinator regen at close-out — lanes never regen)
- native / build-native builds — flock `/tmp/rb3-native-build.lock` (or own build dir)
- milo-trace — its own repo, its own norms

Checkpoints: `/tmp/wave<NN>-checkpoints/<lane>.json` — **check-first** (a valid checkpoint is
AUTHORITATIVE; do NOT redo checkpointed work after a harness restart), **write-before-return**,
update at EVERY milestone.

PLAN/STATUS: `execution/<KEY>/PLAN.md` + `STATUS.md` per lane; human-readable detail lives in
STATUS, the final agent message is the structured output.

Evidence: committed under `execution/<KEY>/evidence/` — `/tmp` is scratch (§4 lint 7).

Flag discipline: new flags default-OFF + `class.json` append under lock; **NO default flips, NO
pin bumps by lanes** (the coordinator bumps the pin ONCE at close-out). Refuted flags UNSET.
`<N>` defaults stay ON. Commit per review cycle.

Harness: headless `RB3_HTTP=1 RB3_FIXED_CLOCK=1`, free ports, frame-count settling, **pgid-only
cleanup (NEVER `pkill` by name)**.

## Pre-dispatch checklist — the ten §4 lints (BINDING on every lane)

_Every box must be checked before dispatch. These are quoted from `RETROSPECTIVE/OPTIONS.md` §4;
the parenthetical is the rationale / cost-of-omission from the record. If a lint is N/A to a
lane, mark it `N/A — <reason>` explicitly; silence is not a pass._

- [ ] **1. Matrix-relative + pointer-verified, or it isn't a bone claim.** "Any basis/rotation
  claim must be a relative matrix product between named instances (`angle(X·inv(Y))`), with each
  instance's pointer identity dumped. Scalar angle-to-identity numbers are banned as evidence."
  (Would have killed the own/bound swap at W9 instead of W13; the 87° story at W11 instead of W15.)
- [ ] **2. Split by gender/mesh/route by default; aggregates cannot refute.** "Any probe over
  heterogeneous populations reports per-population rows; an aggregate number may motivate but
  never close a cell." (The confounded death certificate, W12→W15; the U1 two-mechanism lump,
  W12→W16.)
- [ ] **3. No unvalidated oracles as gates.** "Before a metric gates a wave, fire it on a
  known-GOOD and known-BAD frame and record the separation in the lane STATUS." (wext, SKINPOS.)
- [ ] **4. Shipped-flag contradiction check.** "Before escalating a diagnosis to a new engine
  item, grep the flag registry's one-line 'what this shipped flag PROVES' index; if a default-ON
  flag contradicts the claim, the claim is wrong." (W15 "shader ignores color" vs the shipped
  W4.2 flip — cost a full Fable re-derivation stage.)
- [ ] **5. Diagnosis lanes get wide read grants.** "Fix lanes stay narrow; a lane whose exit is
  a *mechanism* must be allowed to follow the mechanism across TU boundaries." (ROWFIX Part B
  stopped at its grant edge and mislabeled an rb3 plumbing bug as an engine gap.)
- [ ] **6. Enumerate the option table before the second fix attempt in any family.** "Maintain
  the (anchor × bone) — or family-equivalent — coverage table with measured/unmeasured status in
  the family STATUS. The hands winner sat unmeasured for 4 waves while measured-dead cells were
  re-argued. Run the adjudication lane **before** the 3rd dead cell, not after the 7th."
- [ ] **7. Evidence lives in `execution/<KEY>/evidence/`, committed, or it doesn't exist.**
  "Probe outputs feeding any conclusion get copied before the lane returns; `/tmp` is scratch."
- [ ] **8. Flag hit-count on every negative result.** "'Fix measured no benefit' requires the
  branch-entry count (`RB3_FLAG_HITCOUNT` pattern) proving the code ran." (W3's false-negative
  BIND_FIX fired 0×.)
- [ ] **9. Flavor-membership check before scoping a lane on a file.** "One `grep` of the CMake
  source lists: is this TU compiled into rb3-native?" (W3's primary lead was DC3-only.)
- [ ] **10. Tools ship as pre-dispatch diagnosis gates, not post-mortems.** "If a wave's plan
  includes 'build instrument X to verify the fix,' build X *first* and let it grade the diagnosis
  before the fix is implemented." (Gaps 4/5/6/8 of REPORT §4 were each built 1–3 waves late, as
  the refuting verify stage.)

## Close-out (coordinator, end of EVERY wave — owner directive 2026-07-07)

1. **Post-wave Fable review** — a Fable subagent reviews the wave's RESULTS (lane STATUS docs,
   checkpoints, evidence, commits) the same way the pre-dispatch review vets the kickoff:
   verdict validity, gate coverage, contradictions with shipped flags, anything a lane
   self-graded too generously. Output `WAVE<NN>_CLOSEOUT_REVIEW.md`, committed.
2. **Docs updated** — `execution/README.md` gains the wave's results table + next-wave menu;
   per-lane STATUS docs finalized; ROADMAP/plan docs amended where the wave changed them;
   single classjson regen + census check; ONE coordinator pin bump.
3. **Findings summary handed back to the owner** — the coordinator's final message of the wave
   is a user-facing summary: what shipped, what was refuted (with the evidence one-liner), what
   the next wave should be and why. Not a log dump — the "teammate catching up" version.

## Risks / open questions for the reviewer

- **R-A:** <first open question — the thing most likely to be wrong in this draft>.
- **R-B:** <second>.
- **R-C:** <verify the plans/edit-ranges still match the CURRENT tree state at the named pin —
  they may have been authored against an older tree>.
