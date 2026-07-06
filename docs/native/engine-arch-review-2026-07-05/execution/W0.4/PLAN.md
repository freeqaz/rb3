# W0.4 — Bone ground-truth expansion to live pose

**Item:** REFACTOR_PLAN §Phase 0 W0.4 · lane doc `06-arch-crosscut.md` §4.3 point 4 (lines 292–294).
**Owner file (ONLY):** `milo-native-engine/tests/test_bone_ground_truth.cpp` (+ optional STATUS notes).
**Do NOT edit** `tests/CMakeLists.txt` — the file is already registered there (line 88); W0.1 is the
lane that adds a *new* file + a CMakeLists line in parallel, so touching CMakeLists here would collide.

---

## Objective

`test_bone_ground_truth.cpp` today validates bone **topology, hierarchy, symmetry, rest-pose
non-identity, finiteness, determinism, and local↔world composition** — but it **never pins a
specific posed effector WORLD position to a committed number**. Every live-pose test is a *relative*
or *invariant* check (`worldMoved > 0`, `isfinite`, `deterministic`, `world == local·parentWorld`).
That means a placement regression that still produces finite, deterministic, self-consistent output —
exactly the bug class in the count-in shard / hub-bar / crowd co-location incidents — passes green.

W0.4 closes that gap: **apply a pinned clip at pinned beats to the real crowd skeleton, then assert
the WORLD-space `(x,y,z)` of the hand + foot effectors (and a prop/drum-stick bone if one is
reachable) against committed golden constants within an epsilon.** This is the placement-bug net that
grades the Phase-2 skinned-placement rewrite (W2.1/W2.3) against numbers instead of eyeballs.

### Faithful reference (decomp file:line)

- `CharClip::PoseMeshes(ObjectDir*, float)` — `rb3/src/system/char/CharClip.cpp:227`
  (`StuffBones → ScaleDown → ScaleAdd(1.0, beat, 0.0) → CharBonesMeshes::PoseMeshes()`). This is the
  exact pose pipeline the test drives; the golden captures its output.
- `CharBonesMeshes::PoseMeshes()` — `rb3/src/system/char/CharBonesMeshes.cpp:98` (writes each bone's
  local pose back onto its `RndTransformable`).
- World composition the effector positions flow through — `rb3/src/system/rndobj/Trans.cpp:65–74`
  (`Multiply(mLocalXfm, parentWorld, world)`), already re-proven by the existing
  `BoneWorldMatchesLocalComposedWithParent` test (`test_bone_ground_truth.cpp:717`).
- Clip beat API used to pin sample beats — `CharClip::StartBeat/EndBeat/LengthBeats`,
  `rb3/src/system/char/CharClip.h:195–204`.

---

## Key facts established during planning (do not re-derive)

1. **Where this test runs.** `test_bone_ground_truth.cpp` is compiled ONLY by the engine's
   **standalone** `milo-engine-tests` target (`tests/CMakeLists.txt:88`). It is **not** built by rb3's
   `rb3-native`/`rb3-tests` (those compile a different rb3-side test set; the engine's tests default
   `OFF` when the engine is consumed via `add_subdirectory`, since `MILO_ENGINE_BUILD_TESTS` defaults
   to `MILO_ENGINE_STANDALONE`, which is `OFF` in that case — engine `CMakeLists.txt:33-37,141`).
   **Therefore all verification for W0.4 is done by building `milo-engine-tests` standalone in the
   engine repo**, not from an rb3 build dir. Use your own engine build dir (see Build below); never
   reuse `milo-native-engine/build-tests` or `build-dc3ref`.
