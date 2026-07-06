# W1.1 — Externalize the 5 inline WGSL modules + shader validation

**Wave:** 1 · **Phase:** 1 (render-monolith decomposition, REFACTOR) · **Planner:** Opus
**Lane refs:** `REFACTOR_PLAN.md` §Phase-1 W1.1 (line 55); `01-renderer-core.md` §3 (shader census),
§5a/§5b (decomposition, migration step A1). **Disposition:** REFACTOR — pure **MOVE** commits,
byte-identical rendering. One shader per commit.

## Objective

Move the 5 inline WGSL raw-string literals in the engine file
`milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` out to file-backed shader modules loaded by the
**same compile-time `#include` mechanism** the engine already uses for `standard_wgsl.inc`, then add
offline/gtest shader validation so a WGSL syntax error is caught before runtime. This is step A1 of
the lane-01 §5b migration order: "Move the 5 inline WGSL strings to `src/gfx/Shaders/*` loaded like
`standard_wgsl.inc`. No logic change — just externalize strings and add validation."

**Nothing here changes rendered output.** Every commit is a behavior-preserving MOVE, verified
byte-identical. This satisfies the Phase-1 exit clause "zero inline WGSL strings remain."

### The 5 modules (MEASURED, `Rnd_Wgpu_RB3.cpp`, current HEAD)

| # | Symbol | Decl line | Close `)WGSL";` | Scope | Consumed at |
|---|---|---|---|---|---|
| 1 | `kRB3HaloBlitShaderSource` | 2350 | 2386 | file-scope `static const char*` | 2393 `wgsl.code =` |
| 2 | `kRB3PostProcShaderSource` | 2703 | 2897 | file-scope `static const char*` | 3439 `ppWgsl.code =` |
| 3 | `kRB3QuadShaderSource` | 3267 | 3311 | file-scope `static const char*` | 3332 `wgsl.code =` |
| 4 | `kRB3ComposeShaderSource` | 3376 | 3399 | **function-local** `static const char*` (indented 4) | 3401 `cWgsl.code =` |
| 5 | `kRB3ParticleShaderSource` | 6631 | 6665 | namespace-scope `const char*` (non-static, inside anon ns) | 6677 `src.code =` |

`grep -nE 'R"[A-Za-z_]*\(' src/platform/Rnd_Wgpu_RB3.cpp` returns EXACTLY these 5 opens — there are
no other raw-string shaders in the file. Verified.

### Faithful-reference citations (the embedding mechanism to replicate)

- `milo-native-engine/src/gfx/standard_wgsl.inc` — the reference file. Its entire content is **one
  C++ raw-string-literal token**: first bytes `R"WGSL(\n`, last bytes `)WGSL"\n`. It is **not** pure
  WGSL — it carries the raw-string wrapper.
- `milo-native-engine/src/gfx/PipelineManager.cpp:37-40` — the embedding site:
  ```cpp
  static const char* kBuiltinShaderSource =
  #include "gfx/standard_wgsl.inc"
  ;
  ```
  `#include` textually pastes the raw-string token as the initializer. This is a **compile-time**
  embed: it works identically for the native Dawn build and the Emscripten/WASM `rb3-web` build
  (no runtime fetch, no MEMFS dependency). This is the mechanism W1.1 replicates.
- Include resolution: `milo-native-engine/CMakeLists.txt:456-459` marks `src/` a PUBLIC include dir,
  so `#include "gfx/Shaders/<name>.wgsl.inc"` resolves to `src/gfx/Shaders/<name>.wgsl.inc`. Engine
  sources are an **explicit list** (not GLOB) and `.inc` files are `#include`d, **not compiled** —
  so **no `CMakeLists.txt` source-list edit is needed** for the shader files.
- Shader-compile-error detection template: `PipelineManager.cpp:261-296` — Dawn always returns a
  non-null module even on a bad shader; you must call `module.GetCompilationInfo(...)` and scan
  `info->messages[i].type == wgpu::CompilationMessageType::Error`, then
  `Instance().WaitAny(future, UINT64_MAX)`. Copy this pattern for the validation gtest.
