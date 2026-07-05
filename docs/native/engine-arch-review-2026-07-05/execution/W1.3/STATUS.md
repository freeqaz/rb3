# W1.3 — Extract material→uniform binder into RB3MaterialBinder TU — STATUS

Lane A (Rnd_Wgpu_RB3.cpp): W1.2 → **W1.3** → W1.4 → W1.5 → W1.7 → W1.6.
Engine repo: /home/free/code/milohax/milo-native-engine. Commit prefix: `W1.3:`.
Plan: ./PLAN.md. Append sections below under `flock /tmp/rb3-docs.lock`.

<!-- Implementers/verifiers: one `## W1.3.S<N> — done|partial|blocked` section each,
     with commit SHAs + the screenshot sha256 + gate results (see PLAN Evidence Procedure). -->

## W1.3.S1 — done

**Commit:** engine `c43b6fd` — `W1.3: expose GetRB3TexView/UploadRndTexIfNeeded for cross-TU binder (linkage MOVE)`.
Files staged (only these two): `src/platform/RB3MaterialBinder.h` (new), `src/platform/Rnd_Wgpu_RB3.cpp`.
(`FxSendNative.cpp` — a concurrent agents uncommitted edit — left untouched, not staged, per rule 8.)

### What landed (linkage-only MOVE, zero behaviour/call-site change)
- NEW `src/platform/RB3MaterialBinder.h`: the two extern tex-helper decls
  (`GetRB3TexView`, `UploadRndTexIfNeeded`), `RB3MaterialBindResult` struct
  (`mu` + `isTextMeshHeur` + `gemForce`), and the `RB3BuildMaterialUniforms` decl
  (signature per PLAN). Declared-but-unreferenced → links fine (def is S2s job).
- `Rnd_Wgpu_RB3.cpp`: removed `static` on the `UploadRndTexIfNeeded` (now :532) and
  `GetRB3TexView` (now :801) definitions; added `#include "platform/RB3MaterialBinder.h"`
  (:25, next to the RB3MeshCache.h include) so the defs are checked vs the header decls.
- Diff is exactly `Rnd_Wgpu_RB3.cpp` (+3/-2) + the new header. No `.cpp`/CMake entry yet.

### PLAN DEVIATION (recorded per protocol)
PLAN §"Proposed interface" step 1 says include `platform/Rnd_Wgpu.h` for `MaterialUniforms`.
**That header is NOT buildable in the RB3 backend** — it `#include`s `rndobj/Rnd_NG.h`,
which does not exist anywhere in this engine tree (it is the DC3 draw-path header; the RB3
side uses `Rnd_Wgpu_RB3.h`, which pulls the `gfx/*` pieces in directly and never includes
`Rnd_Wgpu.h`). First build failed with `fatal error: rndobj/Rnd_NG.h file not found`.
**Fix:** `RB3MaterialBinder.h` includes `gfx/UniformStructs.h` (MaterialUniforms) +
`gfx/GpuDevice.h` (GpuDevice) directly, mirroring `RB3MeshCache.h` / `Rnd_Wgpu_RB3.h`.
Behaviour-neutral (same types, same defs). **S2 must include the same two gfx headers in
`RB3MaterialBinder.cpp`, NOT `Rnd_Wgpu.h`.** Also `GpuDevice` fwd-decl dropped (it is a full
include; forward-declaring `class GpuDevice;` alongside the definition is redundant).
Second-order env note: a bare `cmake -B ... -S native` defaults to g++ and dies on
`-fdelayed-template-parsing`; must configure with `-DCMAKE_C_COMPILER=/usr/bin/clang
-DCMAKE_CXX_COMPILER=/usr/bin/clang++` (matches build-native / other agent dirs).

### Build + evidence
- `cmake --build native/build-agent-W1.3 --target rb3-native -j8` → **exit 0, clean** (clang).
- Baseline lineup capture (deterministic scene, doubles as the S2 gate baseline):
  `scripts/native/patch-lineup-capture.py --bin native/build-agent-W1.3/rb3-native
  --out /tmp/w13-lineup-base --tag base` → `max_band_ratio=4.71`, 4 frames written.
  PNG sha256 (time-varying directed-cam scene, so not a bit-stable gate on its own —
  recorded for reference):
  - `base_coop_g_b_0.png`   `c255491e3d494986019261c3df632f123986105c3f3e1a4bfa5ed02aa72d62e0`
  - `base_coop_g_b_1.png`   `35f5123243b8facde4dd5b06f815b2ff5d34d19af1e3a25c9c781b14b0baf683`
  - `base_coop_g_n03_0.png` `a496de5b6776c10ac556369360bd08f09c0d320c5f2fcd650ecec200a993dda6`
  - `base_coop_g_n03_1.png` `da35ffeee9cd086b48da017730385f07ef154555075c2aedfbd9cd3a1211524c`
