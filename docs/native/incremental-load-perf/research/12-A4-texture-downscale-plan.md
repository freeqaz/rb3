# A4 — Web Texture Downscale (mip-strip) Plan

**Status:** DRAFT — authored by Opus 2026-06-23 (Fable, the usual planner, was
unavailable). **Review before implementing.** This is the only remaining lever for
the 1.5 Mbps regime, and it also shrinks the 4/8 Mbps journey (milos are 75 MB of
the 115 MB cold wire). It is a content/asset change with cross-venue visual
impact, so it MUST ship behind a visual quality gate.

## Diagnosis (measured + code-grounded)

- The 4 Mbps cold journey is **75 MB milos / 115 MB total** (research/10). Bytes
  are the wall (throughput-bound; parallelism already maxed in Wave 4).
- **Textures live inside the milos**, not as separate files (separate texture
  files = 0.2 MB total). The journey venue `small_club_01.milo_xbox` = 18.5 MB
  raw / ~11.7 MB q11 wire — the W5 `maxSingleMilo`.
- Those textures are **BC/DXT-compressed** (`RndBitmap.mOrder & 0x38` =
  DXT, `src/system/rndobj/Bitmap.h:61`) — i.e. already near-incompressible, which
  is why brotli-q11 only gets milos to ~63% and the milo bytes stay large.
- **The textures carry mip chains.** `RndBitmap` holds an inline linked mip list
  (`mMip`, `src/system/rndobj/Bitmap.h:87`; `mNumMips`; `LoadHeader` reads the
  count, `Bitmap.cpp:16`). A full mip chain's TOP level is ~3/4 of the chain's
  bytes (1 + ¼ + 1/16 + … → top = ¾). **Dropping the top mip ≈ −75% of that
  texture's bytes for a half-resolution image.**
- **The engine already renders from lower mips.** `Tex.cpp:UseBottomMip()`
  (SystemConfig `rnd/use_bottom_mip`) + `CopyBottomMip()` walk the chain and
  `Create()` from a lower level — so reducing `mNumMips` on disk is a path the
  engine tolerates, not a new code path. This de-risks the whole approach.

## Chosen design: asset-side TOP-MIP STRIP, web-only

Produce a **downscaled COPY** of the milos that the web build serves, leaving the
canonical `orig-assets/extracted` tree (and therefore native/Wii and the matched
decomp build) **untouched**. For each eligible BC texture with `mNumMips > 1`,
drop the largest mip level (advance the chain head to `mMip`, decrement
`mNumMips`, halve `mWidth/mHeight`), re-serialize the milo, re-deflate the
container. q11-prewarm the stripped tree. Half-resolution venue/char textures;
near-lossless (the remaining mips are pre-authored, no recompression artifacts).

Why a strip and not a re-encode: the bytes we remove are raw BC blocks the
encoder authored; removing a whole mip level needs **no BC decode/encode**, has
**zero added artifacts**, and reuses the engine's existing lower-mip render path.

## Pipeline

1. **Decompress** the `.milo_xbox` container — reuse
   `../dc3-decomp/scripts/milo/inflate_milo.py` (the container is zlib-deflated;
   dc3 already has the inflater + `validate_milo_entries.py`).
2. **Parse** the object dir → find `Tex`/`RndBitmap` objects. The on-disk bitmap
   layout is the authority in `src/system/rndobj/Bitmap.cpp:LoadHeader` +
   `Bitmap.h` (`mBpp`, `mOrder`, `mWidth`, `mHeight`, `mMip`, mip count). Port
   that read/write to the tool (or drive it through a tiny C++ harness linked
   against the engine's RndBitmap serializer — preferred, since it can't drift
   from the real format).
3. **Strip** the top mip on eligible textures (see exclusion list), re-serialize,
   re-deflate, write to `orig-assets/web-downscaled/…` mirroring the request key.
4. **Serve**: `server.py` resolves `/api/file/...` from `web-downscaled/` first
   (when downscale is enabled), falling back to `extracted/`. New flag
   `RB3_WEB_DOWNSCALE` (default OFF until the quality gate passes, then default-ON
   for web). The q11 prewarm walks the downscaled tree.

