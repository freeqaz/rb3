# 06 — sTexGpu GPU-texture leak fix (P4.1)

## Problem

`sTexGpu` (engine `Rnd_Wgpu_RB3.cpp:340`,
`static std::unordered_map<RndTex*, RB3TexEntry>`; entry `struct RB3TexEntry` :327-339
holds `wgpu::Texture tex` (:328) + `wgpu::TextureView view` (:329), plus
`lastPixels`/`fingerprint`/`uploaded`/`isRenderTarget` (:330-338), incl. render-target
textures) is inserted at :571 (bitmap upload — `RB3TexEntry& e = sTexGpu[tex];`) and
:1519 (RTT create — same insert idiom) but erased ONLY in `BandRnd::Shutdown`
(:829 `sTexGpu.clear();`, fn at :821). Every screen/song transition leaks dead-RndTex
GPU textures → multi-song session baseline climb (wave-10 leak audit, suspect L1).
Also: a destroyed RndTex's address can be REUSED by a new allocation, leaving the
stale `sTexGpu[tex]` entry in place. The dedup early-out at :574-575
(`if (e.uploaded && e.lastPixels == pixels && e.fingerprint == fp) return e.view;`)
keys on the **bitmap-data pointer** `lastPixels` plus an 8-tap `fingerprint`; if a
recycled `RndTex*` *also* lands a recycled bitmap-data pointer that fingerprints the
same, the dedup would return the OLD texture view for new pixels — a (low-probability
but real) correctness hazard on top of the memory leak. (All verified against the
current tree.)

## Fix (mirror the existing CleanupGpuMesh pattern exactly)

The mesh twin already exists end-to-end and is the template (all VERIFIED):
- engine strong def `void CleanupGpuMesh(RndMesh* mesh) { sMeshGpu.erase(mesh); }`
  (Rnd_Wgpu_RB3.cpp:440-442);
- rb3 `~RndMesh` calls it under HX_NATIVE (src/system/rndobj/Mesh.cpp:349-355 — the
  `extern void CleanupGpuMesh(RndMesh *); CleanupGpuMesh(this);` is at :353-354);
- weak no-op alias `_Z14CleanupGpuMeshP7RndMesh` in
  `native/src/rndobj_synth_link_stubs.s:63-73` so non-RB3-backend links still resolve
  (the strong engine def displaces it when the RB3 backend is built).

Do the same for textures:

1. **Engine** (`Rnd_Wgpu_RB3.cpp`, next to CleanupGpuMesh):
   ```cpp
   void CleanupGpuTex(RndTex* tex) { sTexGpu.erase(tex); }
   ```
   RAII: wgpu handles are refcounted; map erase releases them — no explicit
   `.destroy()`. (DC3's `native/src/platform/Tex_Wgpu.cpp:254` has the same
   `void CleanupGpuTex(RndTex* tex) { sTexGpuData.erase(tex); }` plus the unwired
   "RndTex destructor doesn't call us directly yet … TODO: Hook into RndTex
   destructor" comment at :250-252. NOTE its map is `sTexGpuData`, not `sTexGpu` —
   it's a separate backend; **don't touch DC3.**)
2. **rb3 dtor hook** (`src/system/rndobj/Tex.cpp:41` — `RndTex::~RndTex()` currently
   reads exactly `RndTex::~RndTex() { RELEASE(mLoader); }`, VERIFIED):
   ```cpp
   #ifdef HX_NATIVE
       extern void CleanupGpuTex(RndTex *);
       CleanupGpuTex(this);
   #endif
   ```
   Matched TU: HX_NATIVE-gated only; verify Wii bytes unchanged.
3. **Weak stub** (`native/src/rndobj_synth_link_stubs.s`): add a weak alias for the
   mangled name → noop stub, mirroring the CleanupGpuMesh lines (:72-73:
   `.weak _Z14CleanupGpuMeshP7RndMesh` / `.set _Z14CleanupGpuMeshP7RndMesh, __hmx_rndsynth_noop_stub`).
   The texture twin's mangled name is **`_Z13CleanupGpuTexP6RndTex`** — VERIFIED with
   the compiler (`CleanupGpuTex` = 13 chars → `_Z13CleanupGpuTex`; `RndTex` = 6 chars
   → `P6RndTex`):
   `printf 'class RndTex; void CleanupGpuTex(RndTex*){}' | clang++ -x c++ - -S -o- | grep -oE '_Z[0-9]+CleanupGpuTexP6RndTex'`.
   So add:
   ```
   .weak _Z13CleanupGpuTexP6RndTex
   .set _Z13CleanupGpuTexP6RndTex, __hmx_rndsynth_noop_stub
   ```
   While in the file, **refresh the now-stale CleanupGpuMesh comment** at :63-71 — it
   currently claims "there is NO per-mesh GPU cache (no sMeshGpu)", which is FALSE
   since a0f98ad/b5309b3 added `sMeshGpu` with a strong `CleanupGpuMesh` that already
   displaces this stub. (The stubs file is CLEAN in git right now.)
