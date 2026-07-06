# W1.4 — Extract postproc / halo / quad passes into TUs — PLAN

**Repo:** engine `/home/free/code/milohax/milo-native-engine` (own git). Commit prefix `W1.4:`.
**Lane A (sequential on `src/platform/Rnd_Wgpu_RB3.cpp`):** W1.2 → W1.3 → **W1.4** → W1.5 → W1.7 → W1.6.
Assume W1.2 (`RB3MeshCache`, engine `6f9d340`/`daf0ed1`/`daa0286`) and W1.3 (`RB3MaterialBinder`,
engine `b206d44`/`c43b6fd`) are **in** (head at plan time = `b206d44`). Do not pre-empt W1.5/W1.6/W1.7.

> Line numbers below were re-grepped against `b206d44` (file = **5980 lines**). Re-grep before every
> edit — prior lane-A commits already shifted things and your own commits shift the rest.

---

## Objective

Move the RB3 **post-process composite**, **additive-halo gem-bloom capture/replay+blit**, and **2D
quad / `DrawRect`** code out of the 5,980-line `Rnd_Wgpu_RB3.cpp` monolith into three cohesive
**new RB3-only TUs** — `src/platform/RB3HaloPass.cpp`, `RB3PostProc.cpp`, `RB3Quad.cpp` (+ small
headers) — each compiled into `MILO_ENGINE_GPU_PLATFORM_SOURCES_RB3`. **Pure MOVE commits only:
byte-identical rendered output at every commit.** This mirrors the W1.2/W1.3 precedent exactly
(new RB3-only twins, not convergence).

### Convergence decision — DO NOT converge onto `gfx/PostProcPass` / `gfx/DrawRect2D` (diff-proven)

The review brief says "converge where the RB3 port is a proven hand-copy." **It is not, and it
structurally cannot be:**
- `gfx/DrawRect2D.cpp` and `gfx/PostProcPass.cpp` live in `MILO_ENGINE_GFX_RNDOBJ_SOURCES` — the
  **DC3-flavor** source set. `CMakeLists.txt:273-287` documents that these include DC3-shaped
  `rndobj/` headers and **"RB3 (older rndobj/) drops them"**; the RB3 backend
  (`MILO_ENGINE_GPU_PLATFORM_SOURCES_RB3`) compiles neither. Converging RB3's code into those files
  would drag them into the RB3 link and break it (the exact reason W1.2/W1.3 made RB3-only twins).
- The bodies diverge heavily, not hand-copies:
  - `BandRnd::DrawRect` modulates by `mat->GetColor() * paramColor` and carries the entire
    OutfitConfig two-colour **compose** path (`gRB3OutfitComposeActive`, `mCompose*`, the
    `rb3_compose.wgsl` lerp pass) — `gfx/DrawRect2D::Draw` ignores `matColor` and has no compose
    (header comment `Rnd_Wgpu_RB3.h:170-178` + `:2782-2792` already say so).
  - `BandRnd::RunPostProcComposite` runs its own bloom (`mBloom`) + 160-byte `PostProcUniforms` +
    `mQuadPost*` pipeline; `PostProcPass::Run` has a different signature/state (`DofPass`, flicker
    state) and consumes DC3 `RndPostProc`.
- The RB3 halo capture/replay pass has **no DC3 counterpart at all**.

**Conclusion:** create NEW RB3-only TUs (RB3HaloPass / RB3PostProc / RB3Quad). Record this
diff-proof in `STATUS.md` (S4). This is a decision, not an open question — do not attempt to merge
into the DC3 gfx files.

### Why this is a clean set of MOVE commits (mechanism)

Every function in scope is already a **`BandRnd::` method** (or a file-scope helper/struct/shader
`#include`) declared in `Rnd_Wgpu_RB3.h`. So the extraction is a **cross-TU relocation of
definitions with the class definition unchanged**: `void BandRnd::RunPostProcComposite(...)` simply
gets *defined* in `RB3PostProc.cpp` instead of `Rnd_Wgpu_RB3.cpp`. All member access
(`this->mQuad*`, `mBloom`, `mHaloDraws`, …) and all cross-method calls
(`EnsureQuadPipeline()`, `CompositeHaloBloom()`, `HighwayBloomEnabled()`) resolve at link time
because both TUs land in the same target and every member/method is in the shared header. **No
signatures change, no state is plumbed, no header struct moves.** The only linkage adjustment is
de-`static`-ing the two file-scope helpers that end up called across the TU split (below).

