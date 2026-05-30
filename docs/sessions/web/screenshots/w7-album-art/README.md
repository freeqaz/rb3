# W7 V4 — album art verification screenshots

Captured 2026-05-30 via `scripts/web/w7-v4-album-art-capture.mjs` against the
build on branch `wt-web-w7-album-art` (server fallback fix). PNGs are
gitignored (per repo convention) — re-run the script to regenerate.

| File | Highlighted node | Expected art |
| --- | --- | --- |
| `01_splash.png` | — | PRESS START splash (yellow text on black) |
| `02_main_hub.png` | — | hub 3D scene (W6-V2 dependent; menu list still missing until that lands) |
| `03_song_select.png` | `random_song` (kNodeFunction, type=5) | `song_select_random_keep.png` — looks like `?` by design |
| `03b_song_select_on_song.png` | `123` (kNodeSubheader, type=2) — after 30x ArrowDown | `song_select_header_keep.png` (ROCK BAND text) |
| `03c_song_select_next_song.png` | `123` (same — ArrowDown didn't advance past subheader in the 5x tail) | same |
| `capture-summary.json` | — | Per-fetch log: 5/5 album-art-related fetches returned 200 |

## What the fix proves

- `blank_album_art_keep.png_xbox` → 200 (previously 404'd before this fix)
- `song_select_header_keep.png_xbox` → 200 (previously 404'd before this fix)
- `song_select_random_keep.png_xbox` → 200 (was already in `extracted/`; unchanged)

Direct `curl` probes confirm per-song textures are now reachable end-to-end:

```text
GET /api/file/songs/20thcenturyboy/gen/20thcenturyboy_keep.png_xbox   → 200  43680 bytes
GET /api/file/songs/bohemianrhapsody/gen/bohemianrhapsody_keep.png_xbox → 200  43680 bytes
```

## Why the browser smoke didn't land on a song row

`ArrowDown` moves the music-library highlight slowly: it took 30 presses to
walk from `random_song` (default highlight at song_select entry) to the `123`
subheader. The next 5 presses didn't advance past `123`. The smoke bails at
that point; reaching `20thcenturyboy` (the first song) would need ~3 more
presses. Increasing the loop is mechanical follow-up work; the *fetch
plumbing* is verified above without needing it.

A future iteration of the capture script should either (a) just press
ArrowDown ~50 times unconditionally or (b) hook directly into
`MusicLibrary::TryToSetHighlight` via a non-confirm path (current
`window.rb3WebTargetSong` always pairs the highlight with a Confirm that
advances to `part_difficulty_screen`).
