# R6 — Post-async CPU floor: GPU BC (DXT) decode + decode-pipeline trims

> **Status:** design, **Wave 3 / post-async** (lower priority). Depends on R1–R4
> landing first (the synchronous on-demand fetch must already be off the main
> thread, see `docs/native/web-netperf-findings-2026-06-08.md`) — until the
> network freeze is gone, the per-transition A/B is noise. This is a
> **per-transition CPU + peak-memory + VRAM win, NOT a boot-wall win.** This doc
> is the handoff for the implementation agent.

## Problem & data

Once the synchronous on-demand fetch is removed (R1–R4), the network freeze
collapses toward the loopback numbers and what's left is the **real CPU work**
the wasm main thread does to turn fetched bytes into GPU-ready data. The
measured shape of that floor (from the two profiling passes in the repo):

- **Boot CPU floor (`new App()`):** the boot window is ~78% idle; only ~5 s is
  real CPU work. Self-time breakdown
  (`docs/native/web-loadperf-findings-2026-06-03.md`, lines 39–43):
  DTA lexer (`yylex`/`yy_*`/`DataInput`) ~0.4 s, **DXT decompress
  (`DecompressDXT1/5*`) ~0.25 s**, `std::vector<uint8>` buffer ctors ~0.7 s,
  plus `BinStream::Read` + `DataArray::FindArray` + `NativeStdioFile::Read`.
  At boot, DXT decode is a *minor* slice; DTA lexing + buffer churn dominate.
- **Per-transition CPU (the bigger DXT win):** the menu-transition offenders
  (`docs/native/web-netperf-findings-2026-06-08.md`, lines 74–88) are
  texture-heavy milos —
  `char/main/shared/gen/colorpalettes.milo_xbox` (20.8 MB),
  `world/venue/.../small_club_01.milo_xbox` (19.4 MB),
  `world/vignette/shell/gen/sv*_a.milo_xbox` (11–12 MB each). The **bulk of
  those bytes is DXT block data**, and every byte is currently CPU-expanded to
  RGBA8 on the main thread at upload time, then thrown away.

**Two measured facts pin R6's value (and bound it):**

1. **The milos are incompressible** — gzip of the named offenders is
   essentially 1.00 (`colorpalettes` raw 11M → gzip 11M, ratio **1.00**;
   `small_club_01` raw 13M → gzip 13M, ratio **0.98**). Their payload is
   *already* compressed (DXT blocks). This is why **R5 (wire compression) cannot
   touch the worst offenders** — the only way to shrink them is to stop
   expanding them. R6 is the lever R5 cannot be.
2. **CPU DXT→RGBA8 is a 4–8× blow-up.** A 4×4 DXT1 block is 8 bytes; DXT3/DXT5
   are 16 bytes; the RGBA8 expansion is always 64 bytes. So every DXT texture
   currently allocates **4× (DXT3/5) to 8× (DXT1)** its size in a transient
   `std::vector<uint8_t>` (`Rnd_Wgpu_RB3.cpp:479`), runs a per-pixel decode
   loop, uploads the fat RGBA8, and frees the vector. R6 uploads the BC blocks
   **as-is**: no decode loop, no expansion buffer, ~4–8× less VRAM.

**Realistic expected savings.** R6 is **not** a big *boot-wall* win (DXT is
~0.25 s of a ~5 s boot floor, itself dwarfed by the ~7 s async idle R1–R4
remove). Its wins are: (a) shaving per-transition CPU on the texture-heavy
venue/palette loads (the colorpalettes/venue transitions, where DXT decode is a
much larger fraction than at boot), (b) cutting **peak memory / GC pressure** by
eliminating the 4–8× RGBA8 expansion buffers (relevant to the wasm 32-bit heap),
and (c) cutting VRAM 4–8× for DXT textures. Frame R6 honestly as the
*post-async polish* item: a memory-footprint + transition-smoothness win, not a
headline boot-time win.

