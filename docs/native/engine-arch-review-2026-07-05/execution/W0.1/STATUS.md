# W0.1 — Status log

Append-only. One `## <subtask-id> — done|partial|blocked` section per subtask, under
`flock /tmp/rb3-docs.lock`, with engine commit SHAs and any blockers. Implementers read this +
`git log --grep=W0.1` (engine repo) and skip already-done work.

<!-- sections appended by implementers below -->

## W0.1.S1 — done

**Engine commit:** `d8d21271e9638d07ba14725f1bd0bbcd54a7f727` (milo-native-engine repo),
staging only `tests/test_skin_golden.cpp` (new, 584 lines) + one line appended to
`tests/CMakeLists.txt` `MILO_ENGINE_TEST_SOURCES`.

**Deliverable:** `tests/test_skin_golden.cpp` — standalone LP64 CPU reference skinner
`RefSkinVertex` mirroring `RndMesh::SkinVertex` (Mesh.cpp:1367-1410): skip
`boneIdx >= NumBones()`, no weight normalization, `skinMat = BoneOffsetAt(i)·boneWorld_i`,
row-vector `pos·tf60`. Golden + internal-oracle cross-check + permanent positive-control.

**Golden provenance:** asset `char/main/gen/main.milo_xbox` (loads as HamCharacter `main`);
posed via `ApplyDeterministicPose` (fixed ~34° X-rotation on arm/hand bones, `SetLocalRot`);
10 golden verts from `left_arm.mesh` (2126 decoded verts, 25 bones incl. hand bones) —
**6 are hand/finger-region** (dominant bone is a hand bone; verts 81-86) + 4 contrast verts.
Captured MILO_SKIN_GOLDEN_CAPTURE=1, run twice → **byte-identical** (deterministic).

**Verification (build-agent-W0.1, all with committed DC3_DATA/MILO_LIB assets):**
- `SkinGolden.GoldenMatchesReference` PASS — 10 verts within kEpsAbs=5e-2 (6 hand-region).
- `SkinGolden.ReferenceMatchesCompiledSkinVertex` PASS — RefSkinVertex vs compiled
  `mesh->SkinVertex` within 1e-4 (reference is faithful to the decomp).
- `SkinGolden.BrokenSkinDivergesFromGolden` PASS — hand-vert fling 25.479u > 5.0 threshold.
- **Fail-red proven:** `MILO_SKIN_GOLDEN_BREAK=1 … --gtest_filter=SkinGolden.GoldenMatchesReference`
  → EXIT=1, `[  FAILED  ]`. Sample: left_arm.mesh vert 81 .z p=60.63 vs g=35.17 (Δ25.47u);
  .y Δ12.89u; .x Δ5.60u — many-unit flings on hand verts, exactly the H2 rotation-basis mode.

**Deviations from PLAN.md (dc3 checkout is mid-refactor), recorded in the file header:**
1. Engine-test compile context is DC3 headers where `RndMesh::Vert::boneWeights` is a plain
   float `Vector4` read via `(&boneWeights.x)[i]`, NOT rb3 `Vector4_16_01`/`FloatAt`. RefSkinVertex
   mirrors the DC3 (== compiled) form; the three faithful invariants (a)/(b)/(c) are identical.
2. `CharClip::PoseMeshes(main|crowd, female_base)` aborts on a hardened
   `std::vector<CharBones::Bone>` bounds assert in `CharBones::ScaleAdd` (retarget mismatch —
   OOB the shipped build reads through, `_GLIBCXX_ASSERTIONS` aborts on). A suite-aborting test
   is unacceptable and fixing dc3 CharBones is out of scope, so the character is posed directly
   via `SetLocalRot` (more deterministic; no clip/beat dependency). Backtrace confirmed frame
   `CharClip::PoseMeshes` → `CharBones::ScaleAdd` → `vector::operator[]`.
