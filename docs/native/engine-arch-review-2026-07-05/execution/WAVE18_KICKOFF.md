# Wave 18 — Kickoff (cash-in wave: D5 funded follow-on + R4 M4 + R2 net-tightening)

**Author:** coordinator. **Status:** REVIEWED (Fable, `WAVE18_REVIEW.md` rb3 `c20701a0`) —
dispatch-with-amendments, ALL adopted below.
Parent: `execution/README.md` (Wave 17 results + Wave 18 menu) + `WAVE17_CLOSEOUT_REVIEW.md` +
`R5-HANDS-ENDGAME/VERDICT.md` (branch table BINDING). Engine pin `49ca0d6`. ELEVEN defaults ON.

## COORDINATOR ACCEPTANCE (2026-07-08) — BINDING

- **A0 (basis correction):** V-over-GT-D is justified by the VERDICT's OWN closure-honesty
  condition (§3: GT-D is honest only when ground truth is *unobtainable in budget*; D5's own
  exhaustion report prices the visually-guided path FEASIBLE on this box; falsifier #4
  anticipated exactly this). Pre-authorization removed permission, not mandated execution.
- **A1 (BLOCKING, Lane V — hard stop made mechanical):** end-of-box without BOTH a
  G-D5-1-passing capture AND an assigned branch letter — **including indeterminate/partial
  captures (e.g. <3 frames or swing <15° per the VERDICT power-floor row)** — IS exhaustion →
  coordinator executes GT-D. No re-pricing loop. Exhaustion-report contract (D5-style):
  machine JSON + states reached/screenshots/articulation spans/failing stage/price + the
  explicit sentence "no branch letter assignable". Gameplay captures use the ε_noise-via-pause
  mechanism.
- **A2 (BLOCKING, Lane W — F6 verbatim):** the precondition is "ledger PASS 10/10 **on the
  exact measurement boots**, otherwise the measurement is **VOID**" (the harness must REFUSE to
  emit a verdict) — NOT discard-and-rerun, which masks seam regressions and biases the sample.
  Integration scoped: the white/wash boots themselves run `RB3_LOADDET_ATTRIB=1` and the ledger
  is graded from THOSE boots' logs (grade-external-logs mode) — never from separate
  `loaddet_gate.py` boots.
