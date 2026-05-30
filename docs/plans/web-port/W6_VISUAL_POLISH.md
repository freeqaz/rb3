# W6 — Visual polish backlog

**Parent plan:** [`PLAN.md`](PLAN.md). Read it for context.
**Depends on:** [`W5_TEXT_RENDERING.md`](W5_TEXT_RENDERING.md) (Phase 1+2 landed; Phase 3 dimness is being investigated in parallel).
**Blocks:** ship-quality web build.

**Audit base — what we looked at:**

- Current state: `docs/sessions/web/screenshots/w5-text-fix/01..07_*.png` (engine `8397fa6`, rb3 master at audit time `9e56d3e1`; capture commit recorded as `6cfb0a7d`).
- Pre-W5 baseline: `docs/sessions/web/screenshots/baseline-2026-05-29/01..07_*.png`.
- Ground-truth: `images/retail-screenshots/` (Wii primary, 360/PS3 backup) — note this dir was moved out of `orig-assets/native-refs/rb3-ui/` in commit `49b3a084`.

---

## Top-line state

W5 text fix is a real and durable improvement: the splash "PRESS START" is a saturated yellow now (vs olive-grey baseline), the in-game LOADING text is crisp white (vs grey baseline), and `02/03/04/07` each pick up the labels the W5 README claims (news ticker, FRIEND RANKINGS, RIGHTY MODE, COOL panel). **The geometry layer is healthy** — 3D scenes (hub, venue, gem highway, intro cinematic) and the venue art in `04_part_difficulty` all render at the right framing, lighting roughly resembles the Wii capture, no obvious z-fighting or missing meshes. **The unfixed front is the UI layer:** song-row titles are not just dim (the known Phase 3 issue) — they are *entirely absent* in `03_song_select` so the user has nothing to read or pick from; the gameplay HUD is missing every numeric (score, streak, multiplier, song-progress); album art is the `?` placeholder; the splash hub is missing the whole PLAY NOW / CAREER / TRAINING menu list; the intro cinematic plays against a pure-black background instead of the venue, which makes the silhouette of two microphones look like a bug even though it's the correct camera shot. None of this blocks a tester *driving* a song to completion (they can keyboard-mash through), but it blocks the experience from feeling like Rock Band.

---

## Polish backlog

Ordered by impact (highest first). "Side" = engine (= `milo-native-engine`), rb3 (= game code or assets in this repo), or unknown. Complexity: **S** = one-line, **M** = single file edit + test, **L** = investigation needed before edit.

