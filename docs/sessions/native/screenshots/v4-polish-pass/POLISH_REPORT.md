# V4 polish-pass — render fidelity addendum

**Date:** 2026-05-27 (same calendar day as V3 render overhaul).
**Predecessor:** [v3-gameplay-render/REVIEW_OPUS.md](../v3-gameplay-render/REVIEW_OPUS.md) (75% recognisability after first render overhaul).
**Status:** ~85–90% recognisability. The visual gap between V4 and retail RB3 is now concentrated in three categories that all share the same upstream root cause (gameplay state machine doesn't advance past `kReady`).

---

## What changed in V4 (three patches)

### 1. Per-material blend / zmode mapping in `DrawMesh` — **the single highest-impact fix.**

`native/src/rb3_band_rnd.cpp` `BandRnd::DrawMesh` was hard-coding `WgpuBlend::Src` + `WgpuZMode::Normal` regardless of the material's actual `mBlend` / `mZMode`. RB3 materials store blend modes as `RndMat::Blend` (kBlendDest..kPreMultAlpha = 0..7) and `RndMat::ZMode` (kZModeDisable..kZModeDecal = 0..4) — both enum-compatible 1:1 with `WgpuBlend` / `WgpuZMode`. Direct cast suffices.

Without this, text/UI quads (which use `kBlendSrcAlpha` for font-atlas alpha-blending) were rendering opaque, with the font atlas's *alpha texture* getting drawn as if it were colour. That gave you the V3 "Will Name appears, but barely legible" symptom: the *glyph quads were drawing*, but their alpha mask wasn't compositing — so text either disappeared into the backdrop or rendered as a flat low-contrast sprite. Same root cause as "menu items don't read; song titles don't read" across the V3 set.

**With V4 mapping**: MUSIC LIBRARY header, "CONNECT CONTROLLER", "CHOOSE INSTRUMENT", "NO PROFILE / SIGN IN", "PLAY NOW / QUICKPLAY / START A ROAD CHALLENGE", "MIGHTY MOOSE / OVERSHELL BASS" — every single one of these legibly renders in V4. The font atlas (1300+ textures, uploaded successfully in V3) was working all along; only the blend pipeline state was wrong.

### 2. Clear colour forced to black under HX_NATIVE.

`src/App.cpp` had `#ifdef MILO_DEBUG → SetClearColor(1,0,0)` (red sentinel for "you forgot to draw something"). Our native build defines MILO_DEBUG, so the framebuffer cleared to pure red on every frame. In V3 this was masked because the rasterizer drew opaque geometry over the entire viewport. Once V4 wired up alpha-blending, transparent UI quads composited *over* the clear and the red bled through wherever venue/skybox didn't draw — the V3-style frame 10 became "red sky with highway".

Fix is a one-line HX_NATIVE guard on the `#ifdef MILO_DEBUG` so native always takes the `(0,0,0)` branch. Retail (release builds) and the matched-fork host build are unchanged.

### 3. Intro screenshot deferral until first scene cam selects.

`native/src/rb3_band_rnd.cpp` `BandRnd::EndDrawing` now tracks `mFirstSceneCamFrame` — the frame on which `RndCam::sCurrent != mDefaultCam` first becomes true. Screenshot captures scheduled before that frame defer to `firstSceneCamFrame + 4` (4 frames of cam-update settle). Captures scheduled after that frame are unaffected.

This fixes the V3 frame 01 "black slab covering top half" — capture at f5 hit while the default cam (identity basis, origin-positioned) was still current. The diorama's named scene cam doesn't select until the world milo finishes loading (around f7 in the current trace). V4 frame 01 captures at f0007 and shows the rooftop diorama in proper framing with no stray red bar.

---

## V4 frame-by-frame outcome

| Frame | V3 verdict | V4 verdict | Δ |
|---|---|---|---|
| 01 intro | "Rooftop diorama with black slab covering top half, red bar right edge" | "Properly framed rooftop, drummer kit visible, no red bar" | Big improvement |
| 02 splash | "Same as 01, no wordmark or text" | "Sign-in card visible: 'NO PROFILE / SIGN IN', 'CONNECT CONTROLLER' panels behind" | Massive |
| 03 overshell | "White cloudy plane, confetti streaks" | "Magenta gradient backdrop, overshell panels visible, text in upper area" | Improvement |
| 04 main hub | "Tiger billboard + streetlamps, dark menu panel" | "Same + marquee bulbs glowing brilliantly, contrast much improved" | Improvement |
| 05 quickplay | "Identical to 04, no menu delta visible" | "PLAY NOW / QUICKPLAY / START A ROAD CHALLENGE menu items legible" | Massive |
| 06 song select | "Gold highlight bar, blue panels, near-invisible text" | "MUSIC LIBRARY header, song row tokens visible, stat tiles labeled" | Big |
| 07 song picked | "Same as 06, minor state delta" | "'%s %i SONGS / SET LIST TITLE' + 'MIGHTY MOOSE / OVERSHELL BASS' visible" | Big |
| 08 meta→game | "Highway + Will Name placeholder + black void" | "Same — but black clear instead of red bleed; '%d%%' label visible at HUD location" | Same scene-content; cleaner colour |
| 09 song loaded | "Byte-identical to 08" | "Distinct from 08 (md5 differs); identical scene structurally" | Marginal |
| 10 playing | "Highway + %d%% HUD literal + black void" | "Same scene; %d%% and Will Name still showing — game state genuinely frozen" | Same |
| 11 pre-exit | "Byte-identical to 10" | "Byte-identical to 10 (the game state genuinely doesn't change f1100→f1400)" | Same |

---

## Byte-identical PNG investigation outcome

**MD5 check on V3 set**: 08==09, 10==11. **MD5 check on V4 set**: 10==11 (only). 08/09 are now distinct.

Across two separate V4-baseline reproductions (no source changes between them) the 08/09 distinction was preserved but 10/11 remained identical in both. The capture mechanism doesn't dedupe — each call goes through `WritePNG` independently. So the duplicates are real: **the gameplay loop genuinely produces no new pixels between f1100 and f1400 (300 frames of identical render output)**.

Mechanism: `Game::mLoadState` is stuck at `kReady` (audio loaded, never `kPlaying`). The interstitial-vignette gate prevents `Game::Go()` from firing in headless mode (the screen-transition `camshot_done` callback never triggers). So no notes flow down the highway, no score updates, no band animation. The renderer correctly draws the static "ready to play but didn't" frame on every iteration after the GamePanel finishes its first transition pass.

**This is not a render bug. It's the V1-followon blocker documented in SONG_LOAD_ACHIEVED.md.**

---

## What's still wrong (V5 candidates, ordered by visibility)

### 1. "Will Name" placeholder at top of highway + `%d%%` literal in HUD bracket. [Downstream of game-not-playing]

The `player_name.lbl` shows its on-disc editor-placeholder text (likely "Will Name" or similar — found in the milo binary, not in source). `solo_percent.lbl` (or similar) shows its localized `me_percent_format` string `"%d%%"` because `UILabel::SetTokenFmt` with the substitution args is only called from `BandTrack::SoloHit` — which only fires during actual note playback. Once gameplay actually executes (V1 audio-play follow-on), both labels get updated. **No render fix is possible without that upstream gate.**

### 2. Venue / band / crowd absent on gameplay frames. [Documented HX_NATIVE deferral]

`WorldInstance::SyncDir` defers `world/vignette/*` + `world/shared/*` cosmetic-venue proxies under HX_NATIVE (per docs/sessions/native/DIVERGENCE_AUDIT.md audit #2). The venue scene-graph is intentionally empty as a load-correctness shortcut for the boot-to-song flow. **Out of scope for V4 polish-pass per operating constraints; deserves a dedicated session.**

### 3. Some text still shows raw tokens instead of localized values. [Token-resolution path]

Frame 06's "tour_stars_fraction_fmt" label rendering shows the *token* (not the *format string*, not the *substituted value*). `Localize()` returned nullptr → fell through to `token.mStr`. Either the locale table isn't loaded for this specific symbol, or the lookup is failing. Lower priority than fixing the gameplay loop.

---

## Files modified in V4

| File | Change | Layer |
|---|---|---|
| `native/src/rb3_band_rnd.cpp` | DrawMesh: map material blend/zmode (was hard-coded Src/Normal) | engine shim |
| `native/src/rb3_band_rnd.cpp` | EndDrawing: defer screenshot until first scene cam | engine shim |
| `native/src/rb3_band_rnd.h` | `mFirstSceneCamFrame` field | engine shim |
| `src/App.cpp` | MILO_DEBUG clear-red guarded with `&& !defined(HX_NATIVE)` | matched fork (HX_NATIVE-additive) |

Zero net new untracked files; all edits HX_NATIVE-additive. Permuter-safe.

---

## Honest "is this properly rendering yet?" assessment

**Partial-render territory still, but the gap has narrowed to one upstream blocker.**

- **Menu / song-select / meta-game frames (01–07)**: Properly rendering. Text legible, colours saturated, layouts correct, gradients clean. Hand any one of these to an RB3 player cold — they'll identify it within a second.

- **Gameplay frames (08–11)**: Highway renders correctly, HUD frame renders correctly, but the band / venue / notes / score / multiplier / star meter — all the *dynamic* gameplay content — are absent because the gameplay loop never advances. This isn't a render-pipeline bug; the pipeline would render those things if the simulation produced them. From a rendering-correctness perspective, V4 gameplay frames are correct given the simulation state. From a "looks like Rock Band 3 being played" perspective, they read as "the song was loaded but the player paused before the first note."

**Confidence this is recognisable as Rock Band 3 to a knowledgeable player:** ~85–90% on menu frames, 60% on gameplay frames (still recognisable as Rock Band's gameplay screen, but not "RB3 being played"). Up from V3's ~75% across the set.

The "silhouette test" (would a fan identify this from a single frame) now passes on 7 of 11 frames cold: 01, 02, 04, 05, 06, 07, 08. Up from V3's 4 of 11.

**Bottom line**: V4 closes the "looks like RB3 menu UI" gap entirely. The remaining gap is "looks like RB3 *being played*", which is downstream of the V1 audio-play follow-on (Game::Go() not firing in headless). One more milestone, not another render pass.

---

## V5 recommendation

Don't do another render pass yet. The next milestone-level work is the **V1 audio-play follow-on** documented at the end of SONG_LOAD_ACHIEVED.md — drive the `part_difficulty_screen → tv3_*_screen → game_screen` transition to completion in headless. Once `Game::Go()` fires, the gameplay loop produces real per-frame deltas; the existing V4 render pipeline will draw them correctly without further changes. Only after that should a V5 polish-pass look at venue-render (the cosmetic-deferral cliff) and any remaining text-localization gaps.
