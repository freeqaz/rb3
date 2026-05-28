# W2 — Render one .milo in browser

**Parent plan:** [`PLAN.md`](PLAN.md). Read it for context.
**Depends on:** [`W1_CLEAR_FRAME.md`](W1_CLEAR_FRAME.md) (browser clears canvas, ticks frames).
**Blocks:** W3.

## Current starting state (read before coding)

W1 shipped a file-static `GpuDevice sWebGpu` + `DrawClearFrame()` path in
`rb3/native/src/main_web.cpp`. It does NOT use `BandRnd` at all — the boot
machine purposely bypasses it because `BandRnd::InitGpu` is monolithic and
aborts on web (see task 1 below). W2's first job is to retire `sWebGpu` and
drive `gBandRnd` instead.

The good news: `BandRnd` already compiles into `rb3-web.wasm`. It lives in
`milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp`, which the engine includes
unconditionally in the `rb3` GPU-backend source set
(`MILO_ENGINE_GPU_PLATFORM_SOURCES_RB3`, appended at engine
`CMakeLists.txt:366`). It provides the global `BandRnd gBandRnd` (line 31)
plus strong defs of `RndMesh::DrawShowing`, `RndTex::SyncBitmap/PresyncBitmap`.
**W2 is mostly wiring, not new rendering code.**

## Goal

Parity with `RB3_RENDER_MESH=1 rb3-native <milo_path>` — but in the browser,
rendering to the canvas instead of a PNG. Three milos verified end-to-end:

1. `ui/track/gen/tracksystem.milo_xbox`
2. `ui/track/gen/gem_smasher_guitar.milo_xbox`
3. `ui/main/gen/main_hub.milo_xbox`

For each: a browser screenshot visibly matches the corresponding native PNG
under reasonable tolerance (pixel-exact Linux Vulkan to browser WebGPU is
unrealistic; pixelmatch <2% is the bar).

## Out of scope

- Audio. W3.
- Texture sampling at full fidelity. RB3 native currently renders with
  diffuse=white (textures not yet sampled in all paths). Don't block W2 on this.
- Skinning. Native uses an identity bone palette; same here.
- Latent heap overwrite that trips on some char milos natively. Same constraint
  carries over; pick milos that don't trip it.

## Ordered task list

Split into **W2a** (critical path, first milo renders) and **W2b** (full
coverage + pixelmatch).

### W2a — Critical path (Opus-heavy) — DONE 2026-05-28

**Status: SHIPPED.** Geometry renders in-browser via WebGPU. Acceptance milo
`ui/track/gen/gem_smasher_guitar_meshes.milo_xbox` (the gem-highway track
surface — a sibling of `tracksystem`): `rb3MilosLoaded=1`, `rb3FrameCount=49`,
`nonclear_pct=3.69%`, `error_count=0`, no WASM trap. Screenshot:
`scripts/web/results/w2a/gem_smasher_guitar_meshes/canvas.png` — visually
matches the native PNG (`RB3_RENDER_MESH=1 rb3-native …_meshes.milo_xbox`).

Commits: engine `4077997491c801380cf28e364992c36ae5dc662b` (task 1) + rb3
`<see git log>` (tasks 2-4 + pin bump).

**Deviation — acceptance milo.** The plan names `tracksystem.milo_xbox`, but
that milo (and `main_hub.milo_xbox`) crash on BOTH native and web during
`ObjectDir::PreLoad` — they are **multi-chunk** `ChunkStream` containers
(tracksystem=7 chunks, main_hub=27, `track_shared.milo_xbox`=60,
`ingame_bank.milo_xbox`=8) and the multi-chunk read path faults. Single-chunk
milos load cleanly. `gem_smasher_guitar.milo_xbox` itself is single-chunk and
renders natively, but on web it pulls multi-chunk *subdir* deps (`track_shared`,
`ingame_bank`) which spin ~46s then OOM the wasm heap (page crash). Loading the
single-chunk `…_meshes.milo_xbox` directly sidesteps both: it IS the geometry
(10 meshes) with no multi-chunk deps. The multi-chunk `ChunkStream` bug is the
**top W2b blocker** (see "Known gotchas" Risk 4 below). It is pre-existing,
independent of the W2a wiring, and blocks the full-coverage pixelmatch in
W2b task 7 for the multi-chunk milos.

