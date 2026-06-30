# TASK B — Song-library album-art covers not loading (native + web)

**Date:** 2026-06-30 · engine pin 998b8734 · agent: album-art (Opus)
**Status:** ROOT-CAUSED + proven end-to-end. Research only — no src/engine edits landed.

## TL;DR

In `song_select`, every on-disc song's cover renders as the `?` **blank
placeholder** (`ui/image/blank_album_art_keep.png`) instead of the real album
art (retail ref shows real covers: `images/retail-screenshots/yt_qRagnZCIMzk_song_select_album_art.png`).

Two **compounding** root causes — BOTH must be fixed:

1. **DATA** — the native data dir `orig-assets/extracted/` ships each song's
   `.mid/.mogg/.milo_xbox` but **not** the per-song cover texture
   `songs/<name>/gen/<name>_keep.png_xbox`. Those textures DO exist in
   `orig-assets/extracted-xbox-full/songs/*/gen/` (81/86 native songs) and
   `orig-assets/wii-extracted/songs/*/gen/<name>_keep.png_wii`.
2. **CODE** — `BandSongMgr::GetAlbumArtPath` (`src/band3/meta_band/BandSongMgr.cpp:345-368`),
   in its `HX_NATIVE` block (lines 348-364), `stat()`s the **logical** path
   `songs/<name>/<name>_keep.png`. The cooked texture is **always** at
   `songs/<name>/gen/<name>_keep.png_xbox` (gen/ subdir + platform suffix), so
   the logical path **never exists** — the fallback fires unconditionally and
   returns the blank placeholder for **every** song, even when art is present.

It is **ALL** on-disc songs, and the box falls back to the blank placeholder
(it draws the `?`, not nothing). The blank itself loads & decodes fine, which
proves the `.png_xbox` decode + gen/_xbox loader path work on native.

## Reproduction (native)

`python3 scripts/native/song-select-capture.py` → boots to `song_select_screen`.
Right-side cover box shows the `?` placeholder for 20th Century Boy / every song.

DTA probe of the highlighted song (`/api/dta/eval`):
```
get_token:                    20thcenturyboy
node_type:                    4            (OwnedSongSortNode = real song)
album_art_path:               ui/image/blank_album_art_keep.png   <-- BLANK
song_file_path("_keep.png"):  songs/20thcenturyboy/20thcenturyboy_keep.png
```
`songs.dta` has `(album_art TRUE)` for the song, so `HasAlbumArt()` is **true** —
the blank comes from the `BandSongMgr` stat fallback, NOT from the no-art branch.

## The art path (how a cover is meant to bind)

```
OwnedSongSortNode::GetAlbumArtPath()           src/band3/meta_band/SongSortNode.cpp:367
  if HasAlbumArt() -> TheSongMgr.GetAlbumArtPath(token)   (else blank placeholder)
       |
BandSongMgr::GetAlbumArtPath(Symbol)           src/band3/meta_band/BandSongMgr.cpp:345
  path = SongFilePath(s,"_keep.png",true)  ->  "songs/<name>/<name>_keep.png"  (logical)
  #ifdef HX_NATIVE  stat(path) fails (real file is gen/<name>_keep.png_xbox) -> blank  <== BUG
       |
album_art_path HANDLE_EXPR  SongSortNode.cpp:171  -> consumed by the song-select milo DTA,
  which does `album_art.pic set tex_file <path>` -> RndTex::SetBitmap(FilePath)
  (Tex.cpp:221) -> loader rewrites logical -> gen/<base>.png_xbox  (same as .milo gen rewrite)
```
The cooked-path convention is confirmed in `AsyncFile.cpp:42-75` (`PrintDiscFile`):
`<dir>/gen/<base>.<ext>_<plat>`, plat from `PlatformSymbol(TheLoadMgr.GetPlatform())`.
Native forces `kPlatformXBox` (`native/src/main_native.cpp:360` etc.) →
`PlatformSymbol`=`"xbox"` (`System.cpp:186-189`). Native chdir's into `RB3_DATA`
(`rb3_render_mesh.cpp:513`), so the stat is relative to `orig-assets/extracted/`.

## Proof (end-to-end)