## Architecture

The engine **already has** a complete, production-proven GPU BC path —
`TextureConvert.cpp` (`MapBitmapFormat` → `BC1RGBAUnorm`/`BC2RGBAUnorm`/
`BC3RGBAUnorm` when `GpuDevice::HasBCCompression()`), shipped by DC3
(`MILO_ENGINE_GPU_BACKEND=dc3`, links `TextureConvert.cpp`). **RB3 does not use
it.** RB3 builds the `rb3` GPU-backend flavor
(`milo-native-engine/CMakeLists.txt:100,277–287`), which deliberately **drops
`TextureConvert.cpp`** and instead carries a *self-contained, always-CPU-decode*
upload path inside `Rnd_Wgpu_RB3.cpp::UploadRndTexIfNeeded`
(`src/platform/Rnd_Wgpu_RB3.cpp:453–549`). That function hardcodes
`fmt = RGBA8Unorm` and always runs `DecompressDXT1/3/5` (lines 476–495).

So R6 is conceptually small: **teach `UploadRndTexIfNeeded` the BC-direct branch
the engine already proved in `TextureConvert.cpp`.** Data/control flow:

1. **Capability flag (already plumbed).** `GpuDevice::HasBCCompression()` is set
   for the web build by the shared `GpuDevice_Web.cpp`
   (linked into rb3-web at `native/CMakeLists.txt:650`): on adapter acquire it
   calls `mAdapter.HasFeature(TextureCompressionBC)` and, if present, requests
   the `TextureCompressionBC` device feature (`GpuDevice_Web.cpp:74–82`). The
   flag is *live today*; RB3's renderer just ignores it.
2. **Format decision.** In `UploadRndTexIfNeeded`, before choosing
   `RGBA8Unorm`, gate on `dxt && mGpu.HasBCCompression() && IsBCDirectable(dxt)`:
   - `0x08` (DXT1) → `BC1RGBAUnorm` (8 bytes/block)
   - `0x10` (DXT3) → `BC2RGBAUnorm` (16 bytes/block)
   - `0x18` (DXT5) → `BC3RGBAUnorm` (16 bytes/block)
   - anything else (DXN/`0x20`, palette, bpp 8/24/32) → keep the existing
     CPU/RGBA8 path unchanged.
3. **Endian fix stays, decode goes.** The Xbox BE 16-bit `ByteSwapDXT16`
   (`Rnd_Wgpu_RB3.cpp:444`) is **still required** for the BC path — the GPU
   needs little-endian block words. Run it on the copied block buffer, then
   upload that buffer *directly* (no `DecompressDXT*`, no RGBA8 `std::vector`).
4. **Upload layout.** Texture descriptor `format = BCnRGBAUnorm`;
   `WriteTexture` with `bytesPerRow = blockW * blockBytes`
   (`blockW = (w+3)/4`, `blockBytes = (dxt==0x08)?8:16`) — exactly what the
   engine's `TextureConvert.cpp:521–525` does. **Queue `WriteTexture` does not
   require 256-byte `bytesPerRow` alignment** (that constraint is only for
   `CopyB2T` via a GPUBuffer), and DC3 ships this raw value — so no padding
   logic is needed.
5. **Explicit CPU fallback (load-bearing).** When `HasBCCompression()` is false
   (no `texture-compression-bc` adapter feature — possible on some mobile GPUs and
   software/SwiftShader adapters), the BC branch is skipped entirely and the
   existing CPU `DecompressDXT*` → RGBA8 path runs **unchanged**. This is the
   correctness floor and must stay: R6 *adds* a branch, it never *removes* the CPU
   decoder. Note: the capability is queried per-adapter at device init
   (`GpuDevice_Web.cpp:74` web / `GpuDevice.cpp:120` native), so the branch is
   selected once and is stable for the process. **Caveat for verification:** the
   native screenshot harness here runs on a **real BC-capable Vulkan adapter**, so
   `HasBCCompression()` is *true* there — BC is **live, not inert**, in CI/native
   screenshots. Do not assume the fallback keeps native baselines byte-identical
   (see Verification — the diff needs a tolerance).

