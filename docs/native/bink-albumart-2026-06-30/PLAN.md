# PLAN — Album-art covers + Festival Bink jumbotron (synthesis)

**Date:** 2026-06-30 · engine pin `998b8734` · synthesis agent (Opus)
**Inputs:** `album-art.md` (ROOT-CAUSED, proven end-to-end) · `bink-unblock.md` (asset hunt + render plan, ASSET-BLOCKED)
**Scope:** research-only synthesis. No src/engine edits landed by this workflow.

## Recommended ordering

1. **AlbumArt — IMPLEMENT now.** Quick, fully visible win. Root cause proven end-to-end
   (real cover renders once the cooked path is found). Code is RB3-only + DC3-safe; data is
   gitignored symlinks already mirrored for `.mid/.mogg`.
2. **Bink — BLOCKED (asset).** Render plumbing is implement-ready and DC3-safe, but the
   source `fest{1,2}_mass*.bik` exist in **no** accessible ARK/disc (Wii or Xbox). Keep
   Option A (skip the unpainted RT quad). Re-open only when a full 360/PS3 disc image lands.

Rationale: value × tractability. AlbumArt = high value (covers visible on every song row),
fully tractable today. Bink = high value but tractability = 0 until assets are obtained;
implementing the plumbing now would paint nothing and risk regressing the Option-A fallback.

---

## TRACK 1 — ALBUM ART · verdict: IMPLEMENT · effort: small

**Symptom:** every on-disc song's cover renders as the `?` blank placeholder
(`ui/image/blank_album_art_keep.png`) in `song_select`. ALL songs, native + web.

**Two compounding causes (both must be fixed):**
- DATA: `orig-assets/extracted/` ships each song's `.mid/.mogg/.milo_xbox` but NOT the cover
  `songs/<s>/gen/<s>_keep.png_xbox`. Those textures exist in `orig-assets/extracted-xbox-full/`
  (81/86 native songs) and `orig-assets/wii-extracted/` (`_keep.png_wii`).
- CODE: `BandSongMgr::GetAlbumArtPath` (`src/band3/meta_band/BandSongMgr.cpp:345-368`),
  `HX_NATIVE` block lines 348-364, `::stat()`s the **logical** path
  `songs/<s>/<s>_keep.png`. The cooked texture is always at `songs/<s>/gen/<s>_keep.png_xbox`
  (gen/ subdir + platform suffix), so the logical stat **never** succeeds → blank fires
  unconditionally for every song even when art is present.

### Part 1 — DATA (gitignored, no code)
Mirror the existing `.mid/.mogg` symlink pattern: for each song `s`, symlink
`orig-assets/extracted-xbox-full/songs/<s>/gen/<s>_keep.png_xbox` →
`orig-assets/extracted/songs/<s>/gen/<s>_keep.png_xbox`. ~81/86 covered. The misses
(`ifeelgoodalt`, `radarlove` — only `_keep.png_wii`, `updates` non-song) correctly stay blank.
Web needs nothing: `native/web/server.py` W7-V4 fallback (`_find_fallback_dirs` → `extracted-xbox-full`)
already serves `songs/<s>/gen/<s>_keep.png_xbox`.

### Part 2 — CODE (RB3-only `src/band3/meta_band/BandSongMgr.cpp`, DC3-safe)
Replace the lines 348-364 `HX_NATIVE` block: stat the **cooked** path (not the logical one),
and skip the stat on web so the loader fetches from the server fallback:
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
`FileGet{Path,Base,Ext}` + `PlatformSymbol` + `MakeString` + `<sys/stat.h>` are already
available in this TU (FileGetBase used at `BandSongMgr.cpp:325`; mirrors `AsyncFile.cpp:42-75`).
Blast radius: 2 callers — `OwnedSongSortNode::GetAlbumArtPath` (song-select list) and
`SelectDifficultyPanel.cpp:159` (song-detail). Both benefit. RB3-only file → DC3 untouched.

### Where
- `src/band3/meta_band/BandSongMgr.cpp:348-364` (HX_NATIVE, RB3-only)
- DATA: `orig-assets/extracted/songs/*/gen/*_keep.png_xbox` symlinks (gitignored, not committed)
- web: no change (`server.py` W7-V4 fallback already serves it)

### Verification (objective)
Rebuild `rb3-native`; `python3 scripts/native/song-select-capture.py`; the right-side cover
box must show the real cover (not `?`); scroll list → distinct covers per song, matching
`images/retail-screenshots/yt_qRagnZCIMzk_song_select_album_art.png`. Then build `rb3-web` and
confirm covers render in-browser (closes the one OPEN item: web `#ifndef __EMSCRIPTEN__` +
server fallback fetches `songs/x/gen/x_keep.png_xbox`).

### Open / risk
- Web path not directly verified (slow build) — gated by the `#ifndef __EMSCRIPTEN__` guard
  above + server.py fallback; confirm in-browser.
- `radarlove` lacks `_keep.png_xbox` (only `_keep.png_wii`) — source from wii or accept blank.

---

## TRACK 2 — FESTIVAL BINK · verdict: BLOCKED (asset) · effort: medium (plumbing) once unblocked

