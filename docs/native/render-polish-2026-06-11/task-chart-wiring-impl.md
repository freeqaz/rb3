# Wave-7 — chart-wiring: make all 83 on-disc songs LOAD AND PLAY

## TL;DR

The ~80 "dead at the loading vignette" songs were missing TWO things, not one:

1. **Their `.mid` charts were absent from the native loader root** (`orig-assets/extracted`).
   Fixed by **symlinking** the 80 missing `<shortname>.mid` files from the full Xbox
   extract (`extracted-xbox-full`) into `extracted/songs/<id>/<id>.mid`. (Asset wiring.)
2. **A latent decomp bug in the shared vocal-lyric parser** (`SongParser::ParseAndStripLyricText`)
   that SIGSEGVs on real charts the moment a lyric is all trailing strip-characters.
   The 3-song dev slice never tripped it; every real chart does. Fixed `#ifdef HX_NATIVE`
   (Wii byte-identical).

Both are needed for PLAY. Step 1 alone gets the chart to parse but the song then crashes
in step 2; step 2 alone is dead code without the charts.

## Root cause

### Asset gap (the data root)
- The native loader's data root is the `RB3_DATA` env var. All harness scripts default it
  to `orig-assets/extracted`; native source (`native/src/main_native.cpp`,
  `native/src/native_file.cpp`, `native/src/rb3_platform_native.cpp`) `chdir`s into it and
  resolves every asset relative to it. Choosing the root is purely a path choice.
- `extracted/songs` already had all 83 `.mogg` + 293 `.milo_xbox` (a prior agent symlinked the
  moggs from `extracted-xbox-full`; see the comment in `scripts/native/audio_verify.py:69`),
  but **only 3 `.mid` charts** (20thcenturyboy / 25or6to4 / antibodies). The other 80 song
  dirs had mogg+milo but no chart → wave-6's `Game::IsLoaded()` no-chart gate held them
  alive at the loading vignette (`tv3_b_screen`), unplayable.
- `extracted-xbox-full/songs` is the full on-disc 83-song set with all 635 `.mid` files
  (the `<shortname>.mid` for every song, IDENTICAL md5 to the 3 already in `extracted`).

### Why symlink (option b), NOT repoint the root (option a)
`extracted/songs` holds native-specific files the full extract LACKS — `songs.dta` +
`missing_song_data.dta` (real files, not symlinks) that drive song discovery. A wholesale
repoint of `RB3_DATA` to `extracted-xbox-full` would lose those and break the library + the
DTA overlay layout. Symlinking the missing charts INTO `extracted` keeps the curated root
intact, works for the 3 dev songs (already symlinks), and touches no other asset category
(char/ui/world/sfx untouched). This mirrors the established mogg-symlink precedent exactly.
Charts are tiny (38.6 MB total) so a symlink (vs the 23 GB of moggs) is not even about size —
it is about not duplicating an already-on-disk file.

### The latent parser crash (exposed once real charts load)
`SongParser::ParseAndStripLyricText` (`src/system/beatmatch/SongParser.cpp:1087`, 90.77%
matched) strips trailing modifier chars (`$ # ^ % / 1 2`) off the end of a vocal lyric. The
matched-Wii loop decrements the scan pointer `p` TWICE per iteration (`p--` in the body AND
`--p` in the `while (--p - text)` test) and loops on the pointer difference. When a lyric is
all trailing strip-characters (e.g. `"##"`, `"//$"`), `p` underflows BELOW `text`,
`(p - text) + 1` becomes a huge unsigned count, and `String::reserve(huge)` → `_MemOrPoolAlloc`
fails → NULL → `strncpy`/`memcpy` SIGSEGV (crash backtrace symbolised to
`Str.cpp:32 String::reserve` ← `SongParser.cpp:1116`).

The Bank-8 TARGET body (Ghidra + m2c) instead keeps a separate length counter from `strlen`,
decrements `p` ONCE, and loops on the counter — so it terminates regardless. The current
decomp's double-decrement is a genuine bug that happens to score 0.83pp higher purely from a
`mtctr/bdnz` vs `subic./bne` loop-idiom difference. Fix = match the target's counter
semantics; gated `#ifdef HX_NATIVE` so the Wii object stays byte-identical (matched original
kept under `#else`).

## Files changed + why
- **`orig-assets/extracted/songs/<id>/<id>.mid` × 80** — new symlinks → the full Xbox extract's
  charts. NOT committed (the extract trees are gitignored, machine-local assets). Reproduce
  with the one-liner in "Landing / reproduce" below.
- **`src/system/beatmatch/SongParser.cpp`** — `ParseAndStripLyricText` loop, corrected under
  `#ifdef HX_NATIVE`; original matched loop kept under `#else`. Wii byte-identical.

## Branch + commits
- Branch `wt-task-chart-wiring` (worktree `.claude/worktrees/task-chart-wiring`).
- Commit `0173ec04` — `fix(native): SongParser::ParseAndStripLyricText pointer underflow → SIGSEGV on real charts`.

## Evidence
### Before (chartless freebird, fixed binary not yet built; with the wave-6 crash gate)
- `freebird` (mid removed): advances to `tv3_b_screen` (loading vignette) and NEVER reaches
  gameplay — `reached_gameplay=false, score=0`. App alive (wave-6 gate), but unplayable.
  (`/tmp/rp7-chart-wiring/before/`).
