# W2-TESTFIX — Burn down the 4 pre-existing `milo-engine-tests` failure buckets

Planner: Opus. Status: PLAN complete, ready for implementation.
Parent: `../REFACTOR_PLAN.md`; wave protocol: `../README.md` (HARD RULES 1-8 binding).

## Objective

Wave 1's `milo-engine-tests` gate ran **200 tests → 169 passed, 29 failed, 2 skipped**
(full characterization: `../W0.1/STATUS.md`, section "Full-suite `ctest`"). All 29 failures
are **pre-existing dc3-decomp / engine-test drift**, none caused by Wave 1. This item drives
the suite to **green (0 failures, or per-test documented skip-with-reason)** so it can serve as
a wave gate. **Do not weaken assertions to pass; fix the root cause or explicitly skip with a
faithful-reference-cited reason.**

The 29 failures fall in exactly 4 buckets. Each has been root-caused against the current code
(re-grepped 2026-07-05; the W0.1 line numbers had already shifted). Fixes land in the repo that
owns the bug, commit prefix `W2-TESTFIX:`.

### Repos & where each bug lives
- **dc3-decomp** (`/home/free/code/milohax/dc3-decomp`, own repo, **concurrent owners** — check
  `git log`/`git status` before editing; flock `/tmp/dc3-git.lock`): buckets (a), (b), and
  possibly (d).
- **milo-native-engine** (`/home/free/code/milohax/milo-native-engine`, own repo; flock
  `/tmp/milo-engine-git.lock`): the failing test files live here (`tests/test_mesh_loading.cpp`,
  `tests/test_object_lifetime.cpp`); buckets (c) and possibly (d).
- **NEVER bump `MILO_ENGINE_PIN`** (rule 3 — coordinator does it once per wave).

## Root-cause findings (verified against current source)

### Bucket (a) — 16 tests: `undefined symbol: _ZN7dc3_xma16NativeGetDataDirEv`
`AssetLoadingTest.{LoadSFXAssets,LoadWorldAssets,LoadStandaloneMiloFiles,LoadWorldMasterFile,
LoadFullVenueWorlds,LoadDirectorSubdir,LoadSharedWorldSubdirs,CharacterVoiceBanksResolveRuntimeSoundScope,
BetterOffAloneMoveGraphCopyPreservesLayoutResolution,BetterOffAloneAuthoredSongAnimsContainDanceMoves,
BulkLoadAllFiles,BulkLoad_World,BulkLoad_UI,BulkLoad_SFX,BulkLoad_Songs,ChooseModeSubdirLoading}`.

**Root cause (confirmed):** `dc3-decomp/native/src/platform/XmaPcmSidecar.h` opens
`namespace dc3_xma {` at line **56**. Inside it, `inline std::string SidecarDir()` has a
**function-local `extern const char *NativeGetDataDir();` at line 96** — because it is lexically
inside `namespace dc3_xma`, this declares `dc3_xma::NativeGetDataDir()`
(mangled `_ZN7dc3_xma16NativeGetDataDirEv`). But the one real definition is **global**:
`native/src/platform/File_Native.cpp:27` `const char *NativeGetDataDir() {...}`
(`_Z16NativeGetDataDirv`), and `native/src/platform/System_Native.cpp:60` also forward-declares
it in the **global** namespace. Namespace mismatch → undefined symbol at first SFX/XMA sidecar
touch. Only these two files reference `NativeGetDataDir`; nothing depends on the `dc3_xma::` name.

**Fix (dc3-decomp):** hoist the `extern const char *NativeGetDataDir();` declaration **out of**
`namespace dc3_xma` to file/global scope (place it just *above* the `namespace dc3_xma {` at
line 56). Unqualified lookup inside `SidecarDir()` then climbs to global scope and resolves to
`::NativeGetDataDir()`. One-line move, header-only, no behavior change.

### Bucket (b) — 11 tests: `CharBones::ScaleAdd` hardened-vector bounds abort
`AssetLoadingTest.{BoneServoCarriesAndAppliesForeArmRotZChannels,CpuSkinForearmVertexFromCompressedMesh}`
+ all 9 `ClipPoseFixture.*`. Backtrace: `CharClip::PoseMeshes → CharBones::ScaleAdd →
std::vector<CharBones::Bone>::operator[]` abort.