- `scripts/native/lineup-gate.py --bin native/build-agent-W1.3/rb3-native` →
  **`LINEUP_GATE verdict=PASS img=PASS segA=PASS ratioB=PASS countC=PASS pin=PASS`**
  (all 4 frames PASS every layer). Confirms this build renders the deterministic golden
  scene correctly; it is the authoritative MOVE gate for S2 (the lineup scene is
  song-time-driven so raw PNG sha is not bit-stable across runs — gate on the img layer
  zero-diff + segA/ratioB/countC + drawlog structural diff, per PLAN §Determinism).

### For S2
- Define `RB3BuildMaterialUniforms` in NEW `RB3MaterialBinder.cpp`; include
  `gfx/UniformStructs.h` + `gfx/GpuDevice.h` (NOT `Rnd_Wgpu.h`) + the block includes.
- Configure build-agent-W1.3 with the clang flags above (already configured this dir).
- S2 MOVE gate: rebuild + rerun `lineup-gate.py` (expect same PASS) + drawlog structural
  diff empty + `grep -c "MaterialUniforms mu{}" Rnd_Wgpu_RB3.cpp` → 0.

**Remains:** S2 (the block MOVE + .cpp + CMake), S3 (independent verify). **Blockers:** none.

## W1.3.S2 — done

**Commit:** engine `b206d44` — `W1.3: extract RB3BuildMaterialUniforms into RB3MaterialBinder TU (MOVE, byte-identical)`.
Files staged (only these three): `src/platform/RB3MaterialBinder.cpp` (new, +538), `src/platform/Rnd_Wgpu_RB3.cpp` (−503/+7), `CMakeLists.txt` (+1).
(`FxSendNative.cpp` — concurrent agents uncommitted edit — left untouched, not staged, per rule 8.)

### What landed (pure MOVE)
- NEW `src/platform/RB3MaterialBinder.cpp` defines `RB3BuildMaterialUniforms(mesh, mat, skinned, owner, gpu)`.
  The DrawMesh material→uniform region (former lines 5226–5720, **495 lines**) was relocated
  **VERBATIM**: `RB3MaterialBindResult r{}; MaterialUniforms& mu = r.mu;` opens the fn, then the
  block byte-for-byte, then `r.isTextMeshHeur = isTextMeshHeur; r.gemForce = gemForce; return r;`.
  The **only** interior edit is the sole BandRnd member ref `mGpu` → the `gpu` parameter in
  `UploadRndTexIfNeeded(gpu, dt)` (lazy diffuse upload stays inline, no reorder).
- `Rnd_Wgpu_RB3.cpp`: the region was replaced with the 4-line call + 3-line unpack
  (`RB3BuildMaterialUniforms(mesh, mat, skinned, owner, mGpu)` → `mu`/`isTextMeshHeur`/`gemForce`).
  The following `slot.matUB`/`WriteBuffer`/`ResolveMaterialViews`/`MakeMaterialBindGroupCached`
  plumbing and the `PipelineKey` blend/zmode block are **untouched** (W1.6 owns bind groups).
- `CMakeLists.txt`: `src/platform/RB3MaterialBinder.cpp` added to `MILO_ENGINE_GPU_PLATFORM_SOURCES_RB3`.
- Includes in the new .cpp: `RB3MaterialBinder.h` + rndobj `Cam/Mesh/Mat/Tex/Trans/Dir/Env.h` +
  `math/Mtx.h` + `<algorithm> <cmath> <cstdio> <cstdlib> <cstring> <string> <unordered_map>`
  (the subset the block uses; NOT `Rnd_Wgpu.h`, per S1 deviation note).

### PLAN adherence — no deviation
- Bind-group plumbing + PipelineKey blend/zmode left in DrawMesh (W1.6/pipeline scope). ✓
- Asset-name special-cases (crowd-dim, `tail_*`, `surface.mat`/`rails.mat`/`gem_smasher_glow`/
  `peakstate`, `gem_force`, `highlight_*`, `*_skin_diffuse`/`head.mesh` probes) relocated UNCHANGED. ✓
- Block-local `static` caches (`hubFixOff`, `sCrowdDimOff`, `sCrowdDim`, `sTrackLight`, `sWmOff`,
  `sWmDim`, `sFretGlowOff`) moved with the code (function-local; zero shared-state concern). ✓

### Build + evidence (all PASS)
- `cmake --build native/build-agent-W1.3 --target rb3-native -j8` → **exit 0, clean** (both
  `Rnd_Wgpu_RB3.cpp.o` and `RB3MaterialBinder.cpp.o` compiled; libmilo-engine.a + rb3-native linked).