2. **Assets are deterministic and present** under dc3-decomp (the standalone test data root):
   - Skeleton: `char/shared/gen/skeleton_bones_resource.milo_xbox` — loaded as `sDir` (the
     `BoneGroundTruth` fixture's first candidate).
   - Dance clip source: `char/crowd/anim/gen/female_base.milo_xbox` — first `danceCandidate`
     (`ClipPoseFixture::SetUpTestSuite`, `test_bone_ground_truth.cpp:283-313`), `sDanceClip == true`.
   - Effector bones already confirmed present/soft-checked by existing tests:
     `bone_R-hand.mesh`, `bone_L-hand.mesh` (Gate 1, `:115`), `bone_L-ankle.mesh`,
     `bone_R-ankle.mesh`, `bone_L-toe.mesh`, `bone_R-toe.mesh`, `bone_R-foot.mesh` (Gate 4, `:813`).
3. **The existing `ClipPoseFixture` picks the FIRST non-skeleton clip by iteration order** — that is
   fragile for a golden. The new golden test MUST pin its clip **by name** (not reuse `sClip`
   blindly), and MUST pin explicit beats, so the golden is reproducible.
4. **Determinism is exact within one build config.** `PoseDeterminism` (`:537`) already asserts
   `PoseMeshes` is bit-identical for a repeated beat. Since this test only ever runs under one
   compiler/flag set (host clang, `milo-engine-tests`), the golden is stable; epsilon exists only to
   absorb "behavior-preserving" engine refactors (Phase 1 claims byte-identical render but may reorder
   FP in the pose path). A real placement bug shifts effectors by *tens* of units (memory: count-in
   shard 50–65u), so a **tight** epsilon still has enormous margin. Recommended epsilon: **0.05 world
   units** (model is centimeter-scale, hands ~15u from center); bump to ≤0.25 only if CI shows drift.
5. **Prop/drum-stick bone is NOT expected to be reachable** in `skeleton_bones_resource` (a bare crowd
   humanoid skeleton — no instrument props). Treat it as best-effort per the brief's "if reachable":
   S1 probes for any `bone_*stick*` / prop bone in the loaded dirs; if none, **omit it and document
   the omission in a code comment + STATUS** (hand+foot effectors are the required core). Do not load
   extra prop assets or edit CMakeLists to chase it.

---

## Subtasks

### W0.4.S1 — Design + implement the live-pose effector-golden test (with dump + fail-red hooks)
- **model:** opus
- **goal:** Add one new `TEST_F(ClipPoseFixture, EffectorWorldPositionsMatchGolden)` (plus small
  helpers) to `test_bone_ground_truth.cpp` that: pins a named clip + explicit beats, poses the real
  skeleton, reads effector `WorldXfm().v`, and compares to a golden table with epsilon. Ship it with a
  **golden-dump mode** and a **fail-red perturbation hook** so S2 can fill goldens and prove red. Leave
  the golden table as a clearly-marked placeholder that S2 replaces.
- **files to touch:** `milo-native-engine/tests/test_bone_ground_truth.cpp` ONLY.
- **approach (step by step):**
  1. Read the existing file end-to-end first; reuse `ClipPoseFixture` (it already loads
     `skeleton_bones_resource` as `sDir` and `female_base` as the clip dir). Do NOT change existing
     tests or fixtures' behavior.
  2. Build `milo-engine-tests` standalone (see Build) and run ONLY the existing
     `ClipPoseFixture.*` tests once to (a) list the clip names actually present in `female_base`
     (the `ClipExists` test prints up to 5; if you need the full list, temporarily run with a wider
     print or read `crouching_great_01`'s presence via the existing
     `CrouchingGreatClipContainsForeArmAndHandChannels` test output) and (b) confirm which effector
     bones resolve. **Pick a stable clip by name** — prefer `crouching_great_01` if present (the file
     already special-cases it and it is known to carry L/R foreArm+hand channels, `:362`); otherwise
     pick the first clip whose `ListBones` contains both a hand and a foot/ankle channel and record
     the chosen name in a code comment + STATUS. Do not silently depend on iteration order.
  3. Add a file-local helper `FindClipByName(ObjectDir* clipDir, const char* name) -> CharClip*`
     (iterate `ObjDirItr<CharClip>`), and a small `struct EffectorGolden { const char* bone; float
     beatFrac; float x, y, z; }`.
  4. Choose 3 pinned beats expressed as fractions of the clip range so they survive if the asset's
     absolute beats shift: `beat = StartBeat() + LengthBeats()*frac` for `frac ∈ {0.0, 0.5, 0.9}`
     (avoid EndBeat() exactly — some clips wrap). Pin the effector bone set:
     `bone_R-hand.mesh`, `bone_L-hand.mesh`, `bone_R-toe.mesh`, `bone_L-toe.mesh` (fall back to
     `bone_R-foot.mesh`/`bone_R-ankle.mesh` if a toe is absent — resolve once and record the actual
     name used). Probe for a prop/drum-stick bone (`strstr(name,"stick")` over `ObjDirItr` on `sDir`
     and `sClipDir`); if found add it, else leave a documented comment that no prop bone is reachable
     in this asset.
  5. Write the test body: for each pinned beat, `clip->PoseMeshes(sDir, beat)` once, then for each
     effector read `FindBone(name)->WorldXfm().v` and `EXPECT_NEAR` each component to the golden with
     `kEffectorEps = 0.05f`. Guard `GTEST_SKIP()` cleanly if the pinned clip or all effectors are
     missing (same graceful-skip posture as the rest of the file).
  6. **Golden-dump mode:** if `getenv("MILO_TEST_DUMP_POSE_GOLDEN")` is set, print every measured
     `{bone, frac, x, y, z}` in copy-paste-ready C++ initializer form (e.g.
     `// { "bone_R-hand.mesh", 0.50f, 12.345678f, ... },`) and then `GTEST_SKIP()` (dump runs do not
     assert). This is how S2 captures goldens without hand-transcribing floats.
  7. **Fail-red hook:** read `float perturb = getenv("MILO_TEST_POSE_PERTURB") ? atof(...) : 0.0f;`
     and, when non-zero, add `perturb` to the measured `x` before comparing (a synthetic placement
     error). Default 0 → no behavior change. This lets S2/CI demonstrate the gate fails red on a real
     effector displacement, mirroring W0.1's `RB3_SKEL_REBIND_FULL` fail-red pattern.
  8. Leave the golden table as a single obvious placeholder block:
     `static const EffectorGolden kEffectorGoldens[] = { /* W0.4.S2: paste dump output here */ };`
     and make the assertion loop `GTEST_SKIP() << "golden table empty — run MILO_TEST_DUMP_POSE_GOLDEN"`
     when the array is empty, so S1's commit is green-or-skip (never a spurious red) yet S2's job is
     unambiguous.
  9. Commit (engine repo, under `flock /tmp/milo-engine-git.lock`; message `W0.4: ` prefix; stage
     only `tests/test_bone_ground_truth.cpp`). This is a single **CHANGES** commit (adds a test).
- **verification:**
  - `cmake --build milo-native-engine/build-agent-W0.4 --target milo-engine-tests -j8` compiles clean.
  - `ctest --test-dir milo-native-engine/build-agent-W0.4 -R 'ClipPoseFixture.EffectorWorldPositionsMatchGolden' -V`
    → SKIPs cleanly (empty golden) — NOT red.
  - Dump run prints a non-empty initializer block:
    `MILO_TEST_DUMP_POSE_GOLDEN=1 <build>/tests/milo-engine-tests --gtest_filter='ClipPoseFixture.EffectorWorldPositionsMatchGolden'`
  - Append `## W0.4.S1 — done` to STATUS.md (commit SHA, chosen clip name, effector names used,
    whether a prop bone was found) under `flock /tmp/rb3-docs.lock`.

### W0.4.S2 — Capture, commit goldens, and demonstrate fail-red
- **model:** sonnet
- **goal:** Fill `kEffectorGoldens[]` from a real dump run, turn the test green, and prove it fails
  red under `MILO_TEST_POSE_PERTURB`. Record both outcomes in STATUS.
- **files to touch:** `milo-native-engine/tests/test_bone_ground_truth.cpp` ONLY (the golden array).
- **approach:**
  1. `git log --grep=W0.4` + read STATUS.md to confirm S1 landed; read the current
     `test_bone_ground_truth.cpp` golden block and dump/perturb hooks.
  2. Build `milo-engine-tests` standalone (Build section). Run the dump:
     `MILO_TEST_DUMP_POSE_GOLDEN=1 <build>/tests/milo-engine-tests --gtest_filter='ClipPoseFixture.EffectorWorldPositionsMatchGolden'`
  3. Paste the printed initializer lines verbatim into `kEffectorGoldens[]` (keep the emitted float
     precision). Rebuild.
  4. **Green run:** `ctest --test-dir <build> -R EffectorWorldPositionsMatchGolden -V` → PASS.
  5. **Fail-red demo:** re-run with `MILO_TEST_POSE_PERTURB=1.0` → the test must FAIL (1.0u ≫ 0.05
     epsilon). Capture the failing assertion text.
  6. Commit the golden values (engine repo, `flock /tmp/milo-engine-git.lock`, `W0.4: ` prefix, stage
     only the one file). Single **CHANGES** commit.
  7. Append `## W0.4.S2 — done` to STATUS.md with: dump values committed (SHA), green ctest line, and
     the pasted fail-red assertion output (the measurable exit-gate evidence).
- **verification:** green ctest with populated goldens; red ctest under `MILO_TEST_POSE_PERTURB=1.0`;
  both transcripts in STATUS.md.

---

## Build (standalone engine, your own dir — do this in both subtasks)

The engine test is NOT reachable from an rb3 build; configure a standalone engine build with tests on,
pointed at the dc3-decomp runtime + assets (same config `build-tests` already proves works):

```bash
cmake -S /home/free/code/milohax/milo-native-engine \
      -B /home/free/code/milohax/milo-native-engine/build-agent-W0.4 \
      -DMILO_ENGINE_BUILD_TESTS=ON \
      -DDC3_RUNTIME_ROOT=/home/free/code/milohax/dc3-decomp
cmake --build /home/free/code/milohax/milo-native-engine/build-agent-W0.4 --target milo-engine-tests -j8
ctest --test-dir /home/free/code/milohax/milo-native-engine/build-agent-W0.4 \
      -R 'ClipPoseFixture' -V
```

Reuse `build-agent-W0.4` on resume; never touch `build-tests`, `build-dc3ref`, `native/build-native`,
or `native/build-web*`. If the first configure is slow, that is expected (full engine + dc3 runtime).

---

## Exit criteria (measurable)

1. New `TEST_F(ClipPoseFixture, EffectorWorldPositionsMatchGolden)` exists in
   `test_bone_ground_truth.cpp`, compiles, and PASSES green with the committed `kEffectorGoldens[]`
   populated (hand + foot effectors at ≥3 pinned beats; prop bone included iff reachable, else
   documented as omitted).
2. **Fail-red demonstrated:** running the same test with `MILO_TEST_POSE_PERTURB=1.0` produces a RED
   failure whose assertion text is recorded in STATUS.md. (This is the phase exit-gate requirement
   that every net can fail red on a real placement error.)
3. The clip is pinned **by name** and beats by **fraction of clip range** — no dependence on
   `ObjDirItr` order or absolute asset beats.
4. `tests/CMakeLists.txt` is UNCHANGED (no collision with W0.1); only
   `tests/test_bone_ground_truth.cpp` is modified across both commits.
5. All pre-existing `BoneGroundTruth.*` / `ClipPoseFixture.*` tests still pass (no regression to the
   topology/symmetry/finiteness suite).

---

## Risks / conflicts

- **W0.1 (skin per-vertex golden)** runs in parallel and ALSO edits `milo-native-engine/tests/` — it
  adds a NEW file `test_skin_golden.cpp` and a CMakeLists line. **Mitigation:** W0.4 touches ONLY
  `test_bone_ground_truth.cpp` and NOT `tests/CMakeLists.txt` (already registered). Zero shared file.
  If, and only if, an unforeseen need to edit CMakeLists arises, re-read it immediately before editing
  and add a single line adjacent to line 88 — but the plan is designed so this never happens.
- **Golden fragility across compilers.** The test runs in exactly one build config, so exact FP
  determinism holds; the 0.05 epsilon absorbs Phase-1 "byte-identical" refactor FP-reordering while
  keeping tens-of-units margin over any real placement bug. Do not use `EXPECT_FLOAT_EQ` (too brittle)
  nor a loose epsilon >0.25 (would mask sub-unit regressions).
- **Clip-selection drift.** Reusing `sClip` (first-by-iteration) would make the golden silently
  re-baseline if the asset's object order changes. Pinning by name (S1 step 2/3) eliminates this.
- **Prop-bone scope creep.** Do not load additional prop/instrument milos or edit CMakeLists to reach
  a drum-stick bone; the brief scopes it "if reachable". Probe the already-loaded dirs only.
- **Other Wave-1 items** (W0.2 loud stubs, W0.3 draw-log golden, W0.5 lineup gate, W0.6 flag registry,
  W1.1 WGSL) touch engine `src/`/rb3 `native/src`/`Rnd_Wgpu_RB3.cpp`, not engine `tests/` — no overlap
  with W0.4's single file.
