# W4.3-C2a — song-select difficulty-ratings panel background

KEY=W4.3-C2a. Engine working tree 44716f4 (task header pin). Build dir: reuse
`native/build-agent-W4.3-C2` (own lane continuation of W4.3-C2, that agent is done;
reflink-copy baked FetchContent paths → abandoned a separate dir).

## Goal
The retail song-select sidebar shows a dark rounded backing behind the instrument
difficulty dots (`difficulty_bg*`/`raitings_bg`/`details_background`). Native draws
the `.idd` grid floating with no backing. Determine WHY (never-submitted vs
submitted-and-dropped), then game-side fix flag-first default-OFF.

## Grounding correction to W4.3-C2/STATUS.md
STATUS claimed the backing lives ONLY in the sibling `song_select_details.milo`.
FALSE: `strings song_select.milo_xbox` shows the MAIN panel milo ALSO contains
`difficulty_bg01-07.mesh`, `raitings_bg.mesh`, `details_background.mat`,
`live_diffs.grp`, and the `.idd`s. The prior `mDir->Find(name,false)` returned
`<not found>` because that Find does NOT recurse subdirs (`ObjectDir::FindObject(name,false)`);
the backing meshes live in an inlined subdir. `song_select_details` (dta token,
`song_select.dta:50,133,161`) is the FULL details overlay (album/description/stars/
review) shown on `show_details`, hidden in quick-view — A10 says do NOT blanket-show it.

## File ranges (re-derived by symbol on current tree, SongSelectPanel.cpp)
- includes + fwd decl: L21-29 (added rndobj/Mesh.h, cstring, `C2CensusDir` fwd decl)
- `FinishLoad()`: L44-77 (added RB3_SS_CENSUS call at L71-75)
- `C2NameOfInterest` + `C2CensusDir`: L79-165 (new census walker, HX_NATIVE + getenv gated)
- `SetMiniLeaderboardGroupShowing`: L167-200 (unchanged prior RB3_SS_GRPLOG probe)

## Stages
1. CENSUS (RB3_SS_CENSUS): ObjDirItr recursive walk of mDir + inlined subdirs →
   locate difficulty_bg*/raitings_bg/details_background, their subdir, showing
   state, and containing group. Decide never-submitted vs submitted-and-dropped.
2. A10 sub-panel content census: enumerate what song_select_details would draw;
   hide-list natively-opaque quads; do NOT blanket-show.
3. FIX (flag-first default-OFF): game-side. Gates: draw-order evidence backing
   composites BEHIND grid; grid-glyph ROI intact; before/after vs retail ref;
   drawlog 792 flag-OFF byte-identical.

## Match-neutrality
All edits `#ifdef HX_NATIVE` + `getenv`-gated → Wii decomp byte-identical.
