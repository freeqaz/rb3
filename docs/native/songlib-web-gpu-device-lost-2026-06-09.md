# Song library won't load on web — song_select crash (2026-06-09)

## RESOLUTION (read first)
**Crash = default-on guest-profile + char-preview (rb3 `9318bb73`), ALREADY REVERTED
to opt-in by concurrent commit `92fcb32c` (06:07). Current HEAD build reaches
`song_select_screen` cleanly (verified vs the 06:42 deploy).** Anyone still crashing
is on a STALE build (04:17 deploy, or a Mac build from before `92fcb32c`) → pull+rebuild.

**Real mechanism = "domino ②" use-after-free, NOT GPU device-loss.** Per the author's
notes (`CharCache.cpp:52`, `rb3_guestprofile_native.cpp:50`, roadmap `0d099762`): with
BOTH flags on, the menu-wide char-material composite drives a menu DTA →
`RndMat::SyncProperty` → `PropSync<RndTex>` (`PropSync_p.h:124`) → `dynamic_cast` on a
DANGLING (freed) `Hmx::Object*` → vtable fault on the song_select transition. "Each flag
alone is safe; only the combination crashes" ⇒ use-after-free, not generic mesh churn.
The `device lost (reason=2)` console line is BENIGN (prints on clean runs too); the real
signal is `page.crash`/frame-freeze.

**Two real but SEPARATE wins kept:** (1) JS-heap leak fixed (`b79cbafa`) — `__rb3CachePut`
kept a 2nd copy of every asset in `__rb3IdbCache` (~187MB by song_select). (2) Latent
perf (NOT the crash): `Rnd_Wgpu_RB3.cpp` DrawMesh recreates vbuf/ibuf every mesh every
frame (no cache; `InvalidateGpuMesh`/`CleanupGpuMesh` are no-ops) → buffer-cache is a
perf win. **Proper fix to re-enable char preview = fix domino ②** (roadmap C11).

---
## Symptom
Web build (native/web/build/release, served by `native/web/server.py` on :8421)
hangs on a black screen entering the song library. Last console lines the user
sees:
```
NOTIFY: mDoNotCompress player3_m (.../subwayhangout_clips.milo)   # benign CharClip warn
RB3 screen: frame 387 currentScreen = 'song_select_enter_screen' (in transition)
NOTIFY: Data kSeqStop is not Int                                  # benign (HX_NATIVE warn+recover)
NOTIFY: Data 123 is not Symbol (file config/song_select.dta, line 1)  # benign (alpha_shortcuts `123`)
```
The 4 NOTIFY lines are pre-existing/benign (DataNode.cpp HX_NATIVE warn-and-recover).
They are just the last log output before the freeze, NOT the cause.

## Root-cause signal (confirmed)
Headless Playwright repro at `song_select_enter_screen`:
```
[console:error] GpuDevice: device lost (reason=2): Device was destroyed.
*** page.on(crash) FIRED ***
```
→ The **WebGPU device is lost** entering song_select, which crashes the renderer.
Reason=2 "Device was destroyed" with no preceding wasm RuntimeError/Aborted in the
page console → looks like a **GPU-layer fault** (renderer/GPU process dies; the
device-lost may be cause OR teardown-symptom — to be disambiguated).

Last GPU-related stubs fired right before the loss:
- `env._Z14CleanupGpuMeshP7RndMesh`  (RndMesh GPU cleanup — **web stub / no-op?**)
- `env.BinkSetMemory`

## Ruled OUT
- **Not OOM.** Host has 93GB RAM / 74GB free, no cgroup limit. Renderer RSS peaks
  ~0.9–1.1GB then crashes — normal working set, not exhaustion. wasm MAXIMUM_MEMORY
  is 2GB (native/CMakeLists.txt:867) and is never approached.
- **Not the R3 boot bundle.** `?bootBundle=0` (old sync-XHR path) ALSO crashes at
  song_select with the same ~887MB peak.
- **Not native.** rb3-native reaches `song_select_screen` cleanly (frame 253) —
  `scripts/native/song-select-capture.py`. So engine LOGIC is fine; the fault is in
  the **web-specific WebGPU/render path** (browser WebGPU vs native Dawn).
- Crash is somewhat **intermittent** (1 of ~5 runs reached song_select) — consistent
  with a GPU resource/timing fault rather than a deterministic logic bug.

