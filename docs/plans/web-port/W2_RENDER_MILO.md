# W2 — Render one .milo in browser

**Parent plan:** [`PLAN.md`](PLAN.md). Read it for context.
**Depends on:** [`W1_CLEAR_FRAME.md`](W1_CLEAR_FRAME.md) (browser clears canvas, ticks frames).
**Blocks:** W3.

## Goal

Parity with `RB3_RENDER_MESH=1 rb3-native <milo_path>` — but in the browser,
rendering to the canvas instead of a PNG. Three milos verified end-to-end:

1. `ui/track/gen/tracksystem.milo_xbox` (129 meshes, 27878 tris — the
   native acceptance reference).
2. `ui/track/gen/gem_smasher_guitar.milo_xbox` (strike pads + beveled
   text — used to verify text pipeline).
3. `ui/main/gen/main_hub.milo_xbox` — verifies UI 2D draw path.

For each: a browser screenshot visibly matches the corresponding native PNG
under reasonable tolerance (pixel-exact Linux Vulkan ↔ browser WebGPU is
unrealistic).

## Out of scope

- Audio. W3.
- Texture sampling at full fidelity. RB3 native currently renders with
  diffuse=white (textures not yet sampled — same constraint carries over).
  Don't block W2 on this.
- Skinning. Native uses an identity bone palette; same here.
- The latent matched-fork heap over-write that trips on some char milos
  natively — same constraint carries over; pick milos that don't trip it.

## Files to touch

### `rb3/native/src/main_web.cpp`

Extend the W1 boot machine to accept a milo path:

1. Parse a URL query parameter via `EM_ASM_PTR`:
   `?milo=ui/track/gen/tracksystem.milo_xbox`. Default to no-milo
   (W1 clear-frame behaviour).
