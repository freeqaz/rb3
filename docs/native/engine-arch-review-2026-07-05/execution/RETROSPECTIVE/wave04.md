# Wave 4 — Retrospective (written with hindsight through Wave 12)

**Reviewer:** retro subagent. **Scope:** Wave 4 only (run `wf_c7f69c01-0ea`, 19 agents).
Engine pin `6221a56` → `609efb7`. Sources: `WAVE4_KICKOFF.md`, `WAVE4_REVIEW.md` (Fable pre-dispatch),
README Wave-4 results + the Wave-5/6/7/11 sections that later judged it, and the item STATUS docs
(`W2.1/`, `W2.3/`, `W0.3d/`, `W2.6/`).

**Evidence tags:** MEASURED = read from the file/commit; JUDGMENT = my call on measured facts.

---

## What Wave 4 set out to do

Land the two remaining SYS-1 mesh fixes now unblocked by W1.6's DrawContext, clean the draw-log gate,
and close out the hands work:

- **Lane A (engine, sequential):** W2.1 placement contract (crowd/drum-kit-at-one-point) → W2.3
  GeomOwner aliasing.
- **Lane B (rb3):** W0.3d draw-log determinism cleanup.
- **Lane C (rb3):** W2.6 foot/shoe rest-capture + flag-registry cleanup (this replaced the mistaken
  "W2.2-flip" after the Fable review caught the wrong-flag/refuted-premise error).

The Fable pre-dispatch review was unusually load-bearing here and it earned its keep: it caught a
**factual error that would have shipped a known-broken path** (D1 — the kickoff said W2.2 landed
behind `RB3_SKEL_REBIND_FULL` and Wave 4 would flip it default-ON; that flag is the *deliberately-broken
full-body rebind* used as the Phase-0 fail-red control — flipping it ships the sharding skin), and a
**scene-hollow gate** (B1 — the only committed drawlog golden is `splash_screen`, which contains no
crowd or drum, so the canonical gate literally could not observe the fix). Both catches were correct
and both were adopted.

## What Wave 4 actually shipped (MEASURED)

| Item | Outcome | Held up? |
|---|---|---|
| **W2.1 placement contract** | Landed default-OFF behind `RB3_PLACEMENT_CONTRACT` (engine `6852caa`) as a **vertex-invariant** reorganization: `obj.world = mesh->WorldXfm()` + bind-relative palette, worst element diff 0.0000; gate-first placement oracle (fail-red free, RED on default). | **YES — this is Wave 4's real win.** Flip eventually shipped Wave 6 (`fced18b`). The fix itself was never questioned. |
| **W2.3 GeomOwner aliasing** | **Refuted as a no-op** — crowd meshes already self-owned (owner==mesh). No behavior change, no flag. `RebindCrowdCharBonesToOwnSkeleton` retained + proven load-bearing (~24× shard-drop when disabled). | YES — honest refutation, correct. But an entire agent lane to prove a one-probe question (below). |
| **W0.3d gate cleanup** | Froze CharEyes/CharLookAt RNG (rb3 `c6b961da`); exit met via the **sanctioned per-name-eps fallback**, NOT the empty-sidecar ideal — a ~20u non-RNG residual survived. Part (b) async-loader determinism = **diagnosis-only, staged not landed.** | PARTIAL — see below; the deferred part-b became a multi-wave tail. |
| **W2.6 foot/shoe + registry** | PART 1: foot/shoe guard-DROP diagnosed as "within structural envelope," **no fix landed.** PART 2: flag-registry S4 **died on API overload**; scanner extension landed as follow-up `a537c2a3`; classification punted to W0.6b (Wave 5). | PARTIAL. The "structural envelope" verdict rested on a blind oracle (below). |

Headline as written: "the crowd-at-one-point / drum-kit-at-one-point root cause (SYS-1 placement) is
fixed … one deliberate flag-flip from shipping." That headline was **accurate**. Wave 4's core
deliverable was genuinely good work.

