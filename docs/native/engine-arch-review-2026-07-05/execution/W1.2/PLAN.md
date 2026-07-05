# W1.2 — Extract the RB3MeshEntry mesh-upload cache into its own TU

**KEY:** `W1.2` (commit prefix `W1.2:`) · **Lane:** A (sequential, first Wave-2 item on
`src/platform/Rnd_Wgpu_RB3.cpp`) · **Repo:** engine (`/home/free/code/milohax/milo-native-engine`,
its own git) · **Type:** pure MOVE commits (behavior-preserving), byte-identical gate per commit.

Parent: `REFACTOR_PLAN.md` §W1.2 · Lane doc: `01-renderer-core.md` §1b, §5a (`MeshUploadCache`),
§5b step 2. Wave protocol + HARD RULES 1–8: `execution/README.md` (binding).

---

## Objective

Move the per-mesh GPU **upload cache** — the `RB3MeshEntry` map, its keying/fingerprint,
invalidation, CPU vertex unpack, and the buffer-creation upload helper — out of the 6,853-line
`src/platform/Rnd_Wgpu_RB3.cpp` monolith into a dedicated engine TU
`src/platform/RB3MeshCache.{h,cpp}`. The public surface is exactly the symbols `DrawMesh`,
`WarmGpuForDir`, `BeginFrame`/`EndFrame`, and the `RndMesh` virtual bodies still call.

**Convergence stance (from the brief + measured below): DO NOT converge onto the shared
`gfx`/`platform` copies. Keep RB3's version verbatim.** The old comment (`Rnd_Wgpu_RB3.cpp:389`)
cites `MeshGpuCache::EnsureMeshUploaded` as the copy source, but the two have **diverged
materially** since (see §Convergence analysis). Convergence is a later CHANGE commit, out of
W1.2 scope. W1.2 only relocates; it changes no bytes of output.

---

## Faithful-reference citations (CURRENT line numbers — re-grep, they drift)

All in `src/platform/Rnd_Wgpu_RB3.cpp` unless noted. Verify with a fresh
`grep -n` before editing (Wave-1 shifted ~700 WGSL lines + added the draw-log ring).

| Piece | Current lines | Role |
|---|---|---|
| `struct RB3MeshEntry` (+ nested `UniformSlot`) | ~396–472 | Cache-entry data: VB/IB handles, fingerprint fields, L1 skinned-vert cache, per-instance uniform slots |
| `static std::unordered_map<RndMesh*, RB3MeshEntry> sMeshGpu;` | ~474 | The cache map (keyed by drawn mesh) |
| `static std::unordered_map<RndMesh*, uint32_t> sGeomSyncGen;` | ~497 | Per-owner geom-generation counter (owner-proxy invalidation) |
| `LookupGeomSyncGen(RndMesh*)` | ~498–501 | Reads the gen counter |
| `sMeshBufCreatesThisFrame` / `sMeshBGCreatesThisFrame` | 507 / 508 | Per-frame CreateBuffer leak counters (reset 1647–1648, logged 1811, bumped 4100/4118 + in `RB3EnsureMeshGpu`) |
| `sFrameSeq` | 516 | Frame sequence for lazy per-frame slot reset (bumped 1651, read 4177) |
| `CleanupGpuMesh(RndMesh*)` | ~519–527 | Strong def displacing the weak link-stub; erases both maps |
| `XboxCVert` + `BeFloat`/`Half2Float`/`BeUV`/`BeColor`/`BeDec4n`/`BeUDec4n`/`BeUByte4` | 1945–1996 | Xbox-compressed-vert decode helpers (used ONLY by the unpack below) |
| `RB3UnpackMeshVerts(RndMesh*, bool, vector&, vector&)` | 2008–2078 | CPU vertex unpack (uncompressed `mVerts` OR compressed `mCompressedVerts`) |
| `RB3EnsureMeshGpu(BandRnd&, RndMesh*)` | ~6268–6370 | Idempotent upload helper (unpack + VB/IB CreateBuffer + fingerprint stamp + L1 warm). Used by `WarmGpuForDir` |
| `RndMesh::OnSync(int)` | ~6415–6435 | Dirty signal: clears `uploaded`, bumps `sGeomSyncGen` |
| In-`DrawMesh` inline upload block | ~3747 (`sMeshGpu[mesh]`) … ~4133 (stamp) | The per-draw copy of the same upload — **STAYS** (interleaved with slot/sphere/debug; moving it = CHANGE, not MOVE) |

