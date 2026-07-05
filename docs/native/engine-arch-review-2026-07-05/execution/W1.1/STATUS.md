# W1.1 — Externalize 5 inline WGSL modules + shader validation — STATUS

Append-only. One `## <subtask-id> — done|partial|blocked` section per subtask, under
`flock /tmp/rb3-docs.lock`. Include commit SHAs (engine repo for shader/gtest moves) and blockers.
Re-runs read this + `git log --grep=W1.1` and skip done work.


## W1.1.S1 — done

All 5 inline WGSL raw-string modules externalized to `src/gfx/Shaders/*.wgsl.inc` in the engine
repo (`/home/free/code/milohax/milo-native-engine`), each a separate byte-identical MOVE commit,
each build-gated (`rb3-native` in `native/build-agent-W1.1`, clang) before commit.

**Deviation from PLAN.md:** the default `cc`/`c++` on this host is GNU 16, not Clang, so the initial
`cmake -B build-agent-W1.1 -S native` configure (no compiler override) fails to build the engine
(`-fms-compatibility`/`-ferror-limit=0`/`-fdelayed-template-parsing` unrecognized). Reconfigured with
`-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++` (matches the CMakeLists.txt Clang
requirement/warning). Noting this since PLAN.md's configure command as written doesn't force the
compiler; future subtasks reusing `build-agent-W1.1` should be fine (compiler choice is cached in
the build dir), but a fresh build dir on this host needs the same `-DCMAKE_*_COMPILER=clang*` flags.

Line numbers matched PLAN.md exactly for shader 1 (halo); shrank as expected after each move, so
shaders 2-5 were re-derived fresh via a `grep -nE` for the raw-string opens and a `grep -n` for the
`)WGSL"` closes before each edit, per instructions.

**Commits (engine repo, chronological):**
1. `6602bc0` — kRB3HaloBlitShaderSource -> rb3_halo_blit.wgsl.inc (decl 2350/close 2386 at time of edit)
2. `a8a90ce` — kRB3PostProcShaderSource -> rb3_postproc.wgsl.inc (decl 2669/close 2863, re-derived)
3. `9fc02b4` — kRB3QuadShaderSource -> rb3_quad.wgsl.inc (decl 3041/close 3085, re-derived)
4. `e625c80` — kRB3ComposeShaderSource -> rb3_compose.wgsl.inc (decl 3108/close 3131, re-derived; kept
   indented local-static decl, `#include` at column 0, indented trailing `;`)
5. `bd7c2f5` — kRB3ParticleShaderSource -> rb3_particle.wgsl.inc (decl 6342/close 6376, re-derived;
   kept non-static)

**Byte-identity gate:** all 5 produced a zero-line `diff` between the `sed`-stripped original
raw-string token (from `git show HEAD:...` pre-edit) and the new `.inc` file — `BYTE-IDENTICAL`
printed for every shader before its commit.

**Build gate:** `cmake --build native/build-agent-W1.1 --target rb3-native -j8` green after every
one of the 5 commits (incremental — only `Rnd_Wgpu_RB3.cpp` + link recompiled each time).

**Final verification (whole subtask):**
- `grep -nE 'R"[A-Za-z_]*\('` over `Rnd_Wgpu_RB3.cpp` -> empty (0 inline raw-string shaders remain)
- `grep -c '#include "gfx/Shaders/'` over `Rnd_Wgpu_RB3.cpp` -> 5
- `ls src/gfx/Shaders/*.wgsl.inc` -> 5 files (compose, halo_blit, particle, postproc, quad)
- `rb3-native` build in `build-agent-W1.1` -> green

`MILO_ENGINE_PIN` in `native/CMakeLists.txt` NOT touched (per hard rule 3 — coordinator bumps once
per wave). No rb3-repo files touched in this subtask.

**Handoff:** W1.1.S2 (shader validation gtest, model: opus) and W1.1.S3 (final byte-identical
verification + closeout, model: sonnet) remain — not started by this subtask.


## W1.1.S2 — done

Shader validation delivered: an always-on wgpu-based gtest (primary) + a best-effort
offline naga/tint script (graceful skip). rb3 repo commit `908a5d1f` (3 files, only mine):
- NEW `native/tests/test_wgsl_validation.cpp`
- EDIT `native/CMakeLists.txt` (+1 line: `test_wgsl_validation.cpp` in the rb3-tests
  `add_executable`, on its own line — staged via a single-hunk `git apply --cached` so a
  concurrent W0.2 edit to the same file (line ~648 `rb3_stub_census.cpp`) was NOT folded in)
- NEW `scripts/native/validate_wgsl.sh`

**gtest design:** `CompileOk(wgsl, firstError)` reproduces `PipelineManager.cpp:261-296`
(Dawn returns a non-null module on error → `CreateShaderModule` then
`GetCompilationInfo(WaitAnyOnly, cb)` scan for `CompilationMessageType::Error`, then
`Instance().WaitAny(future, UINT64_MAX)`). Device reached via `gBandRnd.Gpu().Device()` /
`.Instance()`. `EnsureGpu()` mirrors `test_texsharpen.cpp:38-100`
(`gBandRnd.InitGpu(64,64,true)` once; `GTEST_SKIP` if no device). The 5 externalized
shaders + `standard_wgsl.inc` are `#include`d as the exact shipped bytes (same compile-time
embed the engine uses), so a bad edit to any `.wgsl.inc` turns the test red.

