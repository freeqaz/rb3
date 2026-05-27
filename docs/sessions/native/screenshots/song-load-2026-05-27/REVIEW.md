# RB3 Native — Screenshot Review: Boot + Song-Load (2026-05-27)

**Run:** reproducer from SONG_LOAD_ACHIEVED.md at MILO_MAX_FRAMES=1500.
**Binary:** `/home/free/code/milohax/rb3/native/build-native/rb3-native` (post screenshot-instrumentation build)
**Capture mechanism:** new `BandRnd::BeginDrawing()` / `EndDrawing()` overrides wired to the game
frame loop; reads `MILO_SCREENSHOT_DIR` + `MILO_SCREENSHOT_FRAMES` + `MILO_SCREENSHOT_NAMES`.
`BeginDrawing()` calls `BeginFrame(RndCam::sCurrent ?: mDefaultCam)`, `EndDrawing()` calls
`EndFrame()` + GPU readback + `WritePNG`. Frame counter maintained independently in `mFrameCount`
since base `Rnd::EndDrawing()` is bypassed.

---

## Per-screenshot summary

### 01_f0005_intro.png — Pre-`@10:start` (intro movie / title)
3D geometry is rendering: a rooftop/city diorama (the RB3 intro backdrop), lit with flat
vertex/material colors (pinkish and lavender buildings, gray sky). A large bright-red shape
dominates the left — the RB3 logo/banner mesh. Geometry counts confirm substantial draw work
(552 meshes, 406 k tris). **No textures** (all surfaces show bare vertex or material color;
`BandRnd::DrawMesh` hardcodes `mu.useTexture = 0`). Camera angle is awkward (shallow-oblique),
showing the stage floor edge — `RndCam::sCurrent` was either null or set to an ambient/secondary
camera before frame 5.

### 02_f0025_splash.png — Post-`@10:start` (splash / intro screen)
The 3D background is gone; only 2D UI panels are visible: a gray gradient band at top and two
blue-striped panel pairs below (button-selector quads). The stripes are the correct shape for
RB3's controller-select buttons. No text is visible — `RndText` geometry is likely being drawn
but renders white-on-white (text mesh color = white, no texture decode). Clear reading of the
underlying panel structure is present. Mesh count 696, 419 k tris — all UI geometry.

