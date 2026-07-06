# W2.8 — Missing hands + partially-transparent torsos (CHARACTERIZATION)

**Charter:** characterization-only. No source edited, no flags flipped, no git
reset/rebase/checkout. Writes confined to `execution/W2.8/` + `/tmp/w28`. The
native binary (`native/build-native/rb3-native`, built 2026-07-06 18:12) was run
**read-only** headless; no build/`native/build-native` mutation.

**User report (2026-07-06):** "missing hands, some torsos are partially transparent."

**TL;DR verdicts**
- **(A) Missing hands = CONFIRMED, mechanism = rotation-basis skinning shard.** The
  hand/finger meshes ARE loaded, bound, and drawn every frame (not dropped, not
  translation-flung), but the fingers/hands **explode into thin flat radiating
  sheets/slivers** — the documented C8 rotation-basis divergence between the
  per-member animated skeleton and the static magnet the outfit inverse-bind was
  baked against. This is a **known-open pre-existing residual, NOT a Wave 3–5
  regression.**
- **(B) Transparent torsos = torso BASE meshes are OPAQUE; no torso-material bug
  found in evidence.** Every torso outfit *base* mesh draws `blend=1 (opaque)` and
  skins cleanly. The only skinned transparent (`blend=3`) draws are faithfully
  alpha-authored **detail/decal/string/accessory** layers (fingernails, logos,
  guitar/bass strings, a jacket-emblem layer, a watch strap). The reported
  "partial transparency" is best explained as **the same symptom-A shard**: thin,
  semi-transparent exploded hand/finger/forearm sheets (incl. the `blend=3`
  fingernails) draping over the upper body. A distinct SrcAlpha-base + missing-
  texture-alpha torso bug was **not observed** on the ~5 members captured (kept as
  a secondary hypothesis pending a named member/outfit).

Evidence: `evidence/MONTAGE_hands_torso.png` (ground-truth vs 6 native frames),
`evidence/drawlog_blend_scan.txt`, `evidence/*.png`.

---

## How this was measured

Headless `rb3-native` (`RB3_HTTP=1`), state-gated HTTP nav to gameplay
(start→confirm→PLAY NOW→QUICKPLAY→song_select→select→part:guitar/diff:expert→
game_screen, nofail+autohit), **real-time clock** (song plays, band animates).
Screenshots + `/api/drawlog` sampled at songMs {3,12,25,40,60,80,100}k, plus 3
`rb3_set cam_yaw` close-ups. Probes: `SHARD_DBG`, `SHARD_RATIO_DBG`,
`REBIND_DRAW_SKINPOS`, `RB3_DRAWLOG`. Harness: `/tmp/w28/capture2.py`; engine
stderr `/tmp/w28/engine2.log`. Ground truth: `images/retail-screenshots/
fandom_gameplay_guitar.png` (retail: gloved hands fully visible + opaque jacket)
and `docs/native/c8-ground-truth-2026-07-01/dolphin-shots/`.

Drawlog note: `/api/drawlog` records the mesh name as an FNV-1a hash. All hashes
were reverse-mapped from the run's own `mesh='…'` log corpus + the extracted-asset
`*.mesh` name list (`evidence/run_meshnames.txt`); the 2 initially-unmapped
transparent hashes resolved to `sm57_logo.mesh` and
`malewrist_watchleatherstrap.1.mesh`.

---

## (A) MISSING HANDS — mechanism verdict: rotation-basis shard (not drop, not collapse)

The kickoff's prime suspect was a **guard-DROP** (W0.1 skin-golden guard, as in the
W2.6 saddleshoe residual). **Ruled out for band members** by three independent
measurements:

1. **Draw-log presence.** `hands_naked.mesh` / `gloves_resource.mesh` /
   `fingernails_resource.mesh` appear in every gameplay `/api/drawlog` (skinned,
   `blend=1` opaque for hands, drawn 4–8×/frame). They are submitted, not skipped.
2. **Not guard-dropped.** `SHARD_GUARD dropped …` fires **only** for crowd extras
   (`male_extras_eyebrows11/hair02`, `dir='male_extras02/11'`) and the menu
   `scrollbar_bg.mesh` — **zero band-member drops** in either run. Band garments
   ride the relaxed band cap (4.0× ratio / 110u world); `hands_naked.mesh`
   whole-mesh ratio is only **~1.2–1.36×** (bindExt ≈ 80.3 → worldExt 97–109) —
   nowhere near the cap.
3. **Not translation-flung / not collapsed.** `REBIND_DRAW_SKINPOS` for
   hands/gloves/fingernails/jacket bones is **clean, 47–58u** (< the 65u tripwire;
   fling would be hundreds). Translation is fine.

