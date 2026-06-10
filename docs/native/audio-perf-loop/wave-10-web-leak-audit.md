# Wave 10 — web-leak-audit (code read, NO run)

**Probe:** static audit of the WEB path for unbounded heap/GPU retention remaining
**after** `b79cbafa` fixed the IDB warm-cache doubling. Question: does anything else
grow during steady playback → GC pressure → main-thread pause → SAB-ring underrun?

**Method:** read every Map/cache/array/listener in the web path and classified each by
(a) growth trigger (per-frame / per-asset-load / per-song / once) and (b) bounded vs
unbounded by code alone. Files read in full: `native/web/rb3_pre.js`,
`native/src/native_file.cpp`, `native/src/rb3_stream_receiver_native.cpp`,
`../milo-native-engine/src/audio/AudioDevice_Web.cpp`,
`../milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` (texture/mesh/bindgroup caches),
plus the link-stub file and the RB3 CMake GPU-backend filter.

---

## HEADLINE

The one **confirmed-by-inspection unbounded retention** still present on the deployed
web build is the **per-RndTex GPU texture cache `sTexGpu`** in
`milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp:340`. It is a
`std::unordered_map<RndTex*, RB3TexEntry>` where each value holds a **live
`wgpu::Texture` + `wgpu::TextureView` GPU handle**. Entries are **only ever inserted**
(lines 469, 1356) and the map is **only ever `.clear()`ed in `BandRnd::Shutdown()`
(line 726)** — which runs **once, at process exit**. There is **no per-texture removal
hook**: `RndTex::~RndTex` does not erase its `sTexGpu` entry. Every texture loaded
across a whole session accumulates forever.

The CleanupGpuMesh lead in the brief is **real but mis-aimed at the wrong cache** — see
below. The mesh-buffer story is GPU *churn*, not a heap *leak*; `sTexGpu` is the actual
unbounded retention.

---

## Suspects ranked by growth rate during STEADY playback

Ranking is by *steady single-song playback* growth (the user's symptom), with the
transition-time growth noted separately because that is where the texture cache actually
balloons.

### 1. `sTexGpu` — per-RndTex GPU texture cache — CONFIRMED UNBOUNDED (by inspection)
- **File:** `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp:340` (decl), insert at
  `:469` (`RB3TexEntry& e = sTexGpu[tex];`) and `:1356` (RTT), clear ONLY at `:726`
  (`sTexGpu.clear()` inside `BandRnd::Shutdown()`).
- **What grows:** one map entry per distinct `RndTex*` ever uploaded, each holding a
  `wgpu::Texture` + `wgpu::TextureView` (RB3TexEntry struct, `:327-339`). On the
  emdawnwebgpu backend these are JS-side GPU objects with a small wasm-heap shadow each;
  the bulk of the cost is GPU memory, but the never-freed JS handles + map nodes also
  add steady ArrayBuffer/JS-heap retention that V8 must scan.
- **No removal path:** grep of the whole engine shows `CleanupGpuMesh` exists but there
  is **no** `CleanupGpuTex` / `sTexGpu.erase` anywhere. `RndTex` destruction is invisible
  to the GPU backend. Re-upload (changed pixels) REUSES the same entry and RAII-drops the
  old handle (`:469` get-ref, `:543-544` overwrite) → re-upload does **not** grow the map
  (correctly ruled out as a per-frame growth source).
- **Estimated rate:**
  - *Steady single-song gameplay:* ~0 growth. The gem/highway/HUD/venue/char texture set
    is fixed once the song is loaded; `texCount` (logged at `:1186` via RB3_RENDER_DBG)
    plateaus. So this leak contributes **little to the user's *steady-playback* jitter**
    directly — its growth is event-driven, not time-driven.
  - *Transitions (the real driver):* each milo load/unload that introduces new RndTex
    objects adds permanent entries — song_select album-art / song-row textures, screen
    changes, **and every song you load → play → exit → load again**. A user navigating
    main_hub → song_select → song → results → song_select → another song churns hundreds
    of textures per loop, none of which are ever released. Over a multi-song session this
    is the classic slow climb that eventually triggers larger/longer V8 GCs and (on the
    GPU side) device pressure.
- **Severity for THIS wave:** unbounded and real, but the causation chain to *steady-
  playback* jitter is weaker than the hypothesis assumes — it grows at transitions, not
  during a single steady song. Still the #1 remaining leak to fix (per-RndTex erase hook,
  mirroring the intended-but-absent CleanupGpuMesh wiring).
- **Verdict:** `decisive=true`, confirmed unbounded by code alone.

### 2. CleanupGpuMesh / per-frame mesh GPU buffers — GPU CHURN, NOT a heap leak
- **File:** `Rnd_Wgpu_RB3.cpp:3017` (`wgpu::Buffer vbuf, ibuf;` inside `DrawMesh`),
  `:3024`/`:3041` create, RAII scope-end release. `DrawMesh` is called once per mesh per
  frame (`RndMesh::DrawShowing → gBandRnd.DrawMesh(this)` at `:4075`).
