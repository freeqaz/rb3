# Wave 9 — Kickoff Design (coordinator draft, for Fable review before dispatch)

**Author:** coordinator. **Status:** REVIEWED (Fable, `WAVE9_REVIEW.md`) — **all amendments adopted**;
dispatched with the corrected shape below.

## COORDINATOR ACCEPTANCE (2026-07-07) — final dispatched shape

Fable review returned **dispatch-with-amendments** (7). Adopted:

- **A1 — RefSkinVertex is NOT independent:** it reads the same `BoneOffsetAt()` + `WorldXfm()` the
  GPU palette composes (verified `test_skin_golden.cpp:164` vs `Rnd_Wgpu_RB3.cpp:3617`); (i)-vs-(ii)
  can only reveal palette fallbacks + WorldXfm cache freshness (the palette force-refreshes bone
  chains before sampling; a Poll-time CPU read doesn't — sample at the palette-compose point or a
  stale-cache artifact masquerades as "the wrong factor"). **(iii) is the primary comparison.**
- **A2 — S1 scoped to the TWO live candidates** (composition order / owner-vs-drawn / translation
  are already closed by source + prior probes): (a) unfaithful live-bone rotation *evolution* (pose
  pipeline) vs (b) offset conjugated against the wrong frame. (iii) executable without Dolphin:
  **(iii-a)** evaluate the CharClip channel directly vs the live bone's LocalXfm; **(iii-b)**
  compute the magnet-vs-per-member rest-basis ΔR from asset data and check the R·sin(θ) prediction
  against the recorded worst offenders (bone_R-thumb03, R=78.5).
- **A3 — the "<20u vs ~106u wext" hard exit CANNOT go green:** `wext` is the world-AABB diagonal
  (`:4322`) — a perfect skin still reads ~80-110u on hands_naked (bind diagonal 80.3u). **Hard exit
  restated: the BL-A2 `RealPathFixture` gtest GREEN** — S1 builds the dual-skin engine probe that
  populates `goldens/w2.8-farvert/live_pose.txt` (same instrument S1's factor table needs; engine
  probes allowed this wave).
- **A4 — Lane B force-reproduces WHITE first:** at the observed 1/6 rate a fix-free build passes an
  N=8 rate gate ~23% of the time. S1 exit = a deterministic/≥50% reproducer + the PP_OFF
  scene-vs-composite discriminator (weak prior: scene-side — residual WHITE luma 0.709 is sub-knee,
  and chroma-preserve passes blown-white through unchanged, chroma≈0). S2 gates on a paired
  continuous wash_score delta on the reproducer; N≥8 demoted to regression sweep.
- **A5 — `Rnd_Wgpu_RB3.cpp` single-writer = Lane A this wave.** If Lane B's localization comes back
  scene-side (world.cam lighting exposure lives in that TU), Lane B **stages its patch** (W3.3
  precedent) rather than landing it.
- **A6 — montage confounds:** gameplay cells need N≥3 boots/state + state-level judgment (director
  RNG persists under RB3_FIXED_CLOCK, proven 3×); the Wave-6 baseline is itself dirty
  (gameplay_default_1 is an anomalous pre-fix wash frame; the "default" arm is pre-placement-
  contract — use its placement_on frames for the placement axis); part_difficulty uses the W4.1
  settle-frame protocol (366-734); the Wave-6 dir is **archived** at
  `execution/baselines/wave6-current-state/`; Lane C gets the known-open list (finger shard, WHITE
  residual, grey/green skin-RTT) so it doesn't re-file them.
- **A7 — re-establish baselines on pin `a320f9d`** before any A/B (all prior hands numbers were on
  `a94762f`); compose site corrected to `:3617`.

---

_(Original draft below, retained for provenance; superseded where the acceptance above differs.)_

**Draft status:** DRAFT — under Fable pre-dispatch review, not yet dispatched.
Parent: `execution/README.md` (Wave 1–8 results + hard rules + Wave 9 menu). Engine pin `a320f9d`.

## Where we are (entering Wave 9)

Six default-ON fixes shipped. The venue wash + grayscale family closed in Wave 8 (chroma-preserve).
The one big open user-visible defect is the **finger shard** — three fix classes are dead with
numbers, and the Wave-8 refutation left a precise clue: *"rotation basis wrong, translation
correct."* Wave 9 is a **diagnosis-first wave**: no fourth blind fix; instead, name the wrong
factor empirically. Plus the Wave-8 disclosed WHITE over-exposure residual, and a re-baseline
screenshot sweep (five defaults have flipped since the last user-facing look).

## Proposed Wave 9 lanes

