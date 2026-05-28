# Song-select (Music Library) occluder + stray SAVE panel — FIX (status item N4)

**Authored:** 2026-05-28 (N4 dispatch, Opus). Build: `native/build-native/rb3-native`,
engine pinned `cfaaa5bc`. Reproducer: the standard v34/v37 RB3_GAME_INPUT script.
Evidence: `screenshots/v37-songselect/` (fresh boot→gameplay sweep, exit 0).

## TL;DR

The song-select screen had **two independent defects**, both now fixed:

1. **Large grey occluder box** (upper center-right) = the song's **album-art display
   rendering as a solid grey rectangle** because the placeholder texture
   `ui/image/blank_album_art_keep.png` (and the rest of `ui/image/`) was **absent
   from the `orig-assets/extracted` data dir**. With no diffuse texture the album
   meshes (`album.mesh` / `album_frame01.mesh` / `bone_album*.mesh`, `mat=…` /
   `tex=(none)`) drew opaque grey.
   **Fix:** populated `orig-assets/extracted/ui/image/` (7 files, 268K) from
   `orig-assets/extracted-xbox-full/ui/image/`. The region now shows the retail
   teal "?" blank-album-art placeholder.

2. **Overlapping SAVE / "Are you…" panel** (right edge, with a gamer-card widget) =
   the **`song_select_details` sub-pane left showing while `details_mode` is off**.
   This pane (song-detail / leaderboard / "view gamer card" + save widgets) is only
   meant to be visible after the player presses Options on a song. Retail hides it
   via the `details_hide.trg` PropAnim whose terminal `showing=FALSE` keyframe does
   not apply natively (same PropAnim/transform-keyframe gap class seen elsewhere),
   so it was left drawing over the right of the list.
   **Fix:** glue-layer enforcement of the retail invariant (details pane visible iff
   `details_mode`).

Boot / main-menu / song-load / gameplay / venue / HUD all still render and the
24000-frame run exits 0 — no regression.

## Root cause #1 — grey occluder = untextured album art (DATA)

- The Music Library right side shows the highlighted song's album cover. For an
  unmounted/blank song the screen's `refresh_top` handler
  (`ui/song_select/song_select.dta:1432`) sets
  `album_art.pic → tex_file "ui/image/blank_album_art_keep.png"`.
- `orig-assets/extracted/ui/` had **no `image/` subdir** (it exists in
  `extracted-xbox-full/ui/image/`). So the placeholder texture could not load and
  `album.mat`'s diffuse tex stayed null → the album meshes rendered solid grey.
- Confirmed empirically: the box is composed of stacked meshes at world
  ~`(261…277, 0, 58…209)` (`album.mesh`, `album_frame01.mesh`, `bone_album.mesh`,
  `bone_album_group.mesh`), all `tex=(none)`. Hiding all four together removed the
  box; hiding any single one left the others behind it. Providing `ui/image/`
  replaced the grey with the proper teal "?" placeholder.

**Fix (data):** `cp -rn orig-assets/extracted-xbox-full/ui/image
orig-assets/extracted/ui/`. Files added:
`gen/{blank_album_art_keep,canvas_keep,song_select_header_keep,song_select_random_keep,song_select_setlist_keep}.png_xbox`,
`gen/generic_band_logo_keep.{bmp,png}_xbox`.
The `orig-assets/extracted/` tree is `.gitignore`d (local asset dir, not committed),
so this is a working-data fix — **the asset-extraction step that builds `extracted`
should include `ui/image/`** so it survives a re-extract.

## Root cause #2 — stray SAVE panel = song_select_details left showing (CODE)

- `song_select_details` is a sub-PanelDir loaded from
  `ui/song_select/gen/song_select_details.milo_xbox`. It holds the details / mini-
  leaderboard view and the "view gamer card" + save-prompt widgets ("FRIEND
  RANKINGS", the SAVE/"Are you…" dialog).
- It should be visible only while `song_select_panel.details_mode` is set
  (`show_details`/`hide_details` in `song_select.dta`). On enter the screen calls
  `hide_details`, which triggers `details_hide.trg`; the trigger's PropAnim is what
  sets `song_select_details showing=FALSE`. Natively that terminal keyframe does
  not apply, so the pane stayed `showing=1` with `details_mode=0` — drawing over
  the right of the song list.
- Confirmed via a per-frame panel dump: `details_mode=0` but
  `song_select_details.showing=1` on every song-select frame (pre-fix).

**Fix (glue, permuter-safe):**
`native/src/rb3_game_input.cpp:699-725` (`RB3GameInputPoll`). On
`song_select_screen`, once `song_select_panel` is `kUp`, if `details_mode` is off
and `song_select_details` is showing, `SetShowing(false)`. Only force-HIDEs when
details_mode is off, so opening the details view later (which sets `details_mode=1`
and runs `details_show.trg`) is unaffected. Opt-out: `RB3_NO_DETAILS_FIX=1`.
Post-fix dump: `details_mode=0  song_select_details.showing=0`.

## What renders now (`screenshots/v37-songselect/06_f0280.png`)

- "MUSIC LIBRARY" header + filter rows (`0/10`, `0/5`, `0/30★`) over the blue-grey
  list — clean, full-width, readable.
- Album-art region shows the teal "?" blank-album placeholder (retail-correct),
  **no grey occluder**.
- **No SAVE / "Are you…" panel** intruding on the right edge.
- The overshell player-slot card sits bottom-left as before.

## Regression status (`screenshots/v37-songselect/`, 24000-frame run, exit 0)

- Boot/main-hub (`01_f0007`), main menu (`04_f0120`/`05_f0200`): unchanged.
- Gameplay highway (`10_f1100`, `11_f1400`): gems + strike plate + guitar closeup +
  top-center HUD + venue all intact.
- Venue/crowd (`09_f0700`): renders as before (residual character slivers = the
  pre-existing N5 item, unchanged).
- `08_f0500` is a void/wall camera cut — the pre-existing N2 intermittent camera
  issue, present in the v34 baseline too; **not** introduced here.

## Diagnostic scaffolding left in place (env-gated, off by default)

`native/src/rb3_game_input.cpp`:
- `RB3_PANEL_DBG=1` — per-screen panel showing/state dump + `details_mode` /
  `song_select_details.showing` (the probe that localized defect #2).
- `RB3_HIDE_MESH=a,b,c` — hide named drawables in the song_select_panel dir
  (subdir search) to localize render artifacts (the probe used for defect #1).

## What remains (out of scope here)

- Album art for a **real** mounted song would show the actual cover; with the blank
  placeholder texture present the unmounted case now matches retail. If the album
  region should composite via RTT for the "etched glass" effect, that is the same
  deferred-RTT class as the documented `BandPatchMesh` no-op — not required to
  remove the occluder.
- N2 void/wall camera cuts and N5 crowd slivers are separate, untouched items.
