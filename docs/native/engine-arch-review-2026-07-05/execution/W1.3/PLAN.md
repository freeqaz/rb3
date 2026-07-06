# W1.3 — Extract material→uniform binder into `RB3MaterialBinder` TU

**Lane:** A (sequential on `src/platform/Rnd_Wgpu_RB3.cpp`: W1.2 → **W1.3** → W1.4 → W1.5 → W1.7 → W1.6).
**Type:** REFACTOR — pure MOVE commits, byte-identical rendered output at every commit.
**Repo:** engine = `/home/free/code/milohax/milo-native-engine` (commit here; its own git).
**Commit prefix:** `W1.3:`   **Build dir:** `native/build-agent-W1.3` (own dir only).

> Line numbers below are anchors as of the current tree (engine HEAD `6f9d340`, W1.2 landed).
> **They will shift** — always re-grep the text markers, never trust the numbers.

---

## Objective

Move the material → `MaterialUniforms` translation logic out of the 2,670-line
`BandRnd::DrawMesh` body into a new RB3-only translation unit
`src/platform/RB3MaterialBinder.{h,cpp}`, so the material fill becomes a legible,
testable function instead of ~490 inline lines. **No rendered-output change.**

The ~asset-name `strcmp`/`strstr` special-cases embedded in the block (crowd dim,
`tail_*` fret colours, track-light `surface.mat`/`rails.mat`/`gem_smasher_glow`/
`peakstate`, `gem_force`, `highlight_*` UI bar, the `*_skin_diffuse_output` /
`head.mesh` debug probes) **MOVE ALONG UNCHANGED**. Relocating them behind the
game render hook is **W1.7's** job, not W1.3's — do not alter their behaviour or
their `RB3_*` flags here.

### Scope boundary (read carefully)

- **IN scope:** the contiguous block that fills the local `MaterialUniforms mu`
  (colour/alpha/texture/prelit/unlit/emissive/texGen + all the RB3 asset-name
  special-cases + text/UI heuristics + `gemForce`).
- **OUT of scope, stays in `DrawMesh`:** the material *GPU bind-group population*
  that follows the block — `slot.matUB` create/`WriteBuffer`, `ResolveMaterialViews`,
  `MakeMaterialBindGroupCached`, and the `sMeshBufCreatesThisFrame`/
  `sMeshBGCreatesThisFrame` counters. That plumbing is tied to `MeshEntry& slot` and
  BandRnd member helpers, and **W1.6 rewrites the whole scene/material bind-group
  path** (immutable-per-write, `DrawContext`). Moving it now would collide with W1.6
  and buy nothing. Leave it.
- **OUT of scope, stays in `DrawMesh`:** the `PipelineKey` blend/zmode derivation
  (`WgpuBlend blendMode` / `WgpuZMode zMode`, ~`5758`+). That is pipeline selection,
  not `mu` fill; it only *consumes* `isTextMeshHeur` + `gemForce` (which the binder
  returns).

### Why a NEW RB3-only TU, not convergence onto `gfx/MaterialSetup`

The item brief and lane doc 01 §5 float "converge onto `gfx/MaterialSetup` where a
diff proves the RB3 copy identical". **It is not identical** — a `diff` proves it:
`MaterialSetup::BuildMaterialParams` (`src/platform/MaterialSetup.cpp`) is the DC3
draw path and has **none** of RB3's text/UI heuristics, crowd-dim, track-light,
`tail_*`, `highlight_*`, or `gem_force` branches. The in-code comments at the
`isTextMeshHeur`/`useAlphaAsRGB`/`texGen` sites merely *cite* `BuildMaterialParams`
as the conceptual DC3 twin. So the correct outcome is a **new RB3-only TU mirroring
W1.2's `RB3MeshCache`** (the RB3-only twin of DC3's `gfx/MeshGpuCache`), compiled
into `MILO_ENGINE_GPU_PLATFORM_SOURCES_RB3`. Do **not** edit `MaterialSetup.cpp`.

---

## Faithful-reference citations (re-grep before use)

