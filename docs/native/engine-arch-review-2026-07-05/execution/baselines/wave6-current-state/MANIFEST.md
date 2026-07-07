# Wave 6 current-state screenshots (2026-07-06)

Captured from HEAD (`master`, native build `rb3-native`, built clean immediately
before capture — build succeeded, no errors). Harness: adapted from
`scripts/native/song-select-capture.py` + `scripts/native/song-end-test.py`
(headless `RB3_HTTP=1`, `/api/health` polling, `/api/input` nav verbs incl. the
real `part:`/`diff:` press verbs, `/api/screenshot`). `RB3_FIXED_CLOCK=1` set on
every run for determinism. Data dir: `orig-assets/extracted`. No source was
modified; nothing was committed.

All screens reached via one continuous nav: start -> confirm -> PLAY NOW ->
QUICKPLAY -> song_select -> down + select-highlighted -> part_difficulty ->
part:guitar + diff:expert -> game_screen -> nofail (to avoid a fail-out during
the long capture window). No crashes, aborts, hangs, or engine-log
errors/warnings were observed across any of the runs (both default-env and
`RB3_PLACEMENT_CONTRACT=1` runs completed cleanly and were torn down normally).

## Files

- `mainhub_default.png` — main hub / main menu, reached via `start`+`confirm`
  only (no quickplay nav), state `main_hub_screen` frame 89. Default env.
  Renders normally (PLAY NOW / QUICKPLAY / ROAD CHALLENGE tiles, venue-flyer
  backdrop, controller-connect prompts). No anomalies.

- `songselect_default.png` — song select / music library, state
  `song_select_screen` frame 245. Default env. Renders normally (song list,
  album art panel, header). No anomalies.

- `partdiff_default.png` — part/difficulty select screen, state
  `part_difficulty_screen` frame 390. Default env. **ANOMALOUS**: the frame
  shows a restaurant-menu/flyer background texture with several large solid
  black rectangular patches overlaid where UI content (band-member icons /
  difficulty tiers / show poster art, going by the retail layout) would be
  expected — no part-select or difficulty-gauge widgets are visible in this
  capture. Whether this is a mid-transition frame (captured too early after
  the screen-name flip) or a genuine missing-UI-element bug was not
  investigated further; flagging for review.

- `gameplay_default_1.png` — gameplay early, `game_screen`, songMs≈3015
  (frame 2224). Default env. **ANOMALOUS**: the entire 3D scene (highway,
  venue backdrop, character) renders in grayscale/desaturated
  black-and-white; only the 2D HUD overlay (overdrive/energy meter, score
  bar) is in color. The same songMs (~3032) under
  `RB3_PLACEMENT_CONTRACT=1` (`gameplay_placement_on_1.png`) shows the
  identical grayscale look, so this is not caused by the placement-contract
  flag — it reproduces at this early-song timepoint regardless.

- `gameplay_default_2.png` — gameplay mid, `game_screen`, songMs≈20153
  (frame 4255). Default env. Full color venue renders correctly (blue
  chain-link backdrop, cymbals, practice-space set dressing). **ANOMALOUS**:
  the visible band member's head renders as a solid flat-black silhouette
  with no face texture/features, while the body/arms are pale and textured
  normally. Same timepoint under `RB3_PLACEMENT_CONTRACT=1`
  (`gameplay_placement_on_2.png`, songMs≈20149) shows the same black-head
  character in the same pose/venue — not specific to the placement flag.

- `gameplay_default_3.png` — gameplay later, `game_screen`, songMs≈45146
  (frame 8120). Default env. Full color, wood-paneled venue; singer character
  visible mid-animation (open mouth, spiked hair) with face fully textured/
  shaded (no black-silhouette issue here). No obvious rendering anomaly beyond
  the pose itself looking exaggerated, which may just be normal singing
  animation.

- `band_default.png` — band-visible wide/establishing frame, `game_screen`,
  songMs≈70175 (frame 11694). Default env. Purple-lit venue clearly showing
  two band members (mic-stand vocalist + guitarist stage-right) alongside the
  highway. No anomalies; both characters render fully textured.

- `gameplay_placement_on_1.png` — gameplay early, `game_screen`, songMs≈3032
  (frame 2335), env `RB3_PLACEMENT_CONTRACT=1`. Same grayscale/desaturated
  scene as `gameplay_default_1.png` (see note above — reproduces with or
  without the flag).

- `gameplay_placement_on_2.png` — gameplay mid, `game_screen`, songMs≈20149
  (frame 4325), env `RB3_PLACEMENT_CONTRACT=1`. Same black-head-silhouette
  character as `gameplay_default_2.png` (see note above — reproduces with or
  without the flag). No visible drum/crowd placement difference was noticed
  by eye between this and the default-env frame at the matching timepoint,
  but no pixel-diff was performed (out of scope here — visual-only capture).

## Notes / caveats

- Song under test was whatever QUICKPLAY's default pick resolved to given the
  nav script (not pinned by name); the same song was used for the default and
  `RB3_PLACEMENT_CONTRACT=1` runs since both used the identical nav script
  against the same asset library, but song identity was not explicitly
  verified to be byte-identical between the two separate process runs.
- All screenshots captured successfully on first attempt except
  `mainhub_default.png`, which required a second, dedicated capture: the
  first attempt (in the same run as the rest) used a loose "frame >= 5"
  fallback predicate and accidentally captured `splash_screen` instead of
  `main_hub_screen`; this was caught and corrected before finalizing the
  manifest.
- Engine logs for both main capture runs are at
  `/tmp/rb3-wave6-default-48327.log` and
  `/tmp/rb3-wave6-placement_on-53395.log` (no errors/warnings found via grep).