---

## What later waves refuted, and what would have caught it earlier

### 1. The wash: Wave 4 got it RIGHT, Wave 5 un-got-it, Wave 6 re-got-it. (~1.5 waves burned re-deriving Wave 4's own finding.)

This is the sharpest hindsight lesson of the wave, and it is **not** a Wave-4 failure — it's a
Wave-4 *success that was discarded and then expensively rebuilt.*

- MEASURED: `W2.1/STATUS.md:304-328` (Wave 4) already ran A/A controls (OFF twice + ON twice) on the
  gameplay A/B and concluded the pink bloom/exposure "wash" **"appears in BOTH flag states with
  run-to-run-varying intensity … orthogonal to W2.1 (pre-existing, likely the W0.3d venue/eye
  nondeterminism class)."** Wave 4 correctly declared it flip-independent.
- Then **Wave 5** attempted the flip, captured an **n=2-per-state** A/A, saw the blow-out in 1/2
  flag-ON and 0/2 flag-OFF, and the coordinator **HELD the flip** on the premise "wash appears only
  in flag-ON" — directly contradicting Wave 4's STATUS. (README Wave-5 §"COORDINATOR FLIP DECISION: HELD".)
- Then **Wave 6** built `wash_score.py`, ran an n=7-per-state songMs-pinned interleaved measurement,
  and got Mann-Whitney U=24.0 **p=1.0** / Fisher p≈0.59 → **"the Wave-5 'wash only flag-ON' premise
  is refuted under time control"** — i.e. it re-established exactly what `W2.1/STATUS.md` said in
  Wave 4, and *then* flipped.

**The premise failure was created in Wave 5, not Wave 4** — but Wave 4 is where it should have been
made un-reopenable. Wave 4 had the correct conclusion but only as prose in a STATUS file backed by an
n=2 A/A. It did not leave behind a **reusable, adequately-powered wash gate**, so when the flip came
up in Wave 5 the coordinator re-litigated it from a fresh underpowered sample and reached the opposite
(wrong) answer.

- **Missing tool:** a time-controlled, N≥6-per-state "full-frame exposure/wash" detector (luma
  both-tails + hue), runnable as a one-command gate. **Question it answers in one run:** "is this
  full-frame wash flag-dependent or A/A-variable?" — the exact question Wave 5 answered wrong by eye
  from n=2.
- **What the campaign did instead:** Wave 4 proved it by hand (n=2 A/A, prose); Wave 5 re-proved it
  wrong (n=2, coordinator eye); Wave 6 finally built `wash_score.py` and proved it right (n=7, one
  gated agent-item `W2.1-flip-blocker` + coordinator sign-off). **That detector, built at Wave 4,
  would have flipped `RB3_PLACEMENT_CONTRACT` at Wave 5 (or made the case airtight in Wave 4) and
  saved the entire Wave-5 hold + the Wave-6 `W2.1-flip-blocker` item.** Estimate: ~1.5 waves of
  coordinator + agent effort.

### 2. W2.6's "foot/shoe is within the structural envelope" leaned on an oracle later proven BLIND.

- MEASURED: W2.6 PART 1 declared `saddleshoe_skin.2`'s guard-DROP "within the structural envelope,
  not a clean rest-capture target" — using W2.2's oracle (whole-mesh ratio + origin skinpos, max
  SKINPOS 69.5u, "nothing in the 200-460u tripwire band").
- Wave 6/7 (`W2.8`) then MEASURED that **the same oracle is blind to the real defect**: fingers/hands
  "shard into thin sheets by R·sin(θ) … Whole-mesh ratio + origin skinpos read CLEAN = **W2.2's
  oracle is BLIND to this** (needs a far-vertex rotation metric)." The BL-A2 far-vertex oracle built
  in Wave 7 went RED at **79-107u** on a build W2.2's oracle called clean.
