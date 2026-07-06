# W2-TESTFIX — Status log

Append-only. One `## <subtask-id> — done|partial|blocked` section per subtask, under
`flock /tmp/rb3-docs.lock`, with commit SHAs (dc3-decomp / milo-native-engine), the fail→pass
evidence, and the no-regression `ctest` diff. Implementers read this + `git log --grep=W2-TESTFIX`
(both repos) and skip already-done work.

Objective: drive `milo-engine-tests` from the Wave-1 baseline (200 → 169 passed / 29 failed / 2
skipped) to **0 failed**. Four buckets, root-caused in PLAN.md:
(a) XMA sidecar namespace [dc3, S1], (b) CharBones hardened-vector bounds [dc3, S2],
(c) compressed-skin test oracle [engine, S3], (d) Dir-merge faithful resolution [engine/dc3, S4],
(e) full-suite green gate [S5].

<!-- sections appended by implementers below -->

## W2-TESTFIX.S1 — done

**Commit:** dc3-decomp `034e8d12` — "W2-TESTFIX: hoist NativeGetDataDir extern out of namespace dc3_xma" (only file: `native/src/platform/XmaPcmSidecar.h`).

**Fix:** moved `extern const char *NativeGetDataDir();` from inside `SidecarDir()` (lexically inside `namespace dc3_xma`, causing unqualified lookup to declare `dc3_xma::NativeGetDataDir()`) to file/global scope just above `namespace dc3_xma {` (wrapped in the same `#ifndef __EMSCRIPTEN__` guard as its only use site, matching the pre-existing native-only usage). Deleted the in-function extern; call site is now an ordinary unqualified call resolving to the global `::NativeGetDataDir()` defined in `File_Native.cpp:27`.

**Build:** configured `milo-native-engine/build-agent-W2-TESTFIX` from the W0.1 cache-var recipe in PLAN.md; `cmake --build build-agent-W2-TESTFIX --target milo-engine-tests -j8` succeeded clean.

**Evidence:**
- `nm -C tests/milo-engine-tests | grep -c 'dc3_xma.*NativeGetDataDir'` → **0** (was previously undefined at link). `nm` confirms `NativeGetDataDir()` is now a defined global text symbol (`T`).
- Ran the 16 bucket-(a) tests via `--gtest_filter` (DC3_DATA/MILO_LIB set per PLAN.md): **all 16 PASSED** (`AssetLoadingTest.{LoadSFXAssets,LoadWorldAssets,LoadStandaloneMiloFiles,LoadWorldMasterFile,LoadFullVenueWorlds,LoadDirectorSubdir,LoadSharedWorldSubdirs,CharacterVoiceBanksResolveRuntimeSoundScope,BetterOffAloneMoveGraphCopyPreservesLayoutResolution,BetterOffAloneAuthoredSongAnimsContainDanceMoves,BulkLoadAllFiles,BulkLoad_World,BulkLoad_UI,BulkLoad_SFX,BulkLoad_Songs,ChooseModeSubdirLoading}`).
- Full `ctest -j1` (same env): **13 failed out of 200** (was 29 in the Wave-1 baseline; -16 exactly). Failing-test list matches buckets (b)+(c)+(d) exactly, no new failures introduced:
  - `MeshVertexLoading.CompressedSkinningMatchesCpuSkinningForSyntheticBones` (bucket c)
  - `AssetLoadingTest.BoneServoCarriesAndAppliesForeArmRotZChannels`, `AssetLoadingTest.CpuSkinForearmVertexFromCompressedMesh`, all 9 `ClipPoseFixture.*` (bucket b, 11 total)
  - `ObjectLifetimeTest.MergeDirsMoveAllSubdirsTransfersOwnership` (bucket d)

**Deviations from PLAN.md:** none. Build dir `build-agent-W2-TESTFIX` left in place for S2-S5 to reuse (per PLAN.md shared-build note).

**Remains/blockers:** none for S1. Buckets (b), (c), (d) and the final S5 gate are separate subtasks.

## W2-TESTFIX.S2 — done

**Commit:** dc3-decomp `a14f7e01` — "W2-TESTFIX: CharBones hardened-vector bounds fix in Blend/ScaleAdd/RotateBy/RotateTo" (only file: `src/system/char/CharBones.cpp`).

