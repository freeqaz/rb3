# Wave 9 — Pre-dispatch Review (Fable adversarial pass)

**Reviewer:** Fable subagent. **Input:** `WAVE9_KICKOFF.md` (draft). **Date:** 2026-07-07.
Grounding: `execution/README.md` (Waves 7–8), `W2.8/STATUS.md`, `W2.8c/STATUS.md`,
`WASH-fix/STATUS.md`, plus source spot-checks (appendix).

## VERDICT: **dispatch-with-amendments**

The lane structure, diagnosis-first posture, and carried process rules are sound. But the
draft contains **one gate that cannot go green** (Lane A's `<20u` IK_SHARD_VERT wext exit —
wrong metric, physically unsatisfiable), **one gate that cannot reliably fail red** (Lane B's
"WHITE rate → 0 over N≥8" — underpowered at the observed stochastic rates), a three-way
comparison whose (i)-vs-(ii) arm is a same-inputs near-no-op unless (iii) is made concrete,
and a conditional file collision the kickoff's R-C misses (Lane B's fix lands in
`Rnd_Wgpu_RB3.cpp`, not `RB3PostProc.*`, if the WHITE localizes scene-side). All fixable by
amendment; no redesign needed.

---

## A1 — R-A answered: `RefSkinVertex` is NOT an independent oracle for offset/boneWorld; make (iii) primary and define it concretely