Convergence targets (read-only reference): `src/platform/MeshGpuCache.{h,cpp}` (DC3 backend,
`GpuMeshData`, 365 lines) and `src/gfx/VertexFormats.{h,cpp}` (`Unpack*Vertices`, 385 lines).

### Key facts established during planning (do not re-derive)
- **`mGpu` is public** on `BandRnd` (`Rnd_Wgpu_RB3.h:262`, under `public:` at 261) and there is a
  `GpuDevice& Gpu()` accessor (`:135`). `RB3EnsureMeshGpu` already uses `rnd.mGpu.Device()/Queue()`
  — it can move to another TU unchanged (or switch to `rnd.Gpu().Device()`; either is byte-identical).
- **No ODR clash.** `MeshGpuCache.cpp` (which also defines a `CleanupGpuMesh`) is in the **DC3**
  source set (`CMakeLists.txt:305`), mutually exclusive at configure time with the **RB3** set
  (`:320`, `MILO_ENGINE_GPU_PLATFORM_SOURCES_RB3`). `RB3MeshCache.cpp` goes in the RB3 set ONLY;
  the two are never compiled together (`MILO_ENGINE_GPU_BACKEND={dc3,rb3}` picks one).
- **Be\*/XboxCVert/Half2Float are used ONLY by `RB3UnpackMeshVerts`** (grep: all call sites are
  2051–2069). They move cleanly with it. Not referenced by the DXT texture path (that uses
  `Decompress*`/`TexFingerprint`, a separate family that STAYS).
- **`sFrameSeq` and the two create-counters are frame-lifecycle**, owned by BeginFrame/EndFrame/
  DrawMesh, which all STAY. Keep `sFrameSeq` entirely in `Rnd_Wgpu_RB3.cpp` (the cache TU never
  needs it). Keep the two counters DEFINED in `Rnd_Wgpu_RB3.cpp`; only add `extern` decls to the
  new header so the moved `RB3EnsureMeshGpu` can still bump `sMeshBufCreatesThisFrame`.

---

## Subtasks

Subtasks are **sequential** (each edits `Rnd_Wgpu_RB3.cpp` + the new TU + once `CMakeLists.txt`).
Every subtask is its own commit(s) and passes the byte-identical gate (§Evidence procedure)
before the next begins.

### W1.2.S0 — capture the baseline (model: sonnet)
**Goal:** Freeze the pre-change render output so every later commit can prove byte-identity
against it. **Do this FIRST, on unmodified engine HEAD.**
- **Files:** none in source; writes evidence PNG/JSON under
  `docs/native/engine-arch-review-2026-07-05/execution/W1.2/evidence/baseline/`.
- **Steps:**
  1. Configure own build dir (reuse if present):
     `cmake -B /home/free/code/milohax/rb3/native/build-agent-W1.2 -S /home/free/code/milohax/rb3/native`
     then `cmake --build /home/free/code/milohax/rb3/native/build-agent-W1.2 --target rb3-native -j8`.
  2. Run the committed lineup gate (deterministic 2-char lineup exercising skinned mesh upload):
     `python3 scripts/native/lineup-gate.py` (from repo root, with the freshly built binary on the
     path the script expects — read the script header). Confirm it PASSES on the baseline. Copy the
     captured WIDE PNGs it produces into `evidence/baseline/` and record their `sha256sum`.
  3. Also capture a deterministic song-select screenshot hash for corroboration:
     `RB3_HTTP=1 <rb3-native> &` then drive via `scripts/native/song-select-capture.py`; save the PNG
     + `sha256sum` to `evidence/baseline/`.
  4. (Corroborating, non-gating) `RB3_DRAWLOG=1` at the lineup scene → dump `/api/drawlog`, save to
     `evidence/baseline/drawlog.json`.
  5. Build + run `milo-engine-tests` (recipe in `execution/W0.1/STATUS.md`); record the pass set
     (`SkinGolden.*`, `ClipPoseFixture.*` MUST be green; the 29 pre-existing dc3-drift failures are
     characterized in `W0.1/STATUS.md` and excluded).