- `cmake --build native/build-agent-W1.3 --target rb3-tests -j8` → **exit 0, clean** (rb3-tests linked).
- **Byte-identity proof (authoritative for a MOVE):** the 495-line moved block in
  `RB3MaterialBinder.cpp` is **byte-identical** to `c43b6fd:Rnd_Wgpu_RB3.cpp` lines 5226–5720 after
  applying the single `mGpu`→`gpu` edit (python line-by-line compare → 0 diffs).
- **MOVE gate — `scripts/native/lineup-gate.py --bin native/build-agent-W1.3/rb3-native`** →
  **`LINEUP_GATE verdict=PASS img=PASS segA=PASS ratioB=PASS countC=PASS pin=PASS`** (all 4 frames
  PASS every layer: coop_g_n03 [0,1], coop_g_b [0,1]; img sim 69.97–82.35). Identical verdict to the
  S1 baseline against the committed `scripts/native/goldens/w0.5-lineup/` golden — the deterministic
  MOVE gate per PLAN §Determinism (lineup scene is song-time driven so raw PNG sha is not bit-stable;
  gate on img zero-diff + segA/ratioB/countC layers). RB3_DRAWLOG is blind to `mu` VALUES (PLAN §72),
  and the W0.3 drawlog golden is not yet stable (W0.3 partial), so the lineup img layer is the gate.
- `grep -c "MaterialUniforms mu{}" src/platform/Rnd_Wgpu_RB3.cpp` → **0** (exit criterion 2). ✓

### For S3
- Independent verify: clean-build both targets at `b206d44`; re-run `lineup-gate.py` (expect same
  PASS); build+run `milo-engine-tests` to confirm W0.1 skin golden + W0.4 ClipPoseFixture green
  (29 documented dc3-drift failures expected, not attributable to W1.3); spot-check the moved
  special-cases via `git show b206d44` are relocation-only (already proven byte-identical here).

**Remains:** S3 (independent verify + milo-engine-tests skin/pose greens). **Blockers:** none.

## W1.3.S3 — done

**Independent verifier.** No source edits; author-independent re-derivation of S1/S2's
claims plus the full Evidence Procedure. Verified at engine commits `c43b6fd` (S1),
`6f9d340` (parent-of-S1, i.e. W1.2 tip), and `b206d44` (S2, current engine HEAD).

### Build (own dirs; shared trees untouched)
- Reused `native/build-agent-W1.3` (already at engine HEAD `b206d44`) — clean rebuild:
  `cmake --build . --target rb3-native -j8` → exit 0; `--target rb3-tests -j8` → exit 0.
