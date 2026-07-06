# W2.2 — Hands/fingers bind fix — PLAN (Lane B, rb3-only, engine READ-ONLY)

**Role:** Planner (Opus). **Wave:** 3. **Lane:** B (rb3-only, staged S1→S4, parallel to Lane A).
**Engine pin:** `41b9e3a` (do NOT bump; coordinator bumps per wave). **Engine repo = READ-ONLY.**
**W0.3-dependency:** WAIVED for this item (WAVE3_REVIEW A1; recorded in STATUS).

---

## 0. Headline finding — the brief's "untried" premise is STALE (read this first)

The brief and WAVE3_KICKOFF describe W2.2's thesis as the **untried** "bind-pose-captured rebake":
rebind outfit+appendage meshes to the per-member animated `skeleton_unshared` **and** rebake
`invBind` as `offset' = meshBindWorld · inverse(perMemberBoneBindWorld)` captured at
skeleton-load/first-pose.

**Investigation of the CURRENT tree (post-Wave-2) shows this fix is ALREADY IMPLEMENTED and
default-ON.** It is not untried — it shipped as the C7/C8 render-polish work:

- `BandCharacter::RebindHeadHandsAtRest()` — `src/system/bandobj/BandCharacter.cpp:1214` (decl
  `.h:140`). Called from `Poll()` at `:522`, **before `Character::Poll()`** (comment `:515-521`).
  For each head/hair/hands/face skin bone it repoints to the live per-member instance and bakes
  `Multiply(mesh->WorldXfm(), invRest, mesh->BoneOffsetAt(b))` where `invRest = inverse(restWorld)`
  (`:1425-1428`). That is **exactly** `offset' = meshWorld · inverse(perMemberBoneBindWorld)`.
- The **load-time bind-pose capture seam EXISTS**: `NativeCaptureRestPoseAfterDeform()`
  (`.cpp:934`, decl `.h:173`) is called from `SyncObjects` at `:1693`, immediately after
  `SetDeformation()` posed the skeleton at "the weighted gender-bind REST pose — the one
  deterministic rest point in the flow" (`:1680-1693`). This **resolves the wave-08 blocker**
  ("the skeleton is already animating when reachable") that the brief's S1-EXIT branch worries
  about. The rest is captured in CHARACTER space (placement divided out — `NativeCharSpaceRestXfm`,
  `:905`), with a clip-playing poison-guard so mid-clip poses are never baked as rest.
- Provenance: `git log` — `0de768a1` (C7/C8 head/hair/hands shard fix), `2580e128` (adversarial
  harden), `3c02e08b` (reload-re-entrant + two-pass apply + post-deform seeding), `0f2f5df2`
  (own==bound rebake → opt-in `RB3_BOUND_REBAKE`), `491288ec` (capture in CHARACTER space — C8 root
  cause), `2f393eaa` (instrument `_strings` rebind). Opt-out: `RB3_NO_HEAD_REBIND=1`.

The torso path (`RebindOutfitBonesToOwnSkeleton`, `.cpp:1062`, `calcOffset=false`, `Poll()` AFTER
`Character::Poll()`, `:570`) is the wave-08 default-ON torso rebind, retained as-is.

**Consequence for W2.2:** the item **pivots** from "implement the fix" to **"build the numeric
oracle the fix never had, characterize the existing default-ON behavior against it, and gate any
net-new change default-OFF."** Per WAVE3_REVIEW Missing-item #1, the single most important
deliverable is exactly the gate that is still missing: *"W2.2 without the bind-pose identity gtest
is the exact SYS-7 hole that shipped both BandPatchMesh reverts."* That gate does not exist today
and is the highest-value output of this item regardless of whether the current behavior passes it.

This pivot is faithful to the brief's binding four-layer exit (B1): S1 builds the gates FIRST; S3
measures against the numeric bars; S4 flip is separate + reviewer-gated; behavior-changing deltas
land default-OFF behind a new registered flag. It only corrects the stale "this is greenfield"
assumption. **The coordinator must be aware of this reframing** (flagged in Risks §R1 + STATUS).

---

## 1. Objective

Give the already-shipping per-member bind-pose rebake a **falsifiable numeric correctness oracle**
and a **hand/finger/hair-closeup visual gate incl. the count-in/walkon window**, then:
- **If the current default-ON `RebindHeadHandsAtRest` passes both** (≤65u incl. hands/fingers/hair,
  0 flings, shard-ratio ≤2×, identity residual within eps, visual clean vs Dolphin/retail): deliver
  the oracle + harness + a validation report that retroactively supplies the SYS-7 gate the shipped
  code never had. No net-new default-ON behavior is introduced.