- **Verify:** lineup-gate PASS recorded; baseline hashes written; append `## W1.2.S0 — done` to
  STATUS.md with the hashes.

### W1.2.S1 — create the TU + move DATA / KEYING / INVALIDATION (model: opus)
**Goal:** Stand up `RB3MeshCache.{h,cpp}` and relocate the cache's data structures, maps, keying,
and invalidation. Correctness-critical: this is the one subtask that changes *linkage*
(file-static → external) of the maps/counters, and preserves the weak-stub-displacing strong def
of `CleanupGpuMesh`.
- **Files:** NEW `src/platform/RB3MeshCache.h`, NEW `src/platform/RB3MeshCache.cpp`;
  edit `src/platform/Rnd_Wgpu_RB3.cpp`; edit `CMakeLists.txt` (RB3 set only).
- **Move into `RB3MeshCache.h`** (verbatim, same field order/comments):
  - `struct RB3MeshEntry` incl. nested `struct UniformSlot`.
  - `extern std::unordered_map<RndMesh*, RB3MeshEntry> sMeshGpu;`
  - `extern std::unordered_map<RndMesh*, uint32_t> sGeomSyncGen;`
  - `inline uint32_t LookupGeomSyncGen(RndMesh*);` (or declare + define in .cpp — pick one, keep
    the body byte-identical).
  - `void CleanupGpuMesh(RndMesh*);` (declaration).
  - `extern int sMeshBufCreatesThisFrame; extern int sMeshBGCreatesThisFrame;` (decls only — the
    DEFINITIONS stay in `Rnd_Wgpu_RB3.cpp`; needed by S3's moved `RB3EnsureMeshGpu`).
  - Header needs: `#include <webgpu/webgpu_cpp.h>`, `#include "gfx/VertexFormats.h"` (for
    `GpuVertexSkinned`), `#include <unordered_map>`, `#include <vector>`, `#include <cstdint>`;
    forward-declare `class RndMesh;`.
- **Move into `RB3MeshCache.cpp`** (verbatim):
  - Definitions of `sMeshGpu`, `sGeomSyncGen`.
  - `LookupGeomSyncGen` body (if not `inline` in header), `CleanupGpuMesh` body.
  - `RndMesh::OnSync(int)` body (references `sMeshGpu` + `sGeomSyncGen`, now co-located).
    `#include "rndobj/Mesh.h"` in the .cpp.
- **Edit `Rnd_Wgpu_RB3.cpp`:** delete the moved definitions; add `#include "platform/RB3MeshCache.h"`
  near the other platform includes (~line 23). All existing call sites (`sMeshGpu[mesh]`,
  `LookupGeomSyncGen(...)`, `RB3MeshEntry::UniformSlot`, `CleanupGpuMesh`) resolve through the header
  unchanged — **do not touch call-site code**.
- **Edit `CMakeLists.txt`:** add `src/platform/RB3MeshCache.cpp` to
  `MILO_ENGINE_GPU_PLATFORM_SOURCES_RB3` (the block at ~320) — RB3 set ONLY. Do NOT add to the DC3
  set.
- **Linkage note (the one risk):** `sMeshGpu`/`sGeomSyncGen`/the two counters change from
  `static` (internal) to external linkage. This is behavior-identical (single instance, unique
  names, RB3-only TU). Verify no duplicate-symbol link error and no second definition
  (`grep -rn "sMeshGpu" src/` should show exactly one definition after the move). Keep the names in
  the global namespace (lowest call-site churn); do NOT wrap in a namespace (would force editing
  every call site = more risk).
- **Verify:** build `rb3-native` + `rb3-tests` in `build-agent-W1.2`; run the byte-identical gate
  (§Evidence). Commit `W1.2: extract RB3MeshEntry cache data + invalidation into RB3MeshCache TU`.

### W1.2.S2 — move the CPU vertex-unpack helper (model: sonnet)
**Goal:** Relocate the pure unpack function + its decode helpers. Mechanical: zero coupling to
`BandRnd` members.
- **Files:** `src/platform/RB3MeshCache.{h,cpp}`, `src/platform/Rnd_Wgpu_RB3.cpp`.
- **Move to `RB3MeshCache.cpp`** (verbatim): `struct XboxCVert` + `BeFloat`/`Half2Float`/`BeUV`/
  `BeColor`/`BeDec4n`/`BeUDec4n`/`BeUByte4` (1945–1996) + `RB3UnpackMeshVerts` (2008–2078). Keep
  `XboxCVert` + the `Be*` helpers file-static inside `RB3MeshCache.cpp` (they have no external
  consumer). Export only `RB3UnpackMeshVerts` via the header:
  `int RB3UnpackMeshVerts(RndMesh* owner, bool skinned, std::vector<GpuVertex>& gpuVerts, std::vector<GpuVertexSkinned>& gpuVertsSkinned);`
  (note the header uses `GpuVertex`; `GpuVertexRB3` is an alias for it — either compiles, prefer
  `GpuVertex` in the shared header to avoid pulling the RB3-only alias).
- **Edit `Rnd_Wgpu_RB3.cpp`:** delete the moved bodies; the two call sites (DrawMesh ~3821, and —
  after S3 — none, since `RB3EnsureMeshGpu` moves too) resolve via the header. Do not touch call code.
- **Verify:** build + byte-identical gate. Commit
  `W1.2: move RB3UnpackMeshVerts + Xbox-cvert decode helpers into RB3MeshCache`.

### W1.2.S3 — move the buffer-creation upload helper (model: opus)
**Goal:** Relocate `RB3EnsureMeshGpu` (the VB/IB CreateBuffer + WriteBuffer + fingerprint-stamp
path used by the L2 warm sweep). Correctness-sensitive: it touches `BandRnd` (`mGpu`) and the
extern counters from S1.
- **Files:** `src/platform/RB3MeshCache.{h,cpp}`, `src/platform/Rnd_Wgpu_RB3.cpp`.
- **Move to `RB3MeshCache.cpp`** (verbatim): `RB3EnsureMeshGpu(BandRnd&, RndMesh*)` (~6268–6370).
  It needs `class BandRnd` visible — `#include "platform/Rnd_Wgpu_RB3.h"` in `RB3MeshCache.cpp`
  (NOT in the header — keep the header light, forward-declare `class BandRnd;` there and export
  `bool RB3EnsureMeshGpu(BandRnd&, RndMesh*);`). It also needs `rndobj/Mesh.h` (already included
  from S1) for `RndMesh::Face`/`mFaces`/`mVerts`. `sMeshBufCreatesThisFrame` resolves via the
  S1 extern decl. Keep `rnd.mGpu.Device()/Queue()` exactly as-is (mGpu is public).
- **Edit `Rnd_Wgpu_RB3.cpp`:** delete the moved body; `WarmGpuForDir`’s call
  `RB3EnsureMeshGpu(*this, it)` resolves via the header. Do not touch `WarmGpuForDir`.
- **DO NOT** touch the in-`DrawMesh` inline upload block (~3747–4133). It is interleaved with the
  per-instance uniform-slot claim, the local-sphere recompute, and debug probes; extracting or
  deduping it against `RB3EnsureMeshGpu` is a behavior CHANGE, explicitly out of MOVE scope.
  Record it in STATUS.md as a follow-up convergence candidate (a later CHANGE commit could route
  DrawMesh through `RB3EnsureMeshGpu`, but only under the draw-log golden once W0.3b lands).
- **Verify:** build + byte-identical gate (this one exercises `WarmGpuForDir`, so include a
  warm-path scene — the lineup gate boots through a loading dwell that calls it). Commit
  `W1.2: move RB3EnsureMeshGpu upload helper into RB3MeshCache`.

### W1.2.S4 — convergence analysis + delta note (model: opus, doc-only)
**Goal:** Satisfy the brief's "diff to prove identity, else note the delta" requirement. No source
change.
- **Files:** append to `execution/W1.2/STATUS.md` (under `flock /tmp/rb3-docs.lock`); optional
  `evidence/convergence-notes.md`.
- **Steps:** Produce a written diff verdict for each candidate:
  1. `RB3UnpackMeshVerts` vs `gfx/VertexFormats.cpp::UnpackStaticVertices/UnpackSkinnedVertices/
     UnpackCompressedVertices/UnpackCompressedSkinnedVertices`. **Expected verdict: NOT identical**
     — different signatures (`vector&` vs `out[]+maxVerts`), RB3 forces `tangent=(1,0,0,1)` for
     uncompressed verts, and the compressed record differs (`XboxCVert` = 36 B with `int`-field
     layout + `Be*` decode vs `CompressedVertex_Xbox` with SWAPPED bone fields). Keep verbatim.
  2. `RB3MeshEntry`/`sMeshGpu` vs `platform/MeshGpuCache.cpp::GpuMeshData`. **Expected verdict: NOT
     identical** — RB3 adds per-instance `UniformSlot` vectors, the `sGeomSyncGen` owner-gen
     invalidation, the L1 `cachedSkinnedVerts` cache, and a richer fingerprint; DC3's is a plain
     VB/IB entry. Keep verbatim; convergence is a future CHANGE.
  3. State the concrete convergence path for a later wave (a `MeshUploadCache` interface both
     backends implement) but confirm it is OUT of W1.2 scope.
- **Verify:** verdicts recorded in STATUS.md with the exact divergence points. No build needed.

---

## Byte-identical evidence procedure (REQUIRED per MOVE commit)

These are pure relocations (same statements; only static→extern linkage changes). The gate proves
render output is unchanged at a **deterministic** scene. Splash boot is wall-clock nondeterministic
(W0.3 found draw count 885–888 across boots) — so **do not** gate on raw boot draw counts.

**Per commit (S1, S2, S3):**
1. **Build clean** in the own dir: `cmake --build /home/free/code/milohax/rb3/native/build-agent-W1.2
   --target rb3-native -j8` (and `rb3-tests` for S1). Zero new warnings from the moved code.
2. **PRIMARY — lineup gate (deterministic, exercises skinned mesh upload + warm path):**
   `python3 scripts/native/lineup-gate.py` → must PASS all four layers (img / segA / ratioB /
   countC). Then compare the captured WIDE PNGs' `sha256sum` against `evidence/baseline/` — **must
   be byte-identical** (fixed-pose capture is deterministic). Record PASS + hash match in STATUS.md.
3. **CORROBORATING — song-select screenshot hash:** re-capture via
   `scripts/native/song-select-capture.py`; `sha256sum` must equal the S0 baseline.
4. **CORROBORATING — draw-log diff (non-gating until W0.3b):** `RB3_DRAWLOG=1` at the lineup scene,
   dump `/api/drawlog`, diff vs `evidence/baseline/drawlog.json` with
   `scripts/native/drawlog-golden.py`. Per-draw pipeline/blend/bind-group/count fields must match
   (raw wgpu buffer handles may differ — ignore handle identity, compare structural fields). If
   noisy, note it and rely on 2–3.
5. **milo-engine-tests** stay green (`SkinGolden.*`, `ClipPoseFixture.*`); the 29 known dc3-drift
   failures remain excluded.
6. Append `## W1.2.S<n> — done` to STATUS.md (flock `/tmp/rb3-docs.lock`) with commit SHA, the
   lineup PASS, and the before/after hash equality line.

If ANY hash differs or the lineup gate regresses: the commit is NOT a clean MOVE — STOP, do not
commit, diff the moved code against the original for an accidental edit, and record the finding in
STATUS.md. Never `git reset`/`checkout --`/`restore` on the shared tree (HARD RULE 7).

---

## Exit criteria (measurable)

1. `src/platform/RB3MeshCache.h` and `src/platform/RB3MeshCache.cpp` exist and
   `RB3MeshCache.cpp` is listed in `MILO_ENGINE_GPU_PLATFORM_SOURCES_RB3` (RB3 set only).
2. In `Rnd_Wgpu_RB3.cpp`: `grep -n "struct RB3MeshEntry"` → 0; the DEFINITIONS of
   `RB3UnpackMeshVerts`, `RB3EnsureMeshGpu`, `RndMesh::OnSync`, `LookupGeomSyncGen`,
   `CleanupGpuMesh`, and the `sMeshGpu`/`sGeomSyncGen` maps → 0 (only calls / extern refs remain).
3. Exactly one definition of each moved symbol across `src/` (`grep -rn` — no ODR duplicate).
4. `rb3-native`, `rb3-tests`, and `milo-engine-tests` build clean in `build-agent-W1.2`;
   `SkinGolden.*` + `ClipPoseFixture.*` green.
5. Every W1.2 commit is MOVE-only and passes the byte-identical gate: lineup-gate PASS + WIDE-PNG
   SHA256 unchanged vs the S0 baseline, recorded in STATUS.md per commit.
6. The in-`DrawMesh` inline upload block is untouched and documented as a deferred CHANGE.
7. `RB3MeshCache.h` public surface is minimal — only `RB3MeshEntry`, `sMeshGpu` (+ `sGeomSyncGen`),
   `LookupGeomSyncGen`, `CleanupGpuMesh`, `RB3UnpackMeshVerts`, `RB3EnsureMeshGpu`, and the two
   counter externs. No incidental exports.
8. S4 convergence verdict recorded (unpack + entry both NOT provably identical → kept verbatim;
   convergence deferred to a CHANGE commit).

---

## Risks / conflicts

- **Lane A ordering.** W1.2 is the FIRST Wave-2 lane-A item on `Rnd_Wgpu_RB3.cpp`
  (W1.3→W1.4→W1.5→W1.7→W1.6 follow, each assuming W1.2's commits are in). Base is current engine
  HEAD (`9561a19` at plan time, or the coordinator's Wave-2 pin). No prior Wave-2 lane-A deps.
- **Parallel lanes.** W0.3b (frozen sim-clock seam, extends `RB3_REPLAY_FIXED_CLOCK`) and
  W2-TESTFIX (dc3-drift `milo-engine-tests`) run elsewhere. W2-TESTFIX touches test files — no
  overlap. **W0.3b may touch `Rnd_Wgpu_RB3.cpp` BeginFrame / the clock seam / `sFrameSeq` region.**
  MITIGATION: W1.2 deliberately leaves `sFrameSeq` and BeginFrame/EndFrame **entirely in place**
  (the cache TU never references `sFrameSeq`), so the two lanes edit disjoint regions. If a merge
  conflict still arises on BeginFrame, it is textual only — resolve by keeping both edits; never
  revert a W0.3b line (HARD RULE 8), flag it in STATUS.md.
- **Linkage change (static→extern)** for `sMeshGpu`/`sGeomSyncGen`/the two counters is the only
  non-trivial aspect. Unique names + RB3-only compilation ⇒ behavior-identical. Verify with a
  link + `grep -rn` single-definition check (exit criterion 3).
- **Name collision with DC3 `MeshGpuCache::CleanupGpuMesh`** is avoided ONLY because the two source
  sets are mutually exclusive at configure time. Do NOT add `RB3MeshCache.cpp` to the DC3 set, and
  do NOT add `MeshGpuCache.cpp` to the RB3 set.
- **Do NOT converge** `RB3UnpackMeshVerts` onto `gfx/VertexFormats` or `RB3MeshEntry` onto
  `MeshGpuCache::GpuMeshData` in this item — they are measurably divergent (§S4). A convergence
  attempt here would be a CHANGE masquerading as a MOVE (the exact regression class the memory
  notes warn about) and would fail the byte-identical gate anyway.
- **Concurrent engine working tree** has an unrelated agent's uncommitted `FxSendNative.cpp` audio
  edit — leave it untouched; stage only W1.2 files (`git add src/platform/RB3MeshCache.* 
  src/platform/Rnd_Wgpu_RB3.cpp CMakeLists.txt`). Flock `/tmp/milo-engine-git.lock` around
  add+commit.