**Results (MEASURED, GPU up):**
- `WgslValidation.AllRB3ShadersCompile` — PASS. Backend path logged: **real (native Dawn)**.
  All 6 compiled with zero WGSL Errors: rb3_halo_blit, rb3_postproc, rb3_quad, rb3_compose,
  rb3_particle, standard_wgsl.
- `WgslValidation.HarnessCatchesBadShader` — PASS (fail-red proof). Feeding
  `return nonexistent_fn();` → CompileOk returns false with
  `"unresolved call target 'nonexistent_fn'"`. This codified test IS the required fail-red
  demonstration and runs the IDENTICAL `CompileOk` path used by AllRB3ShadersCompile.

**Deviation (documented):** PLAN.md also suggested a one-time manual demonstration by
hand-corrupting a real `.wgsl.inc` and watching AllRB3ShadersCompile fail. I deliberately did
NOT corrupt the shared engine source tree — concurrent Wave-1 agents compile from it and a
transient corrupt `.inc` would break their incremental builds. The permanent
`HarnessCatchesBadShader` test is a strictly stronger, CI-enforced fail-red proof (same code
path, known-bad input, demonstrably returns false), so the intent is fully satisfied.

**Offline script:** `scripts/native/validate_wgsl.sh` detects naga/tint; neither installed on
this host (MEASURED) → prints the skip message and `exit 0` (skips-green). When a CLI is
present it strips the `R"WGSL(` / `)WGSL"` wrapper from every `src/gfx/*.inc` +
`src/gfx/Shaders/*.wgsl.inc` and validates the pure body (covers `standard_wgsl.inc` too).

**Build:** `rb3-tests` green in `native/build-agent-W1.1` (clang, reused S1 dir).

**Env notes for the verifier (S3):**
1. The RTX 3090 vkCreateDevice is intermittently `VK_ERROR_INITIALIZATION_FAILED` (GPU
   contention from concurrent Wave-1 agents). When it fails, `EnsureGpu` returns false and
   both WgslValidation tests `GTEST_SKIP` — retry until the device comes up (took 2 tries).
2. **Pre-existing shared-infra segfault:** when the GPU is unavailable, the rb3-tests binary
   exits 139 (segfault at process teardown on the failed-GPU-init path). This is NOT specific
   to this test — the reference `test_texsharpen` (`TexSharpenTest.*`) exits 139 identically
   under the same GPU-unavailable condition. Out of scope for W1.1; flagging for whoever owns
   the test-harness teardown. With the GPU up, no segfault and both tests PASS.
3. During S2 the shared build initially failed to LINK rb3-tests
   (`undefined reference to __hmx_stub_first_hit`) because W0.2's new untracked
   `native/src/rb3_stub_census.cpp` (defining that symbol, referenced by the W0.2-modified
   `band3_link_stubs.s`) had not yet been captured into `_RB3_NATIVE_SRCS`. A `cmake -B`
   reconfigure recaptured it and the link went green. Not a W1.1 issue.

**Handoff:** W1.1.S3 (final byte-identical verification + closeout, model: sonnet) remains.


## W1.1.S3 — done

Final verification of the whole W1.1 item (S1 + S2), confirming byte-identical MOVE +
green build/test + non-black rendering. No code changes in this subtask (docs-only closeout).

**Code invariants (re-confirmed, MEASURED on current HEAD):**
```
grep -nE 'R"[A-Za-z_]*\(' milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp   # -> empty
ls milo-native-engine/src/gfx/Shaders/*.wgsl.inc                        # -> 5 files
  (rb3_compose.wgsl.inc, rb3_halo_blit.wgsl.inc, rb3_particle.wgsl.inc,
   rb3_postproc.wgsl.inc, rb3_quad.wgsl.inc)
grep -c '#include "gfx/Shaders/' milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp   # -> 5
```
All confirmed as expected — 0 inline raw-string shaders remain, 5 `.wgsl.inc` files present, 5
`#include` sites.

**Build gate:** `rb3-native` and `rb3-tests` both build green in `native/build-agent-W1.1`
(reused S1/S2 dir; `-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++`, cached from S1).