**Fix:** applied the exact shipped `ScaleDown` (line 402, comment 404-409) transform to the four sibling functions that still formed one-past-the-end loop-bound pointers via raw `operator[]`: `Blend` (591), `ScaleAdd` (710), `RotateBy` (1000), `RotateTo` (1202). Mechanically replaced `&mBones[idx]` -> `mBones.data() + idx` and `&bones.mBones[idx]` -> `bones.mBones.data() + idx` at all 40 sites listed in PLAN.md's grep site list (10 per function x 4 functions). Left the `HX_NATIVE`-only `DC3_IK_DIAG` diagnostic print's `&mBones[0]` (line 771, inside `ScaleAdd`) untouched per PLAN.md guidance — index 0 is always in-bounds given the outer `!mBones.empty()` guard, so it never trips the hardened assertion; not part of the required transform.

**Confirmed pointer-formation-only (not a real deref OOB):** every one-past-end pointer formed at the listed sites (`otherBonesEnd`/`myBonesEnd`) is used exclusively as a loop-bound in a `>=`/`==` comparison (`if (otherBonesItr >= otherBonesEnd) { TestDstComplain(...); return; }` and `if (myBonesItr == myBonesEnd) { break/return; }`); no site dereferences the end pointer itself. This matches the already-shipped `ScaleDown` precedent's reasoning exactly.

**Build:** reused shared `milo-native-engine/build-agent-W2-TESTFIX` (from S1); `cmake --build build-agent-W2-TESTFIX --target milo-engine-tests -j8` succeeded clean (only `CharBones.cpp.o` recompiled + link).