- **If it still shards anywhere** (the known count-in/walkon thin-geo residual is the prime
  suspect — memory: "thin-geo count-in shards = pose-independent skinning residual"): implement a
  **targeted** fix behind a **new default-OFF** registered flag, staged S2→S4, with the existing
  head-rebind + torso-only paths retained as opt-backs.

Worst case under this staging = an unflipped flag + a characterization writeup. A BandPatchMesh-style
blind revert is **structurally impossible** because nothing behavior-changing lands un-gated or
default-on-unproven, and the oracle makes "different vs right" decidable.

### Faithful-reference citations
- `docs/native/CHAR_SKINNING_DEFORM_INVESTIGATION.md` — H2 rotation-basis mismatch (`:152-189`),
  wave-08 IMPLEMENT (`:94-195`), wave-08 OPEN item (`:185-193`, the bind-pose-capture thesis),
  DC3 identical bind-pose-inverse compose (`:1163-1200`), authoritative metric `|skinWorld−boneWorld|`
  (`:180-184`, NOT mesh-local which reads back world placement).
- WAVE3_REVIEW R-A (`:20-57`, invariance-net premise), R-B (`:59-135`, four-layer exit + tripwire),
  Missing-items #1-#4 (`:238-252`).
- The mathematical oracle (WAVE3_REVIEW `:73-79`): by definition of `invBind`, at the captured bind
  pose `offset' · perMemberBoneBindWorld` = identity in mesh space ⇒ **skinned verts at bind pose
  must equal authored (decoded) verts**. Falsifiable, fail-red-able, Dolphin-independent.
- Engine skin compose (READ-ONLY): `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` —
  `Multiply(owner->BoneOffsetAt(b), boneTrans->WorldXfm())` palette build; `mNativeBonesRebound`
  skip at `:2920-2921`, `:3190`, `:3226`; `REBIND_DRAW_SKINPOS/FLING` at `:3199/:3214`;
  `SKIN_CLAMP_PROBE` at `:3238-3242`; shard-guard + `RB3_GUARD_EXEMPT_REBOUND` at `:3712-3729`.

### STOP-TRIPWIRE (do NOT re-run the failed experiment)
The FAILED prior experiment is the **own==bound rest-rebake** path: `RB3_BOUND_REBAKE=1` +
`RB3_GUARD_EXEMPT_REBOUND=1` (engine `Rnd_Wgpu_RB3.cpp:3715-3729`, default OFF). It anchored
translation (≤92u) but the rotation-basis divergence smeared far-from-bone verts to persistent
**200-460u** extents (gloves/nails/jackets vs ~70u character) and exempt meshes drew as full-screen
slabs. **This is distinct** from the DISTINCT-resolve rest-rebake in `RebindHeadHandsAtRest` (the
default-ON path this item validates). **Failure signature = STOP:** if any measurement shows
`SKINPOS > ~92u`, `shard-ratio > 2×`, or `200-460u` extents on a rebound appendage mesh, you have
reproduced the failure — record it, do not "fix forward" into the engine, treat it as the S1a branch
that says the current default-ON is NOT clean.

---

## 2. Subtasks

> Ordering is strict: **S1 gates land before any bind change.** S1a is the decision gate that
> branches S2. Each behavior-changing subtask names its default-OFF/gate staging + fail-red demo.
> All new flags are rb3-side getenv reads in `rb3/src` (NOT a census scan root — see Risks §R4);
> the registry entry is a coordinator data-edit, never a W2.2 engine edit.

### W2.2.S1a — Characterize the CURRENT default-ON rebind (DECISION GATE) — model: opus
**Goal:** measure whether the shipping `RebindHeadHandsAtRest` (default-ON) already meets the S3
numeric bars on hands/fingers/hair/head, or still shards — this branches S2 (fix vs
validate-only). Also capture the crowd/extras `SKIN_CLAMP` baseline (S3 negative control).
**Files (all NEW, no shipped-source edits):** `scripts/native/hands_bind_characterize.py`,
outputs under `docs/native/engine-arch-review-2026-07-05/execution/W2.2/char/`.
**Steps:**
1. Build rb3-native in the OWN build dir:
   `cmake -B native/build-agent-W2.2 -S native -DCMAKE_C_COMPILER=/usr/bin/clang
   -DCMAKE_CXX_COMPILER=/usr/bin/clang++ && cmake --build native/build-agent-W2.2 --target rb3-native`.