## Tasks

**T0 — feasibility measurement (DECIDES go/no-go; do FIRST).** Build a milo/Tex
parser (port `Bitmap.cpp:LoadHeader`, or a C++ harness on the engine serializer).
Measure, for `small_club_01.milo_xbox` and one loaded char/outfit milo: total
bytes, texture-byte fraction, per-texture `mNumMips` distribution, and the
projected post-strip size. **Proceed only if texture bytes are a large fraction
AND most carry `mNumMips > 1`.** Owner files: a new `scripts/milo/mip_census.py`
(+ optional `native/tools/`). Output: a census table appended to this doc.

### T0 partial results (Opus, 2026-06-23 — entropy proxy, mip-chain check still open)

Measured on `small_club_01.milo_xbox` (the journey venue) via `inflate_milo.py`
(dc3) + a Shannon-entropy texture proxy + a direct brotli split. **Container is
UNCOMPRESSED (Version A): 18.5 MB raw = 18.5 MB inflated.** Object census: 129
Mesh, 78 Tex, 65 Mat, 41 Light.

| | raw | brotli-q11 wire | ratio |
|---|---|---|---|
| Texture (high-entropy / BC) | 5.2 MB (28%) | **4.9 MB (44% of wire)** | 0.94 (≈incompressible) |
| Geometry/other (low-entropy) | 13.4 MB (72%) | 6.3 MB | 0.47 (halves) |
| Full venue | 18.5 MB | 11.1 MB (≈ W5's 11.7) | — |

**Key correction to this plan's premise:** textures are a *raw-byte minority*
(28%) but a **WIRE plurality (44%)** — BC blocks barely brotli-compress while the
structured geometry halves. So the lever is real on the WIRE even though "75% of
bytes are texture" (my first draft) was wrong. **A top-mip strip removes exactly
the bytes brotli can't** — it stacks on Wave 5's q11, it doesn't overlap.

Projected (IF the textures carry strippable mip chains — see caveat):
- Venue `small_club_01`: wire **11.1 → ~7.5 MB (−33%)**.
- Journey milos 75 MB wire × ~44% texture, strip 75%: **save ~24 MB → journey
  ~115 → ~91 MB (−21%)**. Real yield lower after the exclusion list.

**Mip-chain strippability — CONFIRMED at the format level.**
`src/system/rndobj/Bitmap.cpp:LoadHeader` is explicit: *"Cached (Xbox/PS3
`.milo_xbox`) bitmaps serialize a full mip chain after the base level."* The
header layout is `rev, mBpp, mOrder, numMips(u8), mWidth, mHeight, mRowBytes,
pad` followed by the base level then the mip chain; `SaveHeader` re-serializes
`NumMips()` + the chain. So venue textures **do** carry strippable mip chains, the
count is a single header byte, and the engine's own serializer can round-trip a
stripped bitmap (no format drift) — which is why T1 should be a C++ tool linked
against the engine `RndBitmap`, not a hand-rolled Python parser. The native loader
already consumes cached mip bytes (`mNativeCachedMips`), and `CopyBottomMip`
proves render-from-lower-mip works.

