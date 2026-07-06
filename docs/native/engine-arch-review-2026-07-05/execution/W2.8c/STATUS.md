## B.S1 — done (planner, Opus)

PLAN.md written (design-only, no source edited). Core deliverables per WAVE8_KICKOFF Lane B / A4 / A5:

- **Per-frame math (§1):** per-bone conjugation `offset_b(t) = inv(A_b)·inv(L_b(t0))·L_b(t)·A_b·inv(L_b(t))`,
  keeping each hand/finger bone bound to its OWN live per-member bone (NOT wrist-collapsed) so the
  live joint's rotation is applied ABOUT the authored magnet frame A_b — cancels the growing R·sin(θ)
  twist per bone. Reduces to the proven rigid transport for uniform glove shells (no regression, §1.3),
  and is strictly a superset of the RB3_HANDS_POSEAWARE win without its per-bone-authored distortion.
  Verified at t0: skin=I (no latch pop), articulating under animation. One falsifiable assumption
  stated (§1.5, magnet/per-member share bone-local rest conventions up to the t0 bridge) → §4 decides.
- **A4 hazards (§2):** (a) write `drawn->GeomOwner()` offsets + `ownersDone` dedupe (reuse
  NativeRepinHandsRigid:1739 pattern); pristine A_b via mutual-exclusion with RebindHeadHandsAtRest
  (preferred) or first-encounter snapshot. (b) force each live bone's TransParent chain root→leaf
  (WorldXfm_Force/DirtyLocalXfm, Trans.h:77,150 — game-callable) BEFORE sampling L_b, mirroring the
  engine :3421-3446 pass, so Poll-time L_b == palette L_b (kills the stale-cache residual).
- **Flag (§3):** new `RB3_HANDS_PERFRAME_CONJ` default-OFF (getenv-cached no-op); RB3_HANDS_POSEAWARE
  stays landed + UNSET in all arms, mutually exclusive in code (A5); classification row append-only.