**Lane A — W2.8d hands bone-level attribution (Opus; engine probes allowed — the render backend is
unowned this wave):**
- **S1:** for ONE reproducible sharding finger vertex (the IK_SHARD_VERT worst offender on
  hands_naked), dump per contributing bone: the authored `invBind`/offset, the live `boneWorld`
  the palette samples, the weight, and the composed per-bone matrix — from (i) the GPU palette
  path (`owner->BoneOffsetAt(b) * boneWorld`, `Rnd_Wgpu_RB3.cpp:3529`), (ii) the CPU reference
  skinner (W0.1 `RefSkinVertex`), and (iii) if reachable, the Wii-faithful expectation (what
  `CharBonesMeshes`/the decomp math says the composition SHOULD be — source-derive, don't guess).
  Deliverable: a factor-by-factor table naming WHERE the composed matrix diverges (offset? bone
  world? composition order? owner-vs-drawn mesh?), with magnitudes. The Wave-8 clue ("rotation
  wrong, translation correct") predicts the divergence is in the rotation part of ONE factor —
  find which.
- **S2 (only if S1 names the factor unambiguously):** the minimal fix at the named factor,
  default-OFF, IK_SHARD_VERT A/B hard exit (<20u vs ~106u), W2.2 gate set. If S1 is ambiguous,
  S2 = the honest diagnosis writeup + Wave-10 design.

**Lane B — engaged-venue WHITE over-exposure (Opus; composite family, `RB3PostProc.*` +
`rb3_postproc.wgsl.inc` — same files as Wave-8 Lane A, now free):**
- The Wave-8 disclosed residual: hot ENGAGED venues can read WHITE (raw sub-knee over-exposure —
  the space `RB3_PP_LUMA_CEILING` was aimed at, whose highlight-only shape was refuted). S1:
  reproduce + quantify on the S2 dataset's WHITE captures (wash_score hi_frac); localize whether
  the over-exposure is scene-render (venue lighting hot) vs composite gain. S2: fix behind a
  default-OFF flag with a luminance-histogram gate (WHITE class rate → 0 over N≥8 wash-prone
  boots; authored bright moments — SP overlay, stage strobes — preserved), fail-red, byte-identical
  OFF, lineup. Chroma-preserve (now default-ON) is the baseline in all arms.

**Lane C — current-state re-baseline sweep (Sonnet, capture-only):**
- Re-run the Wave-6 capture protocol (same nav, same states: main_hub, song_select,
  part_difficulty, gameplay early/mid/late, band wide) on HEAD defaults; produce
  `/tmp/wave9-current-state/` + MANIFEST with anomaly notes; side-by-side montage vs
  `/tmp/wave6-current-state/` where states match. NO source edits. The coordinator reviews the
  montages (user-eyes proxy) and files new items for Wave 10.

**Deferred:** 4→8 lights (DC3 gates), W2.4 BandPatchMesh, song_select minor residuals,
`wave6-boxmap-proto` (shelved).

## Process rules (carried)

Commit-per-review-cycle (locks as before); checkpoints `/tmp/wave9-checkpoints/`; flags append-only
+ coordinator regens once; no pin bumps/flips by lanes. Defaults now ON: placement contract, black
head, hands rest-capture, text floor, hub quad, **chroma-preserve** (opt-out
`RB3_PP_CHROMA_PRESERVE_OFF`). `RB3_PP_LUMA_CEILING` and `RB3_HANDS_POSEAWARE` and
`RB3_HANDS_PERFRAME_CONJ` (refuted experiments) stay landed default-OFF — UNSET in all arms.

## Risks / open questions for the reviewer

- **R-A:** Lane A S1's three-way comparison — is the CPU reference (`RefSkinVertex`) actually an
  independent oracle here, or does it consume the SAME possibly-wrong offset/boneWorld inputs as
  the GPU palette (making (i)-vs-(ii) a no-op comparison and (iii) the only real ground truth)?
  Check what RefSkinVertex reads.
- **R-B:** Lane B's "WHITE class rate → 0" gate — is N≥8 statistically meaningful given Wave-8
  measured WHITE at low stochastic rates? Should the gate instead force-reproduce WHITE
  deterministically first (find the hot-input condition) before fixing?
- **R-C:** Lanes A and B both may touch engine files — A probes `Rnd_Wgpu_RB3.cpp`, B fixes
  `RB3PostProc.*`/WGSL. Disjoint enough? A's probes should be additive-only under a flag.
- **R-D:** anything in the six flipped defaults that invalidates Wave-6-vs-Wave-9 montage
  comparisons in Lane C beyond the obvious (they're the point of the comparison)?