**Verified in source.** `RefSkinVertex` (engine `tests/test_skin_golden.cpp:164`) reads
`m->BoneOffsetAt(boneIdx)` and `bt->WorldXfm()` and composes `Multiply(invBind, bt->WorldXfm())`
with `ScaleAddEq` weighting — **byte-for-byte the same inputs and the same composition** as the
GPU palette (`Rnd_Wgpu_RB3.cpp:3617`: `Multiply(owner->BoneOffsetAt(b), wt, skin)` where
`wt = bt->WorldXfm()`). The rb3-tests duplicate (`native/tests/test_farvert_rotation_oracle.cpp`)
is parameterized over an explicit palette, but on the real path it would be fed the same captured
values. So for the two live suspects — offset and boneWorld — **(i) vs (ii) is a no-op comparison**:
a wrong offset or wrong boneWorld flows into both identically. What (i)-vs-(ii) CAN legitimately
reveal is limited to palette-side divergences: the V24 composed-matrix reject and SKIN_CLAMP
identity fallbacks (`Rnd_Wgpu_RB3.cpp:3652-3671`, palette-only), the placement-contract
`inverse(meshWorld)` strip (band character meshes have identity meshWorld → skipped), and
**WorldXfm cache freshness** — the palette forces each bone's TransParent chain root→leaf before
sampling (`:3505-3530`, the documented stale-leaf fix), while a Poll-time CPU read can get a stale
cache (W2.8c PLAN §2b already knew this: "Poll-time L_b ≠ palette L_b" without forcing).
**Amendment:** (a) S1 must sample the CPU reference at the exact palette-compose point (or dump the
palette's own composed values) so a stale-cache artifact is not misread as "the wrong factor";
(b) demote (i)-vs-(ii) to a fallback-detection check; (c) make **(iii) the primary comparison and
define it as two executable, Dolphin-free references** (see A2) — the kickoff's "(iii) if reachable"
soft language guarantees an S1 that trivially reports (i)==(ii) and guesses at (iii). Note also the
Dolphin material in `docs/native/c8-ground-truth-2026-07-01/` is **screenshots, not bone dumps** —
usable as visual corroboration only, never as the numeric (iii).

## A2 — Lane A's hypothesis set is half-answered already; scope S1 to the two live candidates and the two executable forms of (iii)

The kickoff's factor list ("offset? bone world? composition order? owner-vs-drawn?") re-measures
two factors that are already closed:

- **Composition order — CLOSED by source.** The Wii-faithful composition (decomp
  `src/system/rndobj/Mesh.cpp:1368-1410` `RndMesh::SkinVertex`, "matches in retail":
  `Multiply(BoneOffsetAt(boneIdx), curBoneTrans->WorldXfm())`, `ScaleAddEq` weighted, no weight
  normalization, row-vector `Multiply(vert.pos, tf60, ret)`) is **identical** to the engine palette
  and to RefSkinVertex. `RndMesh::SetBone(calcOffset=true)` (`Mesh.cpp:328-334`) confirms the
  offset convention: `offset = meshWorld · inverse(boneWorld)`. Cite these lines in the kickoff;
  do not spend lane budget measuring composition order.
- **Owner-vs-drawn — CLOSED by measurement.** W2.8c B.S2 CLAIM probe: `owner==mesh` for all
  claimed hand/finger/glove meshes; W2.3 refuted GeomOwner aliasing for crowd. A one-line
  re-confirmation in the dump is fine; it is not a live hypothesis.
- **Translation of every factor — CLOSED thrice** (REBIND_DRAW_SKINPOS 47–58u clean, FLING=0,
  W2.8c APPLY probe 40–54u).

What the Wave-8 data does NOT close — and the kickoff should state this precisely so S1
discriminates rather than rediscovers: `skin(t0) ≈ I` (W2.2 bind oracle GREEN; conj smoke t0
consistency) plus **error growing with pose deviation from t0** eliminates any *constant*
composed-matrix error and leaves exactly **two candidates**: (a) the live per-member bone's
animated **rotation evolution** is unfaithful (pose pipeline: CharClip eval → CharBones →
TransParent composition applies the clip rotation in a basis inconsistent with the bone's own
rest), or (b) the live bone rotates faithfully but the **offset's rotation basis is conjugated
against the wrong frame** (baked vs the magnet basis), so error grows with bone rotation from the
capture pose. Both produce the observed signature; only (iii) separates them.
**The two executable forms of (iii):** (iii-a) **clip-channel comparison** — evaluate the CharClip
channel for the worst-offender bone directly (the decomp clip evaluator is compiled into
rb3-native; the clip is asset data) and compare against the live bone's LocalXfm at the same frac:
match ⇒ pose pipeline faithful ⇒ candidate (b); mismatch ⇒ candidate (a). (iii-b) **rest-basis
delta** — load both skeletons' rest bases (magnet `char/main/skeleton.milo` vs the member's
`skeleton_unshared`; pure asset data) and compute per-bone ΔR; candidate (b) predicts the measured
far-vert fling quantitatively as R·sin(θ(t)) with θ derived from ΔR conjugation — a checkable
number, using the already-recorded worst offenders (`bone_R-thumb03` R=78.5, `bone_L-index02`
R=64.3, W2.8 B.S2 table).

## A3 — Lane A S2's hard exit "<20u vs ~106u" in IK_SHARD_VERT units **cannot go green**; restate it in BL-A2 oracle units via the dual-skin probe