| What | Where (current anchors) | Notes |
|---|---|---|
| Enclosing fn | `BandRnd::DrawMesh(RndMesh* mesh)` `Rnd_Wgpu_RB3.cpp:~3302` | the monolith |
| **Move START** | `MaterialUniforms mu{};` (`~5224`) | first line of the region |
| **Move END (exclusive)** | comment `// Per-(mesh,instance) persistent material uniform buffer + cached bind group.` (`~5720`) | bind-group plumbing stays |
| Region-local `owner`/`skinned` | `RndMesh* owner = mesh->GeomOwner(); if(!owner) owner=mesh;` (`~3314`); `bool skinned = owner->IsSkinned();` (`~3434`) | become fn params |
| Post-region users of block outputs | `isTextMeshHeur` @ `~5783` (zMode), `gemForce` @ `~5784` (blend/zMode) | must be **returned** |
| Tex helpers used inside block | `GetRB3TexView` (`static`, def `~800`), `UploadRndTexIfNeeded(GpuDevice&,RndTex*)` (`static`, def `~531`) | must be exposed cross-TU (S1) |
| Only BandRnd member ref in block | `mGpu` (in `UploadRndTexIfNeeded(mGpu, dt)`, `~5351`) | becomes a `GpuDevice& gpu` param |
| W1.2 precedent (mirror it) | `src/platform/RB3MeshCache.{h,cpp}`; CMake `MILO_ENGINE_GPU_PLATFORM_SOURCES_RB3` `CMakeLists.txt:~319` | RB3-only twin pattern |
| DC3 conceptual twin (reference ONLY, do not merge) | `MaterialSetup::BuildMaterialParams` `MaterialSetup.cpp:~54` | diverges heavily |

### Verified facts (from investigation — the seam is clean)

- The move region reads exactly these non-local inputs: `mesh`, `mat`, `skinned`,
  `owner` (all locals/params) and `mGpu` (the sole BandRnd member, only via
  `UploadRndTexIfNeeded`). It also calls the two `static` tex helpers.
- All the block's `static int sX = -1;` caches (`hubFixOff`, `sCrowdDimOff`,
  `sCrowdDim`, `sTrackLight`, `sWmOff`, `sWmDim`, `sFretGlowOff`) are **function/
  block-local statics** — they move with the code with zero shared-state concerns.
- `meshName`, `matName`, `c`, `dt`, `hasTex`, `isLikelyUiText`, `isUiHighlightOverlay`
  are **declared inside** the region and **not read after** it (grep confirms: none
  appear past `~5730`). They stay internal to the binder.
- Only `isTextMeshHeur` and `gemForce` are read after the region → the binder must
  **return** them alongside `mu`.
- `mu` is only *read* after the region (`WriteBuffer(slot.matUB,0,&mu,…)` `~5732`) —
  never mutated — so returning it by value and using it is safe.
- **`RB3_DRAWLOG` does NOT record material-uniform VALUES** (only pipeline hash,
  blend/zmode/layout/flags/counts/world/bind-group *pointers* — see `RecordDrawLog`
  `~5900`). So a wrong `mu.color` would NOT show in a drawlog diff. **The
  screenshot pixel hash is the authoritative MOVE gate**; drawlog is corroboration
  of the bind-group/pipeline structure only.

---

## Proposed interface

```cpp
// src/platform/RB3MaterialBinder.h  (RB3-only TU; twin of DC3 gfx/MaterialSetup)
#pragma once
#include "platform/Rnd_Wgpu.h"   // MaterialUniforms
#include <webgpu/webgpu_cpp.h>
class RndMesh; class RndMat; class RndTex; class GpuDevice;

// Cross-TU decls for the two tex-resolve helpers that live in Rnd_Wgpu_RB3.cpp
// (de-static'd in S1 so the binder can resolve/upload a material's diffuse view).
wgpu::TextureView GetRB3TexView(RndTex* tex);
wgpu::TextureView UploadRndTexIfNeeded(GpuDevice& gpu, RndTex* tex);

struct RB3MaterialBindResult {
    MaterialUniforms mu;
    bool isTextMeshHeur = false;  // consumed by DrawMesh pipeline-key (zMode)
    bool gemForce       = false;  // consumed by DrawMesh pipeline-key (blend/zMode)
};

// Verbatim relocation of the DrawMesh material→uniform block. Pure translation:
// no GPU submits other than the lazy diffuse upload the block already did.
RB3MaterialBindResult RB3BuildMaterialUniforms(
    RndMesh* mesh, RndMat* mat, bool skinned, RndMesh* owner, GpuDevice& gpu);
```

`DrawMesh` call site replaces the deleted block with:

```cpp
RB3MaterialBindResult matRes = RB3BuildMaterialUniforms(mesh, mat, skinned, owner, mGpu);
MaterialUniforms& mu   = matRes.mu;
bool isTextMeshHeur    = matRes.isTextMeshHeur;
bool gemForce          = matRes.gemForce;
// ...unchanged: slot.matUB / WriteBuffer(&mu) / ResolveMaterialViews / MakeMaterialBindGroupCached...
// ...unchanged: PipelineKey blend/zmode block (uses isTextMeshHeur, gemForce)...
```

This is a **true textual move**: inside the function body the only edits are
`mGpu` → `gpu` (the param) and hoisting the three returned locals into the result
struct. Every asset-name branch, comment, probe, and `static` cache is copied
byte-for-byte.

---

## Subtasks

### W1.3.S1 — Expose tex-resolve helpers + create binder header  (model: opus)
**Goal:** make `GetRB3TexView` + `UploadRndTexIfNeeded` linkable from another TU and
land the `RB3MaterialBinder.h` interface, with **zero behaviour change and zero
call-site change** (a linkage-only MOVE).
**Files:** `src/platform/Rnd_Wgpu_RB3.cpp` (remove `static` on the two helper
defs; `#include "platform/RB3MaterialBinder.h"`), NEW `src/platform/RB3MaterialBinder.h`.
**Approach:**
1. Create `RB3MaterialBinder.h` with: the two extern tex-helper decls, the
   `RB3MaterialBindResult` struct, and the `RB3BuildMaterialUniforms` declaration
   (signature above). Header-guard `#pragma once`. Forward-declare `RndMesh/RndMat/
   RndTex/GpuDevice`; include `platform/Rnd_Wgpu.h` for `MaterialUniforms` +
   `<webgpu/webgpu_cpp.h>` for `wgpu::TextureView` (match how `RB3MeshCache.h`
   includes them).
2. In `Rnd_Wgpu_RB3.cpp`: delete the `static` keyword on the `UploadRndTexIfNeeded`
   (`~531`) and `GetRB3TexView` (`~800`) definitions; add
   `#include "platform/RB3MaterialBinder.h"` near the other platform includes so the
   definitions are checked against the header decls. Do **not** add a `.cpp` or CMake
   entry yet (no def for `RB3BuildMaterialUniforms` exists — it is declared but never
   referenced, which links fine).
3. Build `rb3-native` in `native/build-agent-W1.3`. Confirm clean.
**Verification / evidence:** `cmake --build native/build-agent-W1.3 --target rb3-native -j8`
clean. Because no code path changed, capture the baseline screenshot NOW (see
Evidence Procedure step 1) — this doubles as the S2 baseline. Commit
`W1.3: expose GetRB3TexView/UploadRndTexIfNeeded for cross-TU binder (linkage MOVE)`
(stage only the two files, `flock /tmp/milo-engine-git.lock`).

### W1.3.S2 — Move the material→uniform block into `RB3BuildMaterialUniforms`  (model: opus)
**Goal:** the big verbatim MOVE + call-site rewire + CMake. Byte-identical output.
**Files:** NEW `src/platform/RB3MaterialBinder.cpp`; `src/platform/Rnd_Wgpu_RB3.cpp`
(delete block + insert call); `CMakeLists.txt` (add cpp to `…_SOURCES_RB3`).
**Approach:**
1. In `RB3MaterialBinder.cpp`: `#include "platform/RB3MaterialBinder.h"` plus every
   include the block needs (grep the block for types: `rndobj/Mat.h`, `rndobj/Mesh.h`,
   `rndobj/Tex.h`, `rndobj/Cam.h`, `rndobj/Environ.h`, `math/Mtx.h`, `<algorithm>`,
   `<cmath>`, `<cstring>`, `<cstdio>`, `<string>`, `<unordered_map>` — copy the exact
   set already `#include`d atop `Rnd_Wgpu_RB3.cpp` that these lines rely on).
2. Define `RB3BuildMaterialUniforms(mesh, mat, skinned, owner, gpu)`:
   - `RB3MaterialBindResult r{}; MaterialUniforms& mu = r.mu;`
   - paste the region **verbatim** from `MaterialUniforms mu{};` (drop the redundant
     re-declaration — reuse `r.mu`) through the end of the `gemForce`/`GEM_FORCE`
     block.
   - `bool isTextMeshHeur` and `bool gemForce` become `r.isTextMeshHeur` /
     `r.gemForce` (or local `bool`s assigned into `r` at `return`). Keep every other
     line, comment, `static` cache, and probe **unchanged**.
   - the single member ref `mGpu` (in `UploadRndTexIfNeeded(mGpu, dt)`) becomes `gpu`.
   - `return r;`