**Task 1 [OPUS] — Split `BandRnd::InitGpu` into `StartGpuInit()` +
`InitGpuResources()`** (engine `Rnd_Wgpu_RB3.cpp` + `.h`). **DONE** (engine
`4077997`). This is the **first step**; nothing else can proceed until it lands.

`StartGpuInit(w,h,headless)->bool` = `mGpu.Init(desc)` only, no `IsReady()` gate.
`InitGpuResources()` = pipelines/depth/4 rings/`CreateDefaultTextures`/
`mGpuReady=true`, idempotent (early-return on `mGpuReady`), and now sets
`mTargetFmt = IsHeadless() ? RGBA8Unorm : mGpu.SurfaceFormat()` so the pipeline
target format matches the actual color attachment (native headless readback
stays RGBA8 → PNG unchanged; web canvas gets the browser-preferred format,
observed `fmt=22` = BGRA8Unorm — Risk 2 resolved, colors not swapped). The
monolithic `InitGpu` now calls `StartGpuInit` then (after the synchronous
native `IsReady()`) `InitGpuResources`; native behavior unchanged.

Current blocker: `BandRnd::InitGpu` (`Rnd_Wgpu_RB3.cpp:595`) combines
`mGpu.Init(desc)` and the full resource setup in a single synchronous call.
Line 602: `if (!mGpu.Init(desc) || !mGpu.IsReady()) return false;`. On web,
`mGpu.IsReady()` is always false at call time — WebGPU adapter/device
acquisition is async. So the call returns false immediately, `gBandRnd` never
initializes, and `sWebGpu` was the W1 workaround.

**Split spec:**

- `StartGpuInit(int width, int height, bool headless) -> bool`:
  Calls `mGpu.Init(desc)` only, no `IsReady()` gate. Returns false only on a
  sync dispatch error. On web, `Init` returns true immediately after starting
  the async adapter/device request.

- `InitGpuResources() -> void` (idempotent, guarded by early-return if already
  done): The pipelines/depth-tex/4 uniform rings/`CreateDefaultTextures`/
  `mGpuReady=true` block currently at lines 606-628. Matches the DC3 template:
  `WgpuRnd::InitGpuResources()` in engine `Rnd_Wgpu.cpp:320`. That function is
  called from DC3's native path (line 309) after a synchronous `mGpu.Init` and
  from the web boot machine after `mGpu.IsReady()`.

- Keep the existing monolithic `InitGpu` for native (calls `StartGpuInit` +
  polls `IsReady` synchronously + calls `InitGpuResources`). Callers in
  `rb3-native` (`main_native.cpp`, `rb3_render_mesh.cpp`) must be updated to
  match.

This is an **engine edit** (`milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp`
and `.h`). Bump `MILO_ENGINE_PIN` in `rb3/native/CMakeLists.txt` after it lands.

---

**Task 2 [OPUS] — Rewrite `rb3/native/src/main_web.cpp` boot machine** to
drive `gBandRnd` instead of `sWebGpu`. **DONE.** Deleted `sWebGpu` +
`DrawClearFrame`. New states: `BOOT_ENGINE_INIT` (SystemPreInit/Init +
`RegisterCommonFactories` + `gBandRnd.PreInitRender()` + `SetClearColor` +
`gBandRnd.StartGpuInit(W,H,false)`), `BOOT_GPU_WAIT` (`Gpu().PollEvents()` +
`IsReady()`), `BOOT_GPU_READY` (`InitGpuResources()`), `BOOT_LOADING_MILO`
(parse `?milo=`, anchor relative paths under `/data`, `LoadMiloAndWalk`, emit
`window.rb3MilosLoaded`), `BOOT_RUNNING_RENDER` (per-frame `RenderFrame` or a
clear-only `BeginFrame`/`EndFrame` fallback if no/failed milo; emit
`window.rb3FrameCount`). try/catch → `BOOT_ERROR` retained on all heavy steps.