## Already changed (a real, separate fix — keep, but it does NOT fix the GPU crash)
A genuine JS-heap leak was found and fixed: `native/web/rb3_pre.js` `__rb3CachePut`
retained a copy of EVERY fetched asset in the in-memory `window.__rb3IdbCache`
Map (read by native_file.cpp `cacheTryHit` only on MEMFS-miss = first open; the
file is always already in MEMFS when `__rb3CachePut` runs, so the in-memory copy
is never re-read this session — pure waste). At song_select it grew to ~187MB
(475 entries). FIX: drop the in-memory retention, keep the async IDB write-through.
- Source fix: `native/web/rb3_pre.js` (committed-worthy).
- For fast diagnosis I ALSO hand-patched the DEPLOYED `native/web/build/release/rb3-web.js`
  (the `--pre-js` is baked in at link time) and **stashed** its stale precompressed
  siblings: `rb3-web.js.br.stash`, `rb3-web.js.gz.stash` (so the server serves the
  patched identity JS). A clean `scripts/web/build.sh` rebuild regenerates all of
  these from the rb3_pre.js source — restore/rebuild to normalize.

## Leading hypotheses (to verify)
1. **GPU resource cleanup is stubbed on the RB3 web backend** → meshes/textures/
   buffers leak across the main_hub→song_select transition (which destroys+recreates
   a lot of scene geometry) → WebGPU device runs out of / faults on resources →
   device lost. Suspects: `CleanupGpuMesh`/`InvalidateGpuMesh`/GPU texture+buffer
   destroy paths in the RB3 Wgpu backend (`Rnd_Wgpu_RB3.cpp`) and the `native/src`
   GPU shims. (MEMORY: "HX_NATIVE calls to DC3-WgpuRnd-only engine symbols (e.g.
   InvalidateGpuMesh) break the rb3 BandRnd-backend link — shim in native/src".)
2. A specific render feature hit during the song_select transition issues an
   invalid/oversized WebGPU command (bloom/glow/venue-light/char-preview paths all
   landed recently and are default-on). Opt-outs exist:
   RB3_HIGHWAY_BLOOM_OFF, RB3_VENUE_LIGHT_OFF, RB3_TRACK_LIGHT_OFF, RB3_NO_CHAR_PREVIEW,
   RB3_NO_GUEST_PROFILE, RB3_NO_CLOSET_POLL (only bootBundle is URL-wired today; see
   main_web.cpp ApplyUrlLoaderEnv kPairs — others need wiring or a build to A/B on web).
3. The recent default-on **closet/guest-char preview** (commit 9318bb73, today) makes
   the WEB build load+render a full band character (heaviest asset class) by default —
   extra GPU resources that could tip the song_select transition over.

## ROOT CAUSE — characterized with REAL GPU (2026-06-09, empirical session)

Reproduced on real hardware (chromium got NVIDIA **Ampere** / RTX 3090 via
`--use-angle=vulkan`; page-side `adapter.info.vendor=nvidia, architecture=ampere`,
non-fallback; chromium stderr `GPU.MultiGpu.Nvidia recorded 2 samples`). NOT
SwiftShader. The crash reproduces on the real GPU.

