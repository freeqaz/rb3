# Album-art covers — IMPLEMENTATION (native)

**Date:** 2026-06-30 · worktree `converge-albumart` (branch `wt-converge-albumart`)
**Status:** LANDED + verified native. Implements the fix root-caused in
[`album-art.md`](album-art.md). Two compounding causes, both fixed; Wii byte-neutral.

## What was wrong (recap)

Every on-disc song's cover rendered as the blank `?` placeholder
(`ui/image/blank_album_art_keep.png`). Two compounding causes:

1. **DATA** — the native data dir `orig-assets/extracted/songs/<s>/` ships
   `.mid/.mogg/gen/<s>.milo_xbox` but NOT the per-song cover
   `gen/<s>_keep.png_xbox`.
2. **CODE** — `BandSongMgr::GetAlbumArtPath` (`src/band3/meta_band/BandSongMgr.cpp`)
   stat'd the LOGICAL path `songs/<s>/<s>_keep.png`, but the cooked texture is
   ALWAYS at `songs/<s>/gen/<s>_keep.png_xbox` (gen/ subdir + platform suffix).
   The logical path never exists → the blank fallback fired for EVERY song.

## PART 1 — DATA (reproducible provisioning script)

Extended the existing per-song provisioner **`tools/link-song-charts.sh`** (the
same script `tools/setup-worktree.sh` already runs to symlink `.mid/.mogg` from
`extracted-xbox-full/` into the loader root `extracted/`). Added an album-art
cover step inside the same per-song loop:

```
extracted-xbox-full/songs/<s>/gen/<s>_keep.png_xbox
   -> extracted/songs/<s>/gen/<s>_keep.png_xbox   (symlink, idempotent)
```

- Suffix is `_keep.png_xbox` — native forces `kPlatformXBox`, so the cooked
  texture the loader binds is the xbox flavor (higher fidelity + matches the
  rendered `.milo_xbox` scenes).
- Provisioned **81/86** songs. The remaining have no xbox source and correctly
  stay blank: `ifeelgoodalt` (alt-mix), `radarlove` (wii-only cover), `updates`.
- `orig-assets/*` is gitignored (machine-local) → the symlinks are NOT committed;
  the **script** is. Re-run any time after a fresh checkout / re-extract:
  `tools/link-song-charts.sh` (or `tools/link-song-charts.sh /path/orig-assets`).
- The cover symlink **targets the stable main-repo path**
  (`/home/.../rb3/orig-assets/extracted-xbox-full/...`), matching the existing
  `.mogg` convention, so they survive worktree removal (the covers live in the
  SHARED `extracted/` dir, like `.mid/.mogg`).

## PART 2 — CODE (`src/band3/meta_band/BandSongMgr.cpp`, HX_NATIVE-only)

Rewrote the `HX_NATIVE` block in `GetAlbumArtPath` to stat the **cooked** path
(`<dir>/gen/<base>.<ext>_<plat>`, mirroring `AsyncFile.cpp` `PrintDiscFile`)
instead of the logical path, and to **skip the stat on web** so the loader
fetches the cover from the server's asset fallback:

```c
#ifdef HX_NATIVE
#ifndef __EMSCRIPTEN__
    if (path && *path) {
        Symbol plat = PlatformSymbol(TheLoadMgr.GetPlatform());
        String cooked(MakeString(
            "%s/gen/%s.%s_%s", FileGetPath(path, nullptr), FileGetBase(path, nullptr),
            FileGetExt(path), plat.Str()));
        struct stat st;
        if (::stat(cooked.c_str(), &st) != 0)
            return "ui/image/blank_album_art_keep.png";
    }
#endif
#endif
```

- `FileGet{Path,Base,Ext}`, `PlatformSymbol` (`os/System.h`), `TheLoadMgr`
  (`utl/Loader.h`, via `obj/DirLoader.h`), `MakeString`, `String`, and
  `<sys/stat.h>` (already `#ifdef HX_NATIVE`-guarded) are all in this TU.
- 2 callers benefit: `OwnedSongSortNode::GetAlbumArtPath` (song-select grid) and
  `SelectDifficultyPanel.cpp:159` (song-detail screen).

## Wii-neutrality

The entire change is inside `#ifdef HX_NATIVE`, which is NOT defined for the
mwcc/Wii decomp build → byte-neutral by construction. Confirmed via objdiff:

```
BandSongMgr::GetAlbumArtPath(Symbol) const -- Match: 100.0% normalized (100.0% raw)
32 total | all equal -- Complete.
```

`BandSongMgr.cpp` is RB3-only (DC3 has no `src/band3/` dir) → DC3 unaffected.

## Verification (native)

`scripts/native/song-select-capture.py` → boots to `song_select_screen`,
captured at depths 0/8/16/30/50 (`shots/native_depth_*.png`). Real, distinct
per-song covers render (no `?`):

| depth | song / artist | cover |
|---|---|---|
| 0  | (category/shuffle node) | `?` — correct (no song highlighted) |
| 8  | The Beautiful People / Marilyn Manson | real |
| 16 | China Grove / The Doobie Brothers     | real |
| 30 | False Alarm / The Bronx               | real |
| 50 | Imagine / John Lennon                 | real (classic sky cover) |

The "before" state (every song `?`) is documented in `album-art.md`.
Art-less songs (`ifeelgoodalt`, `radarlove`) correctly stay blank: the cooked
path doesn't exist for them → `::stat` fails → blank fallback.

## Web (not re-verified here)

`#ifndef __EMSCRIPTEN__` skips the stat on web so the loader fetches the cover
from `native/web/server.py`'s W7-V4 `extracted-xbox-full` fallback. Per
`album-art.md`, this is the intended web path but was not re-verified in-browser
(slow web build) — OPEN.