Delete `sWebGpu` and `DrawClearFrame`. Replace with:

- `BOOT_ENGINE_INIT`: call `gBandRnd.PreInitRender()` + `SetClearColor` +
  `gBandRnd.StartGpuInit(W, H, /*headless=*/false)`.
- `BOOT_GPU_WAIT`: poll `gBandRnd.Gpu().PollEvents()` + `gBandRnd.Gpu().IsReady()`.
- `BOOT_GPU_READY`: call `gBandRnd.InitGpuResources()`.
- New `BOOT_LOADING_MILO`: parse `?milo=` URL param (see code snippet below),
  call `LoadMiloAndWalk(path)` from the refactored `rb3_render_mesh.cpp`.
- New `BOOT_RUNNING_RENDER`: per-frame `gBandRnd.BeginFrame(cam)` / walk-meshes
  / `gBandRnd.EndFrame()`. Do NOT call any Present/Swap API — under WebGPU the
  surface auto-composites at `requestAnimationFrame` return.
- Emit `window.rb3FrameCount` and `window.rb3MilosLoaded` for the test harness.

URL param parsing snippet for `main_web.cpp`:

```cpp
static std::string GetMiloPathFromUrl() {
    char* s = (char*)EM_ASM_PTR({
        const params = new URLSearchParams(window.location.search);
        const p = params.get('milo') || '';
        const len = lengthBytesUTF8(p) + 1;
        const buf = _malloc(len);
        stringToUTF8(p, buf, len);
        return buf;
    });
    std::string out(s);
    free(s);
    return out;
}
```

Depends on: task 1.

---

**Task 3 [OPUS] — On-demand milo fetch hook in
`rb3/native/src/native_file.cpp`.** **DONE.** Added an `__EMSCRIPTEN__` block to
`NativeStdioFile`'s ctor: on a failed READ-mode `fopen`, call
`WebAssetsFetchSync(path)` (engine `WebAssets.cpp:261`) then retry `fopen` with
the same path. Verified working: a `gem_smasher_guitar.milo_xbox` load pulls its
deps lazily — `gem_smasher_guitar_meshes.milo_xbox`, `track_shared.milo_xbox`,
`ingame_bank.milo_xbox` (all 200) — and the dev-only `selvenue.dta` /
`frame_rate.dta` 404s parse as empty (the documented File.cpp tolerance).

`HmxNativeOpenFile` (line 93) currently delegates to `NativeStdioFile` /
`fopen`. Under Emscripten, assets must be fetched from the dev server into
MEMFS before `fopen` can open them. The engine's `AsyncFile_Native.cpp` hosts
this hook normally but is excluded from the RB3 build
(`rb3/native/CMakeLists.txt:161`) because RB3's `AsyncFile` vtable differs.

Add an `__EMSCRIPTEN__` path to `HmxNativeOpenFile`: if `fopen` fails, call
`WebAssetsFetchSync(path)` (defined in engine
`milo-native-engine/src/platform/WebAssets.cpp:261`, signature
`bool WebAssetsFetchSync(const char* memfsPath)`) then retry. This is what
makes `DirLoader::LoadObjects` work in-browser — it calls `HmxNativeOpenFile`
for every file in the milo's dependency graph.

Depends on: task 1 (needs the build working first; can be developed in parallel
with task 2).

---

