# A4-progressive — In-session Texture Sharpen Plan (option C)

**Status:** plan authored by Opus 2026-06-30 (Fable unavailable). Engine make-or-break
already investigated + de-risked (see §Seams). This is the follow-on to A4: instead of
serving half-res venue textures to ALL web sessions forever, load the stripped (fast)
version to reach gameplay, then **background-fetch the full-res top-mips and sharpen
each texture live in the same session.** Fast time-to-gameplay (A4's wire-byte win, incl.
the 1.5 Mbps→gameplay flip) is preserved; visual quality is restored progressively.

## Why this shape (investigated seams, ../milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp)

- **Backend is SINGLE-MIP.** Every `CreateTexture` uses `mipLevelCount = 1` (line 705
  et al.) because RB3 native bitmaps load with `numMips==0` (cached mip bytes are
  consumed-and-discarded via `mNativeCachedMips`, Bitmap.cpp). So the GPU texture is one
  level at the bitmap's base resolution. ⇒ **mip-residency / lod-bias streaming is NOT
  available; the upgrade path is recreate-at-full-res.**
- **A downscaled texture ⇒ a half-res single-mip GPU texture** (A4 made the on-disk base
  level the old mip[1]). To sharpen, give the RndBitmap the full-res base level again and
  let the GPU texture be recreated at full size.
- **Churn detection already exists.** `struct RB3TexEntry { wgpu::Texture tex; TextureView
  view; const uint8_t* lastPixels; uint32_t fingerprint; bool uploaded; }` (line 345) and
  `UploadRndTexIfNeeded` (line 633) — the comment at `lastPixels` is literally "detect
  bitmap data churn." So **swapping a bitmap's pixel data + bumping its fingerprint makes
  the existing upload path recreate the texture at the new size and produce a new view** —
  we reuse this, we don't invent streaming.
- **Material bind groups are slot-cached** (`slot.matBG` rebuilt only when
  `slot.matKey != mat || …`, line 5778; `MakeMaterialBindGroupCached` binds the cached
  diffuse/emissive views). ⇒ the ONE real engine addition: when a texture's view changes,
  the cached `matBG`s that bound the old view must rebuild — add a texture-view generation
  to the slot's rebuild key (or bump a global tex-generation the slot checks).

## Design

1. **Sidecar (asset/tool side).** The strip tool (`scripts/milo/mip_strip.py`) already
   computes the removed top-mip bytes (`strip_bitmap_bytes` → `removed`) but discards
   them. Emit them instead: per downscaled venue, a `<venue>.milo_xbox.sharpen` sidecar
   listing, per stripped texture: a stable identity (RndTex object name + index within the
   venue dir), the original full `width/height/rowBytes/bpp/order`, and the top-mip BC
   bytes. The sidecar IS the ~75% of texture bytes A4 stripped — it's the high-res delta.