The decode-pipeline trim is a second, independent lever on the same function and
on `WebAssets`: cut the transient `std::vector<uint8_t>` churn (the RGBA8
expansion is the biggest single allocation; BC-direct removes it for DXT, and
for the remaining CPU paths the work copy + decompress buffer can be pooled).

## Implementation plan

Phased, smallest-blast-radius first. All edits are in the **shared engine**
(`milo-native-engine`), built via `cmake --build native/build-native` /
`scripts/web/build.sh` — *not* `--target rb3-native` (BandRnd lives in the
`milo-engine` target).

**Phase 1 — BC-direct upload in BandRnd (the core win).**
- `src/platform/Rnd_Wgpu_RB3.cpp::UploadRndTexIfNeeded` (lines ~476–541): add a
  `MapRB3DxtToBC(dxt)` helper returning `{wgpu::TextureFormat, blockBytes}` or a
  "not directable" sentinel. Branch: if `dxt && mGpu.HasBCCompression() &&
  directable`, set `fmt` to the BC format, run `ByteSwapDXT16` on the copied
  block buffer, and skip the RGBA8 allocation + `DecompressDXT*` entirely. The
  upload descriptor + `WriteTexture` then use `bytesPerRow = blockW*blockBytes`
  and `uploadData = work.data()`.