**Status:** the Option-B render plumbing is IMPLEMENT-ready, bounded, DC3-proven. But the
source movies exist in **no** ARK we can access (Wii or Xbox), so there is nothing to decode.
**Asset-blocked — keep Option A.**

### Why blocked (asset hunt was exhaustive)
- `fest{1,2}_mass*.bik` (7 per venue) are referenced ONLY by the `.milo_xbox` festival milos
  (`coop_crowd_massNN_screenmask.shot`). The `.milo_wii` festival path has no `_screenmask`
  and references only ambient `crowd_*.bik`.
- Wii ARK header: 0 refs to the jumbotron biks. Xbox ARK (`main_xbox`, full `arkhelper` extract,
  7863 files): only 6 biks total (intro/credits/4 guitar tutorials) — zero `fest*_mass`.
- Not embedded in the milos (no Bink container magic). Filesystem-wide find: 0 hits.
- The only loose Wii "crowd" biks are 4×4 audio-only dummies (no crowd video anywhere).
- **Conclusion:** these are Xbox/PS3 loose disc-stream files shipped OUTSIDE `main_xbox.ark`.
  Unobtainable without a full 360/PS3 disc filesystem image (we only have the ARK + Wii wbfs).

### To unblock
1. Obtain a full RB3 Xbox 360 (or PS3) **disc image** and pull
   `world/venue/festival/festival_0{1,2}/gen/texture[s]/fest{1,2}_mass*.bik` (7 per venue).
2. Transcode video-only (`-an`, in-world silent loop) via an extended `scripts/web/transcode_videos.py`
   (glob `world/venue/festival/**/*.bik`). `ffmpeg` Bink→VP9/.webm proven here.
3. Implement Option B1 (below), ~1 day.

### Render plan (Option B1 — RB3-only, DC3-safe) — ready once assets exist
- **NEW `native/src/rb3_texmovie_native.cpp`** (~120-180 LOC): `std::map<TexMovie*, FFmpegMovieImpl*>`
  (native) / `WebMovieImpl*` (web). `RB3TexMovieBegin/Poll/UploadIfReady/End`; UploadIfReady →
  `UploadRGBAToRndTex(RndTex*, rgba, w, h)` (`Tex_Wgpu.cpp:258`, shared/RB3-safe). Self-contained;
  never reshapes the byte-matched Wii `Movie` class.
- **`src/system/movie/TexMovie.cpp`** (RB3-only HX_NATIVE branches):
  `DrawToTexture()` (`:86`) → `RB3TexMovieUploadIfReady(this, mTex)` instead of Wii
  MakeDrawTarget/Draw/Finish; `BeginMovie()` (`:72`) → `RB3TexMovieBegin(this, unk_0x38, mLoop)`;
  `Poll()` (`:116`) → `RB3TexMoviePoll(this, …)` so decode advances only while showing.
- **`native/CMakeLists.txt:178`**: `FFmpegMovieImpl.cpp` is in `..._PLATFORM_EXCLUDE` and its
  header is DC3-only — compile it into the new RB3 TU with an include shim OR lift the FFmpeg
  decode core into `rb3_texmovie_native.cpp`. Add `libavformat/avcodec/swscale/avutil` to the
  rb3-native link line.
- **Web:** reuse `WebMovieImpl` (`<video>`+canvas readback); extend `transcode_videos.py` to emit
  `.webm` for festival biks; `server.py` already serves `.webm` sidecars via extracted-xbox-full.
- **Mapping:** all `crowd_massNN.tmov` in a venue paint the single shared RT `crowd_mass.tex`;
  active `..._screenmask.shot` selects which `.tmov` (→ which bik). Need 7 webm/venue.
- **Loop:** silent in-world loop — set `mLoop`, video-only transcode, no audio.
- **Option-A skip** (`Rnd_Wgpu_RB3.cpp:~3342`): naturally bypassed once the RT is painted
  (`hasTex==true`); keep it as the unpainted fallback.

### Where
- engine DC3-safe: `Tex_Wgpu.cpp` (UploadRGBAToRndTex, already used), `Rnd_Wgpu_RB3.cpp` (RB3-only),
  `FFmpegMovieImpl/WebMovieImpl` (existing).
- src/ HX_NATIVE: `src/system/movie/TexMovie.cpp:72/86/116`.
- native/src: NEW `native/src/rb3_texmovie_native.cpp`; `native/CMakeLists.txt:178`.
- transcode/assets: `scripts/web/transcode_videos.py` (extend); biks under
  `world/venue/festival/festival_0{1,2}/gen/texture[s]/` (UNOBTAINED).

### Verification (objective) — only meaningful once assets exist
Boot to a festival venue in gameplay; `/api/screenshot`; the `crowd_mass` jumbotron RT must
show the looping crowd video (not black). A/B vs the Option-A black-skip baseline. Web: same
shot in-browser via the `.webm` sidecar.

---

## Summary table

| Track | Verdict | Effort | Blocker |
|---|---|---|---|
| AlbumArt | IMPLEMENT | small | none — data symlinks + 1 RB3-only code block |
| Bink | BLOCKED | medium (once unblocked) | source biks absent from all accessible ARKs/discs |