**Device-lost is a SYMPTOM, not the cause.** The GPU PROCESS dies with a hard
**`Received signal 11 SEGV_ACCERR`** (segfault) — captured from chromium's own
process stderr via a CDP-attach harness. Sequence: GPU process SIGSEGV →
`GPU.GPUProcessExitCode` recorded → renderer `command_buffer_proxy_impl.cc: GPU
state invalid after WaitForGetOffsetInRange` → Dawn observes the dead GPU process
and (sometimes) fires the `SetDeviceLostCallback` ("reason=2 Device was
destroyed"). In some runs the renderer `page.on('crash')` fires with NO
device-lost message at all — confirming device-lost is downstream of the SEGV.
There is **NO preceding WebGPU validation error and NO out-of-memory error** —
verified both at the JS layer (a `createBuffer/createTexture/createBindGroup/
queue.submit` interposer with an `uncapturederror` listener saw ZERO
WGPU-UNCAPTURED / WGPU-FAIL up to the crash) and the process layer (only the raw
SIGSEGV, no `VK_ERROR_*`). The SEGV fires exactly at `song_select_enter_screen`,
right after the song_select milos load (`song_select_shortcut/_filter.milo`,
`list_filters.milo`) and the benign `Data 123 is not Symbol` NOTIFY.

**Mechanism = an unbounded per-frame GPU-buffer leak in `BandRnd::DrawMesh`**
(`../milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp:2734`). Every `DrawMesh`
call unconditionally creates a fresh `MeshVB` + `MeshIB` `CreateBuffer` (lines
3017-3043) plus an object bindGroup (3066) + bone bindGroup for skinned (3673),
with **NO per-mesh GPU cache** — unlike textures, which ARE cached in `sTexGpu`
keyed by `RndTex*` with a fingerprint dirty-check (line 340/472). `RndMesh::OnSync`
is a documented no-op ("we re-upload every draw"; line 4078-4081) and
`CleanupGpuMesh`/`InvalidateGpuMesh` are no-op web stubs. Nothing ever calls
`.Destroy()` and there is no retire/recycle list, so the Dawn buffers accumulate
without bound.

**Interposer counts (real-GPU run, label `MeshVB`/`MeshIB` only):**
- AT main_hub (idle, just from reaching it): **72,900 buffers** (36,441 `MeshVB`
  + 36,441 `MeshIB`), **97,192 bindGroups**, ~**1,781 MB** of mesh buffers.
- Growth continues ~1,500 buffers/sec; at the song_select SEGV: **97,102
  buffers**, **135,280 bindGroups**, **~2,245 MB**. `biggestBufMB=0.47` — it is a
  COUNT explosion, not an oversized single allocation. Default WebGPU limits are
  huge here (`maxBufferSize=4GB`) so no validation tripped.
- This is GPU-PROCESS memory (separate process), NOT the wasm heap — consistent
  with the prior "Not OOM" note (wasm RSS never neared its 2GB MAXIMUM_MEMORY).

**Attribution to a render feature:** NOT feature-specific. The leak is the generic
`DrawMesh` path common to ALL geometry (band chars, venue, UI). The render
opt-outs (RB3_NO_CHAR_PREVIEW / RB3_HIGHWAY_BLOOM_OFF / RB3_VENUE_LIGHT_OFF / …)
are not URL-wired (only `bootBundle`/`loaderYieldMs`/`loaderBudgetMs`/
`frameInstrument` are, in `main_web.cpp ApplyUrlLoaderEnv kPairs`) AND would not
fix it — they would only slow the leak rate, not stop the unbounded growth. The
fix is to **cache the per-mesh VB/IB/bindGroups** (mirror `sTexGpu`: a
`sMeshGpu[RndMesh*]` side-table re-uploaded only on geometry change) so DrawMesh
reuses GPU buffers instead of recreating them every frame.

**Crash rate:** every dedicated real-GPU run reaching song_select crashed
(adapter-probe, interposer, CDP-stderr, plus the crash-rate batch). The doc's
earlier "1 of ~5 survived" reflects timing jitter on WHICH frame the GPU process
faults; the leak itself is deterministic.

## Tooling
- `scripts/web/_songlib-repro.mjs` — boot→main_hub→song_select, console capture.
- `scripts/web/_songlib-stderr.mjs` — launches chromium directly, captures the
  device-lost / GPU signals (this is what surfaced the device-lost).
- `scripts/web/_songlib-gpu-probe.mjs` — real-GPU adapter probe (confirms
  NVIDIA Ampere, not SwiftShader) + page-console GPU signals.
- `scripts/web/_songlib-wgpu-interpose.mjs` — `addInitScript` GPU-op interposer:
  wraps createBuffer/createTexture/createBindGroup/queue.submit + uncapturederror,
  logs running resource counts + last-good/first-failing op. **This surfaced the
  72.9k→97.1k buffer leak.**
- `scripts/web/_songlib-cdp-stderr.mjs` — launches chrome-headless-shell directly
  and connects over CDP so the chromium PROCESS stderr is captured to a file.
  **This surfaced the `Received signal 11 SEGV_ACCERR` GPU-process crash.**
- `scripts/web/_songlib-crashrate.mjs` — hang-proof N-run crash-rate batch
  (force-kills each browser; writes JSON incrementally).
- `scripts/web/_songlib-mem.mjs`, `_songlib-crash.mjs` — memory probes (UA-memory
  API returns NaN here; external /proc RSS watch was the reliable memory signal).
- Server already running: `python3 native/web/server.py --no-encode` on :8421.
- GPU note: run browser probes with the **bash sandbox disabled** for real GPU.

## REGRESSING COMMIT (git bisect, worktree + matched engine pin per step)
First-bad: **`9318bb73`** `feat(native): customize closet reachable + renders a char
without sign-in (default-on)` (06-09 04:13). Parent `ed9a3e92` is last-good; both
pin the SAME engine `7e5b87c`, so the regression is the **rb3 commit itself**, not
an engine bump. It flips `RB3InstallGuestProfile()` from opt-in to **default-on**
(opt-out `RB3_NO_GUEST_PROFILE`) → a primary profile exists → `PrimaryProfileChangedMsg`
→ `CharSync::UpdateCharCache` loads + renders a full ~140-mesh band character with no
sign-in. That ~140 extra meshes/frame through the leaky `DrawMesh` path is what tips
the per-frame WebGPU-buffer churn into the GPU-process SIGSEGV at the song_select
transition. Verdicts: ed9a3e92 GOOD 6/6, 9318bb73 BAD (2/3), HEAD BAD. (Landmarks
a27ccdea/2d0a269a/5a5edee8/08ec1442 all GOOD.)

## FIX OPTIONS
- **Durable (real fix):** give the `rb3` WebGPU backend a per-mesh GPU-buffer cache
  (mirror the existing `sTexGpu` texture cache / the dc3 `MeshGpuCache::EnsureMeshUploaded`)
  so `DrawMesh` reuses VB/IB/bindGroups instead of recreating them every frame, and
  wire `CleanupGpuMesh`/`OnSync` to erase/dirty. Fixes the latent leak for ALL geometry
  and is a large perf win (no per-frame re-upload). Engine change → commit engine, bump
  `MILO_ENGINE_PIN`. Risk: dynamic meshes (text/HamRibbon) must re-upload on change.
- **Immediate stopgap:** make the closet/guest char-preview opt-IN again on web (revert
  the default-on flip, or gate it off for the web target / wire `RB3_NO_GUEST_PROFILE`
  into `main_web.cpp ApplyUrlLoaderEnv kPairs`). Restores song_select on web fast, but
  only hides the latent leak (which still bounds how much geometry web can ever draw).

## FIX IMPLEMENTED (2026-06-09) — per-mesh GPU cache in the rb3 backend

Took the durable option. In `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp`
(+ `.h`), `BandRnd::DrawMesh` now keys a per-mesh side-table `sMeshGpu`
(`std::unordered_map<RndMesh*, RB3MeshEntry>`, mirroring `sTexGpu`):
- **VB/IB**: uploaded ONCE per mesh, reused every frame. Re-uploaded only on a
  geometry change — detected by a fingerprint (geom-owner ptr + vert/face counts +
  skinned flag) AND `RndMesh::OnSync()` marking the entry dirty (the signal RndText
  / dynamic meshes already fire via `RndMesh::Sync`).
- **Object / Bone / Material uniforms + bind groups**: each mesh gets its OWN small
  persistent uniform buffer (`MeshObjUB`/`MeshBoneUB`/`MeshMatUB`) + ONE bind group,
  created once. Per frame we only `WriteBuffer` the (animated) uniforms into the same
  buffer and reuse the bind group. (No bind-group-LAYOUT change → the shared dc3
  backend is untouched.) The material bind group rebuilds only when its resolved
  diffuse/emissive views or the RndMat ptr change.
- `CleanupGpuMesh(RndMesh*)` got a real strong def (erases the entry; displaces the
  weak no-op stub in `rb3/native/src/rndobj_synth_link_stubs.s` + the web auto-stub),
  called from the HX_NATIVE `RndMesh::~RndMesh`. `Shutdown()` clears `sMeshGpu`.
- The CPU vertex UNPACK still runs every frame (the skinned shard-guard re-blends
  bind-pose verts against the live bone pose); only the leaky GPU UPLOAD is gated.

**Results (real-GPU, ANGLE/Vulkan, NVIDIA):**
- Native (`RENDER_DBG`): 823-mesh scene → per-frame `buf_creates=0 bg_creates=0`
  steady (was `823×2` buffers + `823×3` bind groups/frame). Text still updates
  (song-list scroll shows different rows — not frozen).
- Web interpose (`_songlib-wgpu-interpose.mjs`): **6/6 runs reached `song_select`
  crash-free**. `MeshVB`/`MeshIB` 36,441 → **~672** at song_select (was 97k+ and
  climbing → SIGSEGV). Total live buffers FLAT at ~3,380. `_songlib-stderr.mjs`:
  no `page.on(crash)`, reaches song_select (the lone device-lost line is the
  end-of-test teardown, after `FINAL: song_select_screen`).
- Opt-out: `RB3_NO_MESH_CACHE=1` restores the legacy per-draw upload for A/B.

**Follow-ups (tracked, not blocking):**
1. **DrawRect 2D-quad bind-group churn (the remaining per-frame create).** With the
   mesh path fixed, the residual `~37 createBindGroup/frame` at song_select is
   `BandRnd::DrawRect` (Rnd_Wgpu_RB3.cpp ~line 2834) building a fresh bind group per
   UI quad against the shared mutable `mRectUB`. Bounded-per-frame (∝ visible quads,
   buffers stay flat), but still churns under web submit backpressure. Fix needs a
   per-rect uniform (not one shared `mRectUB`) so the bind group can be cached by
   texView. Separate path from the assigned DrawMesh leak; deferred.
2. **Validate the OnSync-only dirty policy + push dirty-checking further (per jw).**
   We chose OnSync-dirty over dc3's "always re-upload text every draw" (the latter
   still leaked ~22k MeshVB under web backpressure). Verified text updates correctly
   on scroll. Worth a longer-soak A/B to confirm no dynamic-mesh class freezes, and
   to extend the cache CPU-side: cache the unpacked bind-pose verts so the per-frame
   unpack (kept for the shard guard) can also be skipped on a cache hit — a further
   CPU win the current fix leaves on the table.

