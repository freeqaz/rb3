# Wave 18 — Close-out review (Fable)

**Input:** lane STATUS/evidence for V (VISCAP, rb3 `5655f499`, milo-trace `1167f42`),
coordinator GT-D CLOSURE (rb3 `5d1cc39f`), M (R5-MITTEN, engine `4b8a809`, rb3 `37884a7b`),
W (R4-M4, rb3 `f35f41a2`), N (R2-NET, engine `e69a35f`, rb3 `10b7caea`); coordinator E1 flip
(engine `403ff00`) + close-out commit (rb3 `0f76bc60`); checked against `WAVE18_KICKOFF.md`
acceptance A0–A10, `R5-HANDS-ENDGAME/VERDICT.md`, `R4-DETERMINISM/LEDGER.md`, PLAN-R4 §M4,
PLAN-R5 §3.4, the committed evidence JSONs/PNGs, and the live engine tree at `403ff00`.

**Overall verdict:** the wave's three big calls stand — **V's exhaustion is contractually
valid and the GT-D closure is honest now**, **N is clean end-to-end**, and the **E1 mitten
flip is defensible on its evidence**. Two lanes' *self-grading* does not fully survive
adversarial re-derivation: Lane W's headline "wash co-sampler VALIDATED" is refuted by its
own committed JSON (F1 — the one finding that must not reach the README as stated), and the
mitten flip leaves registry/record inconsistencies plus an unrecorded residual (F4/F5).
Nothing here reopens a closed family or requires a code revert.

---

## Verdicts on the scoped questions (before the findings)

**Lane V / A1 contract — SATISFIED; "no branch letter assignable" follows.**
`V_exhaustion.json` carries every A1 element: states reached w/ screenshots (`V_nav_*.png`,
`V_gameplay_*.png`), oracle-independent articulation spans per state (hub 0.00°/10.5 s ==
D2/D4 bind; gameplay 0/992 movers, span 0.0°), failing stage + price (`priced_paths_forward`),
the explicit sentence (`branch_letter`: "NONE - no branch letter assignable"), the G-D5-1..4
gate table, and the ε_noise-via-pause mechanism honestly N/A'd (no articulated frames to
repeat). G-D5-1 fails everywhere (0° < 15°), the power floor is unmet, so the VERDICT §4
fallback row applies mechanically — including under the A1 "partial/indeterminate counts as
exhaustion" clause, which never even triggers (there is no partial capture). The strongest
alternative reading — *stale bone-address list makes 0 movement trivial* — is excluded in the
instrument itself: `wii_visgame_capture.py cmd_gpmotion` re-scans guest RAM for CharBone VT
instances FRESH at every sample during live gameplay (`allpos()` calls `find_u32_addrs` per
sample), and the same read returns the D2/D4-validated bind pose while MEM2 provably mutates.
Corroborants (exo/target/head/jaw/spine static, CharServoBone 0 resident, native D3 animated
54–99° in the identical representation) close the subclass-VT / wrong-representation outs.
One record note (LOW, F8): the lane checkpoint claimed the normal-speed confound scan showed
0/992; the committed findings conservatively downgraded it to INCONCLUSIVE (black loading) and
rest the decision on the unlimited-speed gpmotion run — the committed record is the correct,
weaker claim; no action.

**GT-D closure honesty — GENUINELY MET NOW.** The condition that was correctly rejected twice
("ground truth unobtainable in budget", VERDICT §3 / PLAN §3.2) is now met on evidence: D5
exhausted the blind path; V built the priced visually-guided path, SOLVED the nav wall, and
proved the substrate wall one level deeper (no reachable state, incl. confirmed-live gameplay,
drives band CharBone animation). Falsifier #4's hindsight risk is answered by the pre-registered
reopen condition (≥5 matched frames, mr-swing ≥15°, live clip+frame key; `v18_hand_classify.py`
committed) — well-posed and mechanical. The residual statement carries the §8.3 clamp
correction verbatim (`RB3_NO_SKIN_CLAMP` is NOT a hands mitigation, with the engine/rb3 cites).
Record hygiene landed: ROADMAP R5 row CLOSED (`RETROSPECTIVE/ROADMAP.md:28`), HANDS-FIX
stub-claim annotated (`HANDS-FIX/STATUS.md:100-107`, also carries the §8.4 do-not-cite),
dead-cell registry rows (`RB3_HANDS_AUTHORED_REPOINT`, `RB3_HANDS_RESKIN`) now point at
CLOSURE.md — note these pointers landed in the `403ff00` wave-end regen, not `49ca0d6`.
Two closure-text follow-ups are in the findings: the now-resolved E1 conditional (F5a) and the
"no mesh tearing on the shipped default" clause vs Lane M's own OFF evidence (F5b).

