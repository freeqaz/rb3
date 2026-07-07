# Wave 11 — Kickoff Design (coordinator draft, for Fable review before dispatch)

**Author:** coordinator. **Status:** REVIEWED (Fable, `WAVE11_REVIEW.md`) — **all amendments
adopted**; dispatched with the corrected shape below.

## COORDINATOR ACCEPTANCE (2026-07-07) — final dispatched shape

Fable review: **dispatch-with-amendments** (9). Adopted:

- **A1 — Lane A leads with the named prime suspect:** under `RB3_FIXED_CLOCK` the seed is pinned
  (`System.cpp:397-407`, `0x5EED`) but all subsystems share ONE global `gRand` stream, and the
  light preset at a songMs-deterministic lighting event is a **global-RNG pick**
  (`LightPresetManager.cpp:258-263`); a preset rewrites env light colors AND type — retro-dicting
  Wave-8's per-boot dl/pl cluster split. S1 instruments preset-pick identity + a gRand
  draw-counter + the postproc source tuple FIRST, with the pre-registered prediction that preset
  identity partitions the boots and stratifies mid_sat.
- **A2 —** `RndPostProc::Current()` *identity* is uninformative (`world.pp` rewritten every frame);
  log the SOURCE TUPLE (`mPostProcA/B`/blend or shot `mCamPostProc`) + resolved ColorXfm values the
  native composite consumes.
- **A3 —** Wave-8's "byte-identical lighting inputs" exoneration was count-based and COLOR-BLIND
  (digest carries env/engaged/dl/pl only); add a per-light VALUE digest; do not inherit that claim.
- **A4 — S2 is a determinism SEAM, not de-randomization:** preset randomness is retail-faithful;
  pin selection only under `RB3_FIXED_CLOCK` (the 0x5EED/CharEyes precedent). If confirmed, the
  user-visible wash reframes as per-preset rendering fidelity — a follow-up item.
- **A5 — the R-C crux resolved:** "track the post-repoint bone" is the trivial-zero trap (the
  rebind bakes against that same bone). The corrected instrument = two palette-internal invariants
  at the existing probe site: **Tier 1** full-palette per-bone rest sweep with pointer-identity
  freshness validation; **Tier 2 (primary)** parent/child **joint-attachment**
  `‖j_b·P[parent] − j_b·P[child]‖` on the UPLOADED palette (`j_b = −off_b.v`) — ≈0 for any coherent
  palette at every pose, grows exactly as R·sin(θ), zero at rest, and **needs no rest capture**
  (killing the confound class that produced the W2.8e artifact; the old probe's "rest" was
  captured at an already-smeared frame since the `wext>60` gate enclosed the capture).
- **A6 —** GPU-vs-CPU-same-inputs reserved as the S2 branch instrument (V24/weights axis) if the
  tiers read GREEN while the smear reproduces; rest-neutrality/round-trip blind under animation.
- **A7 —** the instrument is the Wave-12 gate only if its per-frame trajectory **co-varies with
  the measured `hands_naked` worldExt (61→106u) on the same sighting frames**.
- **A8 —** probe-only + declared line ranges is sufficient (no serialization): Lane A ~1044-1660 +
  `RB3PostProc.*`, Lane B 4389-4671 + `BandCharacter.cpp` read-mostly; all probes getenv-gated;
  per-commit drawlog-792 flag-OFF re-runs; prefer rb3-side HX_NATIVE files for new instrumentation.
- **A9 —** Lane A S2 gates pre-registered: PRIMARY = mechanism identity (10/10 same preset pick +
  postproc tuple + gRand stream position), SECONDARY = mid_sat range < 0.05 (vs observed 0.295);
  PRIMARY-pass/SECONDARY-fail = a new finding to file, not a gate fudge.

---

_(Original draft below, retained for provenance; superseded where the acceptance above differs.)_

**Draft status:** DRAFT — under Fable pre-dispatch review, not yet dispatched.
Parent: `execution/README.md` (Wave 10 results + Wave 11 menu). Engine pin `6834744`.

## Where we are (entering Wave 11)

Six default-ON fixes shipped and holding. Wave 10 ended with zero flips and two hard walls, both of
which point at MEASUREMENT problems before any further fixing:

1. **Per-boot lighting nondeterminism (BOOTRNG)** — at a pinned shot, pinned songMs, under
   `RB3_FIXED_CLOCK`, `mid_sat` swings 0.067–0.362 and `hi_frac` ±18 across boots. It made the
   WHITE gate non-resolving (sign-flipping deltas), it IS the residual stochastic wash the user can
   see, and it invalidates every future arm-mean visual gate. Root-causing it unblocks three items
   at once (WHITE real-lever, wash residual, gate trustworthiness).
