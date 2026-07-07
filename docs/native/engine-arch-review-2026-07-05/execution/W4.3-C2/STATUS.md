# W4.3-C2 STATUS — song-library sidebar (C2c > C2a ≈ C2b)

Engine pin 146fd19 (unchanged). Build dir: `native/build-agent-W4.3-C2` (own, clang).
Probe binary boots to song_select on "25 or 6 to 4" via `c2_probe.py`.
All edits are getenv-gated + `#ifdef HX_NATIVE` → Wii decomp match byte-identical (verified in diff).

Harness: `execution/W4.3-C2/c2_probe.py` (RB3_RANKTIER_LOG + RB3_SS_GRPLOG).
Captures: `/tmp/wave12-c2/{ss_list,ss_diffpanel,partdiff}.png`; crops
`/tmp/c2_sidebar_crop.png`, `/tmp/c2_partdiff_crop.png`; log `/tmp/wave12-c2/ranktier.log`.

---

## C2c — WRONG DIFFICULTY RATINGS + devil (BINDING, A8) — RESOLVED: DATA CORRECT, NOT A BUG

Widget path proven: `orig-assets/extracted/ui/ui_objects.dta:408,412,432,461`
`InstrumentDifficultyDisplay.set_rank/set_song` → `{song_mgr rank_tier $rank $track_type}` /
`{song_mgr rank_tier_for_song $song $part}` → `BandSongMgr.cpp:966/969` HANDLE_EXPR →
`RankTier` (:380) / `GetPartDifficulty`(:846)→`RankTier`(:851). So every difficulty dot goes
through `RankTier`, exactly as A8 stated.

**One-log-line result (856 calls, boot to the "25 or 6 to 4" sidebar):**
- EVERY call: `found=1`, `ntiers=7`. std::find NEVER misses; mTierRanges NEVER empty.
- Tiers are monotonic in rank and evenly distributed over the library:
  tier histogram `0:150 1:110 2:117 3:115 4:122 5:118 6:124` — the exact equal-count
  bucketing `ContentDone` (:145-172) builds (splits sorted songs into `numRanks` groups).
- Highlighted "25 or 6 to 4": guitar rank=469→**tier6**, drum 499→**tier6**,
  real_guitar 485→**tier6**, bass 393→5, keys 416→5, real_bass 412→5, vocals 315→4,
  real_keys 384→4. Matches the on-screen "guitar/pro-guitar/drums = all-devil".

**Both A8 probes REFUTED:** (i) end()-deref garbage — never (found=1 always);
(ii) empty mTierRanges → −1 — never (ntiers=7 always). RankTier data is correct.

**Devil semantics:** `song_groupings/rank` (`config/band_keep.dta:59-95`) = header + 7 tiers →
`Size()-1 = numRanks = 7`; tier index 6 = `generic_tier9` = the top/"Impossible" tier. The
tier-6 glyph is a row of red devil-heads and it **renders correctly** (recognizable skulls in
`c2_sidebar_crop.png` and the partdiff `c2_partdiff_crop.png`) — the "red scribble" impression
is just 6 small skull glyphs at UI scale. NO atlas/CellDiff corruption (the separate
sub-finding the coordinator flagged does not reproduce; devil glyph is intact).

**Root of the visual discrepancy = library SIZE, not a code bug.** RB3's difficulty display is
**relative** (equal-count bucketing over the *loaded* library). Native loads the full 83-song
on-disc library ("VIEWING ALL 83 SONGS"), which is the stock Wii disc with no DLC and skews
hard (AAA rock/metal). The hardest ~1/7 (~12 songs) therefore land in tier 6 = devil; "25 or 6
to 4" (a notoriously hard guitar/solo chart) is genuinely in that set. The retail reference
`yt_qRagnZCIMzk_song_select_diff_ratings.png` is a **587-song DLC library** ("VIEWING 66 OF
587") — the SAME song buckets to a lower tier there. This is faithful behavior of the RB3
algorithm on a stock disc; **a stock Wii RB3 disc would show the same devils.** No fix landed
(nothing is broken). If the coordinator wants native to *match the 587-song screenshot*
specifically, that is a library-content question (load DLC-scale song set), not a RankTier fix.

Caveat left open (not evidence of a bug, just the one path not fully audited): the per-instrument
song *count* that seeds the bucketing depends on `HasPart`/`!IsDownload` filtering
(`ContentDone:139-145`). If native counted a different per-part song set than Wii, boundaries
would shift by ≤1 tier. Not investigated — the distribution is textbook equal-count, so no signal
of a filtering bug.

## C2a — MISSING sidebar panel background — leaderboard-hide theory REFUTED; bg is in a sibling sub-panel

Census (`RB3_SS_GRPLOG`, member walk of the two toggled groups):
- `live_lb.grp` (hidden by native `FinishLoad:64`) contains ONLY `leaderboard.mld`
  (MiniLeaderboardDisplay). **No background mesh.**
- `live_diffs.grp` (shown) contains the 10 `*.idd` InstrumentDifficultyDisplays + `pro.lbl`
  + `basic.lbl`. **No background mesh.**

⇒ The native leaderboard-hide (`SetMiniLeaderboardGroupShowing(false)`) is NOT what removes the
panel background — neither group owns it. The dark backing (`difficulty_bg*.mesh`,
`raitings_bg.mesh`, `details_background.mat`, `details_songscores_bg*`) lives in the separate
`ui/song_select/gen/song_select_details.milo_xbox` sub-panel (strings-confirmed), a sibling of
the main `song_select.milo` that owns live_diffs.grp. A direct `mDir->Find` for those bg names
returns `<not found>` (they are not children of the song_select panel dir; only `all.grp` is a
direct child). So the difficulty grid (.idd, main milo) and its intended backing (details milo)
are in different dirs, and the backing is not compositing behind the grid natively.

Next step (not done — partial return per A10): walk the `song_select_details` sub-dir
(ObjDirItr) to decide **never-submitted** (details sub-panel not instantiated/shown for the
sidebar quick-view) vs **submitted-and-dropped** (present + showing but not visible → render
guard / depth / venue-overdraw). Both are game-side-fixable; fix flag-first default-OFF.

## C2b — album-art panel OVERLAPS header row — characterized (diagnosis only)

Native `ss_diffpanel.png`: the album-art quad (Chicago) top edge overlaps the header-row "0"
score at top-right. Retail (`yt_qRagnZCIMzk_...`): album art starts BELOW the header
("318 [star]" fully visible). This is a Y-anchor / panel-origin offset — the SYS-5 360-ARK 720p
layout family (authored-anchor vs drawn-rect), same class as C4 (ticker) and the prior
MainHubPanel offsets. Fix = compare authored art-panel milo xfm vs drawn quad rect, correct the
per-panel offset game-side. Not landed (partial return).

## Files touched (rb3, probes only — no fixes; getenv-gated, HX_NATIVE, match-neutral)
- `src/band3/meta_band/BandSongMgr.cpp` — RankTier `RB3_RANKTIER_LOG` probe (C2c).
- `src/band3/meta_band/SongSelectPanel.cpp` — `RB3_SS_GRPLOG` group + bg census (C2a).
- `docs/native/engine-arch-review-2026-07-05/execution/W4.3-C2/{PLAN,STATUS}.md`, `c2_probe.py`.