2. After `BOOT_GPU_READY`, if a milo path was supplied, transition to a
   new `BOOT_LOADING_MILO` state that calls the same `DirLoader::LoadObjects`
   path `rb3_render_mesh.cpp` uses. See the refactor section below for
   the helper-split strategy (the load logic is interleaved with boot in
   `RunRenderMesh`, so it's not a simple lift).
3. After load, transition to `BOOT_RUNNING_RENDER`:
   - Each frame: drive `BandRnd` per the mesh-walking loop that
     `RB3_RENDER_MESH` already implements (mesh walk + per-draw uniform
     update + `BeginFrame` / draw / `EndFrame`).
   - **Do not call any `Present`/`Swap` API** — under WebGPU, the surface
     auto-composites at end of `requestAnimationFrame`. The native
     `Screenshot::ReadbackToPng` step in `rb3_render_mesh.cpp` is simply
     omitted in the web path.
4. Emit `window.rb3FrameCount` and `window.rb3MilosLoaded` for the test
   harness.

### `rb3/native/src/rb3_render_mesh.cpp`

**This refactor is not mechanical.** `RunRenderMesh` interleaves: GpuDevice
init → `chdir` → `SystemPreInit` / `SystemInit` → object-type-def stub
injection → `gBandRnd.PreInitRender()` → `DirLoader::LoadObjects` →
bounds-compute → camera synthesis → draw loop → readback → PNG write →
`_exit(rc)` (line ~371). Only the draw loop + readback are cleanly
separable.

Split into three helpers that match the actual control flow:

- `BootOnce()` — GpuDevice + chdir + SystemPreInit/SystemInit + object
  factory registration + `gBandRnd.PreInitRender()`. Called once. Already
  done by W1's `main_web.cpp` for the web path, so the web target reuses
  W1's boot and skips this.
- `LoadMiloAndWalk(const char* path) -> WalkResult` — `DirLoader::LoadObjects`
  + bounds-compute + camera synthesis. Pure load + walk; no rendering.
  Shared by native + web.
- `RenderFrame(const WalkResult&)` — one frame's `BeginFrame` / draw loop
  / `EndFrame`. Shared by native + web.
- `RenderToPng(const WalkResult&)` — native PNG readback (existing
  semantics, post-`RenderFrame`).

**Critical: do NOT preserve the `_exit(rc)` call in the web path.** The
native side keeps it (it dodges a Dawn-vs-libc-static-dtor race on
process exit). The browser loops indefinitely; `_exit` would terminate
the page.

`main_web.cpp` drives:
- W1 boot (its own equivalent of `BootOnce`)
- `LoadMiloAndWalk(path)` once after `BOOT_GPU_READY`
- `RenderFrame(walk)` every tick
- No PNG path

### `rb3/native/src/rb3_band_rnd.cpp`

W1 already added `InitForCanvas()`; W2 needs no new BandRnd API. The
draw loop uses the existing `BeginFrame(cam)` / per-draw / `EndFrame()`
methods native already uses.

Verify uniform buffer alignments under WebGPU — Dawn enforces stricter
alignment than some native validation layers. Native Linux on Vulkan
already passes Dawn validation since the engine builds against Dawn, so
this is unlikely to bite, but watch for `kAlign = 256` vs the WebGPU
minimum (`minUniformBufferOffsetAlignment`).

### `rb3/native/src/main_web.cpp` query-param hook

```cpp
// EM_ASM_INT returns the malloc'd cstring length so we know to free.
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

### Asset bundle for W2

The W1 bundle was `config/`-only. W2 adds:

- `ui/track/gen/` (houses both `tracksystem.milo_xbox` and the
  `gem_smasher_*` milos + their referenced textures).
- `ui/main/gen/main_hub.milo_xbox` + dependencies.

The server (`server.py`) auto-includes everything under `extracted/`; add
a `--include-glob` filter or just bundle the test milos explicitly.

### `rb3/scripts/web/smoke-test.mjs`

Add a `--milo <path>` flag. When set:

- Navigate to `http://localhost:8421/?milo=<path>` (URL-encode).
- Wait until `window.rb3MilosLoaded >= 1` (5-minute timeout for big milos).
- Wait until `window.rb3FrameCount >= 30` so the render has settled.
- Take a canvas screenshot; write to `results/<timestamp>/<milo-basename>.png`.

Add a `--diff-against <png>` flag for the native baseline PNG:

- Use `pixelmatch` (already declared in `rb3/scripts/web/package.json` by
  W1) to compute pixel-diff. If W1 forgot to install it, add now: `npm
  install --save pixelmatch`.
- Tolerance: default 2% pixels differing > epsilon=32. Report `diff
  percentage` in `summary.json`.
- The test harness should pass when `diff <= tolerance`.

## Acceptance test

For each of the three test milos, run from `rb3/`:

```sh
node scripts/web/smoke-test.mjs --milo ui/track/gen/tracksystem.milo_xbox \
    --diff-against orig-assets/native-refs/tracksystem.png \
    --frames 30
```

(Generate `native-refs/` once via `RB3_RENDER_MESH=1 rb3-native <milo>` →
copy PNG into `orig-assets/native-refs/`.)

Expect for tracksystem:
- `summary.json: { result: pass, milosLoaded: 1, frames: 30, diff: <2% }`
- `tracksystem.png` (browser screenshot) visibly matches
  `orig-assets/native-refs/tracksystem.png`.

Same for gem-smasher and UI panel. All three must pass.

Regression checks:
- DC3 web smoke (`dc3-decomp/scripts/web/smoke-test.mjs`) still passes.
- RB3 native still renders the same milos to PNG identically.

## Known gotchas

- WebGPU surface format is BGRA8Unorm (Chrome's preferred). The native
  Linux path may use RGBA8Unorm. The engine's web `GpuDevice` surface
  configuration (lifted in W0) must pick the surface-preferred format —
  DC3 hit this exact issue (`dc3-decomp/docs/plans/web-port/PLAN.md`
  Phase 5 "WebGPU surface format fix"). If RB3 colours come out swapped,
  this is the cause.
- MSAA: native uses 4×; DC3 disables MSAA on web (`sampleCount=1`). RB3
  should do the same — wire under `HX_WEB`.
- Texture upload path under WASM: `wgpu::Queue::WriteTexture` works the
  same in both worlds, but the `Bytes per row` alignment requirement
  (256) is enforced strictly in Dawn — RB3's native build already
  satisfies this because it uses Dawn natively, so it should carry.
- Large milos can take 5-30s to load over HTTP in dev. Bump
  smoke-test.mjs timeout accordingly.
- The native render harness writes a single PNG and **calls `_exit(rc)`**
  (`rb3_render_mesh.cpp` ~line 371) to dodge a Dawn-vs-libc-static-dtor
  race. The web target loops. Make sure the split helpers do **not**
  invoke `_exit` from the web path.

## Suggested subagent prompt

> Execute Phase W2 of the RB3 web port plan
> (`rb3/docs/plans/web-port/W2_RENDER_MILO.md`). W1 has landed: the browser
> clears the RB3 canvas. Extend `main_web.cpp` to load a `.milo` (path
> from a `?milo=` URL query param) and render per-frame to the canvas (no
> Present/Swap call — WebGPU auto-composites at rAF return). Refactor
> `rb3_render_mesh.cpp::RunRenderMesh` into `BootOnce` / `LoadMiloAndWalk`
> / `RenderFrame` / `RenderToPng` helpers — the refactor is **not**
> mechanical (load is interleaved with boot, and there's an `_exit(rc)` at
> the bottom that must NOT survive in the web path). Three test milo
> paths: `ui/track/gen/tracksystem.milo_xbox`,
> `ui/track/gen/gem_smasher_guitar.milo_xbox`, and
> `ui/main/gen/main_hub.milo_xbox`. Extend
> `rb3/scripts/web/smoke-test.mjs` with `--milo` and `--diff-against`
> flags using `pixelmatch`. Acceptance: three milos render in-browser
> within 2% pixel diff of their native PNGs. Generate native baselines
> via `RB3_RENDER_MESH=1 rb3-native` first if `orig-assets/native-refs/`
> doesn't exist. Do not break the native PNG output or the DC3 web build.