- **Measurement (§4):** IK_SHARD_VERT wext A/B, same-binary/same-member (B.S4 protocol) as the HARD
  exit — flag-ON worst appendage <20u vs ~106u OFF, FLING=0, nothing in 200-460u, 0 added drops,
  gloves not regressed. Both arms RB3_HANDS_POSEAWARE + RB3_PP_LUMA_CEILING unset (A5/A7), contract
  default-ON, baseline re-established on pin a94762f. RealPathFixture dual-skin arm = optional/staged
  Lane-A-tail (engine probe). Saddleshoe W2.6 bonus check (measure, don't assume).

FENCE respected: rb3 BandCharacter.{cpp,h} + native/tests/ + append-only engine classification row
only; NO engine code edits (design needs none — §6). Confirmed all accessors game-callable:
Trans.h:77 WorldXfm_Force, :150 DirtyLocalXfm; Mesh.h:226 SetBone, :241 GeomOwner, :257 BoneOffsetAt.

Verified source seams: palette compose Rnd_Wgpu_RB3.cpp:3529, owner-vs-own :3183-3200, WorldXfm_Force
pass :3421-3446; Poll seam BandCharacter.cpp:581 (post Character::Poll :527, post
RebindOutfitBonesToOwnSkeleton :572); NativeRepinHandsRigid owner/ownersDone :1739; existing
RB3_HANDS_POSEAWARE classification row engine json:207.

Handoff: W2.8c.S2 (impl, Opus) → W2.8c.S3 (verify, Opus, B.S4 protocol).

## B.S2 — done (impl, Opus) — code landed default-OFF; flag-ON smoke = MEASURED NEGATIVE (STOP condition hit)

**Implementation landed exactly per PLAN §1–§3** (rb3 `BandCharacter.{cpp,h}`, engine
classification append-only). Builds green (clang, `native/build-agent-W2.8c`).

- New `BandCharacter::NativeConjHandsPerFrame()` — UNLATCHED per-Poll pass at the `:581` seam,
  selected INSTEAD of `NativeRepinHandsRigid()` when `RB3_HANDS_PERFRAME_CONJ` is set (mutually
  exclusive, A5). Phase A (pre-latch) claims hand/finger/glove owner meshes, capturing per bone
  `A_b = inv(pristine authored invBind)` and `L_b(t0)` once; binds each bone to its OWN live
  per-member bone (SetBone, per-bone — NOT wrist-collapsed). Phase B (every Poll) recomputes
  `offset_b(t) = pre·L_b(t)·A_b·inv(L_b(t))` (pre = offA·inv(L0)) and writes the owner palette.
- **Hazard 2a (owner palette):** operate on `drawn->GeomOwner()`, `ownersDone` dedupe, propagate
  `mNativeBonesRebound` to shared draws; pristine `A_b` via mutual exclusion — `RebindHeadHandsAtRest`
  now skips hand/finger/glove meshes when the flag is ON (mitigation i). Confirmed at runtime: CLAIM
  probe shows `owner==mesh` for all claimed meshes (no owner/name mismatch), offsets pristine.
- **Hazard 2b (draw-time world recompute):** `NativeForceBoneChain` forces each live bone's
  TransParent chain root→leaf (`WorldXfm_Force`, deduped) before sampling `L_b(t)` and `L_b(t0)`.
- **Flag `RB3_HANDS_PERFRAME_CONJ`** default-OFF getenv-cached; `HANDS_CONJ_PROBE` print-only. Both
  registered append-only in engine `NativeCompatFlags.classification.json` (no gen.inc regen).
- Re-arm + map clear at SyncObjects and StartLoad (stale mesh/bone pointers).

**flag-OFF byte-identical — CONFIRMED:** drawlog golden `--fixed-clock --canonical-order` PASS
**792** (bin build-agent-W2.8c); conjOFF arm emits **0** `HANDS_CONJ` probe lines; conjOFF
IK_SHARD_VERT worst appendage = **106.0u** (baseline unchanged, matches documented ~106u on pin
`a94762f`). Wii untouched (`#ifdef HX_NATIVE`).

**flag-ON smoke (the S2 direction gate) — MOVED UP, NOT DOWN → STOP per kickoff directive.**
`_w28c_smoke.py` (RB3_HANDS_POSEAWARE + RB3_PP_LUMA_CEILING unset in BOTH arms, A5/A7):

| arm | worst appendage IK_SHARD_VERT wext |
|---|---|
| conjOFF (baseline) | **106.0u** (hands_naked, bone_L-index02) |
| conjON | **2609.0u** (gloves_resource, bone_L-pinky03; hands_naked 2117u) |

Per-bone binding VERIFIED correct (CLAIM: `perBoneSlots==bones`, 38/38, 40/40, 10/10 — articulation
preserved, NOT wrist-collapsed). The internal skin-*translation* APPLY probe reads a healthy
40–54u (hand origin tracks the wrist), but the engine far-vertex probe explodes → the composed
offset's **rotation basis is wrong while its translation is correct**. Time signature: hands_naked
wext is **~80u at t0** (≈ baseline, consistent with the proven `skin(t0)=I`) and **grows to
500–2600u as the pose animates** — i.e. the conjugation **amplifies** the R·sin(θ) twist under
motion instead of cancelling it.

**Verdict:** the PLAN §1.5 falsifiable assumption (magnet frame `A_b` and live world frame `L`
reconcilable by the constant `A_b·inv(L0)` bridge) is **REFUTED by measurement** at the S2 smoke —
the game-side conjugation as specified does not cancel the twist; it worsens it monotonically with
pose deviation from t0. This is the honest-negative outcome PLAN §6/§8 pre-authorized. Per the
kickoff STOP directive ("if it moves up like the rigid-anchor did, STOP and report rather than tune
blindly"), no formula variants were tried — that is S3/replan territory.

**Landed anyway (correct):** the flag is default-OFF and byte-identical when off, so the code is a
safe no-op that S3 uses as the instrument for the rigorous B.S4 A/B and the BL-A0 staged-engine-patch
decision. **Do NOT flip** (coordinator-only; and the smoke says it must not be flipped).

**Lead for S3 (not acted on):** early-frame wext (~80u) is marginally BELOW the 106u baseline, so
the similarity is exact at t0 but has the WRONG SIGN of cancellation. Two hypotheses worth a
controlled S3 test (each a principled variant, not blind tuning): (1) the conjugation should use the
LIVE rest basis `L_b(t0)` as the bridge target rather than the magnet `A_b` (the shipped
`RebindHeadHandsAtRest` deliberately bakes against a CHAR-SPACE, placement-divided rest —
`NativeCharSpaceRestXfm`, `BandCharacter.cpp:901-915` — whereas §1.2 mixes magnet-space `A_b` with
world-space `L`); (2) the inner/outer inversion (`A_b`↔`inv(A_b)`) or bridge composition order in the
§1.2 formula is transposed. If neither game-side variant reaches <20u, the recorded negative escalates
to the staged BL-A0 per-member-skeleton basis engine patch (Wave 9), per §6.

Commits: rb3 `<pending>` (BandCharacter.{cpp,h} + this STATUS), engine `<pending>` (classification).
Build dir: `native/build-agent-W2.8c` (clang). Smoke artifacts: `/tmp/w28c-smoke/`.

## B.S3 — done (verify, Opus) — HARD EXIT: REFUTED · DO NOT FLIP

Independent hard-exit verify on a **fresh from-scratch build** (`native/build-agent-W2.8c-verify`,
clang, 875/875, `BUILD_EXIT=0`, pin `a94762f`), source committed clean at B.S2 `0bf2ba39` (no
uncommitted edits to `BandCharacter.{cpp,h}`). New driver `scripts/native/_w28c_verify_ab.py`
(mirrors `_w28_handsfix_ab.run_arm`; **both arms pop `RB3_HANDS_POSEAWARE` + `RB3_PP_LUMA_CEILING`**
per A5/A7, contract default-ON). Confirms the B.S2 measured negative decisively.

### IK_SHARD_VERT wext A/B (the A5 hard exit, same-binary / same-member)

| metric (worst appendage) | conjOFF (baseline) | conjON | bar | verdict |
|---|---|---|---|---|
| `IK_SHARD_VERT` wext | **105.0u** (`hands_naked`, `bone_R-thumb03`) | **2852.0u** (`drivinggloves_skin`, `bone_R-thumb03`) | < 20u | **FAIL** — 27× WORSE than baseline, 140× over the bar |
| appendage FLING (>120u) | 0 | **1** (`hands_naked`, 60 verts, max 311.9u) | 0 | **FAIL** |
| worst in 200–460u STOP band | none | 311.9u fling **in band** (wext far beyond) | none | **FAIL** |
| added appendage guard-DROPs | 0 | **+4** (`hands_naked`/`fingernails`/`drivinggloves`×2) | 0 | **FAIL** |
| uniform glove not regressed | 60–78u | **2204–2852u** | ≤84u | **FAIL** |

conjON `hands_naked` 2311u / `drivinggloves_resource` 2204u / `fingernails` 1203u — every hand mesh
explodes into the thousands. A7 baseline **re-established on the current pin**: conjOFF 105u ≈
documented ~106u.

### Supporting gates

- **flag-OFF byte-identical (layer 1):** drawlog golden `--fixed-clock --canonical-order` **PASS 792**
  (fresh binary); **0** `HANDS_CONJ` probe lines in the conjOFF arm. Wii untouched (`#ifdef HX_NATIVE`).
- **lineup gate:** flag-OFF **PASS** all layers (img/segA/ratioB/countC/pin, max_band 4.96);
  flag-ON **FAIL ratioB** (`fingernails_resource` ratio 11.95 > cap 8.0, max_band 39.68) — the
  non-blind numeric layer catches the shard even where segA/image stay green (the documented W2.8
  blindness).
- **crowd/scrollbar clamp:** byte-identical flag-OFF (flag-scoped; drawlog 792); untouched flag-ON
  (conj scope = hand/finger/glove only).
- **fail-red (layer 4):** the `IK_SHARD_VERT` metric is live — RED 105u on the baseline (fires on the
  basis mismatch, not on animation). Intact.
- **RealPathFixture gtest:** remains a **SKIP** (dual-skin population needs a Lane-A-owned engine
  probe; **no engine code added** per fence — A5).

### Saddleshoe (W2.6 bonus — measure, don't assume)

**W2.6 does NOT close for free.** `saddleshoe_*` is OUT of the hand/finger/glove scope, so the conj
pass never claims it (0 CLAIM/APPLY lines, absent from the conjON drop list). Expected negative per
the PLAN bonus note — the shoe residual stays W2.6's own (different rest-capture path).

### Root cause + verdict

**PLAN §1.5 falsifiable assumption is REFUTED by measurement.** The game-side conjugation
`offset_b(t) = inv(A_b)·inv(L_b(t0))·L_b(t)·A_b·inv(L_b(t))` mixes magnet-space `A_b` with
world-space `L`: the composed offset's **translation is correct** (hand origin tracks the wrist) but
its **rotation basis is wrong**, so it **amplifies** the growing R·sin(θ) twist under animation
(~80u@t0 → thousands) instead of cancelling it. Three fix classes are now empirically dead with
numbers: `RB3_HANDS_BIND_FIX` (seed), `RB3_HANDS_POSEAWARE` (rigid wrist-collapse), and this
per-frame game-side conjugation. The independent 2852u vs the B.S2 smoke 2609u agree qualitatively;
the ~250u spread + worst-mesh identity swap = documented async-loader completion-order
nondeterminism, immaterial to the verdict.

**Recommendation → DO NOT FLIP (coordinator-only).** Keep `RB3_HANDS_PERFRAME_CONJ` landed
default-OFF as the S3 instrument (byte-identical no-op). The finger shard's remaining fix escalates
to the **staged BL-A0 per-member-skeleton basis ENGINE patch (Wave 9)** per §6/§8 — the correct
rotation basis is unreachable inside Lane B's game-side fence (it needs the per-member CharBones pose
pipeline to carry the authored basis). The B.S2 leads (live-rest bridge; inner/outer inversion) are
principled variants, but the kickoff STOP directive + §6 pre-authorize this honest-negative
escalation over blind formula tuning.

Artifacts: `/tmp/w28c-verify-ab/ab-result.json`, `raw-conj{OFF,ON}.log`;
`/tmp/w28c-lineup-{off,on}/verdict.json`. Checkpoint `/tmp/wave8-checkpoints/B-S3.json`.
Commit: rb3 `<pending>` (`_w28c_verify_ab.py` + this STATUS).