3. Real asset meshes store Xbox-compressed verts (`mCompressedVerts`), so `Verts()` is empty
   after load; the test decodes them via `VertexFormats::UnpackCompressedSkinnedVertices`
   (same path Mesh_Wgpu uses) into `RndMesh::Vert` and skins those.
4. Two `--allow-multiple-definition` link anchors in the test TU (`MemcardXbox::Init`,
   `Hmx::SoundAudioTraceOn`) restore fresh-binary loadability that dc3 drift broke (undefined
   `_ZTV11MemcardXbox` / `_ZN3Hmx17SoundAudioTraceOnEv` at load, before any test runs). Harmless
   where dc3 already provides the symbols. Delete when dc3 is consistent.

**Build note:** the PLAN.md bare `cmake -B … -DDC3_RUNTIME_ROOT=…` yields `context: OFF`
(placeholder engine, tests do not compile). The suite needs the decomp context cache vars
(`MILO_ENGINE_DECOMP_INCLUDE_DIRS/_COMPAT_FLAGS/_PCH`, Dawn `CMAKE_PREFIX_PATH`), replicated
from the working `build-tests` cache via an initial-cache file. Also configured
`-DCMAKE_GTEST_DISCOVER_TESTS_DISCOVERY_MODE=PRE_TEST` so a load failure does not delete the
binary at build time.

**Pre-existing failure NOT caused by W0.1 (for S2 / coordinator):**
`MeshVertexLoading.CompressedSkinningMatchesCpuSkinningForSyntheticBones`
(test_mesh_loading.cpp:220) fails in this fresh build — GPU 10/10/10/2 compressed-weight skin
vs raw CPU skin diverges ~0.26 > 0.02 tol. Compression code (`VertexFormats`/`MeshVertCompress`)
is NOT locally modified; the drift is in `dc3_runtime_sources.cmake`/`FxSendNative.cpp` (other
agents / dc3 refactor). Unrelated to the skin oracle. S2 should confirm via `ctest -j1` whether
it is present on the coordinator gate env (consistent dc3) and characterize.

**Remaining for S2:** full-suite `ctest -j1` no-regression run + gate-wiring note. Gate target
is `milo-engine-tests` via ctest; SkinGolden tests run non-skipped under the committed
DC3_DATA/MILO_LIB. Note: this env needs the two link anchors above to load at all.

## W0.1.S2 — done

