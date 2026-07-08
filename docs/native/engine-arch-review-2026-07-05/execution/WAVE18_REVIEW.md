# Wave 18 — Pre-dispatch review (Fable)

**Input:** `WAVE18_KICKOFF.md` (rb3 `bf79af2e`) checked against the live tree:
`R5-HANDS-ENDGAME/VERDICT.md` + `evidence/D5_findings.md`/`D5_exhaustion.json` (`09fc594d`),
`WAVE17_CLOSEOUT_REVIEW.md` (`a11b22a0`) + the landed correction commit (`b991c582`),
`R4-DETERMINISM/{STATUS,LEDGER}.md`, `PLAN-R4` §M4/§5, `PLAN-R2` §3.1,
`scripts/native/loaddet_gate.py`, engine `49ca0d6` (`Rnd_Wgpu_RB3.cpp` probe blocks,
`NativeCompatFlags.gen.inc`, classjson), `native/tests/{skinning_oracle.h,test_skinning_oracle.cpp}`,
`native/tests/goldens/r2-skinning/`, milo-trace `0bc0610`, `WHITE-fix/STATUS.md`,
`src/system/math/Rand.{h,cpp}`.

**Verdict: DISPATCH WITH AMENDMENTS.** The wave's shape is right and the V-over-GT-D call is
justified (§Decision below). Two amendments are blocking (A1, A2); the rest are lane-brief
corrections that prevent known failure modes, not redesigns.

Facts verified clean before the findings: engine pin `49ca0d6` is engine HEAD and carries the
amended `RB3_HANDS_AUTHORED_REPOINT` row; the W17 F1 record corrections DID land (`b991c582`:
D4_findings.md:133 correction block, R1-DOLPHIN/STATUS.md:227, ROADMAP.md:28) — the close-out's
"F1 lands first" precondition for the D5-branch lane is satisfied; milo-trace `0bc0610` exists
and is the pipe-nav instrument; the kickoff's D5 shell-state summary (START→18 drivers, A→8
resident clips, wall = guest-profile/YES/nav-help prompts, Xvfb/xdotool/GPU present, prompts not
blind-clearable) matches `D5_findings.md`/`D5_exhaustion.json` exactly; both hazard-note files
(`FxSendNative.cpp`, `rb3_session_trace.cpp`) are indeed modified-uncommitted right now; the
arm-w/arm-s env flags (`RB3_NO_HEAD_REBIND`/`RB3_NO_SKIN_CLAMP`/`SHARD_GUARD_OFF`,
`RB3_HANDS_SHELL_FIX`) are all still registered at `49ca0d6`, so the arm captures are possible.

---

## The open decision: funding Lane V instead of executing pre-authorized GT-D

**JUSTIFIED — and on stronger formal grounds than the kickoff states.** The kickoff justifies V
by "empirical cost-correction." The binding basis is the VERDICT's own closure-honesty
condition: VERDICT §3 — "GT-D closure is honest only when ground truth is *unobtainable in
budget* (PLAN §3.2); it is obtainable" — and PLAN-R5 §3.2's GT-D row, "No ground truth
obtainable in budget." D5's exhaustion report itself prices path 2 as **FEASIBLE on this box**
(`D5_findings.md` §Priced paths #2: Xvfb + GPU backend + xdotool all present; the identical
pattern already captured real gameplay for the C8 oracle, `t2-dolphin-oracle.md`). Executing
GT-D today would put a closure package on record whose honesty condition (gate G-C2) is
contradicted by its own evidence file. The VERDICT §4 pre-authorization removed the *permission*
requirement for closure; it did not mandate immediate execution — and falsifier #4 explicitly
names the hindsight-risk of closing while a cheap route exists. One bounded lane with a
doubly-bound stop is the campaign-consistent resolution. **Amend the kickoff to cite VERDICT §3
/ PLAN §3.2 as the basis** so the record shows GT-D was deferred *because its own honesty
condition failed*, not because the coordinator liked another lane.

---

## Amendments (ranked)

### A1 (BLOCKING — Lane V): the hard stop is not mechanically executable as written

