# HANDS-ADJUDICATION — VERDICT (Wave 15, Lane H)

**Disposition: (a) — proof-level derivation.** The wrong factor is NAMED with
numbers, the reason every one of the seven dead artifacts HAD to fail is a single
invariant, and the minimal change is specified (files + shape). One confirmation
gate is pre-registered for the fix lane; the item does **not** retire — the option
set was NOT exhausted (one cell in the option table was never measured, and the
"6th dead cell"'s death certificate is shown to be confounded).

No fix code was landed (charter). Engine pin `fdf0ad9` untouched; zero engine
edits; zero rb3 source edits; existing probes only. All measurements on
`native/build-native/rb3-native` (2026-07-07 12:31), protocol + binary validated
by an arm-C control that reproduces the record's numbers exactly
(`evidence/arm_summaries.txt`).

---

## 0. A2 discharge — the two load-bearing numbers, reproduced from raw evidence

- **87.3°**: `W2.8f/evidence/readings.txt` line `hands_naked.mesh 38 … min=42.60
  med=87.30 max=87.30 n=2214` — reproduced, WITH its bimodality: the co-sample
  table flips 87.3 (t1cnt=34) ↔ 42.6 (t1cnt=36); the bone-count co-flip means the
  per-bone capture/application state is a variable, exactly as ACCEPTANCE A1
  required. 87.3° is a mode, not a constant — and §2 below derives *what* it is.
- **Mixed-sign per-bone gaps**: `SKEL/evidence/apd_diag_gameplay_grep.log` —
  R-index01 bound 103.0 vs own 109.5/120.1 (+6.5/+17.1); L-index01 142.1 vs
  121.6/119.6 (−20.5/−22.5); R-middlefinger03 129.9 vs 106.0/119.7 (−23.9/−10.2);
  L-middlefinger03 112.6 vs 147.6/127.2 (+35.0/+14.6). `bound` pointer shared
  across members, `own` distinct — reproduced. NEW datum from the same log:
  player3 `gloves` rows show `bakedRest.ang(106.0) != ownNow.ang(119.7)` — a
  stale/cross-basis bake existing in the shipped default, foreshadowing §2.
  **Caveat carried:** these are angle-vs-identity SCALARS. Scalars do not
  subtract — the record repeatedly treated ±6–35° scalar gaps as the operative
  error. The TRUE per-bone relative rotation is 87.2° (§2). Several of the saga's
  premise inversions trace to exactly this scalar-vs-matrix slip.

## 1. The Wii composition and the pairing, anchored per ACCEPTANCE A1

Wii composes `skinPos = v · Σᵢ wᵢ (A_bᵢ · live_bᵢ(t))` (`RndMesh::SkinVertex`,
`src/system/rndobj/Mesh.cpp:1368-1410`, retail-matched) — identical formula to the
native palette (`Rnd_Wgpu_RB3.cpp` `off·WorldXfm`). Animation writes ABSOLUTE
local poses into the bone Trans objects (`CharBonesMeshes::PoseMeshes`,
`src/system/char/CharBonesMeshes.cpp:98`; `Matrix3::RotateAbout*` SETS, does not
compose — `src/system/math/Mtx.h:58-72`; the channel blob strides are
pointer-size-free — no LP64 hazard found in this path). So Wii correctness needs
exactly one contract: **the instance a mesh's offsets are authored against is the
instance the animation drives.**

**Measured (arm W, the pre-registered clean Wii-composition baseline —
`RB3_NO_HEAD_REBIND=1`, reskin OFF, clamp+guard opened so nothing censors or
freezes the raw composition):** natively that contract is unsatisfiable —

- The male authored offsets match `bound`'s basis **exactly, matrix-level, all 38
  bones** (Tier-1 xcheck 0.1°, count(>5°)=0, 401 blocks) — the authored pairing
  `(verts, A_b) ↔ bound` is perfect.
- `bound` is **static**: the same worst vertex (686), the same
  `boneWorld.v=(−21.4, 0.6, 43.5)`, `skinRow0 = identity`, every sampled frame;
  hands_naked wext collapses from arm C's 452 distinct values to 74, quantized.
- 8 of 46 bone slots (the arm/forearm subset) DO resolve to per-member animated
  instances at load — the mixed-anchor states produce the 100–185u
  placement-lever smears (clusters == member |placement|).
- The female's authored offsets are ~29° off the shared male-bind `bound`
  (28.9°, 34/40 bones) — the original 2026-06-05 female-fling component,
  cleanly re-measured.

So the record's decisive syllogism resolves: **the defect is (b)-at-load — the
loader/merge never produces the Wii object** (one per-member instance that both
carries the authored bind basis B and receives animation; the documented
`kInlineCached`-under-preloaded-share divergence + the never-firing
`sBoneMergeDir` ReplaceRefs remap, `BandCharacter.cpp:4159-4181` +
CHAR_SKINNING_DEFORM_INVESTIGATION.md "FIX ATTEMPTS RULED OUT") — **and every
downstream bake was an attempt to synthesize that object out of the two halves
native does have** (`bound` = right basis, no animation; `own` = animation,
different basis).

## 2. The wrong factor, numerically closed (the 87.3° IS the seed-rest error)

From COMMITTED evidence alone (`evidence/offset_basis_derivation.py`):

1. Authored offsets (RESKIN R1 `[RESKIN_OFF]` capture 1) equal `inv(B)` — angles
   129.9/103.0/142.1/112.6 == APD_DIAG `boundNow` to 0.1° on 4/4 bones.
2. The same log's SECOND `[RESKIN_OFF]` capture block ("cap2") dumped
   ALREADY-REBAKED offsets — 106.0/147.7 == APD_DIAG own-male-rest exactly. So the
   default path bakes `off = meshWorld·inv(R)` with R = the rest seeded at
   SetDeformation time (`NativeCaptureRestPoseAfterDeform` →
   `RebindHeadHandsAtRest`, `BandCharacter.cpp:974/1254`).
3. **`angle(B·inv(R)) = 87.2°` for BOTH L/R middlefinger03**, axes
   mirror-symmetric `(±0.130, ±0.316, −0.940)` — an anatomical-pose-shaped
   rotation, not a yaw/placement/convention artifact. **This is the runtime
   Tier-1 87.3° mode, reproduced from committed matrices with zero runtime.**
4. **Arm S (new run, `RB3_HANDS_SHELL_FIX=1`, gender-split for the first time):
   with the authored-bind anchor, the MALE palette reads Tier-1 = 3.1°,
   count(>5°)=0, on ALL 1038 blocks** — i.e. at every freshness capture during
   play, `own`'s hand bones sit ≈ at the authored bind basis B.

(2)+(3)+(4) close the mechanism: **the default rebake anchors the hand offsets to
a TRANSIENT pose — the SetDeformation-time seed R — that sits 87.2° (at the
distal finger bones) from both the authored bind B and the pose the bones
actually hold during play (≈B).** The composed skin `inv(R)·L(t)` therefore
differs from the Wii composition `inv(B)·L(t)` by an L(t)-conjugated per-bone
rotation of exactly that magnitude: joint origins preserved (Tier-2 EXACT ≈ 0 —
a conjugation fixes the bone origin), sub-shells transported rigidly (W2.8g
isoDistort ≈ 0), far verts displaced by `2R·sin(43.6°) ≈ 1.38·R` — at wrist
radius ≈ 48u that is the whole hand rigidly displaced ~50–65u: **the "ceiling
hand" morphology (arm C burst_12: a COHERENT, correctly-formed hand floating
off the wrist, joined to the arm by stretched spike webbing) is this equation
drawn on screen.** The 42.6↔87.3 bimodality = two capture classes of the same
transient (t1cnt co-flip), and the player3 gloves stale-bake row (§0) is the
same defect caught in the act on another member.

