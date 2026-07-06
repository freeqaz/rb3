# W2.6 — Foot/shoe rest-capture coverage + flag-registry cleanup — PLAN

**Lane C (rb3-only, parallel). Engine RENDER/skinning seams READ-ONLY.** Planner: Opus.
Parent: `execution/WAVE4_KICKOFF.md` (COORDINATOR ACCEPTANCE — binding), `execution/WAVE4_REVIEW.md`
(Fable, amendments D1/D2/F2), `execution/README.md` (hard rules 1–8), engine pin `6221a56` (do NOT bump).

## Objective

Two independent deliverables, both landing under W2.6:

**PART 1 — foot/shoe rest-capture coverage (behavior change, DEFAULT-OFF).**
The actual open residual W2.2 filed. The lower-body/outfit skin meshes still **guard-DROP**:
`saddleshoe_skin.2` at **4.73×** (S1a measurement, `W2.2/STATUS.md`), routed through
`RebindHeadHandsAtRest`'s lane but never completing because the shoe/leg bones lack a **clip-free rest
basis** at the time they first-resolve (they stream in as late LOD pieces *during the count-in/walk-on
clip window*). Extend the existing **load-time rest capture** (the `RebindHeadHandsAtRest` /
`NativeCaptureRestPoseAfterDeform` pattern) to cover these lower-body bones, so the mesh can complete
its rest-pose rebind instead of being dropped. rb3-only (`BandCharacter.{cpp,h}`), engine READ-ONLY
(reuse the existing `RndMesh::mNativeBonesRebound` + `RB3_GUARD_EXEMPT_REBOUND` seams; **NO
`Rnd_Wgpu_RB3.cpp` / `DrawMesh` edits**), DEFAULT-OFF behind its own registered flag, inheriting
W2.2's four-layer anti-revert staging.

**PART 2 — flag-registry cleanup (metadata, no behavior).**
Register `RB3_HANDS_BIND_FIX` (W2.2's outstanding handoff) and `RB3_SKEL_REBIND_FULL` (the
known-broken fail-red control) in the classification sidecar; extend
`scripts/analysis/native_compat_census.py` to **also scan `rb3/src/system/`** (currently only
`milo-native-engine/src` + `rb3/native/src` are scan roots, so all 90 game-code native flags went
undetected); classify every newly-surfaced flag and regenerate the ledger so `census check` exits 0.

## Faithful-reference citations (verified at pin `6221a56`, 2026-07-06)

- **The drop & its lane** — `RebindHeadHandsAtRest` (`src/system/bandobj/BandCharacter.cpp:1214`)
  processes every **non-torso** skin mesh (the torso set `trackjacket|vestdenim|plaidshirt|shred` is
  owned by `RebindOutfitBonesToOwnSkeleton` :1062). Shoes/socks/legs fall to this method's lane.
- **Why the shoe drops (the mechanism to confirm in S1)** — a bone's FIRST distinct resolve that
  lands **mid-clip** is rejected (`missWhy="clipPlaying"`, :1360/:1396) unless a **clip-free load-time
  seed** already exists in `mNativeRestPose` (`RB3_HANDS_BIND_FIX` reuse branch, :1383–1394, requires
  `rp != mNativeRestPose.end()`). A rejected bone → mesh stays `pending` (:1453) → the engine V24
  ratio guard drops it every frame. The in-file comment names the exact symptom: "S1a measured:
  `saddleshoe_skin.2` 4.73x DROP … all in the count-in window" (:1367).
- **Why no clip-free seed exists for late lower-body bones** — `NativeCaptureRestPoseAfterDeform`
  (:934) is the load-time seeder, called from `SyncObjects()` right after `SetDeformation()`
  (:1711/:1727) where the skeleton holds the deterministic gender-bind REST pose. It has a **poison
  guard that SKIPS entirely while any clip plays** (:946–953), and it only iterates bones of
  **already-collected** meshes (:957–990). Late-streaming shoe meshes that arrive during the count-in
  clip are therefore never seeded clip-free → their first (distinct, mid-clip) resolve has `rp == end`
  → even `RB3_HANDS_BIND_FIX` cannot complete them. This is the coverage gap PART 1 closes.
