# W4.3-C2 — song-library sidebar (C2c > C2a ≈ C2b)

Engine pin 146fd19. Build dir: native/build-agent-W4.3-C2.

## Bound facts (grounding)
- C2c widget path: `orig-assets/extracted/ui/ui_objects.dta:408,412,432,461`
  `InstrumentDifficultyDisplay.set_rank/set_song` → `{song_mgr rank_tier $rank $track_type}`
  and `{song_mgr rank_tier_for_song $song $track_type_sym}`.
- HANDLE_EXPR dispatch: `BandSongMgr.cpp:966` (rank_tier), `:969` (rank_tier_for_song →
  `GetPartDifficulty` :846 → `RankTier` :851). So ALL difficulty dots go through
  `BandSongMgr::RankTier` (:380-390). A8 binding confirmed.
- `difficulty` property drives `instrument_difficulty_display.milo` + `difficulty.anim`
  (frame = tier). devil = highest/impossible frame.
- C2a/C2b widget: `SongSelectPanel.cpp` — `live_diffs.grp` (difficulty grid) vs
  `live_lb.grp` (mini-leaderboard). Native FinishLoad forces diffs-shown/lb-hidden (:64).

## Stage C2c (BINDING, first): instrument RankTier
- EDIT `src/band3/meta_band/BandSongMgr.cpp` RankTier (:380-390): add HX_NATIVE-only,
  getenv("RB3_RANKTIER_LOG")-gated probe logging (instrument, rank, found, ntiers, tier).
  Safe on end() (no deref in probe). Match-neutral (HX_NATIVE guard).
- Boot to song-select on "25 or 6 to 4", read log → data-vs-widget decision.

## Stage C2a: draw-log census on live_diffs.grp panel background (submitted-and-dropped vs never submitted).
## Stage C2b: authored milo art-panel/header rects vs drawn quads (SYS-5 360-ARK family).

## Devil-glyph atlas sub-finding: filed distinctly if tier data is correct but glyph renders as red scribble.

## C2c RESULT (BINDING, done)
Probe ran (build-agent-W4.3-C2, RB3_RANKTIER_LOG=1, boot to song_select on "25 or 6 to 4").
856 RankTier calls, ALL found=1 ntiers=7, tiers monotonic 0-6, even distribution
(150/110/117/115/122/118/124). Highlighted song: guitar rank=469->tier6, drum 499->tier6,
real_guitar 485->tier6, bass 393->5, keys 416->5, vocals 315->4. Both probes (i end-deref,
ii empty-ranges) REFUTED. song_groupings/rank (band_keep.dta:59-95) = 7 tiers, index6 =
generic_tier9 = Impossible = devil. Devil glyph RENDERS CORRECTLY (recognizable red skulls,
c2_sidebar_crop/c2_partdiff_crop). => DATA CORRECT + rendering correct. All-devil is FAITHFUL
relative equal-count bucketing over the 83-song disc library (hardest ~1/7 -> tier6). NOT a bug.

## C2a probe plan (EDIT SongSelectPanel.cpp SetMiniLeaderboardGroupShowing region, add
##   getenv("RB3_SS_GRPLOG") member-census of live_lb.grp / live_diffs.grp). Hypothesis:
##   difficulty_bg*/raitings_bg background meshes live in live_lb.grp (hidden by native
##   FinishLoad:64) -> grid floats. Assets confirmed present in song_select_details milo.
