# Wave 11 — Pre-dispatch Review (Fable adversarial pass)

**Reviewer:** Fable subagent. **Input:** `WAVE11_KICKOFF.md` (draft), README Waves 8–10,
`W2.8e/STATUS.md` (A.S1/A.S2/A.S3), `WHITE-fix/STATUS.md` (Wave-8 A.S1 + Wave-10 B.S2),
`W0.3d/STATUS.md` part-b, plus source spot-checks (appendix). Engine pin `6834744` verified
== engine HEAD (working tree clean except the concurrent audio agent's `FxSendNative.cpp`).

## VERDICT: **dispatch-with-amendments**

The two-lane diagnosis structure is right, the no-fix discipline is right, and the lane
scoping is nearly collision-free. But (1) Lane A as written is a "correlate everything"
sweep when the source already names a single overwhelmingly likely mechanism that a
one-variable instrument would confirm or kill in one run — the plan should lead with it;
(2) Lane A's stated instrument list contains one item that is mechanically uninformative
as written (`RndPostProc::Current()` *identity*) and leans on a Wave-8 exoneration
("byte-identical lighting inputs") that is count-based and color-blind; (3) the R-C crux:
the kickoff's own S1 wording ("rebuild the dual-skin reference to track the POST-repoint
live bone") walks straight into the trivial-zero trap R-C worries about — the corrected
instrument must be specified as palette-internal invariants (per-bone sweep + parent/child
joint-attachment), not a repaired same-bone dual-skin; (4) the S2 variance gate needs a
mechanism-identity primary form or it can fail a correct fix.

---

## Amendments

### A1 — Lane A: name the prime suspect and instrument it FIRST (the preset-pick RNG chain)

The kickoff's S1 is a broad correlate-everything sweep. The source supports a specific,
cheap-to-confirm causal chain, and Lane A should lead with it as pre-registered H-A:

- Under `RB3_FIXED_CLOCK` the boot **seed** is pinned (`System.cpp:397–407`, `seed=0x5EED`),
  but **every RNG-driven subsystem shares ONE global stream** (`gRand`, `Rand.cpp:6`). The
  stream **position** at any songMs is a function of every prior consumer — and the consumer
  set is huge (`CharClipDriver`/`CharClipGroup` clip picks, `Crowd`, `Part`/`PartLauncher`,
  `EventTrigger`, `CameraShot` shake `:265–274`, `RndPostProc` flicker `:141–142`,
  `AnimFilter`, `Wind`, …). Any boot-variant consumption count upstream (the W0.3d part-b
  async-loader/worker completion-order residual — diagnosed, staged, **its object-list
  insertion/poll-start effects still unlanded**) permutes every later draw.
- Lighting events at a pinned songMs are deterministic in *category*, but the preset within
  the category is a **global-RNG pick**: `LightPresetManager::SetLighting` →
  `PickRandomPreset` → `mPresets[s][RandomInt(0, count)]` (`LightPresetManager.cpp:250–263`).
- A `LightPreset` animates **environment light color AND light TYPE**
  (`LightPreset.h:28–65`, `EnvironmentEntry` carries `RndLight::Type mLightType`) plus
  spotlights, and can carry a postproc (`GetCurrentPostProc`, `LightPreset.cpp:299`). A
  different preset pick therefore changes exactly the state the venue path reads.
- **This chain retro-dicts the Wave-8 data.** WASH-fix A.S1 already observed the boots
  splitting into two *lighting clusters* at the same shot — `chars.env dl=1/pl=0` vs
  `dl=0/pl=1` ("a boot-nondeterminism orthogonal to wash", `WHITE-fix/STATUS.md` H1 table
  notes). A directional-vs-point **count flip on the same env is the fingerprint of two
  different presets** (the only in-source object that rewrites a light's type per event is
  the preset's `EnvironmentEntry`). Nobody has yet logged *which preset* each boot picked.

Concrete S1 amendment: before (or alongside) the broad sweep, instrument
(a) selected preset name per `SetLighting`/`SelectPreset` call (rb3 `LightPresetManager.cpp`,
HX_NATIVE-gated addition), (b) a `gRand` draw-counter (flag-gated counter in `Rand.cpp`
consumers or the `Rand` class) sampled at the pinned capture, (c) the BandDirector postproc
source tuple (A2). Pre-register the prediction: *preset identity partitions the N≥10 boots
into the Wave-8 dl/pl clusters and stratifies `mid_sat`/`hi_frac`; within a preset-identical
subset the spread collapses toward the frame-level floor.* If that prediction fails, the
broad sweep is still there — nothing is lost; if it holds, S1 is done in one run and the
"upstream owner" (gRand stream divergence ← loader completion order) is named with numbers.

### A2 — Lane A: `RndPostProc::Current()` *identity* is uninformative on the gameplay path — log the source tuple

`BandDirector::Poll` **rewrites the contents of one object** (`world.pp`,
`BandDirector.cpp:149–156`) every frame via `mWorldPostProc->Interp(mPostProcA, mPostProcB,
mPostProcBlend)` — or `Copy(mCamPostProc)` when a shot carries one — (`BandDirector.cpp:283–299`).
So at the pinned shot `Current()` will read the *same name* every boot while its grade
params (`saturation`/`contrast`/levels — exactly what the native composite consumes,
`RB3PostProc.cpp:226–246`) differ. The instrument must log the tuple the game itself
computes for its debug overlay: source string ("camera" / "song authoring" / "music video
light presets"), `p1`/`p2` names, blend (`UpdatePostProcOverlay` inputs) — plus the
*resolved* ColorXfm values actually uploaded. Note: `mColorModulation` (the gRand flicker,
`PostProc.cpp:133–147`) is **not consumed by the native composite** (absent from the
`PostProcUniforms` fill) — it cannot be a direct visual mover natively, but it IS a stream
consumer (A1), so log its firing count, not its value.

### A3 — Lane A: do not inherit Wave-8's "byte-identical lighting inputs" as covering light colors

The claim (WASH-fix A.S1 H1) was derived from the `sWashDigest` tuple —
`env/engaged/miss/dl/pl/greykey` **counts only** (`Rnd_Wgpu_RB3.cpp:1079–1092`). Two
different presets with the same light-count signature but different colors/intensities are
indistinguishable to it. The kickoff's Lane A brief should say explicitly that the Wave-8
exoneration covers the *engagement machine*, not the *light values*, and add a per-env
light-value digest (e.g. hash of per-light `GetColor()`/type/`Showing()` over
`mLightsApprox`+`mLightsReal`) to the probe so "identical lighting inputs" becomes a
value-level claim this time.

### A4 — Lane A S2: the fix is a determinism SEAM, not de-randomization of the shipping path

`PickRandomPreset`'s randomness is **retail-faithful decomp code** — the Wii rolls the same
dice. If H-A confirms, the correct in-lane fix is: pin the RNG-driven selection **only under
`RB3_FIXED_CLOCK`** (mirroring the `0x5EED` seed pin, `System.cpp:397–407`, and the W0.3d
CharEyes RNG freeze precedent, rb3 `c6b961da`) — e.g. a fixed-clock-gated deterministic pick
(first-of-category or a dedicated seeded Rand) so gates and captures become boot-stable. The
shipping path keeps authored randomness. Consequence the kickoff should state: if BOOTRNG =
faithful preset randomness, then the *user-visible* "residual stochastic wash" reframes as
**per-preset rendering fidelity** (some presets render hotter/greyer natively than on Wii)
— a follow-up fix-class item (per-preset exposure, the WHITE real-lever), not something Lane
A may "fix" by removing randomness default-ON. Guard against that scope error in the brief.

### A5 — R-C (the crux): the corrected reference as drafted is the trivial-zero trap; specify palette-internal invariants instead

Lane B S1's wording — "rebuild the dual-skin reference to track the POST-repoint live bone"
— reproduces the confound in mirror image. Per A.S2's own mechanism, the default rebind
bakes `off = inv(rest(own))` against the same live bone the palette evolves
(`BandCharacter.cpp:1499–1533` region): a reference `v·inv(rest_own)·live_own` compared to
`asDrawn = v·(off·live_own)` is **~0 by construction** whenever the rebind did its job —
it measures the rebind's bookkeeping, not the defect. The instrument that is (1) non-trivial,
(2) tracks the visible R·sin(θ) smear, and (3) computable with the existing probe machinery
is **palette-internal consistency**, two tiers, both at the existing dualskin site:

- **Tier 1 — full-palette per-bone rest sweep (pose-independent amplitude predictor).**
  For EVERY bone in the mesh's palette (not the single worst/dominant bone A.S2 dumped),
  check `angle(off_b · restW_b, I)` with a **freshness-validated** rest: store the
  `BoneTransAt(b)` *pointers* at capture and invalidate/recapture when the pointer changes
  (the repoint IS a pointer change — this mechanically kills the W2.8e stale-bone confound;
  the probe already reads `mNativeBonesRebound` at `:4625`). Any bone with a large residual
  = a stale/mis-baked palette *entry* — the mixed-palette hypothesis, which the worst-bone
  DIAG could not see and which exactly produces "wrist attached, fingertips fling". Its
  angle feeds the existing `R·2sin(θ/2)` predictor (`:4632`).
- **Tier 2 — parent/child joint-attachment (the PRIMARY, pose-tracking metric).** For each
  palette bone `b` with parent `p` (via `TransParent()`, `:4605`, pointer-matched into the
  owner's bone list), the authored child-joint position `j_b = −off_b.v` (already computed
  as the R radius base, `:4545`) must map to the same world point under BOTH uploaded
  palette matrices: `‖j_b·P[p] − j_b·P[b]‖` using `bones.bones[]` directly. For any coherent
  palette this is ≈0 at **every** pose (rotation-only articulation with constant bone
  lengths: both sides equal the live joint position); a wrong-basis factor on `b` makes it
  grow as `R·sin(θ_pose)` — the exact functional form and localization of the visible smear
  — and it is zero at rest, matching the symptom (hands fine at rest, smear when the forearm
  raises). Crucially it needs **no rest capture at all** (only the currently-uploaded
  palette + authored offsets), eliminating the entire capture-timing confound class that
  produced both the W2.8e artifact and the probe's other latent flaw: the current `bw`
  capture is keyed on the first frame `wext > 60u` — i.e. "rest" is captured at the first
  *already-smeared* frame (`:4413` gate encloses `:4459–4472` capture), another reason no
  rest-capture-based reference should be trusted as the primary metric.
  Tolerances: calibrate on a known-clean body mesh in the same run (e.g.
  `greaserjacket_resource.mesh`, coherent at ~50u/ratio≈1.0 per A.S3) as the in-run negative
  control; fail-red by perturbing one palette entry's rotation (~0.15 rad → ≈R·0.15 jump).

### A6 — R-C candidate dispositions (evaluated against source)

- **Per-vertex |gpuSkinned − CPU-composed-with-same-post-repoint-inputs|:** the probe's
  `asDrawn` already IS the CPU composition of the exact uploaded palette (`:4497–4522`), so
  the only new information is GPU-side (WGSL indexing, V24 vertex decode, shader weight
  normalization) and it requires **new readback machinery** (staging copy / compute). It
  cannot see palette-input faults (both sides share them) so it does not track the smear if
  the fault is where all evidence points. Disposition: do NOT build in S1; it is the
  designated **S2 branch instrument** for the "tiers read GREEN yet A.S3 smear reproduces"
  outcome, which moves the axis to authored-vert/weight interpretation exactly as the
  kickoff anticipates.
- **Rest-pose-neutrality / authored-geometry round-trip at rest:** both are zero at rest *by
  construction of the default rebind* and therefore cannot track a pose-growing smear; they
  survive only as Tier-1's per-bone sweep form (where the value is the SWEEP over all
  entries + freshness, not the rest identity itself).
- **Inter-bone consistency:** adopt in the joint-attachment form of A5/Tier-2 (the
  "relative transforms vs authored relative transforms" phrasing requires clip knowledge;
  joint-attachment is the clip-free equivalent).

### A7 — Lane B S2: "tracks the symptom" must be established by co-sampling, not co-existence

Accept the corrected instrument as the Wave-12 fix gate only if its per-frame trajectory
**co-varies with the visible smear on the same frames**: run an A.S3-style sighting capture
with Tier-2 live and require the joint-attachment worst value to rise and fall with the
measured `hands_naked` worldExt (61→106u trajectory), not merely to be RED somewhere in the
run. Otherwise Lane B re-delivers a metric that is RED for its own reasons — the exact
failure mode being repaired.

### A8 — R-B: probe-only + declared line ranges is sufficient; no A1-style serialization needed

Measured regions in the shared TU (`Rnd_Wgpu_RB3.cpp`, 5,582 lines): Lane A's existing +
proposed surface = helpers `~1044–1230` (wash probe `1063–1092`), `WriteSceneUniforms`
`1333–1650` (digests at `1606`/`1635`), composite trigger sites `~1830–1960` and
`~2240–2270`, DrawMesh env-staleness probe `~2480–2520`, plus the separate `RB3PostProc.{h,cpp}`
TU. Lane B = the dualskin block `4389–4671` (the Wave-10 audit measured its hunks as
`4402–4650`) + `BandCharacter.cpp` read-mostly. Minimum gap ≈1,900 lines; textual conflict
is implausible, and the WHITE-fix B.S2 audit already demonstrated the verification pattern
(grep the other lane's symbols over the diff → empty). Conditions to carry into the briefs:
(i) every new probe getenv-gated, flag-unset byte-identical, each lane re-runs drawlog-792
flag-OFF before each commit (both lanes will be building from a tree containing the other
lane's uncommitted gated probes — that is fine only while gating holds); (ii) git add/commit
under the standing flock locks, staging only own files; (iii) any edit outside the declared
ranges, or to shared helpers, escalates to the coordinator; (iv) note that much of Lane A's
NEW instrumentation belongs in rb3-side files anyway (`LightPresetManager.cpp`,
`BandDirector.cpp`, `Rand.{h,cpp}` — all HX_NATIVE-gated additions, match-neutral for the
MWCC build), shrinking the shared-TU surface further.

### A9 — R-D: a raw variance gate at N≥10 is resolvable only for a fully-deterministic fix — pre-register a mechanism-identity primary gate

Observed per-boot `mid_sat` {0.219, 0.067, 0.362} (SD≈0.15, range 0.295) is plausibly
**discrete/multi-modal** (a handful of presets), for which an SD/F-test is the wrong shape:
at n=10/arm an F-test reliably resolves variance ratios ≳4 — trivial if the fix collapses
the spread to frame-level (~0.01), but marginal if a second un-pinned RNG consumer leaves a
partial collapse, and a correct fix could then "fail" the gate. Pre-register two levels:
**PRIMARY = mechanism identity** — 10/10 boots report the same preset pick(s), the same
postproc source tuple, and the same gRand stream position at the pinned capture (this is
what "deterministic" means, and it resolves at N=10 with zero statistical machinery);
**SECONDARY = visual confirm** — `mid_sat` range < ε (pre-register ε≈0.05 vs the observed
0.295) and `hi_frac` range < ~5 (vs observed ±18). If PRIMARY passes and SECONDARY fails,
that residual is a NEW finding (a non-RNG mover) to file, not a gate fudge.

---

## R-A / R-B / R-C / R-D — direct answers

**R-A (what's missing from the state-instrumentation list):** the global-RNG **stream
position** and the **preset pick identity** — the two variables that tie the whole list
together (A1). The kickoff's own items check out in source as plausible movers, with one
correction: `Current()` identity is constant on the gameplay path; log the source tuple
(A2). Loader completion order is confirmed still-live upstream (W0.3d part-b staged/unlanded;
the Wave-5 SortDraws tie-break fixed submission SORT, not completion-timing effects on
poll-start/event order — `W0.3d/STATUS.md:203` names the same root). Asset/texture residency
is a legitimate secondary (it changes pixels, not lighting state) — keep it in the sweep but
below H-A. Crowd idle-anim variation (gRand) is directly in-frame for a crowd shot — same
root, covered by the stream-position probe.

**R-B:** probe-only + declared ranges acceptable; ranges and conditions in A8.

**R-C:** yes, the post-repoint bone is samplable at the palette-compose point (the palette
IS post-repoint; `BoneTransAt` is queried live at `:4511`; the freeze arose from *forcing
the mesh onto the static bone as a fix*, not from sampling) — but a same-bone corrected
dual-skin is trivially ~0 whenever the rebind is self-consistent, so it cannot be the
instrument. The non-trivial invariants are A5's Tier-1 (full-palette freshness-validated
rest sweep — catches stale/mixed entries, predicts amplitude) and Tier-2 joint-attachment
(pose-tracking, rest-capture-free, catches wrong-basis conjugation AND mixed-skeleton
palettes — the two surviving hypotheses for a real residual). Candidate dispositions in A6.

**R-D:** resolvable at N≥10 **iff** the gate's primary form is mechanism identity (A9); a
pure arm-variance gate risks failing a correct-but-partial determinism fix and cannot
distinguish "fix removed the mechanism" from "lucky draw of the same mode".

---

## Source appendix (evidence for the claims above)

All paths rb3 repo unless noted; engine = `/home/free/code/milohax/milo-native-engine` @ `6834744`.

- `src/system/os/System.cpp:386–408` — boot seed: replay seed → `RB3FixedClockActive()` →
  `seed = 0x5EED` → `SeedRand(seed)`; `srand(RandomInt())`. Seed pinned under fixed clock;
  stream position is not.
- `src/system/math/Rand.cpp:6,65–84` — single global `gRand(0x29A)`; `RandomInt/RandomFloat`
  all draw from it.
- Global-RNG consumers (grep `RandomInt|RandomFloat`): `world/LightPresetManager.cpp`,
  `world/Crowd.cpp`, `world/CameraShot.cpp:265–274` (shake), `char/CharClipDriver.cpp`,
  `char/CharClipGroup.cpp`, `rndobj/Part.cpp`, `rndobj/PartLauncher.cpp`,
  `rndobj/EventTrigger.cpp`, `rndobj/PostProc.cpp:141–142` (flicker), `rndobj/Wind.cpp`,
  `rndobj/AnimFilter.cpp`, `bandobj/BandCharacter.cpp`, `bandobj/BandDirector.cpp`, et al.
- `src/system/world/LightPresetManager.cpp:250–263` — `SetLighting` →
  `PickRandomPreset(s)` → `mPresets[s][RandomInt(0, count)]`.
- `src/system/world/LightPreset.h:23–65` — preset stores `EnvironmentEntry` (ambient/light
  colors AND `RndLight::Type mLightType`) + `SpotlightEntry`; `LightPreset.cpp:299`
  `GetCurrentPostProc`.
- `src/system/bandobj/BandDirector.cpp:149–156` — `mWorldPostProc = world.pp`;
  `:264–303` `Poll()` rewrites `world.pp` per frame from `mCamPostProc` (shot) OR light
  presets (music video) OR `Interp(mPostProcA, mPostProcB, mPostProcBlend)` ("song
  authoring", set via DTA handler `:1727–1729`).
- engine `src/platform/RB3PostProc.cpp:226–246` — native composite consumes
  saturation/contrast/brightness/levels/vignette from `Current()`'s ColorXfm;
  `mColorModulation` (flicker) absent → visually inert natively, still a stream consumer.
- engine `src/platform/Rnd_Wgpu_RB3.cpp:1079–1092` — `sWashDigest` fields =
  env/engaged/miss/dl/pl/greykey (counts only → color-blind, A3);
  `:1333` `WriteSceneUniforms`; `:1467` engagement condition (world.cam + `venv` +
  `mAmbientFogOwner`); `:2397` `DrawMesh`; `:2506` staleness probe; `:4389–4671` dualskin
  block — capture gate `:4411–4413` (`wext > 60` encloses the rest capture `:4459–4472`,
  so "rest" = first already-smeared frame), live `BoneTransAt` at `:4511`, authored joint
  radius base `−off.v` at `:4545`, `TransParent` chain `:4605`, `mNativeBonesRebound` in
  DIAG `:4625`, `R·2sin(θ/2)` predictor `:4632`. TU is 5,582 lines → ≈1,900-line gap
  between the lanes' regions.
- `src/system/bandobj/BandCharacter.cpp:1418–1433` (GeomOwner propagation),
  `:1424–1439` two-pass all-or-nothing rebind comment, `:1440–1533` resolve/capture logic
  incl. the A.S2 `RB3_APPENDAGE_ASSET_REBAKE` site and the `Find(name)` repoint semantics —
  the bound/own 106°/129° provenance story flip-flops between A.S1 and A.S2 across these
  captures, which is itself evidence the Wave-11 instrument must not depend on any rest/
  provenance capture (A5 Tier-2 doesn't).
- `WHITE-fix/STATUS.md` Wave-8 A.S1 H1 table + notes — dl/pl **cluster split across boots at
  identical songMs** (cluster A `chars.env dl=1/pl=0`, cluster B `dl=0/pl=1`) = the
  preset-pick fingerprint (A1); Wave-10 B.S2 `:494–532` — per-boot eng_hot OFF
  `hi_frac {40.9, 65.2, 28.7}`, `mid_sat {0.219, 0.067, 0.362}`, null-control swing ±5–18.
- `W0.3d/STATUS.md:190–215` — part-b staged-not-landed; SortDraws tie-break (landed Wave 5,
  `76f51077`) fixes submission ORDER only; upstream async-worker completion/allocation
  nondeterminism named as the shared root.
- `src/system/world/CameraManager.cpp:21–22,58–84,364–366` — shot shuffle uses a SEPARATE
  seeded `sRand` (`sSeed` static, DTA-settable) — shot order is seed-deterministic, but shot
  *history* before the pinned capture shot still differs per boot via the global stream
  (shake, director picks), so history-carried state (interp blends, spotlight states) must
  be in the S1 log.