- **Brief's lead, corrected:** the brief is right that on the deployed build
  `CleanupGpuMesh` is the **weak no-op stub** — there is **no `sMeshGpu` map and no
  `CleanupGpuMesh` definition in `Rnd_Wgpu_RB3.cpp` at all**. `CleanupGpuMesh` is defined
  ONLY in `MeshGpuCache.cpp:53`, which is in `MILO_ENGINE_GPU_PLATFORM_SOURCES` (the dc3
  list, `CMakeLists.txt:305`) and is **EXCLUDED from the RB3 build** (RB3 uses
  `MILO_ENGINE_GPU_PLATFORM_SOURCES_RB3` = `Rnd_Wgpu_RB3.cpp` only, `:320-321`,
  selected at `:365-366`). So `RndMesh::~RndMesh`'s `CleanupGpuMesh(this)` IS a no-op on
  RB3.
- **BUT there is no per-mesh GPU buffer cache to leak.** RB3's `DrawMesh` **recreates
  vbuf/ibuf every single draw** (local `wgpu::Buffer`s, no `sMeshGpu`), so a destroyed
  mesh has nothing cached against it — CleanupGpuMesh being a no-op leaks nothing for RB3
  because nothing is stored per-mesh in the first place. The "weak no-op leaks per-mesh
  GPU buffers" concern is a **DC3-architecture concern that does not apply to the RB3
  backend.** What RB3 has instead is per-frame VBO/IBO re-upload churn (every mesh
  re-creates + WriteBuffer its geometry each frame) — a GPU-bandwidth / allocator-pressure
  cost and a per-frame transient-allocation source (feeds GC), but the buffers ARE freed
  each frame, so **no monotonic growth**.
- **The uncommitted edit is a half-finished trap.** `native/src/rndobj_synth_link_stubs.s`
  (working-tree modified, NOT in the deployed build) removes the
  `_Z14CleanupGpuMeshP7RndMesh` weak alias and claims it is "now STRONGLY defined in
  Rnd_Wgpu_RB3.cpp" — **but no such definition exists there.** Building with this edit
  as-is would fail to link RB3 (undefined `CleanupGpuMesh`) unless the dc3 MeshGpuCache.cpp
  gets pulled in (it operates on a `sMeshGpu` map RB3 never populates). Flag: do not ship
  that .s edit without first adding a real RB3 `CleanupGpuMesh` (erasing `sTexGpu`/any new
  per-mesh cache) to `Rnd_Wgpu_RB3.cpp`.
- **Estimated rate (steady playback):** 0 monotonic growth; high transient churn (one VB +
  one IB created+freed per mesh per frame, ~hundreds of meshes/frame). Transient alloc
  feeds GC frequency but not heap *growth*.
- **Verdict:** NOT a leak. Needs-runtime only if you want to quantify the per-frame
  transient-allocation GC contribution (separate from leak).

### 3. JS asset/save caches in `rb3_pre.js` — BOUNDED (post-b79cbafa)
- **`window.__rb3IdbCache`** (`rb3_pre.js:139`): the b79cbafa fix is present and correct.
  `__rb3CachePut` (`:240-255`) explicitly does **NOT** retain bytes in the in-memory map
  anymore (the OOM-FIX comment `:227-239`); it copies into `sPendingWrites`, flushes to
  IDB on the next microtask (`flushPendingWrites :205-219`), and the copy is released. The
  in-memory map is populated ONLY at boot by `loadAllRows` (`:183-203`) — a one-time,
  bounded warm-cache read, never appended to at runtime. **Bounded.**
- **`sPendingWrites`** (`:144`): drained to `[]` at the top of every `flushPendingWrites`
  (`:207`). Bounded to one microtask's burst.
- **`window.__rb3SaveCache`** (`:322`): two tiny blobs (globaloptions ~tens of bytes,
  gameplayopts_0.bin 9 bytes); `__rb3SavePut` (`:382`) `Map.set`s by fixed key (overwrite,
  not append). Bounded.
- **`window.__rb3CacheStats` / `__rb3SaveStats`**: fixed-shape counter objects. Bounded.
- **Verdict:** clean. No remaining JS-glue leak after b79cbafa.

### 4. MEMFS asset files — BOUNDED, never unlinked but never re-grown
- `native_file.cpp:87` (cache-hit) and the engine's `WebAssetsFetchSync` write fetched
  assets into MEMFS; there is **no `FS.unlink`** anywhere in `native/src/`. MEMFS therefore
  grows monotonically *as new assets are first touched*, but is **bounded by the asset
  set** and is **NOT written during steady playback** (no per-frame/per-note MEMFS writes).
  Same data lives in MEMFS exactly once (the b79cbafa fix removed the duplicate JS copy).
- **Estimated rate (steady playback):** 0. Growth is first-touch-of-asset only.
- **Verdict:** bounded; not a steady-playback leak.