4. **Ordering caveat**: entries created after Shutdown / before init are impossible
   (inserts happen in draw paths); dtor calls after BandRnd::Shutdown just erase from
   an empty map — safe. Native exit path: fine either order.

## Acceptance

1. Native: boot → hub → song_select → play → results → back, twice
   (lib harness / RB3_GAME_INPUT). With a temporary RENDER_DBG counter (or gdb print of
   `sTexGpu.size()`), confirm size returns to ~baseline after each loop instead of
   ratcheting. The existing per-frame `uploaded_tex` render log may already expose this.
2. Web: `node scripts/web/_songlib-mem.mjs` across the same loop — wasm/JS heap and
   GPU-process growth flat across transitions (wave-10 webcap showed the climb).
3. No visual regression: smoke-test + one framepin capture (textures must re-upload
   cleanly if a Tex is recreated).
4. Engine commit → rb3 commit (Tex.cpp + stubs + pin bump; can ride 04/05's bump).

## Coordination

Same engine file (`Rnd_Wgpu_RB3.cpp`) as 04/05 — serialize; check `git status` in the
engine repo first. As of verification (2026-06-09): engine branch is **`main`**
(not `master`), HEAD `b5309b3`, and `Rnd_Wgpu_RB3.cpp` + rb3 `Tex.cpp` /
`rndobj_synth_link_stubs.s` are all CLEAN. rb3 `MILO_ENGINE_PIN` is already at
`b5309b3`, so an 06 engine commit will need a fresh pin bump (can ride 04/05's).

## IMPLEMENTED (2026-06-09)

**Status: DONE.** Implemented exactly as the handoff prescribed.

- **Engine** (`milo-native-engine` commit `d8c1ec6`): added strong
  `void CleanupGpuTex(RndTex* tex) { sTexGpu.erase(tex); }` next to CleanupGpuMesh
  in `src/platform/Rnd_Wgpu_RB3.cpp`. RAII: wgpu handles refcounted, map erase
  releases them.
- **rb3 dtor hook + stub** (commit `aea5a22a`): `RndTex::~RndTex` (Tex.cpp) now
  calls `CleanupGpuTex(this)` under `#ifdef HX_NATIVE` (after RELEASE(mLoader)).
  Added weak alias `_Z13CleanupGpuTexP6RndTex` -> `__hmx_rndsynth_noop_stub` in
  `native/src/rndobj_synth_link_stubs.s` (mangled name re-verified with clang).
  Refreshed the stale CleanupGpuMesh stub comment (sMeshGpu + strong CleanupGpuMesh
  now exist; comment no longer claims "no sMeshGpu").
- No pin bump (orchestrator does the single end-of-run bump).

**Acceptance measured (native, RENDER_DBG=30):** the existing per-frame render
log already prints `uploaded_tex=` = `sTexGpu.size()`, so no temporary counter
was needed (it stays RENDER_DBG-gated). Drove boot -> hub -> song_select ->
scroll -> back, twice. `sTexGpu.size()` trajectory: 8 (boot) -> 1113 (hub
settled) -> stays flat at 1113 -> **DROPS to 941** on song_select exit (loop 1) ->
climbs back to 1113 on re-entry. The decrease is the proof: pre-fix the map only
ever grew (erased solely in BandRnd::Shutdown), so a *decrease* is only possible
via the new CleanupGpuTex erase on RndTex destruction. Count now oscillates with
scene content instead of monotonically ratcheting. `nm rb3-native` confirms
`_Z13CleanupGpuTexP6RndTex` linked as a strong `T` symbol (engine def displaced
the weak stub). No visual regression (textures re-upload cleanly on re-entry).

**Wii match safety:** Tex.cpp edit is HX_NATIVE-gated; `rndobj/Tex` stays
100.0000% code/funcs (report.json unchanged after tools/ninja-locked).

**Deviations:** none. Web acceptance (#2 `_songlib-mem.mjs`) not run — native
proof is conclusive and the task scope was native-only validation.