**Root cause (confirmed):** `dc3-decomp/src/system/char/CharBones.cpp` `CharBones::ScaleAdd(CharBones&, float)`
(starts line **710**) forms **one-past-the-end** loop-bound pointers via `operator[]`, e.g.
`(Bone *)&mBones[mCounts[TYPE_QUAT]]`, `(Bone *)&bones.mBones[bones.mCounts[TYPE_QUAT]]`
(lines 717-941). When a type section is empty, `mCounts[...] == mBones.size()`, so `&vec[size()]`
takes the address of a non-existent element. Hardened libstdc++ (`_GLIBCXX_ASSERTIONS`, on in the
test build) aborts on the out-of-range `operator[]`; the shipped/target build (non-hardened) reads
through. These end-pointers are used purely as loop bounds — **never dereferenced**.

**This exact bug was already fixed in the sibling `CharBones::ScaleDown` (line 402)** using
`mBones.data() + index` instead of `&mBones[index]` — see the committed comment at CharBones.cpp
**line 404-409** ("data() + index is bounds-check-free and lowers to identical PPC (the original
matched form)"). `ScaleAdd` and its three peers were left unconverted.

**Fix (dc3-decomp):** apply the identical `ScaleDown` transform — replace every
`&mBones[<idx>]` → `mBones.data() + <idx>` and `&bones.mBones[<idx>]` →
`bones.mBones.data() + <idx>` — in **`ScaleAdd`** (required for the 11 tests) **and its three
peers that carry the same latent bug**: `Blend` (line 591, sites 594-679), `RotateBy`
(line 1000, sites 1002-1146), `RotateTo` (line 1202, sites 1204-1376). Do **not** touch the
`&mBones[0]` inside the `#ifdef HX_NATIVE` `DC3_IK_DIAG` diagnostic print at line 771 unless
it also trips (index 0 is in-bounds when `!mBones.empty()`, so it does not abort — leave it or
convert for consistency, implementer's call, but keep the diff minimal). Exact site list:
`grep -n '&mBones\[\|&bones\.mBones\[' src/system/char/CharBones.cpp` → lines
594,596,597,598,626,627,628,677,678,679 (Blend); 717,719,720,721,802,803,804,939,940,941 (ScaleAdd);
1002,1004,1005,1006,1061,1062,1063,1144,1145,1146 (RotateBy); 1204,1206,1207,1208,1263,1264,1265,
1374,1375,1376 (RotateTo). **Match-neutral:** identical codegen to `&vec[i]` on the non-hardened
target (the `ScaleDown` precedent already established and shipped this equivalence).

### Bucket (c) — 1 test: `MeshVertexLoading.CompressedSkinningMatchesCpuSkinningForSyntheticBones`
`tests/test_mesh_loading.cpp` (milo-native-engine). GPU compressed-skin vs raw CPU-skin diverges
> the 0.02 tol. Brief flagged this as a possible **real decode regression** — it is **not**;
it is a **test-helper bug**.

**Root cause (confirmed by static analysis + numeric model):**
- The engine's real decode path is **correct** — `src/gfx/VertexFormats.cpp`
  `UnpackCompressedSkinnedVertices` (fixed in engine commit `f75339a`, "compressed-vertex unpack
  truncated big-endian float positions": positions read via `LoadBE32` byte-by-byte + `memcpy`
  bit-reinterpret; weights via `UnpackUDEC4N_BE` /1023, matching the encoder
  `PackVector(..., normalize=false)` in `dc3-decomp/src/system/rndobj/Mesh.cpp:1631`
  `FillCompressedVertex`). Weight quantization drift for the test's 0.25/0.75 weights is ~0.0007.
- The **test helper** `SerializeCompressedVertexBE` (`tests/test_mesh_loading.cpp:45`) writes the
  three **float** position fields with `PutBE32((uint32_t)cv.mPosX)` — a **value truncation**
  (`(uint32_t)1.0f == 1`), not a bit-reinterpret. Decode then reads `0x00000001…` as a float
  (a denormal ≈ 0), collapsing the compressed position toward the origin. Numeric model: with
  positions collapsed, per-axis deltas are ≈ (0.13, 2.1, 3.0) — all ≫ 0.02; with correct
  positions, deltas are ≈ 0.007 — well within tol. `CompressedVertex_Xbox.mPosX/Y/Z` are `float`
  (`dc3-decomp/src/system/rndobj/MeshVertCompress.h:7-9`); the sibling helper
  `MakeCompressedVertexRecord` (line 33) already uses the correct `PutBEFloat(...)`.

**Fix (milo-native-engine):** in `SerializeCompressedVertexBE`, change the three position writes
from `PutBE32((uint32_t)cv.mPosX/Y/Z)` to `PutBEFloat(buf, cv.mPosX/Y/Z)` (bit-reinterpret, per
`tests/test_helpers.h:146`). Leave the six genuinely-integer fields (`mColor`, `mNormal`,
`mTangent`, `mBinormal`, `mBoneIndices`, `mBoneWeights`) as `PutBE32`. This corrects the oracle,
does **not** touch tolerance, and exercises the real (correct) decode path. **The implementer
MUST build + run the single test first to capture the actual per-axis deltas and confirm this
diagnosis before applying** (see S3 approach). `SerializeCompressedVertexBE` has exactly one
caller (line 194), so no other test relies on the truncating behavior.

### Bucket (d) — 1 test: `ObjectLifetimeTest.MergeDirsMoveAllSubdirsTransfersOwnership`
`tests/test_object_lifetime.cpp:413` (milo-native-engine). The **test oracle is the suspect**,
not (only) dc3 `Dir.cpp` — this needs careful faithful-reference resolution, hence Opus.

**Findings (verified against the authoritative RB3 decomp):**
- The test builds `fromDir` with one subdir `movedSubdir`, runs
  `MergeDirs(fromDir, toDir, MergeFilter(kReplace, kMoveAllSubdirs))`, then asserts
  `EXPECT_TRUE(toDir->HasSubDir(movedSubdir))` **and** `EXPECT_FALSE(fromDir->HasSubDir(movedSubdir))`,
  then `delete fromDir; EXPECT_NE(movedSubdir,nullptr); delete toDir;`.
- dc3's current `MergeObjectsRecurse` (`dc3-decomp/src/system/obj/Utl.cpp:364`), `kMoveAllSubdirs`
  → `DefaultSubdirAction` → `kMergeReplace` (== 1), case `1`: `if(!toDir->HasSubDir(fromDir))
  toDir->AppendSubDir(...); return;` — appends to dst, **does NOT erase from source**. So
  `fromDir->HasSubDir(movedSubdir)` stays true → the `EXPECT_FALSE` fails.
- **This dc3 behavior is FAITHFUL.** The authoritative RB3 decomp of the same function —
  **`/home/free/code/milohax/rb3/src/system/obj/Utl.cpp:247` `MergeObjectsRecurse`**, `case
  MergeFilter::kReplace`: `if (!toDir->HasSubDir(fromDir)) toDir->AppendSubDir(ObjDirPtr<ObjectDir>(fromDir));
  return;` — **also does NOT erase from the source dir.** The source-erase (`subDirs.erase(...)`)
  existed only in dc3's **pre-og-dc3-port** structure and was deliberately removed by dc3 commit
  `1c757f63` ("port system/obj/Utl from og-dc3") to match the reference. The engine test's
  `EXPECT_FALSE(fromDir->HasSubDir(...))` therefore encodes a **non-faithful** "move erases source"
  expectation inherited from the old code.
- The test's real intent (name = "TransfersOwnership", plus the `delete fromDir` then check
  `movedSubdir` still valid, then `delete toDir` sequence) is **ownership survival**: after the
  merge, deleting `fromDir` must not destroy `movedSubdir` (dst keeps it alive). Whether that
  holds under the faithful append-both-dirs behavior depends on `ObjDirPtr`/`~ObjectDir` teardown
  semantics (the exact territory of dc3's `docs/plans/dc3-native/object-lifetime-guard-root-cause.md`,
  esp. `gSuppressDirPtrDelete`, `RemoveSubDir`, `~ObjectDir::DeleteSubDirs`).

