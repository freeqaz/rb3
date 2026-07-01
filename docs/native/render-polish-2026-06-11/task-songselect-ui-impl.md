# task-songselect-ui-impl — Clean up the song-select UI clutter (3 defects)

**Issue key:** `songselect-ui`  ·  **Status:** DONE (all 3 verified)  ·  **Wave:** 5 implementer
**Implementer:** opus, 2026-06-15  ·  **Ports used:** 9221–9229

---

## SUMMARY

Fixed the three song-select render-clutter defects from the wave-3/4 reviews, all
native-only (`#ifdef HX_NATIVE`), Wii byte-identical, no engine change:

1. **Garbage header digits** (`...SORTED BY SONG NAME` + a random int like
   `1843121372` / `1056964736` / `2124022576`) — GONE; the header now ends at the
   sort name with the career-star total reading `0` (correct offline-clean state).
2. **FRIEND RANKINGS overlay** drawing over the difficulty grid — GONE; the
   difficulty grid (NO REVIEW + per-instrument dots) renders clean.
3. **Grey album-art box** obscuring the grid when a song is highlighted — GONE;
   now shows the shipped ROCK BAND blank-art placeholder instead of flat grey.

The wave-2 diff-grid icon-centering fix is preserved. Boot→gameplay still green.

---

## ROOT CAUSES (all three independently proven via /api/dta/eval probes)

### 1. Garbage header digits — uninitialized cached-star total

The header label is `status.lbl`, set by `AppLabel::SetMusicLibraryStatus()` →
`MusicLibrary::GetStatusText()` → `MakeString("Viewing ALL %i SONGS, Sorted By
%s", count, sortName)`. That string is correct (no extra format arg). The garbage
is the **`careerstars.sd` star-display** (`refresh_summary` →
`{careerstars.sd set_values {music_library header_career_stars} {music_library
header_possible_stars}}`), positioned in the header row.

`{music_library header_career_stars}` returned **1843121372** (a different junk
int each boot). `MusicLibrary::UpdateHeaderData()` (MusicLibrary.cpp:1854) sets
`mHeaderCareerStars = mgr->GetCachedTotalStars(s)` →
`SongStatusMgr::mCachedTotalStars[ty]` (SongStatusMgr.cpp:760). That array is a
plain inline `int[11]` member (SongStatusMgr.h:292) that the **matched** ctor
(SongStatusMgr.cpp:458, objdiff **100%**) leaves UNINITIALIZED — on the Wii it is
populated by the profile/save-load path (`Clear()` + `UpdateCachedTotalStars`)
before the header ever reads it. Native boots profile-less, so the array stays
garbage. (`header_possible_stars`=410=82×5 is a LEGITIMATE value — only the stars
were junk.)

### 2. FRIEND RANKINGS overlay — env-alpha hide not honored natively

The mini-leaderboard panel is `live_lb.grp` (contains the "FRIEND RANKINGS" title
label `mTitleLabel`, set by `AppMiniLeaderboardDisplay::UpdateLeaderboard` →
`SetTextToken(mini_leaderboards_title_friends)`). It is authored **showing-by-
default** in `song_select.milo`. The difficulty grid is `live_diffs.grp`.

Probe at depth-8 (song highlighted): `live_lb.grp showing`=**1** AND
`live_diffs.grp showing`=**1** — both visible, overlapping. `leaderboard.mld
is_ready`=0, `has_rows`=0 (correctly no data offline).

The rotation logic (`SongSelectPanel::Poll`, SongSelectPanel.cpp:115) only swaps
the leaderboard in when `IsReady() && HasRows()` (never offline). The hide is
driven by `set_mini_leaderboard_showing 0` → `leaderboard_hide.trg` (an
EventTrigger anim that fades `live_lb.env` **alpha**). The native renderer doesn't
honor that env-alpha fade for hide, so the group stays fully visible. **Proven
decisive:** `{{song_select_panel find live_lb.grp} set_showing FALSE}` over
`/api/dta/eval` instantly hides the overlay and reveals a clean grid
(`/tmp/rp5-songselect-ui/direct_hide.png`); a `set_mini_leaderboard_showing 0`
trigger alone leaves `showing`=1 (`manual_hide.png`).

Secondary: `AppMiniLeaderboardDisplay::Poll` after 1s calls
`UpdateLeaderboardOnline` → `StartEnumerate()` → `kEnumState2` (waiting on a
server that never replies offline) — keeps the panel in the loading/faded-in
state too.

