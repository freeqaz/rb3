# task-gem-tails — sustain tails render before being held (mesh-cache fix)

Wave-2 implementer for `gem-polish` sub-symptom **#1** (sustain tails only render
while held). ENGINE fix, fully designed by `scout-gem-polish.md` (note: the scout
doc is named `scout-gem-polish.md`, not `scout-gem-tails.md`). Implemented exactly
as designed; no deviation.

**Scope:** this task covers ONLY sub-symptom #1 (tails). Sub-symptoms #2 (flicker)
and #3 (colors/bloom wash) are separate tasks — I did not touch the bloom or
material-setup regions.

## Status: DONE, verified

Approach-state sustain tails now render before being held, matching the
`RB3_NO_MESH_CACHE=1` cache-off truth, with no upload storm and no regression in
song-select / menu / chars / venue / lighting.

## Root cause (confirmed, matches scout)

The per-mesh GPU cache `sMeshGpu` (in engine `src/platform/Rnd_Wgpu_RB3.cpp`) is
keyed by the **drawn** mesh pointer, but a proxy mesh draws geometry owned by a
*different* mesh via `SetGeomOwner()`. A `Tail` (rb3 `src/band3/bandtrack/Tail.cpp`)
draws two proxies `mTail1`/`mTail2`, both `SetGeomOwner(mTailGeomOwner)`; for a
chord all lanes share the first tail's owner. `Tail::UpdateVerts` rewrites the
OWNER's verts and calls `mTailGeomOwner->Sync()` (Tail.cpp:330), which routes
`RndMesh::Sync → OnSync(owner)` and clears `uploaded` only on the **owner's** entry.
The owner is never drawn, so the drawn proxies' cache entries never get the dirty
signal. The count fingerprint (`fpVerts`/`fpFaces`) only catches count changes, and
an approaching tail's section count saturates at 2 almost immediately
(`GemRepTemplate::SetupTailVerts`), so the proxy's GPU buffer froze at the first
post-saturation upload — an invisible sliver. A held tail uses 90 fine sections, so
its count changes each section boundary → fingerprint mismatch → re-upload → renders
(exactly "only while held").

Confirmed the link path: rb3-native compiles only `Rnd_Wgpu_RB3.cpp`'s
`RndMesh::OnSync` (CMake `MILO_ENGINE_GPU_PLATFORM_SOURCES_RB3`); the dc3
`MeshGpuCache.cpp::OnSync` is dc3-only and not linked here.

## What changed (engine only)

One file: `src/platform/Rnd_Wgpu_RB3.cpp` (+66 / −2), six sites, exactly the
scout's owner sync-generation design (O(1), no map sweeps):

1. **`sGeomSyncGen`** — new `static std::unordered_map<RndMesh*, uint32_t>` +
   `LookupGeomSyncGen(owner)` helper (missing entry ⇒ gen 0), near the `sMeshGpu`
   decl (~:451).
2. **`RB3MeshEntry::fpOwnerGen`** — new `uint32_t fpOwnerGen = 0` field (~:389).
3. **`CleanupGpuMesh`** (~:499) — also `sGeomSyncGen.erase(mesh)`.
4. **`DrawMesh` needUpload** (~:3320) — added
   `bool ownerGenStale = (owner != mesh) && (meshEntry.fpOwnerGen != LookupGeomSyncGen(owner));`
   folded into `needUpload`; stamp `meshEntry.fpOwnerGen = LookupGeomSyncGen(owner);`
   in the upload arm (~:3559). Gated `owner != mesh` so self-owned meshes keep their
   existing path unchanged (they already invalidate via their own OnSync entry).
5. **`RB3EnsureMeshGpu`** (the `WarmGpuForDir` twin, ~:4728) — mirrored the same
   `ownerGenStale` check and `fpOwnerGen` stamp, so warmed proxies stamp identically
   (otherwise a warmed proxy would re-upload once on first draw — harmless but
   defeats the warm win).
6. **`RndMesh::OnSync`** (~:4872) — kept the existing `uploaded=false` on this
   mesh's own entry AND added `++sGeomSyncGen[this];` to bump the owner generation.

Pointer-reuse after free is safe: a recycled owner pointer always gets `Sync()`d
(gen ≥ 1) before its first draw, so it never matches a stale stamped `0`.

The L1 unpack cache keys off the same `needUpload`, so it inherited the fix with no
separate change. No bind-group/layout change; dc3 backend and the Wii build are
untouched (engine-only, match-neutral by construction).

## Branches + commits

- **Engine** worktree `wt-task-gem-tails` (milo-native-engine):
  `27864eb975b5e1bbe1af2cddc0232929078002ca`
  — `fix(rnd-rb3): invalidate geom-owner-proxy meshes via owner sync-generation`
- **rb3** worktree `wt-task-gem-tails`:
  `d6eff2dd` — `build(native): bump MILO_ENGINE_PIN to 27864eb (...)`
  (only `native/CMakeLists.txt` changed: pin `8fb669d…` → `27864eb…`).

Engine main is untouched (still `8fb669d`); both worktrees are clean.

## Verification (all pass)

