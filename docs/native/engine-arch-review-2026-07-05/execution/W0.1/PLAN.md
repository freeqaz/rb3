# W0.1 — CPU reference skinner + per-vertex golden gtest

**Phase 0 safety net (highest leverage).** REFACTOR_PLAN §Phase 0 · lane `02-mesh-skinning.md`
rec 5a + Q5 · ARCHITECTURE_REVIEW SYS-7 (gate blindness) / SYS-1 (skin transform/bind).
Planner: Opus, 2026-07-05. **This item writes an engine test only — it changes NO rendering
output.** It is the oracle the twice-reverted BandPatchMesh rewrites never had.

## Objective

Add `milo-native-engine/tests/test_skin_golden.cpp`: a **standalone LP64 CPU reference skinner**
that reproduces the decomp skinning semantics, run in gtest against a **real posed character** so
that the final 4-bone-blended **world** positions of a fixed vertex set — which **must include
hand/finger-region verts** — are asserted against a **committed golden** within epsilon. The test
must be able to **fail red** on a deliberately-broken skin.

### Faithful reference (decomp ground truth — cite these in the file header)

`rb3/src/system/rndobj/Mesh.cpp:1368-1410` — `RndMesh::SkinVertex` (comment `// matches in retail`):

```
Vector3 ret(0,0,0);
if (NumBones() > 0) {
  Transform tf60; tf60.Zero(); bool any=false;
  for (i=0..3) {
    int boneIdx = vert.boneIndices[i];              // short boneIndices[4], Mesh.h:86
    if (boneIdx < NumBones()) {                      // (a) SKIP out-of-range, do NOT clamp
      RndTransformable* bt = BoneTransAt(boneIdx);   // Mesh.h:256  == mBones[idx].mBone
      unsigned short w16 = vert.boneWeights[i];      // Vector4_16_01::operator[], Vec.h:214
      if (w16 && bt) {
        Transform tf90;
        Multiply(BoneOffsetAt(boneIdx), bt->WorldXfm(), tf90);   // skinMat = invBind · boneWorld
        ScaleAddEq(tf60, tf90, vert.boneWeights.FloatAt(i));     // (b) NO normalization; += w·skinMat
        any=true;
      }
    }
  }
  if (any) Multiply(vert.pos, tf60, ret);            // row-vector: pos · tf60
}
return ret;
```

Faithful invariants to preserve exactly: **(a)** `boneIdx < NumBones()` skip (not clamp); **(b)** no
weight normalization — `tf60` is the raw weighted sum; **(c)** `skinMat = BoneOffsetAt(i) ·
boneWorld_i` (invBind on the left, Milo row-vector convention). Weights are `Vector4_16_01` "hate
format" u16 in [0,1]; the correct float is `FloatAt(i)`/`GetX()` (`Vec.h:179-217`), NOT the raw `.x`.

Supporting facts (grep-verified 2026-07-05):
- Bone accessors: `NumBones()` `Mesh.h:240`, `BoneTransAt` `:256`, `BoneOffsetAt` `:257`,
  `IsSkinned()` `:251`, `Verts()` `:247`, `MAX_BONES 40` `:17`.
- Math helpers all exist and are used by `SkinVertex`: `Multiply(const Transform&,const Transform&,
  Transform&)` `Mtx.h:412`, `Multiply(const Vector3&,const Transform&,Vector3&)` (row-vector) `:415`,
  `ScaleAddEq(Transform&,const Transform&,float)` `:603`, `Transform::Zero()`.
- **`RndMesh::SkinVertex` is compiled in the native/test build** — the `#endif // !HX_NATIVE` at
  `Mesh.cpp:1365` closes BEFORE `SkinVertex` at `:1367`. So the test can call `mesh->SkinVertex(v,
  nullptr)` directly as a *second* internal oracle (see cross-check assertion below).
- Skinned-mesh enumeration pattern already in-tree: `rb3/native/src/rb3_http_handlers.cpp:44`
  (`ObjDirItr<RndMesh>` + `NumBones()`/`Verts()`); iterator include `obj/Dir.h`.

### Correction to the brief / REFACTOR_PLAN exit gate wording

REFACTOR_PLAN Phase-0 exit gate 1 references `RB3_SKEL_REBIND_FULL=1` as the fail-red trigger.
**That env flag does NOT exist in the engine today** (`grep -rn "SKEL_REBIND_FULL"
milo-native-engine/src` → empty; only `RB3_NO_SKEL_REBAKE`/`RB3_NO_SKEL_WORLDFIX` exist, and neither
forces a full-body rebind). It is a Phase-2/W2.2 artifact. **W0.1 must NOT depend on it.** The
fail-red is instead implemented **self-contained inside the test** by corrupting the invBind
rotation basis (see S1 step 5) — cleaner, and it exercises exactly the H2 "rotation-basis mismatch →
R·sinθ fling" failure mode the lane doc measured.