---

## Faithful-reference citations (current lines @ `b206d44`)

**Halo (→ RB3HaloPass):** header decls `Rnd_Wgpu_RB3.h:243-259`, `HaloDraw` struct + `mHaloDraws`
`:432-438`; defs:
- `HighwayBloomEnabled()` `:2030`
- `IsHaloSourceMat()` `:2059`
- `EnsureHaloTarget()` `:2079`
- `kRB3HaloBlitShaderSource` (`#include "gfx/Shaders/rb3_halo_blit.wgsl.inc"`) `:2105-2107`
- `EnsureHaloBlitPipeline()` `:2109`
- `CompositeHaloBloom()` `:2192`

**PostProc (→ RB3PostProc):** header decls `Rnd_Wgpu_RB3.h:151-241` (DoPostProcess/EnsureIntermediate/
MainColorTarget/RunPostProcComposite/FlushPostProcMidFrame); defs:
- `RB3PostProcDisabled()` **static**, fwd-decl `:53`, def `:2505` — **de-static (S2.C1)**
- `FlushPostProcMidFrame()` `:1967`
- `DoPostProcess()` `:2324`
- `PostProcUniforms` struct + `static_assert(sizeof==160)` `:2395-2422`
- `kRB3PostProcShaderSource` (`#include ".../rb3_postproc.wgsl.inc"`) `:2424-2426`
- `MainColorTarget()` `:2437`, `EnsureIntermediate()` `:2453`
- `RunPostProcComposite()` `:2554-2795`

**Quad / DrawRect (→ RB3Quad):**
- `kRB3QuadShaderSource` (`#include ".../rb3_quad.wgsl.inc"`) `:2796`
- `RB3RectUB` / `RB3RectVertex` structs `:2802-2812`
- `EnsureQuadPipeline()` `:2814-2954` (contains the local `kRB3ComposeShaderSource`
  `#include ".../rb3_compose.wgsl.inc"` at `:2863` + all `mCompose*`/`mQuadPost*` infra)
- `RB3QuadPipeKey()` **static** `:2955` (only callers are inside `DrawRect` `:3140`/`:3207` → moves
  with `DrawRect`, **stays static**)
- `gRB3OutfitComposeActive` global (non-static) `:2971` (extern-referenced from
  `rb3/src/system/bandobj/OutfitConfig.cpp:121` via a local `extern bool` — keep non-static, just
  relocate the definition; **no header change**)
- `DrawRect()` `:2973-3302`

**Stays put (out of W1.4 scope — do NOT move):**
- Capture site in `DrawMesh` (`:5339-5350`, the `mHaloDraws.push_back({pipe, mSceneBindGroup, …})`
  leaf) — belongs to W1.7's DrawMesh work. It references `IsHaloSourceMat()`/`mHaloDraws` (moved
  helper / header member) and keeps working unchanged.
- `CompositeHaloBloom()`/`RunPostProcComposite()`/`FlushPostProcMidFrame()` **call sites** in
  `BeginFrame`/`EndFrame`/`EndDrawTarget` (`:1021`,`:1541`,`:1653`,`:1662`,`:2334`) — they call the
  moved methods cross-TU (fine).
- `EnsureDepth()` `:2481`, `ClearDepthForOverlay()` `:2337`, `EnsureQuadPipeline` **callers** — depth
  mgmt / DrawMesh belong to later items; leave them.
- The `RB3PipelinePrewarm*` statics `:2517/2536/2544` — not referenced by any moved function
  (verify with grep during S2); leave static in place.

---

## Subtasks