2. Drive headless to gameplay (song-select-capture.py / song-end-test.py pattern over `RB3_HTTP=1`),
   traversing boot → song-select → gameplay → **the count-in/walk-on window** (known thin-geo shard
   window). Capture with these engine diagnostics (all read-only, already in-tree):
   `REBIND_DRAW_SKINPOS=1 REBIND_DRAW_FLING=1 SHARD_RATIO_DBG=1 SKIN_CLAMP_PROBE=1
   HEAD_REBIND_PROBE=1` (use `grep -a` — logs carry interleaved NUL bytes).
3. Reduce per-mesh: max `SKINPOS`, `FLING(>120u)` count, `shard-ratio`, split by mesh class
   (torso vs head/hair/hands/face/nails/gloves). Record which meshes (if any) exceed 65u / 120u /
   2× and whether any hit the **200-460u tripwire signature**.
4. Record the crowd/extras `[SKIN_CLAMP]` counts (baseline for the S3 negative control) and a full
   `[CHAR_MESH]` inventory (band vs venue-extra attribution).
5. Repeat once with `RB3_NO_HEAD_REBIND=1` (opt-out) to get the before/after delta the current
   default-ON provides.
**Verification / exit:** a `char/CHARACTERIZATION.md` with the per-mesh table + an explicit verdict:
**BRANCH-CLEAN** (current default-ON already ≤65u/0-fling/≤2× on ALL appendages incl. count-in) or
**BRANCH-RESIDUAL** (named meshes/window still shard). This verdict selects S2's mode.

### W2.2.S1b — Numeric bind-pose identity gtest oracle (fail-red) — model: opus
**Goal:** the falsifiable invariant BandPatchMesh never had: at the captured bind pose,
`offset' · perMemberBoneBindWorld ≈ identity` AND skinned verts ≈ authored decoded verts (eps).
**Files (NEW):** `native/tests/test_hands_bind_oracle.cpp`; `native/CMakeLists.txt` (add the one
source line to the `rb3-tests` target list at ~`:704-717`); optional fixture under
`native/tests/goldens/w2.2-hands/`.
**Steps:**
1. **Primary — math/compose unit test (always buildable):** construct representative
   `meshBindWorld` + `perMemberBoneBindWorld` transforms (synthetic + long-thin finger offset to
   exercise the R·sin(θ) failure mode), compute `offset' = meshWorld · inverse(restWorld)` exactly as
   `RebindHeadHandsAtRest:1425-1428`, then assert (a) `offset' · restWorld ≈ I` (‖·−I‖ < eps),
   (b) a vert at radius R skinned by `BoneOffsetAt·WorldXfm` at bind pose returns the authored vert
   within eps. Use the engine `Transform`/`Multiply`/`Invert` (linked via `_RB3_NATIVE_SRCS`, same as
   the existing tests). This is the numeric core of the invariant.