**What actually happens (visible in every capture):** the fingers/hands **smear
into thin flat radiating sheets** — drummer stick-hands become thin tan sheets
(`evidence/A_gameplay_stickhands_exploded_25k.png`), the singer's raised hands
become pink/white sliver fans (`A_singer_hand_shardfan_80k.png`,
`A_singer_raised_hand_slivers.png`), the guitarist's hand becomes flat white
slivers (`A_guitarist_hand_exploded_40k.png`). At a glance / at speed this reads as
**"missing hands / claw hands."**

**Root cause (documented, confirmed).** `CHAR_SKINNING_DEFORM_INVESTIGATION.md`
lines 104–156, 1180: the per-member animated bone's **rotation BASIS differs from
the static magnet** the outfit's authored inverse-bind offsets were baked against,
so **long-thin extremity geometry (fingers, hair, clothing edges) shards into thin
radiating spikes while the compact torso survives.** The engine's own
`IK_SHARD_VERT` comment (`Rnd_Wgpu_RB3.cpp:~4062`) states the geometry precisely: a
vertex at radius R from its bone with rotation-basis error θ flings by **R·sin(θ)**,
which is **invisible to any origin-only metric.** That is exactly why this residual
is real yet the whole-mesh AABB ratio (~1.3×) and the bone-origin `SKINPOS` (47–58u)
both read clean — **the smear is sub-mesh-AABB, per-vertex, rotation-only.**

**Gate-blindness finding (important).** W2.2 (Wave 3) reported hands "fixed" and its
`RB3_HANDS_BIND_FIX` branch "measured no benefit" — but the W2.2 bind-pose identity
oracle **and** `REBIND_DRAW_SKINPOS` both measure **bone-ORIGIN translation**. They
provably cannot see the R·sin(θ) far-vertex rotation smear, so they pass while the
fingers explode. Any future "hands fixed" claim needs a **far-vertex rotation-basis
oracle** (an `IK_SHARD_VERT`-style metric), not an origin metric.

## (B) TRANSPARENT TORSOS — mechanism verdict: no torso-material bug found; shard by-product

Full `blend`-state scan of all 7 gameplay drawlogs (`evidence/drawlog_blend_scan.txt`):

- **Torso BASE outfit meshes are ALL opaque** (`blend=1`): `greaserjacket_resource`,
  `greaserjacket_skin.2`, `tightdistressedpants_resource`, `escapeartist_*`,
  `clearcoat_shirt`, crowd bodies, `hands_naked`, `head`. No opaque torso is drawn
  alpha-blended.
- **Every skinned `blend=3` (transparent) draw is a faithfully alpha-authored
  detail/decal/accessory layer**, NOT a torso body: `fingernails_resource` (×39,
  the exploding finger detail), `logo.mesh`/`sm57_logo.mesh` (decals), `*_strings`
  (guitar/bass strings — legitimately transparent), `greaserjacket_resource.1.mesh`
  (a jacket emblem/trim sub-layer, ×2/frame over an opaque base), and
  `malewrist_watchleatherstrap.1.mesh`. These come straight from `mat->GetBlend()`
  == `kBlendSrcAlpha` and are **faithful to the assets** — they are not the bug.

Across 6 gameplay frames + 3 cam angles the torsos render **opaque** vs the venue
(`evidence/B_drummer_flatsheet_hand_opaque_torso.png` — flat-sheet hand, solid
white vest). The one jacket sub-layer that is SrcAlpha (`greaserjacket_resource.1`)
draws over its opaque base and does not read see-through. Conclusion: the
user-perceived "partial transparency" is most consistent with the **symptom-A thin
exploded hand/forearm/fingernail sheets** (which are genuinely thin + partly
alpha) splaying across the character silhouette — i.e. **one root cause, two
descriptions** — rather than a separate torso-alpha defect.

**Secondary hypothesis (not observed, kept open):** if the user saw a specific
member whose *torso base* is see-through, that would be a SrcAlpha-base material +
missing/low texture-alpha upload (the useAlphaAsRGB / white-icon-blob family, cf.
engine `8397fa6`/`f52f2f1`). None of the ~5 captured members exhibit it. Resolving
this needs the user to name the member/outfit → a targeted re-capture.

## Flag matrix (as charter-requested)

- `RB3_NO_HEAD_REBIND` (opt-out of the default-ON head/hands rest rebind): the
  rebind fixes hand **translation** (fling→hundreds of u when off). It does **not**
  touch the rotation-basis shard characterized here. Not run this pass (read-only
  characterization; the default-ON path is what the user sees and is what we
  measured).
- `RB3_SKEL_REBIND_FULL=1`: **known-broken control** (W0.1 fail-red), full-body
  rebind — not W2.2's flag; not exercised.