### 3. Grey album-art box — no per-song art textures in the native asset set

Song node → `OwnedSongSortNode::GetAlbumArtPath` (HasAlbumArt true) →
`BandSongMgr::GetAlbumArtPath` → `SongFilePath(s, "_keep.png", true)` →
e.g. `songs/thebeautifulpeople/thebeautifulpeople_keep.png`. **That file does not
exist** — `find orig-assets/extracted/songs -iname "*_keep.png*"` returns 0. The
DTA does `{album_art.pic set tex_file {$item album_art_path}}`; the texture load
fails → flat grey box (RGB ~177). (At header/setlist nodes the art path is
`ui/image/song_select_random_keep.png` / `blank_album_art_keep.png`, which DO
exist → the "?" placeholder there is correct, not a bug.)

---

## WHAT CHANGED (files + why) — all `#ifdef HX_NATIVE`, rb3-src only

| File | Change |
|---|---|
| `src/band3/meta_band/SongStatusMgr.cpp` | Ctor: zero `mCachedTotalScores/DiscScores/Stars[11]` (mirrors `Clear()`). Kills the junk header star total. |
| `src/band3/meta_band/AppMiniLeaderboardDisplay.cpp` | `UpdateLeaderboardOnline`: when `!TheRockCentral.IsOnline()`, `ResultFailure()` (fade out) instead of `StartEnumerate()` that hangs in `kEnumState2`. |
| `src/band3/meta_band/SongSelectPanel.cpp` + `.h` | New `SetMiniLeaderboardGroupShowing(bool)` directly toggles `live_lb.grp` vs `live_diffs.grp` `SetShowing`. Called: hide at `FinishLoad` + in `RestartLeaderboardTimer`/`CancelLeaderboardTimer`; show in the `Poll` ready-path (so ONLINE still reveals it). Added `#include "rndobj/Group.h"`. |
| `src/band3/meta_band/BandSongMgr.cpp` | `GetAlbumArtPath`: `::stat()` the resolved `_keep.png` path; if absent, return `ui/image/blank_album_art_keep.png`. Added `#include <sys/stat.h>` (HX_NATIVE). |

Design notes:
- The FRIEND-RANKINGS fix is written to be ONLINE-CORRECT, not just offline: the
  group is hidden by default + on every "hide" rotation event, and explicitly
  shown only in the `Poll` `IsReady() && HasRows()` path. Offline that show-path
  never fires, so it stays hidden; online it reveals exactly when scores load.
- The grey-box fix is centralized in `BandSongMgr::GetAlbumArtPath` so it also
  covers `TexLoadPanel` / `SelectDifficultyPanel` callers, not just song-select.
  Real art (if ever shipped) still resolves to the song path.

---

## BRANCH + COMMITS

- **rb3 worktree branch:** `wt-task-songselect-ui`
  (`/home/free/code/milohax/rb3/.claude/worktrees/task-songselect-ui`)
- **Commit:** `a9e39150668e41229bc10fcca01275e4d90e6cda`
  `fix(native): clean up song-select UI clutter (header digits, FRIEND RANKINGS overlay, grey album box)`
- **Engine commits:** NONE (rb3-src only; `MILO_ENGINE_PIN` unchanged at `58254f7`).
- Branched from rb3 master `23428683`.

---

## EVIDENCE (`/tmp/rp5-songselect-ui/`)