- Mirror the same branch in the cube-map upload if RB3 has one
  (grep shows BandRnd's sky-dome uses RTT, not cube bitmaps — verify; the
  engine's `CreateCubeFromBitmaps` is the reference if needed).
- Gate behind an env opt-out (mirror the project's pattern, e.g.
  `RB3_NO_GPU_BC` / `getenv`) so the implementation agent can A/B BC-direct vs
  CPU-decode without a rebuild, and so a BC-specific visual regression can be
  bisected. Default **on** when `HasBCCompression()`.

**Phase 2 — correctness guards.**
- **Font atlases (DXT5 glyph-in-alpha).** The text path
  (`Rnd_Wgpu_RB3.cpp:3797`, `useAlphaAsRGB`) samples `texture.a`. BC3 stores
  alpha at full 8-bit in its dedicated alpha block, so font atlases render
  identically under BC3-direct — **no special-casing needed**, but the
  verification step must include a text-heavy screen (song list / HUD) to prove
  glyphs aren't black.
- **DXN / BC5 normal maps (`0x20`).** Keep these on the **CPU path**: the engine
  comment (`TextureConvert.cpp:377–379`) documents that `BC5RGUnorm` yields
  `(R,G,0,1)` → B=0 → flipped normal Z, which the shader's DXT5nm heuristic
  mis-decodes. RB3's current decoder doesn't even handle DXN (the `default` case
  leaves the 0xFF-initialized buffer opaque white at `Rnd_Wgpu_RB3.cpp:492–494`),
  so `IsBCDirectable` must return false for `0x20` and
  the CPU branch keeps current (wrong-but-unchanged) behavior — R6 must not
  regress it, and fixing DXN is out of scope.
- **Mip handling.** Current RB3 upload is `mipLevelCount = 1` (single level,
  `Rnd_Wgpu_RB3.cpp:534`). BC textures with `w` or `h` not a multiple of 4 are
  legal at mip 0 (block grid rounds up); keep `mipLevelCount = 1` to match
  current behavior — don't add mip upload in R6.

**Phase 3 — decode-pipeline / buffer-churn trim (independent of BC).**
- Pool/reuse the transient buffers in the **remaining CPU paths** of
  `UploadRndTexIfNeeded` (the `std::vector<uint8_t> work` copy and, for non-BC
  adapters, the RGBA8 `rgba` buffer). A thread-local reusable scratch vector
  avoids per-texture malloc/free churn during a transition that uploads dozens
  of textures.
- (Optional, larger) Look at `WebAssets.cpp` / `native_file.cpp` read buffers:
  the fetched milo is read into a `std::vector<uint8>` then parsed; confirm
  there's no avoidable second copy. This overlaps R3/R4 territory — coordinate.

**Phase 4 — measure & tune.** Run the netperf suite A/B (see Verification). If
BC-direct measurably helps the colorpalettes/venue transitions, consider
extending it: precompute byte-swapped BC blocks at asset-prep time so even
`ByteSwapDXT16` drops out of the hot path (asset-side change in
`scripts/web/` / the extractor — out of scope for the first cut, note as an
open question).

## Key files & call sites (verified)

- `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp:453` —
  `UploadRndTexIfNeeded(GpuDevice&, RndTex*)`. **The function R6 edits.** Hardcodes
  `fmt = wgpu::TextureFormat::RGBA8Unorm` at line 478; CPU decode at 484–495
  (`DecompressDXT1/3/5`, switch on `order & 0x38`); RGBA8 alloc at 479; upload at
  529–541 (`bytesPerRow = w*4` at 539).
- `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp:444` — `ByteSwapDXT16` (keep;
  needed for BC too).
- `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp:354–441` — self-contained
  `DecompressDXT1/3/5*` (stay as the no-BC fallback).
- Call sites of the upload (so the change reaches everything):
  `Rnd_Wgpu_RB3.cpp:1221,1230,2212,2582,4064,4068,4361` — material diffuse, noise
  map, draw-time `BandPatchMesh`/RndTex uploads; `GetRB3TexView` cache at
  `553–556`, side-table `sTexGpu` at `340`.
- `milo-native-engine/src/gfx/TextureConvert.cpp:369` — `MapBitmapFormat`
  (the **reference implementation** of the DXT→BC mapping, incl. the DXN caveat
  at 377–379); `CreateFromBitmap:394` does the BC-direct upload R6 mirrors
  (`blockBytes = (dxt==kDXT1)?8:16`, `bytesPerRow = blockW*blockBytes` at 524–525).
  **DC3 ships this** — proof the BC queue-upload path is correct (but **not** that
  it reproduces RB3's CPU decoder bit-for-bit; see Verification).
- `milo-native-engine/src/gfx/GpuDevice.h:74,119` — `HasBCCompression()` /
  `mHasBCCompression`.
- `milo-native-engine/src/platform/GpuDevice_Web.cpp:74–82` — web adapter sets
  `mHasBCCompression` + requests the `TextureCompressionBC` device feature.
  **Already linked into rb3-web** (`rb3/native/CMakeLists.txt:650`).
- `milo-native-engine/src/gfx/GpuDevice.cpp:120,134–135` — native equivalent
  (`mHasBCCompression = adapter.HasFeature(TextureCompressionBC)` at 120; requests
  the feature at 134–135). **The native screenshot harness uses this adapter and
  it advertises BC** — so BC-direct is exercised, not bypassed, in native CI.
- `milo-native-engine/CMakeLists.txt:100,277–287` — the `rb3` backend flavor that
  **excludes `TextureConvert.cpp`** ("BandRnd does its own DXT decompress … for
  now"). R6 closes that "for now".
- `rb3/native/CMakeLists.txt:257` (dc3 side) vs RB3's `MILO_ENGINE_GPU_BACKEND`
  — confirm RB3 sets `rb3` (it does; the `dc3` line shown is in dc3-decomp).
- Data: `rb3/orig-assets/extracted-xbox-full/char/main/shared/gen/colorpalettes.milo_xbox`,
  `.../world/venue/small_club/small_club_01/gen/small_club_01.milo_xbox` — the
  gzip-incompressible offenders measured above.

## Risks & tradeoffs

- **Adapter feature availability.** `texture-compression-bc` is broadly available
  on desktop D3D12/Vulkan/Metal browsers, but **not guaranteed** on some mobile
  GPUs or software/SwiftShader adapters. Mitigation: the CPU path stays as the
  fallback, gated on `HasBCCompression()`. **Note — the native screenshot harness
  here DOES advertise BC** (`GpuDevice.cpp:120,134–135`), so native CI runs the BC
  branch, not the CPU fallback. The old assumption that CI stays "byte-identical
  to today" is **false** — see the next bullet and Verification.
- **Visual regression surface — and a real (sub-LSB) baseline shift.** BC1/2/3
  *are* the DXT1/3/5 block formats, so a correct BC-direct upload is **visually**
  equivalent to a correct CPU decode. But the GPU's hardware block decoder is
  **not bit-for-bit identical** to RB3's inline `DecompressDXT*`, which is a
  *truncating integer* decoder (no rounding) — the two legitimately differ by a
  few LSBs per channel, especially on DXT1's 2/3–1/3 color interpolation and
  DXT3's 4-bit alpha. Because the native harness runs on a BC-capable adapter, the
  switch to BC-direct **will shift the existing screenshot baselines sub-LSB** —
  this is expected, not a bug. The diff must therefore use a **per-channel
  tolerance (±2 LSB)**, never an exact/byte-identical compare (see Verification).
  The *actual* regression to catch is a **block-layout / endian / bytesPerRow
  bug** producing scrambled or color-swapped textures — a gross, obvious diff well
  outside ±2 LSB. Mitigation: the env opt-out for instant A/B, and tolerant
  screenshot diffs on a texture-heavy + a text-heavy screen.
- **DXN must not be touched.** Routing DXN through BC5 would flip normals
  (engine-documented). `IsBCDirectable` returning false for `0x20` is load-bearing.
- **Two divergent code paths to maintain.** BandRnd now has a CPU decoder *and* a
  BC branch. Acceptable (the engine's `TextureConvert.cpp` already carries both),
  but a future Phase-2 roadmap item should consider just adopting
  `TextureConvert.cpp` wholesale into the `rb3` flavor instead of the inline copy.
- **Boot-wall impact is small.** Don't oversell: DXT is ~0.25 s of boot CPU. The
  win is per-transition CPU + memory, not the headline boot number. Setting
  expectations here avoids a "we did R6 and boot didn't move" false-negative.

## Verification

**Primary — `scripts/web/netperf-suite.mjs` A/B** (the regression/win prover):

```bash
python3 native/web/server.py &
# Baseline (CPU decode): force the opt-out
RB3_NO_GPU_BC=1 node scripts/web/netperf-suite.mjs --scenario nav --profiles normal,local --runs 3 --out /tmp/rb3-web/netperf-R6-cpu
# Candidate (BC-direct): default
node scripts/web/netperf-suite.mjs --scenario nav --profiles normal,local --runs 3 --out /tmp/rb3-web/netperf-R6-bc
```

Compare, per the suite's `summary.json` / `REPORT.md`:
- **`boot->main_hub`** and **`main_hub->song_select`** transitions (these load
  `colorpalettes` 20.8 MB + venue/vignette milos): expect lower `blockedMs` /
  `maxGapMs` on the BC run as the per-texture decode loop + RGBA8 alloc vanish.
  Use `--cpuprofile` and confirm `DecompressDXT*` self-time drops to ~0 on the BC
  run (load the `.cpuprofile` via `scripts/web/analyze-cpuprofile.mjs`).
- Because the milo bytes don't change, **`bytes` per transition is unchanged** —
  R6 is a CPU/memory win, not a network win. Watch `blockedMs` and the cpuprofile,
  not byte counts.

**Memory check.** Add a peak-`HEAP` / `Module.HEAPU8.length` log around the
texture-heavy transition (or read `performance.memory` in the harness) and
confirm peak transient allocation drops on the BC run (no 4–8× RGBA8 buffers).

**Correctness — tolerant screenshot diff (native, fast loop).** Per
`scripts/native/song-select-capture.py` / the `/api/screenshot` harness, capture
a **texture-heavy** screen (venue/gameplay) and a **text-heavy** screen (song
list / HUD) with BC on (default) vs `RB3_NO_GPU_BC=1` (CPU decode), and diff.

**The compare MUST be a per-channel tolerance, NOT exact/byte-identical.** The
native harness runs on a **real BC-capable Vulkan adapter** (`GpuDevice.cpp:120`
advertises `TextureCompressionBC`), so BC-direct is genuinely exercised — and the
GPU hardware block decoder differs from RB3's *truncating integer* `DecompressDXT*`
by a few LSBs per channel. So:
- Pass condition: **max per-channel abs delta ≤ 2 LSB** (and ideally
  mean abs delta < 0.5) between the BC-on and CPU-decode screenshots. A clean
  BC-direct upload lands well inside this band.
- Fail condition: a *gross* diff (color-swapped channels, scrambled blocks, black
  glyphs/textures, large structured regions off by ≫ 2 LSB) — that signals a
  block-layout / endian (`ByteSwapDXT16`) / `bytesPerRow` bug, not the expected
  decoder rounding difference.
- **Do not reuse a byte-identical golden** from before R6: switching the default
  to BC-direct legitimately shifts the native baselines sub-LSB. Re-baseline the
  golden from a BC-on capture, or always compare BC-on vs `RB3_NO_GPU_BC=1`
  within-run under the ±2-LSB tolerance (preferred — it self-references and needs
  no stored golden).

If a deployment target genuinely lacks BC (`HasBCCompression()==false`), that
adapter takes the CPU fallback and *is* byte-identical to today — but that is the
exception path, not the harness's path; do not design the verification around it.

**Unit-ish.** If `rb3-tests` (the gtest target) is convenient, add a check that
`MapRB3DxtToBC` returns the expected `{format, blockBytes}` for 0x08/0x10/0x18 and
"not directable" for 0x20/non-DXT — cheap guard against a future mapping typo.

## Effort, impact & dependencies

- **Effort: M.** Phase 1 is a focused edit to one function with an exact engine
  reference (`TextureConvert.cpp`) to copy from; the device feature is already
  plumbed. Phases 2–3 + the A/B verification add the bulk of the time.
- **Impact: medium.** Real per-transition CPU + peak-memory + VRAM win on the
  texture-heavy loads, and it's the *only* lever for the gzip-incompressible
  offenders R5 can't help. Low on boot-wall.
- **Risk: low–medium.** Bounded by the env opt-out + the engine-proven reference;
  the main risk is a block-layout / endian / `bytesPerRow` bug, caught immediately
  by the **tolerant (±2-LSB)** screenshot diff. (A correct upload is *not*
  byte-identical to the old CPU decode — expect a sub-LSB baseline shift, see
  Verification.)
- **Depends on:** R1–R4 (the async-fetch removal) — **Wave 3 / post-async.** R6
  only matters *after* the sync-fetch idle is gone; until then the network freeze
  dwarfs any CPU win and the A/B would be pure noise. R6 is explicitly the
  "post-async floor" item.
- **Relationship to R5 (wire compression):** **complementary, non-overlapping.**
  R5 cannot compress the texture-heavy milos (measured ~1.0 gzip ratio); R6 is the
  mechanism that shrinks them (on GPU/VRAM/CPU). If both land, R5 covers the
  text/DTA/code-ish assets and R6 covers the texture payload.
- **Unblocks:** a future "drop the inline BandRnd DXT decoder and adopt the
  engine `TextureConvert.cpp` in the `rb3` flavor" consolidation (roadmap
  Phase 2 per `CMakeLists.txt:278`).

## Open questions

- **[TOP] Do the production target browsers/GPUs advertise
  `texture-compression-bc` — especially mobile/low-end?** This sizes R6's whole
  payoff. Chrome/Firefox desktop on D3D12/Vulkan/Metal are yes; the unknowns are
  the **mobile and low-end** set the port cares about (some mobile GPUs and any
  software/SwiftShader adapter may lack BC). Every user without it stays on CPU
  decode and gets *zero* R6 benefit, so if a meaningful slice lacks it, R6's win
  is correspondingly narrower. Confirm against the real target matrix before
  treating R6's per-transition/memory win as universal.
- **Asset-prep pre-swap.** Could the extractor emit BC blocks already
  little-endian (drop `ByteSwapDXT16` from the runtime hot path entirely)? That's
  an asset-side change in `scripts/web/` and would also let the milo be uploaded
  as a zero-copy `WriteTexture`. Worth scoping separately — it trades a one-time
  asset re-derive for a runtime swap-loop removal.
- **How much of the per-transition `blockedMs` is actually DXT decode vs DTA
  lex / mesh build?** The boot profile says DXT is small *at boot*; the
  per-transition split is unmeasured. The first netperf `--cpuprofile` run should
  attribute it before investing in Phase 3 buffer pooling — if DTA lex dominates
  transitions too, R6's CPU win is capped and the *memory* win becomes the headline.
- **Mips.** Current RB3 upload ignores mip chains (`mipLevelCount=1`). Some
  venue/scene textures ship mips; uploading them (BC makes this cheap — blocks
  copy directly) could improve minification quality. Out of R6 scope but noted.
- **Should R6 just adopt `TextureConvert.cpp`** into the `rb3` flavor rather than
  patching the inline copy? Cleaner long-term, but `TextureConvert.cpp` reads
  DC3-era `RndBitmap` shapes — needs an audit that RB3's older `rndobj/Bitmap.h`
  satisfies its API (`Order/Bpp/RowBytes/PixelBytes/nextMip/PixelColor`). The
  inline patch is the lower-risk first cut.

## Review corrections applied

- **Verification claim fixed (the core revise):** dropped the false
  "byte-identical / visually identical" compare. BC hardware decode is *visually*
  but **not pixel-exact** vs RB3's truncating integer CPU decoder — Verification +
  Risks now mandate a **per-channel ±2-LSB tolerance**, not an exact diff.
- **CI/native-harness premise corrected:** the native screenshot harness runs on a
  **real BC-capable Vulkan adapter** (`GpuDevice.cpp:120,134–135` advertise +
  request `TextureCompressionBC`), so BC-direct is *live, not inert*, in CI and
  the existing baselines shift sub-LSB. Removed the "null backend keeps it
  byte-identical" assumption from Architecture, Risks, and Verification.
- **CPU fallback made explicit:** Architecture point 5 + a dedicated bullet state
  that when `HasBCCompression()==false` the CPU `DecompressDXT*` path runs
  unchanged (R6 *adds* a branch, never removes the decoder).
- **Status / sequencing:** marked R6 as **Wave 3 / post-async**, a per-transition
  CPU + peak-memory + VRAM win (NOT a boot-wall win), depending on R1–R4 so the
  A/B isn't noise.
- **Top open question recorded:** whether production target browsers/GPUs (esp.
  mobile/low-end) advertise `texture-compression-bc` — promoted to `[TOP]`.
- **Citations corrected against source:** `useAlphaAsRGB` `3646`→`3797`; DXN
  caveat `TextureConvert.cpp:381`→`377–379`; DXN white-fill `492`→`492–494`
  (`default` case); native `GpuDevice.cpp:120,133–137`→`120,134–135`;
  `TextureConvert.cpp` BC-upload block `521–525`→`524–525`. Spot-checked the
  upload function (`Rnd_Wgpu_RB3.cpp:453`, RGBA8 alloc `479`, decode `488–495`,
  `bytesPerRow=w*4` `539`, `mipLevelCount=1` `534`) — all confirmed accurate.
