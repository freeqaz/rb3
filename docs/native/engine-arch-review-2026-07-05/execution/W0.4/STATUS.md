# W0.4 — Bone ground-truth live-pose golden — STATUS

Append-only. One `## <subtask-id> — done|partial|blocked` section per subtask, under flock /tmp/rb3-docs.lock.

## W0.4.S1 — done

**Commit (engine repo `milo-native-engine`):** `669ebc3` — "W0.4: add live-pose
effector WORLD-position golden test (S1)". Single CHANGES commit, stages only
`tests/test_bone_ground_truth.cpp` (tests/CMakeLists.txt untouched — no W0.1 collision).

**What shipped:** `TEST_F(ClipPoseFixture, EffectorWorldPositionsMatchGolden)` +
file-local helpers (`struct EffectorGolden`, `FindClipByName`).
- **Clip pinned BY NAME:** `crouching_great_01` (first dance clip in
  `char/crowd/anim/gen/female_base.milo_xbox`; confirmed to carry L/R foreArm+hand
  channels). Falls back to `sDir` if not in `sClipDir`.
- **Beats pinned BY FRACTION** of `[StartBeat, StartBeat+LengthBeats]`: `{0.0, 0.5, 0.9}`
  (avoids `EndBeat()` exactly). No dependence on ObjDirItr order or absolute asset beats.
- **Effectors (resolved in `sDir`, the posed dir):** `bone_R-hand.mesh`, `bone_L-hand.mesh`,
  `bone_R-toe.mesh`, `bone_L-toe.mesh` (ankle fallback wired but not needed — all toes present).