Build: cold clang build of `rb3-native` green against the paired engine worktree
(`cmake -B native/build-native -S native -DCMAKE_C_COMPILER=clang
-DCMAKE_CXX_COMPILER=clang++ -DMILO_ENGINE_PATH="$(cat .engine-path)"
-DDawn_DIR=…/dawn/lib/cmake/Dawn`). NOTE: a fresh configure defaults to GNU `c++`,
which rejects the engine's clang-only flags (`-fms-compatibility-version`,
`-fdelayed-template-parsing`) — you must pass `-DCMAKE_C_COMPILER=clang
-DCMAKE_CXX_COMPILER=clang++` (the main build dir's cached compilers).

Repro: Antibodies (Poni Hoax), expert guitar, `--song-downs 4`, jump to 15.5s; the
Y/B/O 3-lane sustain chord approaches the now-bar at songMs ~19.0–19.7s. Driver:
the scout's `/tmp/rp-gem-polish/gem_probe.py`, pointed at my worktree binary
(`/tmp/rp2-gem-tails-probe.py`).

(a) **Tails render in approach — PASS.** Key frames (full set in
`/home/free/tmp/rp2-gem-tails/`, key 10 copied to `/tmp/rp2-gem-tails/`):
- BEFORE (main-repo binary @ pre-fix pin, default env): `before_012.png`
  (songMs 19258) — gems visible, track ABOVE them EMPTY. Matches scout's
  `ea_c012_approach_NO_tails_baseline.png`.
- AFTER (my worktree binary, **default env**): `after_011/012/013.png`
  (songMs 19089/19402/19692) — three colored tails (Y/B/O) extend from the chord
  gems to the top of the track, stable across the whole approach.
- A/B truth: my binary with `RB3_NO_MESH_CACHE=1` (`co_010.png`, songMs 19312) is
  visually identical to my default-env AFTER — i.e. the fix achieves cache-off
  correctness with the cache ON (the scout's PASS criterion).

(b) **No regression — PASS.**
- Song-select (`scripts/native/song-select-capture.py`, depths 0/8/16/30):
  `native_depth_08.png` / `native_depth_30.png` — list text crisp, scroll clean,
  difficulty grid + Friend Rankings render; no stale-glyph overlap (the RndText
  Sync path shares `OnSync`/`uploaded=false` and is unaffected).
- Main menu (`main_hub.png`) renders correctly (PLAY NOW/CAREER/… + backdrop art).
- Gameplay chars/venue/lighting: BEFORE vs AFTER `*_005.png` are essentially
  identical (chars at the same partial-render state — that's the separate
  `char-render` task, NOT caused here; skinned meshes use the owner==mesh path I
  left unchanged).

(c) **No per-frame re-upload storm — PASS.** `RB3_RENDER_DBG=1` per-frame
`buf_creates` over a tail-active window: **98.7% of 8431 frames had buf_creates=0**;
the nonzero steady-state frames were `buf_creates=6` while ~412 meshes were in
scene — i.e. only the ~6 active tail proxies (mTail1/mTail2 × ~3 lanes) re-upload
per frame, not the whole scene. Large one-off spikes are scene-load frames, not a
steady storm.

## LANDING NOTES (for the orchestrator)

- **Land order:** engine commit `27864eb` FIRST (push/merge so the SHA is
  reachable from engine main), THEN the rb3 pin-bump commit `d6eff2dd`. The rb3
  commit references the engine SHA via `MILO_ENGINE_PIN`.
- **Sibling-task conflict surface:** two other wave-2 engine tasks also edit
  `src/platform/Rnd_Wgpu_RB3.cpp` — the **bloom** task (gem-polish #3:
  `CompositeHaloBloom` ~:1894–2069 / `kRB3HaloBlitShaderSource`) and a
  **material-setup** task. My diff is confined to the **mesh-cache** sites only
  (`RB3MeshEntry` struct ~:374–445, the `sMeshGpu`/`sGeomSyncGen` decls ~:446–475,
  `CleanupGpuMesh` ~:499, `DrawMesh` needUpload+stamp ~:3320/3559, `RB3EnsureMeshGpu`
  ~:4728/4814, `RndMesh::OnSync` ~:4872) and touches **none** of the bloom or
  material regions — so a three-way cherry-pick should not conflict in content.
  Cherry-pick order between the three engine commits doesn't matter for correctness
  (disjoint regions); resolve any positional/line-context conflicts by keeping all
  three sets of hunks. Only ONE rb3 pin-bump should win in `native/CMakeLists.txt`
  — set it to the final merged engine SHA after all three engine commits land.
- **No Wii / match impact:** engine-only, `#ifdef`-free changes live entirely in
  the native-only engine backend; the rb3 change is a one-line pin bump in
  `native/CMakeLists.txt`. No `src/band3/` or `src/system/` source touched, so no
  objdiff re-verification needed.
- **Compiler gotcha (above):** if the orchestrator rebuilds in a fresh dir, pass
  `-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++` or configure picks GNU
  c++ and fails on clang-only flags.

## Evidence index

- `/tmp/rp2-gem-tails/` — key 10 frames (before_011-013, after_011-013, co_010,
  native_depth_08/30, main_hub).
- `/home/free/tmp/rp2-gem-tails/` — full sequences: `before/` (40), `after/` (40),
  `after-cacheoff/` (40), `songselect/` (4), `menu/`, plus `storm1/`,
  `upload-check/` engine logs (the buf_creates storm check) and `*_meta.json`
  songMs-per-shot. (Bulk lives on /home due to a /tmp quota cap hit mid-run.)
- Scout truth refs: `/tmp/rp-gem-polish/nm7_approach_TAILS_meshcache_off.png`
  (cache-off) vs `ea_c012_approach_NO_tails_baseline.png` (bug).
