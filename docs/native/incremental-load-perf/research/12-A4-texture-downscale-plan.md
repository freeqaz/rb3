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

**T1 — strip tool + downscaled tree + server wiring.** The strip pass
(`scripts/milo/strip_top_mip.py` or a C++ tool), the `web-downscaled/` generator
(parallel, idempotent, like `prewarm_encode_cache.py`), and `server.py` resolve
order + `RB3_WEB_DOWNSCALE` flag. The C++/engine side: confirm the upload path
accepts a reduced-mip RndBitmap without assert (CopyBottomMip/UseBottomMip suggest
yes — VERIFY). Owner files: `scripts/milo/*`, `native/web/server.py`, possibly a
`native/tools/` C++ harness. NO matched-Wii-TU edits; any engine guard is
`__EMSCRIPTEN__`.

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