## Subtasks

### W0.1.S1 — Reference skinner + golden test + capture the golden  (model: opus)

**Goal:** deliver a building, green `test_skin_golden.cpp` with committed golden numbers captured
from the real posed asset, plus a self-contained fail-red path and two internal oracles.

**Files to touch (engine repo `milo-native-engine`, its own git):**
- NEW `tests/test_skin_golden.cpp`
- EDIT `tests/CMakeLists.txt` — append the single line `test_skin_golden.cpp` to the
  `MILO_ENGINE_TEST_SOURCES` list (ends at `:94`, right after `test_extract_bik.cpp`). One line, no
  reorder — this list is shared (see Risks).

**Build/run environment (this item builds the ENGINE test suite, NOT rb3-native):**
```
cmake -B /home/free/code/milohax/milo-native-engine/build-agent-W0.1 \
      -S /home/free/code/milohax/milo-native-engine \
      -DDC3_RUNTIME_ROOT=/home/free/code/milohax/dc3-decomp
cmake --build /home/free/code/milohax/milo-native-engine/build-agent-W0.1 \
      --target milo-engine-tests -j8
```
`MILO_ENGINE_BUILD_TESTS` auto-ONs because the configure is standalone (source==current, CMakeLists
`:33-34,141`). Use build-agent-W0.1 **only**; never touch the shared `build-tests`/`build-dc3ref`,
and never `native/build-native`/`build-web*`. Test assets come from `DC3_DATA`/`MILO_LIB` which ctest
sets from the cache (`MILO_TEST_DATA_DIR=.../dc3-decomp/orig-assets`,
`MILO_TEST_MILO_LIB=.../orig-assets/extracted`).

**Step-by-step:**
1. **Configure + build** the target above. Confirm the pre-existing suite builds before adding code.
2. **Scaffold the fixture** by *copying* (not editing) the asset-loading pattern from
   `tests/test_bone_ground_truth.cpp`: `TryLoadMilo()` (`:33-51`), the `BoneGroundTruth`
   char-dir fixture (`:71-107`), and the `ClipPoseFixture` clip loader (`:262-343`). Load a real
   char dir + a dance clip, then `sClip->PoseMeshes(sDir, beat)` at a **fixed beat** (use
   `sClip->StartBeat()` for determinism — see `PoseDeterminism` in the bone test). GTEST_SKIP
   cleanly if assets/clip are absent (same as the bone test), so the suite still runs without assets.
   - Reuse the exact candidate asset lists the bone test already proves loadable
     (`char/main/gen/main.milo_xbox`, `char/crowd/gen/crowd_f_*`, clip
     `char/crowd/anim/gen/female_base.milo_xbox`, `MILO_TEST_CHAR`/`MILO_TEST_CLIPS` overrides).
3. **Standalone reference skinner** — implement in the test TU, mirroring Mesh.cpp:1368-1410 exactly:
   ```
   static Vector3 RefSkinVertex(RndMesh* m, const RndMesh::Vert& v, bool breakBasis);
   ```
   Do NOT normalize weights; DO skip `boneIdx >= NumBones()`; compose `Multiply(BoneOffsetAt(idx),
   BoneTransAt(idx)->WorldXfm(), tf90)`; `ScaleAddEq(tf60, tf90, v.boneWeights.FloatAt(i))`;
   `Multiply(v.pos, tf60, ret)`. This function is the oracle — it must not call `mesh->SkinVertex`.
4. **Vertex selection (deterministic, MUST include hand/finger verts).** After posing, iterate
   `ObjDirItr<RndMesh> it(sDir, true)` (recurse) collecting `IsSkinned()` meshes. Choose the vertex
   set by a **stable rule** and record `(meshName, vertIndex)` for each:
   - Prefer a mesh whose `Name()` matches hand/finger geometry (case-insensitive substring:
     `"hand"`, `"finger"`, `"L-hand"`, `"R-hand"`, `"digit"`). This satisfies the hard requirement.
     If none matches, fall back to the mesh with the most bones (a body/outfit mesh) and log a WARN
     that no hand-named mesh was found (S1 must resolve this before committing — the brief requires
     hand/finger coverage; probe multiple assets: crowd chars carry full appendage meshes).
   - Also include one torso/body mesh's verts for contrast.
   - Within a chosen mesh, select the first K (K≈8) verts, ascending index, that are genuinely
     skinned (≥1 nonzero weight AND its `boneIndices[i] < NumBones()` AND `BoneTransAt` non-null)
     **and** whose `|pos|` (bone-local radius) is large enough that a basis error visibly displaces
     them (skip verts at the bone origin) — this makes the fail-red robust.
   - Commit the full selection list as a `static const` table in the file (see step 6). Selection is
     part of the golden: never re-derive it dynamically at assert time.