| ID | Description | Screenshot | Priority | Side | Cx | Root-cause hypothesis |
|---|---|---|---|---|---|---|
| **V1** | Song-row titles entirely missing in song select — list rows are visually empty (just `0/10` / `0/5` / `0/30` score columns + grey row backgrounds). The Wii ref shows each row as `<song name> <artist>  N SONGS  <stars>`. | `03_song_select.png` vs `yt_qRagnZCIMzk_song_select_list.png` | P0 | engine (most likely) | L | Different failure from the known Phase-3 dimness. Phase 3 explains "title text renders dim grey"; here the title text isn't rendering at all. Probably a separate `UIListLabel` slot / list-item refresh path that never builds its `RndText` sub-mesh on web, OR a `WebAssets`-style asset fetch that returns the song-list data but never delivers it to the UI binder. **First check:** add a probe in `BandRnd::DrawMesh` to count un-named (text) sub-meshes drawn per frame on the song-select screen vs the song-select baseline; if count is the same, it's a colour/state issue; if W5-fix has fewer than the native ref's expected count, it's a list-binding miss. |
| **V2** | Main-hub menu list missing — no `PLAY NOW / CAREER / TRAINING / CUSTOMIZE / GET MORE SONGS` text down the left side of the hub. The hub 3D scene + the news ticker text along the bottom *do* render. | `02_main_hub.png` vs `yt_mhKNp9uAT48_menu_hub.png` | P0 | rb3 | S | **ROOT-CAUSED 2026-05-30** — `AnimTask::Poll()` arg-swap (`SetFrame(blend, frame)` vs retail-asm `(frame, blend)`) freezes every reveal anim at frame 1. Buttons are technically `Showing=true` but their PropAnim (`01_main_button_reveal.anim`, frames 0→80) never advances → buttons render at the start "off-screen / hidden" pose. Fix: 1-line HX_NATIVE swap in `src/system/rndobj/Anim.cpp:378`. See `## V2 investigation (2026-05-30)` below. |
| **V3** | Gameplay HUD numerics absent — score, streak, multiplier digits, star-power gauge, energy bar, and song-progress bar are all missing during gameplay. The W5 fix added the "COOL" multiplier-ring panel and the left-edge text strip, but the *numbers* (Wii ref `78 250`) and the star meter still aren't there. | `07_gameplay_t15s.png` vs `yt_qRagnZCIMzk_gameplay_guitar.png` | P0 | engine + rb3 | L | **DIGIT SUB-FIX LANDED 2026-05-30** — the score digit slot was a `SetGeomOwner` GPU-cache miss in the native engine: `MeshGpuCache::EnsureMeshUploaded()` keyed uploads by the drawn mesh pointer but read verts from `mesh->GetGeomOwner()`; the cache returned `true` on every re-upload even after `BandScoreboard::SetScore` swapped owners to `%d_source.mesh`. New `InvalidateGpuMesh()` plus a 5-line `HX_NATIVE` hook in `RndMesh::SetGeomOwner` clears the cache flag so the next draw re-uploads from the new owner. Score "0" now renders top-center at t=30s gameplay (`w7-hud/08_gameplay_t30s.png`). See `## V5 — W7-HUD SetGeomOwner cache fix (2026-05-30)` below. *Streak counter / SP fill / energy bar fills are PropAnim-driven and stay empty because no gems have been hit (score=0 → meters stay at frame 0) — a separate gameplay-input issue, not a render miss.* |
| **V4** | Album art shows `?` placeholder instead of cover art (known carry-over from baseline; W5 did not address it). Affects the entire song-select experience. | `03_song_select.png` vs `yt_qRagnZCIMzk_song_select_album_art.png` | P1 | server | S | **PLUMBING-FIXED 2026-05-30** — root cause was the dev-server's `extracted/` asset dir is a *curated* slice that omits per-song `_keep.png_xbox` cover art (and other long-tail textures). The full extraction at `orig-assets/extracted-xbox-full/` ships all 261 song-art textures. Fix: `native/web/server.py` now probes `extracted-xbox-full/` as a secondary root when the primary 404s — backward-compatible, no engine changes. Verified end-to-end: `/api/file/songs/20thcenturyboy/gen/20thcenturyboy_keep.png_xbox` → 200 (43680 bytes); placeholder `?` only persists for unmounted UGC songs (per retail DTA logic). See `## V4 implementation (2026-05-30)` below. |
| **V5** | Intro cinematic plays against a pure-black backdrop (`05_game_screen_entry.png` shows only the microphones, lit from behind). The Wii intro cinematic has the lit-venue 3D scene visible behind the props. | `05_game_screen_entry.png` vs Wii longplay intros | P1 | engine | L | Likely the tv3 cinematic's vignette / clear-colour state from the recent `acdfc69f` "tv3 transition cinematic default-on" series. The vignette may be over-darkening the venue background, or the camera frustum cull/blackout is too aggressive. The fact that *only* the props are visible suggests the venue mesh is z-rejected or the clear colour is full black with no background draw call. **First check:** does the baseline `05_game_screen_entry.png` show the venue? No — it shows the stage gear lit on black at a different camera angle. So this has been a problem since baseline; the W5 capture happened on a different cinematic frame, not a regression. |
| **V6** | Splash background is pure black (no intro Bink movie). Known stub. The `PRESS START` text is now bright yellow on black, which makes the missing movie more obvious. | `01_splash.png` vs `title_screen_360_tcrf.png` | P2 | engine | L | BinkVideo decode is stubbed (documented). Replacing with a still-image fallback is cheap and would dramatically improve first-impression. Alternatives: extract intro to MP4/WebM and play via HTMLVideoElement; or render a static venue-billboard. Not a regression. |
| **V7** | `03_song_select` "CONNECT CONTROLLER" overlay + "CHOOSE INSTRUMENT" labels both visible simultaneously on the bottom button bar — looks like two distinct UI states overlapping. The Wii ref shows only "CHOOSE INSTRUMENT" (in the post-controller-connect state). | `03_song_select.png` (bottom row) | P1 | rb3 | M | UI state toggle that flips one prompt off when the other comes on is not firing on web. Probably an `OnControllerConnect` / `OnPlayerJoined` event that the web input shim never delivers. **First check:** grep `CONNECT CONTROLLER` in `src/band3/ui/` to find the panel definition, then trace its visibility binder. |
| **V8** | "FRIEND RANKINGS" panel on `03_song_select` is misframed — the panel backdrop is at top-right (where the album-art panel should be), and the album-art `?` placeholder sits OUTSIDE/ABOVE it. The Wii ref puts both in the right column stacked (album art on top, rankings below). | `03_song_select.png` vs `yt_qRagnZCIMzk_song_select_diff_ratings.png` | P1 | rb3 | M | Likely a panel anchor / Y-offset miscalculation, or the FRIEND RANKINGS panel was given the album-art slot's transform. Could also be a missing panel above it pushing layout down. **First check:** view the song-select scene tree via the diagnostics overlay (W4g, if exposed) and compare panel positions vs the milo scene file's declared positions. |
| **V9** | News-ticker text in `02_main_hub` rolls across the *entire bottom half* of the screen as a wide row of small text, where the Wii ref shows a single thin line of text near the bottom-edge ticker bar. Suggests text-row sizing / clipping is too generous. | `02_main_hub.png` (lower third) | P2 | engine | M | `RndText` lacks a clip-rect or `font scale` from the way the W5 fix interprets the alpha mask. The text glyphs themselves look right; the layout is taking the whole panel rect instead of just the ticker line. Or the ticker scroll animation isn't running, so all the queued lines stack on screen. |
| **V10** | Player-profile slot row in `04_part_difficulty` is mostly blank — should show "ALTAIRESPEN" (or whichever profile) avatar + name + the difficulty-dot strip for the selected instrument. Only "GUITAR" + "BASS" tab labels are legible. | `04_part_difficulty.png` (lower-left strip) vs Wii ref of part-difficulty (no clean ref; infer from `band_challenges_menu_wikipedia.jpg` panel style) | P1 | rb3 | M | Player slot row is per-player UI built from `BandUser` data. On web the user list probably has no active users (no controller-connect event). Connects to V7. |
| **V11** | `06_gameplay_t5s` (allegedly "near-black vignette fade" per W5 README) actually shows the same `LOADING…` icon + text as `baseline-2026-05-29/06_gameplay_t5s.png`. So we never reach the gem highway by 5s into the song — we're still on the LOADING screen mid-transition. Indicates the song-load path takes longer on web than the capture script accounts for. | `06_gameplay_t5s.png` vs `baseline 06_*` (same) and `07_gameplay_t15s.png` (~10s later, where the highway finally renders) | P1 | rb3-side capture script + engine load timing | S (capture script timeout bump) / L (load-time fix) | The capture-script timeout for "gameplay should be alive 5s after entry" is too short, or the W3 "MOGG slow main-thread sync XHR" call-out is the actual cause — the audio resource fetch blocks the main thread long enough to miss the 5s window. Bumping the capture script's wait to 10s would surface the real state; fixing the load time is the proper fix. |
| **V12** | Top-left "COOL" panel in `07_gameplay_t15s` is positioned ABOVE the highway (Wii ref has the score panel top-right and a vocal-arrow indicator top-left). The "COOL" panel is actually the vocal freestyle prompt, which suggests the screen thinks it's mid-vocal-section even though we're playing a guitar/drum chart. | `07_gameplay_t15s.png` (top-left) vs `yt_qRagnZCIMzk_gameplay_guitar.png` | P2 | rb3 | L | Either (a) the test song is a vocal section and the COOL is correct, or (b) the per-player track-routing is wrong and a vocal HUD is rendering for a non-vocal player. Need to know which song / part is being played in the capture to disambiguate. |
| **V13** | Lighting on splash-cinematic mics (`05_game_screen_entry.png`) is very dim — the mics are barely visible against the black backdrop. Wii cinematics show props brightly stage-lit with rim light. | `05_game_screen_entry.png` | P2 | engine | L | Could be missing key/fill stage lighting from the venue's lighting rig if the venue scene isn't loaded (see V5). Or the bloom / tone-map pass that brightens stage props is not enabled on the WGPU backend. |
| **V14** | Phase-3 dimness — score digits + song-row titles + some news-ticker text render dim grey not crisp white. *Already being investigated in parallel; listed here for completeness so it doesn't get re-dispatched.* | `03_song_select.png`, `07_gameplay_t15s.png`, `02_main_hub.png` | P0 | engine | L | Material colour selection in `UIListLabel` / `UILabel` style resolution. See W5_TEXT_RENDERING.md "Phase 3". **Do not dispatch on this — parallel agent owns it.** |
| **V15** | Main-hub menu buttons are vertically cramped — PLAY NOW + CAREER + TRAINING + CUSTOMIZE + GET MORE SONGS are visible (V2 fix landed) but the per-button vertical spacing is ~half of retail, so the unfocused labels overlap each other. | `02_main_hub_baseline.png` vs `yt_mhKNp9uAT48_menu_hub.png` | P1 | **engine** | L | **PARTIALLY RESOLVED 2026-05-30 (W8 RndCam investigation, branches `wt-web-w8-cam-verify` + engine `w8-rndcam-projection`)** — Engine fix shipped: NativeSettings::fovScale wired into BandRnd::WriteSceneUniforms (default 1.0 = no-op) + NativeSettings::Get().Init() called in BandRnd::StartGpuInit so MILO_CAM_FOV_SCALE / MILO_CAM_NEAR / MILO_CAM_FAR / etc env vars take effect on the rb3 backend (previously dead). Fixes the dead-knob class of bug. **V15 itself is NOT fully solved by the projection-side fix** — runtime probe traced the UI cam to [ui.cam] at world Y=-768 yfov=0.6024 rad, and the projection math from that cam for a button at world Z=104.4 mathematically yields a 70-pixel vertical span between adjacent buttons vs the observed ~15-25 px. The ~3x discrepancy is downstream of the projection (likely LabelShrinkWrapper / RndText text-mesh world-positioning at draw time differing from the .btn group, or a different camera actually drawing the labels). The W7 BTNPROBE only sampled at Enter() — a W9 follow-up needs to log the draw-time world positions of the labeled text meshes. See `docs/sessions/web/screenshots/w8-rndcam-fix/README.md` for the W8 probe data + reproduction + math. **Hand off to W9.** Prior W7 doc kept verbatim below for context. |