- JUDGMENT: W2.6's "structural envelope, no fix" verdict is therefore **only as trustworthy as an
  oracle we now know can't see rotation-basis shard.** It may be right for the shoe, but it was
  *asserted* on the strength of a metric that measures the wrong thing. Nobody in Wave 4 questioned
  whether "no tripwire hit" meant "no shard."
- **Missing tool:** a **far-vertex rotation oracle** (measure appendage tip displacement under the
  live bone basis vs authored, not just whole-mesh centroid ratio). **Question it answers in one run:**
  "does this mesh shard by rotation-basis mismatch even though its extent looks normal?"
- **What the campaign did instead:** W2.2 (Wave 3) shipped the centroid/tripwire oracle; W2.6 (Wave 4)
  cleared foot/shoe with it; W2.8 (Wave 6) discovered by eye it was blind; W2.8/BL-A2 (Wave 7) finally
  built the rotation oracle. Had the rotation oracle been the standard skinning gate from W2.2 onward,
  Wave 4's W2.6 verdict would have been made against the right metric.

### 3. W0.3d part-b was diagnosis-only and staged; the async-loader nondeterminism it named festered to Wave 11.

- MEASURED: W0.3d landed only the RNG freeze + per-name-eps fallback; part (b)
  (async-loader/worker completion-order feeding object-list insertion order) was correctly held
  **diagnosis-only** (F1 — its fix touches Lane-A files, coordinator-sequenced). Wave 5 landed the
  narrow `SortDraws` material-name tie-break (`76f51077`) as "the staged part-b artifact."
- Wave 11 BOOTRNG then MEASURED that the deeper mechanism was never fixed: **"the boot-varying state =
  global `gRand` stream POSITION … consumption ORDER varies per boot via async loader completion order
  — the exact mechanism W0.3d part-b (staged since Wave 4) exists to fix."** The Wave-12 menu note is
  blunter: *"the only staged part-b artifact was the SortDraws tie-break, landed Wave 5; this is NEW
  design."*
- JUDGMENT: this was a *correct* deferral (the F1 collision constraint was real), but the campaign
  then treated the narrow SortDraws tie-break as if it discharged part-b. It didn't — it made *draw
  submission order* deterministic without touching *gRand stream position*, which is a different
  determinism axis (RNG-consuming code paths). The "staged since Wave 4" patch was quietly assumed
  done and the real fix slipped to Wave 12+.
- **Missing tool / process:** a determinism ledger that tracks the *distinct* nondeterminism axes
  (draw-order vs alloc-order vs gRand-position vs float-clock) and their landed/staged/open state, so
  "part-b landed" can't silently mean "one of three axes landed." **What the campaign did instead:**
  each wave re-diagnosed the boot flake from its own symptom (eye jitter → draw order → preset pick →
  gRand position) across W0.3b/c/d, Wave-5, Wave-11, Wave-12 — at least 4 separate diagnosis lanes
  chasing one root cause because no artifact enumerated the axes.

### 4. W2.3 spent a full agent lane to answer a single-probe question.

