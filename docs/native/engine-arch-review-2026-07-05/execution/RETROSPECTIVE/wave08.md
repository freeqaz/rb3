# Wave 8 — Retrospective (hindsight review through Wave 11)

**Run:** `wf_daec8375-122`, 8 agents. **Engine pin:** `a94762f → a320f9d`.
**Kickoff+review commit:** `2eb2652b`. **One flip shipped** (FIX-H2 chroma-preserve default-ON).

Three lanes ran: **Lane A (WASH-fix)** venue wash/grayscale, **Lane B (W2.8c)** per-frame hands
conjugation, **Lane C (W5.1)** venue black poster quads.

---

## 1. What the wave got RIGHT (and it earned it)

**Lane A S1 instrumentation was a genuine, load-bearing win: it overturned the Wave-7 matrix
mechanism theory.** The `RB3_WASH_PROBE` (engine `71469af`) measured **0/8 default boots miss
venue-light engagement**, and the PINK boots have **byte-identical lighting inputs** to the clean
NEARBLACK boots (WASH-fix/STATUS.md H1 table, d3/d5 == d1/d2 and d4/d6/d8). That exonerated the
entire env-state decision machine (`:1435`/`:1560`/`:2453`) and reframed the Wave-7 verdict: the
pink is a **downstream screen-space composite flood**, not a revealed pink base, and
`venue_light_off` washes because the flat-default flood (1 white dir + 0.45 ambient, `:1576`)
*replaces* dark engaged lighting — not because it "unmasks" anything. This is the model outcome of
an instrumentation stage: a named, measured refutation of the incoming premise.

**FIX-H2 (`RB3_PP_CHROMA_PRESERVE`) is a real, shipped fix** (graded-luma × ungraded-chroma in the
Stage-2 composite; deterministic `venue_light_off` fail-red 5/6→0/6 PINK; flag-OFF byte-identical
792; DC3 zero-blast; menu B+W untouched via the `venueGrade` uniform). It flipped default-ON and
was **not** refuted by any later wave.

**Lane C is the cleanest process win of the wave.** Fable's review (A6) forced the `RB3_HEADMAT_DBG`
census as mandatory step 1 — **zero new code** — and it killed the W2.7-null-diffuse hypothesis
before any fix was written: the "black poster" is the `tv3_a` transition vignette's authored-dark
cork-board poster (`hasTex=1`, embossed text). Outcome: NOT A BUG, no code, no wasted fix stage.
This is exactly what probe-before-fix is supposed to buy.

**Lane B stopped honestly at the smoke tripwire.** The conjugation moved wext the wrong way
(80u@t0 → 500–2600u under animation); B.S2 hit the pre-registered STOP directive and refused to
tune blindly. That discipline is correct and cheap relative to blind formula-hunting.

**The Fable pre-dispatch review earned its cost this wave.** A1 caught the kickoff's "one env-state
bug, two directions" framing as half-wrong *before dispatch* (the grey-direction hypothesis was
already refuted in Wave 6), redirecting S1 onto the two correct hypotheses. A2 fixed gate (b), which
named the WRONG color control (`venue_light_off` = the flat-default look Lane A was trying to
eliminate). A3 upgraded gate (a) from a statistically toothless "wash 0/8 vs a 1/8 baseline"
(a no-op passes ~34% of the time) to a deterministic forced-miss fail-red + mechanism counter.
Every one of these amendments shaped the dispatched work and prevented a re-run of the W3.3-fix
"one lever, wrong stage" failure. Verdict: the review gate is worth keeping.

---

## 2. What the wave got WRONG, and what would have caught it earlier

### 2a. Hands (Lane B): the wave reinforced the wrong AXIS with a confounded oracle

Wave 8 treated the `IK_SHARD_VERT` far-vertex probe (79–107u RED) as the **hard-exit oracle** and
concluded "rotation basis wrong, translation correct → name the wrong factor empirically (Wave 9)."
Attribute-before-fix was the right *instinct*. But hindsight (Waves 9–11) shows the oracle Wave 8
handed forward **could not distinguish the two hypotheses it was being used to arbitrate**:

- **Wave 9** used a dual-skin probe to "name the factor" (offset rotation basis 42–87° off) — and
  its world-space fix was refuted (4th dead class).
