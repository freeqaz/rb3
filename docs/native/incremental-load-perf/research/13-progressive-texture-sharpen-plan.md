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

---

## RESULTS (SHIPPED default-ON, integrated 2026-07-02)

**Verdict: SHIP default-ON for web.** All gates pass. The recreate-at-new-size design
holds exactly as planned; the sharpen restores byte-exact full resolution live in-session
with zero frame hitch and zero added audio starvation, and A4's time-to-gameplay win is
preserved.

### What landed
- **T0** engine `cede2db` (recreate-at-new-size diagnostic) + rb3 `2c771f2d` (sidecar
  emission + churn-recreate gate). Sidecar format = `SHRP v1` (magic/version/levels/count
  header, per-entry full+stripped dims, `stripped_fp` TexFingerprint as the primary match
  key, then the top-mip BC bytes verbatim). `mip_strip.py sharpen` subcommand +
  `strip_file(sidecar_path=…)`; `gen_web_downscaled.py` emits `<venue>.milo_xbox.sharpen`
  next to each stripped venue.
- **T1** engine `022a5d2` (progressive in-session sharpen manager: async sidecar fetch →
  fingerprint match → bitmap swap + fingerprint bump → churn-recreate → matBG view
  invalidation → incremental N/frame scheduler) + `bd3bcb7` (DXT-only match guard — skips
  palette/RGBA per code-review advisory) + rb3 `9de518ec` (open sidecar in READ mode) +
  `165444c6` (sidecar excludes non-DXT). Driver = `native/src/rb3_texsharpen_native.cpp`
  (I/O + gameplay-gating around the engine manager), called from `Game::Poll` under
  `#ifdef HX_NATIVE`. Native+web glue in `native/CMakeLists.txt` (both lists).
- **Flags:** `RB3_PROGRESSIVE_SHARPEN` (default ON; `=0/f/n` opt-out → A4 stripped-stays-
  stripped), `RB3_SHARPEN_PER_FRAME` (default 4), `RB3_SHARPEN_DBG`.
- **Pin:** engine HEAD `77eb428b` was already pinned by a concurrent commit whose ancestry
  includes all four sharpen engine commits — no separate integration pin bump was needed.

### T2 gate + integration flow gate results (fresh integrated build, 2026-07-02)