### W1.4.S1 — Extract the halo gem-bloom pass into `RB3HaloPass.{h,cpp}`  (model: opus)
**Why opus:** the capture-and-replay is load-bearing for gem bloom and subtle (it captures the LIVE
`mSceneBindGroup` **handle**, not an offset; a `dynamicOffsetCount` mismatch on replay silently
discards the whole command buffer — `Rnd_Wgpu_RB3.h:428-438`). The MOVE must not perturb a single
token of it.
**Files:** NEW `src/platform/RB3HaloPass.h`, `src/platform/RB3HaloPass.cpp`; edit
`src/platform/Rnd_Wgpu_RB3.cpp` (delete moved defs), `CMakeLists.txt` (add the .cpp).
**Approach (each numbered step = its own MOVE commit + Evidence Procedure run):**
1. **C1 — create TU + move the 5 halo methods + shader include.** Write `RB3HaloPass.h` (a short
   banner comment in the `RB3MeshCache.h`/`RB3MaterialBinder.h` style: "RB3-only TU, twin has no DC3
   counterpart"; it need declare nothing — the methods are `BandRnd::` members already in
   `Rnd_Wgpu_RB3.h` — but keep the header for the CMake/comment convention and any file-scope decl
   you later need). Create `RB3HaloPass.cpp` `#include "platform/Rnd_Wgpu_RB3.h"` (mirror
   `RB3MaterialBinder.cpp` include list; it transitively pulls `gfx/BloomPass.h`,
   `rndobj/PostProc.h`). **Cut-paste verbatim** — from `Rnd_Wgpu_RB3.cpp` — the `kRB3HaloBlitShaderSource`
   include block and the bodies of `HighwayBloomEnabled`, `IsHaloSourceMat`, `EnsureHaloTarget`,
   `EnsureHaloBlitPipeline`, `CompositeHaloBloom` (with their leading comment blocks). Delete the
   originals. Add `src/platform/RB3HaloPass.cpp` to `MILO_ENGINE_GPU_PLATFORM_SOURCES_RB3` with a
   `# W1.4:` comment.
2. Confirm no file-scope `static` helper that STAYS in `Rnd_Wgpu_RB3.cpp` is referenced by the moved
   code, and vice-versa (grep the 5 bodies for bare-function calls). Expected: none — all references
   are `BandRnd::` methods/members + `RndCam`/`RndMat`/`BloomPass` (public). If any static is shared,
   de-static it in a **separate linkage-MOVE commit first** (see S2.C1 pattern).
**Verify:** Evidence Procedure below, with the **gameplay (game.cam, gems)** scene as the primary
witness (halo only fires under `game.cam` on `IsHaloSourceMat` mats). `grep -c 'BandRnd::CompositeHaloBloom' src/platform/Rnd_Wgpu_RB3.cpp` → `0`.

### W1.4.S2 — Extract the post-process composite into `RB3PostProc.{h,cpp}`  (model: opus)
**Why opus:** requires a linkage change (de-`static` `RB3PostProcDisabled`, which is called from
BOTH the staying `BeginFrame`/`EndFrame` and the moving postproc methods) and touches the RTT
intermediate seam.
**Files:** NEW `src/platform/RB3PostProc.h`, `RB3PostProc.cpp`; edit `Rnd_Wgpu_RB3.cpp`, `CMakeLists.txt`.
**Approach:**
1. **C1 (linkage MOVE — its own commit):** de-`static` `RB3PostProcDisabled()`. Create
   `RB3PostProc.h` declaring `bool RB3PostProcDisabled();` (free function, RB3-only TU banner
   comment). Remove the `static` fwd-decl at `Rnd_Wgpu_RB3.cpp:53`; `#include "platform/RB3PostProc.h"`
   near the other platform includes. Change the def at `:2505` from `static bool` → `bool`. This is
   the analogue of W1.3's `c43b6fd`. **No behavior change; still a MOVE** (message:
   `W1.4: expose RB3PostProcDisabled for cross-TU postproc (linkage MOVE)`).
2. **C2 — move the postproc bodies.** Into `RB3PostProc.cpp` (`#include "platform/Rnd_Wgpu_RB3.h"`
   + `"platform/RB3PostProc.h"`) cut-paste verbatim: `PostProcUniforms` struct + `static_assert`,
   `kRB3PostProcShaderSource` include, `RB3PostProcDisabled()` def, `FlushPostProcMidFrame()`,
   `DoPostProcess()`, `RunPostProcComposite()`. **Recommended also:** `MainColorTarget()` +
   `EnsureIntermediate()` (the RTT intermediate that postproc grades) — move them **only if** the
   cut is clean (they are `BandRnd::` methods; `BeginFrame`/`EndFrame` call them cross-TU fine). If
   `EnsureIntermediate` is entangled with a staying block, leave it and note in STATUS — the core
   deliverable is the composite + flush + disabled-check. Add `RB3PostProc.cpp` to CMake.
   `RunPostProcComposite` calls `EnsureQuadPipeline()` — that's a method (fine); it lands in RB3Quad
   in S3, but S3 order-independence holds because it's a link-time symbol.