**Lane N / A4-A5 — HONORED, clean.** The emitted value IS the `:4820-4841` xcheck quantity:
`off_b × FIRST-SEEN cached rest world` via a local pointer-identity-fresh rest cache replicated
in the dump block (engine `e69a35f`; emit at `Rnd_Wgpu_RB3.cpp:5285` at `403ff00`), with the
LOUD `cold=1` marker A4 demanded — explicitly NOT off×WorldXfm (the 61.95° offline contrast on
the same arm-s frames is the demonstration). It is a NEW header-key line only; `bone`/`v` lines
untouched; the loader tolerates unknown keys, and the flipped test hard-REQUIRES the field
(legacy goldens fail loudly rather than silently passing on the offline metric) — the right
shape. First real reading reproduces Wave-15 (0.06/3.13 male, 28.88/28.92 female vs
0.1/3.1/28.9) as a measured PASS. The `SHARD_GUARD_OFF` finding is real and verified in-tree
(`Rnd_Wgpu_RB3.cpp:4494`: `!SHARD_GUARD_OFF || SHARD_RATIO_DBG` gates the whole diagnostic
block incl. the dump; `guardActive=false` keeps the DROP disabled, so arm-w render behavior is
preserved while dumping — and it explains why Wave-15's arm-w numbers were never
dump-reproducible). The CWD test fix is sound: `GoldenRoot()` resolves env →
`__FILE__`-derived (chdir-immune) → walk-up from a static-init (pre-chdir) CWD → relative
(`test_skinning_oracle.cpp:222-249`); this addresses both named hazards (boot-test `chdir`,
ctest launching from the build dir). F3 both halves done (md5-DISTINCT shots ×6 with the
black-readback root cause named; `M1_tier1_table.json` committed); A9 curl assertion landed in
the real-fixture suite over the committed D4 JSON with the CORRECTION framing. Lane V's GT-A
follow-up is correctly recorded as unblocked.

**Lane W / A2 VOID semantics — HONORED in the harness and in the verdict.**
`grade_external_logs`/`--grade-logs` grades the EXACT measurement boots' own logs
(`loaddet_gate.py:180-186,404-415`), and `white_regrade.py:204-250` refuses a WHITE verdict
unless stream N/N AND nParsed==N — the OFF arm's 1/10 correctly produced VOID, the failing
boot was neither discarded-and-rerun nor allowed to feed a verdict, and the +16 was attributed
to named venue-path consumers (the A2-mandated seam-regression-style finding, with lint-8
counts). The ON-arm discrimination (10/10 stream-matched yet hi_frac 9.5–65.4) is real and IS
the sharpest result of the wave: a non-gRand axis with a named owner. **But** the lane's other
headline — the wash co-sampler "VALIDATED (lint 3), AUC 0.000, WHITE frames are
particle-LULLS" — does not survive contact with its own evidence file (F1), and the N=10
evidence has provenance gaps (F2) plus an attribution arithmetic slip (F3). The
`InitParticle [2505,1755,2509]` reading itself is fine as far as it goes (real addr2line-attributed
per-boot counts on the private `part` stream, `wr_validate` boots), but note what it is
evidence OF: the ~30% swing co-occurs with the render-timeline-length swing (`[WASHPROBE]
SCENE engaged` 19898–27938; songMs 21003 landing at frame 3585 vs 5095 across boots), so
"emission COUNT/timing varies" and "timeline length varies" are one observation, not two — the
correct owner statement is the frame-assignment/timing axis (see F9), and the R4 ledger's
`order` axis (loader completion SEQUENCE, 10/10 PASS at boot) must not be conflated with it.