- **Wave 10** then *unmasked the Wave-9 instrument itself*: the dual-skin reference was **captured
  pre-repoint** — the default rebind repoints the mesh from its static `bound` bone to the live
  `own` bone, so the 37.4u "shard" compared the drawn vertex against **a bone the draw does not
  use**. The metric was unsatisfiable without freezing the hands; the asset rebake "fixed" it to
  0.0u by pinning the appendages to two discrete positions (5th dead class).
- **Wave 11** finally built the right instrument — a **rest-capture-free joint-attachment probe on
  the uploaded palette** — which read **GREEN ≤0.33u over 2,214 samples on the exact frames showing
  the visible 95–106u smear**, EXONERATING the palette/skeleton axis entirely, and named the real
  axis: **authored-vertex-to-offset SHELL composition** (the mesh shell rotates about each coherent
  joint by ΔR; R·sin(θ) = 48.4u/bone → 95–106u finger chain), structurally invisible to every
  skeleton-side metric the campaign built.

So the whole 5-class bind-side campaign (Waves 7→10, of which Wave 8's conjugation was one link) was
chasing an axis the palette-coherence test would have ruled out in one run. Wave 8's contribution to
the error was *not* the honest conjugation stop — that was fine — it was **never questioning whether
the palette was coherent** and treating the far-vertex smear as proof of a fixable bind-side basis.
Nobody asked "does the drawn mesh even use the bone I'm measuring against?" until Wave 10 forced it.

**What would have caught it earlier:** the Wave-11 joint-attachment (coherent-vs-drawn-bone,
rest-capture-free) probe, run at Wave 7/8, answers "is the palette coherent?" = YES in one run — and
that redirects the entire lane off the skeleton axis *before* Wave 8's conjugation attempt, Wave 9's
world-space attempt, and Wave 10's rebake. Cost of not having it: ~4 fix-attempt+verify agent-stages
(W2.8c conj S2/S3, W2.8d S2, W2.8e S2/S3) against a confounded oracle.

### 2b. Wash (Lane A): the gates were confounded by per-boot nondeterminism nobody had named

S1/S2/S3 all disclosed that the natural-boot metrics were non-resolving: the unpinned mid_sat sweep
(gate c) was "NOT a clean A/B" because each boot's **director cut differs** (mean_val 0.16–0.58 at
equal songMs); S3 called gate (c) outright "not a valid numeric gate," and reproduced `vlo_both` as
**1/6 not 0/5** (one boot showed a residual WHITE). The shipped win therefore rested entirely on the
**deterministic forced-miss fail-red** + shader math — a sound but narrow footing that Fable's A3
had to install because the natural rate was un-gateable.

The disclosed residual — **engaged-venue WHITE over-exposure** — was handed to Wave 9 as "a clean
follow-up." It was not clean: Wave 10 landed `RB3_VENUE_WHITE_GUARD` default-OFF and **HELD it (no
flip)** because the primary metric **sign-flipped** (+17.96 vs −6.89) and a null control swung the
same ±5–18 — i.e. the eng_hot delta was indistinguishable from noise. The root of that noise was not
named until **Wave 11**: the global `gRand` **stream position** (~11k draw-count spread per boot,
consumption order varying via async-loader completion order), owned upstream by W0.3d part-b. So
Wave 8's honest caveat "the stochastic wash rate is not cleanly separable at small N" was exactly
right and foreshadowed a **three-wave dig** into an un-gateable metric.

**What would have caught it earlier:** a boot-deterministic capture harness (pinned RNG stream +
pinned director cut) makes the wash/WHITE A/B resolvable in one run. The campaign instead
substituted a deterministic forced-miss workaround + many-boot stochastic sampling (S1 8 boots, S2
5–6, S3 16) disclosed-as-low-power, and burned Waves 9–10 chasing WHITE with a sign-flipping metric
before Wave 11 root-caused the noise floor.

---

## 3. Premise failures (entering and created)