**Evidence:**
- Targeted filter (DC3_DATA/MILO_LIB set per PLAN.md): `--gtest_filter='ClipPoseFixture.*:AssetLoadingTest.BoneServoCarriesAndAppliesForeArmRotZChannels:AssetLoadingTest.CpuSkinForearmVertexFromCompressedMesh'` -> **14/14 PASSED** (2 `AssetLoadingTest` + all 12 `ClipPoseFixture.*` — the fixture has 12 tests total in the current tree, of which the 9 named in PLAN.md were the previously-aborting subset; all pass, no abort, no crash). `EffectorWorldPositionsMatchGolden` (the W0.4 bone live-pose safety net) is among them and passes clean.
- Full `ctest -j1` (same env): **failed count 13 -> 2** (exactly -11, matching bucket (b)'s size). Remaining 2 failures are exactly bucket (c) (`MeshVertexLoading.CompressedSkinningMatchesCpuSkinningForSyntheticBones`) and bucket (d) (`ObjectLifetimeTest.MergeDirsMoveAllSubdirsTransfersOwnership`) — separate subtasks S3/S4, not touched here. No previously-passing test regressed (200 total, 2 skipped-by-design unchanged: `ExtractBik.ExtractSmallest`, `SkinGolden.CaptureGolden`).
- **Match-neutrality (evidence item 4):** not independently objdiff-spot-checked (dc3 objdiff tooling for this file wasn't readily wired up in this session); relying on the shipped `ScaleDown` precedent + the green suite, per PLAN.md's stated fallback ("if not, the precedent + green suite is sufficient — note which").

**Deviations from PLAN.md:** none. `ClipPoseFixture` currently has 12 tests (not the 9 implied by the bucket-(b) test name list) — all pass; this is a superset, not a discrepancy against the fix.

**Remains/blockers:** none for S2. Buckets (c), (d) and the final S5 gate are separate subtasks (S3/S4/S5).

## W2-TESTFIX.S3 — done

**Commit:** milo-native-engine `49c3f38` ("W2-TESTFIX: fix compressed-skin test oracle truncating float positions"), own file only (`tests/test_mesh_loading.cpp`).

**Confirm-before-apply (per PLAN S3):** built + ran the single failing test first. Measured per-axis deltas BEFORE fix: x=0.257, y=0.505, z=3.001 — all ≫ 0.02 tol; gpuSkinned values consistently smaller than cpuSkinned (2.49<2.75, 14.995<15.5, 3.75<6.75) = origin-collapse signature, exactly matching the diagnosed `(uint32_t)cv.mPosX` truncation (`(uint32_t)1.0f==1` → decode reads denormal ≈0). Diagnosis confirmed; NOT a real decode regression, so VertexFormats.cpp untouched.

**Fix:** in `SerializeCompressedVertexBE`, changed the three position writes `PutBE32((uint32_t)cv.mPosX/Y/Z)` → `PutBEFloat(buf, cv.mPosX/Y/Z)` (bit-reinterpret). Six genuinely-integer fields (mColor/mNormal/mTangent/mBinormal/mBoneIndices/mBoneWeights) left as PutBE32. Tolerance (0.02f) untouched.

**Evidence AFTER fix:** `MeshVertexLoading.*` = 5/5 PASSED, including the previously-failing `CompressedSkinningMatchesCpuSkinningForSyntheticBones`. No sibling test regressed (`SerializeCompressedVertexBE` has one caller). Build: `build-agent-W2-TESTFIX`.

**Remains/blockers:** none for S3. Bucket (d) = S4, final full-suite gate = S5.

## W2-TESTFIX.S4 — done

**Resolution chosen: d-i (correct the non-faithful test oracle in the engine).**

**Commit:** milo-native-engine `0dab386` — "W2-TESTFIX: correct non-faithful Dir-merge subdir-ownership oracle" (only file: `tests/test_object_lifetime.cpp`).

**Observed failure (evidence item 5):** built + ran the single test first. The ONLY failing assertion was `EXPECT_FALSE(fromDir->HasSubDir(movedSubdir))` at `test_object_lifetime.cpp:426` (`Actual: true`, `Expected: false`). `EXPECT_TRUE(toDir->HasSubDir(movedSubdir))` at :425 PASSED. Crucially, the subsequent `delete fromDir; ...; delete toDir;` ran to completion with **no crash, no ASan/abort, no double-free** — gtest reported the failed assertion at 0ms and the process exited cleanly (exit 1 = assertion failure only). So the shared subdir already survives source teardown correctly (dc3 `~ObjectDir` survivor-closure logic handles it); there is **no genuine dc3 `Dir.cpp` teardown bug** → d-ii is not needed.

**Faithful-reference reconciliation:** the authoritative RB3 decomp `rb3/src/system/obj/Utl.cpp:247` `MergeObjectsRecurse`, `case MergeFilter::kReplace`: `if (!toDir->HasSubDir(fromDir)) toDir->AppendSubDir(ObjDirPtr<ObjectDir>(fromDir)); return;` — APPENDS to the destination and does **not** erase from the source. dc3's `src/system/obj/Utl.cpp` `MergeObjectsRecurse` (case `1`) is identical after commit `1c757f63` removed the pre-port source-erase. The engine test's `EXPECT_FALSE(fromDir->HasSubDir(...))` encoded the non-faithful "move erases source" expectation inherited from that pre-port code.

**Fix (oracle correction, NOT weakening):** kept `EXPECT_TRUE(toDir->HasSubDir(movedSubdir))`; replaced `EXPECT_FALSE(fromDir->HasSubDir(movedSubdir))` with the faithful `EXPECT_TRUE(fromDir->HasSubDir(movedSubdir))` (source still references it — no erase); and strengthened the weak `EXPECT_NE(movedSubdir, nullptr)` post-`delete fromDir` into a real ownership-survival check `EXPECT_TRUE(toDir->HasSubDir(movedSubdir))` (destination keeps the shared subdir alive & reachable across source teardown). Corrected the misleading leading comment and added a citation to `rb3/src/system/obj/Utl.cpp:247` plus a "do NOT re-add a source-erase to MergeObjectsRecurse" warning. **`MergeObjectsRecurse` (rb3 + dc3) untouched — merge stays byte-faithful to the RB3 target.**

**Evidence (fail→pass + no regression):**
- Targeted: `--gtest_filter=ObjectLifetimeTest.MergeDirsMoveAllSubdirsTransfersOwnership` → **PASSED** (was FAILED), clean teardown.
- Full `ctest -j1` (DC3_DATA/MILO_LIB set, shared `build-agent-W2-TESTFIX`): **100% tests passed, 0 tests failed out of 200** — the bucket-(d) residual (last of the 4 buckets after S1-S3) is now cleared; no previously-passing test regressed.

**Deviations from PLAN.md:** none.

**Remains/blockers:** none for S4. All 4 buckets green; S5 (full-suite green-gate + wave-gate wiring note) can now confirm & document.

## W2-TESTFIX.S5 — done

**Full-suite green gate confirmed.** Rebuilt `milo-engine-tests` (shared `milo-native-engine/build-agent-W2-TESTFIX`, serialized under `flock /tmp/milo-engine-build.lock`) against the committed S1-S4 fixes, then ran the full suite:

```
DC3_DATA=/home/free/code/milohax/dc3-decomp/orig-assets \
MILO_LIB=/home/free/code/milohax/dc3-decomp/orig-assets/extracted \
ctest -j1 --output-on-failure
```

**Result:** `100% tests passed, 0 tests failed out of 200` (Total Test time 131.19 sec).
- **198 passed, 0 failed, 2 skipped-by-design** — exactly the exit criteria.
- The 2 skips are the expected by-design ones (unchanged from Wave-1 baseline), NOT residual failures:
  - `193 - ExtractBik.ExtractSmallest (Skipped)`
  - `194 - SkinGolden.CaptureGolden (Skipped)`
- Failed-test list is **empty**. No previously-passing test regressed.

**Provenance verified before gating:** all four fixes present and staged narrowly, one file each —
- dc3-decomp `034e8d12` (S1, `native/src/platform/XmaPcmSidecar.h`), `a14f7e01` (S2, `src/system/char/CharBones.cpp`)
- milo-native-engine `49c3f38` (S3, `tests/test_mesh_loading.cpp`), `0dab386` (S4, `tests/test_object_lifetime.cpp`)
- `git status --short` on all four touched paths is clean in both repos (nothing built from an uncommitted tree).

**Net delta vs Wave-1 baseline:** 200 → **169 passed / 29 failed / 2 skipped** (Wave-1) → **198 passed / 0 failed / 2 skipped** (now). All 29 pre-existing failures cleared across the 4 buckets (a:16, b:11, c:1, d:1).

**New env flags:** none. Scanned all four commit diffs (`git show` on the added `+` lines) for `getenv`/`RB3_`/`DC3_`/`MILO_` flag introductions — none found. The only `DC3_IK_DIAG` reference is in the S2 commit message describing the pre-existing diagnostic flag left untouched. So **no `NativeCompatFlags` registration and no ledger regeneration required** (matches PLAN.md "none expected").

**Residual documented skips:** none needed. No `DISABLED_`/`GTEST_SKIP()` added; no tolerance changed. **No S5 commit required** (nothing to document via a skip).

### Gate-wiring note (for the coordinator / future waves)

`milo-engine-tests` is now a usable convergence gate. To invoke it:

```bash
cd /home/free/code/milohax/milo-native-engine
cmake --build build-agent-<KEY> --target milo-engine-tests -j8      # own build dir; serialize shared dirs under flock
cd build-agent-<KEY>
DC3_DATA=/home/free/code/milohax/dc3-decomp/orig-assets \
MILO_LIB=/home/free/code/milohax/dc3-decomp/orig-assets/extracted \
ctest -j1 --output-on-failure
```

- **Both asset env vars (`DC3_DATA` + `MILO_LIB`) are REQUIRED** — without them the AssetLoadingTest / ClipPoseFixture / MoggDecode / MiloFiles families skip (or run degraded), so a "green" run without assets is not a real gate pass. With them set, the pass bar is **198 passed / 0 failed / 2 skipped-by-design**.
- Run **`ctest -j1`** (serial) — the suite loads shared assets and shares a build dir; `-j>1` races the shared engine build/asset state.
- The coordinator's integration-worktree CI already invokes this suite; from this item onward, **0 failed / 198 passed / 2 skipped** is the wave-close bar. Any drop below 198 passed (excluding the 2 by-design skips) is a regression to bisect, not a new baseline to accept.
- Wave-1 safety-net gtests are part of this count and green: `SkinGolden.*` (skin golden), `ClipPoseFixture.EffectorWorldPositionsMatchGolden` (bone live-pose), stub-census tests.

**Deviations from PLAN.md:** none. **Remains/blockers:** none — all 4 buckets green, gate documented. W2-TESTFIX complete.