**Lane M + E1 flip — flip stands; adversarial answers:**
(a) *Trigger population*: 16.4% of finger-draws (`evidence/trigger_rate.txt`, monotone
convergent over 1.35M draws) with relAng p50=21.8/p90=62 — TH_LO=45 sits ≈p84, so the trigger
mass matches the distribution tail arithmetic. Calling 16% "the extreme-pose tail" is generous
phrasing, and the ramp is calibrated against the SAME distribution it then triggers on (mildly
circular): nothing independently establishes that all relAng>45° poses are displaced/torn
rather than legitimately curled (a correct fist/fret grip lives at 45–90° to the wrist). The
coherent-frame no-regression claim (α=0 below 45°) is genuinely by-construction; the risk the
E1 pairs would NOT show is *articulation loss on legitimately-curled un-torn hands* — accepted
by the closure's residual statement, but E1 coverage of it is thin (F5c).
(b) *Cross-process nondeterminism* (OFF-vs-OFF 48–93%, median 85% pixels): NOT a red flag —
it is independently corroborated by Lane W's stream-matched render-timeline variance (SCENE
frame swing, emission-count swing at PROVEN-matched gRand). It is, however, an UNPINNED-protocol
measurement: the bursts ran without the campaign's own R4 seam/fixed-clock discipline
(`frame_pairing_note.txt` cites director cuts + clip phase), and a seam-pinned matched-frame E1
was neither attempted nor priced (F6). Given Lane W proved variance persists even
stream-matched, the visual pose-matched E1 (the VERDICT G-C1-sanctioned gate class) is
acceptable; the two records should cross-link.
(c) *Flip shape*: verified at `Rnd_Wgpu_RB3.cpp:3727-3731` — `RB3_HANDS_MITTEN_OFF` presence
wins, legacy `RB3_HANDS_MITTEN=0` disables, else ON; disabled ⇒ block inert. Correct opt-out
shape, matches the other eleven.
(d) *Crowd/extras exclusion*: verified — `IsBandHandMesh` rejects `crowd`/`extra` substrings
before matching hands_naked/gloves/fingernails (`native/src/rb3_render_hook.cpp`), wrist role
requires `-hand` followed by `.`/EOS (prop bones `_fret/_mic` excluded), and
`scope_confirmation.txt` shows band `skeleton_unshared.milo` wrists only, `rebound=1`. The 24×
crowd rebind is untouched (engine keeps the math; the classifier is the only membership gate).
(e) *Drummer + top-center floating structure*: NOT covered — see F5. No E1 pair isolates the
drummer's hands (bursts: guitarist, 2× wide, male singer, female singer/drumstick), and the
persistent pale floating structure top-center in `OFF_burst_08.png` is STILL PRESENT in
`ON_burst_08.png` (unchanged by the mitten, as expected for non-finger bones) yet is recorded
nowhere: CLOSURE.md's residual ("hand stays attached and moving") reads straight over it.

**Close-out mechanics:** pin `49ca0d6→403ff00` in `native/CMakeLists.txt` (`0f76bc60`) with
ledger regen 372→377 rows; gates asserted on the final pin (drawlog 792 canonical PASS, lineup
PASS, rb3-tests 116/0 + 7 env-gated SKIP) — plausible and consistent with both lanes' own runs,
though note the canonical drawlog golden (splash_screen) cannot exercise the mitten (no hand
meshes at splash), so the default-ON gate for it is the visual E1 + trigger-rate pair, not the
drawlog. TWELVE defaults ON is arithmetically right; the registry does not yet SAY so
consistently (F4).

---

## Findings (severity-ranked)

### F1 (HIGH — Lane W): "wash co-sampler VALIDATED" is refuted by its own committed evidence; AUC 0.000 is a tie-handling artifact