Verified diff lives on engine temp branch `fix-rb3-meshcache` (worktree
`/tmp/eng-meshcache`); rb3 pin NOT bumped — for the coordinator to promote.

## DECISIVE EXPERIMENT — mechanism SETTLED (2026-06-09, coordinator session)
Built two cells, BOTH with char preview forced default-ON (the original `9318bb73`
crashing config), real GPU (NVIDIA/ANGLE-Vulkan):
- Cell-1 (control): stock engine. → **6/6 CRASH** at song_select, GPU SIGSEGV.
- Cell-2 (test): mesh-cache engine `/tmp/eng-meshcache`. → **6/6 CRASH** at song_select,
  GPU SIGSEGV — despite the buffer leak being GONE (97k→~624 buffers, flat ~3,150).
- Negative control: HEAD build (preview opt-in) leaks MORE (240k buffers) yet reaches
  song_select **clean 3/3**.
- NATIVE symbolized backtrace (identical on both builds): `PropSync<RndTex>` →
  `RndMat::SyncProperty` → `Hmx::Object::SetProperty/OnSet` → `DataArray::Execute` →
  `MusicLibrary::SendMessageToSongSelectPanel/UpdateHeaderData/OnEnter` → `UIScreen::Enter`.

**VERDICT: Theory B (CPU use-after-free, PropSync<RndTex> dangling dynamic_cast) CONFIRMED;
Theory A (GPU mesh-buffer leak) REFUTED as the cause.** The earlier "ROOT CAUSE — REAL GPU"
section's mechanism is SUPERSEDED: the GPU-process SIGSEGV is a downstream manifestation of
the dangling read corrupting GPU-command state (native shows the direct in-process SIGSEGV
with the readable PropSync stack). The buffer leak is real but orthogonal — a perf issue.

- Fix for the user-facing crash: the shipped revert `92fcb32c` (preview → opt-in). Correct + sufficient.
- The mesh-buffer cache (`/tmp/eng-meshcache` `a0f98ad`) is a PERF win only (buffer mem ~2768MB→22MB,
  no rendering regression: native song_select perceptual 83.6 vs baseline; web hub 47.9 ≥ stock 44.2).
  It does NOT re-enable default-on char preview — that needs the PropSync<RndTex> use-after-free
  (roadmap C11 "domino ②") fixed first.
- Visual-diff automation built: `scripts/analysis/visual_diff.py` (+ capture driver, doc
  `docs/native/visual-diff-tooling.md`) — westworld-style canonical max-channel-Δ + perceptual mode.