**Verified in source.** `wext` is the skinned mesh's **world-AABB diagonal**
(`Rnd_Wgpu_RB3.cpp:4322`: `wext = sqrt(Σ(wmx-wmn)²)` over the 4-bone-blended verts), not an
error norm. `hands_naked.mesh` has bind AABB diagonal ≈ **80.3u** (W2.8 STATUS: "bindExt ≈ 80.3 →
worldExt 97–109"), so a **perfectly skinned** hand mesh under animation reads wext ≈ 80–110u —
it can never read <20u. Worse, the probe only prints when `wext > 60.f` (`:4325`), so sub-60
values are unobservable through it. The `<20u` figure belongs to a **different metric**: the BL-A2
oracle's `kShardThreshold = 20.0f` (`native/tests/test_farvert_rotation_oracle.cpp:92`) on
`|RefSkinVertex(asDrawn) − RefSkinVertex(coherent)|` per far vertex. W2.8c carried the same
conflation ("IK_SHARD_VERT wext A/B … flag-ON <20u") but never reached its exit (refuted at
smoke), so the impossibility was never exposed. **Amendment:** S2's hard exit = the
**RealPathFixture arm of the BL-A2 oracle turning GREEN** (`EXPECT_LT(worst, kShardThreshold)`,
`test_farvert_rotation_oracle.cpp:494`) on a real captured pose. That requires the **dual-skin
engine probe** (asDrawn palette vs coherent reference at the same vert, dumped to
`native/tests/goldens/w2.8-farvert/live_pose.txt` — the directory does not exist yet; RealPathFixture
is still a SKIP) which B.S2/B.S3 explicitly deferred as a Lane-A-owned engine edit. This wave the
render backend is unowned and the kickoff allows engine probes — so **make the dual-skin probe an
S1 deliverable** (it is also exactly the instrument S1's factor table needs). Keep wext only as a
secondary signal (shard-guard drops, ratio), never as the pass/fail bar.

## A4 — R-B answered: yes, force-reproduce WHITE first; the "rate → 0 over N≥8" gate cannot reliably fail red

Observed WHITE rates in the Wave-8 record are low and stochastic: `vlo_both` **1/6** (A.S3 gate b),
h2fix arm "some boots" (S2 amendment), default engaged **0/16** (A.S3 gate a — on the now-shipped
chroma-preserve default, WHITE did not appear at all in the only N≥8 default-arm sample we have).
At a true rate p=1/6, P(0 washes in N=8 | no fix) = (5/6)⁸ ≈ **0.23**; at p=1/12 it is ≈ 0.50 —
a fix-free build passes the proposed gate a quarter to half the time. **Amendment:** restructure
Lane B so S1's exit is (a) a **deterministic (or ≥50%-rate) WHITE reproducer** — find the
hot-input condition (director-pinned hot shot; the `vlo_both` WHITE boot under `RB3_FIXED_CLOCK`;
or a forced-hot probe), and (b) the **PP_OFF discriminator verdict** (scene-render vs composite —
the exact cheap axis WASH-fix S1 prescribed and never ran to a clean N). S2's gate is then a
**paired continuous metric on the reproducer** (wash_score luma / hi_frac delta, fix-ON vs
fix-OFF, same shot), with the N≥8 multi-boot sweep demoted to a no-new-wash regression check.
Two evidence notes for the lane brief: the A.S3 residual WHITE had **luma 0.709 — below the
ppKnee 0.82** (`rb3_postproc.wgsl.inc:217`), independently confirming the kickoff's "highlight-only
shape refuted" premise; and the chroma-preserve path reconstructs from **ungraded scene chroma**
(`cpColor = sceneColor·(gradedLuma/inLuma)`, `:234`) — a scene already blown to white has chroma≈0
and passes through unchanged, which weakly predicts the answer is **scene-side** (venue lighting
hot into the UNORM intermediate clamp at `RB3PostProc.cpp:155`). S1 should treat that as the prior
to test, not assume it.

## A5 — R-C: the real collision is conditional and the kickoff misses it — Lane B's fix may belong in `Rnd_Wgpu_RB3.cpp`, not `RB3PostProc.*`

The kickoff scopes Lane B as "`RB3PostProc.*` + `rb3_postproc.wgsl.inc` — same files as Wave-8
Lane A, now free." But Wave-8's WASH-fix also edited **`Rnd_Wgpu_RB3.{cpp,h}`** (FIX-H1 lives at
`Rnd_Wgpu_RB3.cpp:1601`; the WASHPROBE too), and if Lane B's S1 localization comes back
**scene-side** — which A4's chroma-pass-through argument makes live — the fix is venue-lighting
exposure in `Rnd_Wgpu_RB3.cpp`'s world.cam scene-uniforms region, i.e. **the same TU Lane A is
probing**. **Amendment:** pre-assign `Rnd_Wgpu_RB3.cpp` single-writer to **Lane A** for the wave;
Lane A's probes stay additive-only under a flag (as drafted). If Lane B's verdict is scene-side,
Lane B **stages** its patch (the Wave-6 W3.3 precedent: staged, coordinator-sequenced) instead of
landing it. If the verdict is composite-side, the drafted file split holds and both lanes proceed
in parallel. Both lanes append rows to `NativeCompatFlags.classification.json` — covered by the
carried append-only + single-coordinator-regen rule; keep it.

## A6 — R-D: four confounds in the Wave-6-vs-Wave-9 montage beyond the intended flips

(a) **Director-shot RNG**: equal songMs ≠ equal camera cut even under `RB3_FIXED_CLOCK` — proven
three times (WASH-fix S1 H2 "director-shot-dependent"; A.S2 sweep confound; A.S3 gate c
"mean_val 0.16–0.58 at equal songMs"). Gameplay frame-vs-frame montage cells are shot-confounded;
require **N≥3 boots per gameplay state** and state-level (not pixel-level) judgment.
(b) **The Wave-6 baseline's own anomalies are not clean references**: `gameplay_default_1.png` is
itself flagged ANOMALOUS in the Wave-6 MANIFEST (a pre-chroma-preserve wash boot), and the Wave-6
"default" arm is **pre-placement-contract** — for the placement axis compare against Wave-6's
`gameplay_placement_on_*.png` captures, which exist for exactly this reason.
(c) **part_difficulty must use the W4.1 settle-frame protocol** (frames 366–734): the Wave-6
`partdiff_default.png` frame-390 "missing widgets" anomaly was adjudicated **NOT A BUG**
(mid-transition camera zoom, Wave 6 W4.1). A naive re-run of the Wave-6 protocol re-files a
refuted finding.
(d) **Archive the baseline first**: `/tmp/wave6-current-state/` still exists today (verified) but
is volatile tmp; copy it into the job dir/repo before dispatch — it is the comparison's ground
truth. Also hand Lane C the known-open list (finger shard, engaged-WHITE residual, grey skin-RTT,
song_select scrollbar sliver) so anomaly notes don't re-file knowns; and since Lane C explicitly
compares five flipped defaults, its captures must be on a build at pin `a320f9d` with **no lane
flags set** — a plain-default build, not a lane build dir with leftover env.

## A7 — Baseline re-establishment and env hygiene for both measuring lanes

All prior hands numbers (~105–107u wext, oracle RED 79–107u) were measured on pin `a94762f`
(W2.8/W2.8c); the wave now sits on `a320f9d` (chroma-preserve flipped + Wave-8 lanes landed).
Per the A7 pattern that W2.8c itself followed, **Lane A S1 must re-establish the baseline on the
current pin** (wext ~106u, oracle RED) before any A/B, and both lanes must run with
`RB3_HANDS_POSEAWARE`, `RB3_HANDS_PERFRAME_CONJ`, `RB3_PP_LUMA_CEILING` **unset** (kickoff has
this — keep it) and chroma-preserve at its new default-ON in every arm (kickoff has this too).
Minor citation fix: the palette compose `Multiply(owner->BoneOffsetAt(b), wt, skin)` is at
`Rnd_Wgpu_RB3.cpp:3617` on current HEAD; `:3529` is the surrounding placement-contract region —
"region" is fine, but S1 should anchor on the Multiply line.

## Sequencing / model tiers / other checks

- **Sequencing:** Lane A S1→S2 gating is correct, and the "S2 only if S1 names the factor
  unambiguously" honest-diagnosis fallback is exactly right — keep it. Lane B S1→S2 becomes
  correct with A4's reproducer-first exit. Lane C is independent, capture-only, no gate needed —
  fine by design.
- **Model tiers:** appropriate (Opus on both hard lanes, Sonnet capture-only). No change.
- **Deferred list:** consistent with the Wave-8 menu; `wave6-boxmap-proto` shelving carried
  correctly.
- **No re-measurement traps found in Lane B** (the WHITE item is genuinely new work; the
  engagement machine is exonerated and the kickoff does not re-open it).
- **Process rules:** carried correctly (locks, checkpoints, append-only flags, no pin bumps).

---

## Appendix — what I checked in source

1. **`RefSkinVertex`** — `/home/free/code/milohax/milo-native-engine/tests/test_skin_golden.cpp`
   :164 region: reads `m->BoneOffsetAt(boneIdx)` + `bt->WorldXfm()`, `ScaleAddEq` weighted, no
   normalization, row-vector `Multiply(v.pos, tf60, ret)`. Same-inputs conclusion in A1.
2. **rb3-tests duplicate + oracle** —
   `/home/free/code/milohax/rb3/native/tests/test_farvert_rotation_oracle.cpp`: `kShardThreshold
   = 20.0f` (:92), `RealPathFixture` (:456) reads `goldens/w2.8-farvert/live_pose.txt`;
   **`native/tests/goldens/w2.8-farvert/` does not exist** → still SKIP, dual-skin probe never
   built. Hard-exit restatement in A3.
3. **GPU palette** — `/home/free/code/milohax/milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp`:
   WorldXfm_Force chain pass (:3505-3530), placement-contract mesh-relative strip (:3550-3575),
   compose `Multiply(owner->BoneOffsetAt(b), wt, skin)` (:3617), V24 composed-matrix reject +
   SKIN_CLAMP fallbacks (:3640-3700), `wext` = world-AABB diagonal (:4276-4322), `IK_SHARD_VERT`
   gate `wext > 60.f` (:4325) and its printout (rotation rows, vertR, devFromCentroid).
4. **Wii-faithful composition** — `/home/free/code/milohax/rb3/src/system/rndobj/Mesh.cpp`:
   `SkinVertex` (:1368-1410, "matches in retail") — identical formula; `SetBone` offset
   convention `offset = meshWorld·inverse(boneWorld)` (:328-334). `CharBonesMeshes.cpp` is the
   pose-side (channel offsets into the bone pose blob), not the skin compose — the faithful
   *animated basis* is not derivable from the compose formula alone, hence A2's (iii-a)/(iii-b).
5. **Postproc/exposure** — `/home/free/code/milohax/milo-native-engine/src/platform/RB3PostProc.cpp`:
   UNORM intermediate (:155 region, comment :168), `venueGrade` plumb (:216, :323);
   `src/gfx/Shaders/rb3_postproc.wgsl.inc`: `ppKnee = 0.82` (:217), chroma-preserve
   reconstruction `cpColor = sceneColor·(gradedLuma/max(inLuma,1e-4))` (:230-241) — the
   scene-side-prior argument in A4. FIX-H1 at `Rnd_Wgpu_RB3.cpp:1601` (from WASH-fix STATUS) —
   the A5 collision.
6. **Wave-6 baseline** — `/tmp/wave6-current-state/` exists (verified 2026-07-07); MANIFEST
   confirms fixed-clock protocol, `partdiff_default.png` frame-390 anomaly (later refuted by
   W4.1), `gameplay_default_1.png` anomalous, and separate `gameplay_placement_on_*.png` arms.
7. **Records** — `W2.8/STATUS.md` (BL-A2 design, B.S2 step-0 inert-flag + pose-varying verdict,
   B.S3/B.S4 rigid-anchor refutation, worst-offender table with R values), `W2.8c/STATUS.md`
   (conjugation refutation, "rotation basis wrong / translation correct", 80u@t0 → 500–2600u
   growth, owner==mesh CLAIM), `WASH-fix/STATUS.md` (H1 refuted, WHITE rates: vlo_both 1/6,
   default-arm 0/16, h2fix "some boots"; luma 0.709 residual; PP_OFF discriminator prescription).