- **Rest-capture, char-space, is the thesis (NOT bind-pose)** — `NativeCharSpaceRestXfm` (:905)
  divides out placement; the C8 deep-dive proved a world-space or mid-clip bake produces the
  **200-460u R·sin(θ) smear** the guard hides (:890–904, :1232–1242). **STOP-TRIPWIRE:** if PART 1's
  fix produces a 200-460u smear or a >92u skinpos on any rebound appendage, the thesis is wrong for
  that mesh (rotation-basis, not rest-basis) — STOP, do not force it.
- **Guard-exempt seam (engine, READ-ONLY)** — `mesh->mNativeBonesRebound = true` (set at
  :1122/:1279/:1451) makes the engine skip its rebake AND fling-clamp; `RB3_GUARD_EXEMPT_REBOUND`
  (`milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp:3764/3776`) is the read-only backstop toggle.
  PART 1 uses these existing seams; it writes NO engine code.
- **Gate harness** — `scripts/native/hands_bind_characterize.py` (W2.2.S1a) drives rb3-native
  headless boot→gameplay→count-in→steady with `REBIND_DRAW_SKINPOS` / `REBIND_DRAW_FLING` /
  `SHARD_RATIO_DBG` / `SKIN_CLAMP_PROBE` / `HEAD_REBIND_PROBE` / `RELOAD_PROBE`. Baseline artifacts:
  `W2.2/char/parsed-default.json` (crowd SKIN_CLAMP negative-control reference).
- **Oracle** — `native/tests/test_hands_bind_oracle.cpp` (rb3-tests `HandsBindOracle`): bind-pose
  identity + skinned≈authored, fail-red via `HANDS_BIND_ORACLE_PERTURB`.
- **Census** — `scripts/analysis/native_compat_census.py`. Current `check`: **OK, 230 flags,
  regen clean**; `--selftest`: **14/14 PASS** (selftest passes explicit fixture roots, so a new real
  SCAN_ROOT does not affect it). MEASURED: `rb3/src/system` has **90 distinct native `getenv` flags,
  0 currently in the sidecar** (89-entry sidecar is engine/glue only). Sidecar + `gen.inc` live
  engine-side (`milo-native-engine/src/platform/`); the ledger + census.py live rb3-side.

## Subtasks

### W2.6.S1 — Diagnose the drop, build the fail-red baseline, BRANCH the item — `model: opus`
**Goal:** Prove (or refute) the PART-1 root cause on the CURRENT build before writing any fix, and
capture the default-OFF baseline the S3 A/B is measured against.
**Files:** none source-side (read-only investigation + artifacts under `W2.6/char/`).
**Steps:**
1. Build `rb3-native` in `native/build-agent-W2.6` (Clang, per README build line).
2. Run `hands_bind_characterize.py` on HEAD (no W2.6 flag exists yet) with `RELOAD_PROBE=1
   HEAD_REBIND_PROBE=1 SHARD_RATIO_DBG=1 REBIND_DRAW_SKINPOS=1 REBIND_DRAW_FLING=1
   SKIN_CLAMP_PROBE=1`. Save raw + parsed logs to `W2.6/char/s1-baseline.*`.
3. Confirm the drop set: `saddleshoe_skin.2` at ~4.73× and enumerate every other foot/lower-body
   mesh that DROPs (pants/socks/legs). Record ratio, max SKINPOS, FLING count, crowd SKIN_CLAMP
   counts (the negative-control baseline).
4. Confirm the **mechanism**: `[REST_SEED]` shows the shoe/leg bones were NOT seeded clip-free (or
   seeded only mid-song → poison-skipped), and `[HEAD_REBIND_PENDING]` shows `why=clipPlaying` with
   no reusable seed for those meshes. Cross-check against the code cites above.