**Resolution path (S4, Opus — decide with evidence, do not guess):**
1. Build + run the single test; capture **which** `EXPECT` fails and whether `delete fromDir`/`delete toDir`
   **crashes** (double-free of a now-shared subdir).
2. Reconcile against the faithful RB3 decomp (cited above) + the dc3 lifetime doc.
3. Land exactly one of:
   - **(d-i) Correct the non-faithful oracle (milo-native-engine test).** If the merge itself is
     faithful and the only problem is the `EXPECT_FALSE(fromDir->HasSubDir(...))` assertion
     contradicting RB3 behavior: replace that single assertion with a **faithful ownership-survival
     check** — e.g. keep `EXPECT_TRUE(toDir->HasSubDir(movedSubdir))`, drop/replace the
     source-erased expectation, and assert `movedSubdir` remains valid & dst-reachable after
     `delete fromDir`. Add a comment citing `rb3/src/system/obj/Utl.cpp:247`. This is **oracle
     correction, not weakening** — it must be justified in STATUS.md with the citation.
   - **(d-ii) Fix a genuine teardown/ownership bug (dc3-decomp `obj/Dir.cpp`).** If the faithful
     merge leaves a real double-free / dangling on `delete fromDir` (source `~ObjectDir` frees a
     subdir that dst now references), fix the lifetime handling in `Dir.cpp`
     (`~ObjectDir`/`DeleteSubDirs`/`RemoveSubDir`) so shared subdirs survive source teardown —
     keeping the *merge* itself faithful.
   Either way: keep `toDir->HasSubDir(movedSubdir)` true, do not add a source-erase to
   `MergeObjectsRecurse` (that would diverge from the RB3 target), and do not `DISABLED_`/skip
   without a written faithful-reference reason.