**Remaining caveats (don't block GO, refine the yield):** (1) per-texture
`numMips` distribution not yet enumerated — a quick pass over the Tex headers
gives the exact strippable fraction (some textures may be `numMips==1`). (2)
char/outfit milos are SMALL (<1 MB) and geometry-heavy (~10–17% texture) →
**venues are the target, chars are not.** (3) the ~24 MB journey figure is an
upper bound until the per-texture count + exclusion list land.

**VERDICT: GO.** Texture is 44% of milo wire, mip chains are confirmed present and
strippable via the engine serializer, the engine renders from lower mips, and the
saved bytes are the brotli-incompressible ones (stacks on Wave 5). Net projected
~21% journey wire reduction, ~33% on the heavy venue — the first real lever for
the 1.5 Mbps regime. Ready for an implementation wave (T1 ‖ T2 behind the visual
gate); a Fable plan-review pass is welcome but not required to start T0's parser.

### T0 FINAL — real per-texture census + measured strip (Opus, 2026-06-23)

Tool: `scripts/milo/mip_strip.py` (`census` / `strip` / `brotli` subcommands;
python byte-surgery — the engine has NO ObjectDir save path, `DirLoader::SaveObjects`
is a `MILO_ASSERT(0)` stub, so a C++ re-serialize tool isn't viable). The on-disk
cached-RndBitmap layout was decoded from `src/system/rndobj/Bitmap.cpp`
LoadHeader/SaveHeader + `FileLoader::LoadStream` (the cached bitmap is embedded
INLINE in the milo stream after the RndTex tail fields, NO length prefix,
big-endian / Xbox360) and **validated against the 0xADDEADDE object separator** —
every accepted bitmap chain lands byte-exactly on the next object marker, so the
scan has zero false positives.

The earlier entropy proxy (28% raw / 44% wire texture) is SUPERSEDED by the real
per-texture count below. The strip = promote mip[1] to base (drop the top level:
width/2, height/2, rowBytes/2, numMips-1) — removes the top mip's pixel bytes,
which is exactly 75% of each chain's bytes.

| venue | bitmaps | strippable (numMips≥1) | texture bytes | strippable top-mip | frac | numMips dist (additional) | q11 ORIG → STRIPPED | wire saving |
|---|---|---|---|---|---|---|---|---|
| small_club_01 (journey) | 77 | 76 (98.7%) | 14.27 MB | 10.70 MB | **75.0%** | {0:1,1:2,2:12,3:11,4:29,5:12,6:10} | 11.67 → 4.88 MB | **−6.79 MB (−58.2%)** |
| small_club_11 | 86 | 85 (98.8%) | 11.39 MB | 8.54 MB | **75.0%** | {0:1,1:4,2:12,3:12,4:42,5:8,6:7} | 10.09 → 4.64 MB | **−5.45 MB (−54.0%)** |
| big_club_06 | 106 | 104 (98.1%) | 4.24 MB | 3.18 MB | **75.1%** | {0:2,1:6,2:21,3:51,4:21,5:4,6:1} | 7.14 → 5.62 MB | −1.52 MB (−21.3%) |
| arena_01 | 108 | 106 (98.1%) | 7.83 MB | 5.87 MB | **75.1%** | {0:2,1:3,2:24,3:40,4:23,5:13,6:3} | 8.49 → 5.70 MB | −2.79 MB (−32.9%) |

All bitmaps are DXT/BC. **Strippable fraction is uniformly ~75% of venue texture
bytes** (only 1–2 numMips==0 bitmaps per venue). The measured q11 wire saving
(−54…58% on the journey small_club venues) **far exceeds** the plan's −33% entropy
estimate — dropping the top mip both removes the incompressible BC bytes AND lets
the smaller surviving milo brotli-compress better; the gains stack on Wave 5's q11.
Texture-light venues (big_club/arena) save less (−21…33%) but never regress.

**Round-trip VALIDATED two ways:** (1) dc3 `validate_milo_entries.py` parses every
stripped milo OK (rev=28 WorldDir, entry table byte-identical to the original);
(2) a **headless native boot through the REAL engine** (`RB3_BOOT=1 RB3_LIVE_LOAD=1
rb3-native <stripped>`, `DirLoader::LoadObjects` — the same loader + `RndBitmap::Load`
mip-consume path the web build uses) loads the stripped venue with NO assert / NO
stream desync, producing output BYTE-IDENTICAL to loading the original (same root
`small_club_01 [RndDir]`, same notices). The tool's `strip` also re-parses its own
output and fails loudly if any surviving bitmap no longer lands on 0xADDEADDE.

**Journey projection (measured, not entropy):** the single heaviest milo
(small_club_01) saves −6.79 MB alone — nearly 2× the −3.6 MB/venue the plan
assumed — so the plan's journey 115 → ~91 MB (−21%) is a conservative FLOOR;
stripping every venue-class milo served by the journey yields strictly more.

**GO confirmed.** ≥75% of venue texture bytes strippable on every venue, the strip
round-trips cleanly through the real engine, and the measured wire saving beats the
estimate. Proceed to T1 (downscaled tree generator + `server.py` resolve order +
`RB3_WEB_DOWNSCALE`) and T2 (visual gate + exclusion list). The generated
`web-downscaled/` tree is a build/deploy artifact — gitignore it, do NOT commit.

**T1 — strip tool + downscaled tree + server wiring.** The strip pass
(`scripts/milo/strip_top_mip.py` or a C++ tool), the `web-downscaled/` generator
(parallel, idempotent, like `prewarm_encode_cache.py`), and `server.py` resolve
order + `RB3_WEB_DOWNSCALE` flag. The C++/engine side: confirm the upload path
accepts a reduced-mip RndBitmap without assert (CopyBottomMip/UseBottomMip suggest
yes — VERIFY). Owner files: `scripts/milo/*`, `native/web/server.py`, possibly a
`native/tools/` C++ harness. NO matched-Wii-TU edits; any engine guard is
`__EMSCRIPTEN__`.

### T1 DONE — generator + server wiring + measurements (Opus, 2026-06-23)

**Files (all on branch `xenon-round3-recon`):**
- `scripts/web/gen_web_downscaled.py` — the generator. Walks
  `extracted/world/venue/**/*.milo_xbox` (52 milos; VENUES ONLY — chars/UI are
  small + geometry-heavy, excluded), strips each via `mip_strip.strip_file` into a
  sibling `orig-assets/web-downscaled/` tree mirroring the request key. Parallel
  (`--jobs`, multiprocessing), idempotent (a `.src` size+mtime sidecar → a 2nd run
  is a 0.0 s no-op), atomic (unique temp → validate → rename). HARD-STOPS (nonzero
  exit) on any milo that fails to round-trip. `--validate` also runs dc3
  `validate_milo_entries.py` per output; `--stats` prints the q11 wire summary.
- `native/web/server.py` — `RB3_WEB_DOWNSCALE` flag (`--downscale`/`--no-downscale`
  CLI override; env default OFF), `DOWNSCALE_DIR` (auto-detect
  `orig-assets/web-downscaled`, `RB3_WEB_DOWNSCALE_DIR` override). New module-level
  `downscale_path(rel)` probed FIRST in both `resolve_asset_path` (the shared
  source-of-truth used by the bundle routes + prewarm) and `_serve_asset_file`
  (the `/api/file` handler) — a stripped venue milo shadows the extracted original
  when the flag is on, else a no-op. `_spawn_prewarm` prepends the downscaled tree
  as the first prewarm root when on, so the warmed q11 wire is the stripped size.
- `scripts/web/prewarm_encode_cache.py` — mirrors the flag/dir resolution so a
  standalone prewarm (`--downscale`) walks the stripped tree first (first-root-wins
  de-dupe → same cache key, smaller bytes).
- `.gitignore` — explicit `orig-assets/web-downscaled/` entry (already covered by
  `orig-assets/`; documented as generated build output, NOT committed).

**Serve mechanism (exact):** with the flag on, `/api/file/world/venue/.../foo.milo_xbox`
resolves from `web-downscaled/` (a direct join on the request key) before
`extracted/`. The existing R5 wire-compression path is unchanged — it compresses
the (now smaller) stripped milo and serves it via `Content-Encoding: br`. The
prewarm warms `web-downscaled/world/venue/.../foo.milo_xbox.br` under the SAME
cache key the server looks up, so a warmed journey serves the stripped+q11 artifact
straight from disk. Default OFF until T2's visual gate flips it on
(`RB3_WEB_DOWNSCALE=1`).

**Engine: NO fix needed.** A headless boot through the REAL engine
(`RB3_BOOT=1 rb3-native <stripped>`, full SystemInit config + `DirLoader::LoadObjects`)
loads the stripped small_club_01, arena_01, AND big_club_06 with NO assert / NO
desync — byte-identical behavior to the originals (same root `[RndDir]`, same
notices). The reduced-mip RndBitmap upload is tolerated as the plan predicted
(CopyBottomMip path). No engine change, no matched-TU edit.

**Measured (verified end-to-end through the server):**
- Generation: 52/52 venues stripped + dc3-validated in 30 s; 2nd run a no-op.
- Journey venue small_club_01 q11 wire **11.67 → 4.88 MB (−58.2%, −6.79 MB)** —
  this is the W5 `maxSingleMilo` (11.7 MB → 4.88 MB). Confirmed end-to-end: a
  `--downscale` server with the prewarmed q11 cache serves
  `Content-Encoding: br`, `Content-Length 4,877,312`, which decompresses to the
  byte-exact 8,731,568-byte stripped milo.
- Full 52-venue corpus q11 wire: **610.44 → 513.85 MB (−15.8%, saved 96.59 MB)**.
- Other journey-class venues: small_club_03 −54.0%, small_club_06 −51.4%,
  arena_01 −32.8%, big_club_06 −21.3%. **Caveat:** the `video/*` venues (40–50 MB
  each) are video-backdrop scenes whose bulk is NOT BC texture — they strip ~0%
  (video_03 −0.5%, video_07 −0.2%); the strip never regresses them. The
  texture-heavy small_club / arena / big_club venues an actual song journey uses
  all save 21–58%. So the projected journey 115 → ~91 MB (−21%) holds for a
  small_club/arena journey; a video-venue journey saves less (the lever is
  texture-bound, as designed).

NOTE: the full 4 Mbps Playwright `_netmatrix` journey was started but the cold
release boot under 4 Mbps throttle is slow enough that it had not reached the
gameplay venue-load phase within the time budget; the venue wire delta above is
deterministic (proven per-asset through the actual server resolve + R5 encode
path), so the journey total follows directly. T2 should run the full netmatrix A/B
once the visual gate is being measured anyway.

**T2 — visual quality gate + exclusion list + byte re-measure.** Render gameplay
+ each venue at the camera distances they actually use (the venue cam, not a
close-up), full-res vs downscaled, and gate on SSIM / `visual_diff.py` (the
render-change gate used by the mesh-cache work). Build the **exclusion list**:
UI/HUD textures, album art, fonts, and any texture that visibly degrades stay
full-res. Re-measure the 4 Mbps + 1.5 Mbps journeys: target venue milo ~18.5 →
~8–10 MB, journey wire 115 → ~85–95 MB, and a 1.5 Mbps journey that now reaches
game_screen. Owner files: `scripts/web/` harnesses, this doc.

## Rejected alternatives

- **Lossy BC re-encode at lower quality** — needs a BC encoder, is slow, and adds
  compression artifacts on top of the original BC. Mip-strip removes whole
  authored levels with zero re-encode. Rejected.
- **`UseBottomMip` SystemConfig flag** — uses the *smallest* mip (e.g. 4×4),
  unusable for venue backdrops. The strip drops only the TOP level (half-res).
- **Per-file brotli (already shipped, Wave 5)** — can't shrink raw BC blocks;
  orthogonal, keep it (it still helps the non-texture milo bytes).
- **Downscaling at ARK-extraction time** — would corrupt the native/Wii asset
  source and the decomp. Must be a web-only *copy*. Rejected.

## Risks / invariants

- **Web-only, never the canonical tree.** Native + Wii + the matched build read
  `extracted/` and stay byte-identical. The downscale is a served copy.
- **Mip-less textures** (`mNumMips ≤ 1`) can't be stripped — skip them (T0 says
  how many).
- **Container round-trip must be byte-exact** except the removed mip data — a
  milo the engine rejects is worse than a big one. Validate every output milo
  loads (reuse dc3 `validate_milo_entries.py` + a headless boot per venue).
- **Engine upload of a reduced-mip RndBitmap must not assert** — likely fine
  (CopyBottomMip path), but T1 must prove it on a real downscaled venue.
- Default OFF until T2's quality gate passes; then default-ON for web with
  `RB3_WEB_DOWNSCALE=0` opt-out.

## Suggested execution

This is a clean ultracode wave: T0 (Fable or Opus measurement) gates T1 (Opus
impl) ‖ T2 (Opus visual gate), single integrator runs the byte re-measure + the
4/1.5 Mbps gates. Hold for a Fable plan-review pass if one is available; the
design above is Opus-drafted.

### T2 — VISUAL GATE: pass-with-exclusions, BUT T1's tree has a HARD CRASH BUG (Opus, 2026-06-23)

**Verdict: pass-with-exclusions on quality — gated behind a mandatory T1 fix.**
Evidence + tooling in `/tmp/rb3perf-A4-gate/` (GATE_SUMMARY.json, tex_quality.py,
texq_*.json, run1/*.png, texq2/*.png).

**BLOCKER (must fix before RB3_WEB_DOWNSCALE can ship even default-OFF-then-ON):**
The T1-generated tree CRASHES gameplay. Loading the stripped `small_club_01`
through the in-game venue loader aborts at `ChunkStream.cpp:458`
(`mCurBufOffset + bytes <= (*mCurChunk & kChunkSizeMask)`). Root cause: a venue
`.milo_xbox` is a **ChunkStream** container (magic `0xCABEDEAF`, then
`offset/num_blocks/max_block` + a per-block size table). The reader requires every
object `Read()` to fit inside one block. `mip_strip.write_container` re-chunks the
(now smaller) payload into fresh `max_block`-sized blocks — boundaries that no
longer align to object reads → desync → abort. T1's "no engine fix, validated"
MISSED this because dc3 `validate_milo_entries.py` and the `RB3_BOOT` /
`DirLoader::LoadObjects` path both read the payload as a plain concat and never
exercise ChunkStream; the GAMEPLAY venue load does.
**Fix (verified, trivial, in the tool — NOT the engine):** emit the stripped milo
as ONE block (`num_blocks=1`, `max_block=len(payload)`). With a single chunk no
read can cross a boundary, so the assert is unreachable for ANY venue (universal
by construction). Verified end-to-end: single-block stripped `small_club_01` loads
+ renders through the real gameplay ChunkStream path, 4 frames captured at
songMs>0, no assert. Wire cost of single-block vs multi-block is ±1 KB on a
4.6–5.7 MB brotli wire (negligible — the mip-strip savings are fully preserved:
q11 small_club_01 4,877,312 → 4,876,537). `gen_web_downscaled` / `mip_strip` MUST
adopt single-block output before the tree is served.

**Quality method (the gameplay-screenshot A/B is IMPOSSIBLE here — documented):**
Two identical full-res boots at the same songMs differ by **70.9%** of pixels
(frame counts 3873 vs 3015 at songMs=10000) — the engine is NOT frame-deterministic
across boots and the venue crowd/lights/characters animate off the free-running
FRAME counter, not song position. So matched-songMs (and even director-frozen
burst cross-match, best-pair SSIM ~0.45) gameplay diffs are ALL animation desync,
not texture — exactly the camera-desync false-positive the plan warned about.
Instead the gate is a **deterministic, animation-free per-texture measurement**:
decode each venue BC texture's mip[0] (full-res) vs mip[1] bilinearly upscaled 2×
(what the stripped base shows when the camera is close enough to sample the top
mip) and SSIM/PSNR them. This is the WORST CASE — at gameplay distance the GPU
already samples a lower mip, so on-screen impact ≤ this. (Decode = Milo DXT, k8in16
byte-swap only per engine `TextureConvert::UntileMilo`/`Rnd_Wgpu_RB3::ByteSwapDXT16`;
formats per `TextureConvert.h`: 0x08=BC1, 0x10=BC2, 0x18=BC3, **0x20=BC5/normal**.)

**Measured (4 venues, 371 textures):** SSIM median 0.875 / mean 0.852 / min 0.196;
median PSNR ~30 dB. Per-venue: small_club_01 med 0.81, small_club_11 0.87,
arena_01 0.90, big_club_06 0.90. Only **7 textures (1.9%) below SSIM 0.50**, all
small (≤256²) high-frequency fine-detail surfaces (amp/speaker cloth + "Fender"
logo, perforated grilles, rust strips, noise/grain, normal maps). Visual review of
every worst case: the half-res version is SLIGHTLY SOFTER on fine weave/grille but
the surface + any baked signage/logo stays clearly recognizable/legible — SSIM
over-penalizes high-frequency content; it is NOT perceptually broken. No
UI/HUD/album-art/font textures live in venue milos (the generator is venues-only),
so those are unaffected by scope.

**Exclusion list (stay full-res):** (1) always — UI/HUD, album art, fonts, and any
non-venue milo (already excluded by the venues-only generator scope; keep it).
(2) within venues, the perceptually-worst classes to keep full-res or strip
conservatively: BC5/DXN **normal maps** (`mOrder & 0x38 == 0x20` — stripping a
normal map's top mip softens lighting/specular detail and risks shimmer);
**small textures** (`max(w,h) <= 128`, and ideally `<= 256`) — the top-mip strip
saves few bytes there (the byte win is in the 512²–1024² surfaces) while taking the
largest SSIM hit; **BC3-alpha (`==0x18`) detail/noise textures**. Filtering these
keeps the big-win large surface textures (the bulk of the −54…58% small_club wire
saving) while removing the 1.9% worst tail. Generator already takes an `exclude(bm)`
predicate — wire a size+format filter into it.

**Bottom line:** quality is acceptable (pass-with-exclusions) for default-ON web,
PROVIDED (a) the single-block ChunkStream fix lands — non-negotiable, it's a crash —
and (b) the normal-map + small-texture exclusions are applied. Without (a) the tree
is unservable; ship default-OFF until both land + a real gameplay smoke (a song that
reaches a small_club/arena venue without abort) passes.

### A4 FIX WAVE — 3 blocking issues fixed in `mip_strip.py`, tree regenerated (Opus, 2026-06-23)

All fixes are in `scripts/milo/mip_strip.py` (the offline strip tool) — NO engine
change, NO matched-TU edit, the canonical `extracted/` tree untouched. Default
`RB3_WEB_DOWNSCALE` stays OFF.

1. **CubeTex mixed-face corruption (BLOCKER) — FIXED.** Confirmed real on the
   committed tree: 30/52 venues had a malformed cube map (one face half-res, five
   full-res) — `[(256,256)×5,(128,128)]` etc. Root cause: an `RndCubeTex`
   serializes its 6 face bitmaps back-to-back from the shared milo stream
   (`CubeTex.cpp:147-149` PostLoad), only the 6th lands on the 0xADDEADDE
   separator, so the old `find_bitmaps` (ADDE-anchored per bitmap) saw ONLY the 6th
   face and stripped it. The engine reaction is `ValidateBitmapProperties` fail →
   `Reset()` (cube vanishes) + an invalid WebGPU cube upload (faces must be equal
   dim). FIX: new `find_bitmap_runs` greedily groups consecutive bitmaps into
   ADDE-terminated runs; a run with len>1 is a cube → its members are NEVER
   stripped. The strip self-check now also asserts every surviving cube run has
   uniform face dims. Verified: regenerated tree has 0/52 mixed-face cubes
   (down from 30/52).
2. **Single-block ChunkStream (T2 crash prerequisite) — FIXED.** The committed
   tool's `write_container` still re-chunked into `max_block`-sized blocks → the
   ChunkStream reader (`ChunkStream.cpp:168` ReadImpl assert) desyncs mid-object on
   the gameplay venue load. FIX: `write_container` now emits ONE block
   (`num_blocks=1`, `max_block=len(payload)`) — the assert is unreachable by
   construction. Verified: all 52 regenerated venues are single-block.
3. **Visual-gate exclusion list — APPLIED.** New `default_exclude(bm)` (on by
   default in `strip_file`, `--no-exclude` to bypass) keeps full-res: BC5/DXN
   normal maps (`mOrder&0x38==0x20`), BC3-alpha detail/noise (`==0x18`), and small
   textures (`max(w,h)<=256`). UI/HUD/album-art/fonts are already out of scope
   (venues-only generator).

**Re-validation:** all 52 venues regenerate (`--force`) + round-trip in 24 s,
failed=0; dc3 `validate_milo_entries` OK on the cube venues (small_club_01,
big_club_06, arena_04, video_07); `RB3_BOOT` on stripped small_club_01 is
byte-identical to the original (`root 'small_club_01' [RndDir]`). big_club_06
under `RB3_LIVE_LOAD` aborts identically with/without strip on a pre-existing
native-harness gap (`Couldn't find 'synth' in array`) — not strip-related.

**Post-exclusion wire (measured, brotli-q11):**
- small_club_01 (journey heaviest): **11.67 → 8.68 MB (−25.7%, −2.99 MB)** — vs
  −58.2% without exclusions; the small/normal/BC3 exclusions cost ~3.8 MB of the
  raw win but protect the perceptually-fragile tail.
- Journey-class 5-venue subset (sc01/sc03/sc11/arena_01/big_club_06): 46.79 →
  37.52 MB (−19.8%).
- Per-venue: small_club_11 −28.1%, small_club_03 −18.4%, arena_01 −14.5%,
  big_club_06 −6.6% (texture-light).

The `<=256` small-texture threshold is the conservative T2 recommendation
(`<=128` is the floor); it roughly halves the byte win vs no-exclusion but removes
the entire SSIM<0.5 tail. Tunable in `mip_strip.SMALL_MAX_DIM` if a future gate
wants the larger win at `<=128`.

### A4 INTEGRATION — SHIPPED default-ON (Opus integrator, 2026-06-23)

Single-integrator pass over the landed census + T1 + fix work. Branch
`xenon-round3-recon`; engine pin **unchanged** (`20dba552` — A4 is offline tool +
python server only, NO engine/matched-TU edit, NO pin bump).

**Flag flipped default-ON for web.** `native/web/server.py
_downscale_enabled_from_env()` now defaults TRUE when `RB3_WEB_DOWNSCALE` is unset
(opt-out `RB3_WEB_DOWNSCALE=0` / `--no-downscale`). The flag is server-side only
(the wasm just fetches `/api/file/...`; the downscale is a server resolve), so
"default-ON for web" = the server defaults the stripped-tree shadow on. The
prewarm mirrors the same default (walks the downscaled tree first).

**Gates (all PASS) — measured cold-IDB netmatrix, release, own server.**
| gate | verdict | evidence |
|---|---|---|
| 6a 4 Mbps/150ms A/B | PASS | OFF 118.50 MB → ON 113.85 MB (−4.65 MB); both game_screen; over250=0 |
| 6b **1.5 Mbps/300ms** | **PASS (A4's reason)** | OFF DNF (stuck part_difficulty); **ON reaches game_screen @ 574.5 s** |
| 6c 20 + 8 Mbps backstop | PASS | both game_screen, freeze-free, no regression |
| 6d Wave-6 wins | PASS | 0 ChunkStream/assert across 4 ON runs; reveal+L1 A4-orthogonal |
| 6e flag A/B | PASS | OFF→full-res 19,434,928 B; ON→stripped 14,060,976 B |
| Wii | PASS | 0 matched-TU edits (python/docs/.gitignore); byte-identical |
| native | PASS | rb3-tests 21/21; song-end→game-over; canonical tree untouched |

**Journey venue served wire (A4-ON, q11): 11.67 → 8.68 MB (−25.7%)** with the
exclusion list. The 1.5 Mbps OFF-DNF / ON-reaches-game_screen split is the headline:
A4 makes the lowest-bandwidth regime complete to gameplay for the first time.

**Ship: default-ON.** Generated tree gitignored (NOT committed) — deploy step is
`gen_web_downscaled.py` + `prewarm_encode_cache.py --downscale`.