---

## V4 implementation (2026-05-30) — server-side asset fallback

**Status:** plumbing fix landed on branch `wt-web-w7-album-art`. The album-art fetch path is now reachable for every song with a `_keep.png_xbox` shipped in the full xbox extraction. The visible `?` for the *currently-highlighted* row in `03_song_select.png` (post-V2 sweep) was the genuine RANDOM SONG art (a question-mark icon by design) and the engine was already rendering it correctly — the verifiable break was that EVERY other row's art (per-song covers, header art for SETLISTS / `A` / `B` / etc.) silently 404'd because the dev-server's curated `extracted/` dir omits per-song textures.

### Root cause

The dev-server (`native/web/server.py`) auto-detects `orig-assets/extracted/` as its asset root. That dir is a curated slice — verified empty of every `_keep*` cover art (`find orig-assets/extracted -name "*_keep*"` → 0 results, all 86 songs ship only `.mid + .milo_xbox`). The full xbox extraction at `orig-assets/extracted-xbox-full/` contains 261 per-song `<short>_keep.png_xbox` textures plus all the UI cover-fallback textures (`blank_album_art_keep.png_xbox`, `song_select_header_keep.png_xbox`, `song_select_random_keep.png_xbox`, `song_select_setlist_keep.png_xbox`).

The art-load path itself was wired correctly end-to-end on web:
- DTA `song_select.dta:1432` (`refresh_top` trigger) sets `album_art.pic tex_file = {$item album_art_path}` per highlighted row.
- `OwnedSongSortNode::GetAlbumArtPath()` (`SongSortNode.cpp:367`) returns `TheSongMgr.GetAlbumArtPath(GetToken())` → `BandSongMgr::SongFilePath(s, "_keep.png", true)` → `songs/<short>/<short>_keep.png`.
- `UIPicture::SetTex` → `UIPicture::UpdateTexture` → `TheLoadMgr.AddLoader(FilePath, kLoadFront)` → `ResourceFactory` → `CacheResource` (`src/system/rndobj/Utl.cpp:1091`) rewrites to `songs/<short>/gen/<short>_keep.png_xbox` using `PlatformSymbol(kPlatformXBox)` (the web build forces `kPlatformXBox` in `main_web.cpp:241`).
- `FileLoader::OpenFile` → `NewFile` → `HmxNativeOpenFile` (`native/src/native_file.cpp:216`) → web fallback path → `WebAssetsFetchSync` issues a synchronous XHR to `/api/file/songs/<short>/gen/<short>_keep.png_xbox`.
- Engine `Tex_Wgpu.cpp::RndTex::PresyncBitmap` already handles `RndBitmap::Create(buffer)` (DXT1/3/5 → BC1/2/3, palette expansion, Xbox endian swap) — proven by character textures rendering since W2.

The only gap was the server returning 404 because the file lives in a sibling dir.

### Fix

`native/web/server.py` — add a fallback search root probed when the primary `ASSETS_DIR` lookup misses:

- New module-level `FALLBACK_ASSETS_DIRS = []`.
- New `_find_fallback_dirs()` auto-detects `orig-assets/extracted-xbox-full/` (mirroring the primary auto-detect pattern); honors `RB3_ASSETS_FALLBACK` env var and a repeatable `--assets-fallback <dir>` CLI flag.
- `_serve_asset_file()` extended: after the primary `ASSETS_DIR` and `(..)/(..)/system/...` checks, walk each fallback root with the same two-path probe.
- Boot banner prints `Fallback: <path>` for each configured root.

Backward compatible: a deployment that doesn't have `extracted-xbox-full/` proceeds exactly as before (no fallbacks, original 404 behaviour). No engine, rb3, or DTA changes.

### Verification

Direct HTTP probes (via `curl`):

```text
GET /api/file/songs/20thcenturyboy/gen/20thcenturyboy_keep.png_xbox  → 200, 43680 bytes
GET /api/file/songs/bohemianrhapsody/gen/bohemianrhapsody_keep.png_xbox → 200, 43680 bytes
GET /api/file/songs/foo/gen/foo_keep.png_xbox                        → 404 (unchanged)
GET /api/file/ui/image/gen/blank_album_art_keep.png_xbox             → 200, 43680 bytes  (← previously also 404'd)
GET /api/file/ui/image/gen/song_select_header_keep.png_xbox          → 200
GET /api/file/ui/image/gen/song_select_random_keep.png_xbox          → 200
```

End-to-end browser capture (`scripts/web/w7-v4-album-art-capture.mjs`, headless on port 8957):
- `03_song_select.png` — highlight = `random_song` (type=5 kNodeFunction). The visible `?` IS the genuine `song_select_random_keep.png` icon (RANDOM SONG looks like a question mark by design); fetch confirmed `200` in server log.
- `03b_song_select_on_song.png` — after 30x ArrowDown, highlight = `123` (type=2 SubheaderSortNode); panel renders the ROCK BAND header art (`song_select_header_keep.png`).
- `03c_song_select_next_song.png` — same.

The browser smoke can't easily land the highlight on a `kNodeSong` row (type=4) — ArrowDown moves it slowly and the test bails inside the header section. That's an orthogonal navigation limit; the *fetch* plumbing is verified by both the direct curl probe (which is the canonical test for this layer) and the in-engine successful 200's on the three UI-image cover-fallback assets that previously also 404'd.