**Verify:** Evidence Procedure with **song_select** (`B+W_film02.pp` postproc active) as the primary
witness. Also spot-check a **venue mid-frame** grade if reachable (FlushPostProcMidFrame path).

### W1.4.S3 — Extract the 2D quad / `DrawRect` / compose into `RB3Quad.{h,cpp}`  (model: sonnet)
**Why sonnet:** largest block but the most self-contained and purely mechanical (one contiguous
`:2796-3302` span plus the two rect structs). No de-static needed (`RB3QuadPipeKey` moves with its
only callers).
**Files:** NEW `src/platform/RB3Quad.h`, `RB3Quad.cpp`; edit `Rnd_Wgpu_RB3.cpp`, `CMakeLists.txt`.
**Approach (one MOVE commit, or split C1=EnsureQuadPipeline+structs, C2=DrawRect if you prefer two
smaller gates):**
1. Create `RB3Quad.cpp` (`#include "platform/Rnd_Wgpu_RB3.h"`) and `RB3Quad.h` (banner comment).
   Cut-paste verbatim: `kRB3QuadShaderSource` include, `RB3RectUB`/`RB3RectVertex` structs,
   `EnsureQuadPipeline()` (including its internal `kRB3ComposeShaderSource` include + all
   `mQuadPost*`/`mCompose*` infra), `RB3QuadPipeKey()` (**keep `static`**),
   `gRB3OutfitComposeActive` (**keep non-static, no header** — `OutfitConfig.cpp` `extern`s it),
   and `DrawRect()`. Delete originals. Add `RB3Quad.cpp` to `MILO_ENGINE_GPU_PLATFORM_SOURCES_RB3`.
2. Grep-confirm `gRB3OutfitComposeActive` is still a single non-static definition (nm/objdump the
   built .o if unsure) and `RB3QuadPipeKey` has no caller outside `RB3Quad.cpp`.
**Verify:** Evidence Procedure with the **band lineup** scene (outfit two-colour compose exercises
`DrawRect` + `gRB3OutfitComposeActive` + `mCompose*`) via `patch-lineup-capture.py` +
`lineup-gate.py` as the primary witness, plus a **menu/hub** frame (plain `DrawRect` 2D quads).
`grep -c 'BandRnd::DrawRect' src/platform/Rnd_Wgpu_RB3.cpp` → `0`.

### W1.4.S4 — Independent verify + convergence diff-proof + STATUS  (model: sonnet)
**Goal:** author-independent confirmation all three MOVEs are byte-identical, the recorded evidence
trail, and the written diff-proof that non-convergence was correct.
**Files:** `docs/native/engine-arch-review-2026-07-05/execution/W1.4/STATUS.md` (append under
`flock /tmp/rb3-docs.lock`). No source edits.
**Approach:**
1. Clean-build `rb3-native` + `rb3-tests` at the final S3 commit in `native/build-agent-W1.4`.
2. Re-run the Evidence Procedure end-to-end from **parent-of-S1.C1** vs **final S3 commit** across
   all three witness scenes (gameplay/halo, song_select/postproc, lineup/DrawRect-compose): assert
   screenshot sha256 equality at each; `lineup-gate.py` PASS against the committed
   `scripts/native/goldens/w0.5-lineup/` golden.