**Claim:** "lint 3 (known-good/known-bad separation) — PASS … `fx_emit_win` mean_bad 1290 vs
mean_good 23264, AUC=0.000, p=0.058 — a clean inverse separation (WHITE frames have LOW
particle emission)" (`R4-M4/STATUS.md:64-79,148-153`).
**Evidence against, from `evidence/wash_natural.json` itself:** the 89 "per-frame co-samples"
contain exactly **2 distinct covariate clusters** — every shot carries one of two
`(songms,frame)` metadata points (20944.5/5019 + one other) and `fx_emit_win ∈ {1290, 63425}`,
`light_changes_win ∈ {97, 65}`. All 7 BAD frames have fx=1290 — **and so do 53 of the 82 GOOD
frames** (arithmetic: 53×1290 + 29×63425 = 82×23264.6). There is no separation: the majority of
known-GOOD frames share the identical covariate value with every known-BAD frame.
**Why the reported AUC is 0.000 anyway:** `wash_cosample.py:118-130` computes ranks via
`allv.argsort(); ranks[order]=arange(...)` — ties get arbitrary distinct ranks (no midranks),
and with bads occupying the first indices among 60 tied values they receive the minimal ranks,
driving AUC to 0 artifactually. Tie-corrected (midrank) AUC ≈ 0.32. The identical
`mannwhitney_p = 0.0579463…` for BOTH signals is the same degeneracy showing through (identical
two-cluster group structure), not two independent marginal results.
**What actually happened:** the covariate join is per-burst, not per-frame — the engine-log
poll returned one stale window value for an entire screenshot burst, so the instrument produced
two effective samples, not 89.
**Correction (must land before the README):** (1) strike "VALIDATED"/lint-3 PASS from
`R4-M4/STATUS.md` and the wave summary — the honest grade is **instrument BUILT, join
BROKEN, validation NOT demonstrated**; (2) strike the "WHITE frames are particle-LULLS →
swept-light sub-axis" inference (it rests entirely on the artifact); (3) co-sampler v2's charter
= fix the per-frame join (covariate must advance with the frame counter; assert ≥N distinct
window values per run) + midrank AUC/U — BEFORE adding the light-POSITION signal. The
`light_changes_win` non-separation (AUC 0.42) is equally void — same broken join.
Note what F1 does NOT touch: the WHITE re-grade VOID/HELD verdict, the venue-path consumer
attribution, and the ON-arm stream-matched-spread discrimination all stand on independent
evidence (`wr_n10*.json`, attrib logs).

### F2 (MEDIUM — Lane W): N=10 re-grade evidence provenance — recovered rows, unverifiable capture property, black frames counted as lighting data

**Claims:** "first-frame-crossing capture … screenshot the FIRST frame with songMs >= target
(one-sided, tol=0)" (`R4-M4/STATUS.md:35-37,89-91`).
**Evidence:** every one of the 20 rows in `evidence/wr_n10.json` has `"frame": null,
"songms": null` — the committed run was reconstructed via the `--refinish` crash-recovery path
(`white_regrade.py:119-142,192-195`), which rebuilds rows from on-disk PNGs/logs and loses the
per-boot songMs provenance. So the one-sided-window property, the very confound-removal the
lane advertises, is **not verifiable from the committed evidence** (it IS verifiable in
`wr_validate.json`, whose 3 boots carry frame/songms). Additionally: (a) `capture_arm` retries
up to 4N with silent skips (`white_regrade.py:86-92`) and the overshoot rule "discard+retry if
songMs passes target+overshoot" discards boots whose *load timing* ran long — a mild
survivorship bias on exactly the axis under study; attempt counts are not disclosed; (b)
OFF_01 (and 2 of 3 validate boots) are `mean_luma 0.0` all-black frames scored as
`hi_frac 0.0` NEARBLACK — black readback/loading frames are not lighting measurements, yet
they enter the hi_frac means (the code already excludes them from mid_sat, `white_regrade.py:218`);
the OFF mean 12.43 (used for the reproduce-first reading `12.43 < 15`, STATUS:122) becomes
13.81 excluding the black boot — the conclusion (still <15, substrate-blocked) is unchanged,
but the number as printed mixes a capture failure into a photometric statistic.
**Correction:** one STATUS paragraph disclosing the refinish provenance + attempt counts;
exclude luma≈0 frames from hi_frac stats (mirroring the mid_sat NaN rule) and restate the
reproduce-first line with both numbers; keep the VOID/HELD verdict (unaffected).

### F3 (MEDIUM — Lane W): the +16 attribution is over-claimed as "fully" and the two documents disagree on the +1 term