Symlinked one song's real art into the extracted tree:
`extracted/songs/20thcenturyboy/gen/20thcenturyboy_keep.png_xbox` (for the loader)
+ `extracted/songs/20thcenturyboy/20thcenturyboy_keep.png` (to satisfy the buggy
stat) → re-probed: `album_art_path` flipped **blank → `songs/20thcenturyboy/20thcenturyboy_keep.png`**
and the **real T. Rex cover rendered** in the right-side box (`/tmp/aa_after.png`).
Symlinks removed afterward (tree pristine). This confirms: art-missing + wrong
stat-path are the only blockers; decode/bind/render all work.

## Web

`rb3-web` also defines `HX_NATIVE` (native/CMakeLists.txt) so the same stat block
runs. On web `::stat()` hits Emscripten MEMFS where the lazily-fetched art is NOT
present at call time → same blank. The art IS reachable on web: `native/web/server.py`
already has a **W7-V4 fallback** (line ~999, `_find_fallback_dirs` → `extracted-xbox-full`)
that serves `songs/x/gen/x_keep.png_xbox`. The loader just never runs because the
stat gate returns blank first. **Not directly verified** (slow web build) — OPEN.

## FIX PLAN

**PART 1 — DATA (gitignored, no code).** Provision the per-song cover textures
into the native data dir, mirroring how `.mid/.mogg` are already symlinked from
`extracted-xbox-full`: for each song `s`, symlink (or copy)
`extracted-xbox-full/songs/<s>/gen/<s>_keep.png_xbox` →
`extracted/songs/<s>/gen/<s>_keep.png_xbox`. ~81/86 songs covered. The 3 misses
(`ifeelgoodalt` alt-mix, `radarlove`, `updates` non-song) correctly stay blank.
Web needs nothing (server.py W7-V4 fallback already serves them).

**PART 2 — CODE (RB3-only `src/band3/meta_band/BandSongMgr.cpp`, DC3-safe).**
Fix the `HX_NATIVE` block (lines 348-364) to stat the **cooked** path, and skip
the stat on web so the loader fetches from the server fallback:
```c
#ifdef HX_NATIVE
#ifndef __EMSCRIPTEN__            // web: file is fetched lazily; let the loader pull it
    if (path && *path) {
        // cooked art is "<dir>/gen/<base>.<ext>_<plat>", not the logical path.
        Symbol plat = PlatformSymbol(TheLoadMgr.GetPlatform());
        String cooked(MakeString("%s/gen/%s.%s_%s",
            FileGetPath(path, NULL), FileGetBase(path, NULL), FileGetExt(path), plat.Str()));
        struct stat st;
        if (::stat(cooked.c_str(), &st) != 0)
            return "ui/image/blank_album_art_keep.png";
    }
#endif
#endif
```
`FileGet{Path,Base,Ext}` + `PlatformSymbol` + `MakeString` + `<sys/stat.h>` are
already available/included in this TU (FileGetBase is used at BandSongMgr.cpp:325;
this mirrors `AsyncFile.cpp:42-75`).

**Blast radius:** `GetAlbumArtPath` has 2 callers — `OwnedSongSortNode::GetAlbumArtPath`
(song-select list) and `SelectDifficultyPanel.cpp:159` (song-detail screen). Both
benefit. RB3-only file (not present in DC3) → DC3 unaffected.

**Alternative (no code, less clean):** have PART-1 also create the logical-path
symlink `extracted/songs/<s>/<s>_keep.png` so the *existing* (buggy) stat passes.
Works but ships redundant per-song symlinks and doesn't help web. Prefer PART 2.

**Verification:** rebuild `rb3-native`; `scripts/native/song-select-capture.py`;
the right-side cover box must show the real cover (not `?`); scroll the list and
confirm distinct covers per song vs the retail grid
(`images/retail-screenshots/yt_qRagnZCIMzk_song_select_album_art.png`). Then build
`rb3-web` and confirm covers render in-browser.

## Open

- Web not directly verified — confirm `#ifndef __EMSCRIPTEN__` + server W7-V4
  fallback makes the loader fetch `songs/x/gen/x_keep.png_xbox` in-browser.
- `radarlove` has no `_keep.png_xbox` in extracted-xbox-full (has `_keep.png_wii`
  in wii-extracted) — either source from wii or accept blank.
