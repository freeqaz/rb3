# Wave 10 — Kickoff Design (coordinator draft, for Fable review before dispatch)

**Author:** coordinator. **Status:** REVIEWED (Fable, `WAVE10_REVIEW.md`) — **all amendments
adopted**; dispatched with the corrected shape below.

## COORDINATOR ACCEPTANCE (2026-07-07) — final dispatched shape

Fable review returned **dispatch-with-amendments** (8). Adopted:

- **A1 — serialize via step 0:** Lane B's engine patch is tiny + pre-verified + default-OFF →
  **pre-land it as serialized STEP 0** (gated by one drawlog-792 + milo-engine-tests run); Lane B
  then goes engine-read-only and **Lane A is `Rnd_Wgpu_RB3.cpp`'s single writer**. (Regions are
  verified disjoint, but sub-file `git add` granularity + one shared working tree make dual-writer
  unenforceable.)
- **A2 — the "RealPathFixture GREEN flag-ON" exit was impossible as written:** the gtest reads a
  COMMITTED STATIC capture (no code fix turns it green), and the probe's `wext > 60` entry gate
  would suppress the instrument exactly when the fix works. **Two-fixture protocol** (committed
  flag-OFF fixture stays RED; fresh flag-ON re-capture reads <20u) + parameterize the probe's
  capture gate.
- **A3 — instrument validity first:** the fixture's sep metric plausibly carries the same
  placement-yaw confound S2 proved for ΔR (asDrawn placement-anchored vs first-capture-anchored
  ref; 32.8u sits inside the 27-60u placement range). **S1 re-derives the fixture reference in
  like-space, re-commits, and RE-PROVES IT RED before S2 starts** — "no longer RED" is a
  legitimate premise-death stop. The placement-independent **wext A/B (RED 105-107u) is a
  mandatory S2 exit** with a pre-registered target; "ΔR collapses toward 0" is only a gate if S1
  shows it measurably nonzero pre-fix.
- **A4 — premise wording corrected + stop rule made mechanical:** extraction needs **no new milo
  parsing** (pre-deform capture hook / scratch DirLoader side-load / `rb3-viewer --pose-dump`),
  BUT `skeleton_unshared.milo` is **male-bind, same file per member** (BandCharacter.cpp:3748) —
  a "per-member authored bind" may not exist as asset data, and the 87.3° vs 68.8° spread may be
  pure placement yaw. S1 defines the extraction target operationally and writes
  `verdict: MATCH|NO_MATCH` (pre-registered tolerances) to its checkpoint; **S2's charter line 1
  reads it and exits with NO code on NO_MATCH** (the 5th-class stop rule, mechanical). The rebake
  slots into the existing `RB3_APPENDAGE_REST_ROT` site inside `RebindHeadHandsAtRest`
  (once-latch + rebound flags; the default rebake destroys the pristine authored offset S1 may
  need) — **one space end-to-end** (frame-mixing killed W2.8c).
- **A5 — DC3 zero-blast VERIFIED:** both `SceneUniforms` constructions value-initialize; nothing
  writes `_padPL`; WGSL never reads the pads; static_assert 656 holds; the Dawn validation gtest
  guards the WGSL. Accept the repurpose (pointFalloffMode precedent).
- **A6 — forearm triage is a flip EXHIBIT, not a land gate**, and gains **H3**: the rest of the
  character guard-DROPped leaving one legitimate forearm (the V24 mixed-palette "only teeth/eyes
  render" family, BandCharacter.cpp:1416-1428). One drawlog at the sighting separates H1/H2/H3;
  capture the flag-OFF baseline first; "H1 confirmed AND fix doesn't collapse it" = flip-blocking
  evidence.
- **A7 — WHITE gates are paired continuous deltas** (hi_frac ↓, mid_sat ↑, N≥6/arm, thresholds
  pre-registered), including the **eng_hot engaged arm** (mid_sat 0.256 — where there is chroma to
  save); the SP-overlay/strobe "bright moments preserved" scenes+metric pre-registered so the gate
  can fail. "WHITE class 0/N" demoted (Fisher p≈0.44 at N=5 — the Wave-5/6 trap).
- **A8:** B.S2 verify = a distinct agent; Lane A's probe edit is a modification (re-run
  drawlog-792 after it); stop-rule checkpointed as machine-readable verdict.

---

_(Original draft below, retained for provenance; superseded where the acceptance above differs.)_

**Draft status:** DRAFT — under Fable pre-dispatch review, not yet dispatched.
Parent: `execution/README.md` (Wave 1–9 + Wave 10 menu). Engine pin `10a9ca6`.

## Where we are (entering Wave 10)

Six default-ON fixes shipped and holding on-screen (Wave-9 rebaseline). Two designed-and-staged
fixes wait to land, each with exactly one sanctioned path:

1. **Hands (W2.8e):** four fix classes are dead; the surviving diagnosis says the skin offset's
   ROTATION basis is baked against the shared magnet skeleton instead of the per-member authored
   bind. The only unrefuted path: **asset-derived, char-space, static rebake** — extract the
   per-member `skeleton_unshared.milo` finger-bone AUTHORED bind rotations, rebake the appendage
   offset against that basis. Plus: fix the dualskin ΔR metric to like-space comparison (the
   Wave-9 world-space metric was confounded by member placement yaw).