**Claims:** STATUS.md:103-106 attributes the +16 to "`CharClipDriver`(8) +
`WorldCrowd::OnIterateFrac`(7) + `CharInterest::ComputeScore`(1)";
`evidence/venue-path-divergent-consumers.md:16-23` tables CharInterest as **[4,5,4]**
(a −1 term, anti-phase) and LightPresetManager as [1,0,1], summing "8 + 7 + 1 = 16" via
**LightPresetManager's** 1.
**Evidence:** per-boot deltas boot0−boot1: +8 +7 −1 +1 = **+15**, not +16 — the residual ±1 is
unexplained by the four named consumers, and the STATUS names the wrong consumer for the +1.
**Correction:** reconcile the two docs (LightPresetManager owns the +1; CharInterest is a −1
anti-phase term), and downgrade "fully attributed/fully explained" to "explained within ±1
draw". The follow-up list (isolate the 4 venue-path consumers) is unaffected — all four are
proven-live un-isolated consumers regardless of the sum's last digit.

### F4 (MEDIUM — close-out/registry): the mitten flip is not consistently recorded in the flag registry, and the default-ON-workaround census misses it

**Claims:** "TWELVE defaults now ON" (`0f76bc60`); registry regen at `403ff00`.
**Evidence:** (a) `NativeCompatFlags.classification.json` / `gen.inc:167` `RB3_HANDS_MITTEN`
row still carries `"default": "off"` (the prose prefix says LIVE default-ON — the
machine-readable field contradicts the code at `Rnd_Wgpu_RB3.cpp:3727-3731`); (b) unlike every
other shipped default's `*_OFF` row, `RB3_HANDS_MITTEN_OFF` (gen.inc:168) is
`unknown/unclassified/"n/a"` — the convention that encodes "feature ships ON" is half-applied;
(c) consequently `NATIVE_COMPAT_LEDGER.md`'s census line "**Default-ON workarounds … : 70**"
did not change at the flip — the mitten is a default-ON workaround counted nowhere (the number
the ledger says "§W5.3 must drive to 0" is now understated by 1); (d) the row and the code
comment (`Rnd_Wgpu_RB3.cpp:3739`) bake in stale calibration numbers — "p50~17 p90~54, trigger
~17.7pct" — vs the committed evidence `relang_distribution.txt`/`trigger_rate.txt`:
**p50=21.8, p90=62, max=134.1, rate=16.4%**; (e) the row body still says "Default is
E1-DECIDED by the coordinator, NOT this lane" and "flag-OFF (unset) byte-identical" — both now
stale/inverted post-flip (unset = ON).
**Correction (single classjson edit + regen):** classify `RB3_HANDS_MITTEN_OFF`
(workaround / skinning/hands, faithfulStatus pointing at CLOSURE.md item 1 + the E1 flip),
update the MITTEN row's stale clauses + numbers to the committed evidence, and fix the census
to count it (70→71 or via the newly-classified _OFF row).

### F5 (MEDIUM — closure/E1 record): three residual-statement gaps

(a) **The E1 conditional is now resolved but the closure still reads as a hypothetical.**
`CLOSURE.md:56-58`: "If `RB3_HANDS_MITTEN` ships default-ON (E1-decided), the residual
becomes…" — it DID ship ON (`403ff00`). One line amending the closure (or a dated postscript)
should fix the governing residual to the mitten form, citing the flip sha.
(b) **"no mesh tearing on the shipped default" is contradicted by the wave's own OFF-arm
evidence.** The closure residual (`CLOSURE.md:53-55`) asserts the pre-mitten shipped state has
"no mesh tearing"; Lane M's E1 evidence describes and shows the OFF arm "torn into thin
spikes" / spike-fans (`R5-MITTEN/STATUS.md:40-43`, `OFF_burst_45.png`, `OFF_burst_08.png`).
Whether "spike-webbing" is or isn't "tearing" is not a distinction a user (or the next wave's
reader) will draw. Reword to "no *skinned-blend* tearing of the Wave-16-repoint class; the
seed-R spike-fan/webbing at extreme poses is the residual itself".
(c) **Unrecorded out-of-family residual + E1 coverage gap.** The persistent pale floating
structure top-center of `OFF_burst_08.png` is unchanged in `ON_burst_08.png` — correctly
untouched by the mitten (not a finger-bone blend; lineage is the Wave-9/10 floating-forearm
triage family, `WAVE10_KICKOFF.md:75,94-95`), and the DRUMMER's hands are not isolated by any
E1 pair. Neither fact is recorded in CLOSURE.md, R5-MITTEN/STATUS.md, or the flip commit. As
it stands, the next observer who sees the floating structure will file it against the closed
hands family. **Correction:** add one recorded residual line (closure "what IS established"
section or R5-MITTEN followups) naming the persistent non-finger floating structure (with the
burst_08 pair as evidence and the W10 lineage pointer) and the drummer-coverage caveat.