**Test gate:**
- `ctest -R WgslValidation --output-on-failure`: both tests' internal gtest bodies PASS
  (`AllRB3ShadersCompile` — all 6 shaders incl. `standard_wgsl.inc` compile clean via real
  native Dawn, GPU = RTX 3090; `HarnessCatchesBadShader` — fail-red proof reproduced, catches
  `unresolved call target 'nonexistent_fn'`). ctest itself reports both as `SEGFAULT` —
  this is the **pre-existing shared-infra teardown segfault** already flagged by S2, NOT a
  regression: I reproduced the identical signature on the unrelated, untouched
  `TexSharpenTest.*`/`TexSharpenManagerTest.*` suite (7 more tests, same SIGSEGV-after-PASS
  pattern at process exit, same binary). Full-suite `ctest` run: **84% passed, 9/55 failed**,
  and all 9 failures are exactly this GPU-teardown-segfault class (2 WgslValidation + 7
  pre-existing TexSharpen*) — zero non-GPU regressions, zero new failure classes introduced
  by W1.1. Out of scope for W1.1 per S2's note; flagged again here for whoever owns
  rb3-tests/gtest-teardown-vs-Dawn-device-teardown ordering.
- GPU contention note (worse than S2 saw): both host RTX 3090s were pinned at ~97% VRAM by
  unrelated long-running `VLLM::EngineCore` processes (23.8-23.9/24.5 GiB each) during this
  verification window, causing `vkCreateDevice` -> `VK_ERROR_INITIALIZATION_FAILED` on the
  first several attempts (both `ctest` GPU tests and the standalone screenshot harness below).
  Retried; succeeded (GPU device came up) after ~6 attempts for the headless screenshot boot.
  Not a code issue — environment/host GPU pressure only.

**Screenshot verification — path (b) (baseline rebuild judged impractical: host GPUs are
severely contended by concurrent Wave-1 agents + unrelated VLLM processes; a from-scratch
pre-W1.1 engine rebuild would add significant extra load for marginal proof value beyond
S1's byte-identical diffs, which are already the authoritative per-shader proof — identical
source bytes cannot produce different render output).** Ran
`scripts/native/song-select-capture.py --bin native/build-agent-W1.1/rb3-native` headless
(RB3_HTTP=1), driving boot -> main_hub -> PLAY NOW -> QUICKPLAY -> song_select (exercises the
quad/postproc/compose shader paths) and capturing 5 `/api/screenshot` PNGs at scroll depths
0/8/16/30/50. Result: **PASS** — all 5 frames captured 1280x720, non-black
(ImageMagick mean 16383.8, i.e. real rendered content, not a black/blank frame), identical
sha256 across depths (`67b08cf2...371ab`) since the harness's scroll verbs did not move the
highlighted row far enough to change the rendered pixels in this run — expected, not a bug
(same behavior would occur pre-refactor). This confirms: no shader link/compile failure, no
black-frame regression, the renderer boots and draws end-to-end on the post-W1.1 engine.
Halo/particle shader modules are NOT exercised by the song_select menu scene (they need
gameplay); per PLAN.md S3 guidance, their correctness rests on S1's byte-identical `diff`
gate (identical raw-string bytes -> identical compiled module -> identical output by
construction), which is unconditionally as strong as a pixel-hash comparison for pure MOVE
commits.

**Final commit SHA list for W1.1 (all 8 commits, chronological):**

Engine repo (`milo-native-engine`):
1. `6602bc0` — externalize kRB3HaloBlitShaderSource -> rb3_halo_blit.wgsl.inc
2. `a8a90ce` — externalize kRB3PostProcShaderSource -> rb3_postproc.wgsl.inc
3. `9fc02b4` — externalize kRB3QuadShaderSource -> rb3_quad.wgsl.inc
4. `e625c80` — externalize kRB3ComposeShaderSource -> rb3_compose.wgsl.inc
5. `bd7c2f5` — externalize kRB3ParticleShaderSource -> rb3_particle.wgsl.inc

rb3 repo:
6. `56df5926` — append S1 status
7. `908a5d1f` — shader validation gtest + offline naga/tint script + CMakeLists.txt wiring
8. `7439e125` — append S2 status

`MILO_ENGINE_PIN` in `native/CMakeLists.txt` NOT touched by any W1.1 subtask (per hard rule 3).

**Exit criteria — final check:**
1. Zero inline WGSL strings in `Rnd_Wgpu_RB3.cpp` — CONFIRMED (grep empty).
2. 5 `.wgsl.inc` files + 5 `#include` sites — CONFIRMED.
3. All 5 moves byte-identical (S1 diff gate) — CONFIRMED (recorded in S1 section).
4. `rb3-native` + `rb3-tests` build green in `build-agent-W1.1` — CONFIRMED (this subtask).
5. `WgslValidation.AllRB3ShadersCompile` green + `HarnessCatchesBadShader` fail-red-proven —
   CONFIRMED (gtest-internal PASS; ctest-level SEGFAULT is the documented pre-existing
   teardown issue, not a functional failure).
6. `scripts/native/validate_wgsl.sh` skips green (no naga/tint on host) — CONFIRMED by S2,
   unchanged.
7. Post-refactor headless screenshot renders non-black — CONFIRMED (this subtask, path b).

**W1.1 status: COMPLETE.** All 3 subtasks (S1, S2, S3) done. No blockers. No deviations beyond
those already recorded in S1/S2 (compiler-override note, no manual engine-file corruption
demonstration, GPU-teardown-segfault flagged as pre-existing/out-of-scope, path-(b) screenshot
rationale above).