## Build / run recipe (all subtasks)

Engine test suite target is `milo-engine-tests` in the **milo-native-engine** repo. Use your OWN
build dir `milo-native-engine/build-agent-W2-TESTFIX` (rule 5; the engine-test convention uses the
engine repo's own `build-agent-*`, per `../W0.1/STATUS.md`). Reuse if it already exists.

Seed the decomp-context cache vars from the known-good W0.1 configure (do not hand-type the
compat-flag string):
```bash
cd /home/free/code/milohax/milo-native-engine
# one-time: generate an initial-cache from the working W0.1 build
grep -E '^(MILO_ENGINE_DECOMP_INCLUDE_DIRS|MILO_ENGINE_DECOMP_PCH|MILO_ENGINE_DECOMP_COMPAT_FLAGS|MILO_ENGINE_BUILD_TESTS|DC3_RUNTIME_ROOT|CMAKE_PREFIX_PATH|MILO_TEST_MILO_LIB|CMAKE_GTEST_DISCOVER_TESTS_DISCOVERY_MODE):' \
  build-agent-W0.1/CMakeCache.txt \
  | sed -E 's/^([^:]+):([^=]+)=(.*)$/set("\1" "\3" CACHE \2 "")/' > /tmp/w2-testfix-init.cmake
cmake -B build-agent-W2-TESTFIX -S . -C /tmp/w2-testfix-init.cmake
cmake --build build-agent-W2-TESTFIX --target milo-engine-tests -j8
```
Assets (required for the bucket-a/b AssetLoadingTest + ClipPoseFixture tests to run non-skipped):
```
DC3_DATA=/home/free/code/milohax/dc3-decomp/orig-assets \
MILO_LIB=/home/free/code/milohax/dc3-decomp/orig-assets/extracted
```
Run one test / a bucket / the full suite:
```bash
cd /home/free/code/milohax/milo-native-engine/build-agent-W2-TESTFIX
DC3_DATA=... MILO_LIB=... ./tests/milo-engine-tests --gtest_filter='<Suite.Test>'
DC3_DATA=... MILO_LIB=... ctest -j1 --output-on-failure          # full suite gate
```
Note: dc3 (`XmaPcmSidecar.h`, `CharBones.cpp`, `Utl.cpp`, `Dir.cpp`) is compiled into
`milo-engine-tests` via the decomp context, so dc3 edits take effect on the next engine rebuild —
no engine pin bump needed.

## Evidence procedure (these are behavior CHANGES / test-oracle corrections, NOT MOVE commits)

None of the four fixes are Phase-1 MOVE commits, so **no screenshot-hash / drawlog byte-identical
evidence is required**. The required evidence for each subtask, recorded in STATUS.md:
1. **Baseline** (once, S0 of whoever builds first): `ctest -j1` shows **169 passed / 29 failed / 2
   skipped**; save the failing-test list.
2. **Per bucket, fail→pass:** the bucket's named tests go from FAILED → PASSED after the fix
   (run the exact `--gtest_filter`, paste the count).
3. **No regression:** re-run full `ctest -j1`; the failed count drops by **exactly** the bucket's
   size and **no previously-passing test flips to FAILED** (diff the failing-test lists).
4. **Bucket (b) match-neutrality:** state the `&vec[i]`→`vec.data()+i` transform is match-neutral
   per the shipped `ScaleDown` precedent (CharBones.cpp:404-409). If dc3 objdiff tooling is readily
   available, spot-check that `ScaleAdd`/`Blend`/`RotateBy`/`RotateTo` match% is unchanged; if not,
   the precedent + green suite is sufficient — note which.
5. **Bucket (d):** record the observed failure/crash, the faithful-reference citation
   (`rb3/src/system/obj/Utl.cpp:247`), and which resolution (d-i / d-ii) was chosen and why.

## Subtasks

### W2-TESTFIX.S1 — Bucket (a): XMA sidecar namespace fix  (model: sonnet)
- **Goal:** resolve `undefined symbol _ZN7dc3_xma16NativeGetDataDirEv`; the 16 AssetLoadingTest/
  BulkLoad*/ChooseModeSubdirLoading tests build & run.
- **Files:** `dc3-decomp/native/src/platform/XmaPcmSidecar.h` (only).
- **Approach:** move the `extern const char *NativeGetDataDir();` currently at line ~96 (inside
  `namespace dc3_xma`, inside `SidecarDir()`) to **global scope**, just above `namespace dc3_xma {`
  (~line 56). Delete the in-function extern. Verify unqualified `NativeGetDataDir()` at the call
  site now resolves to the global `::NativeGetDataDir` (File_Native.cpp:27).
- **Verify:** rebuild `milo-engine-tests`; `nm -C tests/milo-engine-tests | grep -c dc3_xma.*NativeGetDataDir`
  → 0 unresolved. Run the 16 bucket-(a) tests (filter list in the findings above) with DC3_DATA/
  MILO_LIB set → all PASS. Full `ctest -j1`: failed count 29 → 13, no new failures.
- **Commit:** dc3-decomp, flock `/tmp/dc3-git.lock`, prefix `W2-TESTFIX:`, stage only
  `native/src/platform/XmaPcmSidecar.h`.

### W2-TESTFIX.S2 — Bucket (b): CharBones hardened-vector bounds fix  (model: sonnet)
- **Goal:** the 11 `ClipPoseFixture.*` / `BoneServo…` / `CpuSkinForearm…` tests stop aborting.
- **Files:** `dc3-decomp/src/system/char/CharBones.cpp` (only).
- **Approach:** apply the exact `ScaleDown` (line 402, comment 404-409) transform to the four
  functions that still use raw `operator[]` end-pointers: `Blend`(591), `ScaleAdd`(710),
  `RotateBy`(1000), `RotateTo`(1202). Mechanically: `&mBones[X]` → `mBones.data() + X`;
  `&bones.mBones[X]` → `bones.mBones.data() + X`. Use the grep site list in the findings to hit
  every occurrence; do not alter loop logic, iteration, or the `TestDstComplain` punt paths.
  Before finalizing, confirm the abort was purely one-past-end **pointer formation** (never a
  dereference) so this is not masking a genuine retarget under-run — the `>=`/`==` end checks
  guard every dereference, consistent with `ScaleDown`.
- **Verify:** rebuild; run `--gtest_filter='ClipPoseFixture.*:AssetLoadingTest.BoneServoCarriesAndAppliesForeArmRotZChannels:AssetLoadingTest.CpuSkinForearmVertexFromCompressedMesh'`
  → all PASS (no abort). Full `ctest -j1`: failed count drops by 11, no new failures. Record the
  match-neutrality note (evidence item 4).
- **Commit:** dc3-decomp, flock `/tmp/dc3-git.lock`, prefix `W2-TESTFIX:`, stage only
  `src/system/char/CharBones.cpp`.

### W2-TESTFIX.S3 — Bucket (c): compressed-skin test-oracle fix  (model: opus)
- **Goal:** `MeshVertexLoading.CompressedSkinningMatchesCpuSkinningForSyntheticBones` passes via
  the real decode path, tolerance untouched.
- **Files:** `milo-native-engine/tests/test_mesh_loading.cpp` (only) — expected fix; **do NOT edit
  `src/gfx/VertexFormats.cpp` unless the confirm step proves a real decode regression.**
- **Approach:** (1) **Confirm first** — build + run just this test with DC3_DATA/MILO_LIB, capture
  the actual per-axis `EXPECT_NEAR` deltas; optionally add a temporary `fprintf` of `gpuVert.pos`
  vs `vert.pos` to prove the position collapse. Expect gv.pos ≈ 0 (denormal), deltas ≈
  (0.13,2.1,3.0). (2) Fix `SerializeCompressedVertexBE`: the three position writes
  `PutBE32((uint32_t)cv.mPosX/Y/Z)` → `PutBEFloat(buf, cv.mPosX/Y/Z)`. Leave the six integer
  fields as `PutBE32`. (3) Remove any temporary prints. If — and only if — the confirm step shows
  the drift persists with correct BE-float positions, escalate: it is a genuine
  `UnpackUDEC4N_BE`/offset regression; root-cause in `VertexFormats.cpp` and document.
- **Verify:** the test PASSES (deltas ≈ 0.007 < 0.02). Full `ctest -j1`: failed count drops by 1,
  no new failures.
- **Commit:** milo-native-engine, flock `/tmp/milo-engine-git.lock`, prefix `W2-TESTFIX:`, stage
  only `tests/test_mesh_loading.cpp`.

### W2-TESTFIX.S4 — Bucket (d): Dir-merge faithful resolution  (model: opus)
- **Goal:** `ObjectLifetimeTest.MergeDirsMoveAllSubdirsTransfersOwnership` passes **without
  diverging the merge from the RB3 target**, or is skipped only with a written faithful reason.
- **Files:** `milo-native-engine/tests/test_object_lifetime.cpp` (path d-i) and/or
  `dc3-decomp/src/system/obj/Dir.cpp` (path d-ii). Read-only references:
  `rb3/src/system/obj/Utl.cpp:247` (authoritative), `dc3-decomp/src/system/obj/Utl.cpp:364`,
  `dc3-decomp/docs/plans/dc3-native/object-lifetime-guard-root-cause.md`.
- **Approach:** follow the 3-step Resolution path in the bucket-(d) findings — run the test,
  observe exact failure/crash, reconcile with the faithful RB3 decomp (which does NOT erase the
  source subdir), then land **d-i** (correct the non-faithful `EXPECT_FALSE(fromDir->HasSubDir)`
  oracle to a faithful ownership-survival check, engine test) **or d-ii** (fix a genuine
  shared-subdir teardown/double-free in dc3 `Dir.cpp`). **Do NOT** add a source-`erase` to
  `MergeObjectsRecurse` (diverges from target). Justify the chosen path in STATUS.md with the
  citation.
- **Verify:** the test PASSES (incl. clean `delete fromDir`/`delete toDir`, no ASan/abort). Full
  `ctest -j1`: failed count drops by 1, no new failures.
- **Commit:** owning repo (engine for d-i, dc3 for d-ii), correct flock lock, prefix
  `W2-TESTFIX:`, stage only the file you changed.

### W2-TESTFIX.S5 — Full-suite green gate + wave-gate wiring note  (model: opus)
- **Goal:** confirm the whole suite is green and document it as a usable wave gate.
- **Depends on:** S1-S4 committed (see Risks — serialize the shared build).
- **Approach:** clean-configure `build-agent-W2-TESTFIX` (or rebuild), run full
  `ctest -j1 --output-on-failure` with DC3_DATA/MILO_LIB. Confirm **200 total → 198 passed, 2
  skipped-by-design** (`ExtractBik.ExtractSmallest`, `SkinGolden.CaptureGolden`), **0 failed**. If
  any residual failure survives, either fix it or record a `DISABLED_`/`GTEST_SKIP()` with a
  faithful-reference reason and a tracking line in STATUS.md (never a silent tolerance change). If
  any subtask introduced a NEW env flag (none expected), register it in the engine
  `NativeCompatFlags` registry and regenerate the ledger via
  `scripts/analysis/native_compat_census.py` (per Wave-1 W0.6 convention). Append a "gate wiring"
  note: `milo-engine-tests` + `ctest -j1` with the two asset env vars is the convergence gate;
  the coordinator's integration-worktree CI already invokes it.
- **Verify:** paste the final `ctest` summary line + the (empty) failed-test list into STATUS.md.
- **Commit:** none required unless a residual needs a documented skip; if so, stage only that test
  file in its owning repo under the correct flock.

## Exit criteria (measurable)

1. `ctest -j1` on `milo-engine-tests` (DC3_DATA + MILO_LIB set) reports **0 failed** (198 passed,
   2 skipped-by-design) — down from the Wave-1 baseline of 29 failed. Final summary pasted in STATUS.md.
2. Bucket (a): all 16 named tests PASS; `nm -C` shows no unresolved `dc3_xma…NativeGetDataDir`.
3. Bucket (b): all 11 named tests PASS (no `operator[]` abort); transform documented match-neutral.
4. Bucket (c): the compressed-skin test PASSES via the real decode path with the **0.02 tolerance
   unchanged** (no weakening); confirm-step deltas recorded.
5. Bucket (d): the merge test PASSES with the merge itself still faithful to
   `rb3/src/system/obj/Utl.cpp:247` (no source-erase added), or is documented-skipped with a cited
   reason; the chosen d-i/d-ii path justified in STATUS.md.
6. No previously-passing test regressed (failing-list diff clean at each step). Each fix is a
   single-file, single-repo commit prefixed `W2-TESTFIX:`, staged narrowly, under the repo's flock.

## Risks / conflicts

- **Concurrent dc3-decomp owners:** S1/S2 (and possibly S4-d-ii) edit dc3 files. `git log`/`git
  status` the target file first; never `git add -A`; flock `/tmp/dc3-git.lock`; NEVER
  `reset/checkout--/restore/rebase` the shared tree (rule 7). If a file already carries an
  unrelated concurrent edit, stage only your hunks (`git add -p`) and flag overlap in STATUS.md.
- **Shared engine build dir:** S1-S5 all build `milo-engine-tests` in the same
  `build-agent-W2-TESTFIX`. If subtasks run concurrently, serialize the build/ctest steps (a build
  lock or run S1→S4 sequentially, S5 last). S5 must run after all fixes are committed.
- **This wave's lane A** (`W1.2→W1.3→W1.4→W1.5→W1.7→W1.6` on `Rnd_Wgpu_RB3.cpp`) and **W0.3b** run
  elsewhere; W2-TESTFIX touches **none** of those files (dc3 `char`/`obj`/`platform` + engine
  `tests/`), so **no file conflict** with lane A or W0.3b. Assume prior lane-A commits may land in
  parallel — irrelevant to this item.
- **Bucket (c)/(d) surprise:** if the confirm step contradicts the diagnosis (c: drift persists
  with correct floats → real decode regression; d: a genuine dc3 teardown bug), the Opus subtask
  escalates into the source fix per its approach; the plan's expected fixes are the high-confidence
  path but the subtasks are instructed to verify before editing.
- **Do not bump `MILO_ENGINE_PIN`** (rule 3). dc3 edits are picked up by engine-test rebuild
  directly; the coordinator handles any pin bump at wave close.