### F6 (LOW — Lane M): the nondeterminism control ran unpinned; cross-link it to Lane W

The OFF-vs-OFF 85%-median pixel-diff control (`frame_pairing_note.txt`) ran without
`RB3_FIXED_CLOCK`/`RB3_LOAD_DETERMINISM`; the conclusion "cross-run pixel diff cannot gate this
class" is nonetheless correct — Lane W independently proved the render timeline varies even on
PROVEN stream-matched seam boots (SCENE-engaged swing 19898–27938, emission-count swing). The
two records should cite each other (one line each), and any future attempt at a pixel-gated E1
should be priced against the seam + a director-camera pin, not re-derived from scratch.

### F7 (LOW — Lane W): "NOT averaged in" vs the printed OFF mean

`R4-M4/STATUS.md:95` prints OFF hi_frac "12.43 (7.76)" — computed over all 10 boots including
the ledger-failing black boot (recomputed: mean 12.43, sd 8.18 — the printed sd is also off).
Under VOID these are descriptive-only, so no verdict is tainted, but the adjacent sentence
"the failing boot was … NOT averaged in" (STATUS:101-103) is literally false for the table.
Tighten the wording (and fix the sd) when applying F2.

### F8 (LOW — Lane V): checkpoint-vs-findings discrepancy, resolved conservatively — record as precedent

`/tmp/wave18-checkpoints/V.json` S4 claims the normal-speed confound scan measured 0/992;
`V_findings.md:70-76` downgrades that run to INCONCLUSIVE (black loading) and rests on the
unlimited-speed gpmotion run. The committed record is the honest one; noting it here so the
stronger checkpoint claim is never resurrected. No action.

### F9 (LOW — Wave-19 scoping): "completion-order" naming collision

Lane W assigns the WHITE expression axis to "W0.3d part-b (loader/worker completion-order
determinism)". The R4 ledger's `order` axis — loader completion SEQUENCE at boot — already
PASSES 10/10 (`R4-DETERMINISM/LEDGER.md:20,31-36`); what varies is which-FRAME work lands /
per-frame emission timing on the gameplay path (the `callerOrder`-adjacent axis, 1/10
non-gating by design). The part-b charter must be scoped to the frame-assignment/timing axis
on the eng_hot gameplay path, or it will re-prove boot-loader determinism and miss WHITE.

### F10 (LOW — hygiene): `wr_n10.json`/`wr_validate.json` contain bare `NaN` tokens

`json.dump` default `allow_nan` emitted literal `NaN` (mid_sat on black frames) — not valid
JSON for strict parsers. Use `null` in future harness dumps.

---

## Coordinator must fix BEFORE the README results table

1. **F1** — downgrade the wash co-sampler to "built, join broken, validation not demonstrated";
   strike "particle-LULLS / swept-light sub-axis" everywhere it is quoted; re-scope co-sampler
   v2 as join-fix + midrank-AUC first. *(The one finding that changes a wave headline.)*
2. **F4** — classjson/regen pass: classify `RB3_HANDS_MITTEN_OFF`, fix the MITTEN row's stale
   default/E1/byte-identical clauses and calibration numbers (16.4% / 21.8 / 62 / 134.1), fix
   the default-ON-workaround census.
3. **F5** — CLOSURE.md postscript: resolve the E1 conditional (ships ON, `403ff00`), fix the
   "no mesh tearing" clause, record the persistent top-center floating structure (+W10 lineage)
   and the drummer E1-coverage caveat as out-of-family residuals.
4. **F2/F3/F7** — R4-M4 STATUS + evidence md: refinish/attempt disclosure, black-frame
   exclusion restatement (13.81 vs 12.43, verdict unchanged), attribution reconciliation
   (LightPresetManager owns the +1; "within ±1" not "fully"), sd fix.
5. **A10 line in the README wave table** — D5 (`09fc594d`) was an interim wave-18 single-lane
   dispatch; don't double-count its box against this wave's.