- MEASURED: W2.3's entire deliverable was the finding "crowd meshes are ALREADY self-owned
  (owner==mesh)" → the bone-source rewrite is a no-op. That is a one-run ownership probe (`owner ==
  mesh?` over the crowd draw set), yet it was scoped as a full Lane-A sequential item with S1 oracle
  + negative controls + VERIFY (`007d49d8`…`ecb9db75`, ~4 commits).
- JUDGMENT: not wasted exactly — the refutation is genuine and the retained-rebind proof (~24×
  shard-drop) is worth having — but the *hypothesis could have been killed pre-dispatch* with the
  same ownership probe W2.1's own gate already computes. The Fable review flagged over-width (R-E)
  but aimed it at W3.1 (correctly deferred); it did not flag that W2.3's core claim was cheaply
  falsifiable before committing a serial engine lane to it.
- **Missing tool:** a pre-dispatch "cheap-falsification" pass — for any item whose premise is a single
  measurable predicate (here: `owner==mesh` for the target draw class), run the probe in the kickoff,
  not the wave. **What the campaign did instead:** dispatched the lane, then measured, then refuted.

---

## Premise failures — entering and created

- **Entering (created upstream, caught by Wave 4's own review):** the "W2.2-flip ships behind
  `RB3_SKEL_REBIND_FULL`" error and the "one flag-flip from shipping hands" claim. **Caught by the
  Fable pre-dispatch review** (D1/D2), before dispatch — the review gate working as intended. This is
  the single best argument in the whole campaign for the pre-dispatch review.
- **Entering (scene-hollow gate):** "the canonical draw-log gate catches co-location" — false because
  the only golden is splash (no crowd/drum). **Caught by Fable (B1)**; Wave 4 built the gameplay
  placement oracle first as a result. Good catch, cleanly fixed.
- **Created by Wave 4 (latent):** treating W2.6's "no tripwire hit → structural envelope" as a real
  clearance, on a blind oracle — **caught two waves later** by W2.8's eye-observation + BL-A2 oracle.
- **NOT created by Wave 4, but Wave 4's correct finding was discarded:** the wash's flip-independence.
  Wave 4 measured it right; Wave 5 un-measured it; Wave 6 re-measured it. The lesson is that a
  prose-plus-n=2 finding is not durable across a coordinator handoff — it needs to ship as a gate.

## Recurring bug families this wave touched

- **Skinning / mesh deform (hands, crowd, foot/shoe, char shard):** W2.1 (placement, fixed correctly),
  W2.3 (refuted no-op), W2.6 (foot/shoe, no fix — verdict on a blind oracle). State after wave:
  placement solved; rotation-basis finger/foot shard still open and, worse, *believed closed* because
  the standard oracle couldn't see it. Reopened Wave 6/7/8 as W2.8/BL-A1.
- **Wash / BOOTRNG / exposure nondeterminism:** W0.3d (RNG freeze + per-name eps; part-b staged) and
  the W2.1 gameplay-A/B wash. State after wave: draw-order axis heading toward determinism; gRand-
  position axis and the venue wash both still open and mis-scoped as smaller than they were. The wash
  specifically was *correctly diagnosed and then lost.*
- **UI text / layout:** not touched this wave (deferred to Phase 4 / Wave 5+).

## Wasted effort estimate: **moderate.**

- **Moderate** overall, concentrated in the wash re-derivation (the biggest single sink but downstream
  of Wave 4, in Wave 5→6) and the W2.3 full-lane-to-kill-a-one-probe-premise.
- Attributable *to Wave 4 itself*: **small-to-moderate.** W2.3's over-scoping (~1 agent lane for a
  one-probe answer) and W2.6's blind-oracle verdict (which required Wave 6/7 to overturn). Wave 4 did
  not waste effort on the wash — it got that right; the waste was the *downstream* failure to trust
  Wave 4's answer, which a wash-gate artifact would have prevented.

## Net judgment

Wave 4 is one of the **stronger** waves: it shipped the SYS-1 placement fix as a provably vertex-
invariant, oracle-gated, default-OFF change (textbook), its pre-dispatch review caught a would-ship
correctness error, and it honestly refuted W2.3. Its weaknesses are all *tooling/durability* rather
than judgment:

1. It proved the wash flip-independent but shipped that proof as prose+n=2 instead of a reusable
   gate → the finding was lost and re-bought over Waves 5-6 (~1.5 waves).
2. It cleared foot/shoe against an oracle later proven blind → a false "closed" that reopened Waves 6-8.
3. It correctly deferred async-loader part-b but let a narrow tie-break stand in for it → the gRand
   axis festered to Wave 11-12.

The through-line: **Wave 4's measurements were good; the artifacts it left behind were not durable
enough to survive a coordinator handoff.** The three missing tools — a time-controlled wash detector,
a far-vertex rotation oracle, and a per-axis determinism ledger — each would have converted a correct
one-wave finding into a standing gate, and each was eventually built anyway, just 1-3 waves too late.