**Reused** `build-agent-W0.1` (engine repo, `milo-native-engine/build-agent-W0.1`; per the
item's own build-dir convention, not `native/build-agent-W0.1`).

**Deviation from PLAN.md (blocking, fixed under W0.1):** the target `milo-engine-tests` did not
build/link at all when S2 started — two pre-existing gaps in `tests/dc3_runtime_sources.cmake`
(shared, "generated" list; unrelated to `test_skin_golden.cpp`'s content), fixed in engine commit
`09c9fab` ("W0.1: fix stale dc3_runtime_sources.cmake paths blocking milo-engine-tests link"):
1. 6 DSP effect `.cpp` (BitCrush/Delay/Distortion/EQ/Flanger/Wah) were listed at their old
   `src/system/synth/` path; dc3-decomp relocated them to `src/system/dsp/` two days before this
   wave (dc3 `2ad3492b`, "relocate DSP source files to original dsp/ directory") — CMake configure
   hard-failed (`Cannot find source file`).
2. `native/src/engine_stubs_generated.cpp` (this wave's loud-stub shim) references
   `dc3::gStubTraceEnabled`, defined in `native/src/StubTrace.cpp`, which was never added to the
   list — undefined symbol at test-process load (lazy PLT let most tests start; anything touching
   the loud-stub path crashed with `symbol lookup error`).
Confirmed against dc3-decomp's own `native/CMakeLists.txt` (:1133-1134,1295-1296) which already
pairs `StubTrace.cpp` with `engine_stubs_generated.cpp` and already compiles the `dsp/` paths —
this is drift in the hand-maintained engine list, not a new bug. **Note this working tree got
concurrently reset by another agent mid-session** (my first uncommitted edit vanished between
build and the next `git status`); reapplied and committed immediately to avoid losing it again.

**Fail-red proven (exit gate 1):**
```
DC3_DATA=.../orig-assets MILO_LIB=.../orig-assets/extracted MILO_SKIN_GOLDEN_BREAK=1 \
  ./tests/milo-engine-tests --gtest_filter='SkinGolden.GoldenMatchesReference'
```
→ `[ FAILED ]`, e.g.:
```
tests/test_skin_golden.cpp:514: Failure
The difference between p.x and g.x is 5.6039056777954102, which exceeds kEpsAbs, where
p.x evaluates to -13.729965209960938, g.x evaluates to -8.1260595321655273, kEpsAbs = 0.05
left_arm.mesh vert 81 .x
tests/test_skin_golden.cpp:515: Failure  (p.y=-32.086563 vs g.y=-19.200035, Δ12.89 > 0.05)  vert 81 .y
tests/test_skin_golden.cpp:516: Failure  (p.z=60.631706 vs g.z=35.165977, Δ25.47 > 0.05)   vert 81 .z
```
Same magnitude/vert as S1's captured evidence (vert 81, Δz≈25.47u) — reproducible across builds.

**Positive-control green (normal mode, no break env):**
```
[ RUN ] SkinGolden.GoldenMatchesReference               -> OK  (checked 10 golden verts, 6 hand/finger-region)
[ RUN ] SkinGolden.ReferenceMatchesCompiledSkinVertex    -> OK
[ RUN ] SkinGolden.BrokenSkinDivergesFromGolden          -> OK  (hand verts=6  maxHandFling=25.479 vs threshold 5.0)
[ RUN ] SkinGolden.CaptureGolden                          -> SKIPPED (capture mode off, by design)
```
All 3 real SkinGolden assertions PASS, non-skipped, under the committed `DC3_DATA`/`MILO_LIB`.

**Full-suite `ctest -j1 --output-on-failure`:** 200 total — **169 passed, 29 failed, 2 skipped**
(`ExtractBik.ExtractSmallest`, `SkinGolden.CaptureGolden`, both skip-by-design). SkinGolden's 3
non-skipped tests are among the passed. The 29 failures are reproducible across two consecutive
full runs (identical list both times) and fall into exactly 4 pre-existing buckets, **none of
which touch skin golden or any file W0.1 changed**:
1. **16 tests** (`AssetLoadingTest.{LoadSFXAssets,LoadWorldAssets,LoadStandaloneMiloFiles,
   LoadWorldMasterFile,LoadFullVenueWorlds,LoadDirectorSubdir,LoadSharedWorldSubdirs,
   CharacterVoiceBanksResolveRuntimeSoundScope,BetterOffAloneMoveGraphCopyPreservesLayoutResolution,
   BetterOffAloneAuthoredSongAnimsContainDanceMoves,BulkLoadAllFiles,BulkLoad_World,BulkLoad_UI,
   BulkLoad_SFX,BulkLoad_Songs,ChooseModeSubdirLoading}`) — `undefined symbol:
   _ZN7dc3_xma16NativeGetDataDirEv` at first SFX/XMA sidecar touch. Root cause: dc3-decomp's own
   `native/src/platform/XmaPcmSidecar.h:96` has a function-local `extern` decl for
   `NativeGetDataDir()` nested inside `namespace dc3_xma { inline std::string SidecarDir() {...} }`,
   giving it `dc3_xma::` linkage, but the one real definition
   (`native/src/platform/File_Native.cpp:27`) is in the global namespace — a namespace mismatch in
   dc3-decomp's own header, dated 2026-07-02 (dc3 `0bd29e7b`), 3 days before this wave. Not owned by
   W0.1; not fixed here (out of scope — SFX subsystem, not skinning).
2. **11 tests** (`AssetLoadingTest.{BoneServoCarriesAndAppliesForeArmRotZChannels,
   CpuSkinForearmVertexFromCompressedMesh}` + all 9 `ClipPoseFixture.*`) — abort on the SAME
   `std::vector<CharBones::Bone>::operator[]` bounds assert inside `CharBones::ScaleAdd` that S1
   already documented (Deviation #2) as a dc3-decomp retarget mismatch, and which is why S1's own
   golden test poses via `SetLocalRot` instead of `CharClip::PoseMeshes`. Confirmed pre-existing/
   out of scope.
3. **1 test** (`MeshVertexLoading.CompressedSkinningMatchesCpuSkinningForSyntheticBones`) — the
   compressed-weight vs raw-CPU-skin numeric drift (~0.26 > 0.02 tol) S1 already flagged as
   pre-existing and unrelated to the skin oracle.
4. **1 test** (`ObjectLifetimeTest.MergeDirsMoveAllSubdirsTransfersOwnership`) — real assertion
   failure in ObjDir subdir-merge bookkeeping. `test_object_lifetime.cpp` and `src/system/obj/Dir.cpp`
   are untouched by any Wave-1 commit; `obj/Dir.cpp`'s last change (dc3 `c6d0b556`, "wip: snapshot
   parallel-session work") predates this wave by 2 days — pre-existing dc3-decomp drift, not a
   W0.1 regression.

**No regression from W0.1:** all 29 failures are attributable to dc3-decomp source drift or
already-documented S1 deviations, none reference `test_skin_golden.cpp` or anything it touches;
the failure set was byte-identical across two independent full-suite runs.

**Gate wiring note:** the integration gate target is `milo-engine-tests` via `ctest` (already the
engine's convergence suite entry point: `cmake --build <dir> --target milo-engine-tests` then
`ctest -j1`), so the coordinator's integration-worktree full-rebuild CI picks up SkinGolden
automatically once it builds — no separate wiring step needed. Requires `DC3_DATA`/`MILO_LIB`
pointed at the committed dc3-decomp assets for SkinGolden (and the broader suite) to run
non-skipped; also requires engine commit `09c9fab` (or an equivalent fix) to be present, since
without it the whole `milo-engine-tests` binary fails to build/link, not just SkinGolden.

**Engine commit:** `09c9fab` (milo-native-engine), staging only `tests/dc3_runtime_sources.cmake`.

**Exit criteria status:** 1 (build+pass, non-skipped) ✅, 2 (fail-red proven) ✅, 3 (hand/finger
vert coverage, unchanged from S1) ✅, 4 (RefSkinVertex vs compiled SkinVertex ≤1e-4, unchanged from
S1) ✅, 5 (full suite green *for SkinGolden specifically*; whole-suite has 29 pre-existing,
independently-characterized, non-regressing failures — see above) ✅ with caveat noted, 6 (one
engine commit per subtask, STATUS.md updated) ✅.

## VERIFY — complete

Independently re-ran every exit criterion from a clean shell (build dir
`milo-native-engine/build-agent-W0.1`, HEAD `09c9fabca05c64a05465aa7e7eeba62b799b1d72`, no
rebuild needed — target was already up to date).

**1. Build + golden/oracle/positive-control PASS, non-skipped:**
```
DC3_DATA=.../orig-assets MILO_LIB=.../orig-assets/extracted \
  ./tests/milo-engine-tests --gtest_filter='SkinGolden*'
[ RUN ] SkinGolden.GoldenMatchesReference            -> checked 10 golden verts (6 hand/finger-region) [ OK ]
[ RUN ] SkinGolden.ReferenceMatchesCompiledSkinVertex -> [ OK ]
[ RUN ] SkinGolden.BrokenSkinDivergesFromGolden       -> hand verts=6 maxHandFling=25.479 (threshold 5.0) [ OK ]
[  PASSED  ] 3 tests. [ SKIPPED ] 1 test (CaptureGolden, skip-by-design).
```
PASS.

**2. Fail-red proven** — re-ran the break-env repro independently (piped to a log file to get
the real gtest exit code, not a `tail` artifact):
```
DC3_DATA=... MILO_LIB=... MILO_SKIN_GOLDEN_BREAK=1 \
  ./tests/milo-engine-tests --gtest_filter='SkinGolden.GoldenMatchesReference' > /tmp/w01_failred.log 2>&1
echo $?   # -> 1  (real gtest exit code; FAILED)
```
30 `EXPECT_NEAR` failures across verts {81,82,83,84,85,86,335,336,340,363} x,y,z. Vert 81 is
present in the failing set (matches S1/S2's recorded evidence for that vert, ~25u-scale flings);
additional vert 340/363 diffs of ~2.0u/~7.6u also observed. PASS — fail-red reproducible on demand,
not a one-off.

**3. Hand/finger vertex coverage** — `checked 10 golden verts (6 hand/finger-region)` printed by
the golden test itself, mesh `left_arm.mesh`. PASS.

**4. RefSkinVertex vs compiled RndMesh::SkinVertex ≤1e-4** — `ReferenceMatchesCompiledSkinVertex`
PASS (re-run above). PASS.

**5. Full-suite no-regression** — re-ran `ctest -j1 --output-on-failure` fresh:
```
86% tests passed, 29 tests failed out of 200
Total Test time (real) = 124.71 sec
```
Failing-test list is byte-identical to the one enumerated in S2's STATUS.md entry (16 XMA-sidecar-
symbol AssetLoadingTest/BulkLoad* tests, 11 CharBones::ScaleAdd-abort ClipPoseFixture/BoneServo/
CpuSkin tests, 1 MeshVertexLoading compressed-skin drift, 1 ObjectLifetimeTest subdir-merge bug) —
confirmed via `grep SkinGolden` on the ctest log: all 3 SkinGolden tests report `Passed` (195, 196,
197), only `CaptureGolden` (194) is `Skipped` by design. None of the 29 failures reference
`test_skin_golden.cpp`, `tests/CMakeLists.txt`, or `tests/dc3_runtime_sources.cmake`. PASS (with the
same pre-existing-drift caveat S2 already documented; not a W0.1 regression).

**6. Commit hygiene** — two engine commits, both prefix `W0.1:`, each MOVES-xor-CHANGES-clean:
  - `d8d21271e9638d07ba14725f1bd0bbcd54a7f727` — adds `tests/test_skin_golden.cpp` (584 lines, new)
    + 1-line append to `tests/CMakeLists.txt` (behavior-additive test, no existing test edited).
  - `09c9fabca05c64a05465aa7e7eeba62b799b1d72` — `tests/dc3_runtime_sources.cmake` only (build-fix,
    unblocks link; separate commit from the test addition, per hard rule 1).
  `git status --porcelain` on all three files W0.1 touched: clean (no uncommitted drift). PASS.

**All 6 exit criteria independently reproduced from a clean run. Item W0.1 is COMPLETE.**

No fixes were needed during verification (S1/S2's own build-fix commit `09c9fab` was already in
place and sufficient). No new commits from this VERIFY pass.

**Next actions if ever resumed:** none required for W0.1 itself. Out-of-scope items surfaced but
NOT part of this item's exit criteria (leave for their owners / a future wave):
  - dc3-decomp `XmaPcmSidecar.h:96` `NativeGetDataDir` namespace mismatch (`dc3_xma::` decl vs
    global-namespace definition) — blocks 16 SFX/XMA-touching tests.
  - dc3-decomp `CharBones::ScaleAdd` OOB assert on a retarget mismatch — blocks `CharClip::PoseMeshes`
    based tests (11 tests, incl. `ClipPoseFixture.*`).
  - `MeshVertexLoading.CompressedSkinningMatchesCpuSkinningForSyntheticBones` numeric drift (~0.26 vs
    0.02 tol) in `VertexFormats`/compressed-skinning path.
  - `ObjectLifetimeTest.MergeDirsMoveAllSubdirsTransfersOwnership` — real ObjDir subdir-merge bug,
    `src/system/obj/Dir.cpp`, pre-existing dc3 drift dated before this wave.