---

## Recommended Wave-19 menu

The hands family is CLOSED — keep it closed; nothing below touches it except via the
pre-registered reopen condition.

**Lane W-ISO (PRIMARY, engine writer) — venue-path consumer isolation + WHITE re-grade
completion.** Extend the R4 M2 isolation pattern to the four named venue-path consumers
(`CharClipDriver.cpp:62`, `Crowd.cpp:1234` OnIterateFrac Fisher-Yates,
`CharInterest.cpp:172`, `LightPresetManager.cpp:286`) — this makes the A2 ledger precondition
*satisfiable* on eng_hot, converting the VOID into a runnable re-grade. Then re-run
`white_regrade.py` N=10/arm under the same F6-verbatim protocol, with the F2 fixes (no
refinish for the graded run; black frames excluded; per-boot songms committed). Exit:
READY_FOR_FLIP or HELD-with-numbers on a ledger-PASS sample. Cheap, mechanical, spends
instruments that already exist.

**Lane W-DET — W0.3d part-b, scoped per F9.** The named owner of WHITE expression at fixed
stream: frame-assignment/timing determinism of per-frame FX emission on the gameplay path
(NOT boot loader order — already 10/10). First deliverable is the instrument (per-frame
emission-count ledger axis on the eng_hot path + a fail-red demo), THEN a seam design.
Fold the **co-sampler v2 join-fix** (F1's correction: per-frame covariate advance + midrank
AUC + light POSITION amplitude signal) into this lane — it is the same instrument surface, and
v2 without part-b's per-frame ledger would repeat the stale-join failure.

**Lane N-TAIL (small) — R2 net completion.** Re-capture the bad-torn golden with the tier1
field (N's own optional follow-up — completes the four-arm tier1 story: good-body/arm-w/arm-s/
bad-torn), plus the F4 registry pass if the coordinator delegates it. Half a lane.

**Lane R3-WALK (small) — R3 v1 walk gap.** Instanced rows/overshell coverage for the uidump
walker (the recorded v1 gap). Independent, low-risk, unlocks UI-lane ROI queries on the
screens the walker currently skips.

**Optional (fund at most one):**
- **W2.4 BandPatchMesh — CONDITIONAL GO.** The hands family being closed removes the
  interleaved-diagnosis hazard, and R2's net (strict loader, BlendSpread, engine tier1,
  SkinningLiveGate grader, curl envelope) is a real regression floor. But BOTH prior
  BandPatchMesh attempts broke native in ways the drop/ratio gate was BLIND to
  (patch-shard corruption; bisect-proven twice) — the charter MUST carry the memory's re-land
  conditions verbatim: patch-bearing lineup captures + reviewer-judged wide frames as the E1
  gate, R2 fixtures as the floor not the gate. Without that clause, do not fund.
- **4→8 lights** — venue-lighting fidelity polish; independent; fine as a small lane if
  render budget allows.
- **Web-build confirmation tail gate** — one boot of the web build at the current pin to
  confirm the twelve defaults compile/behave there (cheap, overdue; the twelve were all
  verified native-only).

**Explicitly NOT recommended for Wave 19: the CharClipDrivers=0 root-cause item.** Chartering
it now would be *consistent* with the closure (CLOSURE.md records it as an optional,
separately-charterable NON-discriminator whose success triggers the reopen mechanically — the
A1 stop bans discriminator lanes, and this is upstream instrument work), but it is uncosted
Dolphin/substrate archaeology with three open candidate causes, its payoff (an articulated
capture) reopens a family the campaign just spent two waves closing honestly, and the mitten
now caps the user-visible severity. Park it on the ROADMAP as the reopen route it already is;
fund it only if a future wave NEEDS Wii-side animated ground truth for a different family
(e.g. a general anim-fidelity campaign), where its cost amortizes.

---

*Process note for the record: all five lanes + both coordinator actions kept the wave's
discipline — evidence committed, checkpoints written, no lane default-flips or pin bumps, the
two hazard files (`FxSendNative.cpp`, `rb3_session_trace.cpp`) never staged, classjson
append-only under lock with the single close-out regen. The A1 hard stop executed exactly as
made mechanical — no re-pricing loop was attempted. The template's close-out §1-3 sequence was
honored (this review precedes the README update).*