Per-song cover art will render on the highlighted song row as soon as either (a) the smoke is taught to walk past `123` to e.g. `20thcenturyboy` (~12 more ArrowDown presses), or (b) any human tester selects a song manually. The DTA `(!song_mgr is_song_mounted $token)` guard does NOT block: `SongMgr::ContentName` returns 0 for `IsOnDisc()`==true songs (all 138 root-located songs in `songs.dta`), so `IsSongMounted` short-circuits to `true` and the per-song path is taken.

### Files touched

- `native/web/server.py` — fallback search-root machinery (~50 lines, all additive).
- `scripts/web/w7-v4-album-art-capture.mjs` — Playwright capture (new file).
- `docs/sessions/web/screenshots/w7-album-art/` — capture output (01_splash / 02_main_hub / 03_song_select / 03b_on_song / 03c_next_song + `capture-summary.json`).
- `docs/plans/web-port/W6_VISUAL_POLISH.md` — this section + the V4 table row update.

No engine, rb3 game code, or DTA changes. Decomp match unaffected.

---

## V2 investigation (2026-05-30) — ROOT CAUSE PINNED

**Status:** root cause confirmed empirically with runtime probe + screenshot proof. Fix is single-line in `src/system/rndobj/Anim.cpp`. Branch `wt-web-w6-v2-probe` (commit pending).

**TL;DR:** `AnimTask::Poll()` calls `mAnim->SetFrame(blend, frame)` but the virtual signature is `SetFrame(float frame, float blend)`. The two FP args are swapped. On retail Wii the source matches at 96.7%; the retail asm has the correct order, so somehow MWCC compiles the swap away (likely an inline/asm pragma artifact — see "Why doesn't this break retail Wii?" below). On native+web the swap is compiled literally: `frame` is fixed at `blend=1.0` for every Poll, so PropAnims never advance past frame 1. UI reveal animations (e.g. `01_main_button_reveal.anim`, frames 0–80) stay frozen at the start pose where buttons are off-screen / transparent.

### Confirmed hypothesis (HIGH confidence)

**Not** hypothesis (1) (`update_state_view` no-op): probe shows the trigger fires correctly. Sequence in the probe log:
1. `MainHubPanel::Enter()` runs with `mHubState=1` (`kMainHubState_Main`).
2. `override_none.trg` fires twice; correctly sets `main_menu.grp` `Showing=1`, `message_area.grp` `Showing=1`, `waiting.grp`/`finding.grp` `Showing=0`.
3. `none_to_main.trg` fires correctly with one anim: `01_main_button_reveal.anim` (start=0, end=80, enable=0, period=0).
4. Direct `mDir->Find<RndDrawable>(name)` probes show all 5 menu buttons (`mb_playnow.btn`, `mb_career.btn`, `mb_trainers.btn` …) AND their parent groups (`mb_*.grp`, `menu_buttons.grp`, `main_menu.grp`) report `Showing=1`.

So the panel hierarchy, trigger graph, and visibility flags are all correct. The buttons are visible to the engine — they just render at the start of their reveal animation (off-screen / 0 alpha / whatever).

**Not** hypothesis (2) (default-hidden milo group): all menu groups report `Showing=1` on the very first frame after `Enter()`. No `set_showing false` is being applied.

**Not** hypothesis (3) (missed `dynamic_cast<AppLabel*>` site): the menu buttons (`mb_playnow.btn` …) are `UIButton`s whose static text is set via `text_token` PROPSYNC at milo load time, NOT via a per-frame Text() binder. They have no `dynamic_cast<AppLabel*>` early-return. (Grep across `src/band3/meta_band/` + `src/band3/game/` confirms the W6-V1 sweep covered all relevant binders; none touch hub-menu labels.)

**Root cause** — frozen reveal animation. Two-step empirical proof:

| Test | Edit | Result |
|---|---|---|
| Baseline | none | `mb_playnow/career/trainers/customize/musicstore` invisible (only PALACE/MUSIC/news-ticker render). Probe confirms `Showing=1` on every button. |
| Hack A | force `it->mAnim->SetFrame(EndFrame(), 1.0)` immediately after spawning the AnimTask in `EventTrigger::TriggerSelf` | **PLAY NOW becomes visible** at its correct screen position. Other 4 still missing (the hack happens to fire on the first iteration; subsequent overrides retrigger `override_none.trg` whose anims overwrite this one — the experiment proved the concept but had a stale-anim regression). |
| Hack B | fix `AnimTask::Poll` arg-swap (gated `HX_NATIVE`): change `mAnim->SetFrame(blend, frame)` → `SetFrame(frame, blend)` at `src/system/rndobj/Anim.cpp:378` | **All 5 menu items appear** (`PLAY NOW`, `CAREER`, `TRAINING`, `CUSTOMIZE`, `GET MORE SONGS`) in the correct vertical stack at the correct screen position. Matches Wii reference (`yt_mhKNp9uAT48_menu_hub.png`). |

Evidence in `docs/sessions/web/screenshots/w6-v2-probe/` (on branch `wt-web-w6-v2-probe`):
- `02_main_hub_swap_fix_only.png` — fix B applied, all menu items visible.
- `v2-dbg-init.log` — probe output confirming triggers/showing-state.
- `02_main_hub_after_swap_fix.png` — earlier hack-A intermediate (partial).

### Proposed fix

**Single-line, HX_NATIVE-gated, decomp-match-safe:**

```cpp
// src/system/rndobj/Anim.cpp:378  (AnimTask::Poll)
-    mAnim->SetFrame(blend, frame);
+#ifdef HX_NATIVE
+    mAnim->SetFrame(frame, blend);   // arg order matches retail asm + virtual signature
+#else
+    mAnim->SetFrame(blend, frame);   // preserves the matched (or near-matched) Wii path
+#endif
```

**Scope: S** (one line + comment). **Side: rb3** (`src/system/rndobj/Anim.cpp`). **Decomp-match risk: ZERO** — the change is inside `#ifdef HX_NATIVE`; the Wii build is byte-identical to before.

### Why doesn't this break retail Wii?

`AnimTask::Poll` is at 96.7% match. The retail asm (per `build/SZBE69_B8/asm/system/rndobj/Anim.s` lines around `0x8087CBEC`) loads `f1=frame` (from f30, the time→frame computation) and `f2=blend=1.0` (f29, the `@F_0000803f` literal) before the `bctrl` SetFrame call — i.e. retail asm uses `(frame, blend)` order, NOT `(blend, frame)`. The C++ source is a *mistranscription* of the original asm. Why does the Wii build still work despite the mistranscription?