**Kickoff claim:** "on exhaustion, report priced; the coordinator then executes GT-D closure …
and NO further discriminator lanes are dispatched" (WAVE18_KICKOFF.md:40-43).
**Evidence:** VERDICT §4 branch table defines the exhaustion row as "D5 box exhausted, **no
articulated capture**" — but the same table's power floor says "≥5 frames × ≥2 pairs, below it
= **indeterminate, not forced**," and VERDICT §0 says indeterminate → "re-price, never a
judgment call." A *partial* V outcome (3 matched frames; swing 9° < the 15° G-D5-1 floor; hub
sway-only capture) is neither "no capture" nor a branch letter — under the VERDICT's text it
reads "re-price," which reopens exactly the loop the hard stop exists to close ("we got 3
frames, one more lane gets 5").
**Correction (must be in the lane brief verbatim):**
1. *Exhaustion definition:* any end-of-box state without BOTH a G-D5-1-passing capture AND a
   G-D5-4 branch letter — **explicitly including partial captures graded indeterminate /
   power-floor misses** — is exhaustion for the purpose of the hard stop. The GT-D residual
   statement then notes the partial data; no re-price is dispatched.
2. *Exhaustion-report contract* (what GT-D needs to execute without ambiguity — D5's own report
   is the template): machine-readable JSON per `D5_exhaustion.json` + findings doc containing
   (a) every shell/gameplay state reached, with screenshots; (b) the oracle-independent
   articulation span at each state; (c) if capture was attempted, the failing stage + its price;
   (d) the explicit line "no branch letter assignable under G-D5-4"; (e) gate-status table
   G-D5-1..4. The coordinator's closure package then = PLAN §3.4 + the VERDICT §8.3
   clamp-corrected residual (the kickoff already carries §8).
3. State the ε_noise mechanism for gameplay capture (G-D5-2 needs ≥3 captures at one held
   `(clip, frame)`): Dolphin pause/savestate-reload — a moving song clip cannot deliver it live.

### A2 (BLOCKING — Lane W): the F6 ledger precondition is NOT carried verbatim, and the wording change is material

**Kickoff claim:** "must PASS the per-axis ledger … before its sample counts — a boot that fails
the ledger is **discarded and rerun**, never averaged in" (WAVE18_KICKOFF.md:48-51).
**Evidence:** PLAN-R4 §M4 item 1: "**Validity precondition: the ledger must PASS 10/10 on these
exact boots** — otherwise the re-grade is **void**"; gate G5 (PLAN-R4 §5): "harness must
**refuse/mark the re-grade invalid**." Close-out F6 (WAVE17_CLOSEOUT_REVIEW.md:123-133): the
charter "MUST carry both preconditions **verbatim**."
**Why it matters:** discard-and-rerun converts a validity gate into a retry loop. A ledger FAIL
under the M3-proven protocol means the *seam regressed* (or the WHITE nav protocol breaks it) —
that is a reportable finding, not noise to rerun past. Worse, if ledger failure correlates with
the RNG-adjacent phenomenon under study, silent discard biases the sample the gate was designed
to protect.
**Correction:** restore M4.1 verbatim (ledger PASS 10/10 on the exact graded boots; FAIL voids
the batch); allow at most a bounded rerun budget with mandatory reporting (e.g. >1 voided boot
per 10 → lane STOPS and files a seam-regression finding with the failing axis + counts — this
also satisfies checklist lint 8's "hit-counts on negatives").
**Plus a missing work item the wording papers over:** per-boot ledger attachment **does not
exist yet**. `loaddet_gate.py` runs its *own* input-free boots (`run_arm`/`boot_measure`,
loaddet_gate.py:66,170) — it cannot grade boots produced by `white_ab.py`/`wash_matrix.py`. The
lane brief must name the integration: run the measurement boots with `RB3_LOADDET_ATTRIB=1`
(the ledger consumes attrib-capture frames/completes — `ledger_for_arm` docstring,
loaddet_gate.py:270-272) and either extend `loaddet_gate.py` with a grade-external-logs mode or
embed `ledger_for_arm` in the WHITE/wash harness. Scripts are in W's writable set; the work
just has to be scoped, or the lane will run the ledger on *separate* boots and claim the
precondition met — the exact "these exact boots" violation M4.1 bans.

### A3 (HIGH — Lane W): the seam pins ONE RNG trajectory; pre-register the substrate-blocked outcome

**Kickoff gap:** the charter treats `RB3_LOAD_DETERMINISM=1` as a pure noise-removal. It is
not: the seam **replaces** the natural stream (hardcoded `gRand.Seed(0x5EED)`,
src/system/math/Rand.cpp:61 — **no seed knob exists**) and reroutes the WHITE/wash-implicated
consumers (`RndParticleSys::InitParticle` was the *dominant* divergent consumer, R4 STATUS §M1)
onto private per-tag streams. Consequences the brief must state:
1. **No comparison to Wave-10/11 absolute numbers.** Both guard arms are re-run fresh under the
   seam (M4.1 already prescribes both-arms N=10; say explicitly that the Wave-10 HOLD numbers
   are not a comparand — different stream by construction).
2. **Reproduce-first.** The WHITE re-grade is meaningful only if the phenomenon *expresses on
   the pinned trajectory*. The WHITE-fix reproducer-validity gate (`mean(hi_frac|OFF) ≥ 15`,
   WHITE-fix/STATUS.md:321) doubles as this check — run it FIRST. If it fails, the outcome is
   **HELD: substrate-blocked ("phenomenon not expressible on the pinned stream")**, NOT "guard
   has no effect," and the named follow-up is an `RB3_LOADDET_SEED` knob — an engine/src edit
   **outside Lane W's writable set**, so the lane cannot self-rescue; it reports and stops.
3. (Cheap, makes the A/B airtight) verify **cross-arm** stream match once: guard-ON vs
   guard-OFF ledger against the same reference boot — the guard is render-side and should
   consume no gRand; proving it turns the A/B into a same-trajectory paired comparison.

This plus A2 answers **R-B**: N=10/arm per M4.1; decision rule = the existing pre-registered
G1a (`mean(hi_frac|ON) ≤ OFF − 3.0pp` AND directional, WHITE-fix/STATUS.md:303) — do not invent
a new one; early stop at N=5/arm permitted ONLY if within-arm hi_frac sd < 0.5pp in BOTH arms
AND ledger 5/5 (stream-matched boots should be near-deterministic; if they are, Δ is a point
measurement and N=10 adds nothing). With A3's two named HELD outcomes (reproducer-invalid,
ledger-void), every other result is decisive by construction — a third blind HOLD is
structurally impossible.

### A4 (MEDIUM — Lane N): the Tier-1 field spec omits the two facts that caused the SKIP in the first place

**Kickoff claim:** "Engine-emitted Tier-1 field in the `RB3_PALETTE_DUMP` sidecar … flips the
VerdictTable gtest from honest SKIP to a real arm-W table reproduction" (WAVE18_KICKOFF.md:58-61).
**Evidence:** R2 STATUS.md:46-57 — the offline `M_Tier1RestCoherence` (off × *current* world)
reads ~180° at gameplay and never 28.9°; the Wave-15 numbers come from the engine xcheck, which
composes `Multiply(off, haRest[b])` against a **first-seen cached rest world** with pointer-
freshness recapture (Rnd_Wgpu_RB3.cpp:4820-4841, `sHaRest`/`sHaPtr` + `t1Recap`). That cache
lives inside the **RB3_HANDS_ATTACH-gated block**, not the dump block.
**Correction (spec, three parts):**
1. The emitted value must be the **xcheck quantity** (off × cached first-seen rest world, plus
   the `t1Count`/`t1Worst`/`t1Recap` triple), NOT `off × WorldXfm()` — the latter re-derives the
   offline artifact and the test still won't reproduce. The dump block must either replicate a
   local rest-cache or declare the env coupling (require the attach probe ON in the capture env
   and emit a LOUD cold-cache marker when `t1Recap == nb`).
2. Flipping the SKIP requires more than the field: **capture + commit `arm-w`
   (`RB3_NO_HEAD_REBIND=1 RB3_NO_SKIN_CLAMP=1 SHARD_GUARD_OFF=1`) and `arm-s`
   (`RB3_HANDS_SHELL_FIX=1`) fixture sets** (PLAN-R2 §3.1 — both env sets verified still
   registered at `49ca0d6`) AND **editing `VerdictTable.ArmWArmSReproducedFromFixtures`** to
   read the emitted field instead of computing `M_Tier1RestCoherence`
   (test_skinning_oracle.cpp:427-448). All in-scope (native/tests is writable) — but absent
   from the brief, the lane ships the field and the test still SKIPs.
3. The kickoff's exit ("PASS or a real measured FAIL — either is a valid exit") is correct and
   should stay: if the recapture doesn't reproduce 0.1/3.1/28.9, that is a finding, not a
   recalibrate-until-green license.

### A5 (MEDIUM — Lane N): format constraint + consumer list correction (answers R-C)

- The probe block is at **Rnd_Wgpu_RB3.cpp:5003-5106** at `49ca0d6`, not ~:4736 (~270-line
  drift). The kickoff's re-derive-by-symbol instruction is correct and necessary — anchor on
  the `"RB3_PALETTE_DUMP (R2 skinning fixtures"` banner; fix or drop the stale number.
- **The new field must be a NEW header-key line** (`tier1 …`). `LoadFrameImpl` silently ignores
  unknown header keys (test_skinning_oracle.cpp:58-99), so old goldens and the new format
  coexist. It must **NEVER extend the existing `bone`/`v` lines** — the strict field-count
  sscanf (`c != 43` / `c != 11` → reject) would invalidate every committed golden at load, the
  G1-strictness working as designed against the wrong target.
- Consumer list correction: **D3/D4 scripts do not read palette dumps** —
  `interbone_framematch.py` and the sweep tooling consume bone-probe JSONs. Actual consumers:
  `LoadFrameImpl` (rb3-tests) + `skinning-fixture-capture.py` provenance. R-C's perturbation
  worry is therefore narrower than stated, and passes once the header-line rule above is kept.
- "default-OFF, classjson append under lock": the field rides the already-default-OFF
  `RB3_PALETTE_DUMP`; what changes is the row's format description — amend the existing row
  (under the lock, single coordinator regen at close-out), don't add a phantom new flag row.

### A6 (MEDIUM — Lane V, answers R-D): stop at the branch letter; GT-A forensics is not V's box

**Kickoff claim:** V "execute[s] the classified branch's next step ONLY if it is measurement
(GT-A palette forensics on R2's dumps, G5′ gates)" (WAVE18_KICKOFF.md:36-37).
**Evidence:** (i) VERDICT §4 GT-A row prices the forensic step as "**One hard-boxed §3.5
forensic lane**" — its own 0.5-1 lane-wave item, not a tail on an already-spent box; (ii) G5′
leans on the R2 `good-body` dumps whose provenance the close-out graded weakest (F3) and whose
re-capture is **Lane N's item (2) in this same wave** — W17 close-out: "re-capture good-body …
**before any Wave-18 lane leans on it**." That is a V→N ordering dependency the kickoff's
"file-disjoint, no ordering constraints" landing note (WAVE18_KICKOFF.md:68-70) denies.
**Correction:** V's exit is the branch letter + committed capture artifacts + the re-validated
join (G-D5-1..4). GT-A palette forensics (and any other branch's measurement step) is
coordinator-dispatched after N's good-body re-capture lands. This also keeps V's box honest for
A1's hard stop — a box that must fit rig-build + capture + join + forensics is a box designed
to exhaust.

### A7 (LOW — Lane V, answers R-A): hub-vignette first, but budget for gameplay from the start

The box is realistic at one lane-wave **given A6**: the visually-guided pattern is proven on
this exact box (t2-dolphin-oracle captured real gameplay; Xvfb/xdotool/GPU verified present by
D5), and the new work reduces to prompt-flow nav with sight + a guitar-extension mapping +
capture-at-pause. Sequence: try the hub-band vignette FIRST (it is the landing state directly
behind the prompt wall D5 mapped — nearly free once prompts clear), but gate it with the
committed oracle-independent articulation detector and timebox it (~20% of box): D5 priced-path
3 says hub finger-articulation is **UNCONFIRMED** ("may be idle body sway only") and gameplay is
the surest source. Configure the guitar-extension mapping at rig-build time, not as a fallback
scramble. Note the swing floor: hub sway that moves fingers 3-9° still FAILS G-D5-1 (≥15°) —
that is A1's "partial capture = exhaustion" case, not a success to argue about.

### A8 (LOW — Lane N): F3 has two halves; the kickoff carries one

Close-out F3 requires the good-body re-capture to fix BOTH gaps: legacy provenance (no
`arm_env`/`palette_dump_env`) AND the byte-identical screenshots (all four committed shots are
one repeated md5). Add to the brief: per-frame screenshots must be md5-distinct fresh captures,
and commit the M1 Tier-1 table as a small JSON (currently prose-only — close-out F3's last
line).

### A9 (LOW — Lane N): the curl assertion's stated home (Suite C) is a category error

Suite C is the SYNTHETIC boot-free suite (test_skinning_oracle.cpp:5-11); the D4 FLOOR envelope
(middle/ring 14-41°) is a real native bone-WORLD sweep measurement. A synthetic Suite-C test can
encode the curl *structure* (1-D family) but not assert the *measured envelope*. Name the real
home: a fixture-based assertion over committed articulated hand fixtures (Suite A/B,
`M_InterBoneRelPoseWorld`) or a script-level check over the committed D4 JSON. Keep the D4
CORRECTION framing (assert the measured envelope, not the dissolved mechanism) — the kickoff
already has this part right.

### A10 (LOW — record): "Wave 18" is now two dispatches

D5 already ran stamped Wave 18 (`D5_exhaustion.json` `"wave": 18`, task #102) before this
kickoff. Add one line to the kickoff header (D5 = interim wave-18 single-lane dispatch;
this document = the wave-18 main wave) so the README wave table doesn't double-count or
misattribute D5's box to this wave's budget.

---

## Answers to the kickoff's R-A..R-D

- **R-A:** Yes, realistic — conditioned on A6 (no forensics in the box) and A7's sequencing
  (hub first, timeboxed, detector-gated; guitar-ext configured up front; gameplay is the
  budgeted main line).
- **R-B:** N=10/arm per PLAN-R4 M4.1 with the verbatim ledger validity gate (A2); decision rule
  = the pre-registered WHITE-fix G1a, unchanged; early-stop at N=5/arm only under near-zero
  within-arm spread + ledger 5/5 (A3). Decisive-by-construction: the only non-decisions left
  are the two *named* substrate outcomes (reproducer-invalid, ledger-void), each with a
  designated follow-up.
- **R-C:** Block re-derives at :5003-5106; emission is safe for all real consumers IFF it is a
  new header-key line (A5); the deeper coherence risk is semantic, not syntactic — the field
  must be the :4820-4841 xcheck quantity with the rest-cache/flag-coupling decision made
  explicitly (A4). D3/D4 scripts are not consumers.
- **R-D:** Stop at the branch letter (A6). GT-A forensics is the VERDICT's own separately-boxed
  lane and depends on Lane N's F3 re-capture landing first.

## Checklist self-grade audit (the ten §4 lints)

Honest on 1, 2, 5, 6, 7, 8, 10. Qualified on three: **lint 3** — W's re-grade gate (G1a) is
validated, but the charter must add A3's reproduce-first step or the "validated oracle" is
being run on a substrate where the phenomenon may not express; **lint 4** — the grep is real
(`49ca0d6` row verified amended; F1 corrections verified landed at `b991c582`), pass; **lint 9**
— "N/A for V" is fine, but for N note the graded surface includes `native/tests` edits
(rb3-tests link, proven) *and* the capture protocol's env arms — the lint's spirit (prove the
code you measure is the code that runs) applies to the arm envs, satisfied by the still-
registered flags check above. No checklist item is falsely claimed.

## Dispatch verdict

**DISPATCH WITH AMENDMENTS.** A1 and A2 must be edited into the lane briefs before dispatch
(both are one-paragraph insertions; neither changes the wave's shape). A3-A6 belong in the lane
briefs as written constraints; A7-A10 may ride as coordinator notes. The V-over-GT-D decision is
approved on the VERDICT §3 / PLAN §3.2 closure-honesty basis and should be recorded as such.