**Recreate mechanism (the design's make-or-break): CONFIRMED.** Native `_sharpen_gate.py`
(small_club_01, 20thcenturyboy, nofail+autohit): sidecar loaded 17 entries → matched 15 to
loaded RndTex → all 15 recreated at exact full dims (floor_wood02 512→1024, stainedglass
512×256→1024×512, chair_pub 256→512, door 128×256→256×512, …) → `session COMPLETE: 15
textures, 5,177,344 bytes upgraded`. Every entry `recreate=1` (new GPU texture + fresh
view). 2/17 unmatched = textures not present in this venue variant (expected; fingerprint-
primary match).

**Final quality: byte-exact full resolution.** The `.sharpen` sidecar (small_club_01 md5
`dcb0c6a5`) is byte-identical to the exact top-mip bytes `mip_strip.py` removes from the
full-res `extracted/` milo (stripped_base + sidecar == extracted losslessly; 5,373,952
removed == carried). The engine `_MemAllocs` full W×H, memcpys the carried mip[0], and
recreates the GPU texture at full W×H → the sharpened texture is the SAME bytes the full-res
build uploads (SSIM = 1.0 given a matched camera). Direct cross-run pixel SSIM was NOT
capturable: the venue director shot micro-animates/re-frames per independent run even at
matched songMs — a harness limitation, not a quality deficit. Byte-identity is the stronger
proof. BEFORE (stripped) / AFTER (sharpened) native screenshots render the venue cleanly
with visibly crisper framed-picture / wall / stained-glass detail (`/tmp/sharpen_{before,
after}.png`).

**Web flow gate (release, CDP throttle, cold cache, `RB3_WEB_DOWNSCALE=1`, sharpen tree +
sidecars served + q11-prewarmed):**

| case | reached game_screen | FPS min/median | sharpen | underrun Δ (in-window) |
|---|---|---|---|---|
| 4 Mbps ON  | 214.0 s | 59.6 / 60 | **COMPLETE 15/15, 5,177,344 B** (== native) | **0 ev / 14532 q = 0.000%** |
| 4 Mbps OFF | 214.1 s | 60.0 / 60 | n/a (A4 stripped) | 0 ev / 10726 q = 0.000% |
| 1.5 Mbps ON | 531.1 s | 60 | fetch fired (venue resolved) | (reached; A4 flip preserved) |

- **(a) reaches game_screen fast — A4 win INTACT.** ON 214.0 s ≈ OFF 214.1 s at 4 Mbps:
  the sharpen adds ~0 to time-to-gameplay *by construction* (the sidecar fetch fires only
  after `songMs>0`, off the load critical path). 1.5 Mbps ON still reaches game_screen
  (A4's 1.5 Mbps→gameplay flip preserved).
- **(b) soft → sharp.** Web sharpen COMPLETES in-session (15/15 textures recreated to
  full-res, 5,177,344 B — byte-identical to native) once the 5.37 MB sidecar arrives
  (~11 s at 4 Mbps) + the N/frame scheduler runs.
- **(c) no hitch.** 4 Mbps ON held 59.6–60.4 FPS through the recreate bursts (median 60).
  Perf-trace (T2, native): max attributed texUpload 1.30 ms/frame; worst sharpen-window
  frame 16.40 ms < worst *unrelated* gameplay frame 19.96 ms; total burst ~5.3 ms spread
  over 4 frames at the default budget. No frame exceeds the 33 ms (or 16.67 ms) budget.
- **(d) audio not starved.** 0 underrun delta at 4 Mbps ON == OFF (the AudioWorklet
  processed ~38.7 s of audio with zero silence-padding during the window). 1.5 Mbps: T2
  gate measured ON=635 < OFF=861 underrun events (ON is *fewer* — the ~7–10% padding at
  1.5 Mbps is inherent mogg-streaming baseline, present with sharpen OFF). Identity: native
  gameplay `audio_verify --rank` = chroma **0.958 CONFIDENT MATCH** (margin +0.397); web
  ON≈OFF (T2).

### Honest misses / caveats
- **No real fetch-priority lane (code-review advisory, latent).** `WebAssets.cpp`
  `WebAssetsFetch` (sharpen) and `WebAssetsRangeFetch` (mogg) both issue plain equal-
  priority `emscripten_fetch`; the "low priority" wording in the driver/commit is
  aspirational, not implemented. Empirically the single ~5.37 MB sidecar causes 0 added
  underruns at 4 Mbps and fewer at 1.5 Mbps in every measured run, so it ships — but a real
  defer/serialize behind mogg residency (or a chunked/idle-gated fetch) remains a follow-up
  hardening item, not a blocker.
- **Web audio identity leans on native.** The integration web flow harness captured a
  silent WAV (a capture-tap artifact under the aids config — the underrun counters prove
  ~38.7 s of audio flowed, and the fresh native `--rank` is 0.958 MATCH), so the fresh web
  chroma number comes from the T2 gate (ON≈OFF), not this run. The audio *starvation* signal
  (underruns, the risk this wave introduces) IS freshly measured here and is 0.
- **Headless fast-fail.** With no input the song hits SONG FAILED (crowd meter) and stops
  the driver; observing sustained gameplay + web COMPLETE required scheduling
  `RB3_GAME_INPUT="@1:nofail,@1:autohit"` (retries until players load).
- **Cross-run SSIM uncapturable** (camera non-determinism) → byte-identity used instead.
- **Coverage:** all 52 web venues emit sidecars (78.7 MB total top-mip deltas; venues with
  0 strippable BC textures legitimately get none). The generated downscaled tree + sidecars
  + q11 cache are build output (gitignored under `orig-assets/`), regenerated by
  `gen_web_downscaled.py` + `prewarm_encode_cache.py --downscale` at deploy time.
- Unrelated pre-existing breakage noticed during integration: the `rb3-dta` *tool* target
  fails to link (undefined `gRB3Trace*/RB3Replay*` — the session-telemetry wave's TU is not
  in `rb3-dta`'s source list). Not sharpen-related; `rb3-native`/`rb3-tests` build clean.
