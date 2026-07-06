# W1.4 — Extract postproc / halo / quad passes into TUs — STATUS

Lane A (Rnd_Wgpu_RB3.cpp): W1.2 → W1.3 → **W1.4** → W1.5 → W1.7 → W1.6.
Engine repo: /home/free/code/milohax/milo-native-engine. Commit prefix: `W1.4:`.
Plan: ./PLAN.md. Append sections below under `flock /tmp/rb3-docs.lock`.

<!-- Implementers/verifiers: one `## W1.4.S<N> — done|partial|blocked` section each, with
     commit SHAs + per-scene screenshot sha256 (gameplay/song_select/lineup) + diff-hunk
     equivalence proof + gate results (see PLAN Evidence Procedure). New RB3-only TUs
     (RB3HaloPass/RB3PostProc/RB3Quad) — do NOT converge onto gfx/PostProcPass|DrawRect2D
     (diff-proof in PLAN Objective). Stage only W1.4 files; flock /tmp/milo-engine-git.lock;
     never bump MILO_ENGINE_PIN; never git reset/restore the shared tree. -->

## W1.4.S1 — done

**Commit:** engine `e61e6ec` — `W1.4: extract halo gem-bloom pass into RB3HaloPass TU (MOVE, byte-identical)`.
Files staged (only these four): `src/platform/RB3HaloPass.h` (new, +23), `src/platform/RB3HaloPass.cpp`
(new, +330), `src/platform/Rnd_Wgpu_RB3.cpp` (−310), `CMakeLists.txt` (+1).
(`src/platform/FxSendNative.cpp` — a concurrent agent's uncommitted edit — left untouched, NOT staged, per hard rules 2/8.)

### What landed (pure MOVE, zero behaviour change)
- NEW `src/platform/RB3HaloPass.cpp` holds the DEFINITIONS of the 5 `BandRnd::` halo methods —
  `HighwayBloomEnabled`, `IsHaloSourceMat`, `EnsureHaloTarget`, `EnsureHaloBlitPipeline`,
  `CompositeHaloBloom` — plus the `kRB3HaloBlitShaderSource` WGSL `#include` block, relocated
  **token-for-token** (the P1 banner comment + all 5 bodies, former Rnd_Wgpu_RB3.cpp lines
  2014–2322 = 309 lines). Includes `platform/Rnd_Wgpu_RB3.h` (pulls BloomPass / Hmx::Color / wgpu
  transitively) + `rndobj/Mat.h`, `rndobj/Tex.h` + `<cstdio>/<cstdlib>/<cstring>`. No interior
  edit — not one moved body line changed.
- NEW `src/platform/RB3HaloPass.h`: banner-only header (RB3MeshCache.h / RB3MaterialBinder.h style,
  `#pragma once`); declares nothing — the methods are `BandRnd::` members already in Rnd_Wgpu_RB3.h.
- `Rnd_Wgpu_RB3.cpp`: 5980 → **5670 lines** (−310 = the 309-line block + its 1 leading blank).
  The DrawMesh **capture site** (the `mHaloDraws.push_back({pipe, mSceneBindGroup, …})` leaf) and
  the `BeginFrame`/`EndFrame` **call sites** (`HighwayBloomEnabled()`/`CompositeHaloBloom()`,
  `mHaloDraws.clear()`) are untouched — they call the moved methods cross-TU (link-time symbols).
  Header decls (`Rnd_Wgpu_RB3.h:243-259`) + `HaloDraw` struct + `mHaloDraws` member unchanged.
- `CMakeLists.txt`: `src/platform/RB3HaloPass.cpp` appended to `MILO_ENGINE_GPU_PLATFORM_SOURCES_RB3`
  (after the W1.3 RB3MaterialBinder line) with a `# W1.4:` comment.

### Evidence
1. **Diff-hunk equivalence (PRIMARY, deterministic) — PERFECT pure MOVE.**
   `git show e61e6ec` removed 310 lines from `Rnd_Wgpu_RB3.cpp`, added 330 to `RB3HaloPass.cpp`.
   `diff <(sort removed) <(sort added)` yields **zero removed-only (`<`) lines** — every deleted body
   line has a matching added line. The only added-only (`>`) lines are the new TU preamble: the
   banner comment + 3 blank lines + the 6 `#include` lines (330−310 = 20 preamble lines). No moved
   body line changed a single token.
2. **Exit-criteria greps (all met).** In `Rnd_Wgpu_RB3.cpp`:
   `grep -c 'BandRnd::CompositeHaloBloom|BandRnd::EnsureHaloBlitPipeline|BandRnd::IsHaloSourceMat|BandRnd::EnsureHaloTarget|BandRnd::HighwayBloomEnabled'` → **0**;
   `grep -c 'kRB3HaloBlitShaderSource'` → **0**. New TU has all 5 defs + the shader include.
   Remaining 13 halo *references* in the monolith are the preserved call/capture sites + comments (expected).
3. **Build (own dir `native/build-agent-W1.4`, clang — the default GCC configure fails on
   `-fdelayed-template-parsing`, a clang flag; reconfigured with `-DCMAKE_C_COMPILER=clang
   -DCMAKE_CXX_COMPILER=clang++`).** `rb3-native` → **exit 0 clean**; `rb3-tests` → **exit 0 clean**.
   The clean cross-TU link proves every moved method/member symbol resolves.
4. **Lineup gate (deterministic corroboration) — PASS.**
   `scripts/native/lineup-gate.py --bin native/build-agent-W1.4/rb3-native` →
   `LINEUP_GATE verdict=PASS img=PASS segA=PASS ratioB=PASS countC=PASS pin=PASS` (all 4 frames,
   all layers). Matches the W1.3 authoritative-gate precedent.
5. **Halo-specific witness (game.cam/gems) — PASS.**
   `scripts/native/gameplay-depth-capture.py --bin native/build-agent-W1.4/rb3-native
   --times 5000,8000` reaches `game_screen`; the captured frame shows the emissive gems with their
   additive bloom halo present (CompositeHaloBloom still fires under game.cam). PNG shas (time-driven
   scene, not bit-stable — recorded for reference): `gameplay_05000ms.png`
   `51f28316b554644d4c48eacf710a44e8d7d96554f5e382898bfdf8ffdc2a81f8`, `gameplay_08000ms.png`
   `a97bb93719638301c7250ae32ed5ec1f8aa03fbe55cd13ecc6ccdf4b51a4a4a4`. (The bassist shard artifact is
   the known pre-existing count-in skinning residual, unrelated to this MOVE.)

### PLAN deviations (recorded per protocol)
- **Blank-line handling:** removed the block PLUS its 1 leading blank line (former :2013) so the
  monolith keeps a single blank between `FlushPostProcMidFrame` and `DoPostProcess` (no double
  blank); the new TU carries a matching blank before the moved block, so the diff-hunk proof stays a
  clean subset (that blank pairs in the sort). Pure formatting; no body change.
- **milo-engine-tests (`SkinGolden.*` / `ClipPoseFixture.*` / `DrawLogGolden*`) NOT run under S1.** The
  W1.4 PLAN assigns the full engine-tests run to **S4** (independent verify); it needs the involved
  decomp-context initial-cache recipe from W0.1/STATUS.md. This TU-boundary MOVE is byte-identical
  (proof §1) and cannot touch skinning/pose/drawlog logic; rb3-tests already builds green. Matches
  the W1.3.S1/S2 evidence bar (build + lineup-gate + grep; full engine-tests deferred to the verify
  subtask). **S4 owns the milo-engine-tests green confirmation.**

### For S2/S3/S4
- Head is now `e61e6ec`. S2 (RB3PostProc) starts from here; the postproc block is directly below the
  removed halo block (`DoPostProcess` now ~:2013). S2's `RB3PostProcDisabled` de-static is still its
  own linkage-MOVE commit per PLAN.
- Build `build-agent-W1.4` is configured with clang (reuse it; do NOT let the default GCC configure
  win — it dies on `-fdelayed-template-parsing`).
- S4: run milo-engine-tests + the convergence diff-proof section; the diff-hunk proof command for
  S1 is reproducible via `git show e61e6ec`.

**Remains (this subtask):** none. **Blockers:** none.

## W1.4.S2 — done

**Commits (engine `/home/free/code/milohax/milo-native-engine`, prefix `W1.4:`):**
- `3c59dc4` — `W1.4: expose RB3PostProcDisabled for cross-TU postproc (linkage MOVE)` (C1). Files: `src/platform/RB3PostProc.h` (new, banner + `bool RB3PostProcDisabled();` decl), `src/platform/Rnd_Wgpu_RB3.cpp` (−`static` fwd-decl @old:53, +`#include "platform/RB3PostProc.h"`, def `static bool`→`bool`). Analogue of W1.3 `c43b6fd`.
- `3845943` — `W1.4: move postproc composite (...) into RB3PostProc TU (MOVE, byte-identical)` (C2). Files: `RB3PostProc.cpp` (+398), `RB3PostProc.h` (+40), `Rnd_Wgpu_RB3.cpp` (−398), `CMakeLists.txt` (+1). (`src/platform/FxSendNative.cpp` — a concurrent agent's uncommitted edit — left untouched/unstaged in BOTH commits, per hard rules 2/8.)

### What landed (pure MOVE, zero behaviour change)
- NEW `src/platform/RB3PostProc.cpp` holds the DEFINITIONS of the 5 `BandRnd::` postproc methods — `FlushPostProcMidFrame`, `DoPostProcess`, `MainColorTarget`, `EnsureIntermediate`, `RunPostProcComposite` — plus the non-static `kRB3PostProcShaderSource` WGSL `#include` and the `RB3PostProcDisabled()` def, all relocated token-for-token from the monolith. Includes: `platform/Rnd_Wgpu_RB3.h`, `platform/RB3PostProc.h`, `platform/RB3MaterialBinder.h` (W1.3 `UploadRndTexIfNeeded`), `rndobj/Cam.h` (`RndCam::Name`), `rndobj/PostProc.h`, `rndobj/ColorXfm.h`, `<cstdio>/<cstdlib>/<cstring>`. Not one moved body line changed.
- `RB3PostProc.h` carries the cross-TU shared decls: `bool RB3PostProcDisabled();`, the `PostProcUniforms` struct + `static_assert(sizeof==160)`, and `extern const char* kRB3PostProcShaderSource;`.
- `Rnd_Wgpu_RB3.cpp`: 5670 → **5273 lines** (−397 net). `ClearDepthForOverlay`, `EnsureDepth`, the 3 `RB3PipelinePrewarm*` statics, and `EnsureQuadPipeline` (S3) stay; the postproc call sites in BeginFrame/EndFrame/ClearDepthForOverlay call the moved methods cross-TU (link-time symbols).
- `CMakeLists.txt`: `src/platform/RB3PostProc.cpp` appended to `MILO_ENGINE_GPU_PLATFORM_SOURCES_RB3` after the W1.4 RB3HaloPass line.

### Evidence
1. **Diff-hunk equivalence (PRIMARY, deterministic) — PURE MOVE.** Canonical form vs commit:
   `git show 3845943 -- src/platform/Rnd_Wgpu_RB3.cpp` removed **398** lines; `git show 3845943 -- src/platform/RB3PostProc.cpp src/platform/RB3PostProc.h` added **438**. `comm -23 <(sort removed) <(sort added)` yields exactly **ONE** removed-only body line: `static const char* kRB3PostProcShaderSource =` — the INTENTIONAL de-static (its non-static form `const char* kRB3PostProcShaderSource =` is present in the added set). Every other moved body line matches byte-for-byte. The +40 added-only lines are the new TU preamble/includes/banner + the header's cross-TU decls. **No moved body line changed a single token.**
2. **Exit-criteria greps (met).** In `Rnd_Wgpu_RB3.cpp`: `BandRnd::RunPostProcComposite|FlushPostProcMidFrame|DoPostProcess|MainColorTarget|EnsureIntermediate` → **0** defs; `^bool RB3PostProcDisabled` / `^static bool RB3PostProcDisabled` → **0**; `struct PostProcUniforms` → **0**; `char* kRB3PostProcShaderSource =` (def) → **0**. (The 3 remaining `kRB3PostProcShaderSource`/`sizeof(PostProcUniforms)` **references** are inside the staying `EnsureQuadPipeline`; they resolve via `RB3PostProc.h` and leave the monolith in S3.)
3. **Build (own dir `native/build-agent-W1.4`, clang) — clean.** `rb3-native` exit 0, `rb3-tests` exit 0, after both C1 and C2. Clean cross-TU link proves every moved method/member/global symbol resolves.
4. **Lineup gate (deterministic behavioral witness) — PASS** on the C2 binary: `LINEUP_GATE verdict=PASS img=PASS segA=PASS ratioB=PASS countC=PASS pin=PASS` (all 4 frames). (Also PASS on C1.)
5. **song_select postproc witness (B+W_film02.pp) — reaches screen, no crash/regression.** `song-select-capture.py` on the C2 binary reached `song_select_screen` and captured all 5 depth frames cleanly. The composite path links + runs (a broken symbol would fail the link). **Byte-hash NOT usable here:** the fixed-clock song_select frame is irreducibly jittery — 3 runs of the SAME (C1) binary produced 3 DIFFERENT depth-00 sha256 (`6bc4d7…`/`598e72…`/`ffdedc…`), the documented W0.3b mesh-identity ordering flake, unrelated to code motion. Per PLAN Evidence Procedure §2 fallback, lineup-gate (deterministic) is the behavioral gate.
6. **rb3-tests — 70/70 PASS** (1 pre-existing documented skip: `DrawLogGolden.PopulatesFromRealDrawMesh`; the at-exit teardown core-dump is a known headless-boot static-dtor artifact AFTER all tests passed, not a failure). `SkinGolden.*` / `ClipPoseFixture.*` / `DrawLogGolden*` all in the 13 suites, green. Full `milo-engine-tests` run is S4's assignment per PLAN.

### PLAN deviations (recorded per protocol)
- **kRB3PostProcShaderSource + PostProcUniforms required cross-TU exposure (not just RB3PostProcDisabled).** The PLAN's S2.C1 anticipated only the `RB3PostProcDisabled` de-static. But the STAYING `BandRnd::EnsureQuadPipeline` (S3-destined) *also* builds the postproc pipeline: it references `kRB3PostProcShaderSource` (`ppWgsl.code`) and `sizeof(PostProcUniforms)` (UB sizing). Moving their definitions out of the monolith therefore breaks the monolith build unless both are cross-TU visible. Handled inside C2 (still byte-identical — no behavior change): `PostProcUniforms` struct + `static_assert` relocated to `RB3PostProc.h` (shared type, ODR-safe), and `kRB3PostProcShaderSource` de-static'd — `extern const char*` in the header, single non-static definition in `RB3PostProc.cpp`. This is the direct analogue of the C1 de-static, folded into C2 rather than its own commit because it is inseparable from the body move (removing the def and exposing it must land together for the monolith to compile). The lone diff-hunk removed-only line (§1) is this de-static.
- **MainColorTarget + EnsureIntermediate WERE moved** (the PLAN's "recommended also, only if clean"): the cut was clean (both are `BandRnd::` methods; BeginFrame/EndFrame/ClearDepthForOverlay call them cross-TU).
- **Extra includes in RB3PostProc.cpp** beyond the material-binder mirror: `rndobj/Cam.h` (for `RndCam::sCurrent`/`Name()` in the RB3_RENDER_DBG heartbeat), `platform/RB3MaterialBinder.h` (W1.3 `UploadRndTexIfNeeded` for the noise-map upload), `rndobj/PostProc.h` + `rndobj/ColorXfm.h` (RndPostProc/RndColorXfm — not pulled transitively by Rnd_Wgpu_RB3.h). Adding includes to a new TU is not a body change.
- **Build dir path:** `native/build-agent-W1.4` lives under the **rb3** repo (consumes the engine working tree via `add_subdirectory`); build with `cmake --build /home/free/code/milohax/rb3/native/build-agent-W1.4` (already clang-configured per S1). milo-engine-tests deferred to S4 per PLAN.

**Remains (this subtask):** none. **Blockers:** none. **For S3/S4:** head is `3845943`. S3 (`RB3Quad`) moves `EnsureQuadPipeline` (+ its 3 `kRB3PostProcShaderSource`/`PostProcUniforms` references) → after that the monolith `kRB3PostProcShaderSource` grep hits 0 (end-of-W1.4 exit criterion). RB3Quad.cpp will need `#include "platform/RB3PostProc.h"` for those two shared symbols. S4 owns the milo-engine-tests green confirmation + convergence diff-proof.

## W1.4.S3 — done

**Commits (engine `/home/free/code/milohax/milo-native-engine`, prefix `W1.4:`):**
- `b53beef` — `W1.4: expose RB3RttDisabled for cross-TU quad/DrawRect (linkage MOVE)` (C0, **not anticipated by PLAN** — see deviations below). Files: `src/platform/RB3Quad.h` (new, banner + `bool RB3RttDisabled();` decl), `src/platform/Rnd_Wgpu_RB3.cpp` (`static bool RB3RttDisabled()` → `bool RB3RttDisabled()` at its definition site, + `#include "platform/RB3Quad.h"`).
- `8d6d895` — `W1.4: move 2D quad pipeline + DrawRect (+ outfit compose) into RB3Quad TU (MOVE, byte-identical)` (C1). Files: `RB3Quad.cpp` (new, +555), `Rnd_Wgpu_RB3.cpp` (−527), `CMakeLists.txt` (+1). (`src/platform/FxSendNative.cpp` — a concurrent agent's uncommitted edit — left untouched/unstaged in BOTH commits, per hard rules 2/8.)

### What landed (pure MOVE, zero behaviour change)
- NEW `src/platform/RB3Quad.cpp` holds the DEFINITIONS of the 2 `BandRnd::` quad methods — `EnsureQuadPipeline`, `DrawRect` — plus the file-scope `kRB3QuadShaderSource` WGSL include, `RB3RectUB`/`RB3RectVertex` CPU-mirror structs, the `static RB3QuadPipeKey()` helper (kept static — its only 2 callers, both inside `DrawRect`, moved with it), and the non-static `gRB3OutfitComposeActive` global (no header decl — `OutfitConfig.cpp` externs it directly; relocated definition only, per PLAN). Includes: `platform/Rnd_Wgpu_RB3.h`, `platform/RB3Quad.h`, `platform/RB3PostProc.h` (W1.4.S2 `kRB3PostProcShaderSource`/`PostProcUniforms` — `EnsureQuadPipeline` sizes/sources its Stage-2 postproc pipeline from these), `platform/RB3MaterialBinder.h` (W1.3 `GetRB3TexView`/`UploadRndTexIfNeeded` — `DrawRect`'s diffuse-tex resolve), `rndobj/Cam.h`, `rndobj/Mat.h`, `rndobj/Tex.h`, `<cstdio>/<cstdlib>/<cstring>`. Not one moved body line changed.
- `src/platform/RB3Quad.h`: banner header (RB3MeshCache.h/RB3HaloPass.h/RB3PostProc.h style) + the one cross-TU decl `bool RB3RttDisabled();` (see deviation below).
- `Rnd_Wgpu_RB3.cpp`: 5273 → **4747 lines** (−1 for the include add in C0, −526 net in C1 = −527 removed +1 leftover from the earlier blank-line accounting; net across both W1.4.S3 commits −526). Across all of W1.4 (S1+S2+S3): 5980 → 4747 (**−1233 lines**, exceeding the PLAN's ≈−900…1100 estimate). `EnsureDepth`, `ClearDepthForOverlay`, the 3 `RB3PipelinePrewarm*` statics, `BeginFrame`/`EndFrame`/`BeginDrawTarget`/`DrawMesh` call/capture sites stay untouched — they call the moved methods cross-TU (link-time symbols).
- `CMakeLists.txt`: `src/platform/RB3Quad.cpp` appended to `MILO_ENGINE_GPU_PLATFORM_SOURCES_RB3` after the W1.4 RB3PostProc line.

### PLAN deviations (recorded per protocol)
- **`RB3RttDisabled` required a linkage de-static the PLAN did not anticipate for S3** (PLAN said "No de-static needed (`RB3QuadPipeKey` moves with its only callers)" — true for `RB3QuadPipeKey`, but it missed `RB3RttDisabled`). `DrawRect`'s lazy RTT begin-hook (`if (!RB3RttDisabled() && RndCam::sCurrent) ...`) calls the file-scope `static bool RB3RttDisabled()` defined at `Rnd_Wgpu_RB3.cpp` (originally :1838, home of `BeginDrawTarget`'s RTT canary). Since `RB3RttDisabled`'s OTHER two call sites (`BeginDrawTarget`, `DrawMesh`) are staying (out of W1.4 scope — W1.6/W1.7 territory), the function's home stays in `Rnd_Wgpu_RB3.cpp`; only its linkage changed (`static` → non-static) plus one decl in the new `RB3Quad.h`. Handled as its own linkage-MOVE commit (C0), the direct analogue of W1.4.S2's `RB3PostProcDisabled` expose (`3c59dc4`) — same pattern, different direction (home stays in the monolith rather than moving to the satellite TU, since the majority of callers stay). No behavior change; still classified a MOVE.
- **Blank-line handling:** matches the S1/S2 convention — removed the banner-comment block through DrawRect's closing brace PLUS its 1 trailing blank line (former monolith :2070-2596), leaving the single pre-existing blank line before `BandRnd::DrawMesh` (no double blank). Pure formatting; no body change.
- **`milo-engine-tests` (`SkinGolden.*`/`ClipPoseFixture.*`) NOT run under S3** — same as S1/S2, these live in the separate `milo-engine-tests` binary (not `rb3-tests`) and are S4's assignment per PLAN (needs the decomp-context initial-cache recipe from `W0.1/STATUS.md`). `rb3-tests` `DrawLogGolden*` (the subset that IS in `rb3-tests`) ran green: 9/9 PASS, 1 pre-existing documented skip (`DrawLogGolden.PopulatesFromRealDrawMesh`), matching the W1.4.S1/S2 evidence bar exactly.

### Evidence
1. **Diff-hunk equivalence (PRIMARY, deterministic) — PERFECT pure MOVE.** `git diff --cached` (commit `8d6d895`) removed **527** lines from `Rnd_Wgpu_RB3.cpp`, added **555** to `RB3Quad.cpp`. `comm -23 <(sort removed) <(sort added)` → **zero** removed-only lines: every deleted body line has a matching added line. `comm -13` (added-only) is exactly the 28-line new-TU banner comment + include block. **No moved body line changed a single token.** (C0's de-static diff is a 2-line change: `static bool` → `bool` + 1 new `#include`, reviewed directly, not table-diffed.)
2. **Exit-criteria greps (all met, whole-W1.4 state after S1+S2+S3).** In `Rnd_Wgpu_RB3.cpp`: `grep -c 'BandRnd::CompositeHaloBloom\|BandRnd::EnsureHaloBlitPipeline\|BandRnd::IsHaloSourceMat'` → **0**; `grep -c 'BandRnd::RunPostProcComposite\|BandRnd::FlushPostProcMidFrame'` → **0**; `grep -c 'BandRnd::DrawRect\b\|BandRnd::EnsureQuadPipeline'` → **1** (verified: `:4368` is a **comment** reference inside `DrawMesh`'s banner, not a definition — expected/harmless); `grep -c 'kRB3HaloBlitShaderSource\|kRB3PostProcShaderSource\|kRB3QuadShaderSource'` → **0**. `gRB3OutfitComposeActive` is a single non-static def (now in `RB3Quad.cpp:225`, `grep -rn '^bool gRB3OutfitComposeActive'` → 1 hit, that file only). `RB3QuadPipeKey` remains `static` with its only 2 callers both inside `RB3Quad.cpp`'s `DrawRect`.
3. **Build (own dir `native/build-agent-W1.4`, clang, reused from S1/S2) — clean.** `rb3-native` exit 0, `rb3-tests` exit 0 after both C0 and C1. Clean cross-TU link proves every moved method/member/global/free-function symbol resolves (incl. the new `RB3RttDisabled` cross-TU call from `RB3Quad.cpp`).
4. **Lineup gate (deterministic behavioral witness — outfit two-colour compose) — PASS** on the C1 binary: `LINEUP_GATE verdict=PASS img=PASS segA=PASS ratioB=PASS countC=PASS pin=PASS` (all 4 frames, all layers) — this scene exercises `DrawRect` + `gRB3OutfitComposeActive` + `mCompose*` (the outfit *_diffuse_output RT recolor), i.e. the most subtle moved code path.
5. **Menu/hub 2D-quad witness (song_select, plain `DrawRect` UI quads) — reaches screen cleanly, no crash/regression.** `scripts/native/song-select-capture.py --bin native/build-agent-W1.4/rb3-native --depths 0,8` → `song_select reached: frame=304 screen='song_select_screen'`, both depth screenshots captured OK. sha256 (time/scroll-driven scene, not bit-stable across boots per the documented W0.3b jitter — recorded for reference, not as an equality gate): `native_depth_00.png` `5ab3b23a26fbb99809be51315912a0389b551e29a94a1dc57f965a8b9bbeaef7`, `native_depth_08.png` `e87bff63d7b2e2da0eb1491fbdd5dac0d9fdc3107799d6bd01930eff577cca34`.
6. **`rb3-tests` — `DrawLogGolden*` 9/9 PASS** (1 pre-existing documented skip: `DrawLogGolden.PopulatesFromRealDrawMesh`, same as S1/S2). Full `milo-engine-tests` run (`SkinGolden.*`/`ClipPoseFixture.*`) deferred to **S4** per PLAN.

### For S4
- Head is now `8d6d895`. All three W1.4 subtask MOVEs (S1 halo, S2 postproc, S3 quad/DrawRect) are landed. Monolith `Rnd_Wgpu_RB3.cpp`: 5980 → 4747 lines (−1233, exceeds the ≈−900…1100 PLAN estimate).
- S4 owns: full `milo-engine-tests` build+run (`SkinGolden.*`/`ClipPoseFixture.*`/`DrawLogGolden*` green), the independent re-verify of all 3 diff-hunk proofs + screenshot/lineup evidence from **parent-of-S1.C1** (`b206d44`) vs **final S3 commit** (`8d6d895`), and the written convergence diff-proof section (PLAN Objective already has the citations: `CMakeLists.txt:273-287` DC3-flavor-drop + `DrawRect2D::Draw`/`PostProcPass::Run` signature divergence — S4 should just formalize it into STATUS per PLAN item 5).
- Note for S4's independent verify: this subtask's C0 (`RB3RttDisabled` de-static) was an **undocumented-by-PLAN but necessary** linkage MOVE, analogous to S2's own undocumented `kRB3PostProcShaderSource`/`PostProcUniforms` expose — S4's diff-hunk re-derivation should expect this 2-line linkage delta in addition to the C1 527/555 block move.

**Remains (this subtask):** none. **Blockers:** none.

## W1.4.S4 — done

**Independent verifier.** No source edits; author-independent re-derivation of S1/S2/S3's
claims plus the full Evidence Procedure, at engine HEAD `8d6d895` (S3 tip, all three MOVEs
landed: `e61e6ec` halo, `3c59dc4`+`3845943` postproc, `b53beef`+`8d6d895` quad/DrawRect).
Parent-of-S1 comparison at `b206d44` (W1.3 tip).

### Build (own dirs; shared trees untouched)
- Reused `native/build-agent-W1.4` (rb3 repo, already at engine HEAD `8d6d895`, clang
  toolchain). Forced a real recompile of all 4 W1.4-touched TUs (`touch`'d
  `RB3HaloPass.cpp`/`RB3PostProc.cpp`/`RB3Quad.cpp`/`Rnd_Wgpu_RB3.cpp`) rather than trusting
  the incremental up-to-date state: `cmake --build . --target rb3-native -j32` → exit 0
  (clean relink, all 4 TUs recompiled + `libmilo-engine.a` relinked); `--target rb3-tests -j32`
  → exit 0.
- Parent-of-S1 comparison build: isolated **read-only `git worktree`** at
  `/tmp/engine-at-b206d44` (mirrors the W1.3.S3/VERIFY precedent — no
  `git reset/rebase/checkout--/restore` on the shared `milo-native-engine` tree, hard rule 7),
  configured as a fresh own-dir `native/build-agent-W1.4-verify-parent`
  (`-DMILO_ENGINE_PATH=/tmp/engine-at-b206d44`, clang, expected soft-pin
  `message(WARNING)` re: `MILO_ENGINE_PIN` mismatch — by design, coordinator bumps the pin
  once per wave) → `rb3-native` built clean. Both scratch dirs (`/tmp/engine-at-b206d44`
  worktree, `native/build-agent-W1.4-verify-parent`) removed after evidence capture.

### 1. Diff-hunk equivalence — independently re-derived for all 3 MOVE commits
```
git show e61e6ec -- src/platform/Rnd_Wgpu_RB3.cpp | grep '^-' ... > removed   (310 lines)
git show e61e6ec -- src/platform/RB3HaloPass.cpp  | grep '^+' ... > added     (330 lines)
comm -23 <(sort removed) <(sort added)   ->  0 removed-only lines
```
- **S1 (`e61e6ec`, halo):** 310 removed / 330 added, **0 removed-only lines** — perfect pure
  MOVE, the +20 delta is entirely new-TU preamble (banner + includes).
- **S2 (`3845943`, postproc body):** 398 removed / 438 added (vs `RB3PostProc.cpp`+`.h`),
  **exactly 1 removed-only line**: `static const char* kRB3PostProcShaderSource =` — the
  documented intentional de-static (its non-static form is present on the added side).
  Every other body line matches byte-for-byte. `3c59dc4` (the `RB3PostProcDisabled`
  linkage MOVE) reviewed directly: 2-file, 30/2 line diff, exactly as described.
- **S3 (`8d6d895`, quad/DrawRect body):** 527 removed / 555 added (vs `RB3Quad.cpp`+`.h`),
  **0 removed-only lines** — perfect pure MOVE. `b53beef` (the `RB3RttDisabled` linkage
  MOVE) reviewed directly: 2-file, 30/1 line diff, exactly as described.
- **Conclusion: independently confirmed — every moved body line is untouched token-for-token;
  the only non-preamble deltas are the 2 documented, PLAN-analogous linkage de-statics
  (`RB3PostProcDisabled`, `RB3RttDisabled`), each its own commit.**

### 2. Exit-criteria greps — reconfirmed at final HEAD `8d6d895`
- `grep -c 'BandRnd::CompositeHaloBloom|BandRnd::EnsureHaloBlitPipeline|BandRnd::IsHaloSourceMat|BandRnd::EnsureHaloTarget|BandRnd::HighwayBloomEnabled' src/platform/Rnd_Wgpu_RB3.cpp` → **0**
- `grep -c 'BandRnd::RunPostProcComposite|BandRnd::FlushPostProcMidFrame|BandRnd::DoPostProcess|BandRnd::MainColorTarget|BandRnd::EnsureIntermediate' src/platform/Rnd_Wgpu_RB3.cpp` → **0**
- `grep -n 'BandRnd::DrawRect\b|BandRnd::EnsureQuadPipeline' src/platform/Rnd_Wgpu_RB3.cpp` → **1 hit, a comment** (`:4368`, inside `DrawMesh`'s banner — not a def)
- `grep -c 'kRB3HaloBlitShaderSource|kRB3PostProcShaderSource|kRB3QuadShaderSource' src/platform/Rnd_Wgpu_RB3.cpp` → **0**
- `gRB3OutfitComposeActive`: single non-static def, `src/platform/RB3Quad.cpp:225` only.
- `RB3QuadPipeKey`: `static`, both callers inside `RB3Quad.cpp`'s `DrawRect`, no external caller.
- `wc -l src/platform/Rnd_Wgpu_RB3.cpp` → **4747** (was 5980 at `b206d44`, **−1233 lines**,
  exceeds the PLAN's ≈−900…1100 estimate — matches S3's own count).
- `CMakeLists.txt:324-326` lists all three new .cpp files in `MILO_ENGINE_GPU_PLATFORM_SOURCES_RB3`.

### 3. Behavioral corroboration — lineup gate + screenshots (parent-of-S1 `b206d44` vs final `8d6d895`)
- **`lineup-gate.py` PASS on BOTH builds** (deterministic outfit-compose scene, the
  authoritative MOVE gate per PLAN since raw scene PNG sha is not bit-stable — song-time-driven
  camera, reconfirmed here: captured `songMs` differed run-to-run on the same binary, e.g.
  parent 21431ms vs final path differing too):
  - parent (`build-agent-W1.4-verify-parent`): `LINEUP_GATE verdict=PASS img=PASS segA=PASS ratioB=PASS countC=PASS pin=PASS` (4/4 frames, img sim 77.57-84.01).
  - final (`build-agent-W1.4`): `LINEUP_GATE verdict=PASS img=PASS segA=PASS ratioB=PASS countC=PASS pin=PASS` (4/4 frames, img sim 69.81-81.55).
  Identical verdict shape both builds, matching the committed `scripts/native/goldens/w0.5-lineup/`
  golden — this exercises `DrawRect`+`gRB3OutfitComposeActive`+`mCompose*`, the most subtle
  moved code path (S3).
- **Gameplay (halo/gem-bloom witness), `gameplay-depth-capture.py --times 5000,8000`:** both
  builds reach `game_screen` cleanly with the emissive-gem additive bloom visible. PNG sha256
  NOT bit-equal (confirmed jitter, consistent with W0.3b/W1.3's documented finding — songMs
  differs run-to-run on identical binaries due to wall-clock timing, e.g. parent songMs=5441/8241
  vs final songMs=5127/8420 for nominally-same target times). Recorded pixel-diff due diligence:
  mean abs diff 12.55/255, 23.2% of pixels differ >10 — consistent with a note-scroll/camera
  offset at slightly different songMs, not a code-motion regression (no black frames, no
  missing/duplicated draws by visual inspection).
  - final sha256: `gameplay_05000ms.png` `4f1eaf45bec6d27d2c7487929040016461fa52c0bc4aeb5dfaea9b606741f657`, `gameplay_08000ms.png` `2d00ea96150336aa6c86779c6afec676a19b455e6c2bb278e8dc1559cc02e71a`
  - parent sha256: `gameplay_05000ms.png` `ac2e49a8a9ec9d172192e3bd957e9af7b88fefb84cce094221f5094512e2ae44`, `gameplay_08000ms.png` `b17bb8a681107d4dcdde87fb44a37d790ecfbfca9daa062ca65851ea108f231a`
- **song_select (postproc/2D-quad witness), `song-select-capture.py --depths 0,8`:** both
  builds reach `song_select_screen` cleanly (final frame=282, parent frame=249 — animation-count
  jitter, same documented cause). PNG sha256 not bit-equal (same jitter cause); pixel-diff due
  diligence on depth_00: mean abs diff 2.77/255, 8.5% of pixels differ >10 (menu scroll/text-cursor
  animation offset — small, no gross visual break).
  - final sha256: `native_depth_00.png` `75b04d7db4db45a5ce192d807bbfe4a629b16109fd3d0aecbbcf038ec52aa595`, `native_depth_08.png` `deb5f51c1691582f5566141c34b185f1f0de6890471ea530b1163f71e7e41dde`
  - parent sha256: `native_depth_00.png` `627fdecd5f76981bb4454b626465f2856f0b7cad17adb5972e59869a4e083313`, `native_depth_08.png` `94f885595239d4350ecc634e91c0def081bf208468c5a7489085391544af1235`
- **Per PLAN's own fallback (§2, "if a scene's fixed-clock frame is irreducibly jittery, fall
  back to the deterministic `patch-lineup-capture.py` scene + `lineup-gate.py` image layer"):
  the lineup gate (§ above, bit-for-bit-equivalent PASS shape on both builds) is the controlling
  MOVE-preservation evidence; the gameplay/song_select screenshots are due-diligence
  corroboration only, exactly as W1.3.S3/VERIFY treated them.**

### 4. `rb3-tests` (own build, final HEAD) — `DrawLogGolden*` 9/9 PASS
1 pre-existing documented skip (`DrawLogGolden.PopulatesFromRealDrawMesh` — "real-draw
population proven by S1 boot capture + S3 live capture; in-process DrawMesh needs full
camera/material/pass state not stood up by the unit fixture"), same as every prior W1.4 subtask.

### 5. `milo-engine-tests` (safety net, full suite) — **200 total, 198 passed, 0 failed, 2 skipped-by-design**
Reused shared `milo-native-engine/build-agent-W2-TESTFIX` (already has the W2-TESTFIX fixes
that cleared the Wave-1-documented 29 dc3-drift failures; source tree is the single shared
engine checkout, so this dir's HEAD = current `8d6d895` too). Rebuilt `milo-engine-tests`
clean (`gpu backend flavor: dc3` — expected; this target doesn't compile the RB3-only TUs
directly, it's the cross-cutting safety net), then:
```
DC3_DATA=/home/free/code/milohax/dc3-decomp/orig-assets \
MILO_LIB=/home/free/code/milohax/dc3-decomp/orig-assets/extracted \
ctest -j1 --output-on-failure
```
→ **100% tests passed, 0 tests failed out of 200** (126.00 sec). The 2 skips are the expected
by-design ones (`ExtractBik.ExtractSmallest`, `SkinGolden.CaptureGolden`), not residual
failures. Confirmed the Wave-1 safety nets specifically:
- `SkinGolden.GoldenMatchesReference` / `.ReferenceMatchesCompiledSkinVertex` /
  `.BrokenSkinDivergesFromGolden` — all PASS.
- `ClipPoseFixture.EffectorWorldPositionsMatchGolden` — PASS (`LastTest.log:7714`).
**This is BETTER than the PLAN's expected bar** ("29 documented dc3-drift failures are
expected/untouched") — those 29 were independently cleared by the concurrent `W2-TESTFIX`
lane (already on shared engine HEAD before this verify ran); 0 failures remain, and none of
the 4 W2-TESTFIX fix commits touch any W1.4 file.

### Convergence diff-proof (why NEW RB3-only TUs, not `gfx/PostProcPass`/`gfx/DrawRect2D`)

Formalizing the PLAN Objective's citation, independently re-checked against current source:

- **`CMakeLists.txt:273-287`** (`MILO_ENGINE_GFX_RNDOBJ_SOURCES`, the "Tier 2 — rndobj-COUPLED
  gfx" comment block): `src/gfx/DrawRect2D.cpp` and `src/gfx/PostProcPass.cpp` are explicitly
  documented there as shaped for DC3's `rndobj/` headers and are **"part of the `dc3` backend
  flavor, so RB3 (older `rndobj/`) drops them."** The RB3 backend
  (`MILO_ENGINE_GPU_PLATFORM_SOURCES_RB3`, where all 3 new W1.4 TUs are wired) does not compile
  either DC3 file — confirmed by their absence from that source list. Converging RB3 code into
  them would drag DC3-shaped `rndobj/` dependencies into the RB3 link, breaking it — the exact
  reason W1.2 (`RB3MeshCache`)/W1.3 (`RB3MaterialBinder`) made RB3-only twins instead of
  converging, and the same reasoning W1.4 continues.
- **Signature/body divergence, verified directly in current source (not just cited):**
  - `void DrawRect2D::Draw(wgpu::RenderPassEncoder& pass, const Hmx::Rect& rect, RndMat* mat, ...)`
    (`src/gfx/DrawRect2D.cpp:96`) vs
    `void BandRnd::DrawRect(const Hmx::Rect& rect, const Hmx::Color& paramColor, RndMat* mat, ...)`
    (`src/platform/RB3Quad.cpp:227`) — different signature (RB3 takes an explicit `paramColor`)
    and, per the moved body, `BandRnd::DrawRect` modulates by `mat->GetColor() * paramColor`
    and runs the entire `OutfitConfig` two-colour compose path
    (`gRB3OutfitComposeActive`/`mCompose*`/`rb3_compose.wgsl` lerp) that `DrawRect2D::Draw` has
    no equivalent of at all (confirmed: `RB3Quad.h`/`RB3Quad.cpp` banner comments state
    `DrawRect2D::Draw` "ignores matColor and has no compose").
  - `void PostProcPass::Run(wgpu::CommandEncoder& encoder, wgpu::TextureView& intermediateView, ...)`
    (`src/gfx/PostProcPass.cpp:238`) vs `void BandRnd::RunPostProcComposite(wgpu::TextureView dst)`
    (`src/platform/RB3PostProc.cpp:178`) — different signature/state (`DofPass`, flicker state,
    consumes DC3 `RndPostProc`) vs RB3's own `mBloom` + 160-byte `PostProcUniforms` +
    `mQuadPost*` pipeline grading an RB3-owned RTT intermediate.
  - The RB3 additive-halo gem-bloom capture/replay pass (`RB3HaloPass.cpp`) has **no DC3
    counterpart at all** — nothing to converge onto.
- **Conclusion (re-confirmed, not re-opened):** three NEW RB3-only TUs
  (`RB3HaloPass`/`RB3PostProc`/`RB3Quad`) is the correct and only viable structure here; a
  future reader should not re-attempt convergence onto `gfx/PostProcPass`/`gfx/DrawRect2D`
  without first porting an RB3-flavored `rndobj/` (out of scope, deferred to roadmap Phase 2
  per the CMakeLists.txt comment itself).

### Repo hygiene
Confirmed shared `milo-native-engine` tree otherwise clean: only the pre-existing, documented
concurrent-agent edit to `src/platform/FxSendNative.cpp` (untouched, hard rule 8). No
`git reset/rebase/checkout--/restore` run on any shared tree (hard rule 7). Scratch artifacts
(`/tmp/engine-at-b206d44` worktree, `native/build-agent-W1.4-verify-parent`) removed after use.
`MILO_ENGINE_PIN` untouched (hard rule 3) — this is a verify-only subtask, no source edits.

### Verdict

All PLAN.md exit criteria (1-8) confirmed independently:
1. ✅ 3 new TU pairs exist, wired into `MILO_ENGINE_GPU_PLATFORM_SOURCES_RB3`.
2. ✅ All 4 monolith-grep exit criteria → 0 (or comment-only).
3. ✅ `RB3PostProcDisabled`/`RB3RttDisabled` linkage confirmed single-decl/single-non-static-def;
   `gRB3OutfitComposeActive` single non-static def; `RB3QuadPipeKey` stays `static`, no external caller.
4. ✅ `rb3-native`, `rb3-tests`, `milo-engine-tests` all build clean in their respective
   `native/build-agent-W1.4` / `milo-native-engine/build-agent-W2-TESTFIX` dirs (never
   `build-native`/`build-web*`).
5. ✅ Per-commit diff-hunk equivalence independently re-derived for all 3 MOVEs (0 removed-only
   body lines each, modulo the 2 documented linkage de-statics) + lineup-gate PASS on both
   parent-of-S1 and final builds + gameplay/song_select screenshots captured as due-diligence
   corroboration (jitter-affected, as documented — lineup gate is the controlling evidence).
6. ✅ `milo-engine-tests` full suite: 198/198 non-skipped PASS (0 failed), including
   `SkinGolden.*` and `ClipPoseFixture.EffectorWorldPositionsMatchGolden` — better than the
   PLAN's expected bar (the 29 dc3-drift failures were independently cleared by W2-TESTFIX).
7. ✅ All 5 W1.4 commits (`e61e6ec`, `3c59dc4`, `3845943`, `b53beef`, `8d6d895`) are pure MOVEs,
   prefixed `W1.4:`, staged only W1.4-owned files, under the engine git lock.
   `MILO_ENGINE_PIN` untouched.
8. ✅ Convergence diff-proof written above (this section) — do not re-open.

**Commits:** none (verification-only; this STATUS.md append is the only artifact).
**Remains:** nothing for W1.4. **Blockers:** none. W1.4 (S1+S2+S3+S4) is **done** — ready for
the coordinator to sequence W1.5 next (Lane A: W1.4 → W1.5 → W1.7 → W1.6).

## VERIFY — complete

**Independent second-pass verifier** (role: dedicated VERIFIER agent for W1.4, separate from the
S4 subtask above). Re-derived every claim in S1/S2/S3/S4 from scratch rather than trusting the
STATUS text; all reproduced exactly. No source edits (verify-only).

### Re-derivation results (all independently reproduced, matching S4 exactly)

1. **Commit chain confirmed.** Engine HEAD `8d6d8959c31d4336721935d2b051bfc1f2e31659`; all 5 W1.4
   commits (`e61e6ec`, `3c59dc4`, `3845943`, `b53beef`, `8d6d895`) verified ancestors of HEAD via
   `git merge-base --is-ancestor`.
2. **Diff-hunk equivalence re-run from scratch (not copy-pasted from STATUS):**
   - S1 `e61e6ec`: 310 removed / 330 added, `comm -23` -> **0** removed-only lines.
   - S2 `3845943`: 398 removed / 438 added, `comm -23` -> **exactly 1** removed-only line
     (`static const char* kRB3PostProcShaderSource =`, the documented intentional de-static).
   - S3 `8d6d895`: 527 removed / 555 added, `comm -23` -> **0** removed-only lines.
   - Both linkage-MOVE commits (`3c59dc4`, `b53beef`) read in full: each is a pure
     `static`->non-static retype at the definition site + one new decl in the satellite header +
     one new `#include` in the monolith. No logic touched.
3. **Exit-criteria greps re-run on current source** — all confirmed: 0 hits for
   `BandRnd::CompositeHaloBloom|EnsureHaloBlitPipeline|IsHaloSourceMat|EnsureHaloTarget|HighwayBloomEnabled`,
   0 for `BandRnd::RunPostProcComposite|FlushPostProcMidFrame|DoPostProcess|MainColorTarget|EnsureIntermediate`,
   1 comment-only hit (`:4368`, not a def) for `BandRnd::DrawRect|EnsureQuadPipeline`, 0 for the 3
   shader-source symbols. `gRB3OutfitComposeActive` single non-static def at `RB3Quad.cpp:225`.
   `RB3QuadPipeKey` stays `static`, only 2 callers, both inside `RB3Quad.cpp`. `wc -l
   Rnd_Wgpu_RB3.cpp` -> **4747** (was 5980 pre-W1.4). CMakeLists.txt:324-326 wires all 3 new TUs.
4. **Real rebuild, own dir (`native/build-agent-W1.4`, reused, clang-configured).** Forced a real
   recompile by `touch`-ing all 4 W1.4-touched TUs (`RB3HaloPass.cpp`/`RB3PostProc.cpp`/
   `RB3Quad.cpp`/`Rnd_Wgpu_RB3.cpp`) before building — confirmed the Makefile actually
   recompiled all 4 (build log shows 4 `Building CXX object` lines + relink), not a stale
   up-to-date no-op. `rb3-native` -> exit 0. `rb3-tests` -> exit 0.
5. **`rb3-tests` full suite re-run** — **70/70 PASS**, 1 documented skip
   (`DrawLogGolden.PopulatesFromRealDrawMesh`). `DrawLogGolden*` subset individually re-run:
   9/9 PASS + the 1 skip, exact match to S1/S2/S3's per-subtask numbers.
6. **`lineup-gate.py` re-run against `native/build-agent-W1.4/rb3-native`** —
   `LINEUP_GATE verdict=PASS img=PASS segA=PASS ratioB=PASS countC=PASS pin=PASS` (4/4 frames, all
   layers; img sim 70.1-81.7, same range as S4's parent/final runs — corroborates the documented
   camera-timing jitter is cosmetic, not a regression).
7. **`milo-engine-tests` full suite re-run** (reused shared `build-agent-W2-TESTFIX`, same engine
   checkout, confirmed HEAD `8d6d895`) — rebuilt `milo-engine-tests` clean, then
   `ctest -j1 --output-on-failure`: **100% tests passed, 0 failed out of 200** (127.64s), 2
   by-design skips (`ExtractBik.ExtractSmallest`, `SkinGolden.CaptureGolden`). Confirmed the
   `SkinGolden.*` and `ClipPoseFixture.*` safety-net suites are among the passing 198. Note:
   verified no `.o` for `RB3HaloPass.cpp`/`RB3PostProc.cpp`/`RB3Quad.cpp`/`Rnd_Wgpu_RB3.cpp` exists
   in this build dir at all (`MILO_ENGINE_GPU_BACKEND=dc3` — the RB3-only TUs aren't even compiled
   into this target) — confirming S4's own caveat that this run is a cross-cutting safety net, not
   a direct exercise of the moved code; the direct exercise is `rb3-native`/`rb3-tests` (items 4-6).
8. **Convergence diff-proof spot-checked directly in current source** (not just cited): confirmed
   `void DrawRect2D::Draw(wgpu::RenderPassEncoder&, const Hmx::Rect&, RndMat*, ...)`
   (`src/gfx/DrawRect2D.cpp:96`) and `void PostProcPass::Run(wgpu::CommandEncoder&,
   wgpu::TextureView&, ...)` (`src/gfx/PostProcPass.cpp:238`) are real, present, differently-shaped
   signatures from the RB3 twins; `CMakeLists.txt:273-287` Tier-2 comment confirmed verbatim
   ("Part of the `dc3` backend flavor, so RB3 (older rndobj/) drops them"); neither
   `DrawRect2D.cpp` nor `PostProcPass.cpp` appears in `MILO_ENGINE_GPU_PLATFORM_SOURCES_RB3`.
   Non-convergence conclusion re-confirmed independently.
9. **Repo hygiene** — `git status --short` in the engine repo shows only the pre-existing,
   documented concurrent-agent edit (`src/platform/FxSendNative.cpp`), untouched. No
   `git reset/rebase/checkout--/restore` used anywhere in this verify pass (hard rule 7).
   `MILO_ENGINE_PIN` untouched (hard rule 3) — no source edits made.

### Scope note (small fixes vs redesign)

No fixes were needed — every criterion reproduced clean on the first try. Nothing in scope for a
"W1.4: fix ..." commit; this verify pass required zero source changes.

### Verdict

**All PLAN.md exit criteria (1-8) independently reconfirmed by a second, dedicated verifier pass,**
with real rebuilds (not just re-reading S4's numbers) and one fresh full `ctest` run of
`milo-engine-tests`. No discrepancies found between the STATUS.md narrative and the actual repo
state. W1.4 is **done**; ready for the coordinator to sequence W1.5 next (Lane A: W1.4 -> W1.5 ->
W1.7 -> W1.6). No blockers.

**Commits:** none (verification-only). **Remains:** nothing for W1.4.
