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
