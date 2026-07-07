# W4.3-C2a STATUS — song-select difficulty-ratings panel background

VERDICT: **DIAGNOSED — no faithful game-side backing fix ships.** The named backing
meshes are never-submitted AND belong to a different (drill-in) view; the xbox-360-ARK
`song_select.milo` has no valid quick-view sidebar difficulty backing to borrow.

Engine working tree 44716f4. Build dir: `native/build-agent-W4.3-C2` (own lane
continuation of the finished W4.3-C2). All edits `#ifdef HX_NATIVE` + `getenv`-gated →
Wii decomp byte-identical; flag-OFF runtime is byte-identical to pre-edit (drawlog-792
gate satisfied by construction — none of the added code is reachable with flags unset).
Harness: `c2a_probe.py` (pgid-only cleanup). Census artifact: `census.txt` (386 lines).
Captures: `/tmp/wave13-c2a/{ss2,diag,fix,lbbg}_scrolled.png`.

## The defect (real, precisely characterized)
Quick-view song-select: the difficulty grid (`live_diffs.grp` → 10 `.idd`
InstrumentDifficultyDisplays + `pro.lbl`/`basic.lbl`, main dir, showing=1) draws its
instrument dots FLOATING over the venue backdrop (character legs / "no on door" text
bleed through). Retail (`yt_qRagnZCIMzk_song_select_diff_ratings.png`) shows a dark
backing panel behind the right sidebar. Native has no backing there.

## Census (ObjDirItr recursive walk of mDir + nested PanelDirs; RB3_SS_CENSUS)
Correction to W4.3-C2/STATUS.md: the backing is NOT in a separately-loaded sibling
file. `song_select_details` is a **nested PanelDir OBJECT** inside the main
`song_select.milo` dir (`ObjectDir::FindObject(name,false)` doesn't recurse into it,
which is why the prior direct `Find` returned `<not found>`). Structure:

- Main dir `all.grp`(show) → `right_side.grp`(show) → `right_side_song.grp`(show) →
  `live_diffs.grp`(show, the quick-view grid), `live_lb.grp`(hidden),
  `song_review.grp`, `help_bg_rating.mesh`(**show=0**), `ps3_storelogo.grp`.
  `right_side.grp` also holds `leaderboards_bg.mesh`(**show=0**).
  **No difficulty backing mesh in the main dir is showing.**
- `song_select_details` **PanelDir = show=0** (quick-view: `details_mode` defaults
  FALSE → `hide_details`, `song_select.dta:19,64-69`). Inside it (each authored
  show=1): `difficulty_bg`+`difficulty_bg01..07.mesh`, `raitings_bg.mesh`,
  `details_songscores_bg*.mesh`, `details_background.mat`.

### Never-submitted vs submitted-and-dropped
**NEVER-SUBMITTED.** The backing meshes are authored show=1 but their container
PanelDir (`song_select_details`) is show=0, so nothing in it is drawn in quick-view.
Not a render guard/alpha/depth drop — the whole sub-panel is hidden.

## Why the coordinator's "show the sub-panel backing" does NOT work (3 proofs)
A10 was right to require the content census + z/compositing guard BEFORE showing.
1. **Entangled (A10 confirmed):** `difficulty_bg04-07` live in `basicstars.grp`
   ALONGSIDE `basic_easy/medium/hard/expert.sd` StarDisplays + percent labels;
   `difficulty_bg`+`bg01-03` live in `prostars.grp` with the pro StarDisplays;
   `raitings_bg` in `rating.grp`; `details_songscores_bg*` in
   `details_bottom.grp`/`performance.grp`. There is NO dedicated backing group — the
   backing is the details-page STAR-BREAKDOWN backing, interleaved with drill-in
   content. Blanket-show double-draws the whole details page (buttons/labels/lists);
   the whole-panel diag (`RB3_SS_DETAILS_DIAG`) produced garbled help-bar text overlap.
2. **Wrong view / no effect:** force-showing the PanelDir (whole OR a surgical
   backing-only per-mesh show) changed the difficulty-grid ROI (x905-1270,y385-600)
   brightness by <1% — **68.75 (no-fix) vs 68.25 (whole panel) vs 68.11 (backing-only)
   = noise.** The backing is positioned/animated for the details drill-in layout
   (revealed via `details_show.trg`), not the quick-view sidebar; it does not
   composite behind the live_diffs grid.
3. **Only main-dir right-side candidate is wrong:** force-showing `leaderboards_bg.mesh`
   + `help_bg_rating.mesh` (`RB3_SS_LBBG_DIAG`) draws a light GLASS leaderboard frame
   low-right (ROI 68.75→72.07 — BRIGHTER, not a dark backing) — it's the
   mini-leaderboard's frame, wrong position and wrong tone. Engine has no refraction
   material path, but no showing=1 right-side difficulty backing is being dropped
   either (the only showing=1 refraction meshes are header/bottom, not the sidebar).

## Conclusion
The xbox-360-ARK `song_select.milo` carries a difficulty backing ONLY inside the
details drill-in page. The quick-view sidebar has no backing mesh in these assets; the
retail Wii reference's dark panel is a Wii-vs-360 layout difference. A faithful
game-side fix cannot borrow a correct backing — it would require authoring a NEW backing
quad behind `live_diffs.grp`, a UI-authoring task out of A10's low-priority remit and
below the campaign's faithfulness bar. No fix landed.

## Gates (A10 pre-registered)
- Draw-order evidence that a backing composites BEHIND the grid: **NOT ACHIEVABLE** —
  no asset backing lands behind the grid (proofs 2+3). Gate correctly BLOCKS a ship.
- Grid-glyph ROI intact: trivially intact (no fix → grid unchanged).
- Before/after vs retail for E1: `ss2_scrolled.png` (native no-backing) vs
  `images/retail-screenshots/yt_qRagnZCIMzk_song_select_diff_ratings.png` (retail backing).
- Drawlog 792 flag-OFF unchanged: satisfied by construction (all additions getenv-gated,
  uncalled with flags unset).

## Follow-up (for coordinator, if parity is later prioritized)
Authoring a new opaque/tinted backing quad in `right_side.grp` behind `live_diffs.grp`
(size/pos matched to the grid) is the only route to the retail look — an asset/UI edit,
not an engine or asset-borrow fix. Left un-actioned (low priority, faithfulness bar).

## Files touched (rb3; probes only, no fix — getenv-gated, HX_NATIVE, match-neutral)
- `src/band3/meta_band/SongSelectPanel.cpp` — added `C2NameOfInterest`/`C2CensusDir`
  recursive census (RB3_SS_CENSUS) + two verdict diagnostics (RB3_SS_DETAILS_DIAG,
  RB3_SS_LBBG_DIAG). Includes: `rndobj/Mesh.h`, `<cstring>`.
- `docs/.../execution/W4.3-C2a/{PLAN,STATUS}.md`, `c2a_probe.py`, `census.txt`.
