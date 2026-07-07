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
