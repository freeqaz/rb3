# BOOTRNG — Stage A.S1 PLAN (Wave 11, Lane A, Opus) — PRIME-SUSPECT-FIRST INSTRUMENTATION

**Checkpoint id:** `A-S1` → `/tmp/wave11-checkpoints/A-S1.json` (check first, write before return).
**Mode:** DIAGNOSIS-ONLY. No fix landings. Exit = the boot-varying STATE named, with per-boot
evidence, and (if upstream of render) the upstream owner named. Six defaults ON; refuted flags UNSET.

## Pre-registered hypothesis (H-A, from WAVE11_REVIEW A1 — the named prime suspect)

Under `RB3_FIXED_CLOCK` the boot **seed** is pinned (`System.cpp:397-407`, `0x5EED`), but every
RNG-driven subsystem shares ONE global stream (`gRand`, `Rand.cpp:6`). The stream **position** at a
songMs-deterministic lighting event is a function of every prior consumer, and the light preset at
that event is a **global-RNG pick** (`LightPresetManager.cpp:258-263`,
`mPresets[s][RandomInt(0,count)]`). A different preset rewrites env light colors AND light type +
can carry a postproc.

**PREDICTION (pre-registered, will be scored against the data):**
1. Preset-pick identity (the tuple of picks fired before the pinned capture) **partitions** the
   N≥10 boots into distinct classes.
2. Those classes **stratify** `mid_sat` / `hi_frac` — i.e. a 0.067 boot and a 0.362 boot report
   DIFFERENT preset picks and/or a different gRand stream position at capture.
3. Within a preset-identical / stream-position-identical subset, the `mid_sat` spread **collapses**
   toward the frame-level floor.

If the prediction FAILS, the broad sweep instruments (LIGHTVAL value digest, PPSRC, PPRESOLVED,
gRand counter) are still logged and correlated — nothing is lost; a different boot-varying state is
then named from those.

## Instruments (all getenv-gated by `RB3_BOOTRNG_PROBE`; rb3-side HX_NATIVE where possible; additive)

| # | Signal (WAVE11_REVIEW ref) | File | Emits |
|---|---|---|---|
| 1 | gRand stream position (A1) | rb3 `src/system/math/Rand.cpp` + `Rand.h` | `Rand::Int()` increments a global draw counter for the `gRand` instance under HX_NATIVE; `RB3GRandDrawCount()` getter. |
| 2 | preset-pick identity + draw-counter (A1) | rb3 `src/system/world/LightPresetManager.cpp` | `[BOOTRNG] PRESET cat=<sym> idx=<i>/<count> preset=<name> gdraw=<N>` at each `PickRandomPreset` (single RNG draw preserved). |
| 3 | postproc SOURCE TUPLE (A2, not `Current()` identity) | rb3 `src/system/bandobj/BandDirector.cpp` Poll | `[BOOTRNG] PPSRC src=<camera\|music video light presets\|song authoring> p1=<name> p2=<name> blend=<f> gdraw=<N>` (throttled). |
| 4 | per-light VALUE digest (A3, color-blind claim NOT inherited) | engine `Rnd_Wgpu_RB3.cpp` (declared range) | `[BOOTRNG] LIGHTVAL env=<> valhash=<hex> dl=<> pl=<> dsum=<> psum=<> amb=(r,g,b) gdraw=<N>` at world.cam engaged write. |
| 5 | resolved ColorXfm the composite consumes (A2) | engine `RB3PostProc.cpp` (declared range) | `[BOOTRNG] PPRESOLVED pp=<name> con=<> bri=<> sat=<> vig=<> lvlInLo/Hi lvlOutLo/Hi` (throttled). |
| 6 | engagement state (existing) | reuse `RB3_WASH_PROBE` `[WASHPROBE] SCENE ...` | inherited. |

## Declared line ranges (A8 — no serialization; escalate if outside)

- Engine `src/platform/Rnd_Wgpu_RB3.cpp`: **~1044–1660** (WriteSceneUniforms engaged branch, LIGHTVAL
  digest additive within the existing venue-light loop region).
- Engine `src/platform/RB3PostProc.cpp` / `.h`: RunPostProcComposite uniform-fill region (~216–246).
- rb3-side (HX_NATIVE, match-neutral, not shared with Lane B): `src/system/math/Rand.{h,cpp}`,
  `src/system/world/LightPresetManager.cpp`, `src/system/bandobj/BandDirector.cpp`.

Lane B is in `Rnd_Wgpu_RB3.cpp` **4389–4671** + `BandCharacter.cpp` (read-mostly). Min gap ≈1900 lines.

## Build / run

- Build dir: `native/build-agent-BOOTRNG` (own; never touch build-native / build-web*).
  `flock /tmp/rb3-native-build.lock`.
- Harness: `bootrng_probe.py` (new, under BOOTRNG/), reuses `wash-measure.capture_pinned` +
  `wash_score` + `tonal_band_sat`, pins `coop_dir_crowd.shot`, songMs≈21000±2000, N≥10, default
  build (six defaults ON, refuted flags unset), `RB3_BOOTRNG_PROBE=1 RB3_WASH_PROBE=1
  RB3_FIXED_CLOCK=1`.
- Parse each boot's `.engine.log` for the `[BOOTRNG]` streams (tail = captured frame) + score PNG.
- Partition boots by (preset-pick tuple, valhash, gdraw@capture); report within-partition mid_sat
  spread vs cross-partition.

## Exit criteria (A.S1 deliverable)

- `BOOTRNG/STATUS.md`: the boot-varying STATE **named** with per-boot evidence tables; H-A scored
  (partition + stratification numbers). If the varying state is UPSTREAM of render (loader/event
  order → gRand divergence), name the upstream owner (W0.3d part-b async-completion order) and STOP —
  an in-lane fix belongs to S2 only if the mechanism is render/selection-side.
- Every probe commit: `drawlog-golden.py --fixed-clock --canonical-order` == 792 with all flags OFF
  (WAVE11_REVIEW A8, byte-identical proof).
- Commit probes (rb3-side + declared engine ranges) under the standing flock locks; STATUS + checkpoint.

## Per-commit verification ledger

- flag-unset byte-identical: drawlog 792 canonical after each probe commit.
- staging own files only; `flock /tmp/rb3-git.lock` (rb3), `flock /tmp/milo-engine-git.lock` (engine).