| premise | status | how/when caught |
|---|---|---|
| Wave-7 matrix: "env-state stochastically masks a pink base" | FALSE | **Caught in-wave** by Lane A S1 (0/8 engagement misses, identical inputs PINK vs clean). Best catch of the wave. |
| Kickoff: "one env-state bug, two directions" | HALF-WRONG | **Caught pre-dispatch** by Fable A1 (grey-direction refuted in Wave 6). Review gate working. |
| Kickoff gate (b) target = `venue_light_off` hue | WRONG control | **Caught pre-dispatch** by Fable A2 (that IS the flat-default look Lane A is eliminating). |
| IK_SHARD_VERT 79–107u = trustworthy hard-exit oracle for a bind-side fix | FALSE (oracle can't separate bind-basis-wrong from shell-composition) | **Caught 3 waves late** (Wave 11 joint-attachment probe). Wave 8 propagated it unquestioned. |
| CREATED: "rotation basis wrong, translation correct → name the factor (Wave 9)" | Directionally right, tooling still confounded | Wave 9 "named" a factor that Wave 10 showed was a **pre-repoint reference artifact**. |
| CREATED: WHITE over-exposure = clean Wave-9 item | FALSE (un-gateable on the same noise floor) | Wave 10 HELD it (sign-flipping metric); root-caused Wave 11 (gRand stream position). |

---

## 4. Recurring bug families touched

- **wash / BOOTRNG:** partially resolved. Chroma-preserve (FIX-H2) shipped and held; the grayscale
  symptom's mechanism confirmed. BUT the WHITE residual + the per-boot nondeterminism noise floor
  survived to Wave 10 (WHITE held) / Wave 11 (BOOTRNG named as gRand stream position). Wave 8's gate
  confound was the first clear symptom of the noise floor the campaign didn't name for 3 more waves.
- **hands (finger shard):** 3rd fix class killed this wave (game-side conjugation), honestly, at
  smoke. The lane stayed on the wrong (skeleton/bind-side) axis; not redirected until Wave 11.
- **UI text / layout:** not this wave's fixes (text floor shipped Wave 7). Lane C's poster-quad
  "layout" anomaly resolved as NOT-A-BUG with zero code.

---

## 5. Tooling gaps (concrete)

1. **Boot-deterministic capture (fixed RNG stream + pinned director cut).** *Question it answers in
   one run:* "does FIX-H2/WHITE-guard change the venue wash on the same shot?" — a resolvable A/B.
   *What the campaign did instead:* deterministic forced-miss fail-red (Fable A3 workaround) +
   many-boot stochastic sampling (S1 8 / S2 5–6 / S3 16 boots) disclosed as low-power, across Waves
   8–10; the noise root (`gRand` stream position) not named until Wave 11 (~2 diagnosis waves later).

2. **Rest-capture-free coherent-vs-drawn-bone palette probe** (the Wave-11 joint-attachment
   instrument). *Question it answers in one run:* "is the uploaded skinning palette coherent, or is
   the smear bind-side?" = the palette is coherent → the defect is the mesh shell, not the skeleton.
   *What the campaign did instead:* 5 bind-side fix classes across Waves 7–10 (~4 fix+verify
   agent-stages attributable from Wave 8 forward), all against a far-vertex/dual-skin oracle that
   could not separate the hypotheses.

3. **A reference-bone-vs-drawn-bone invariant assertion inside the hand probe harness.** *Question
   it answers in one run:* "was my reference captured against a bone the draw actually uses?" — the
   exact pre-repoint confound that made the Wave-9 dual-skin metric unsatisfiable. *What the campaign
   did instead:* discovered it by accident in Wave 10 when the asset rebake could only reach the
   target by freezing the hands.

---

## 6. Wasted-effort estimate

- **Lane C:** none. Census-first (Fable A6) was exactly right; zero fix stages spent on a non-bug.
- **Lane A (wash):** small-to-moderate *this wave* (the fix is real and shipped; the waste is that
  the natural-boot gates were non-resolving, salvaged by the forced-miss control). The larger cost
  (WHITE held, BOOTRNG dig) lands in Waves 9–11, but Wave 8's un-named gate confound was the first
  billable symptom.
- **Lane B (hands):** moderate, attributable to the *tooling choice*, not the honest stop. Wave 8's
  conjugation stopped cheaply at smoke — good. But by shipping the confounded IK_SHARD_VERT oracle
  forward as the hard exit and not testing palette coherence, it kept the lane on the dead axis for
  two more waves. A palette-coherence probe at Wave 7/8 would have avoided ~4 downstream fix-attempt
  agent-stages.

**Net:** the wave delivered one real shipped fix and two clean refutations; its cost was
propagating two un-interrogated instruments (the wash natural-boot gate and the hands far-vertex
oracle) whose confounds the campaign paid to discover over the following three waves.