### Charts present, parser bug NOT yet fixed
- `bohemianrhapsody` chart PARSES (MIDI note-on events processed in the engine log) then
  SIGSEGVs in `String::reserve` ← `ParseAndStripLyricText` ← `OnText` ← `MidiReader` (full
  symbolised backtrace in `/tmp/rp7-chart-wiring/after/engine-9602.log`). This proved the
  asset wiring works AND surfaced the parser bug.
### After (charts present + parser fix) — fixed binary, fresh-boot-per-song
Harness `/tmp/rp7-chart-wiring/play_one.py` (fresh boot, scroll to named song, select,
guitar/expert + nofail + autohit, sample 18-20s of gameplay). Screenshots in
`/tmp/rp7-chart-wiring/after/<song>_GAMEPLAY.png`.

| song | reached gameplay | songMs | score climb (18-20s window) | screenshot |
|---|---|---|---|---|
| bohemianrhapsody | YES (game_screen) | 20065 ms | 0 (a-cappella/piano intro — no guitar gems in first 20s) | highway + 5 smashers + HUD |
| freebird | YES (game_screen) | 20030 ms | **1334** | highway + lit sustain lanes + score "1,388" + star meter |
| rehab | YES (game_screen) | 19950 ms | 0 (intro) | highway + smashers + green disco-lit venue |
| crazytrain | YES (game_screen) | 20027 ms | **233** | highway live |
| duhast | YES (game_screen) | 20266 ms | 0 (intro) | highway live |
| smokeonthewater | YES (game_screen) | 19758 ms | **2434** | highway + 2 held sustain lanes + score widget |

**6/6 previously-dead songs reach live gameplay** (no vignette hold, no crash); 3/6 show a
non-zero score climbing in the short window (the other 3 have slow intros / no guitar gems
in the first 20s — not a scoring failure, just early-song timing).

Notes: `score_peak` is the in-game `{{beatmatch main_performer} score}` sampled over a SHORT
mid-song window; a 0 on a slow-intro song (bohemianrhapsody) is expected (no guitar gems yet),
not a scoring failure — freebird climbing to 1334 in the same 20s proves the autohit→HitGem→
AddPoints pipeline runs on previously-dead songs. (`notes_hit` reads off the band-level
performer binding and stays 0; gem-player stats live elsewhere — harness binding nuance, the
score climb is the authoritative signal.) The dedicated `scripts/native/scoring-test.py`
(plays to natural EOF, reads the endgame widget) is the canonical scorer gate.

## Verification
- BEFORE (chartless, wave-6 gated): freebird holds at `tv3_b_screen` loading vignette,
  reached_gameplay=false, score=0. App alive but unplayable.
- ASSET WIRING: 80 `.mid` symlinks created (all 80 chartless songs that have a source); all
  83 `<shortname>.mid` now resolvable under `extracted/songs`. The 3 dev songs were already
  symlinks; 25or6to4 / antibodies / 20thcenturyboy untouched.
- PARSER BUG surfaced: charts present → bohemianrhapsody SIGSEGV in `String::reserve` ←
  `ParseAndStripLyricText` (full symbolised backtrace captured). Fixed.
- AFTER (6/6 previously-dead): bohemianrhapsody / freebird / rehab / crazytrain / duhast /
  smokeonthewater all reach live `game_screen` (songMs ~20s, advancing), highway + 5 fret
  smashers + now-bar + score widget render; freebird 1334, crazytrain 233, smokeonthewater
  2434 climbing in-window. ZERO SIGSEGV / hold.
- NO REGRESSION (3/3 dev songs): 20thcenturyboy (score 2622), 25or6to4 (6775), antibodies
  (gameplay reached) all still play.
- AUDIO: freebird gameplay WAV (`scripts/native/capture_song_gameplay.py`, worktree bin)
  NON-SILENT — 15.0s @ 44.1kHz stereo, peak=14884, rms=1125, 48.7% nonzero samples (real
  music, not silence/noise).
- SONG COUNT / LIBRARY INTACT: the song-select listing is unchanged — the symlinks only add
  `.mid` to already-listed songs; discovery is driven by `songs.dta` (untouched). Proven by
  the navigation log (full alphabetical library walked: random_song, 123, A/antibodies,
  B/beastandtheharlot/thebeautifulpeople/…, C/caughtinamosh/…, D/…/duhast, E/…, F/…/freebird —
  all 80+ songs with section headers present and navigable).
- Wii byte-identical: `ParseAndStripLyricText` fuzzy 90.77109% UNCHANGED (the `#else` keeps the
  matched original). objdiff in the worktree confirms (before-fix 90.77109 == after-fix 90.77109).

## landingNotes
- The 80 `.mid` symlinks are MACHINE-LOCAL ASSETS (the `orig-assets/extracted*` trees are
  gitignored). They are NOT in the commit; recreate on any machine with the reproduce
  one-liner. The CODE fix (SongParser.cpp) IS committed on `wt-task-chart-wiring`.
- WEB: native-scoped. The web build streams assets on demand from `native/web/server.py`
  `/api/file/<path>`, which resolves through the same `extracted/` symlinks transparently
  (Python `open()` follows symlinks; `.mid` is already in the server's `WIRE_COMPRESS_EXTS`).
  NO manifest regen needed — the boot/screen manifests only bundle `.milo_xbox`/`.dta`; moggs
  and mids are lazily fetched. So pointing the web `ASSETS_DIR` at `extracted/` gets the charts
  for free. The SongParser.cpp fix compiles into rb3-web identically. Flag: confirm web song
  load once in-browser as a follow-up (per the campaign's native-first rule).