5. **Fail-red hook (self-contained).** `breakBasis==true` replaces the invBind used in the blend with
   a **fixed 90°-about-Z rotation of the original** `BoneOffsetAt(idx)` (rotate its 3x3 `m`; keep
   `v`). A 90° angular error guarantees a fling ≈ √2·R for a vertex at bone-local radius R — robust
   regardless of asset. Two ways to trip it, provide BOTH:
   - env `MILO_SKIN_GOLDEN_BREAK` set → the golden test itself uses the broken skinner → the
     `EXPECT_NEAR` asserts **go RED** (this is the human/CI-runnable red demonstration for exit gate).
   - a permanent **positive-control** test `SkinGolden.BrokenSkinDivergesFromGolden` that always runs
     the broken skinner and `EXPECT_GT(maxComponentDiffVsGolden, 5.0f)` for at least one hand/finger
     vert — proves the oracle stays sensitive, and stays GREEN in normal CI.
6. **Capture the golden.** Add a capture mode: if `getenv("MILO_SKIN_GOLDEN_CAPTURE")`, print each
   selected `(meshName, vertIndex, x, y, z)` from `RefSkinVertex(normal)` in paste-ready C-array
   form and don't assert. Run:
   ```
   DC3_DATA=/home/free/code/milohax/dc3-decomp/orig-assets \
   MILO_LIB=/home/free/code/milohax/dc3-decomp/orig-assets/extracted \
   MILO_SKIN_GOLDEN_CAPTURE=1 \
   build-agent-W0.1/tests/milo-engine-tests --gtest_filter='SkinGolden*'
   ```
   Paste the printed rows into a `static const GoldenVert kGolden[]` table. **Header comment must
   record**: asset dir path, clip name, pose beat, engine HEAD SHA, capture date, and the epsilon
   rationale. Run capture **twice** and confirm byte-identical output (determinism) before committing.
7. **Assertions (normal mode)** in the golden test:
   - (a) golden: for each `kGolden[]` entry, recompute `RefSkinVertex(mesh, vert, false)` and
     `EXPECT_NEAR` each component within `kEpsAbs = 5e-2f` (positions are order 10–100u; 5e-2 absorbs
     cross-machine FP noise yet catches multi-unit flings — document this).
   - (b) internal oracle cross-check: `EXPECT_NEAR(RefSkinVertex(mesh,vert,false)[c],
     mesh->SkinVertex(vert,nullptr)[c], 1e-4f)` — proves the standalone reference is faithful to the
     compiled decomp `SkinVertex` (guards silent drift of either).
   - Resolve the mesh by name from `kGolden[]` at assert time (find the same posed mesh), so the
     table is self-describing.
8. **Commit** to the engine repo under `flock /tmp/milo-engine-git.lock`, message prefix `W0.1:`,
   staging ONLY `tests/test_skin_golden.cpp` + `tests/CMakeLists.txt`. Then append STATUS.md
   (`## W0.1.S1 — done`, SHA, asset/clip/beat used, golden count, hand-mesh name) under
   `flock /tmp/rb3-docs.lock`.

**Verification (S1 done when all true):**
```
# build
cmake --build /home/free/code/milohax/milo-native-engine/build-agent-W0.1 --target milo-engine-tests -j8
# golden green (assets present)
DC3_DATA=.../orig-assets MILO_LIB=.../orig-assets/extracted \
  build-agent-W0.1/tests/milo-engine-tests --gtest_filter='SkinGolden*'   # -> PASS
# determinism: capture twice identical (step 6)
```
- SkinGolden golden + cross-check + positive-control tests PASS.
- At least one asserted vertex is from a hand/finger-named mesh (printed in test output).

### W0.1.S2 — Fail-red demonstration + full-suite green + gate wiring note  (model: sonnet)

**Goal:** prove the golden can fail red, prove adding the TU didn't regress the suite, and record the
evidence the coordinator needs to wire it into the integration gate. Verification-only unless a small
epsilon/selection fix in S1's file is needed (still under W0.1).

**Files to touch:** none expected (STATUS.md append only; if a fix is required, patch
`tests/test_skin_golden.cpp` and re-commit under W0.1).

**Step-by-step:**
1. Reuse `build-agent-W0.1` (rebuild the target if the tree changed).
2. **Fail-red (exit gate 1):** run the golden with the break env and confirm it goes RED:
   ```
   DC3_DATA=.../orig-assets MILO_LIB=.../orig-assets/extracted MILO_SKIN_GOLDEN_BREAK=1 \
     build-agent-W0.1/tests/milo-engine-tests --gtest_filter='SkinGolden.*Golden*'
   ```
   Expect a FAILED run with `EXPECT_NEAR` diffs of many units on the hand/finger verts. Paste the
   first few failing lines into STATUS.md as the fail-red evidence.