- Parent-of-S1 comparison build: an isolated **read-only `git worktree`** at
  `/tmp/engine-at-6f9d340` (removed after use — mirrors the W0.3b verifier precedent;
  no `git reset/rebase/checkout--/restore` on the shared `milo-native-engine` tree, per
  hard rule 7), configured as a fresh own-dir
  `native/build-agent-W1.3-verify-parent` (`-DMILO_ENGINE_PATH=/tmp/engine-at-6f9d340`,
  clang toolchain matching S1's documented flags) → `rb3-native` + `rb3-tests` both
  built clean (expected soft-pin `message(WARNING)` re: `MILO_ENGINE_PIN` mismatch —
  by design, not an error). Both scratch dirs removed after evidence capture.

### Evidence Procedure (parent-of-S1 vs S2)
1. **Lineup-gate PASS on both builds** (authoritative MOVE gate per PLAN, since raw
   scene PNG sha is not bit-stable — song-time-driven camera, confirmed again here:
   captured `songMs` differed run-to-run on the *same* binary, e.g. 21080 vs 21014ms):
   - parent (`native/build-agent-W1.3-verify-parent`):
     `LINEUP_GATE verdict=PASS img=PASS segA=PASS ratioB=PASS countC=PASS pin=PASS`
     (4/4 frames, img sim 75.19-86.83).
   - S2 (`native/build-agent-W1.3`):
     `LINEUP_GATE verdict=PASS img=PASS segA=PASS ratioB=PASS countC=PASS pin=PASS`
     (4/4 frames, img sim 38.38-52.42 — different absolute sim scores are expected,
     different camera timing per run; both PASS every layer against the committed
     `scripts/native/goldens/w0.5-lineup/` golden). Identical verdict shape to S1/S2's
     own recorded runs.
2. **RB3_DRAWLOG structural diff — investigated, found NOT usable as a strict
   cross-build gate here (pre-existing, characterized, not a W1.3 issue):** captured
   `RB3_FIXED_CLOCK` splash-scene drawlog dumps (60 frames, `setarch -R`, stabilizer
   env, via `scripts/native/drawlog-golden.py`'s `capture_fixed_clock`) directly from
   both the parent-of-S1 and S2 binaries and diffed with the unchanged
   `compare_drawlogs()`: 709 field-level differences (world-xfm float noise + a
   co-located-mesh draw-order permutation cluster around draws 33-37). **Sanity check:**
   ran the SAME parent-of-S1 binary against itself twice (two independent captures) —
   **291 differences between two runs of the IDENTICAL binary**, of the same
   character (small world-xfm float drift + draw-order permutation). This reproduces
   W0.3b's own documented finding (run-to-run process nondeterminism on an identical
   binary, 112-260 divergences) — i.e. the splash-scene fixed-clock drawlog is
   demonstrably **not** a byte-identical comparator at this granularity yet (W0.3
   remains partial per Wave-1 results table), so a nonzero diff here is expected noise,
   not evidence of a W1.3 regression. Per PLAN's own guidance ("if any residual jitter,
   gate on pixelwise-identical + lineup-gate PASS rather than raw boot-frame sha"),
   the lineup-gate PASS above is the controlling evidence; the drawlog check is
   recorded here only as due diligence, not as a failed gate.
3. **Byte-identity of the moved block — independently re-derived (not just trusting
   S2's own claim):** extracted `Rnd_Wgpu_RB3.cpp` lines 5225-5719 from `6f9d340`
   (the region between `MaterialUniforms mu{};` and the
   `// Per-(mesh,instance) persistent...` end comment, 495 lines) and the function body
   of `RB3BuildMaterialUniforms` in `b206d44:src/platform/RB3MaterialBinder.cpp`
   (495 lines), applied the single documented `mGpu`->`gpu` substitution to the old
   side, and diffed line-by-line in Python: **0 differences.** Confirms S2's claim
   independently.
4. **Asset-name special-case spot-check (relocation-only)** — grep counts of the
   PLAN-cited markers inside the moved region, old vs new, all equal:
   `crowd` 14/14, `tail_` 6/6, `surface.mat` 3/3, `gem_smasher_glow` 3/3,
   `peakstate` 3/3, `highlight_main` 4/4, `GEM_FORCE` 2/2.
5. **Exit criteria 1-2 reconfirmed:** `grep -c "MaterialUniforms mu{}" src/platform/Rnd_Wgpu_RB3.cpp`
   -> `0`; `CMakeLists.txt:323` lists `src/platform/RB3MaterialBinder.cpp` in
   `MILO_ENGINE_GPU_PLATFORM_SOURCES_RB3`.

### milo-engine-tests (safety net)
- Reused `milo-native-engine/build-agent-W0.1` (per-item build-dir convention already
  established by W0.1), reconfigured/rebuilt `milo-engine-tests` at current engine HEAD
  `b206d44` (gpu backend flavor = `dc3` for this target, as expected — RB3MaterialBinder.cpp
  is gated into the `rb3`-flavor source list only, so this target does not compile it
  directly; it is the cross-cutting safety net for the shared engine).
- Targeted filter `SkinGolden.*:ClipPoseFixture.*` (`DC3_DATA`/`MILO_LIB` pointed at
  `dc3-decomp/orig-assets{,/extracted}`): **16/16 ran, 15 passed, 1 skipped by design**
  (`SkinGolden.CaptureGolden`). All 12 `ClipPoseFixture.*` tests green, including
  `EffectorWorldPositionsMatchGolden` (W0.4) and the 3 non-skipped `SkinGolden.*` (W0.1).
- Full-suite `ctest -j1 --output-on-failure`: **200 total, 100% passed, 0 failed**
  (2 intentionally-skipped: `ExtractBik.ExtractSmallest`, `SkinGolden.CaptureGolden`).
  This is a clean improvement over W0.1.S2's baseline (169 passed / 29 failed /
  2 skipped) — the 4 pre-existing dc3-drift failure buckets W0.1 documented are gone,
  consistent with the `W2-TESTFIX` lane commits already on engine HEAD
  (`0dab386` Dir-merge subdir-ownership oracle fix, `49c3f38` compressed-skin test
  oracle fix) plus whatever landed for the XMA-sidecar-namespace and
  `CharBones::ScaleAdd` buckets — not investigated further here (out of W1.3 scope;
  good news for the suite, not attributable to W1.3's own commits since
  `RB3MaterialBinder.cpp` isn't in this target's source list).

### Repo hygiene
- Confirmed shared trees otherwise clean: `milo-native-engine` working tree shows only
  the pre-existing, documented concurrent-agent edit to `src/platform/FxSendNative.cpp`
  (untouched, per rule 8). No `git reset/rebase/checkout--/restore` run on any shared
  tree. Scratch artifacts (`/tmp/engine-at-6f9d340` worktree,
  `native/build-agent-W1.3-verify-parent`) removed after use.

### Verdict
All PLAN.md exit criteria (1-7) confirmed independently. W1.3 (S1+S2+S3) is **done**.

**Commits:** none (verification-only; this STATUS.md append is the only artifact).
**Remains:** nothing for W1.3. **Blockers:** none.