Likely because of MWCC's optimizer: when both args are floats and the function is virtual, MWCC sometimes reorders fp register usage based on liveness analysis and *both* `blend` and `frame` are alive at the call site. The matched Wii build either (a) was compiled with a source variant we don't have, (b) the 3.3% match-gap IS this swap and the decomp authors knew but couldn't get past it, or (c) the asm is right and the source is wrong but ipa/CSE makes them compile identically because of an inlining/strength-reduction quirk we haven't reverse-engineered. Regardless: **on native (clang/emcc/MWCC-without-IPA) the source is compiled literally and the bug surfaces.**

A follow-up TODO is to investigate whether fixing the source order *improves* the Wii match (would be very nice). Pre-emptively NOT in scope for this V2 fix.

### Risk to other animations

This change affects ALL native UI animation Polls — every PropAnim, AnimFilter, every reveal/fade/scroll trigger. The risk is that *other* UI animations currently appear "correct" on web only because they're driven by static start values that happen to render acceptably. Fixing the AnimTask args would make them advance properly, which is the desired behaviour. Manual smoke test needed across: song_select scroll/highlight, gameplay HUD streak/energy/multiplier reveals, pause-menu fade-in, etc.

The W3c gameplay-test script (`scripts/web/w3c-gameplay-test.mjs`) should pass unchanged because (a) the song still plays end-to-end via the gem highway path, (b) the score-screen transition logic doesn't depend on UI anim progression, (c) the existing baseline tests aren't pixel-perfect.

### Files touched in the worktree experiment (branch `wt-web-w6-v2-probe`)

- `src/system/rndobj/Anim.cpp` — the actual proposed fix (HX_NATIVE-gated, line 378).
- `src/band3/meta_band/MainHubPanel.cpp` — runtime probes in `Enter()` / `UpdateStateView()`. **Revert before merge.**
- `src/system/rndobj/EventTrigger.cpp` — runtime probe in `TriggerSelf()` logging trigger graph. **Revert before merge.**
- `scripts/web/w6-v2-probe.mjs` — minimal capture script (boots → main_hub → screenshots + console logs). Useful keepsake for future V2-class investigations.

The probe edits are large enough that I'll commit them on `wt-web-w6-v2-probe` for traceability, but the orchestrator should land *only* the 1-line `Anim.cpp` change (manually cherry-picked from the branch). The probes are noise for production.

---

## V15 investigation (2026-05-30) — NOT an anim bug; engine-side world-to-screen mapping

**Status:** root-caused via runtime probe on branch `wt-web-w7-button-stack`. No fix shipped from rb3 side — hand off to engine.

**TL;DR:** The post-V2 visual "stacking" of CAREER / TRAINING / CUSTOMIZE / GET MORE SONGS is NOT caused by `01_main_button_reveal.anim` mis-targeting all 5 buttons (the W7-cursor-hud hypothesis). The reveal anim only animates booleans on `LabelShrinkWrapper`s (per-button highlight box visibility) plus a `kDirEvent` on the main_hub PanelDir. It does NOT animate transforms. The button-group transforms are static-loaded from the milo binary and ARE correctly spread out in world space — yet they render at ~half the per-button vertical spacing of retail. Root cause is engine-side (camera frustum / projection / orthographic-to-screen mapping), not rb3 code.

### Probe (instrumentation reverted in final tree)

Two HX_NATIVE probes added temporarily:
1. `RndPropAnim::SetFrame` — log every `PropKeys` target/class/type for any anim whose name matches `*button_reveal*`. Output:
   ```
   PROPANIM_DBG anim='01_main_button_reveal.anim' frame=0.00 keys=8
     PK target='career.lsw'       class='LabelShrinkWrapper' ktype=3 (kBool) xcept=0
     PK target='customize.lsw'    class='LabelShrinkWrapper' ktype=3 (kBool) xcept=0
     PK target='music_store.lsw'  class='LabelShrinkWrapper' ktype=3 (kBool) xcept=0
     PK target='playnow.lsw'      class='LabelShrinkWrapper' ktype=3 (kBool) xcept=0
     PK target='pn_quickplay.lsw' class='LabelShrinkWrapper' ktype=3 (kBool) xcept=0
     PK target='pn_tour.lsw'      class='LabelShrinkWrapper' ktype=3 (kBool) xcept=0
     PK target='training.lsw'     class='LabelShrinkWrapper' ktype=3 (kBool) xcept=0
     PK target='main_hub'         class='PanelDir'           ktype=2 (kObject) xcept=4 (kDirEvent)
   ```
   **Conclusion:** reveal anim drives per-button highlight-box visibility (bool show/hide on each `LabelShrinkWrapper`), plus a DirEvent trigger at frame 0. NO position keys, NO `Vector3Keys`. The buttons are NEVER position-animated.