2. **Hands instrument contradiction (W2.8f)** — five bind-side fix classes are dead; the Wave-10
   diagnostic proved the dual-skin metric compared against a pre-repoint stale bone (unsatisfiable
   without freezing). Yet A.S3 confirmed the floating-forearm sightings are REAL vertex smear
   (61→106u worldExt, wrist attached, body coherent). The instruments disagree
   (characterize=MARGINAL vs visible smear): before any 6th fix idea, build the RIGHT instrument
   and reconcile.

This is a two-lane diagnosis wave. No fix landings are expected; the exits are named mechanisms
and trustworthy instruments.

## Proposed Wave 11 lanes

**Lane A — BOOTRNG root-cause (Opus; owns `Rnd_Wgpu_RB3.cpp` + `RB3PostProc.*` probes,
additive/probe-only):**
- **S1:** instrument per boot at the pinned `coop_dir_crowd.shot`/songMs≈21000 capture (N≥10,
  default build): everything that plausibly varies — `RndPostProc::Current()` identity + its
  grade/bloom/vignette params at the shot, which venue events/light presets have fired, environ
  identity, the world.cam exposure inputs, and the already-instrumented engagement state
  (`RB3_WASH_PROBE`). Correlate against the per-boot `wash_score` (mid_sat/hi_frac/class).
  Deliverable: the boot-varying STATE named (which variable differs between a 0.067 boot and a
  0.362 boot), with per-boot evidence tables. If the varying state is upstream of the render
  (loader/event order), say so and name the upstream owner — do NOT chase it out of lane.
- **S2 (conditional):** if the mechanism is in-lane (render/postproc state selection), a
  deterministic fix flag-first with a variance gate (per-boot mid_sat spread collapses, N≥10 per
  arm — a VARIANCE gate, not an arm-mean gate, so it resolves despite the floor it is removing);
  else the staged/backlog outcome with the named owner.

**Lane B — W2.8f the right hands instrument (Opus; `BandCharacter.cpp` read-mostly + the dualskin
probe block in `Rnd_Wgpu_RB3.cpp` — coordinate: Lane A's probes are in the postproc/scene region,
Lane B's in the DrawMesh region; declare exact line ranges in PLAN.md before editing):**
- **S1:** rebuild the dual-skin reference to track the POST-repoint live bone (`own`, the bone the
  draw actually uses) — the coherent-vs-drawn comparison the Wave-10 refutation demanded. Re-run
  the RealPathFixture protocol with the corrected reference: does a real residual survive on
  hands/fingers, and how big?
- **S2:** reconcile with the visible evidence: capture the A.S3 forearm sighting with the corrected
  instrument live — if the corrected metric reads ~0 while the smear is visible, the defect is NOT
  in the palette-vs-reference axis at all (candidates: authored-vert/weight interpretation, V24
  path, per-vertex weight normalization) and the lane must say which axis the evidence now points
  to. If the corrected metric DOES flag the smear, we finally have a fail-red instrument that
  tracks the symptom — the Wave-12 fix gate. Either way: verdict + instrument committed, no fix.

**Deferred:** WHITE real-lever (needs BOOTRNG), 4→8 lights, W2.4, song_select residuals.

## Process rules (carried)

Locks, checkpoints (`/tmp/wave11-checkpoints/`), commit-per-review-cycle, append-only flags +
single coordinator regen, no pin bumps/flips by lanes. Six defaults ON; all refuted-experiment
flags UNSET in all arms (now incl. `RB3_APPENDAGE_ASSET_REBAKE`).

## Risks / open questions for the reviewer

- **R-A:** Lane A's state-instrumentation list — what am I missing as a candidate boot-varying
  input? (Texture/asset residency timing? The known async-loader completion order — W0.3d part-b —
  feeding not draw ORDER but EVENT order? Crowd/venue LOD?) Verify against source which of these
  can actually alter lighting/grade state at a fixed shot.
- **R-B:** Two lanes, one TU again (probe regions). Same A1-style serialization needed, or is
  probe-only + declared-line-ranges acceptable?
- **R-C:** Lane B S1's "corrected reference" — is the post-repoint bone actually samplable at the
  palette-compose point without re-introducing the freeze problem (the reference must ANIMATE with
  the drawn bone; a same-bone comparison could be trivially zero — what non-trivial invariant does
  the corrected instrument actually check)? This is the crux: define the invariant precisely or
  the lane re-derives another confounded metric.
- **R-D:** Is a variance gate (S2) actually resolvable at N≥10, given the observed spread?