**Task 4 [SONNET] — Refactor `rb3/native/src/rb3_render_mesh.cpp`** into
`LoadMiloAndWalk(path) -> WalkResult` + `RenderFrame(walk)` + `RenderToPng(walk)`
helpers; keep `_exit(rc)` ONLY in the native PNG path. Web reuses
`LoadMiloAndWalk` / `RenderFrame`. **DONE.** New shared header
`rb3_render_mesh.h` declares `WalkResult` + the 3 fns. `LoadMiloAndWalk` does the
type-def-stub inject + `DirLoader::LoadObjects` + walk/bounds + camera
pick/synth (NO GPU/config bring-up — the caller owns that). `RenderFrame` is the
per-frame `BeginFrame`/walk-draw/`EndFrame`. `RenderToPng` (readback + PNG +
`_exit`) and the native orchestrator `RunRenderMesh` are wrapped in
`#ifndef __EMSCRIPTEN__` so `_exit` can never reach the web path.
`rb3_render_mesh.cpp` added to the `rb3-web` source list (`RB3_WEB_NATIVE_GLUE`).
`rb3_render_tri.cpp` NOT needed (no shared symbols). Native PNG output verified
unchanged (gem_smasher renders the same track surface, `RENDER_MESH OK`).

Add `rb3_render_mesh.cpp` (and `rb3_render_tri.cpp` if needed for the W2
source list) back to the `rb3-web` source list in `rb3/native/CMakeLists.txt`.
Currently both are excluded at lines 577-581 because they use POSIX
`backtrace()` and PNG write paths. The refactor must isolate those to
`RenderToPng` / the native `main` path so the rest compiles under emcc.

**Critical: `_exit(rc)` must NOT be reachable from the web path.** The native
side keeps it to dodge a Dawn-vs-libc-static-dtor race on process exit. The
browser loops indefinitely; `_exit` would terminate the page.

Can run in parallel with task 3 once task 1 lands.

**W2a acceptance:** `tracksystem.milo_xbox` renders recognizably in-browser
(geometry visible, even if colors/textures are imperfect). `rb3MilosLoaded >= 1`
and `rb3FrameCount >= 30` are set in the page.

---

### W2b — Full coverage + pixelmatch (Sonnet)

Tasks 5-7 can begin once W2a lands. Tasks 5 and 4 can run in parallel if W2a
shipped without completing the libc++ fixes.

**Task 5 [SONNET] — libc++ header shims** for `Gen.cpp` and
`MultiMeshProxy.cpp`. These two TUs are excluded from the web build
(`rb3/native/CMakeLists.txt:622-623`) because libc++ privatized the explicit
`list::iterator(__base_pointer)` constructor that the matched-fork uses for
value-initialization. The recommended fix is a small `#ifdef __EMSCRIPTEN__`
shim header force-included into those 2 TUs that re-exposes the value-init
idiom — NOT an upstream port of the stlport headers. Leave `VorbisReader.cpp`
/ `Synth.cpp` excluded (audio is W3).

**Task 6 [SONNET] — Asset bundle + server verification.** No `server.py`
change is strictly required — the on-demand `/api/file/` endpoint already
serves `.milo_xbox` files via `WebAssetsFetchSync`. Verify the three test
milos' referenced textures (the `*_meshes.milo_xbox` siblings and their
referenced `Tex` objects) resolve without 404s. Optionally add a
`--preload-glob` for faster smoke-test startup. Asset files confirmed under
`rb3/orig-assets/extracted/`:
- `ui/track/gen/tracksystem.milo_xbox` (and `tracksystem_meshes.milo_xbox`)
- `ui/track/gen/gem_smasher_guitar.milo_xbox` (and
  `gem_smasher_guitar_meshes.milo_xbox`)
- `ui/main/gen/main_hub.milo_xbox`

**Task 7 [SONNET] — Extend `rb3/scripts/web/smoke-test.mjs`** with `--milo`
and `--diff-against` flags using `pixelmatch` (already in
`rb3/scripts/web/package.json`):