Why the seed R is 87° off (deform-clip weighted pose vs a mid-deform transient
vs IK-touched state) is deliberately left open — the fix below removes the
dependence on R for appendages entirely, so it is not load-bearing.

## 3. Why all seven artifacts HAD to fail — one invariant

Every frame-consistent bake is `off = meshWorld·inv(X)` bound to live bone `Y`;
correctness = (verts authored in X's basis) ∧ (Y animates Wii-faithfully). The
option table, now complete:

| cell (X anchor, Y bone) | artifact | why dead |
|---|---|---|
| R seed, own | DEFAULT rebake | X ≠ verts' basis by 87.2° → the shard (this verdict) |
| R world-space, own | `RB3_APPENDAGE_REST_ROT` | same + placement lever |
| B, bound | `RB3_APPENDAGE_ASSET_REBAKE` | Y static → freeze (arm W re-proves bound static) |
| shared-B, own — ALL appendage meshes | `RB3_HANDS_SHELL_FIX` | right anchor for MALE hands_naked ONLY; forced 28.9° error on the female, 60–69° on gloves, ~170° on fingernails → aggregate regression (**confounded death certificate**, §4) |
| conjugation | `RB3_HANDS_PERFRAME_CONJ` (W2.8c) | frame-mixing, amplification |
| rigid anchor | `RB3_HANDS_POSEAWARE` | static collapse of per-bone-authored verts |
| vert re-pose | `RB3_HANDS_RESKIN` (R2) | provably invariant to the `inv(R)·L` factor; amplifies radius |

The invariant: **no bake evaluated after load can conjure the missing pairing —
the verts' authored basis is only correct against an instance that animates, and
the anchor every attempt could capture at runtime was either the transient R
(wrong) or the static bound (frozen).** Vert bakes (RESKIN) provably do not touch
the `inv(R)·L(t)` factor at all. This is the SKEL exhaustion proof with the
numeric factor attached — and with one exception the table exposes:

## 4. The never-measured cell + the confounded death certificate

`RB3_HANDS_SHELL_FIX` baked ONE shared anchor (`inv(charSpaceRest(bound))` = the
shared male-hands bind) onto EVERY appendage mesh. That is the correct authored
anchor **only for the male hands_naked** (0.1°); it is wrong by construction for
the female (28.9°), gloves (60–69°, their own asset bind), and fingernails
(~168–177°). B-S2 measured only the aggregate — its "shard-at-rest / min
34.8→51.0 / starburst" verdict is dominated by the meshes it wrongly re-anchored
(gender split, this wave: female min 51.0 = the aggregate's min; the male arm
reads basis-coherent 3.1° with plausible two-hands-apart extents; the residual
>60u population under it is clap/crowd + gloves + nails + female — see
`evidence/arm_summaries.txt` and the E1 frames).

**The cell that was never measured for hands: keep each mesh's OWN AUTHORED
offsets and repoint to `own` — `SetBone(b, own, /*calcOffset*/false)` — with NO
rebake.** That is per-gender/per-asset correct by construction (each mesh
carries `inv(its-own-authored-bind)`: male 0.1° vs B, female hers, gloves
theirs), and it is EXACTLY the composition Wii runs and exactly the pattern the
torso rebind (`RebindOutfitBonesToOwnSkeleton`) already ships successfully. The
2026-06-11 claim that authored offsets "can't work because they were baked
against the magnet basis" (`BandCharacter.cpp:~1244`) is REFUTED by arm W +
arm S: the authored offsets match the magnet/bound basis exactly (that IS their
basis), and `own` sits at that same basis during play (3.1°) — the claim was
made against the 87°-transient seed, i.e. against defect §2 itself.

## 5. The minimal change (files + shape) — for the NEXT wave's fix lane

Flag-first, default-OFF, HX_NATIVE, `src/system/bandobj/BandCharacter.cpp` only
(no engine TU):

- In `RebindHeadHandsAtRest`, for appendage meshes (the existing `apdMesh`
  scope: hand/finger/nail/glove): in pass A resolve `own = Find(bound->Name())`
  as today, but in pass B **repoint only** (`SetBone(b, own, false)`) and **do
  not overwrite `BoneOffsetAt(b)`** — keep the authored inverse-bind. Keep the
  all-or-nothing `miss==0` gate and `mNativeBonesRebound` flagging (clamp
  exemption) unchanged. Non-appendage meshes keep the current rebake (heads were
  a measured net win — do not disturb).
- Guard the authored-offset provenance: the repoint must run before anything
  rebakes (it already does — first Poll), and a re-stuffed mesh re-enters with
  pristine authored offsets (existing pointer-latch semantics).
- The `mNativeRestPose` seed machinery simply stops mattering for appendages
  (no reader), removing the transient-R dependence — the player3-gloves stale
  bake class dies with it.

**Pre-registered fix gate (carry A9's gates, corrected):**
- Tier-1 (`RB3_HANDS_ATTACH_PROBE`) `count(>5°)==0` on male hands_naked (arm S
  already demonstrates this is achievable) AND ≤ authored-gap on female/gloves/
  nails (their own offsets vs own's rest — predicted small; if female/gloves read
  large here, own's rest is NOT their authored basis and the verdict's residual
  risk fires, see below).
- The ceiling-hand/spike-web morphology GONE in the E1 band frames (the visual
  gate — **wext>60 alone is NOT a valid shard gate for hands_naked**: legitimate
  two-hands-apart extents at raised poses reach 60–104u; this false gate is part
  of why arm-S-class results were misread. Re-anchor the wext gate as
  "flag-ON distribution ≈ arm-C-coherent-pose distribution + screenshots").
- Tier-2 EXACT stays ≤1u; Instrument-B rest-free invariants ≈ 0; guard-DROP
  census 0 band drops; drawlog-792 flag-OFF; W2.1 crowd oracle untouched.
- Gender-split EVERYTHING (nb=38 vs 40) — the single biggest instrument lesson
  of this saga.

**Residual risk (one external check, pre-registered):** the derivation assumes
`own` animates Wii-faithfully around B. Three independent measurements support
it (arm S 3.1° coherence at every freshness capture; the torso shipping the same
composition; body meshes rendering correctly), but it is not externally
verified. If the flag-first fix misses its visual gate on MALE hands, the
remaining hypothesis is native mis-animation of the hand-bone channels, and the
decisive instrument is the **Dolphin + milo-trace bone-world capture**
(WAVE15_REVIEW R-A rank 3): capture `WorldXfm` of `bone_R-middlefinger03` +
`bone_R-hand` on real Wii execution at matched clip time and diff against the
native `own` worlds — a single-bone, single-clip comparison suffices.

## 6. Standing corrections to the record

1. **"6th dead cell" (W2.8g B-S2) is a confounded refutation** — do not cite it
   against authored-anchor compositions. Its flag stays default-OFF (it is still
   wrong as shipped — shared-B for all meshes); the CELL CLASS it was taken to
   kill is alive.
2. **The 87.3° is not a mysterious "magnet conjugation"** — it is the
   B-vs-seed-R relative rotation, computable from asset+bake data
   (`evidence/offset_basis_derivation.py`), and it lives in the rebake, not in
   the animation.
3. **Scalar angle-vs-identity comparisons mislead** (mixed-sign ±6–35° scalars
   vs the true 87.2° relative rotation). Future instruments must compare
   matrices.
4. **wext>60 is not a shard oracle for hands_naked** (§5).
5. `RB3_NO_SKIN_CLAMP` (unset = clamp ON) remains the shipped mitigation until
   the §5 fix lands. NO flag flips, no pin bumps, refuted flags stay UNSET —
   nothing in this lane changed behavior.

## Evidence

- `evidence/arm_summaries.txt` — all three arms, split by gender, with env lines.
- `evidence/offset_basis_derivation.py` — the §2 numeric closure (runs offline).
- `evidence/armC_probe_grep.log`, `armW_probe_grep.log`, `armS_probe_grep.log` —
  probe extracts ([HANDS_ATTACH]/TIER1/TIER2/[IK_SHARD_VERT]).
- `evidence/armC_burst_08.png`, `armC_burst_12.png` (default: ceiling-hand +
  spike webbing), `armS_burst_08.png`, `armS_burst_12.png` (SHELL_FIX: male
  morphology change; residual female/gloves/nails fans as predicted by §4).
- Full engine logs archived at `/tmp/wave15-handsadj/` (regenerable; commands in
  `PLAN.md`).
- Checkpoint: `/tmp/wave15-checkpoints/H.json`.