### 5. Event listeners (keydown/click/touchstart/visibilitychange/pagehide) — BOUNDED
- `AudioDevice_Web.cpp:189-191` adds 3 document listeners (resumeAudio) inside
  `js_audio_init`, which is called from `AudioDevice::Init` — **once per process**
  (confirmed: Init guarded by `mInitialized`, `:377`; `rb3_synth_native.cpp:40` calls it
  once; no per-song re-init). `rb3_pre.js:400/404` adds 2 page-lifecycle listeners once.
  None are removed, but the count is **fixed (5 total), not per-song**. Bounded.
- **Verdict:** not a leak.

### 6. AudioContext / SharedArrayBuffer / ring + mix/out/capture buffers — BOUNDED
- One `AudioContext` + one `SharedArrayBuffer` (RING_FRAMES=32768) + one AudioWorkletNode,
  all created once in `Init` (`AudioDevice_Web.cpp:383-402`). `sMixBuffer`/`sOutBuffer`
  (`:383-384`) and `sCaptureBuffer` (`:810`, debug-only) are fixed-size, allocated once,
  freed in `Terminate` (`:453-458`). `MixSources` mixes additively into a fixed buffer and
  **erases finished sources** from `mSources` (`:507-508`) — the source vector does not
  grow across songs.
- `RB3StreamReceiverNative` (one per song channel, ~6) is `new`ed per song and freed on
  song end; `~RB3StreamReceiverNative` `RemoveSource`s itself (`rb3_stream_receiver_native.cpp:130-135`).
  `RenderAudio` (`:243-332`) is **allocation-free** (fixed ring, no per-frame alloc).
- The resampler carry (`mResampleCarry[]`, fixed `kResampleCarryMax`) is a fixed member.
- **Verdict:** not a leak.

### 7. Particle vertex buffer `sVerts` — BOUNDED
- `Rnd_Wgpu_RB3.cpp:4239` `static std::vector<RB3ParticleVertex> sVerts;` is
  `.clear()`ed + `.reserve(256)`ed each call (`:4243-4244`) — reused, not grown.
- **Verdict:** not a leak.

### 8. Debug `static std::unordered_map<…,int>` throttle maps — BOUNDED, env-gated
- Many `sSeen`/`sR`/`sCl`/etc. throttle maps in `Rnd_Wgpu_RB3.cpp` (keyed by mesh name or
  pointer) accumulate one int per distinct key, but they are all inside `if (getenv(...))`
  debug blocks that are **off in production**. Keyed by a finite name/pointer set anyway.
- **Verdict:** not a leak in production.

---

## Cross-reference to the heap-slope probe
The wave-10 web-capture probe doc (`wave-10-audio-jitter-gc.md` Findings section) was
empty at audit time — no measured JS/wasm heap slope numbers were available to confirm
the `sTexGpu` growth rate at runtime. **The texture-cache slope should be measured by
holding sTexGpu.size constant during a single steady song (expect flat) vs. across
several song load/exit cycles (expect monotonic climb).** That A/B is the runtime
confirmation of suspect #1's transition-driven (not steady-time-driven) growth profile.

---

## Bottom line for the GC→underrun hypothesis
- **Confirmed remaining unbounded leak:** `sTexGpu` (suspect #1) — but it grows at
  **screen/song TRANSITIONS, not during a single steady song**, so it is unlikely to be
  the cause of jitter *during* one continuous playback. It IS the cause of a slow
  multi-song-session climb that raises baseline heap → bigger/longer GCs over time.
- **The CleanupGpuMesh lead does not apply to RB3** (no per-mesh GPU cache exists in the
  RB3 backend; the no-op stub leaks nothing) — corrected. The uncommitted `.s` edit is a
  link-breaking half-fix; flag it.
- **No per-frame heap GROWTH found in the steady-playback hot path** (audio render, mix,
  resample, ring push are all allocation-free; per-frame GPU buffers churn but are freed).
  The per-frame VBO/IBO/bindgroup re-creation in `DrawMesh` is a transient-allocation GC
  *frequency* source worth a separate look, but not a leak.

**Recommended fix for #1:** add a real RB3 `CleanupGpuMesh`/`CleanupGpuTex` that
`sTexGpu.erase(tex)` on `RndTex` destruction (and define the CleanupGpuMesh the
uncommitted `.s` edit already assumes exists), so freed textures release their
`wgpu::Texture`/`TextureView` and their map node. This bounds the cache to the live
texture set.

## Artifacts
- This file: `docs/native/audio-perf-loop/wave-10-web-leak-audit.md`
- Decl/leak site: `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp:340,469,726,1356`
- Half-finished trap: `native/src/rndobj_synth_link_stubs.s` (working-tree diff vs HEAD)
- CleanupGpuMesh real def (RB3-excluded): `milo-native-engine/src/platform/MeshGpuCache.cpp:53`
- RB3 GPU-backend CMake filter: `milo-native-engine/CMakeLists.txt:305,320-321,365-366`