- Navigate to `http://localhost:8421/?milo=<path>` (URL-encode the path).
- Poll `window.rb3MilosLoaded >= 1` (5-minute timeout for large milos) then
  `window.rb3FrameCount >= 30`.
- Take canvas screenshot; write to `results/<timestamp>/<milo-basename>.png`.
- `--diff-against <png>`: run pixelmatch at epsilon=32, report diff % in
  `summary.json`. Pass if `diff <= 2%`.

Generate native baselines via `RB3_RENDER_MESH=1 rb3-native <milo>` and copy
PNGs into `rb3/orig-assets/native-refs/` before running the diff.

**W2b acceptance:** All three milos pass pixelmatch <2% vs their native PNGs.
DC3 web smoke (`dc3-decomp/scripts/web/smoke-test.mjs`) still passes. RB3
native still renders the same milos to PNG identically.

---

## Critical path summary

```
1 (engine split) → 2 (boot machine) → 3 (fetch hook) → 4 (mesh refactor)
                                                          ↓
                                              5 (libc++ shims, parallel)
                                                          ↓
                                                      6 (assets)
                                                          ↓
                                                    7 (smoke test)
```

Checkpoints:
- After task 3: a milo loads (prints "milo loaded"), frame count climbs even
  if geometry is wrong (shows the engine is traversing the object graph).
- After task 4: mesh geometry is visible in the browser canvas.
- After task 7: pixelmatch diff passes on all three milos.

## Known gotchas and risks

**Risk 1 — Which stubbed symbols are actually hit on the mesh path.**
Most render stubs (`RndMesh::DrawShowing`, `RndTex::SyncBitmap/PresyncBitmap`)
are already strongly defined by `Rnd_Wgpu_RB3.cpp` in the web build, so the
W1 link-stub concern is largely moot for those. But `Striper`/`STRIPERRESULT`
(around `Mesh.cpp:569` in the matched fork) and `RndGenerator`/
`RndMultiMeshProxy` typeinfo could be reached depending on milo content. The
impl agent must run against a real milo load and watch `[rb3-stub]` console
warnings.

**Risk 2 — WebGPU surface format (BGRA8 vs RGBA8).**
`BandRnd::mTargetFmt` defaults to `RGBA8Unorm`
(`Rnd_Wgpu_RB3.h:124`), but Chrome's canvas surface prefers BGRA8. DC3 hit
this exact issue. Colors-swapped is the tell. Check `GpuDevice_Web`'s surface
configuration and ensure it queries the preferred surface format (as DC3's
`dc3-decomp/docs/plans/web-port/PLAN.md` Phase 5 describes), then propagate
the result into `mTargetFmt`.

**Risk 4 — Multi-chunk `ChunkStream` load fault (TOP W2b BLOCKER).**
`ui/src/system/utl/ChunkStream.cpp` reads `.milo*` containers as N chunks
(header word at file offset +8 = chunk count). **Single-chunk milos load
cleanly; multi-chunk ones fault.** Symptoms:
- **Native:** SIGSEGV in `ObjectDir::PreLoad` → `vector<ObjectDir::Viewport>::
  resize(garbage)` for `tracksystem.milo_xbox` (7 chunks) and
  `main_hub.milo_xbox` (27) — the stream is misaligned by the chunk-transition
  arithmetic, so a bogus viewport count is read and the resize over-allocates.
  (Confirmed pre-existing: `gem_smasher_guitar_meshes.milo_xbox` (1 chunk) and
  `gem_smasher_guitar.milo_xbox` (1 chunk) load fine; `track_shared.milo_xbox`
  (60 chunks) actually *parses* OK natively — so the fault is sensitive to the
  exact chunk layout, not merely "more than one chunk".)