5. **BRANCH (records the item's mode in STATUS.md):**
   - **FIX-VIABLE** (mechanism = missing clip-free seed for late lower-body bones) → proceed to S2.
   - **STOP / diagnosis-only** (the shoe bones DO have a clip-free seed yet still shard >92u /
     200-460u — a rotation-basis failure the bind side cannot fix) → S2 becomes a no-op; W2.6 lands
     PART 2 only + a diagnosis memo. This mirrors W2.2.S1a's decision-gate discipline.
**Verification:** the baseline is the **fail-red proof** — on HEAD the foot meshes DROP (ratio 4.73×),
which is exactly the RED the S3 flag-ON gate must clear. Exit 0 only if gameplay was reached (a boot
crash must not read as "no drop").

### W2.6.S2 — Implement lower-body rest-capture coverage, DEFAULT-OFF — `model: opus`
**(only if S1 = FIX-VIABLE)**
**Goal:** Give the late-streaming lower-body meshes a clip-free rest basis so they complete their
rest-pose rebind, behind a new default-OFF flag. Char-space rest capture ONLY (STOP-TRIPWIRE).
**Files:** `src/system/bandobj/BandCharacter.cpp` (+ `BandCharacter.h` if a latch member is needed).
**New flag:** `RB3_FOOT_REST_CAPTURE` (class:workaround, default OFF, truthy read via the existing
`getenv(...) ? 1 : 0` idiom, static-latched like its siblings).
**Steps (both halves gated by the ONE flag; no intermediate commit with only one half live):**
1. **Load-time seed extension.** At the clip-free initial `SyncObjects` deform site (:1711–1727,
   where `NativeCaptureRestPoseAfterDeform` already runs), seed the per-member **lower-body skeleton
   bones by name** into `mNativeRestPose` (+ `mNativeRestDistinct`) using `NativeCharSpaceRestXfm`,
   even if the shoe MESH has not streamed in yet — so the seed is present when the mesh later
   first-resolves mid-clip. Scope to a lower-body name set (`shoe|saddleshoe|sock|leg|ankle|toe|
   pants|boot`), reuse the finite/NaN guard (:984). Keep the existing poison guard intact for the
   general path; only ADD the lower-body pre-seed, so flag-OFF is byte-identical.
2. **Lower-body completion branch.** In `RebindHeadHandsAtRest`, for a lower-body-scoped mesh whose
   bone first-resolves mid-clip with a reusable clip-free seed (`rp != end`), complete the rebind by
   reusing the seed — a self-contained mirror of the `RB3_HANDS_BIND_FIX` branch (:1383–1394) scoped
   to the lower-body set and gated by `RB3_FOOT_REST_CAPTURE` (does NOT require `RB3_HANDS_BIND_FIX`).
   Set `mesh->mNativeBonesRebound = true` on completion (existing seam).
3. Guard EVERY new statement behind `sFootRestCapture` so flag-OFF changes nothing (early-out `if
   (!sFootRestCapture) …` around both the pre-seed and the completion branch).
4. Register the flag in the sidecar in S4 (PART 2), not here.
**Verification:** build green; `git diff` shows every added line inside a flag guard; flag-OFF path
untouched (reviewed line-by-line). Full numeric verification is S3.
**Fail-red demo (staging layer 1 of 4):** with the flag ON, the S3 characterize A/B must show the
drop CLEAR; with it OFF, the S1 baseline drop must reproduce byte-identically — i.e. the flag is the
only thing that moves the metric.

### W2.6.S3 — Verify (flag-OFF byte-identical + flag-ON gates + STOP-tripwire) — `model: opus`
**(only if S2 ran)**
**Goal:** Prove flag-OFF is inert and flag-ON clears the drop without tripping the rotation-basis
tripwire; produce the go/no-go package for the coordinator-gated flip.
**Files:** artifacts under `W2.6/char/s3-*` + STATUS.md.
**Steps / gates:**
1. **flag-OFF byte-identical:** `hands_bind_characterize.py` default pass with the flag unset vs the
   S1 baseline — parsed metrics identical; drawlog splash canonical golden 0-unexpected
   (`drawlog-golden.py --canonical-order`); W0.5 `lineup-gate.py` PASS all layers; `rb3-tests` green
   incl. `HandsBindOracle` + its `HANDS_BIND_ORACLE_PERTURB` fail-red.
2. **flag-ON drop clears:** `RB3_FOOT_REST_CAPTURE=1` characterize A/B — `saddleshoe_skin.2` and
   every S1-dropped foot/lower-body mesh: **shard-ratio ≤2×, no DROP on foot meshes,
   REBIND_DRAW_FLING(>120u)=0**.
3. **STOP-TRIPWIRE:** max `REBIND_DRAW_SKINPOS < 92u`, **NO 200-460u smear** anywhere. If tripped →
   STOP, revert S2, item becomes diagnosis-only (record in STATUS.md).
4. **Negative control:** crowd/extras `SKIN_CLAMP` counts **byte-identical** vs
   `W2.2/char/parsed-default.json` (the fix must not touch non-rebound crowd/extras).
5. **W0.5 lineup PASS** on the flag-ON path (img/segA/ratioB/countC/pin layers).
6. Write the go/no-go package (metrics table + flag-OFF-vs-ON deltas) to `W2.6/char/S3_MEASURE.md`.
**Verification:** all six gates green ⇒ recommend flip; any red ⇒ no-flip, documented. **The default
flip (`RB3_FOOT_REST_CAPTURE` → default-ON) is a SEPARATE one-line coordinator-gated commit, NOT in
W2.6's lane** (four-layer staging: (1) flag, (2) oracle, (3) characterize gate, (4) lineup).

### W2.6.S4 — Flag-registry cleanup: scan rb3/src/system + classify + regen — `model: sonnet`
**Goal:** Close the census coverage gap; register the three headline flags + every newly-surfaced
`rb3/src/system` flag so `census check` exits 0. Metadata-only — no behavior.
**Files (rb3):** `scripts/analysis/native_compat_census.py`,
`docs/native/engine-arch-review-2026-07-05/NATIVE_COMPAT_LEDGER.md` (regenerated).
**Files (engine, metadata-only — see Risks):** `src/platform/NativeCompatFlags.classification.json`,
`src/platform/NativeCompatFlags.gen.inc` (regenerated).
**Steps:**
1. Add a third scan root to `SCAN_ROOTS`: `("game", RB3_ROOT / "src" / "system")`. Update the summary
   accounting to include a `gameOnly` bucket (currently a game-only flag counts in neither
   engineOnly/glueOnly/shared — cosmetic, but fix it for honest counts). Re-run `--selftest` → must
   stay **14/14** (it passes explicit fixture roots, unaffected).
2. Add classification.json entries for all 90 newly-surfaced flags. **Precise (mandatory):**
   - `RB3_HANDS_BIND_FIX` → `class:feature, default:off, owner:W2.2, faithfulStatus:"native-only
     experiment; measured no benefit, not flipped"`.
   - `RB3_SKEL_REBIND_FULL` → `class:workaround, default:off, owner:W0.1, faithfulStatus:"known-broken
     full-body rebind — the Phase-0 fail-red control; shards thin geo, never ship ON"`.
   - `RB3_FOOT_REST_CAPTURE` (the new W2.6 flag) → `class:workaround, default:off, owner:W2.6,
     faithfulStatus:"lower-body rest-capture coverage; default-OFF pending coordinator flip"`.
   - **Best-effort bulk** for the remaining ~87 by rubric: `*_DBG|*_DBG2|*_PROBE|*_TRACE` →
     `class:probe, default:off`; `*_OFF` (opt-out) → `class:workaround, default:on`; `*_MS|*_SECS|
     *_CAP|*_BUDGET` value knobs → `class:perf` + `read:value`; `RB3_NO_*` → `class:workaround`.
3. Run `gen` → regenerate `gen.inc` + ledger. Run `check` → **exit 0** (all scanned names present,
   regen clean). Run `--selftest` → 14/14.
**Verification:** `census check` exits 0; `--selftest` 14/14; the three headline flags are
`class != unknown` in the ledger. **The engine-side commit (classification.json + gen.inc) is
metadata-only, no pin bump — coordinate with the coordinator per Risks to avoid racing Lane A/W2.1's
engine-flag registration on the same generated file.**

## Exit criteria (measurable)

**PART 1 (behavior — gated, default-OFF):**
1. **flag-OFF byte-identical:** with `RB3_FOOT_REST_CAPTURE` unset, `hands_bind_characterize.py`
   default metrics == S1 baseline; drawlog splash canonical golden 0-unexpected; W0.5 lineup PASS all
   layers; `rb3-tests` green incl. `HandsBindOracle` + fail-red.
2. **flag-ON clears the drop:** `saddleshoe_skin.2` and every S1-dropped foot/lower-body mesh at
   shard-ratio **≤2×**, **no DROP** on foot meshes, **FLING(>120u)=0**.
3. **STOP-tripwire clear:** max SKINPOS **<92u**, **no 200-460u smear**.
4. **Negative control:** crowd/extras SKIN_CLAMP counts **byte-identical** vs
   `W2.2/char/parsed-default.json`.
5. `HandsBindOracle` green + `HANDS_BIND_ORACLE_PERTURB` fail-red RED.
6. Default flip is a **separate coordinator-gated commit**, NOT in W2.6. (If S1=STOP, PART 1 lands as
   diagnosis-only with NO behavior change and criteria 1/5 still hold.)

**PART 2 (metadata):**
7. `native_compat_census.py check` **exits 0** with `rb3/src/system` scanned (all 90 flags present in
   the regenerated `gen.inc`); `--selftest` **14/14**.
8. `RB3_HANDS_BIND_FIX`, `RB3_SKEL_REBIND_FULL`, `RB3_FOOT_REST_CAPTURE` classified (`class != unknown`)
   in `classification.json`; `NATIVE_COMPAT_LEDGER.md` regenerated + committed.

## Files touched (exact repo-relative paths — coordinator cross-diffs lanes)

**rb3 repo:**
- `src/system/bandobj/BandCharacter.cpp` (PART 1, S2)
- `src/system/bandobj/BandCharacter.h` (PART 1, S2 — only if a latch member is needed)
- `scripts/analysis/native_compat_census.py` (PART 2, S4 — add scan root)
- `docs/native/engine-arch-review-2026-07-05/NATIVE_COMPAT_LEDGER.md` (PART 2, S4 — regenerated)
- `docs/native/engine-arch-review-2026-07-05/execution/W2.6/{PLAN.md,STATUS.md}` + `char/*` artifacts

**milo-native-engine repo (metadata-only, no pin bump — see Risks):**
- `src/platform/NativeCompatFlags.classification.json` (PART 2, S4)
- `src/platform/NativeCompatFlags.gen.inc` (PART 2, S4 — regenerated)

**Explicitly NOT touched:** `src/App.cpp`; any engine `Rnd_Wgpu_RB3.cpp` / `DrawMesh` code; the
sibling `FxSendNative.cpp` edit; `native/build-native` / `build-web*` / `build-coord-*`.

## Risks / conflicts

- **Lane collision on engine files (PART 2).** Lane A (W2.1→W2.3) is editing engine
  `Rnd_Wgpu_RB3.cpp` and will register its OWN engine-side flags (`RB3_PLACEMENT_CONTRACT`, W2.3's
  flag) into the SAME `classification.json` + regenerated `gen.inc`. If W2.6 also commits an engine
  `gen.inc` regen concurrently, the two lanes clobber the generated file (hard rules 2/4/7/8 exist for
  exactly this). **MITIGATION (per WAVE4_REVIEW F2/point-4 "Lane C's rb3-side flags follow the W2.2
  precedent — coordinator appends at pin-bump"):** W2.6 authors ALL the content — the census.py
  scan-root extension, the 90 classification entries, and the regenerated ledger — and **commits the
  rb3-side files itself** (census.py + ledger). It provides the `classification.json` additions and a
  freshly-`gen`-erated `gen.inc`, but **hands the engine-side commit to the coordinator to merge at
  pin-bump** after all lanes' engine flags are in, so the generated file is written once. W2.6 proves
  `check` exits 0 locally (against its own merged tree) to satisfy criterion 7; the authoritative
  wave-exit `check` is coordinator-run. If the coordinator prefers, W2.6 may instead make a
  metadata-only engine commit under `flock /tmp/milo-engine-git.lock` (no pin bump, no behavior) — but
  the handoff is the collision-safe default.
- **"Engine READ-ONLY" scope.** PART 1 touches NO engine code (uses `mNativeBonesRebound` /
  `RB3_GUARD_EXEMPT_REBOUND` read-only). PART 2's engine files are registry **metadata** (a data
  sidecar + a generated table), not behavior/seam code — consistent with "engine READ-ONLY" meaning
  no engine behavior/seam changes. Documented so the coordinator's cross-diff expects it.
- **Root-cause uncertainty (PART 1).** The fix assumes the drop is a *missing clip-free seed*, not a
  rotation-basis failure. S1 is a hard decision gate: if the seed is present yet the mesh still shards
  (>92u / 200-460u), the STOP-TRIPWIRE fires and W2.6 lands PART 2 + a diagnosis memo only. This is
  by design — a no-fix outcome is an acceptable, honest result, not a failure.
- **Lane independence.** W2.6 is rb3-only + engine-seams READ-ONLY, so it does NOT collide with Lane
  A (W2.1→W2.3, engine `DrawMesh`, sequential) or Lane B (W0.3d, rb3 `src/system/char/` + sidecar;
  W0.3d part-b is diagnosis-only wrt Lane-A files). The only shared surface is PART 2's engine
  `gen.inc`/`classification.json`, handled by the mitigation above.
- **Census scan cost / correctness.** Scanning `src/system` (large decomp tree) is a text grep —
  fast, and it only reads native `getenv` literals (HX_NATIVE flags). The selftest uses explicit
  fixture roots so it is unaffected by the new real root; re-run `--selftest` to confirm 14/14.