- **A3 (HIGH, Lane W — reproduce-first):** the seam pins ONE trajectory (hardcoded
  `Seed(0x5EED)`, `Rand.cpp:61`, no seed knob) and reroutes WHITE-implicated consumers → NO
  comparisons to Wave-10/11 absolute numbers. First gate = reproduce the phenomenon under the
  seam (WHITE-fix validity gate `mean(hi_frac|OFF) ≥ 15`); on failure the item is HELD
  substrate-blocked and the follow-up is a coordinator item (`RB3_LOADDET_SEED` knob — outside
  W's writable set).
- **A4/A5 (MEDIUM, Lane N):** the Tier-1 field emits the **`:4820-4841` xcheck quantity**
  (off × the first-seen cached rest world from the `RB3_HANDS_ATTACH`-gated `sHaRest` cache),
  NOT off×WorldXfm (that re-derives the ~180° offline artifact the SKIP exists to avoid).
  Probe block is at **`:5003-5106`** (re-derive by symbol; the kickoff's :4736 was stale).
  Dump format: NEW header-key line only (parser ignores unknown keys); NEVER extend `bone`/`v`
  lines (strict sscanf would reject every committed golden). Flipping the SKIP requires
  arm-w/arm-s captures (flags verified registered) + the VerdictTable test edit. D3/D4 scripts
  are NOT palette-dump consumers (kickoff claim withdrawn).
- **A6 (MEDIUM, answers R-D):** Lane V STOPS at the branch letter. GT-A palette forensics is
  the VERDICT's own separately-boxed lane AND depends on Lane N's good-body re-capture — a
  coordinator-dispatched follow-up, not a V stage. The kickoff's "no ordering constraints"
  claim is corrected: V(GT-A follow-up) → gated on N(F3).
- **A7 (LOW, Lane V):** hub-vignette route first but timeboxed ~20% of the box and
  articulation-DETECTOR-gated (articulation there is UNCONFIRMED) before committing.
- **A8 (LOW, Lane N):** F3 completion = md5-DISTINCT screenshots + M1 JSON provenance.
- **A9 (LOW, Lane N):** the curl assertion lives with the real-fixture suite, NOT synthetic
  Suite C.
- **A10 (record):** Lane D5 executed after the Wave-17 close-out — stamped Wave 18.
- **R-B protocol (Lane W):** N=10/arm, existing G1a decision rule unchanged, early-stop at N=5
  only under near-zero within-arm spread AND ledger 5/5.
- **Hazard note:** engine tree carries the long-standing uncommitted `M FxSendNative.cpp`
  (concurrent audio work) — never stage it. rb3 tree carries other agents' uncommitted
  `native/src/rb3_session_trace.cpp` — never stage it.

## Shape

Cash-in wave: Wave 17 built the instruments; this wave spends them. One lane finishes the hands
discriminator with the capability D5 proved missing (visually-guided Wii navigation); one lane
cashes in R4's determinism on the two held venue items; one lane tightens R2's net (the review's
F3 debt + the engine-emitted Tier-1 field). The reviewer's job: check lane scoping/boxes against
the VERDICT branch table, the D5 exhaustion report (`R5-HANDS-ENDGAME/evidence/D5_findings.md`,
rb3 `09fc594d`), and the F6 ledger-precondition requirement — and adversarially check the two
DECISION rules below (V's hard stop; W's flip protocol).

## Lanes

**Lane V — VISCAP: visually-guided articulated Wii capture → D5 branch execution (Opus;
executes `R5-HANDS-ENDGAME/VERDICT.md` §4 with D5's followup #2):**
Build the visually-guided nav rig on the D2 Bank-8 patched-disc boot (Xvfb + GPU video backend +
periodic screenshots driving input decisions + guitar-extension Wiimote where needed), reusing
D5's pipe-input instrument (`milo-trace tools/wii_bone_dirboot.py`, `0bc0610`) and D5's measured
shell-state map (START→18 drivers, A→8 resident clips; the wall = guest-profile/YES/nav-help
prompt flow). Goal state: any state with a LIVE hand-animating CharClipDriver (hub-band vignette
or gameplay). Then: articulated capture with clip+frame key → the matched-frame join (D3/D4
machinery, re-validated red/green on the new capture) → classify per the VERDICT branch table →
execute the classified branch's next step ONLY if it is measurement (GT-A palette forensics on
R2's dumps, G5′ gates); any FIX is a separate coordinator-dispatched item.
Writable: milo-trace (own repo), rb3 `execution/` docs/evidence + `scripts/`; engine READ-ONLY;
rb3 game/native source READ-ONLY (native side capture uses the committed D3/D4 probes).
**HARD STOP (BINDING, pre-authorized twice — VERDICT §4 + this kickoff): one lane-wave box; on
exhaustion, report priced; the coordinator then executes GT-D closure with the §8
clamp-corrected residual statement and NO further discriminator lanes are dispatched.**
Exit: branch letter + its measurement artifact, or the exhaustion report that triggers GT-D.

**Lane W — R4-M4: WHITE re-grade + wash per-FX co-sampling (Opus; the F6 cash-in):**
Re-grade the held `RB3_VENUE_WHITE_GUARD` on the now-resolving gate and build/run the wash
per-FX co-sampling instrument (the Wave-11 residual: FX/swept-light PHASE axis). CHARTER
(BINDING, F6): every measurement boot runs under `RB3_FIXED_CLOCK=1 RB3_LOAD_DETERMINISM=1` and
must PASS the per-axis ledger (`scripts/native/loaddet_gate.py --ledger`) before its sample
counts — a boot that fails the ledger is discarded and rerun, never averaged in. Deliverable =
measurement package + READY_FOR_FLIP or a held verdict with the numbers; **the flip decision is
coordinator E1-gated as always — the lane flips nothing.** Note the classjson row precondition:
the ON-arm has no gameplay drawlog golden — if the lane needs one, it captures and proposes it
as evidence, coordinator adopts at close-out.
Writable: rb3 `execution/` + `scripts/`; engine READ-ONLY (the guard flag is already landed).

**Lane N — R2-NET: fixture net-tightening (Opus; R2 followups + review F3):**
(1) Engine-emitted Tier-1 field in the `RB3_PALETTE_DUMP` sidecar (SOLE engine writer this wave;
declared range = the existing R2 probe block region around `Rnd_Wgpu_RB3.cpp:4736` — re-derive
by symbol; default-OFF, classjson append under lock) → flips the VerdictTable gtest from honest
SKIP to a real arm-W table reproduction. (2) Re-capture the good-body golden with the fixed
provenance format (F3: legacy provenance, byte-identical repeated frames) so it can back G5′.
(3) Add the cheap middle/ring curl regression assertion from the D4 FLOOR table (carrying the
D4 CORRECTION block's framing — it asserts the measured envelope, not the dissolved mechanism).
Exit: rb3-tests VerdictTable RUNS (PASS or a real measured FAIL — either is a valid exit),
good-body re-captured, curl assertion in Suite C.

_Landing sequencing: Lane N is the only engine writer — no ordering constraints. Lanes V/W/N
are file-disjoint in rb3 (V: R5-HANDS-ENDGAME/ + milo-trace; W: WHITE/WASH execution dirs +
scripts; N: native/tests/ + R2-FIXTURES/ + engine probe block)._

## Process rules (carried) — VERBATIM per KICKOFF_TEMPLATE.md

Locks: rb3 `/tmp/rb3-git.lock`; engine `/tmp/milo-engine-git.lock`; classjson
`/tmp/milo-engine-classjson.lock` (append-only, single coordinator regen at close-out); builds
`/tmp/rb3-native-build.lock` or own dir; milo-trace own norms. Checkpoints
`/tmp/wave18-checkpoints/<lane>.json` — check-first, write-before-return, update every
milestone. PLAN/STATUS per lane under `execution/<KEY>/`. Evidence committed under
`execution/<KEY>/evidence/` or it doesn't exist. New flags default-OFF; NO default flips, NO
pin bumps by lanes (coordinator, ONCE, at close-out). Refuted flags UNSET. ELEVEN defaults stay
ON. Headless `RB3_HTTP=1 RB3_FIXED_CLOCK=1`, free ports, frame-count settling, pgid-only
cleanup (NEVER pkill by name). Instance-scoped Dolphin (`-u` per-lane user dirs).

## Pre-dispatch checklist — the ten §4 lints

- [x] **1. Matrix-relative + pointer-verified bone claims** — V inherits D3/D4's join (relative
  products, pointer provenance); N's curl assertion cites the D4 FLOOR table rows.
- [x] **2. Split by gender/mesh/route** — V: gender split carried (D2 chain was male — capture
  both genders if reachable, D4's female-confound note applies); N: fixtures already split.
- [x] **3. No unvalidated oracles as gates** — V re-runs red/green join validation ON THE NEW
  capture; W's wash instrument must demonstrate known-good/known-bad separation before its
  numbers gate the WHITE re-grade; N's VerdictTable runs against known captures.
- [x] **4. Shipped-flag contradiction grep** — done for this kickoff: no lane claim contradicts
  the eleven defaults or the amended `RB3_HANDS_AUTHORED_REPOINT` row (engine `49ca0d6`).
- [x] **5. Wide read grants for diagnosis lanes** — V/W are measurement lanes with wide read;
  N is the narrow fix-shaped lane (engine probe block only).
- [x] **6. Option table before 2nd fix attempt** — no fix attempts this wave; V's branch
  execution stops at measurement (fixes are coordinator-dispatched with the branch letter).
- [x] **7. Evidence committed** — carried verbatim in every lane brief.
- [x] **8. Flag hit-counts on negatives** — W: any "guard has no effect" reading must carry the
  guard's branch-entry count; N: Tier-1 field emission count in the dump.
- [x] **9. Flavor-membership grep** — N/A for V (out-of-repo); W/N touch TUs already proven
  compiled into rb3-native (probe block, postproc).
- [x] **10. Instruments before fixes** — the wave's entire shape.

## Risks / open questions for the reviewer

- **R-A (Lane V):** is the visually-guided rig box realistic at one lane-wave given D5's
  evidence (Xvfb/xdotool/GPU present; ~8-screen nav; prompts not blind-clearable)? Should the
  lane try the HUB-BAND vignette route before full gameplay (fewer screens, no guitar-ext)?
- **R-B (Lane W):** the WHITE re-grade's prior gate sign-flipped between blind runs at Wave 10
  under RNG noise. With the ledger precondition, what N and what early-stop rule make the
  re-grade decisive rather than another HOLD?
- **R-C (Lane N):** the engine Tier-1 field — verify the declared probe-block range still
  resolves by symbol at `49ca0d6`, and that emitting per-block Tier-1 doesn't perturb the
  palette dump's existing consumers (D3/D4 scripts, R2 tests).
- **R-D:** V's "execute the classified branch's measurement step" — is GT-A palette forensics
  cleanly separable from fix territory, or should the wave stop at the branch letter?