- **Web:** parsing a multi-chunk subdir dep (`track_shared`, `ingame_bank`)
  spins ~46 s with no console output, then OOMs the wasm heap (`[CRASH] page
  crashed`). The single-threaded web build is suspect: `ChunkStream` has an
  async-decompress worker (`gDecompressionQueue` + `DecompressChunkAsync` /
  `PollDecompressionWorker`); for *uncompressed* (`0xCABEDEAF`) milos the chunk
  is marked `kReady` synchronously, so the spin is more likely in the
  `Eof()`/`ReadChunkAsync` double-buffer state machine (buffer indices
  `mCurChunk[bufIdx]`, `mBuffersOffset`) than in decompression. The runaway
  `vector::resize` then OOMs instead of segfaulting (wasm bounds-checks the
  store). `EndianSwapEq<int>` is a `[rb3-stub]` no-op on the web build, but that
  is CORRECT on a little-endian host (verified: adding a real swap BROKE the
  single-chunk native load) — so it is NOT the cause; do not "fix" it.
- **Fix direction for W2b:** debug `ChunkStream::Eof()` / `ReadChunkAsync` /
  `ReadChunkInfo` for the uncompressed multi-chunk case (compare a 2-chunk milo
  read offset-by-offset against the matched Wii asm). Until then, the three
  named test milos (`tracksystem`, `gem_smasher_guitar`, `main_hub`) cannot
  pixelmatch end-to-end; W2b task 7 should be validated against the single-chunk
  `*_meshes.milo_xbox` siblings (`gem_smasher_guitar_meshes` works today;
  `tracksystem_meshes` is 12 chunks and will also need this fix).

**Risk 3 — Texture upload under Dawn-web.**
`WriteTexture` has a 256-byte `bytesPerRow` alignment requirement that is
enforced more strictly in the browser's Dawn validation than in native Vulkan
validation layers. RB3's native build already satisfies `kAlign=256` (see
`BandUniformRing` in `Rnd_Wgpu_RB3.cpp`), so uniform buffers are safe. The
risk is the texture upload path in `UploadRndTexIfNeeded`: `layout.bytesPerRow`
is computed as `w * 4` (line 515 of `Rnd_Wgpu_RB3.cpp`) and must be rounded
up to 256 for textures whose width is not a multiple of 64. CPU DXT decode path
(`DecompressDXT1/3/5`) is also untested on web — watch for validation errors.

## Acceptance test

First generate native baselines (once):

```sh
cd rb3
RB3_RENDER_MESH=1 ./build/rb3-native orig-assets/extracted/ui/track/gen/tracksystem.milo_xbox
cp tracksystem.png orig-assets/native-refs/tracksystem.png
# repeat for gem_smasher_guitar and main_hub
```

Then for each test milo:

```sh
node scripts/web/smoke-test.mjs \
    --milo ui/track/gen/tracksystem.milo_xbox \
    --diff-against orig-assets/native-refs/tracksystem.png \
    --frames 30
```

Expected `summary.json`:
```json
{ "result": "pass", "milosLoaded": 1, "frames": 30, "diff": "<2%" }
```

All three must pass. The DC3 web smoke must still pass. Native PNG output must
be unchanged.

## Suggested subagent prompts

### W2a prompt (Opus)