2. **Serve (server side).** server.py serves the sidecar from the downscaled tree (no new
   logic; it's a file). Wire-compressible like any asset.
3. **Engine sharpen (the work).** When a downscaled venue has loaded and gameplay is
   running (songMs>0 / not on the critical path), an HX_NATIVE sharpen manager:
   a. **Async, low-priority fetch** of the venue's `.sharpen` sidecar via the existing
      WebAssets async path (`WebAssetsEnsureResidentAsync`). MUST be low priority so it
      does not starve the mogg Range streaming on a slow link — sharpen is cosmetic and
      can take minutes; audio cannot.
   b. **Match** sidecar entries to the loaded RndTex objects by name (walk the venue
      ObjectDir's RndTex list).
   c. **Swap** each matched bitmap to full-res: reconstruct the full base level (top-mip
      bytes at original W/H), update the RndBitmap (mWidth/mHeight/mRowBytes/data), bump
      the texture's churn fingerprint.
   d. The existing `UploadRndTexIfNeeded` churn path recreates the GPU texture at full
      size on the next draw → new view in `sTexGpu`.
   e. **Invalidate** the cached `matBG`s that bound the old view (the engine addition).
   f. **Incremental scheduler:** N textures/frame (env-tunable, default small) so the
      recreate+upload bursts don't hitch gameplay — the SAME frame-budget discipline as
      A4/L1. The per-texture upload is the cost A4 deferred; spreading it over steady
      gameplay frames is the whole point.
4. **Flag** `RB3_PROGRESSIVE_SHARPEN` (default ON for web once the gate passes); opt-out
   restores the A4 stripped-stays-stripped behavior.

## Tasks

**T0 — sidecar emission + churn-recreate verification (GATES the engine work).** Extend
`mip_strip.py` to emit `.sharpen` sidecars (it already has `removed`); regenerate for the
journey venues. THEN verify the engine assumption directly: in a native test, load a
downscaled venue, swap one RndTex's bitmap to a different-size image + bump its
fingerprint, and confirm `UploadRndTexIfNeeded` **recreates the GPU texture at the new
size and the new view binds** (the recreate-at-new-size assumption is the whole design —
prove it before building the manager). Owner: `scripts/milo/mip_strip.py`, a native test.
Gate: go iff the churn path recreates cleanly at a new size.

**T1 — engine sharpen manager.** The async low-priority sidecar fetch, name-match,
bitmap-swap + fingerprint bump, matBG view-generation invalidation, incremental
per-frame scheduler, `RB3_PROGRESSIVE_SHARPEN` flag. Owner: engine
`src/platform/Rnd_Wgpu_RB3.cpp` (+ a new sharpen TU), RndTex/RndBitmap swap helper
(HX_NATIVE-guarded if in a matched TU — prefer a native-only engine file), the web fetch
glue. `__EMSCRIPTEN__` for web-only arms, never HX_WEB. No matched-Wii-TU behavior change.

**T2 — visual + perf gate.** (1) **Final correctness:** after sharpen completes, the
rendered venue is pixel-identical (or SSIM≈1) to the full-res `extracted/` build — prove
the sharpen actually reaches full quality. (2) **No hitch:** frame-trace gameplay during
the sharpen window — no frame > budget attributable to the recreate/upload bursts (the
scheduler works). (3) **Audio not starved:** on a 4/1.5 Mbps throttle, the low-priority
sharpen fetch must not cause mogg underruns (audio_verify MATCH through the sharpen
window). Owner: scripts/native + scripts/web harnesses.

## Rejected alternatives

- **Mip-residency / lod-bias** — the backend is single-mip (mipLevelCount=1); there is no
  GPU mip chain to stream into. Rejected by the engine reality.
- **In-place GPU texture resize** — not a thing in wgpu; recreate is the only path.
- **Whole-milo re-fetch + reload-swap** — re-parsing+reloading a live venue mid-scene is
  far heavier and more disruptive than per-texture sharpen, and it re-downloads the
  geometry we already have. Rejected.
- **Server-side adaptive (option A/B)** — simpler, but the user chose the in-session
  result; A/B remain a fallback if C's perf gate fails.

## Risks / invariants

- **Sharpen fetch MUST be low-priority** — it competes with mogg streaming on slow links;
  it is cosmetic and may take minutes. Gate it behind "gameplay reached + link idle-ish."
- **VRAM returns to full-res baseline** after sharpen (the textures come back) — fine; the
  win was wire-bytes/time-to-gameplay, not VRAM. Memory peak == the pre-A4 full-res build.
- **matBG invalidation must be complete** — a stale bind group binding the freed old view
  is a use-after-free / wrong-texture bug. The view-generation key must cover every matBG
  that could bind the texture (the wave-2/A4 reviews caught exactly this lifetime class —
  adversarially verify).
- **Wii byte-identical** — all rb3-side changes inside `#ifdef HX_NATIVE`; the engine
  sharpen TU is native-only. No matched-TU behavior change.
- **A4's wire win preserved** — sharpen does not change time-to-gameplay (it runs after);
  the 1.5 Mbps→gameplay flip must still hold with sharpen ON.
- Engine change ⇒ MILO_ENGINE_PIN bump at integration (single bump).

## Suggested execution

Ultracode wave, all Opus: T0 (sidecar + churn-recreate gate) → T1 (engine manager) →
review ‖ T2 (visual+perf+audio gate) → conditional fix → integrate (pin bump, full build,
the flow gate: fast load → blurry → sharp, no-regression on A4/first-frame/L1/Wii).