### 03_f0050_overshell_join.png — Post-`@30:confirm` (overshell user-join flow)
Heavy red-dominant frame. Two large diagonal red planes fill the left half (the overshell's RB3
red-accent chevrons), white diagonal UI panels at center. The overshell user-join overlay is
confirming — geometry is correct in shape and position. No textures, no legible text. Red is
correct for this screen (RB3's overshell branding color).

### 04_f0120_main_hub.png — Post-confirm transition (main_hub_screen)
**Blank white.** The main hub screen's background is an unlit white plane that fills the full
viewport. All other scene meshes (button labels, 3D hub elements) either draw on top of white
and are washed out, or the scene camera is pointing at a blank backdrop. At this frame the
renderer is submitting 533 meshes / 385 k tris (confirmed from log), so geometry exists — it is
just not visible at this camera angle against the white background. Text meshes are likely white
on white.

### 05_f0200_quickplay_screen.png — Mid-`@140:select` (party-quickplay screen)
**Blank white.** Same pattern as frame 04. The quickplay setup screen has a white background and
the current camera (RndCam::sCurrent = some scene cam looking into the background plane) washes
everything out. Mesh count matches the hub (~533) — same scene, slightly advanced.

### 06_f0280_song_select.png — Post-`@220:select` (song_select_screen, 83 songs)
Most visually informative frame. Visible elements: gray-gradient horizontal bands (song list
rows), darker gray/brown thumbnail-region for one song at upper center, a 3D character figure at
right (tan shirt, gray pants, pinkish skin — rendered correctly as a lit 3D mesh with vertex
colors). Two black chevron bars at bottom-left and bottom-right (navigation elements). The layout
matches the expected song-select UI. No song name text is readable (text meshes white-on-white or
off-screen). Character geometry rendering is clearly working.

### 07_f0360_song_selected.png — Post-`@320:down` + `@350:select_highlighted_node` (song selected, transition)
Nearly identical to 06 with the song-list rows shifted slightly (cursor moved down one). Character
pose unchanged. The scroll/highlight transition registered in the geometry arrangement. Same
rendering quality as 06.

### 08_f0500_meta_game_transition.png — Meta→game transition (loading vignette)
**Solid red, full-screen.** The interstitial vignette mesh (a red full-screen quad drawn during
the meta→game transition) covers the entire viewport. This is expected behavior — RB3 uses a red
fade-out/fade-in as a screen-transition vignette. No geometry behind it is visible. Mesh count
drops sharply (see log: ~537 → small count), consistent with "loading" scene state.

### 09_f0700_load_mid.png — LoadSong mid-load
**Solid red.** The loading vignette persists through the full song-load phase. The song is being
loaded in background (audio path, MOGG decryption, MIDI parsing all running), but the renderer
sees only the full-screen red vignette. No loading-bar or loading-screen elements are visible
(they would appear behind the vignette in a real game flow, but here the vignette stays up
because the headless screen-transition gate does not clear).

### 10_f1100_kready.png — `Game::mLoadState = kReady` reached
**Solid red.** Same as 08–09. `kReady` (audio loaded) is confirmed in the terminal log, but the
rendering state has not advanced past the vignette. The screen-flow is stuck in the
`InterstitialPanel` waiting for `transition_camshot_done` (the known V1 blocker documented in
SONG_LOAD_ACHIEVED.md).

### 11_f1400_pre_exit.png — Pre-exit (frame budget expiry)
**Solid red.** No change from frames 08–10. The run exits cleanly at the frame budget; the game
remains on the loading vignette throughout the song-load phase.

---

## Overall rendering health: PARTIAL

The render path is live and producing real GPU output:
- 3D geometry draws (400–430 k triangles per frame during menu phases).
- UI panel geometry, 3D character meshes, and background props are all present and correctly
  shaped.
- The frame loop, GPU submit, headless readback, and PNG encode all work correctly.

However, multiple issues prevent correct visual output:

1. **No texture sampling** — `BandRnd::DrawMesh` sets `useTexture = 0.0f` unconditionally and
   binds only white/black placeholder views. All surfaces render with bare vertex/material colors.
   `RndTex::SyncBitmap()` is a no-op, so no bitmap data ever reaches the GPU. This is the
   single biggest visual deficit: the entire game looks untextured.

2. **White-out on hub/quickplay screens** — The main hub and party-quickplay screens render as
   blank white because the background geometry is a large untextured white plane, and
   `RndCam::sCurrent` at those frames points into the plane from close range. `RndText` meshes
   (labels, song titles, button text) render white vertices on white backgrounds and are
   invisible. Once text rendering or textures are wired up this should resolve.

3. **Loading vignette never exits** — Frames 08–11 are solid red because the headless
   interstitial screen-flow gate (`InterstitialPanel::Exiting()` waiting for
   `transition_camshot_done`) never fires. This is the known V1 follow-on blocker from
   SONG_LOAD_ACHIEVED.md — the vignette needs a HX_NATIVE skip path to let the game-screen
   draw. Not a rendering bug per se, but prevents any game-screen screenshot.

---

## Rendering issues needing follow-up (ranked)

| # | Issue | Revealed by | Required fix |
|---|-------|-------------|--------------|
| 1 | **No texture sampling** — `useTexture=0` hardcoded; `RndTex::SyncBitmap()` no-op | 01, 06, 07 (all geometry flat-colored) | Implement `RndTex::SyncBitmap()` to upload `.milo` bitmap data to a `wgpu::Texture`; bind actual texture views in `MakeMaterialBindGroup`; set `useTexture = 1.0f` when a valid texture is bound. |
| 2 | **Camera wrong on some screens** — scene cam not selected before `BeginFrame` | 01 (oblique intro angle), 04–05 (white-out) | `BandRnd::BeginDrawing()` uses `RndCam::sCurrent` which may not be set yet for the first few frames. The game calls `mDefaultCam->Select()` in `Rnd::BeginDrawing()` which we bypass. Consider calling the base `Rnd::BeginDrawing()` first (for its camera + timer bookkeeping), then the GPU frame setup. |
| 3 | **Loading vignette blocks game-screen** — `InterstitialPanel::Exiting()` stalls headless | 08–11 (solid red, no game HUD) | Add a HX_NATIVE skip path in `InterstitialPanel::Exiting()` (and possibly `CustomSplash`/`tv3_*` screen transitions) so the interstitial exits unconditionally in headless mode. Documented in SONG_LOAD_ACHIEVED.md as the V1 next blocker. |