> Execute Phase W2a of the RB3 web port plan
> (`rb3/docs/plans/web-port/W2_RENDER_MILO.md`). W1 has landed: the browser
> clears the canvas using a file-static `sWebGpu` device — BandRnd is NOT yet
> wired. W2a's job is to retire `sWebGpu` and drive `gBandRnd` instead.
>
> Step 1 (engine edit, must land first): Split `BandRnd::InitGpu`
> (`milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp:595`) into
> `StartGpuInit(w,h,headless)->bool` (calls `mGpu.Init(desc)` only) and
> `InitGpuResources()` (idempotent, pipelines + depth tex + 4 uniform rings +
> `CreateDefaultTextures` + `mGpuReady=true`, lines 606-628). Mirror DC3's
> `WgpuRnd::InitGpuResources()` pattern (engine `Rnd_Wgpu.cpp:320`). Keep the
> monolithic `InitGpu` for native callers. Update `Rnd_Wgpu_RB3.h` to declare
> both new methods. Bump `MILO_ENGINE_PIN` in `rb3/native/CMakeLists.txt` after
> the engine commit lands.
>
> Step 2: Rewrite the boot machine in `rb3/native/src/main_web.cpp` to use
> `gBandRnd.StartGpuInit()` in `BOOT_ENGINE_INIT`, poll `gBandRnd.Gpu().IsReady()`
> in `BOOT_GPU_WAIT`, call `gBandRnd.InitGpuResources()` in `BOOT_GPU_READY`.
> Add `BOOT_LOADING_MILO` (parse `?milo=` URL param, call `LoadMiloAndWalk`)
> and `BOOT_RUNNING_RENDER` (per-frame `BeginFrame`/draw/`EndFrame`). Delete
> `sWebGpu` and `DrawClearFrame`. Emit `window.rb3MilosLoaded`.
>
> Step 3: Add on-demand fetch fallback in `rb3/native/src/native_file.cpp`
> `HmxNativeOpenFile`: under `__EMSCRIPTEN__`, if `fopen` fails call
> `WebAssetsFetchSync(path)` (engine `WebAssets.cpp:261`) then retry `fopen`.
>
> Step 4: Refactor `rb3/native/src/rb3_render_mesh.cpp::RunRenderMesh` into
> `LoadMiloAndWalk(path)->WalkResult` + `RenderFrame(walk)` + `RenderToPng(walk)`.
> The refactor is NOT mechanical — load is interleaved with boot and there is an
> `_exit(rc)` at the bottom that MUST NOT survive in the web path. Add
> `rb3_render_mesh.cpp` back to the `rb3-web` source list.
>
> W2a acceptance: `tracksystem.milo_xbox` renders recognizably in-browser
> (`rb3MilosLoaded >= 1`, `rb3FrameCount >= 30`). Do not break the native PNG
> output or the DC3 web build.

### W2b prompt (Sonnet)

> Execute Phase W2b of the RB3 web port plan
> (`rb3/docs/plans/web-port/W2_RENDER_MILO.md`). W2a has landed: one milo
> renders in-browser. W2b completes coverage and adds pixelmatch verification.
>
> Task 5: Fix the libc++ `list::iterator` explicit-ctor issue in `Gen.cpp` and
> `MultiMeshProxy.cpp` so they compile under emcc. Recommended approach: a
> small `#ifdef __EMSCRIPTEN__` shim header re-exposing the value-init idiom,
> force-included into those two TUs. Do NOT port upstream stlport headers.
> Remove both from `RB3_WEB_NATIVE_FORK_EXCLUDE` in
> `rb3/native/CMakeLists.txt:620`.
>
> Task 6: Verify asset resolution for all three test milos:
> `ui/track/gen/tracksystem.milo_xbox`, `ui/track/gen/gem_smasher_guitar.milo_xbox`,
> `ui/main/gen/main_hub.milo_xbox`. Confirm their referenced texture/mesh sibling
> milos (e.g. `tracksystem_meshes.milo_xbox`, `gem_smasher_guitar_meshes.milo_xbox`)
> are served correctly by the `/api/file/` endpoint. No `server.py` change
> required; just verify no 404s during a real milo load.
>
> Task 7: Extend `rb3/scripts/web/smoke-test.mjs` with `--milo <path>` and
> `--diff-against <png>` flags. Use `pixelmatch` (already in `package.json`).
> Poll `window.rb3MilosLoaded >= 1` then `window.rb3FrameCount >= 30`. Write
> PNG + `summary.json`. Tolerance: 2% pixels differing at epsilon=32. Generate
> native baselines via `RB3_RENDER_MESH=1 rb3-native <milo>`.
>
> W2b acceptance: all three milos pass pixelmatch <2%. DC3 web smoke still
> passes. Native PNG output unchanged.
