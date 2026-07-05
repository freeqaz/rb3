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
