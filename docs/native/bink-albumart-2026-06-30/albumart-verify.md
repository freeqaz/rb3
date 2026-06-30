# Album-art covers — independent VERIFY (adversarial)

**Date:** 2026-06-30 · worktree `converge-albumart` (branch `wt-converge-albumart`)
**Verifies:** commit `dbd27ee4` (impl) against [`album-art.md`](album-art.md) +
[`albumart-impl.md`](albumart-impl.md).
**Verdict:** **LAND_WITH_NOTES** — fix is correct + Wii-neutral; 2 minor non-blocking notes.

## What I re-derived independently

### 1. Native — covers render real per-song art ✅
Rebuilt `rb3-native` (BandSongMgr already current → binary matches source).
`song-select-capture.py` at depths 0/8/16/30/50 + a second run at 44–47:

| song (highlighted) | artist | box |
|---|---|---|
| RANDOM SONG (FunctionSortNode) | — | `?` special-node placeholder — correct |
| The Beautiful People | Marilyn Manson | real portrait cover |
| China Grove | The Doobie Brothers | real (sky) cover |
| False Alarm | The Bronx | real cover |
| Imagine | John Lennon | real (classic sky) cover |
| I Can See for Miles | The Who | real (vinyl) cover |
| I Love Rock N' Roll | Joan Jett | real cover |

6+ **distinct real covers**, no `?`, matching the retail grid layout
(`images/retail-screenshots/yt_qRagnZCIMzk_song_select_album_art.png`).

### 2. Art-less songs stay blank, no crash ✅
- `ifeelgoodalt` (= "I Got You (I Feel Good)" / James Brown): `songs.dta`
  `(album_art TRUE)` but **no** `_keep.png_xbox` source. Highlighted at depth 46 it
  shows **`blank_album_art_keep.png` (the Rock Band logo)** — the correct no-art
  fallback, NOT a wrong cover, NOT a crash. (The new cooked-path `::stat` fails →
  returns the blank literal.)
- `radarlove`: `(album_art FALSE)` → `OwnedSongSortNode::GetAlbumArtPath` returns the
  blank **directly** (never reaches the BandSongMgr stat). Blank, correct.
- `updates`: non-song, no `album_art`. Blank, correct.
- **Disambiguation:** the depth-0 `?` is NOT the song no-art image. Special nodes
  return their own placeholders (`HeaderSortNode`→`song_select_header_keep.png`,
  `SetlistSortNode`→`song_select_setlist_keep.png`, FunctionSortNode→`?`). The song
  no-art image is `blank_album_art_keep.png` = the RB logo (what `ifeelgoodalt` shows).
  The impl-doc table loosely called the song fallback "?"; behavior is correct.

### 3. Wii-neutrality ✅
`run_objdiff GetAlbumArtPath__11BandSongMgrCF6Symbol` →
**100.0% normalized (100.0% raw), 32/32 insns equal, Complete**. The whole change
is inside `#ifdef HX_NATIVE` (and `<sys/stat.h>` is HX_NATIVE-guarded), so it is
byte-neutral by construction. `BandSongMgr.cpp` is RB3-only (DC3 has no `band3/`) →
DC3 unaffected.

### 4. Code audit ✅
- Cooked path: `MakeString("%s/gen/%s.%s_%s", FileGetPath(path,0), FileGetBase(path,0),
  FileGetExt(path), plat.Str())`. `FileGetPath` uses `static_path[256]`, `FileGetBase`
  uses a **different** `my_path[256]`, `FileGetExt` returns a pointer **into** `path` —
  3 distinct buffers, **no arg-aliasing clobber** when all evaluated before the format.
  For `songs/x/x_keep.png` → `songs/x` + `/gen/` + `x_keep` + `.png` + `_xbox` =
  `songs/x/gen/x_keep.png_xbox` — exactly the provisioned symlink path.
- String lifetime: `String cooked(...)` copies the MakeString buffer; returned `path`
  is the original MakeString buffer. Only **one** extra MakeString call added; pool has
  10 buffers/thread (2KB each) → ample headroom, no clobber.
- `#ifndef __EMSCRIPTEN__` web skip correct; `#include <sys/stat.h>` guarded.
- Callers: **3** route through the fix — `OwnedSongSortNode::GetAlbumArtPath`,
  `SelectDifficultyPanel.cpp:159`, **and `TexLoadPanel.cpp:47`** (impl doc said 2; all
  benefit identically).

### 5. Web — logic confirmed, not runtime-verified ✅ (per task allowance)
`main_web.cpp:597` also forces `kPlatformXBox` → `PlatformSymbol`=`"xbox"`. So web:
skip stat → return logical `songs/x/x_keep.png` → loader rewrites to
`songs/x/gen/x_keep.png_xbox` → `server.py` W7-V4 `_find_fallback_dirs` serves it from
`extracted-xbox-full` (lines 999–1001, 1130–1140). Path + platform + server all line
up. No web build run (slow; task permits static).

### 6. Provisioning script (`tools/link-song-charts.sh`) ✅
Idempotent: re-ran twice → `linked 0 dirs + 0 files + 0 covers`. **81/81** available
covers symlinked. Worktree `orig-assets/extracted` is a symlink to the **shared**
main-repo dir, so covers land in the shared tree; symlink targets point at the stable
main-repo `extracted-xbox-full` path → survive worktree removal. Main repo sees all 81.

## Non-blocking notes
- **Web `ifeelgoodalt` edge case:** on web, an `album_art TRUE` song with no xbox
  cover (only `ifeelgoodalt`) skips the stat → loader fetches `gen/..._keep.png_xbox`
  → 404 → likely a failed-load box instead of the native RB-logo blank. Cosmetic, 1
  song, no worse than the pre-fix all-blank state. (Native handles it cleanly.)
- **Pre-existing (NOT this commit):** `link-song-charts.sh:56` `ln -s "$sd%/"` has a
  literal `%/` typo in the whole-song-dir branch; not triggered here (all dirs exist),
  out of scope for album art.

## Verdict
**LAND_WITH_NOTES.** Fix is correct, objdiff-proven Wii byte-neutral (100%), data
provisioned reproducibly + idempotently, covers render real per-song art with correct
blank fallback for art-less songs. The two notes are advisory only.