3. **Positive-control green:** confirm `SkinGolden.BrokenSkinDivergesFromGolden` PASSES in normal mode
   (no break env) — the sensitivity check is continuously enforced without leaving CI red.
4. **Full-suite no-regression:** run the whole engine suite serialized (RESOURCE_LOCK requires
   `-j1`) and confirm no previously-green test broke:
   ```
   ( cd /home/free/code/milohax/milo-native-engine/build-agent-W0.1 && ctest -j1 --output-on-failure )
   ```
   Record the pass/skip counts; confirm SkinGolden tests are among "passed" (not "skipped") under the
   committed `DC3_DATA`/`MILO_LIB` — the integration gate must actually exercise them.
5. **Gate wiring note:** in STATUS.md, state that the gate target is `milo-engine-tests` via `ctest`
   (already the engine convergence suite), so the coordinator's integration-worktree full-rebuild CI
   picks it up automatically; flag that assets must be present for it to run non-skipped.
6. Append `## W0.1.S2 — done` (fail-red evidence, suite counts) under `flock /tmp/rb3-docs.lock`.

**Verification (S2 done when all true):**
- `MILO_SKIN_GOLDEN_BREAK=1` run of the golden test is RED (captured in STATUS.md).
- Normal-mode `ctest -j1` all-green; SkinGolden tests reported passed, not skipped.

## Exit criteria (measurable — item complete when all hold)

1. `tests/test_skin_golden.cpp` exists, builds into `milo-engine-tests`, and its golden +
   internal-oracle cross-check + positive-control tests PASS on the current engine build with the
   committed `DC3_DATA`/`MILO_LIB` assets (not skipped).
2. **Fail-red proven:** running the golden with the invBind-basis corruption
   (`MILO_SKIN_GOLDEN_BREAK=1`) turns the test RED — evidence pasted in STATUS.md. (Self-contained;
   does NOT use the non-existent `RB3_SKEL_REBIND_FULL`.)
3. The asserted vertex set includes at least one hand/finger-region vertex (mesh name logged).
4. The standalone `RefSkinVertex` agrees with the compiled `RndMesh::SkinVertex` within 1e-4 on every
   selected vertex (proves the reference is faithful to the decomp).
5. Full engine suite still green under `ctest -j1`; no prior test regressed by the added TU.
6. One engine commit, prefix `W0.1:`, staging only the two test files; STATUS.md updated per subtask.

## Risks / conflicts

- **`tests/CMakeLists.txt` `MILO_ENGINE_TEST_SOURCES` list is shared with other Wave-1 items** —
  **W0.3** adds `test_draw_log_golden.cpp` to the same list. (W0.4 *edits* `test_bone_ground_truth.cpp`
  and does not add a list entry.) Mitigation: append exactly one line at the END of the list
  (`:94`), never reorder; commit the engine change under `flock /tmp/milo-engine-git.lock`. If a
  concurrent W0.3 commit conflicts, it is a trivial one-line list-append merge — re-append and rebuild.
- **Do NOT edit `test_bone_ground_truth.cpp`** — that file is W0.4's surface. W0.1 *copies* its
  loader patterns into the new TU; it must not modify the shared file. Likewise treat
  `test_helpers.{h,cpp}` and `EngineTestFixture` as read-only shared infra.
- **No rendering source is touched** (this is a test-only item), so there is zero collision with the
  renderer lanes (W1.1 WGSL, W0.3 draw-log which only adds a *new* test file + reads the ring). SYS-1
  skin path in `Rnd_Wgpu_RB3.cpp` is NOT modified here.
- **Never bump `MILO_ENGINE_PIN`** (currently `a8089c3…` in `rb3/native/CMakeLists.txt:74`) — the
  coordinator bumps once per wave after all Wave-1 engine commits land.
- **Asset availability:** the golden runs against dc3 `orig-assets` (the engine suite's asset root).
  If no hand/finger-named skinned mesh is found in the first asset, S1 must probe additional char
  assets (crowd_f/crowd_m carry full appendage meshes) before committing — the hand/finger coverage
  is a hard requirement, not optional. If assets are entirely absent the test GTEST_SKIPs (matching
  the other 16 gtests) but the coordinator's gate environment does have them.
- **Build-dir hygiene:** this item builds the *engine* test suite standalone
  (`milo-native-engine/build-agent-W0.1`), which is the correct analogue of the "use your own build
  dir" rule for an engine-test deliverable — NOT the rb3-native `native/build-agent-W0.1` path in the
  generic wave instructions. Do not touch shared `build-tests`/`build-dc3ref`.