3. In `DrawMesh`: delete the moved region; insert the call + three-line unpack
   (interface section above). The following `slot.matUB`/bind-group and `PipelineKey`
   code is untouched and now reads `mu`/`isTextMeshHeur`/`gemForce` from the unpack.
4. CMake: add `    src/platform/RB3MaterialBinder.cpp    # W1.3: material→MaterialUniforms binder (RB3-only; twin of DC3 MaterialSetup)`
   to `MILO_ENGINE_GPU_PLATFORM_SOURCES_RB3` (`~319`).
5. Reconfigure + build `rb3-native` and `rb3-tests`. Confirm clean.
**Verification / evidence:** run the **full Evidence Procedure** below (screenshot
sha256 identical vs the S1 baseline; drawlog structural diff empty; lineup-gate PASS;
`grep -c 'MaterialUniforms mu{}' Rnd_Wgpu_RB3.cpp` → 0). Commit
`W1.3: extract RB3BuildMaterialUniforms into RB3MaterialBinder TU (MOVE, byte-identical)`
(stage `RB3MaterialBinder.cpp`, `Rnd_Wgpu_RB3.cpp`, `CMakeLists.txt`; flock).

### W1.3.S3 — Independent verify + evidence + STATUS  (model: sonnet)
**Goal:** an author-independent confirmation the MOVE is byte-identical, plus the
recorded evidence trail the wave protocol requires.
**Files:** `docs/native/engine-arch-review-2026-07-05/execution/W1.3/STATUS.md`
(append under `flock /tmp/rb3-docs.lock`). No source edits.
**Approach:**
1. Clean-build `rb3-native` + `rb3-tests` at the S2 commit in `native/build-agent-W1.3`.
2. Re-run the Evidence Procedure end-to-end from the **parent of S1** vs the **S2
   commit** (two builds, own dir): assert screenshot sha256 equality; assert
   `drawlog-golden.py`/structural diff empty; assert `lineup-gate.py` PASS.
3. Build + run `milo-engine-tests` (working recipe in `W0.1/STATUS.md`): confirm the
   W0.1 skin golden + W0.4 `ClipPoseFixture` are green (the 29 pre-existing
   dc3-drift failures are expected and untouched — see `W0.1/STATUS.md`; do not
   attribute them to W1.3).
4. Confirm the asset-name special-cases relocated unchanged: `git show <S2>` restricted
   to the moved lines is a pure relocation (same bytes, new file) — spot-check
   `crowd`, `tail_`, `surface.mat`, `gem_smasher_glow`, `peakstate`, `highlight_main`,
   `GEM_FORCE`.
5. Append STATUS.md sections (`## W1.3.S1 — done`, `## W1.3.S2 — done`,
   `## W1.3.S3 — done`) with commit SHAs, the screenshot sha256, and gate results.

---

## Evidence Procedure (REQUIRED byte-identical gate for every MOVE commit)

The drawlog cannot see `mu` values, so **the deterministic-scene screenshot hash is
the authoritative gate.** Use the fixed clock so the frame is reproducible.

1. **Baseline (once, at parent-of-S1):** build `rb3-native` in
   `native/build-agent-W1.3`; run headless and drive to a deterministic scene, then
   capture:
   ```bash
   RB3_HTTP=1 RB3_FIXED_CLOCK=1 <run rb3-native>   # W0.3b fixed-clock seam
   # scripted nav (reuse scripts/native/song-select-capture.py pattern) to a fixed screen
   curl -s localhost:<port>/api/screenshot -o /tmp/w13-base.png
   sha256sum /tmp/w13-base.png                      # record in STATUS.md
   # structural: RB3_DRAWLOG=1 RB3_DRAWLOG_DUMP=/tmp/w13-base.drawlog.json at same frame
   ```
   If the fixed-clock frame still jitters (W0.3 splash draw-count note), prefer the
   **lineup scene** via `scripts/native/patch-lineup-capture.py` (deterministic; it
   backs the committed `scripts/native/goldens/w0.5-lineup/` golden) as the capture
   point, and compare pixelwise (`lineup-gate.py` image layer, zero-diff for a MOVE).