- `RB3_NO_SKEL_REBIND` / skin-clamp / `SHARD_GUARD_OFF`: guard/clamp opt-outs; not
  toggled — the DROP path is already proven not to touch band hands (zero band
  drops with the guard ON).
- `RB3_PLACEMENT_CONTRACT`: irrelevant — the Wave-6 current-state capture already
  showed both symptoms reproduce identically flag-ON/OFF.

## Regression status — NOT a Wave 3–5 regression

- Documented **known-open** C8 rotation-basis residual since wave-07/08
  (2026-06-06, `CHAR_SKINNING_DEFORM_INVESTIGATION.md`). The shard-guard comment
  explicitly defers it: "keep dropping them by default until the pose-pipeline
  basis root-cause (C8) is fixed."
- Wave-3 **W2.2** landed the *translation* fix (`RebindHeadHandsAtRest`,
  default-ON) and **explicitly left** the rotation-basis residual; its
  `RB3_HANDS_BIND_FIX` experiment "measured no benefit → correctly NOT flipped"
  (blind to rotation, per the gate finding above).
- `git log` of the skinning-adjacent files across Waves 3–5 (engine `Rnd_Wgpu_RB3.cpp`,
  `RB3MaterialBinder.cpp`; rb3 `BandCharacter.cpp`, `OutfitConfig.cpp`) is **MOVE
  refactors + default-OFF flags** (W1.2–W1.7 extractions, W1.6 DrawContext,
  W2.1 placement contract default-OFF). None change default skinning/blend behavior.

---

## Backlog proposal

### BL-A1 — Rotation-aware outfit inverse-bind rebake (the real symptom-A fix)
- **What:** rebake the outfit/appendage inverse-bind against the per-member bone's
  **full transform incl. rotation basis**, not translation only, so fingertip verts
  at radius R stop smearing by R·sin(θ). This is the substantive C8 pose-pipeline
  fix the campaign has deferred.
- **Owning file:** rb3 `src/system/bandobj/BandCharacter.cpp`
  (`RebindHeadHandsAtRest` ~:522/:936, `RebindOutfitBonesToOwnSkeleton` ~:1062, and
  the invBind offset math). **File-disjoint from all Wave-6 lanes** (Lane A=engine
  render, Lane B=BoxMap, Lane C=UI game-code, Lane D=BandFace/venue) → collision-free
  to land — but **must be gated by BL-A2 first** (charter: no source edits here).
- **Risk/prereq:** W2.2's `RB3_HANDS_BIND_FIX` already tried a naive rest-capture and
  saw "no benefit" *under a blind gate*. Do not retry without BL-A2.

### BL-A2 — Far-vertex rotation-basis oracle (unblocks any A-fix)
- **What:** an `IK_SHARD_VERT`-style far-vertex metric (max R·sin(θ) over hand/finger
  verts) wired as a `rb3-tests` oracle, replacing/augmenting the origin-only
  bind-pose oracle that passed while fingers exploded.
- **Owning files:** rb3 `native/tests/` (oracle) + read of engine `IK_SHARD_VERT`
  logic in `Rnd_Wgpu_RB3.cpp`. **COLLISION NOTE:** adding an engine-side oracle hook
  in `Rnd_Wgpu_RB3.cpp` collides with **Lane A** (single-writer this wave) → stage as
  a Wave-7 patch, or implement the oracle CPU-side in `native/tests/` reusing the
  RefSkinVertex path (W0.1) to avoid the engine file.

### BL-B1 — (conditional) Named-member torso transparency re-capture
- **What:** only if the user can name the specific see-through member/outfit. Re-capture
  that member and check for a SrcAlpha-base torso material + low/missing texture-alpha
  upload. If confirmed, fix is engine texture-alpha (RB3MaterialBinder / tex upload).
- **Owning file (if it materializes):** engine `src/platform/RB3MaterialBinder.cpp` or
  tex-upload path → **COLLISION with Lane A** (engine render TU) → Wave-7 patch.
- **Priority:** LOW — no evidence of this bug in the current captures; torsos are opaque.

### Non-actionable (documented, do not "fix")
- The `blend=3` fingernail/logo/string/jacket-emblem/watch layers are **faithful**
  alpha authoring; leave them. The crowd-extras + `scrollbar_bg` guard-drops are a
  separate (crowd/UI) population, out of W2.8 scope.

---

## B.S1 (W2.8.BL-A2) — FAR-VERTEX ROTATION-BASIS ORACLE — LANDED

**Deliverable:** `native/tests/test_farvert_rotation_oracle.cpp` (rb3-tests),
registered in `native/CMakeLists.txt`. The falsifiable far-vertex metric that the
W2.2 origin oracle + REBIND_DRAW_SKINPOS + whole-mesh AABB ratio were all blind
to. FENCE honored: no engine files edited, BandCharacter.cpp read-only; the
RefSkinVertex skinner is DUPLICATED (not exported) per WAVE7_REVIEW A6.