- Headless GPU bring-up template: `rb3/native/tests/test_texsharpen.cpp:38-100` — `EnsureGpu()`
  calls `gBandRnd.InitGpu(64,64,/*headless=*/true)` once (process-global device) and
  `GTEST_SKIP() << "headless GPU device unavailable on this host"` when it returns false. Mirror it.

### Design decision — file naming & why `.wgsl.inc` (READ THIS)

The externalized files carry the **same `R"WGSL(...)WGSL"` wrapper** as `standard_wgsl.inc` and are
`#include`d the same way. Name them **`.wgsl.inc`** (e.g. `src/gfx/Shaders/rb3_halo_blit.wgsl.inc`),
**not** bare `.wgsl`, because:
- The first line is `R"WGSL(` and the last is `)WGSL"` — that is C++, not WGSL. Naming a
  wrapper-bearing file `.wgsl` would mislead editors and `naga`/`tint` into parsing the wrapper as
  WGSL. `standard_wgsl.inc` uses `.inc` for exactly this reason.
- This keeps the MOVE **byte-trivial and zero-build-change** (matches the existing precedent
  exactly), which is the lowest-risk path for byte-identical commits. Do **not** introduce CMake
  codegen or pure-`.wgsl` + wrap-generation — that adds newline-handling risk to a MOVE that must be
  byte-perfect.
- The offline validator (S2) strips the 2 wrapper lines before feeding `naga`/`tint`, so it
  validates the real shader body — and as a bonus it also validates `standard_wgsl.inc`.

REFACTOR_PLAN's `*.wgsl` wording is satisfied in spirit: the files live under `src/gfx/Shaders/`,
are loaded exactly like `standard_wgsl.inc`, and zero inline WGSL strings remain in the `.cpp`.

### Tooling probe (MEASURED 2026-07-05)

