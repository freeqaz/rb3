# BOOTRNG — Stage A.S1 STATUS (Wave 11, Lane A, Opus) — DIAGNOSIS

Checkpoint id `A-S1` → `/tmp/wave11-checkpoints/A-S1.json`. Mode: DIAGNOSIS-ONLY (no fix).
Build: `native/build-agent-BOOTRNG` (own dir). Engine probes committed `milo-native-engine 29fc0aa`;
rb3-side probes + harness `rb3 fe696d63`. Six defaults ON, refuted flags UNSET, all probes
getenv-gated (`RB3_BOOTRNG_PROBE`). drawlog-golden `--fixed-clock --canonical-order` = **792**
flag-OFF (byte-identical). Captures: N=12 (`measure/as1.json`, color-only hash) + N=10 confirm
(`measure/as1v2.json`, position-inclusive hash), pinned `coop_dir_crowd.shot`, songMs≈21000±2000,
`RB3_FIXED_CLOCK=1`.

## VERDICT

**H-A (the pre-registered prime suspect — "preset-pick identity partitions the boots and stratifies
mid_sat") is REFUTED.** Every lighting category in the coop venue holds exactly ONE preset
(`idx=0/1`), so `PickRandomPreset`'s `RandomInt(0,1)` is identically 0 — the pick **consumes a gRand
draw but is deterministic in identity**. All 12 boots picked the identical preset per category
(`coop_intro/loop_cool/flare_slow/silhouettes_spot/strobe_fast/flare_fast`); the entire `mid_sat`
spread (0.388) lies inside this single preset class. Preset identity explains **zero** of the variance.

**The boot-varying STATE is the global `gRand` STREAM POSITION at the capture frame** — and it is
**UPSTREAM of the render lighting-selection and grade**, both of which are proven invariant/identical
across washed and non-washed boots. Per the A.S1 exit rule (kickoff §S1: "if the varying state is
upstream of the render (loader/event order), name the upstream owner and stop"), the mechanism is
**not** a render/postproc selection defect and there is **no in-lane S2 fix** — the render selection
is already deterministic. Named upstream owner: **W0.3d part-b** (async-loader/worker completion-order
→ gRand stream divergence), compounded by a **harness confound** (the ±2000 ms capture window).

## The boot-varying state, named precisely

Under `RB3_FIXED_CLOCK` the seed is pinned (`System.cpp:397-407`, `0x5EED`) but the shared global
stream (`gRand`, `Rand.cpp:6`) **position** at capture varies per boot across a **~11,000-draw
spread** (`gdraw_capture` 349,822 – 361,053, 12 distinct values in 12 boots). The lighting EVENT
COUNT is deterministic (every boot fires exactly **24** `SetLighting`/`PickRandomPreset` events before
capture) but the gRand draws BETWEEN those fixed events vary by **~7,000** per boot
(last preset event `gdraw` 338,482 – 345,369). So the divergence lives in the **non-lighting** gRand
consumers between the lighting events — the animated-content consumers named in WAVE11_REVIEW A1:
`Crowd` idle anim, `CharClipDriver/Group` clip picks, `Part`/`PartLauncher` FX (the flare/strobe
presets this venue uses), `CameraShot` shake, `Wind`, `AnimFilter`. Their phase at capture is what
differs between a 0.06 boot and a 0.45 boot.

## Evidence — the render-side state is INVARIANT across the wash range (A2/A3 value-level)

1. **Composite grade (PPRESOLVED, A2) — invariant at fixed phase.** All 12 N=12 boots captured the
   identical resolved ColorXfm: `world.pp con=50 bri=15 sat=-20`, identical levels
   (`lvlInHi=(0.596,0.776,0.631)`, `lvlOutLo=(0.263,0,0)`). The grade is a deterministic **ramp** in
   song time (`"song authoring"` `Interp(mPostProcA,mPostProcB,blend)`, `BandDirector.cpp:283-299`):
   the v2 confirm caught 2 boots mid-ramp (`con≈42.8`) purely because they captured ~100 ms earlier —
   monotonic in songMs, **not** per-boot. The grade is NOT the fixed-phase mover. `mColorModulation`
   (the gRand flicker) is absent from the native composite fill (confirmed — WAVE11_REVIEW A2) → a
   stream consumer but visually inert natively.
2. **Engaged venue LIGHTING VALUES (LIGHTVAL, A3) — identical across a WHITE and a NEUTRAL boot.** The
   full near-capture LIGHTVAL multiset is **byte-identical** between the WHITE boot (mid_sat 0.172,
   `hi_frac` 29) and NEUTRAL boots (0.31–0.45): same 4 environ groups
   (`geom.env valhash=a900cd29 ×25`, `chars.env=8feaa34a ×18`, `''=14650fb0 ×9`,
   `crowd.env=d46a89bc ×8`), same colors/types/showing, same ambient. **5 boots** share the identical
   `valhash` tuple `(8feaa34a,a900cd29,d46a89bc)` yet span mid_sat **0.314–0.451** and classes
   NEUTRAL→NEARBLACK; the WHITE boot's grade matches theirs exactly. → the venue-light COLOR/TYPE/
   SHOWING selection does not determine the wash. (v2 folded light world POSITION+range into the hash;
   positions animate every frame → phase-driven, tied to songMs, not a fixed-phase per-boot mover.)
3. **Preset identity — deterministic.** `[BOOTRNG] PRESET` shows `idx=0/1` for every category, 12/12
   boots identical picks. H-A's specific mechanism (a preset REWRITING env light color/type per boot)
   cannot fire: there is only one preset per category to pick.

## The variance decomposes into two components (both upstream of render selection)

| Component | Signal | Owner |
|---|---|---|
| **Capture-window phase (dominant, ≈harness)** | The ±2000 ms window admitted a **bimodal** songMs split (~20,900 vs ~21,150); `pearson(songMs,mid_sat)=0.77` is driven by this cluster separation, NOT within-cluster structure. | The **harness `--tol`** — a free, non-engine lever: tightening the window collapses this component. |
| **Fixed-phase per-boot residual (genuine)** | WITHIN a songMs cluster mid_sat still scatters ~4× at <30 ms separation (EARLY cluster: 20876→0.063 vs 20878→0.280; spread 0.246 over 72 ms). `pearson(gdrawCap,mid_sat)=0.68`. | **W0.3d part-b** — gRand stream divergence via async-completion order → animated-content phase. |

Per-boot table (N=12, sorted by songMs) — note the two songMs clusters and the within-cluster scatter:

| songMs | class | mid_sat | hi_frac | gdrawCap |
|---:|---|---:|---:|---:|
| 20876 | NEUTRAL | 0.063 | 2.6 | 355170 |
| 20878 | NEUTRAL | 0.280 | 2.2 | 356277 |
| 20896 | NEARBLACK | 0.089 | 2.1 | 349860 |
| 20899 | NEARBLACK | 0.090 | 2.1 | 349855 |
| 20906 | NEARBLACK | 0.309 | 2.1 | 349822 |
| 20926 | **WHITE** | 0.081 | **22.1** | 350504 |
| 20948 | NEARBLACK | 0.087 | 2.0 | 350666 |
| 21120 | NEARBLACK | 0.451 | 1.7 | 355318 |
| 21157 | NEUTRAL | 0.356 | 1.6 | 354826 |
| 21168 | NEUTRAL | 0.314 | 1.5 | 355511 |
| 21179 | NEUTRAL | 0.333 | 1.5 | 357103 |
| 21180 | NEARBLACK | 0.435 | 1.4 | 361053 |

(The smoke boot, songMs 21079, was a second **WHITE**, `hi_frac` 29 — WHITE reproduces ~2/13 ≈ the
~1/6 stochastic rate WHITE-fix reported; grade + LIGHTVAL identical to non-washed boots.)

## What the residual wash comes from (leading candidate, NOT yet instrumented — S2/Wave-12 hand-off)

The fixed-phase residual over-exposure (the WHITE spikes, `hi_frac` 22–29) is scene-side and NOT in
the venue-light color selection or grade (both invariant). With colors identical, the remaining
gRand-driven, camera-proximate over-exposure sources are: **particle/pyro FX** (`PartLauncher`/`Part`,
directly gRand-driven — the coop venue runs `flare_fast`/`flare_slow`/`strobe_fast` presets that
trigger bursts) and/or **swept point-light POSITION** hitting the camera at a divergent phase. These
are the two unresolved sub-axes; distinguishing them needs a PartLauncher/emission probe (a follow-up
instrument, out of this stage's declared range). This is the same shape as A.S3's charter for Lane B:
the instrument that would make a BOOTRNG-affected gate resolvable is one that co-samples the FX/light
phase against `hi_frac` per frame.

## Consequences for the deferred items (per A4/A9)

- **The A4 "pin the preset pick under RB3_FIXED_CLOCK" seam does NOT apply** — the pick is already
  deterministic (count=1). There is nothing render-selection-side to pin. A true determinism seam
  would have to pin the ENTIRE pre-capture gRand consumer sequence, i.e. close W0.3d part-b — the
  named owner's job, not a lighting fix.
- **The A9 mechanism-identity gate is only PARTIALLY satisfiable today:** 10/10 same preset pick ✅,
  same postproc source tuple ✅ (`song authoring`), but the same gRand stream position ❌ (12/12
  distinct) — the PRIMARY gate's third clause is exactly the open divergence. A variance gate at
  fixed `--tol` cannot resolve until W0.3d part-b lands.
- **First free lever for gate-trustworthiness (recommend to coordinator):** shrink the capture window
  `--tol` (2000 → ~100–150 ms) to remove the dominant bimodal-phase component of the "BOOTRNG floor";
  the residual is then the true W0.3d-rooted stream divergence, ~4× smaller in the observed data.
- **The WHITE real-lever reframes (A4):** since WHITE fires at a grade + venue-light state IDENTICAL
  to non-washed boots, the over-exposure is a per-FX/per-swept-light rendering-fidelity issue at a
  specific animation phase, not a static venue-exposure constant — confirming WHITE-fix B.S2's "needs
  a deterministic ENGAGED-hot reproducer" and pointing that reproducer at the FX/light-phase axis.

## Instruments delivered (all getenv-gated, probe-only, drawlog-792 flag-OFF)

- rb3 `math/Rand.{h,cpp}` `RB3GRandDrawCount()` — global-stream draw counter (HX_NATIVE).
- rb3 `world/LightPresetManager.cpp` `[BOOTRNG] PRESET` — pick identity + idx/count + stream position.
- rb3 `bandobj/BandDirector.cpp` `[BOOTRNG] PPSRC` — postproc source tuple + stream position.
- engine `Rnd_Wgpu_RB3.cpp` `[BOOTRNG] LIGHTVAL` — per-env value digest (color/type/showing +
  position/range) + resolved ambient.
- engine `RB3PostProc.cpp` `[BOOTRNG] PPRESOLVED` — resolved ColorXfm the composite consumes.
- Harness `bootrng_probe.py` — N≥10 pinned capture, parse + partition + correlate.

Trustworthiness: the delivered instruments are **trustworthy for what they measure** — they
DEFINITIVELY exonerate the lighting-selection and grade (identical across the wash range) and prove
the stream-position divergence. They are **NOT** a fail-red gate for the wash itself: no delivered
signal co-varies monotonically with `hi_frac` at fixed phase (the wash driver is one axis deeper —
FX/swept-light phase — which S2/Wave-12 must instrument to build the gate).

## Artifacts
`PLAN.md`, `bootrng_probe.py`, `measure/as1.json` (N=12), `measure/as1v2.json` (N=10 confirm), raws
`/tmp/bootrng-caps{,-v2}/`, run logs `/tmp/bootrng-run{,-v2}.log`. Checkpoint
`/tmp/wave11-checkpoints/A-S1.json`. Commits: rb3 `fe696d63`, engine `29fc0aa`.

---

# BOOTRNG — Stage A.S2 STATUS (Wave 11, Lane A, Opus) — DETERMINISM-SEAM DECISION

Checkpoint id `A-S2` → `/tmp/wave11-checkpoints/A-S2.json`. Mode: DIAGNOSIS-ONLY.
**Outcome: UPSTREAM/OUT-OF-LANE branch — no in-lane seam exists to build. Honest writeup + backlog.**
No code committed this stage (docs-only). Binaries unchanged from S1 (`rb3 fe696d63`, `engine 29fc0aa`);
drawlog-golden `--fixed-clock --canonical-order` = **792** flag-OFF already proven at that state (S1).

## The S2 fork and which branch S1 forces

The S2 charter: **if S1 named an in-lane mechanism** (preset selection / postproc tuple / render-side
state) → implement an A4 determinism seam (pin the varying selection ONLY under `RB3_FIXED_CLOCK`, the
`0x5EED`/CharEyes precedent). **If S1 named an upstream/out-of-lane owner** → honest writeup + backlog,
no code.

S1 named the **upstream owner**: the boot-varying state is the global `gRand` **stream position** at
the capture frame, driven by **async-loader/worker completion order = W0.3d part-b** (staged-not-landed;
its fix touches the Lane-A object-list/loader path, coordinator-sequenced — README `:228-236`,
`:129`). Every render-side SELECTION is already deterministic. There is therefore **nothing
render-selection-side to pin** → the A4 seam does not apply. Second branch taken.

## Re-verification of S1 (I did not rubber-stamp — re-derived from `measure/as1.json`, N=12)

| S1 claim | Independent check this stage | Result |
|---|---|---|
| Preset pick identity is deterministic (H-A refuted) | `distinct(preset_seq tuple)` over 12 boots | **1** — all 12 boots identical 24-event sequence ✅ |
| " | `distinct(preset_final_by_cat)` | **1** ✅ |
| Resolved composite grade is invariant (incl. WHITE boot) | `distinct(ppres_tail)` over 12 boots | **1** — byte-identical ColorXfm every boot ✅ |
| gRand stream position varies per boot (the open axis) | `gdraw_capture` spread | **11,231 draws**, 12 distinct values / 12 boots ✅ |
| Variance is real & tracks stream position, not selection | `pearson(gdraw_capture, mid_sat)` | **0.684** (SD-range `mid_sat` 0.063→0.451 = **0.388**) ✅ |
| Capture-window confound present | `pearson(songms, mid_sat)` | **0.771** (the ±2000 ms bimodal harness component) ✅ |

S1's mechanism holds exactly. The render pipeline picks the **same preset, same grade, same light
color/type/showing** every boot; the only thing that differs is *where in the shared `gRand` stream*
the capture frame lands — set by upstream async-completion order, one axis below anything Lane A's
BOOTRNG range (`Rnd_Wgpu_RB3.cpp ~1044-1660` + `RB3PostProc.*`) can reach.

## Why NO in-lane determinism seam is buildable (the A4 analysis, scored)

A4's seam recipe presupposes **H-A** ("pin the RNG-driven preset SELECTION under `RB3_FIXED_CLOCK`").
H-A is refuted, so the recipe's target does not exist:

1. **Pinning the preset pick is a no-op.** `PickRandomPreset` does `mPresets[s][RandomInt(0,count)]`
   with `count==1` for every category in the coop venue → the pick is *already* the single available
   preset on every boot. Freezing it changes nothing (it still consumes its 1 gRand draw; identity is
   invariant with or without a seam).
2. **Pinning the grade is a no-op.** `ppres_tail` is byte-identical 12/12; the grade is a
   deterministic song-time ramp (`BandDirector` `Interp(A,B,blend)`), already boot-stable at fixed
   phase.
3. **The A9-PRIMARY third clause ("same gRand stream position at capture") is unsatisfiable in-lane.**
   Making the stream position identical requires pinning the **entire pre-capture consumer sequence**
   (~350 k draws across Crowd/CharClipDriver/Part/CameraShot/Wind/AnimFilter, phase-shuffled ~7 k
   draws between the fixed 24 lighting events). That IS W0.3d part-b (async-loader/worker
   completion-order determinism). It is (a) out of BOOTRNG's declared range, (b) not a render/postproc
   *selection* defect, (c) already an owned, staged, coordinator-sequenced item whose fix touches the
   Lane-A DrawMesh/object-list path. Building it here would collide with the W0.3d-fix owner and
   violate the out-of-range rule (A8: escalate, don't edit).

So the A4 seam has no in-lane surface. The authored preset randomness is retail-faithful and stays
live (nothing to remove); there is simply no render-side selection whose determinism the seam would
buy. This is precisely A4's own predicted fallout: **BOOTRNG = faithful randomness ⇒ the residual
"stochastic wash" reframes as per-FX / per-swept-light RENDERING FIDELITY at a specific animation
phase (the WHITE real-lever), a follow-up fix-class item — NOT something Lane A may fix by
de-randomizing.**

## A9 pre-registered gates — resolved honestly (PRIMARY-partial / SECONDARY-fail = new finding, not fudged)

| Gate | Clause | Result under the (absent) seam |
|---|---|---|
| **PRIMARY** (mechanism identity) | same preset pick | ✅ 12/12 identical (10/10 satisfied) |
| " | same postproc source tuple | ✅ 12/12 identical (`song authoring`, `ppres_tail` distinct=1) |
| " | same gRand stream position | ❌ 12/12 **distinct** — the open divergence, owned by W0.3d part-b |
| **SECONDARY** (visual) | `mid_sat` range < 0.05 (vs 0.295 obs) | ❌ **0.388** at N=12 — the residual |
| **SECONDARY** | `hi_frac` range < ~5 | ❌ WHITE boot spikes `hi_frac` 22.1 vs ~1.4–2.6 floor |

PRIMARY passes on 2 of 3 clauses; the third clause **IS** the named upstream divergence, so it cannot
close in-lane. Per A9's own rule ("if PRIMARY passes and SECONDARY fails, that residual is a NEW
finding, not a gate fudge"), the SECONDARY miss is filed below as the FX/swept-light-phase finding —
**not** back-fit into a seam or a re-golden.

- **drawlog 792:** no engine/rb3 code committed this stage → no binary change → the S1-verified
  792 flag-OFF canonical count stands unchanged. Nothing to re-golden.
- **lineup PASS + fail-red (seam off → variance returns):** N/A — there is no seam to toggle. The
  fail-red control this gate wanted (variance returns when the seam is disabled) is trivially the
  observed baseline: variance is *already present* because no seam exists.

## NEW FINDING (filed, not fudged) — the residual wash driver is one axis deeper

With preset identity, grade, and venue-light color/type/showing all proven invariant across the wash
range, the surviving boot-variance in `mid_sat`/`hi_frac` (the WHITE spikes, `hi_frac` 22–29) is
**scene-side rendering fidelity at a gRand-phase-dependent animation state** — the two camera-proximate,
gRand-driven over-exposure candidates:
- **particle / pyro FX** (`PartLauncher`/`Part`, directly gRand-driven; the coop venue runs
  `flare_fast`/`flare_slow`/`strobe_fast` presets that trigger bursts), and/or
- **swept point-light POSITION** hitting the camera at a divergent phase.

Distinguishing them needs a **PartLauncher/emission + swept-light-position probe co-sampled against
`hi_frac` per frame** — an instrument outside BOOTRNG's declared range and this stage's charter. This
is the same instrument shape A.S3 (Lane B) is chartered for: the co-sampling probe is what turns a
BOOTRNG-affected wash gate from unbuildable into resolvable. **Handed to S3/Wave-12.**

## BACKLOG (S2 hand-off to coordinator)

1. **[owner: W0.3d part-b — coordinator-sequenced]** Land the staged async-loader/worker
   completion-order determinism patch. Until it lands, the gRand stream position at any pinned capture
   is boot-nondeterministic → **no fixed-clock wash gate can reach A9-PRIMARY's third clause.** This is
   the single blocker for a boot-stable BOOTRNG gate. It touches the Lane-A DrawMesh/object-list path
   (README `:229`, `:236`), so it must sequence *after* Lane-A DrawMesh work settles — exactly why it
   cannot be an in-lane BOOTRNG seam.
2. **[free lever — harness, no engine change]** Tighten the capture window `--tol` from 2000 ms to
   ~100–150 ms. This removes the DOMINANT bimodal-phase component (`pearson(songms,mid_sat)=0.771` is
   driven by the ~20,900 vs ~21,150 ms cluster split, not within-cluster structure). The residual
   after tightening is the true W0.3d-rooted stream divergence — ~4× smaller in the observed data
   (within-cluster EARLY: 20876→0.063 vs 20878→0.280). Recommend the coordinator apply this to the
   "BOOTRNG floor" definition immediately; it is free and independent of item 1.
3. **[S3 / Wave-12 — new instrument]** Build the PartLauncher-emission + swept-light-position probe
   co-sampled against `hi_frac` per frame (the NEW FINDING above). This is the instrument that makes a
   BOOTRNG-affected wash gate a fail-red gate rather than an exoneration-only tool. Out of BOOTRNG's
   declared range; belongs with the FX/light-phase axis.
4. **[reframe adopted — WHITE real-lever]** WHITE fires at a grade + venue-light state IDENTICAL to
   non-washed boots (proven: `ppres_tail` and near-capture LIGHTVAL multiset byte-identical between the
   WHITE boot and NEUTRAL boots). ⇒ the over-exposure is a **per-FX / per-swept-light rendering-fidelity
   defect at a specific animation phase**, NOT a static venue-exposure constant. Confirms WHITE-fix
   B.S2's "needs a deterministic ENGAGED-hot reproducer" and points that reproducer at the
   FX/light-phase axis (item 3), not at a venue-exposure tuning constant.

## Trustworthiness restatement (unchanged from S1, re-affirmed)

The S1 instruments are **trustworthy for what they measure**: they DEFINITIVELY exonerate the
lighting-SELECTION and the grade (identical across the wash range) and prove the stream-position
divergence. They are **NOT** a fail-red gate for the wash itself — no delivered signal co-varies
monotonically with `hi_frac` at fixed phase, because the wash driver is one axis deeper (FX/swept-light
phase, item 3). S2 adds no new instruments and no seam; it converts S1's diagnosis into the correct
disposition: **upstream owner + free harness lever + a named S3 instrument charter.**

## Artifacts (S2)
STATUS append (this section); checkpoint `/tmp/wave11-checkpoints/A-S2.json`. No code, no new probes,
no re-golden (no binary change). S1 artifacts unchanged. Commits this stage: docs-only STATUS update.