**Metric.** `max` over hand/finger far verts of
`|RefSkinVertex(v, asDrawnInvBind) - RefSkinVertex(v, coherentInvBind)|` under an
ANIMATED bone pose. `RefSkinVertex` is the RB3 `RndMesh::SkinVertex`
(Mesh.cpp:1367-1410) semantics — SKIP-not-clamp, no weight normalization,
`skinMat = invBind*boneWorld` — duplicated from engine
`tests/test_skin_golden.cpp:164` and parameterized over an explicit bone palette
(runs without an RndMesh). The basis break is modeled FAITHFULLY as a bone-local
rotation `invBind * RotZ(theta) * boneWorld`, so a vert at bone-local radius R
flings by exactly `2*R*sin(theta/2)` about the BONE origin (a vert AT the bone
origin does not move — the exact reason the origin metric is blind). NB: the
engine test's left-multiply `breakBasis` rotates about the MESH origin and does
NOT scale with bone radius — corrected here (documented in the TU header).

**EXIT — RED on today's build (fail-red demo):**
`RB3_FARVERT_PERTURB=1.645 rb3-tests --gtest_filter=FarVertRotationOracle.CoherentUnderAnimation_FailsRedOnPerturb`
=> RED, worst far fingertip **fling = 58.6u** at the documented ~94deg
(1.645 rad) magnet-vs-per-member divergence
(CHAR_SKINNING_DEFORM_INVESTIGATION.md:104-156; the two documented worldRot
vectors dot to -0.077 => theta ~= 94.4deg). Recorded magnitudes:
`shard_fling_90deg_u=57`, `shard_fling_documented_u=59`. This is the shard the
game shows now; the coherent (theta=0) skin under the SAME animated pose is
GREEN (metric < 1e-2u) — so the oracle fires on the basis mismatch, never on
animation alone (the A6 bind-pose-green trap is avoided by measuring post-anim).

**EXIT — synthetic-perturbation control (R*sin(theta) scaling):**
`MetricScalesWithRSinTheta` sweeps the break angle on the R=40u fingertip and
asserts `metric == 2*R*sin(theta/2)` (~R*sin(theta) small-angle), monotone on
(0,pi], zero at theta=0. MEASURED vs predicted (u):
| theta | 0.1 | 0.3 | 0.6 | 1.0 | 1.5708 | 2.5 | pi |
|---|---|---|---|---|---|---|---|
| metric | 4.00 | 11.96 | 23.64 | 38.35 | 56.57 | 75.92 | 80.0 |
| 2R·sin(θ/2) | 4.00 | 11.96 | 23.64 | 38.35 | 56.57 | 75.92 | 80.0 |
Exact match — the metric is a genuine rotation-sensitive far-vertex statistic.

**EXIT — blindness contrast (why BL-A2 had to exist):**
`OriginAndWholeMeshMetricsAreBlind` — under the same ~90deg break the bone-ORIGIN
(R~0) skinpos delta stays < 0.1u and the whole-mesh AABB ratio reads ~clean,
while the far-vertex metric is 51-59u (far > origin*50). Directly demonstrates
that W2.2's origin oracle / REBIND_DRAW_SKINPOS (47-58u "clean") and the ~1.3x
whole-mesh ratio cannot see the R*sin(theta) finger shard.

**Committed tests (4 GREEN + 1 SKIP):** CoherentUnderAnimation_FailsRedOnPerturb
(GREEN default, RED under env — the fail-red), FarVertShardIsDetected (RED-
capability lock), OriginAndWholeMeshMetricsAreBlind, MetricScalesWithRSinTheta,
RealPathFixture (SKIP — the A6-sanctioned real-band-path hook: BL-A1/S2 dumps a
captured live gameplay pose to `native/tests/goldens/w2.8-farvert/live_pose.txt`
or `RB3_FARVERT_FIXTURE=<path>`, format = `asDrawnXYZ refXYZ R` per far vert;
the metric runs unchanged and EXPECT_LT(worst, 20u) is the real-data fail-red the
S2 fix must turn GREEN).

**Handoff to BL-A1 (S2/S3):** the fix must (a) turn the RealPathFixture arm GREEN
on a real capture, and (b) keep the three math/control tiers GREEN. The rebake
that closes the shard is `offset' = meshWorld * inverse(perMemberBoneBindWorld)`
using the per-member bone's OWN bind basis (not the magnet's) — this oracle is
the numeric gate that a naive rest-capture (W2.2's `RB3_HANDS_BIND_FIX`, judged
"no benefit" under the blind origin gate) provably fails.