`naga --version` → not found. `tint --version` → not found. `cargo` IS present. Per the brief:
since **neither CLI is installed**, the **wgpu-based validation gtest is the primary, always-on
deliverable** (S2). The offline `naga`/`tint` script is added too but must **skip gracefully** when
the CLI is absent (so it's a no-op here yet works on a CI box that has one).

---

## Subtasks

### W1.1.S1 — Externalize the 5 WGSL modules (one commit each)  ·  model: sonnet

**Goal:** Replace each of the 5 inline `R"WGSL(...)WGSL"` literals with a `#include` of a new
`src/gfx/Shaders/<name>.wgsl.inc` file, byte-identical, **5 separate engine commits** (one shader
per commit, per the brief). Engine repo = `/home/free/code/milohax/milo-native-engine`.

**Files to touch (engine repo):**
- NEW `src/gfx/Shaders/rb3_halo_blit.wgsl.inc`
- NEW `src/gfx/Shaders/rb3_postproc.wgsl.inc`
- NEW `src/gfx/Shaders/rb3_quad.wgsl.inc`
- NEW `src/gfx/Shaders/rb3_compose.wgsl.inc`
- NEW `src/gfx/Shaders/rb3_particle.wgsl.inc`
- EDIT `src/platform/Rnd_Wgpu_RB3.cpp` (the 5 declaration sites only — see table above)
- NEW dir `src/gfx/Shaders/` (mkdir on first commit)

**Per-shader recipe (do ONE shader, commit, then the next). Example uses halo (decl 2350, close 2386):**

1. `mkdir -p src/gfx/Shaders` (first shader only).
2. Extract the **complete raw-string token** into the `.inc` file. The `.inc` file's **line 1 must
   be exactly `R"WGSL(`**, followed by the shader body lines, and its **last line exactly `)WGSL"`**
   (no trailing `;`). Concretely, from the current decl block:
   - decl line 2350 is `static const char* kRB3HaloBlitShaderSource = R"WGSL(` → the `.inc` keeps
     only the `R"WGSL(` part (drop `static const char* kRB3HaloBlitShaderSource = `).
   - body lines 2351..2385 → copied verbatim, byte-for-byte (no reflow, no whitespace edits).
   - close line 2386 is `)WGSL";` → the `.inc` keeps only `)WGSL"` (drop the trailing `;`).
   A precise way to produce it (adjust the `START,END` line numbers per shader; verify current
   numbers first with `grep -nE 'R"[A-Za-z_]*\(' src/platform/Rnd_Wgpu_RB3.cpp` and the matching
   `)WGSL";` from `grep -n ')WGSL"' ...`):
   ```bash
   cd /home/free/code/milohax/milo-native-engine
   # halo: decl 2350, close 2386
   { printf 'R"WGSL(\n'; sed -n '2351,2385p' src/platform/Rnd_Wgpu_RB3.cpp; printf ')WGSL"\n'; } \
     > src/gfx/Shaders/rb3_halo_blit.wgsl.inc
   ```
3. Replace the inline literal in `Rnd_Wgpu_RB3.cpp` (lines 2350..2386) with:
   ```cpp
   static const char* kRB3HaloBlitShaderSource =
   #include "gfx/Shaders/rb3_halo_blit.wgsl.inc"
   ;
   ```
   Preserve the exact declaration prefix per shader:
   - halo/postproc/quad: `static const char* kX =`
   - compose (#4): keep `    static const char* kRB3ComposeShaderSource =` indented; put the
     `#include` line at column 0 (preprocessor directives may sit at col 0 inside a function body);
     the trailing `;` may stay indented `    ;`.
   - particle (#5): `const char* kRB3ParticleShaderSource =` (NON-static — do not add `static`).
4. **Byte-identity gate (per shader, before committing):** prove the reconstructed string equals the
   original token from the pre-edit blob:
   ```bash
   cd /home/free/code/milohax/milo-native-engine
   # Original token (from git HEAD before this edit) — the R"WGSL(...)WGSL" span:
   git show HEAD:src/platform/Rnd_Wgpu_RB3.cpp | sed -n '2350,2386p' \
     | sed '1s/^.*= //; $s/;$//' > /tmp/w11_orig_halo.txt
   diff /tmp/w11_orig_halo.txt src/gfx/Shaders/rb3_halo_blit.wgsl.inc && echo "BYTE-IDENTICAL"
   ```
   (`1s/^.*= //` strips the `static const char* kX = ` prefix on line 1; `$s/;$//` strips the
   trailing `;`. The result must be a zero-line `diff`.) If this diff is non-empty, STOP — the move
   is not byte-identical; fix the extraction.
5. **Build gate:** compile the engine through the rb3-native target in your own build dir:
   ```bash
   cmake -B /home/free/code/milohax/rb3/native/build-agent-W1.1 -S /home/free/code/milohax/rb3/native
   cmake --build /home/free/code/milohax/rb3/native/build-agent-W1.1 --target rb3-native -j8
   ```
   (Reuse the dir on later shaders — only the changed TU recompiles.)
6. **Commit (engine repo, under flock):**
   ```bash
   flock /tmp/milo-engine-git.lock bash -c '
     cd /home/free/code/milohax/milo-native-engine &&
     git add src/gfx/Shaders/rb3_halo_blit.wgsl.inc src/platform/Rnd_Wgpu_RB3.cpp &&
     git commit -m "W1.1: externalize kRB3HaloBlitShaderSource to gfx/Shaders/rb3_halo_blit.wgsl.inc (MOVE, byte-identical)"'
   ```
   Stage ONLY these two paths. Do NOT bump `MILO_ENGINE_PIN`.
7. Repeat 2–6 for postproc (2703/2897 → `rb3_postproc.wgsl.inc`), quad (3267/3311 → `rb3_quad.wgsl.inc`),
   compose (3376/3399 → `rb3_compose.wgsl.inc`, indented local-static), particle (6631/6665 →
   `rb3_particle.wgsl.inc`, non-static). **Re-derive the START,END line numbers before each shader**
   — earlier edits shift later line numbers (each move shrinks the file). Always `grep -n` fresh.

**Verification command (whole subtask):**
```bash
grep -nE 'R"[A-Za-z_]*\(' /home/free/code/milohax/milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp
# → must print NOTHING (zero inline raw-string shaders remain)
grep -c '#include "gfx/Shaders/' /home/free/code/milohax/milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp
# → must print 5
cmake --build /home/free/code/milohax/rb3/native/build-agent-W1.1 --target rb3-native -j8   # green
```
Append a `## W1.1.S1 — done` block to STATUS.md (under `flock /tmp/rb3-docs.lock`) listing the 5
engine commit SHAs and the 5 `BYTE-IDENTICAL` confirmations.

---

### W1.1.S2 — Shader validation: wgpu gtest (primary) + offline naga/tint script (optional)  ·  model: opus

**Goal:** Catch a WGSL syntax error at build/test time instead of at runtime `CreateShaderModule`.
Deliver (a) an always-on wgpu-based validation gtest, and (b) a best-effort offline `naga`/`tint`
script that skips gracefully when the CLI is absent (it is absent on this host — MEASURED). Include a
**fail-red self-test** proving the harness actually catches a bad shader.

**Depends on:** W1.1.S1 done (the 5 `.wgsl.inc` files exist).

**Files to touch:**
- NEW `milo-native-engine/tests/... ` — do **not** add the gtest to the engine repo; the rb3 test
  runner is `rb3-tests`. Put the gtest in the **rb3 repo**: NEW
  `/home/free/code/milohax/rb3/native/tests/test_wgsl_validation.cpp`.
- EDIT `/home/free/code/milohax/rb3/native/CMakeLists.txt` — add the test file to the `rb3-tests`
  `add_executable` list (around line 705, after `test_texsharpen_manager.cpp`). **MERGE-POINT: other
  Wave-1 items also append to this list — add your line on its own line and do NOT reformat
  neighboring lines.**
- NEW `/home/free/code/milohax/rb3/scripts/native/validate_wgsl.sh` — offline naga/tint validator.

**gtest design (`test_wgsl_validation.cpp`):**
1. Mirror `test_texsharpen.cpp:38-100`'s `EnsureGpu()` (call `gBandRnd.InitGpu(64,64,true)` once;
   `GTEST_SKIP()` if false). Include `platform/Rnd_Wgpu_RB3.h` and `gfx/GpuDevice.h`.
2. Obtain the process-global `GpuDevice` (the same one `BandRnd` initialized — reachable via the
   engine's gpu singleton; see how `test_texsharpen.cpp` reaches the device / `gBandRnd`). You need
   `wgpu::Device` + `wgpu::Instance` to call `WaitAny`.
3. Write a helper `bool CompileOk(const char* wgsl, std::string& firstError)` that reproduces
   `PipelineManager.cpp:261-296`: build `ShaderSourceWGSL{ .code = wgsl }`, `CreateShaderModule`,
   `GetCompilationInfo(WaitAnyOnly, cb)`, `Instance().WaitAny(future, UINT64_MAX)`, return
   `!hasError` and capture the first Error message text.
4. Pull the 5 externalized shader strings by `#include`ing the same `.inc` files the engine uses, so
   the test validates the EXACT bytes shipped:
   ```cpp
   static const char* kHalo =
   #include "gfx/Shaders/rb3_halo_blit.wgsl.inc"
   ;   // ...and the other four, plus standard_wgsl.inc
   ```
   (rb3-tests already has the engine `src/` on its include path via `rb3_configure_target`; confirm
   `gfx/Shaders/...` resolves — if not, add `${MILO_ENGINE_DIR}/src` to the test include dirs.)
5. `TEST(WgslValidation, AllRB3ShadersCompile)` — assert `CompileOk` for each of the 6 (5 RB3 +
   `standard_wgsl.inc`); on failure `ADD_FAILURE() << name << ": " << firstError`.
6. **Fail-red self-test** `TEST(WgslValidation, HarnessCatchesBadShader)` — feed a deliberately
   broken WGSL string (e.g. `"@fragment fn f() -> @location(0) vec4f { return nonexistent_fn(); }"`)
   and `EXPECT_FALSE(CompileOk(...))`. This proves the validator can fail red, not silently pass.
   **Demonstrate it once during development** by temporarily corrupting one real shader and watching
   `AllRB3ShadersCompile` fail, then revert (note the demonstration in STATUS.md).

> **Null-backend note:** if `InitGpu` yields Dawn's null backend (`GpuDevice::IsNullBackend()`),
> Tint still runs WGSL front-end validation on `CreateShaderModule`, so `GetCompilationInfo` still
> reports real syntax errors — the test remains meaningful. Only a total device-init failure causes
> `GTEST_SKIP`. State which path ran in the test log.

**Offline script (`scripts/native/validate_wgsl.sh`):**
- Detect `naga --version` / `tint --version`; if neither present, print a clear
  `"[validate_wgsl] naga/tint not installed — skipping offline validation (gtest covers it)"` and
  `exit 0` (graceful skip — do not fail the build).
- If present: for each `src/gfx/*.inc` and `src/gfx/Shaders/*.wgsl.inc`, **strip the wrapper**
  (`sed '1{/^R"WGSL($/d}; ${/^)WGSL"$/d}'` — remove a lone `R"WGSL(` first line and lone `)WGSL"`
  last line) into a temp `.wgsl`, run `naga <tmp>.wgsl` (or `tint --validate`), and fail non-zero on
  any validation error. This validates `standard_wgsl.inc` too.
- Make it runnable standalone AND wireable as an optional CMake `add_test` (name `wgsl_offline`) that
  invokes the script — but since it skips-green without the CLI, gate the CMake test behind
  `if(EXISTS naga/tint)` OR just let it skip-green.

**Verification commands:**
```bash
cmake -B /home/free/code/milohax/rb3/native/build-agent-W1.1 -S /home/free/code/milohax/rb3/native
cmake --build /home/free/code/milohax/rb3/native/build-agent-W1.1 --target rb3-tests -j8
cd /home/free/code/milohax/rb3/native/build-agent-W1.1 && ctest -R 'WgslValidation' --output-on-failure
bash /home/free/code/milohax/rb3/scripts/native/validate_wgsl.sh   # skips green here (no naga/tint)
```
Commit gtest + CMake edit + script to the **rb3 repo** under `flock /tmp/rb3-git.lock`, staging only
those 3 paths, message prefix `W1.1:`. Append `## W1.1.S2 — done` to STATUS.md with the fail-red
demonstration result and which GPU backend path ran.

---

### W1.1.S3 — Final byte-identical verification + STATUS closeout  ·  model: sonnet

**Goal:** Prove the whole item is a byte-identical MOVE with a running-engine screenshot check, and
record the closeout. This is the belt-and-suspenders on top of S1's per-shader text diff.

**Depends on:** W1.1.S1 + S2 done.

**Steps:**
1. Confirm the code invariants:
   ```bash
   grep -nE 'R"[A-Za-z_]*\(' /home/free/code/milohax/milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp   # empty
   ls /home/free/code/milohax/milo-native-engine/src/gfx/Shaders/*.wgsl.inc   # 5 files
   ```
2. Build `rb3-native` in the W1.1 build dir (should already be built from S1/S2).
3. Headless screenshot equality vs a pre-refactor baseline. Two acceptable ways — do at least one:
   - **(a) Deterministic scene hash:** boot `RB3_HTTP=1 rb3-native` headless, drive to a fixed scene
     with the harness pattern in `scripts/native/song-select-capture.py` (menu is deterministic and
     exercises the quad/postproc/compose shaders), `GET /api/screenshot` → PNG, `sha256`. Compare to
     the same capture taken from a build of the **pre-W1.1 engine commit** (check out the parent of
     the first W1.1 engine commit into a throwaway build dir, or reuse a baseline hash captured at
     S1 start). Assert hashes equal. Note: halo/particle need gameplay; a menu scene covers
     quad/postproc/compose — for halo/particle, rely on S1's byte-diff proof (they are the same
     string bytes → same module → same output by construction) and state that in STATUS.
   - **(b)** If a stable baseline build is impractical, rely on S1's per-shader `BYTE-IDENTICAL`
     diffs (the authoritative proof — identical source bytes cannot produce different pixels) plus a
     single post-refactor screenshot to confirm the scene still renders (no black frame / no shader
     link failure).
4. Run the full `rb3-tests` suite once to confirm no regression:
   ```bash
   cd /home/free/code/milohax/rb3/native/build-agent-W1.1 && ctest --output-on-failure
   ```
5. Append `## W1.1.S3 — done` to STATUS.md (under `flock /tmp/rb3-docs.lock`) with: the screenshot
   hash comparison result (or the rationale for path (b)), the `grep`-empty confirmation, and the
   final list of all engine + rb3 commit SHAs for W1.1. No code changes in this subtask (docs only),
   so no code commit unless the screenshot harness needed a tiny scripted helper (commit that to rb3
   under flock if so).

---

## Exit criteria (measurable)

1. `grep -nE 'R"[A-Za-z_]*\(' milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` prints **nothing** —
   zero inline WGSL raw-string modules remain (Phase-1 exit clause "zero inline WGSL strings remain").
2. `ls src/gfx/Shaders/*.wgsl.inc` shows **5 files**; `grep -c '#include "gfx/Shaders/'
   Rnd_Wgpu_RB3.cpp` == **5**.
3. Each of the 5 moves proven **byte-identical** by S1's `diff` gate (the original raw-string token
   equals the reconstructed `.inc`), recorded in STATUS.md.
4. `rb3-native` and `rb3-tests` build green in `native/build-agent-W1.1`.
5. `ctest -R WgslValidation` passes: `AllRB3ShadersCompile` green (or `GTEST_SKIP` only if the host
   truly has no GPU device), and **`HarnessCatchesBadShader` demonstrably fails red** on a bad
   shader (the required fail-red demonstration) — noted in STATUS.md.
6. `scripts/native/validate_wgsl.sh` runs: skips-green here (no naga/tint), and would validate the
   wrapper-stripped shader bodies if a CLI were installed.
7. Post-refactor headless screenshot renders (non-black) and — where a baseline was captured —
   hash-equals the pre-refactor capture (S3).

## Risks / conflicts

- **Same-file collision with W0.3 (per-draw draw-log golden).** README wave list pins the ordering
  **W1.1 → W0.3** because both edit `Rnd_Wgpu_RB3.cpp`. W1.1 touches ONLY the 5 shader-declaration
  regions (lines ~2350/2703/3267/3376/6631) and only shrinks the file; W0.3 adds a draw-record ring
  around `DrawMesh` (~2670) and `EndFrame`. Land **all** W1.1 engine commits first so W0.3 rebases on
  the smaller file. If W0.3 has already landed when you start, re-derive every shader's line numbers
  with fresh `grep -n` (the table above may have shifted) and rebase; the extraction recipe is
  line-number-driven, so never trust stale numbers.
- **`native/CMakeLists.txt` rb3-tests source list is a Wave-1 merge point.** W0.1
  (`test_skin_golden.cpp`), W0.3 (`test_draw_log_golden.cpp`), W0.4 (extends
  `test_bone_ground_truth.cpp`), W0.5, and this item all append to the same `add_executable(rb3-tests
  ...)` block (~line 696-706). Add your `test_wgsl_validation.cpp` line on its own line, don't
  reformat neighbors, and serialize with `flock /tmp/rb3-git.lock`. Git will usually auto-merge
  distinct added lines; if a conflict appears, keep BOTH added lines.
- **Do NOT bump `MILO_ENGINE_PIN`** (`native/CMakeLists.txt`). The coordinator bumps it once per wave
  after all engine commits land. Your `build-agent-W1.1` builds the engine from source via
  `add_subdirectory`, so your local edits are tested without a pin bump.
- **Byte-identity is fragile to whitespace.** The `.inc` body must be copied verbatim — no editor
  auto-format, no trailing-whitespace trim, no CRLF. Use the `sed -n 'START,ENDp'` extraction, not
  hand-retyping. The S1 `diff` gate is the guard; a non-empty diff means STOP.
- **`.inc` naming vs REFACTOR_PLAN's `*.wgsl`.** Intentional (see "Design decision" above): the files
  carry the C++ raw-string wrapper exactly like `standard_wgsl.inc`, so `.wgsl.inc` is the honest
  extension and keeps the MOVE zero-build-change. Flag to coordinator; not a blocker.
- **Web build parity.** The `#include` embed is compile-time, so `rb3-web` picks up the files with no
  change — but per HARD RULE 5, do NOT run `scripts/web/build.sh`. Native build + gtest is the gate;
  web parity is structural (same `#include` path resolves under Emscripten).
