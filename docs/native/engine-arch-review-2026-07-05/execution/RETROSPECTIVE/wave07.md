# Wave 7 — Retrospective (hindsight review)

Run `wf_5527e4f1-ad7`, 11 agents. Engine `1b045d9` → `a94762f`. Kickoff+review `7fddb7da`.
Reviewed against Waves 8–15 outcomes.

## What Wave 7 set out to do

A declared **landing wave**: convert the four remaining user-visible complaints — root-caused
in Wave 6 — into shipped, gated fixes, plus two flip decisions.

- **Lane A (venue exposure):** land the staged grayscale-song-start fix (W3.3-fix), then run the
  WASH 5-config isolation matrix to name the stochastic full-frame venue wash.
- **Lane B (hands):** BL-A2 far-vertex rotation oracle (the metric W2.2's oracle provably lacked),
  step-0 re-measure of the dormant `RB3_HANDS_BIND_FIX`, then BL-A1 rotation-aware rebake.
- **Lane C (UI):** relax the UI-text color floor (W4.2-fix) + hub grey-quad flip package.
- **Lane D:** W3.2b point-light escalate-or-drop decision (plan-only).

## What actually shipped (and held up)

| Item | Wave-7 verdict | State after Wave 15 |
|---|---|---|
| **W4.2-fix UI text floor** | FLIPPED default-ON (engine `a94762f`, opt-out `RB3_UI_TEXT_FLOOR_STRICT`) | **HELD.** Durable win; contributed to the Wave-15 "defaults now TEN". |
| **W4.1 hub grey-quad** | FLIPPED default-ON (rb3 `cda3b326`, opt-out `RB3_HUB_MENU_QUAD_OFF`) | **HELD.** Wave-9 rebaseline confirmed the quad gone on-screen. |
| **W3.2b point-light** | DROP (keep per-pixel Lambert; do not grow `SceneUniforms` 656→752) | **HELD.** Never reopened; DC3 blast stayed zero. |

These three are the wave's clean, durable output. The UI/text/layout bug family was genuinely
retired here, and the SYS-4 point-light decision was a correct evidence-based *no-build*.

## What Wave 7 got wrong — refuted by later waves

### 1. W3.3-fix grayscale — shipped a documented no-op on a false Wave-6 premise

Wave 6 diagnosed the grey song-start as "the per-channel Reinhard ceiling desaturates hot pink to
grey." A.S1 (Sonnet) implemented the luma-keyed ceiling (`RB3_PP_LUMA_CEILING`, engine `7943bfa`)
with every gate green. A.S2's own determinism-controlled per-tonal-band verify then **refuted the
premise**: the grey is a **sub-knee MID/LOW-tone desaturation** (default mid-sat 0.026 vs pp_off
0.389), and both ceiling branches are identity below the knee — the fix architecturally cannot
touch it. The single-crop positive read that motivated it did not survive a determinism-controlled
analysis.

- **Root premise failure was in Wave 6, not Wave 7** — but Wave 7 built and landed a real engine
  feature against it before the tonal-band analysis was ever run. Wave 8 then fixed the grayscale
  for real with chroma-preserve (`RB3_PP_CHROMA_PRESERVE`, default-ON `a320f9d`).
- **What saved the campaign:** review amendment **A3** forced flag-first (default-OFF + coordinator
  flip) over the kickoff's "default-ON if the sweep is unambiguous." The sweep *was* unambiguous —
  and wrong. Flag-first is why a broken default never shipped. Process caught what the diagnosis
  didn't.

### 2. WASH matrix — named the wrong mechanism; the discriminator it needed was skipped

The 5-config N=8 matrix reached a coordinator verdict: **"MECHANISM NAMED: the P4 per-environ
venue-light rewrite"** — `venue_light_off` washed PINK 8/8 vs default 1/8 (Fisher p≈0.001),
read as "the pink IS the broken/unlit-env base; the venue-light rewrite masks it stochastically."

Wave 8's `RB3_WASH_PROBE` instrumentation **overturned this in one run**: 0/8 default boots miss
lighting engagement, and PINK boots have **byte-identical lighting inputs** to clean boots. The
pink is a downstream screen-space magenta flood in the composite grade — *not* a revealed base.
`venue_light_off` washes because the flat-default flood *replaces* engaged lighting that the grade
turns magenta, **not because it unmasks a pink base**. Wave 11 (BOOTRNG) then traced the actual
stochasticity to global `gRand` stream position (async loader completion order), and Wave 12
refuted even the timing sub-hypothesis (H-ORDER, not H-TIMING).

The sharpest self-indictment is in WASH/STATUS.md itself: the "To finalize" step 4 **explicitly
named the one discriminator the flag matrix cannot make** — a `RB3_PP_OFF` composite-off probe to
separate "postproc grade" from "async residency." The coordinator verdict inferred mechanism from
*which flag moved the rate* and never ran that probe. A flag-isolation matrix answers "which knob
changes the outcome," never "why" — and here "off makes it worse" was misread as "on masks it."

### 3. W2.8 hands — correctly killed two static classes, then prescribed the wrong direction

The durable value: BL-A2's far-vertex oracle was RED on today's build (79–107u vs 20u threshold) —
the gate W2.2 provably lacked. Step-0 killed the dormant `RB3_HANDS_BIND_FIX` (inert: its
`clipPlaying` trigger fires 0×), and B.S3 killed the rigid-anchor `RB3_HANDS_POSEAWARE`
(106→205u on per-bone-authored hands) with numbers. Dead-end pruning is real progress.

But the wave's **conclusion** — "two whole classes of *static* fix are now proven dead ends → the
shard requires a true **per-frame** pose-aware basis correction" — was the wrong prescription, and
it steered Waves 8–14:

- Wave 8 per-frame conjugation: **refuted** (80u → 500–2600u).
- Waves 9/10/12/14: four more measured dead classes (world-space rest, asset rebake, single-live-bone,
  reskin) — **seven measured dead artifacts total**.
- Wave 13 **inverted the core premise**: the dual-skin probe the hands lane trusted had `own`/`bound`
  **labelled backwards** — `own=Find(name)` *is* per-member and animates; `bound` is the static
  authored bind. Every hands metric from W2.8 through Wave 12 was built on a mislabelled reference.
- Wave 15 adjudicated the fix: **keep authored per-mesh offsets + repoint to `own` (SetBone
  calcOffset=false — the torso pattern)** — a **static** change, in `RebindHeadHandsAtRest`. Wave 7
  had closed "static fix" as a dead class and sent the campaign chasing per-frame corrections that
  the eventual answer never needed.
- Wave 15 also declared **`wext>60 is NOT a hands-shard oracle** (legit two-hand extents reach
  104u)" — the far-vertex numeric gate lineage Wave 7 established was partly measuring legitimate
  geometry.

## Premise failures — entering and created

- **Entering (Wave 6):** the W3.3 "ceiling desaturates hot pink" mechanism. Caught only by A.S2's
  post-impl tonal-band verify. Flag-first (A3) contained the damage.
- **Entering (Wave 6 backlog):** the WASH "async residency / venue-light masks pink base" ranked
  prior. The kickoff review (A2) already corrected the stale "unlanded W0.3d part-b patch" premise —
  but the matrix then **created a new false premise** (P4 venue-light mechanism) and blessed it as a
  coordinator verdict. Overturned Wave 8.
- **Created by this wave:** "the hands need a per-frame pose-aware basis correction." A false
  prescription that cost ~six downstream waves.
- **Latent, not caught here:** the own/bound label inversion in the dual-skin probe — carried
  silently through Wave 12, inverted Wave 13. Wave 7 trusted and extended the mislabelled instrument.

## Tooling gaps — what one capability would have shortcut each miss

1. **Per-boot lighting-input digest probe** (what became `RB3_WASH_PROBE` in Wave 8). One
   instrumented run answers "are the lighting inputs different on PINK vs clean boots?" → *no,
   byte-identical* → the wash is composite-side, not lighting-side. **Instead:** a ~40-boot,
   5-config detached statistical matrix + a coordinator verdict that named the wrong mechanism
   (≈1.5 agent-stages + ~40 boots of compute, redone in Wave 8).

2. **Composite-grade-vs-input discriminator** (`RB3_PP_OFF` probe). The WASH STATUS listed this by
   name as "the one discriminator the 4-flag matrix cannot make on its own" — and it was not run
   before the verdict. The matrix concluded from flag-response alone. **Cost:** the entire mechanism
   verdict had to be re-derived in Wave 8.

3. **Runtime bone-pointer identity/animation probe for the hands** — "which pointer animates vs is
   static, per-member vs shared?" Wave 13's S.S1 finally ran this and inverted the premise in one
   pass. Had it been the *first* thing BL-A2 verified, Waves 8–12's per-frame chase would not have
   happened. **Instead:** ~6 waves of fix-attempt-then-refute on a mislabelled reference bone.

4. **Tonal-band saturation profiler as the W3.3 *diagnosis* gate (pre-impl).** A.S2 built exactly
   this — but *after* the ceiling was implemented. Running it as the Wave-6/7 diagnosis gate would
   have shown the ceiling is sub-knee-identity and skipped the whole `RB3_PP_LUMA_CEILING` build.

5. **Boot-determinism (gRand stream-position pin).** Named Wave 11, still unfixed. Without it every
   statistical visual gate — the WASH N=8 matrix here, the Wave-10 WHITE gate — runs on a
   non-resolving noise floor. The WASH matrix fought a boot-stochastic phenomenon whose stochasticity
   source was unfixed; N=8 could establish "it's stochastic" but never a clean mechanism.

## Wasted effort — moderate-to-large

- **W3.3-fix impl:** A.S1 (Sonnet) built a real engine feature that A.S2 (Opus) then proved is a
  no-op on target. The verify was valuable (named the real mechanism); the *impl* was avoidable had
  the tonal-band profile been the diagnosis gate. ~1 wasted build-stage; flag-first prevented worse.
- **WASH matrix:** the rate histograms are legitimate evidence the wash is boot-stochastic and reach
  both cameras — not wasted. But the *mechanism verdict* was wrong and fully redone in Wave 8 (one
  probe). ~1 wasted verdict-stage + ~40 boots.
- **W2.8 per-frame prescription:** the oracle + two-class-kill had real value. The "need per-frame"
  conclusion is the expensive one — it framed Waves 8, 9, 10, 12, 14 as per-frame/vert-bake attempts
  (seven dead artifacts) before Wave 15's static-cell adjudication. **Large** downstream cost, though
  the triggering premise inversion (own/bound labels) was genuinely subtle and only a runtime-pointer
  probe would have exposed it early.

**Net:** Wave 7 shipped three durable fixes (two UI flips + a correct point-light drop) and pruned
two dead hands classes with numbers — but its two *mechanism verdicts* (WASH = venue-light rewrite;
hands = need per-frame) were both wrong, and both were the kind of question an instrumentation probe
answers in one run where a statistical/flag-isolation approach answered wrongly. The campaign's
flag-first discipline was the thing that kept the wrong grayscale fix from shipping default-ON.