2. **WHITE (scene-side):** the staged `RB3_VENUE_WHITE_GUARD` patch (luminance-preserving venue
   highlight compression via the repurposed `SceneUniforms.venueHighlightLumaMode`, world.cam
   upload) — touches `Rnd_Wgpu_RB3.cpp` + DC3-shared files (behavioral-zero-blast: gate value 0 =
   inert), which is why it waited for single-writer sequencing.

Plus the Wave-9 rebaseline's one new anomaly: the **disconnected floating forearm/hand** (seen in a
wipe transition AND a no-wipe gameplay shot) — needs an H1 (shard-family full detachment) vs H2
(occlusion illusion) triage.

## Proposed Wave 10 lanes

**Lane A — W2.8e hands asset rebake (Opus; owns `BandCharacter.*` + the dualskin probe block in
`Rnd_Wgpu_RB3.cpp`; sequential):**
- **S1 (metric + asset ground truth):** fix the dualskin ΔR to like-space (both char-space or both
  world-space with placement yaw removed); extract per-member `skeleton_unshared.milo` AUTHORED
  finger-bone bind rotations vs the shared magnet `char/main/skeleton.milo` and confirm the
  asset-level per-bone ΔR matches the operative error (the Wave-9 S1 "optional provenance
  confirmation," now mandatory — it is the fix's foundation).
- **S2 (fix):** char-space static rebake of the appendage offset against the asset-derived
  authored bind basis, default-OFF registered flag. HARD EXIT: **RealPathFixture GREEN flag-ON**
  (RED 32.8u committed baseline) + the corrected like-space ΔR collapsing toward 0 + W2.2 gate set
  (FLING=0, no 200-460u band, crowd clamp byte-identical, lineup PASS both arms, drawlog 792
  flag-OFF) + finger close-up before/after. If S1's asset ΔR does NOT match the operative error,
  the fix premise is dead too — honest writeup, no code (5th-class refutations must stop here).
- **S3 (floating-forearm triage, cheap):** reproduce the two Wave-9 sightings
  (`REBASELINE/STATUS.md` §b); decide H1 vs H2 via draw-log (is the forearm mesh a detached
  member of a known character's skin set, and where are its bone worlds?) — if H1, it should
  reproduce/collapse with the S2 fix flag; measure both arms.

**Lane B — WHITE-fix landing (Opus; owns `RB3PostProc.*`, WGSL, and the SceneUniforms venue-upload
site in `Rnd_Wgpu_RB3.cpp` — coordinate with Lane A: Lane A's block is the dualskin probe only,
disjoint regions, both additive; flock the build+git as usual):**
- **S1 (land):** apply the staged patch from `execution/WHITE-fix/staged-patch-scene-side.md`
  behind default-OFF `RB3_VENUE_WHITE_GUARD`. Gates (from the Wave-9 design): paired continuous
  wash_score delta on the deterministic reproducer (WHITE class eliminated, hi_frac collapsed,
  luma plausible); authored bright moments preserved (SP overlay + strobe A/B); fail-red; flag-OFF
  byte-identical (drawlog 792); DC3 zero-blast (gate 0 = inert — verify with the standing diff +
  milo-engine-tests); lineup PASS.
- **S2 (independent verify):** all gates on a fresh build + 2-venue spot-check; recommend flip.

**Deferred:** 4→8 lights (DC3 gates), W2.4 BandPatchMesh, song_select residuals.

## Process rules (carried)

Locks, checkpoints (`/tmp/wave10-checkpoints/`), commit-per-review-cycle, append-only flags +
single coordinator regen, no pin bumps/flips by lanes. Defaults ON: the six shipped fixes.
Refuted-experiment flags (`RB3_PP_LUMA_CEILING`, `RB3_HANDS_POSEAWARE`, `RB3_HANDS_PERFRAME_CONJ`,
`RB3_APPENDAGE_REST_ROT`) UNSET in all arms.

## Risks / open questions for the reviewer

- **R-A:** Both lanes touch `Rnd_Wgpu_RB3.cpp` (Lane A: existing dualskin probe block; Lane B: the
  SceneUniforms venue-upload site). Is "disjoint regions in one TU, additive-only" acceptable this
  once, or must one lane serialize behind the other? (Prior waves enforced strict single-writer.)
- **R-B:** W2.8e's premise check (asset ΔR must match the operative error) — is the extraction
  actually feasible from `skeleton_unshared.milo` with existing loaders, or does it need new
  parsing (scope risk)?
- **R-C:** The WHITE patch repurposes a SceneUniforms field — verify the "behavioral-zero-blast"
  claim against DC3 (the field must be zero-initialized in DC3's path, not garbage).
- **R-D:** Should the floating-forearm triage block the W2.8e flip decision (if H1, the fix should
  resolve it; if it doesn't, that's evidence against the fix's completeness)?