3. For each moved region run the **diff-hunk equivalence proof** (primary, deterministic gate — see
   Evidence Procedure §1): the deleted lines from `Rnd_Wgpu_RB3.cpp` equal the added lines in the new
   TU byte-for-byte (modulo the new file's include/banner header). Record the proof command output.
4. Build + run `milo-engine-tests` (recipe in `W0.1/STATUS.md`): W0.1 `SkinGolden.*`, W0.4
   `ClipPoseFixture.*`, and W0.3 `DrawLogGolden*` gtests green (the 29 documented dc3-drift failures
   are expected/untouched — do not attribute them to W1.4).
5. Write the **convergence diff-proof** section into STATUS: record the `CMakeLists.txt:273-287`
   citation + the `DrawRect2D::Draw`/`PostProcPass::Run` signature divergence, concluding NEW-TU was
   correct (so a future reader does not re-open it).
6. Append `## W1.4.S1|S2|S3|S4 — done` sections with commit SHAs, per-scene sha256s, and gate results.

---

## Evidence Procedure (REQUIRED byte-identical gate for every MOVE commit)

Because W0.3b's `RB3_FIXED_CLOCK` drawlog golden is only ~2/3 reliable (see `W0.3b/STATUS.md`), the
**primary, deterministic gate is diff-hunk equivalence**; the screenshot hash + lineup gate are the
behavioral corroboration.

1. **Diff-hunk equivalence (primary — deterministic, not flaky).** For each MOVE commit, prove the
   moved text is a pure relocation:
   ```bash
   cd /home/free/code/milohax/milo-native-engine
   # lines removed from the monolith this commit:
   git show <commit> -- src/platform/Rnd_Wgpu_RB3.cpp | grep '^-' | sed 's/^-//' > /tmp/w14-removed.txt
   # lines added to the new TU this commit (strip the new file's banner/include preamble):
   git show <commit> -- src/platform/RB3<Pass>.cpp | grep '^+' | sed 's/^+//' > /tmp/w14-added.txt
   # after removing the TU's own header/include preamble from added, the moved bodies must match:
   diff <(sort /tmp/w14-removed.txt) <(sort /tmp/w14-added.txt)   # only the preamble lines differ
   ```
   A cleaner form: `git show --stat` should show `Rnd_Wgpu_RB3.cpp` lost ≈ the line count the new TU
   gained (plus the small include/banner delta). Record the delta in STATUS. **Any moved *body* line
   that changed = STOP, not a MOVE.**
2. **Deterministic-scene screenshot hash (corroboration).** Build `rb3-native` in
   `native/build-agent-W1.4`; run headless with the fixed clock and drive to the pass-specific
   witness scene, capture + hash:
   ```bash
   RB3_HTTP=1 RB3_FIXED_CLOCK=1 <run rb3-native on a free port>
   # scripted nav — reuse the matching harness per scene:
   #   halo/gameplay   → scripts/native/capture_song_gameplay.py (or gameplay-depth-capture.py)
   #   postproc        → scripts/native/song-select-capture.py   (B+W_film02.pp active)
   #   drawrect/compose→ scripts/native/patch-lineup-capture.py  (deterministic; backs w0.5 golden)
   curl -s localhost:<port>/api/screenshot -o /tmp/w14-<scene>-after.png
   sha256sum /tmp/w14-<scene>-after.png     # record in STATUS
   ```
   Capture the **baseline once at parent-of-S1.C1** for all three scenes. After each MOVE commit,
   recapture and assert `sha256(base)==sha256(after)`. Run the fixed-clock capture **3×** and require
   the stable hash to equal baseline (per W0.3b the ~1/3 flake is mesh-identity ordering, unrelated
   to a code-motion MOVE — a *consistent* diff across runs is the red flag). If a scene's fixed-clock
   frame is irreducibly jittery, fall back to the deterministic `patch-lineup-capture.py` scene +
   `lineup-gate.py` image layer (zero pixel diff for a MOVE) as that commit's behavioral witness.
3. **Lineup gate + gtests (every commit).** `scripts/native/lineup-gate.py` PASS against
   `scripts/native/goldens/w0.5-lineup/`; `milo-engine-tests` `SkinGolden.*` + `ClipPoseFixture.*` +
   `DrawLogGolden*` green.
4. **On any mismatch:** STOP, do not commit. Most likely cause: an accidentally-edited moved line, a
   missed `static`→extern de-static, or a moved function that referenced a file-scope static left
   behind (link error) — fix and re-gate.

---

## Exit criteria (measurable)

1. `src/platform/RB3HaloPass.{h,cpp}`, `RB3PostProc.{h,cpp}`, `RB3Quad.{h,cpp}` exist and are wired
   into `MILO_ENGINE_GPU_PLATFORM_SOURCES_RB3` in `CMakeLists.txt`.
2. In `src/platform/Rnd_Wgpu_RB3.cpp` (all `→ 0`):
   `grep -c 'BandRnd::CompositeHaloBloom\|BandRnd::EnsureHaloBlitPipeline\|BandRnd::IsHaloSourceMat'`,
   `grep -c 'BandRnd::RunPostProcComposite\|BandRnd::FlushPostProcMidFrame'`,
   `grep -c 'BandRnd::DrawRect\|BandRnd::EnsureQuadPipeline'`,
   `grep -c 'kRB3HaloBlitShaderSource\|kRB3PostProcShaderSource\|kRB3QuadShaderSource'`.
   The file is meaningfully smaller (≈ −900…1100 lines).
3. `RB3PostProcDisabled` is declared once in `RB3PostProc.h`, defined once (non-static) in
   `RB3PostProc.cpp`; the `static` fwd-decl at old `:53` is gone. `gRB3OutfitComposeActive` remains a
   single non-static definition (now in `RB3Quad.cpp`); `RB3QuadPipeKey` remains `static` with no
   caller outside `RB3Quad.cpp`.
4. `rb3-native`, `rb3-tests`, `milo-engine-tests` build clean in `native/build-agent-W1.4` (never
   `build-native`/`build-web*`).
5. **Per-commit byte-identical evidence recorded in STATUS.md:** diff-hunk equivalence proof (pure
   relocation) + deterministic-scene screenshot sha256 equality (gameplay + song_select + lineup) +
   `lineup-gate.py` PASS.
6. `milo-engine-tests` `SkinGolden.*` + `ClipPoseFixture.*` + `DrawLogGolden*` green (no new failures
   beyond the 29 documented dc3-drift ones).
7. Every commit is a pure MOVE, message starts `W1.4:`, stages only W1.4-owned files (the new TUs +
   `Rnd_Wgpu_RB3.cpp` + `CMakeLists.txt`), made under `flock /tmp/milo-engine-git.lock`.
   `MILO_ENGINE_PIN` untouched.
8. STATUS carries the written convergence diff-proof (why NEW RB3-only TUs, not `gfx/PostProcPass` /
   `gfx/DrawRect2D`).

---

## Risks / conflicts

- **Lane A is sequential on `Rnd_Wgpu_RB3.cpp`.** W1.2/W1.3 are in (head `b206d44`). W1.5
  (uniform-ring dedupe) and W1.7 (game hook / asset-name relocation) and W1.6 (scene bind group) run
  **after** W1.4. Do not touch: the DrawMesh material/mesh-cache paths (W1.2/W1.3, already extracted),
  the DrawMesh body + the halo **capture site** at `:5339-5350` and asset-name branches (W1.7), the
  bind-group / `mSceneBindGroup` restructure (W1.6), the two uniform-ring classes (W1.5). W1.4 moves
  only the postproc/halo/quad *pass* code.
- **Halo capture/replay is load-bearing and token-fragile.** `CompositeHaloBloom` replays captured
  draws with the captured LIVE `mSceneBindGroup` handle; a `dynamicOffsetCount` mismatch discards the
  command buffer. This MOVE must not reorder or edit a single line of the replay loop — verify with
  the diff-hunk proof AND the gameplay screenshot hash (gem bloom visible).
- **`RB3PostProcDisabled` de-static is a real linkage change** (shared by staying BeginFrame/EndFrame
  and moving postproc methods) — do it as its own commit (S2.C1), analogous to W1.3 `c43b6fd`. It is
  still classified a MOVE (no behavior change).
- **Do NOT converge onto `gfx/PostProcPass` / `gfx/DrawRect2D`** — they are DC3-flavor TUs the RB3
  backend deliberately drops (`CMakeLists.txt:273-287`); the bodies diverge (matColor tint, outfit
  compose, RB3 bloom/UB). New RB3-only TUs only (twins of `RB3MeshCache`/`RB3MaterialBinder`). Record
  the diff-proof (S4.5).
- **Drawlog/fixed-clock flake (~1/3).** Do not treat the drawlog golden as the authoritative gate;
  the diff-hunk equivalence proof is primary, screenshot sha (3× stable) + lineup gate corroborate.
- **Concurrent-agent uncommitted edits in the engine tree** (e.g. `FxSendNative.cpp`) — never
  `git add -A`; stage only W1.4 files; never `git reset`/`checkout --`/`restore` on the shared tree
  (hard rules 2, 7). If the tree looks wrong, STOP and record in STATUS.
- **Cross-TU method-call ordering** — `RunPostProcComposite` (S2) calls `EnsureQuadPipeline` (lands
  in S3). Both are `BandRnd::` methods → link-time symbols; the S2-before-S3 order is safe because
  the definition exists in the same target regardless of which .cpp holds it. Build after S3 to
  confirm the whole set links.