2. **After each MOVE commit:** rebuild in the same dir; recapture `/tmp/w13-after.png`
   + `/tmp/w13-after.drawlog.json` at the identical scene.
3. **Assert:** `sha256(base.png) == sha256(after.png)` (pure MOVE ⇒ exact). Structural
   drawlog diff empty. `scripts/native/lineup-gate.py` PASS against the committed
   golden. Record all three in STATUS.md; a mismatch means the MOVE changed behaviour
   — STOP, do not commit, diagnose (most likely an accidental edit to a moved line or
   a missed `mGpu`→`gpu`/return-value hookup).

---

## Exit criteria (measurable)

1. `src/platform/RB3MaterialBinder.{h,cpp}` exist; `RB3BuildMaterialUniforms` is
   defined there and wired into `MILO_ENGINE_GPU_PLATFORM_SOURCES_RB3`.
2. `grep -c 'MaterialUniforms mu{}' src/platform/Rnd_Wgpu_RB3.cpp` → `0`
   (the block is gone from `DrawMesh`); the only `mu{}`/`MaterialUniforms mu` fill is
   in `RB3MaterialBinder.cpp`.
3. `rb3-native`, `rb3-tests`, and `milo-engine-tests` build clean in
   `native/build-agent-W1.3` (never in `build-native`/`build-web*`).
4. **Deterministic-scene screenshot sha256 is identical** parent-of-S1 vs S2
   (recorded in STATUS.md); RB3_DRAWLOG structural diff empty; `lineup-gate.py` PASS.
5. `milo-engine-tests` W0.1 skin-golden + W0.4 ClipPoseFixture green (no new failures
   beyond the 29 documented dc3-drift ones).
6. Every commit is a pure MOVE, message starts `W1.3:`, stages only W1.3-owned files,
   made under `flock /tmp/milo-engine-git.lock`. `MILO_ENGINE_PIN` untouched.
7. The asset-name special-cases (crowd dim, `tail_*`, track-light surface/rails/
   gem_smasher/peakstate, `gem_force`, `highlight_*`, debug probes) are relocated
   **unchanged** — a diff of the moved lines is relocation-only. (W1.7 will later move
   them behind the game hook; W1.3 does not touch their logic or flags.)

---

## Risks / conflicts

- **Lane A is sequential on `Rnd_Wgpu_RB3.cpp`.** Assume W1.2 (`RB3MeshCache`,
  engine `6f9d340`) is in — it is. W1.4 (postproc/halo/quad, ~`2665`–`3327`) and
  W1.5 (uniform-ring) run **after** W1.3 and touch different regions — no overlap
  with the material block (`~5224`–`5719`). Do not pre-empt them.
- **W1.6 owns the bind-group restructure.** W1.3 deliberately leaves `slot.matUB` /
  `ResolveMaterialViews` / `MakeMaterialBindGroupCached` and the create-counters in
  `DrawMesh`. Do NOT move them — W1.6 replaces that path with an immutable-per-write
  scene bind group + `DrawContext`. Moving it here would create a merge collision and
  is out of W1.3 scope.
- **Do NOT converge onto `MaterialSetup.cpp`.** The RB3 block diverges from DC3's
  `BuildMaterialParams` by ~490 lines of RB3-specific logic; a merge would change
  behaviour on both backends. New RB3-only TU only (twin of `RB3MeshCache`).
- **Drawlog blindness to `mu` values** — the verifier must NOT treat an empty drawlog
  diff as sufficient. Screenshot pixel/sha equality is the required gate.
- **Determinism** — splash boot draw-count jitters with wall-clock (W0.3). Use
  `RB3_FIXED_CLOCK` and/or the deterministic `patch-lineup-capture.py` scene as the
  capture point; if any residual jitter, gate on pixelwise-identical + lineup-gate
  PASS rather than raw boot-frame sha.
- **Hard rules (README §Hard rules):** own build dir only; `flock` per-repo git;
  never `git reset/rebase/checkout--/restore` on the shared tree; never bump the pin;
  never touch a sibling lane's lines — flag in STATUS.md instead.
- **Lazy diffuse upload stays inside the binder** (option: pass `GpuDevice& gpu` +
  de-static the two helpers, per S1) so the move is a **true textual relocation with
  zero reorder** of the `UploadRndTexIfNeeded` call. Do not "optimise" by hoisting the
  upload to the call site — that would be a behaviour reorder mixed into a MOVE
  (violates hard rule 1) even if output-identical.