- **Prop bone: FOUND and included** — `bone_prop3.mesh` IS reachable in
  `skeleton_bones_resource` (the plan's "no prop expected" assumption was conservative).
  NOTE: the crowd dance clip has no prop channel, so it stays frozen at world origin
  `(0,0,0)` across all beats → low placement-signal (it asserts an "stays at origin"
  invariant). S2 may keep or drop it; the harness includes it per the brief's "if reachable".
- **eps = 0.05f** world units (model is cm-scale; hands ~15u from center; count-in-shard
  regressions shift 50–65u → huge margin).
- **`MILO_TEST_DUMP_POSE_GOLDEN`** dump mode: prints copy-paste-ready `kEffectorGoldens[]`
  initializers then `GTEST_SKIP()`. **`MILO_TEST_POSE_PERTURB=<f>`** fail-red hook: adds `<f>`
  to each measured world-X before comparing. `kEffectorGoldens[]` ships EMPTY → SKIPs cleanly
  (green-or-skip, never spurious-red).

**Verification (standalone `milo-engine-tests`, all 5 modes proven):**
1. Compiles clean (no errors/warnings in `test_bone_ground_truth.cpp.o`).
2. Committed state (empty golden) → `[ SKIPPED ]` cleanly (NOT red).
3. Dump mode → prints a full non-empty initializer block (15 rows), then SKIPs.
4. Temp-pasted goldens + perturb=0 → `checked 15 effector golden(s)`, `[ PASSED ] 1 test.`
5. Temp-pasted goldens + `MILO_TEST_POSE_PERTURB=1.0` → exit 1, every effector's "world X"
   assertion FAILS red. (Goldens then reverted to empty for the commit.)

**Captured goldens (head-start for S2 — verify with a fresh dump before committing):**
```
    { "bone_R-hand.mesh", 0.00f, 2.519011f, 13.530660f, 15.481359f },
    { "bone_L-hand.mesh", 0.00f, 4.419144f, 10.330550f, 13.740979f },
    { "bone_R-toe.mesh", 0.00f, 8.257820f, 2.759385f, -0.619148f },
    { "bone_L-toe.mesh", 0.00f, -6.312924f, -8.265928f, 2.694563f },
    { "bone_prop3.mesh", 0.00f, -0.000000f, 0.000000f, 0.000000f },
    { "bone_R-hand.mesh", 0.50f, 2.753734f, 2.600021f, 46.956837f },
    { "bone_L-hand.mesh", 0.50f, -4.046994f, 9.154058f, 11.256922f },
    { "bone_R-toe.mesh", 0.50f, 8.278828f, 2.709412f, -0.756977f },
    { "bone_L-toe.mesh", 0.50f, -6.438015f, -8.774557f, 2.522069f },
    { "bone_prop3.mesh", 0.50f, -0.000000f, 0.000000f, 0.000000f },
    { "bone_R-hand.mesh", 0.90f, 8.679708f, 7.524742f, 52.679539f },
    { "bone_L-hand.mesh", 0.90f, -15.249717f, 4.059789f, 53.735123f },
    { "bone_R-toe.mesh", 0.90f, 4.766071f, -9.629133f, 2.280952f },
    { "bone_L-toe.mesh", 0.90f, -7.154653f, -9.448845f, 1.787806f },
    { "bone_prop3.mesh", 0.90f, -0.000000f, 0.000000f, 0.000000f },
```

**DEVIATIONS / ENVIRONMENT BLOCKERS (pre-existing, NOT W0.4 — S2 + coordinator must know):**
The plan's assumption "the same config `build-tests` already proves works" is FALSE in the
current checkout. A fresh standalone configure/build/run of `milo-engine-tests` is blocked by
dc3-decomp having drifted ahead of the engine's frozen source list + Arch libstdc++ hardening.
Three distinct issues, each worked around LOCALLY (all workarounds reverted — repos left clean):
1. **dsp/ relocation.** `tests/dc3_runtime_sources.cmake` lists 6 DSP files at old
   `src/system/synth/*.cpp` paths; dc3 moved them to `src/system/dsp/`. Fresh configure FAILS
   ("Cannot find source file BitCrushEffect.cpp"). Workaround: temp symlinks (removed).
2. **Missing native glue.** The list omits `native/src/StubTrace.cpp` and the link leaves several
   dc3 symbols undefined (`vtable for MemcardXbox`, and a namespace-mismatched
   `Hmx::SoundAudioTraceOn` — Sound.cpp defines `::SoundAudioTraceOn` global but a caller
   references it inside `Hmx::`). Link uses `--unresolved-symbols=ignore-all`, so the exe builds
   but ld.so aborts at startup. Workaround: a 2-symbol behavior-neutral link shim (MemcardXbox
   vtable never exercised by pose tests; `Hmx::SoundAudioTraceOn()`→false is the real default).
3. **Latent PoseMeshes OOB + `-O0` assertions.** `CharBones::PoseMeshes` trips
   `vector<CharBones::Bone>::operator[]` `__glibcxx_assert(__n < size())` — a PRE-EXISTING latent
   out-of-bounds that the reference CI hides by building optimized (`__OPTIMIZE__` defined → GCC
   auto-disables `_GLIBCXX_ASSERTIONS`). At `-O0` it aborts EVERY pose test (verified: existing
   `PoseMeshesDoesNotCrash`/`PoseChangesTransforms` crash identically). Workaround:
   `-D_GLIBCXX_NO_ASSERTIONS` in `CMAKE_CXX_FLAGS`.
None of the three is in W0.4's single-file scope. **Recommend the coordinator regenerate
`tests/dc3_runtime_sources.cmake`** (fix dsp paths, add StubTrace.cpp) and file the
`Hmx::SoundAudioTraceOn` namespace mismatch + the `PoseMeshes` OOB against dc3. Until then, S2's
dump-run needs the same three local workarounds (reproduce from this section). Reference build
command that works: `cmake -B build-agent-W0.4 -C cmake/dc3-reference.cmake
-DMILO_ENGINE_BUILD_TESTS=ON -DCMAKE_CXX_FLAGS=-D_GLIBCXX_NO_ASSERTIONS`, then symlink the 6 dsp
files + add `StubTrace.cpp` to the source list + relink with a `_ZTV11MemcardXbox` +
`Hmx::SoundAudioTraceOn` shim (or, once the list is regenerated, only the last two).

**Remaining for S2:** run the dump under the workarounds above, paste goldens (verify against the
captured block above — should match this build), commit, and record the green + red transcripts.