2. `MainHubPanel::Enter()` — log local/world `RndTransformable::LocalXfm`/`WorldXfm` for every `mb_*.grp` + `mb_*.btn` + `.lsw`. Output:
   ```
   BTNPROBE menu_buttons.grp local pos=(-401.830 0.000  32.404)  world pos=(-401.830 0.000  32.404)
   BTNPROBE mb_playnow.grp   local pos=(   0.000 0.000   0.000)  world pos=(-401.830 0.000  32.404)
   BTNPROBE mb_career.grp    local pos=( 371.545 0.000 -29.757)  world pos=( -30.286 0.000   2.647)
   BTNPROBE mb_trainers.grp  local pos=( 371.545 0.000 -29.757)  world pos=( -30.286 0.000   2.647)
   BTNPROBE mb_shop.grp      local pos=( 371.545 0.000 -29.757)  world pos=( -30.286 0.000   2.647)
   BTNPROBE mb_musicstore.grp local pos=( 371.545 0.000 -29.757)  world pos=( -30.286 0.000   2.647)
   BTNPROBE mb_playnow.btn   local pos=(  25.000 0.000  72.000)  world pos=(-376.830 0.000 104.404)
   BTNPROBE mb_career.btn    local pos=(-346.540 -0.120 72.750)  world pos=(-376.826 -0.120 75.397)
   BTNPROBE mb_trainers.btn  local pos=(-346.540  1.250 44.067)  world pos=(-376.826  1.250 46.713)
   ```
   The 4 non-playnow `mb_*.grp` groups share identical local positions (this is intended — they're authored at the same anchor in the milo, the differentiation happens in their `.btn` children). The `.btn` children DO have distinct world Z values (104.4 → 75.4 → 46.7 → ...) with ~29-unit spacing between adjacent items, matching the retail menu's vertical layout proportions.

### What this implies

The button positions in world space are CORRECT and match retail's vertical spacing. The rendering compresses world-Z to screen-Y at ~half the rate of retail, which is why our build shows ~half the per-button visual gap. Possibilities (engine investigation needed):

1. **Camera frustum / `y_fov`:** `RndCam::SetFrustum` parameters from the main_hub camera's `.cam` object may load differently in the native build (different y_fov, near/far plane, or aspect handling).
2. **Aspect / viewport scale:** Our canvas is 1024×576 (16:9) vs retail 1280×720. Same aspect, but the camera's expected reference resolution may be different, leading to a Y-scale during projection.
3. **Orthographic vs perspective:** If the menu is rendered via an ortho camera, the world-unit-to-pixel scale is set explicitly — and could be off by ~2×.
4. **DPI / device-pixel-ratio:** A canvas DPR mismatch could halve the effective vertical extent.

### Why this is NOT in scope for an rb3-side fix

- The transform data being read out of the milo binary is CORRECT (positions match retail layout proportions).
- The PropAnim system is working CORRECTLY (V2 fix landed; reveal anim runs, the bool show/hide on each `LabelShrinkWrapper` fires at the staggered keyframes).
- The decomp code in `src/system/rndobj/` (Anim, PropAnim, PropKeys, Group, Trans) is doing exactly what its retail counterpart does.

The fix is in `milo-native-engine/src/platform/` (camera/projection setup, viewport scale, or `NativeSettings::fovScale` defaulting). That repo already exposes a runtime `fovScale` knob via `DebugPanel.cpp` + `HttpServer.cpp`; the right fix is probably auto-deriving the scale from the menu camera's intended reference resolution rather than fixing it per-screen.

### Files touched in the worktree experiment (probes only — reverted)

- `src/system/rndobj/PropAnim.cpp` — PROPANIM_DBG probe in `SetFrame` (reverted in final tree).
- `src/band3/meta_band/MainHubPanel.cpp` — BTNPROBE in `Enter()` (reverted in final tree).
- `scripts/web/quick-capture.mjs` — minimal Playwright capture script for main_hub (kept as a future probing tool).

### Risk to decomp match

**ZERO.** No source-code changes shipped. All probes were HX_NATIVE-gated, and even those are reverted in the final commit. Only docs + the capture script were committed.

---

## Things that turned out FINE on inspection

- **Splash `PRESS START` colour:** baseline showed it olive-grey (rendered as alpha-only and multiplied by the wrong colour). W5 fixed it to a saturated yellow, which matches the retail title screen aesthetic. Not a polish issue any more.
- **`LOADING…` text:** went from dim grey (baseline) to crisp white (W5). Fine.
- **Hub 3D scene framing:** matches the Wii hub geometry layout — neon signs (`MUSIC`, `PALACE`), the venue arch, the crowd in front. Lighting is plausible. Geometry quality is not the issue.
- **Venue art in `04_part_difficulty` (subway/graffiti wall):** photo-realistic and matches the Wii art for "20th Century Boy". No regression here.
- **Gem highway in `07_gameplay_t15s`:** lane geometry, fret lines, gem positions all look correct. No z-fighting visible. The bandobj characters in the background are visible.
- **No geometry / mesh regressions vs baseline:** every paint% delta is within ±3.5 percentage points, all attributable to capture-instant differences (different beat of the song captured). The W5 fix predicate (`!mesh->Name()[0]`) is narrow enough that it cannot regress non-text geometry.

---

## Investigation plan for the top 5

### V1 — Song-row titles missing entirely

1. **Re-read** `src/system/ui/UIListLabel.cpp` (Refresh / SetState path) — find what triggers the per-row `RndText::Set*` call.
2. **Probe-add** in engine `BandRnd::DrawMesh` (the W5 fix file): for one capture, log every text sub-mesh's `mesh->Name()` (always `""`) plus its parent panel's name (via `mesh->Parent()->Name()`) and the byte count of its glyph mesh. Look for `song_list_row_N` panels in the log; if absent, the list rows aren't being built; if present with zero glyph bytes, the binder is being called with empty text.
3. **Cross-check DC3:** does DC3's web build show song-row titles correctly? If yes, port DC3's binder path. If DC3has the same gap, this is novel-RB3 work.
4. **Files to read first:** `src/system/ui/UIListLabel.{h,cpp}`, `src/system/ui/UIListMesh.{h,cpp}`, `src/band3/meta_band/MusicLibraryPanel.cpp` (or equivalent), `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp::BandRnd::DrawMesh`.

### V2 — Main-hub menu list missing

1. **Locate the hub menu** in the milo scene file: `orig-assets/extracted/ui/hub/` or similar. Confirm whether the menu is a single panel with text labels (would behave like V1) or N separate panels per item.
2. **Probe-add** on the hub screen: dump `RndDir::ListPanels()` after `main_hub_screen` becomes active. Cross-reference vs the Wii video — the list-of-5 panels should all be present.
3. If panels are absent, the hub scene's milo deserialiser is dropping them. If present but empty, this is a sibling of V1.
4. **Files to read first:** `src/band3/ui/MainHubPanel.cpp` (or whatever the hub host is), the milo loader output for the hub scene (run `rb3-dta` headless on the hub `.milo` and see if it lists 5 menu panel objects).

### V3 — Gameplay HUD numerics

1. **Run a probe on the `game_screen`:** which `UIDigitalLabel` instances exist? Are they being drawn? With what value?
2. **Two failure paths** to distinguish:
   - **(a) Text-style failure** (sibling of Phase 3): digits render but in unreadable colour. Wait on Phase 3 agent.
   - **(b) Binder failure**: digits are never updated from gameplay state (`GamePlayer::GetScore()` not called or its result discarded).
3. **Bars / star meter / progress bar** are separate `Rnd*Mesh` panels — they may or may not have a separate root cause. Inventory them via a `RndDir::ListPanels` dump first.
4. **Files to read first:** `src/band3/game/GemHUD.cpp` (and `BandHUD`, `BandScoreboard` if they exist), `src/system/ui/UIDigitalLabel.{h,cpp}`.

### V4 — Album art `?` placeholder

1. **DevTools network tab** during song-select scroll: are there any `*.png`/`*.bmp` fetches for art? If 404, it's a path-resolver issue; if no fetch, it's a code path.
2. **Read** `src/band3/meta_band/SongData.cpp` for the `HasArt` / `GetArtPath` family — confirm what filename is computed.
3. **Read** `milo-native-engine/src/platform/WebAssets.cpp` to see how the relative path is resolved into a fetch URL.
4. **Files to read first:** `src/band3/meta_band/SongData.cpp`, `src/system/ui/UIPicture.{h,cpp}` or `UIPanel.{h,cpp}`, `milo-native-engine/src/platform/WebAssets.cpp`.

### V5 — Intro cinematic black backdrop

1. **Confirm scope:** is the venue scene loaded at all during the tv3 cinematic? Run the capture script with a tap on frame 1300 (between baseline 05 and 07) and see if the venue ever becomes visible behind the props.
2. **If venue never loads during cinematic:** the cinematic uses an isolated camera/scene by design and the props are the only thing the camera sees — in which case "black backdrop" is correct and we should chase **better stage lighting** on the props (V13) instead.
3. **If venue *does* load** but is z-rejected / overdrawn by black: investigate the vignette pass from commit `acdfc69f`.
4. **Files to read first:** `native/src/rb3_netsession_native.cpp` (recent tv3 fixes are nearby), `src/band3/game/IntroCinematic.cpp` (if it exists), `milo-native-engine/src/scene/` vignette code.

---

## V5 — W7-HUD SetGeomOwner cache fix (2026-05-30)

**Status:** sub-fix LANDED on `wt-web-w7-hud`. Engine branch `w7-hud-setgeomowner` (`e6c8f86`); rb3 5-line `HX_NATIVE` hook in `src/system/rndobj/Mesh.cpp::SetGeomOwner`. The BandScoreboard digit slot (`num%d.mesh`) now renders the live score on web — verified by `docs/sessions/web/screenshots/w7-hud/08_gameplay_t30s.png` showing "0|0FF" top-center where the baseline `w7-phase3/07_gameplay_t15s.png` had an empty plate.

### Root cause

`MeshGpuCache.cpp::EnsureMeshUploaded(mesh)` keyed uploaded GPU buffers by the **drawn mesh pointer** but read vertices/indices from `mesh->GetGeomOwner()`:

```cpp
auto it = sMeshGpuData.find(mesh);
if (it != sMeshGpuData.end() && it->second.uploaded && !isTextMesh)
    return true;                       // <- short-circuits forever
RndMesh* geomOwner = mesh->GetGeomOwner();
...
```

RB3's `BandScoreboard::SetScore()` hot-swaps the digit slots by calling `mNumMeshes[i]->SetGeomOwner(mSrcMeshes[digit])`. `RndMesh::SetGeomOwner` only updates the pointer; it never calls `Sync()`. So the first upload of e.g. `num0.mesh` captured its self-owned (empty) geometry, and the cache returned `true` on every subsequent call — the swap to `5_source.mesh` never reached the GPU.

The same bug latent in the other 4 SetGeomOwner sites (`Tail.cpp`, `VocalTrack.cpp`, `ArpeggioShape.cpp`, `NoteTube.cpp`); all benefit from the single fix.

### Fix

Two pieces, mirroring the existing `CleanupGpuMesh()` weak-link pattern in `RndMesh::~RndMesh`:

1. **Engine** (`milo-native-engine` commit `e6c8f86`, branch `w7-hud-setgeomowner`):
   - Add `void InvalidateGpuMesh(RndMesh*)` to `src/platform/MeshGpuCache.{h,cpp}`. Marks `sMeshGpuData[mesh].uploaded = false` without dropping buffers; the next `EnsureMeshUploaded()` re-reads from the new owner.

2. **rb3** (`src/system/rndobj/Mesh.cpp::SetGeomOwner`):
   ```cpp
   void RndMesh::SetGeomOwner(RndMesh *m) {
       MILO_ASSERT(m, 487);
       mGeomOwner = m;
   #ifdef HX_NATIVE
       extern void InvalidateGpuMesh(RndMesh *);
       InvalidateGpuMesh(this);
   #endif
   }
   ```

Decomp match unchanged (`HX_NATIVE` is a port-only guard, ignored by MWCC).

### Verification

| Screen | Pre-fix (`w7-phase3/`) | Post-fix (`w7-hud/`) |
|---|---|---|
| `07_gameplay_t15s` | empty score plate top-center | empty score plate (camera still cinematic-left at this frame) |
| `08_gameplay_t30s` | n/a (capture stopped at 15s) | **"0|0FF" score digit clearly visible top-center** |

Smoke regression: `scripts/web/w3c-gameplay-test.mjs` PASS — boots, navigates main_hub → song_select → part_difficulty → game_screen, runs gameplay for 30+ seconds at 34 fps, ends on `game_screen` with no crash.

### What this does NOT fix (deferred)

- **Streak counter** (right-side multiplier digits) — same `SetGeomOwner` path? No, the streak meter uses `BandLabel::SetTokenFmt(streak_multiplier_fmt, mult)` (a UILabel, not a mesh swap). Stays blank because `MultiplierChanged()` only writes the label when mult > 1, and mult stays at 1 with no gem hits.
- **OD fill / energy meter fill** — `OverdriveMeter::SetEnergy` calls `mExtendAnimGroup->SetFrame(energy)`. PropAnim → property → mesh material UV setter, which DOES go through Sync (and thus the cache invalidation that's been in place since W2). Fill stays empty because `BandTrack::RefreshOverdrive(energy)` is gated on player gem-hit events that don't fire when no gems are auto-hit on web.
- **Multiplier-dot icons** (5 small ring dots) — `mMultiMeterAnim->SetFrame(mult)`; same story as the fill.

All three above stay empty because the gameplay scoring loop doesn't get input — they would self-fill the moment a gem-hit increments `mScore`. That gap is a separate input/scoring issue, NOT a render miss. Confirmed by the score going from `-1` (initial) to `0` (rendered via this fix) — the BandScoreboard IS being called every Poll cycle in `TrackPanel.cpp:611`.

---

## Suggested first dispatch

**V1 (song-row titles missing).** It is the single thing most preventing a tester from picking a song without keyboard-mashing through a blank list. The Phase 3 dimness agent owns "text is dim"; V1 is "text isn't there at all" so the two don't collide. The investigation likely reuses the W5-fix probe machinery, which is fresh in the engine repo. Expected outcome: discover whether this is a list-binder bug (rb3-side, fixable here) or a sibling of Phase 3 with a different style slot (engine-side, hand to engine).

Once V1 has a hypothesis, V2 (hub menu) and V3 (HUD digits) likely benefit from the same probe — dispatch them in parallel as a follow-up wave once the probe is in place.

---

## V6 — W8 HUD meters investigation (2026-05-30) — DEFERRED, three separate engine-side gaps

**Status:** INVESTIGATION ONLY — no source fix landed. Probes instrumented + reverted on `wt-web-w8-meters` (no commit). Root causes documented for engine-side handoff.

### Symptom

Post-V5 (W7-HUD), score digit ("0|0FF" → ticks up to 6,694 over 30s) renders correctly. But the streak meter `×N` counter, OD meter fill, and 5 multiplier-dot icons all stay empty during gameplay, even with autohit firing and the scoring loop accumulating points correctly.

### Method

Built rb3-web release with always-on `MILO_LOG("[HUD_DBG] ...")` probes at every meter call site in the chain:
- `BandTrack::ResetStreakMeter`, `BandTrack::Reset` (init + post-Reset visibility gate)
- `BandTrack::RefreshStreakMeter`, `BandTrack::RefreshOverdrive` (one-time refresh)
- `BandTrack::SetMultiplier`, `BandTrack::SetStreak` (per-streak-change propagation)
- `Track::Poll` (per-frame change-detector that drives SetStreak)
- `TrackPanel::Poll`'s `SetMultiplier(em)` (per-frame band-multiplier propagation)
- `StreakMeter::SetMultiplier`, `StreakMeter::UpdateMultiplierText` (widget data write)
- `OverdriveMeter::SetEnergy`, `OverdriveMeter::Reset` (fill setter)
- `Player::SetEnergyAutomatically`, `Player::SetEnergy` (energy data source)

Ran `w3c-gameplay-test.mjs` against worktree port 8719 (release build), captured 30s of gameplay, harvested `console.jsonl`.

### Findings — three independent gaps

**1. Streak `×N` text — DATA REACHES THE LABEL, WIDGET DOES NOT RENDER.**
- Per probe: `StreakMeter::SetMultiplier(2)` fires at t≈97s as streak hits the 2x threshold (`UpdateMultiplierText(2)` confirmed); `SetMultiplier(3)` fires at t≈100s (`UpdateMultiplierText(3)` confirmed); `SetMultiplier(4)` fires at t≈103s. `mMultiplierLabel` and `mXLabel` pointers are non-null and stable.
- `BandTrack::Reset` confirms `HasPlayer=1, showing=1, mShowOverDriveMeter=1, !unk1b` — the streak meter parent dir IS shown.
- Yet the `×N` text never appears on screen. Same class as the W6 V1 song-list `AppLabel` failure (label text bound but not rendering) — but on a `BandLabel`/`UILabel` path that's distinct from V1's `set_score_or_stars` engine fix landed in V5. Likely needs the **same engine-side label-rendering refresh-on-SetTokenFmt** that V5 added for the score-digit mesh-swap, but applied to the `BandLabel::SetTokenFmt → mesh upload` path. **Engine-side fix required.**

**2. OD meter fill — `mBandEnergy` is RESET TO 0 every frame by an unidentified caller.**
- `Player::SetEnergyAutomatically(this=0x93b99c0, f=0.000)` fires ~30 times/sec on the active player throughout gameplay (270+ calls captured), with `cur=0.000` every time — the function sets `mBandEnergy = f`, so any energy accrued by `CompleteCommonPhrase → AddEnergy` gets stomped on the next frame.
- `Player::SetEnergy` is NEVER called in the captured period (zero `HUD_DBG] Player::SetEnergy` events).
- Therefore the per-frame caller is either `Player::SetEnergyFromNet` (via `remote_update_energy` action handler) or the `set_energy_automatically` action message — neither of which has any in-source caller, so the source must be a net-message loop or a DTA handler chain not in the extracted assets. **Needs further probing of `Player::SetEnergyFromNet` and `HandleType` dispatch to identify the per-frame caller, then suppress on `HX_NATIVE` (or gate behind a "is networked" predicate that's true on native by default but should be false in singleplayer-web).**
- Side-effect: even when `Player::mBandEnergy` does transiently reach 0.251 (as observed via `Track::Poll` probe at frame 360), the next `SetEnergyAutomatically(0)` zeros it out before the next `meter->SetEnergy` call lands.

**3. Multiplier 5-dot icons — `mMultiMeterAnim->SetFrame(mult)` is NEVER called in source.**
- Grep across all `src/`: zero references to `mMultiMeterAnim->SetFrame` or any setter. The `RndTransAnim` is only `Find`-bound in `StreakMeter::SyncObjects` and never driven.
- Retail behavior must be DTA-script-driven (a handler on `set_multiplier` or `set_streak` that fires `mMultiMeterAnim->SetFrame(mult)`), but the trigger is missing from the extracted assets we have. **Either an extraction gap or a milo-internal trigger that needs to be hooked from the C++ side in `BandTrack::SetMultiplier`/`SetStreak`.**

### Confirmed (NOT the bug)

- **`HasPlayer` gate** (W7-HUD doc suspect) — fires correctly. Active player's `showing=1` after Reset; `mStreakMeter`/`mStarPowerMeter` pointers stable and non-null.
- **Score increment** — confirmed working: `mainScore=0 → 7166` over 720 frames (per `TrackPanel::Poll` probe). `mStats.GetCurrentStreak()` also rising (0 → 47+).
- **`BandTrack::SetMultiplier` propagation** — `Track::Poll` correctly detects `curstreak != mLastStreakCount` and fires `bandtrack->SetStreak(streak, mult, notes, false)` which descends to `StreakMeter::SetMultiplier(mult)`.
- **Smoke test (`w3c-gameplay-test.mjs`)** — PASS on release build with probes; reaches `game_screen`, autohit fires, song plays 30s+ at 30-35 fps.

### Why not a single rb3-side patch

Each gap is a separate mechanism:
- (1) is engine-side `BandLabel::SetTokenFmt → mesh refresh` (label render path).
- (2) is an unidentified net/handler per-frame caller (needs deeper probing to find).
- (3) is a missing DTA handler or C++ hook to `mMultiMeterAnim`.

None of (1)/(3) can be fixed from `BandTrack`/`StreakMeter`/`OverdriveMeter` source while preserving decomp match. (2) might be HX_NATIVE-gateable once the caller is identified, but the identification itself is the bulk of the remaining work and would need additional probes (`Player::SetEnergyFromNet`, `Player::HandleType` for `set_energy_automatically`) plus a re-build/re-capture cycle.

### Capture

- Screenshots: `docs/sessions/web/screenshots/w8-meters/` (8 frames, post-build baseline + smoke run results in `scripts/web/results/web-w3c/gameplay/`).
- The most informative is `05_gameplay_t30s.png` (score "6,694" visible top-center, left vertical bar shows only a small white tick at top, right OD bar empty, no `×N` label).

### Files read

- `src/system/bandobj/{StreakMeter,OverdriveMeter,BandTrack,GemTrackDir}.{cpp,h}`
- `src/band3/bandtrack/{Track,TrackPanel,GemTrack,TrackConfig}.{cpp,h}`
- `src/band3/game/{Player,GemPlayer,Performer,Band,BandUser}.{cpp,h}`
- `src/system/beatmatch/{TrackWatcherImpl,BeatMatcher}.{cpp,h}`
- `native/src/rb3_game_input.cpp` (autohit chain)
- `orig-assets/extracted/config/player.dta`, `player_net.dta` (handler audit)

### Recommended next step

Defer until either (a) the W6-V1-class label-render fix family is generalized engine-side to cover `BandLabel::SetTokenFmt` (which would fix gap (1) "for free"), or (b) a focused probe-and-grep session is run on `Player::SetEnergyFromNet` + the `set_energy_automatically` handler dispatch to find the per-frame zero-energy caller for gap (2). Gap (3) (multi-meter dots) is the smallest and might land as a 5-line `HX_NATIVE` hook in `BandTrack::SetMultiplier` once gap (1) is sorted (so the change is visible).