2. **Real-path arm (best-effort, test_charload5b pattern):** register the obj/rndobj/char factory
   set (copy from `test_charload5b.cpp:47+`), load a real char/outfit milo, walk a rebound mesh's
   bones and assert the same identity residual on REAL captured transforms. If the full BandCharacter
   SyncObjects/deform chain is not drivable headless (reachability risk — see Risks §R3), fall back
   to a fixture dumped by S1a's in-game probe (`offset'`,`restWorld`,authored-vert triples) and assert
   the invariant on that real data. Either way the invariant is checked on real numbers.
3. **Fail-red:** a `HANDS_BIND_ORACLE_PERTURB=<θ>` env that rotates the captured bind pose before
   the bake — the identity residual and the skinned-vert error must both exceed eps and turn the test
   RED (proving the gate detects a wrong-basis bind — the exact BandPatchMesh hole). Demonstrate RED,
   then GREEN unperturbed.
**Verification:** `ctest -R HandsBindOracle` GREEN unperturbed; `HANDS_BIND_ORACLE_PERTURB=0.15
ctest -R HandsBindOracle` RED. Registered in the `rb3-tests` bar (must not regress the suite).

### W2.2.S1c — Hand-closeup capture harness incl. count-in/walkon window — model: sonnet
**Goal:** deterministic framed hand/finger/hair closeup shots (the reviewer-judged visual layer),
explicitly covering the count-in/walk-on thin-geo window the W0.5 lineup gate does NOT frame.
**Files (NEW):** `scripts/native/hand-closeup-capture.py`; goldens under
`docs/native/engine-arch-review-2026-07-05/execution/W2.2/goldens/hand-closeup/`.
**Steps:**
1. Follow `scripts/native/song-select-capture.py` structure: boot `RB3_HTTP=1 rb3-native`, nav to
   gameplay, `/api/screenshot` at framed hand/fret/hair moments AND during count-in/walk-on
   (use the walk-on window timing; `RB3_WALKON_SNAP_OFF` toggle documented in memory to A/B it).
2. Emit fixed-name PNGs + a manifest (frame label, sim time, camera). Baseline set committed as the
   golden reference for S3/S4 reviewer comparison.
**Verification:** running it twice produces frame-stable shots (same labels/positions); the count-in
window frame is present and shows hand/finger geometry.

### W2.2.S2 — Net-new fix behind a NEW default-OFF flag (CONDITIONAL on S1a) — model: opus
**Goal:** ONLY if S1a = BRANCH-RESIDUAL. Implement the smallest targeted change that clears the
residual (most likely: guarantee a clip-free bind-pose capture for the count-in/walk-on window, or
extend rest-capture coverage to the still-sharding meshes) behind a **new default-OFF** registered
flag `RB3_HANDS_BIND_FIX` (class: feature). If S1a = BRANCH-CLEAN, S2 is a **no-op documentation
subtask** (record that the existing default-ON path already satisfies the oracle — no net-new
behavior; the deliverable is oracle+harness+validation).
**Files (conditional):** `src/system/bandobj/BandCharacter.cpp`, `src/system/bandobj/BandCharacter.h`.
**NOT touched:** engine repo (READ-ONLY — use `mNativeBonesRebound` + `RB3_GUARD_EXEMPT_REBOUND=1`
seams only); `src/App.cpp` (forbidden — D2); `DrawMesh` (Lane A / W1.6 collision — the shard-guard
default flip is a Wave-4 coordinator one-liner, D1).
**Staging (binding, brief B1):** DEFAULT-OFF. The existing default-ON `RebindHeadHandsAtRest` and the
torso-only path are BOTH retained as opt-backs ("supersedes" = default-*replaces* in a later wave,
never *removes* this wave — R5 pattern). Set `mNativeBonesRebound` on any newly-rebound mesh (existing
convention). Run captures with `RB3_GUARD_EXEMPT_REBOUND=1`.
**Flag registration (Risks §R4):** `BandCharacter.cpp` is under `rb3/src`, NOT a census scan root, so
the getenv won't trip `native_compat_census.py check`. Registry hygiene is still required: append the
`RB3_HANDS_BIND_FIX` entry (class:feature, owner, faithfulStatus) to
`milo-native-engine/.../NativeCompatFlags.classification.json` **as a coordinator/pin-bump data edit**
(engine READ-ONLY forbids W2.2 doing it). Provide the exact JSON entry text in STATUS for the
coordinator; verify `native_compat_census.py check` still exits 0 (it will, since the flag is
rb3/src).
**Fail-red:** the new-flag path must have a demonstrable RED via the S1b oracle when the fix's bind
basis is perturbed (reuses S1b's perturb hook), i.e. the fix is itself under the numeric gate.
**MOVE-xor-CHANGE:** this is a CHANGE commit; keep it separate from S1's additive test/script commits.

### W2.2.S3 — Measure all gates + negative control + invariance nets — model: opus
**Goal:** prove the target state (existing default-ON if BRANCH-CLEAN, else the new-flag path).
**Files:** `docs/native/.../W2.2/STATUS.md` + a `char/S3_MEASURE.md`; no source edits.
**Gates (all must pass):**
1. **Numeric draw-time:** `REBIND_DRAW_SKINPOS ≤ ~65u` on ALL rebound meshes **incl.
   hands/fingers/hair**; `REBIND_DRAW_FLING(>120u) = 0` over a full captured gameplay run;
   shard-ratio ≤2× with the guard still ON (`SHARD_RATIO_DBG=1`) — explicitly NOT the 200-460u
   signature. Torso metrics not regressed vs a `RB3_NO_HEAD_REBIND=1`-equivalent torso baseline.
2. **Negative control:** crowd/extras `[SKIN_CLAMP]` counts **byte-identical** vs the S1a baseline
   (the rest-capture hook is new code near shared char paths — this proves band-scoped, extras
   untouched).
3. **Oracle:** `ctest -R HandsBindOracle` GREEN (perturb RED).
4. **Invariance nets:** the DC3-context `milo-engine-tests` suite stays **198 pass / 0 fail / 2 skip**
   (`DC3_DATA`+`MILO_LIB`, `ctest -j1`) — W0.1 `SkinGolden` + W0.4 `ClipPoseFixture` green. A red
   there = leaked into shared skinning math = **auto-stop** (R-A).
5. **W0.5 lineup gate** PASS (patch-bearing lineup, numeric bbox layers).
**Verification:** all five recorded in `S3_MEASURE.md` with the exact numbers + commands.

### W2.2.S4 — Default-flip (SEPARATE one-line commit, coordinator-gated) — model: opus
**Goal:** flip the default ONLY if a net-new `RB3_HANDS_BIND_FIX` was introduced (S2 BRANCH-RESIDUAL)
AND every S1-S3 layer is green AND reviewer-judged visuals pass.
**Files:** one-line default toggle in `src/system/bandobj/BandCharacter.cpp` (separate commit) + STATUS.
**Gate (all required):** S1b oracle green + S3 numeric+negative-control green + W0.5 lineup +
**reviewer-judged wide AND hand-closeup frames vs fresh Dolphin t2 captures**
(`docs/native/c8-ground-truth-2026-07-01/t2-dolphin-oracle.md` recipe; `dolphin-shots/` band
closeups) + `images/retail-screenshots/`. **If reviewer judgment is not available in-wave: DO NOT
flip** — land S2/S3 default-OFF and leave the flip for coordinator sign-off (record in STATUS).
In the BRANCH-CLEAN case there is no flip (nothing new to enable); S4 instead records the validation
sign-off for the already-default-ON path.

---

## 3. Exit criteria (measurable — maps to brief S1→S4)

- **S1 (gates first):** (a) `test_hands_bind_oracle` in `rb3-tests`: `offset'·perMemberBoneBindWorld
  ≈ I` and skinned-vert ≈ authored-vert within eps, **fail-red proven** via `HANDS_BIND_ORACLE_PERTURB`.
  (b) `hand-closeup-capture.py` produces frame-stable hand/finger/hair shots **incl. the
  count-in/walkon window**. (c) S1a characterization verdict (BRANCH-CLEAN | BRANCH-RESIDUAL) recorded.
- **S2 (fix, default-OFF):** any net-new behavior lands **default-OFF** behind registered
  `RB3_HANDS_BIND_FIX`; existing head-rebind + torso-only retained as opt-backs; engine untouched;
  census `check` exits 0; flag JSON entry handed to coordinator. (No-op in BRANCH-CLEAN.)
- **S3 (measure):** SKINPOS ≤65u on ALL rebound meshes incl. hands/fingers/hair; FLING(>120u)=0 over
  a full run; shard-ratio ≤2× (guard ON); NOT the 200-460u signature; crowd/extras `SKIN_CLAMP`
  counts byte-identical vs baseline; torso not regressed; W0.1/W0.4 invariance nets green
  (198/0/2); W0.5 lineup PASS.
- **S4 (flip):** default flip is a SEPARATE one-line commit, gated on S1-S3 all green + W0.5 +
  reviewer-judged wide AND hand-closeup vs fresh Dolphin t2 + retail screenshots. Absent reviewer
  judgment in-wave ⇒ land default-OFF, defer flip to coordinator.
- **S1-EXIT branch:** if the bind-frame capture seam were unreachable (it is NOT — resolved via
  `NativeCaptureRestPoseAfterDeform` at SyncObjects), STOP at "characterize + propose oracle" without
  landing a blind bind change. Recorded as satisfied-not-triggered.
- **Wave gate (unchanged):** engine-test suite 198/0 + lineup PASS + coordinator-only pin bump.

---

## 4. Files touched (exact repo-relative — coordinator cross-diffs lanes)

**rb3 — NEW (additive, S1):**
- `native/tests/test_hands_bind_oracle.cpp`
- `native/CMakeLists.txt` (one line: add the test source to the `rb3-tests` target)
- `scripts/native/hands_bind_characterize.py`
- `scripts/native/hand-closeup-capture.py`
- `docs/native/engine-arch-review-2026-07-05/execution/W2.2/{PLAN.md,STATUS.md,char/*,goldens/hand-closeup/*}`
- (optional) `native/tests/goldens/w2.2-hands/*`

**rb3 — CHANGE, CONDITIONAL on S1a=BRANCH-RESIDUAL (S2/S4, behind default-OFF `RB3_HANDS_BIND_FIX`):**
- `src/system/bandobj/BandCharacter.cpp`
- `src/system/bandobj/BandCharacter.h`

**engine — READ-ONLY (ZERO W2.2 edits).** Uses existing seams only. The
`NativeCompatFlags.classification.json` entry for the new flag is a **coordinator/pin-bump data
edit**, not a W2.2 edit.

**Explicitly NOT touched:** `src/App.cpp` (D2 forbidden); engine `Rnd_Wgpu_RB3.cpp` / `DrawMesh`
(Lane A / W1.6; shard-guard flip is a Wave-4 coordinator one-liner, D1); `src/system/bandobj/
BandConfiguration.cpp` (Lane C / W2.5).

---

## 5. Risks / conflicts

- **R1 (reframing — coordinator must ack):** the brief's "untried bind-pose rebake" is already
  implemented default-ON (`RebindHeadHandsAtRest`, `0de768a1`). W2.2 pivots to oracle-construction +
  characterization + gate-any-net-new-change. This is faithful to the four-layer exit but changes the
  item's shape from "implement" to "validate/gate." Surfaced here + in STATUS so a future audit
  doesn't read the pivot as scope drift.
- **R2 (STOP-tripwire confusion):** the FAILED experiment is `RB3_BOUND_REBAKE` (own==bound path,
  engine :3715, default OFF) with the 200-460u smear — NOT the default-ON distinct-resolve
  `RebindHeadHandsAtRest`. Do not re-run `RB3_BOUND_REBAKE`. 200-460u / >92u / >2× on a rebound
  appendage = reproduced failure = STOP (→ S1a BRANCH-RESIDUAL, gate OFF, do not fix into engine).
- **R3 (S1b real-path reachability):** driving a full BandCharacter through SyncObjects+deform
  headless in a gtest may not be feasible (the wave-08 "reachable only mid-animation" concern applies
  to a *unit* test even though the in-game seam is reachable). Mitigation: S1b's **math/compose unit
  test is always buildable + fail-red**; the real-numbers arm uses a fixture dumped by S1a's in-game
  probe if the in-process load path can't be driven. The invariant is checked on real transforms
  either way.
- **R4 (flag census / engine read-only tension):** `native_compat_census.py` scans `engine/src` +
  `rb3/native/src` only — NOT `rb3/src` where `BandCharacter.cpp` lives — so a new getenv there won't
  trip `check` (stays exit 0). But `classification.json` (the registry) is in the engine repo, which
  is READ-ONLY for W2.2. Resolution: hand the exact JSON entry to the coordinator to append at
  pin-bump (data edit, coordinator-owned like the guard flip). W2.2 does not edit the engine repo.
- **R5 (lane collisions — cross-diff inputs):** Lane A (W0.3c→W1.6) is engine `Rnd_Wgpu_RB3.cpp` +
  extracted TUs — **file-disjoint** from W2.2 (rb3 `BandCharacter.*` + `native/tests` + `scripts`).
  Lane C / W2.5 edits `src/system/bandobj/BandConfiguration.cpp` — same directory, DIFFERENT file, no
  line overlap. **`native/CMakeLists.txt`** is the one shared-ish file: W2.2 adds a `rb3-tests`
  source line; no other Wave-3 rb3 lane is expected to edit it, but flag it as the single merge point
  for coordinator cross-diff. Hard rules 4/7/8 in force: flock git, never reset/rebase on shared
  trees, never touch a sibling lane's lines.
- **R6 (invariance-net auto-stop):** W0.1 `SkinGolden` + W0.4 `ClipPoseFixture` run in DC3-context
  `milo-engine-tests` and NEVER execute the band rebind — they are invariance nets. A red there means
  W2.2 leaked into shared skinning math ⇒ auto-stop. Keep them green untouched; never re-baseline.
- **R7 (count-in/walkon coverage):** the known thin-geo shard window is the count-in/walk-on
  (pose-independent residual; W0.5 lineup does not frame it). S1c mandates a closeup frame there so
  W2.2 cannot pass every gate and still shard at the user-visible song-start moment.