| Path | What |
|---|---|
| `before/native_depth_{00,08,16}.png` | BEFORE (baseline worktree binary, pre-fix) |
| `after2/native_depth_{00,08,16}.png` | AFTER (all 3 fixes) |
| `sidebyside_depth08.png` | **money shot**: before (L) garbage+FRIEND RANKINGS+grey vs after (R) clean |
| `header_zoom_before.png` / `header_zoom_after.png` | header: `...SONG NAME1056964736` → `...SONG NAME 0` |
| `albumbox_zoom_d08.png` | the flat grey box (before) |
| `friendrank_zoom2_after.png` | FRIEND RANKINGS still over the grid (intermediate, fix #1+#3 only) |
| `diffgrid_zoom_after.png` | wave-2 icon-on-dot-row centering preserved |
| `direct_hide.png` / `manual_hide.png` | the A/B that proved env-alpha hide is the FRIEND-RANKINGS root cause |
| `gameplay_wt/01_song_select.png`, `07_playing.png` | worktree binary boot→gameplay (no crash; clean song-select; gameplay renders) |

---

## VERIFICATION RESULTS — `verified=true`

**(1) Garbage digits — PASS.** `header_career_stars` was 1843121372 (probe) /
1056964736 (screenshot). After: header reads "VIEWING ALL 83 SONGS, SORTED BY
SONG NAME 0" — the star total is a clean `0` (`header_zoom_after.png`).

**(2) FRIEND RANKINGS — PASS.** Gone at depths 0, 8, 16 (`after2/*`). Depth-8
difficulty grid renders clean (NO REVIEW + dots, no overlay). Online path
preserved (show only in `Poll` ready-path).

**(3) Grey album box — PASS.** Depth-8 song highlight now shows the ROCK BAND
blank-art placeholder, not flat grey (`after2/native_depth_08.png` vs
`albumbox_zoom_d08.png`).

**No regression — PASS.** (a) wave-2 diff-grid icon-on-dot centering preserved
(`diffgrid_zoom_after.png`). (b) Full boot→main_hub→song_select→part_difficulty→
gameplay green on the worktree binary (`keyboard-to-gameplay.py --port 9223`),
song playing, 0 crashes/asserts; gameplay highway/gems/band/venue render
(`gameplay_wt/07_playing.png`). (c) Song-list / main-hub text unshifted.

**Wii byte-identical — CONFIRMED (`wiiByteIdentical=true`).** All 5 edits behind
`#ifdef HX_NATIVE`. Touched-function objdiff (worktree build vs target):
- `SongStatusMgr` ctor `__ct__13SongStatusMgr...` — **100.0**
- `BandSongMgr::GetAlbumArtPath` `...11BandSongMgrCF6Symbol` — **100.0**
- `AppMiniLeaderboardDisplay::UpdateLeaderboardOnline` `...25...Fi` — **100.0**
- `SongSelectPanel::FinishLoad/RestartLeaderboardTimer/CancelLeaderboardTimer` — **100.0** each
- `SongSelectPanel::Poll` — **98.84393** == master baseline (re-checked on master, identical).

---

## LANDING NOTES (for the orchestrator)

- **Cherry-pick `a9e39150` from `wt-task-songselect-ui`.** rb3-src only; NO engine
  commit, NO `MILO_ENGINE_PIN` bump.
- **5 files, all `band3/meta_band/`:** `SongStatusMgr.cpp`, `AppMiniLeaderboard
  Display.cpp`, `SongSelectPanel.cpp`, `SongSelectPanel.h`, `BandSongMgr.cpp`.
- **Conflict surface:** none expected with other wave-5 tasks (none touch these
  meta_band files). Exact regions:
  - `SongStatusMgr.cpp:458-462` ctor body (append a `#ifdef HX_NATIVE` zero-loop).
  - `AppMiniLeaderboardDisplay.cpp:86-91` `UpdateLeaderboardOnline` prologue
    (prepend a `#ifdef HX_NATIVE` early-return). `net_band/RockCentral.h` already
    included.
  - `SongSelectPanel.cpp`: include block (+`rndobj/Group.h`); `FinishLoad`
    (~line 50, after `unk50` set); new `SetMiniLeaderboardGroupShowing` def after
    `FinishLoad`; `Poll` ready-branch (~line 145); `RestartLeaderboardTimer`
    (~line 157); `CancelLeaderboardTimer` (~line 165). `SongSelectPanel.h`: one
    `#ifdef HX_NATIVE` method decl after `CancelLeaderboardTimer`.
  - `BandSongMgr.cpp`: include block (+`<sys/stat.h>` under HX_NATIVE);
    `GetAlbumArtPath` (~line 341).
- **No opt-out env** — fixes are unconditional under HX_NATIVE (all are corrective,
  not behavioral toggles). If a regression is ever suspected, the FRIEND-RANKINGS
  show-path is the only online-affecting change and is gated to `IsReady() &&
  HasRows()`.
- After landing, re-confirm `SongSelectPanel::Poll` stays 98.84393 if a full
  report is regenerated (it's HX_NATIVE-gated, so it will).

## Follow-ups / out of scope (none are regressions)
- None deferred — all three target defects fixed. The depth-0 "?" placeholder is
  the correct authored no-art texture for header/random nodes, not a bug.
- Pre-existing (other wave-5 backlog owners): gameplay venue pink/red wash
  (`06_game_screen` clear-color transient), char pose-fling. Not touched here.
