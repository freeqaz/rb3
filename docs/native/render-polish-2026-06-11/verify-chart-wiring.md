# Verify — wave-7 chart-wiring (close-out): all on-disc songs LOAD AND PLAY

**Verdict: CONFIRM.**

Independent adversarial review (Opus) of the wave-7 close-out claim on rb3 master
`a36bfcf9` (SongParser fix `e83e2c79`), engine pin `15ce606`, against the freshly-built
binary `/home/free/code/milohax/rb3/native/build-native/rb3-native` (mtime 2026-06-16
22:25, AFTER the `e83e2c79` commit at 22:02). Ports 9701-9709. Evidence under
`/tmp/rp7rev-chart-wiring/`.

## What was claimed (wave-7 chart-wiring)
1. Machine-local symlinks of the 80 missing `.mid` charts into the extracted loader root.
2. Committed `SongParser::ParseAndStripLyricText` pointer-underflow SIGSEGV fix
   (`e83e2c79`, `#ifdef HX_NATIVE`, Wii byte-identical).
→ Result: ALL 83 on-disc songs now LOAD AND PLAY (gems + audio + score), not just "don't crash".

## Independent evidence

### Asset wiring is in place
- `find -L orig-assets/extracted/songs -iname '*.mid' | wc -l` = **83**; all 83 are
  symlinks (`-type l`), **0 broken** (`-xtype l` = 0). Targets point at
  `…/extracted-xbox-full/songs/<id>/<id>.mid`. The 3 dev songs are also symlinks (untouched).

### (a) 24 INDEPENDENTLY-chosen previously-dead songs reach live gameplay
Driven with the reusable `/tmp/rp7-chart-wiring/play_one.py` (fresh boot per song,
`--bin` pinned to the main-repo binary — NOT the worktree default it hardcodes; the
worktree binary is a stale 46 MB build vs the main-repo 115 MB), guitar/expert + nofail +
autohit. My set deliberately AVOIDS the impl's examples and sweeps the alphabet:

beastandtheharlot, caughtinamosh, centerfold, chinagrove, coldasice, deadendfriends,
everybodywantstorule, getfree, heartofglass, humanoid, imagine, iwannabesedated,
jerrywasaracecar, killingloneliness, nooneknows, ohmygod, plush, radarlove, rocklobster,
spaceoddity, thebeautifulpeople, walkoflife, werewolvesoflondon, yoshimibattles
(plus the impl-overlapping `duhast` checked at dir level).

**24/24 previously-dead songs reached `game_screen`** with `songMs` advancing past 2s,
and most show a non-zero in-game score climbing within a short window (e.g. chinagrove
10379, getfree 10223, everybodywantstorule 7775, centerfold 6114, beastandtheharlot 4876,
heartofglass 8349). A few read 0 in the short window (imagine, walkoflife, radarlove low)
= slow/piano intros with no guitar gems yet, NOT a scoring failure. Gameplay screenshots
show a fully rendered highway: lit lanes, scrolling gems, 5 fret smashers, now-bar, HUD
score meter, venue + band chars (`/tmp/rp7rev-chart-wiring/after/*_GAMEPLAY.png`).

### (b) PLAY proven to full EOF on 2 previously-dead songs (gems scroll + score accumulates)
Played to NATURAL EOF (no `{game jump}` artifact) with continuous autohit, then read the
`coop_endgame_screen` widget bindings:

| song | songMs at EOF | endgame score | stars | is_invalid | end screen |
|---|---|---|---|---|---|
| centerfold | 210,144 | **95,366** | **5** | 0 | coop_endgame_screen |
| chinagrove | 197,494 | **117,286** | **5** | 0 | coop_endgame_screen |

The chinagrove endgame screenshot shows the real score-detail screen ("CHINA GROVE",
117,286 pts, 5 stars, FLAWLESS / 100% EXPERT, BAND HIGH SCORE, solo-score widget). This is
decisive PLAY: gems scrolled across the full song, autohit→HitGem→AddPoints accumulated a
real score that survived into a valid endgame result. (`/tmp/rp7rev-chart-wiring/eof/`.)

### (c) The 3 dev songs still play (no regression)
20thcenturyboy (peak 5622), 25or6to4 (9975), antibodies (gameplay reached, slow intro) —
all reach `game_screen`, 0 crashes. (`/tmp/rp7rev-chart-wiring/dev/`.)

### (d) SongParser fix is match-neutral — verified by me
Rebuilt the base object (`build/SZBE69_B8/src/system/beatmatch/SongParser.o`) from the
committed source via `tools/ninja-locked` and regenerated the golden target via
`dtk dol split`, then ran objdiff:
- `objdiff-cli diff … ParseAndStripLyricText__10SongParserFPCcR9VocalNote` → **90.8%**
  (raw == normalized), exactly the impl's claimed unchanged 90.77109.
- Regenerated `report.json`: SongParser unit fuzzy = **99.01348** (matches the pre-fix
  campaign-recorded value), ParseAndStripLyricText fuzzy = **90.77109**. Milo Engine Code
  overall 75.23% matched — unchanged.
- The fix is `#ifdef HX_NATIVE`; the Wii build leaves HX_NATIVE undefined → compiles the
  `#else` matched original. Byte-identical on Wii confirmed two ways (objdiff + report).
- I also traced the native loop's semantics on the exact pathological inputs
  (`"##"`, `"//$"`, `"%"`, `"12"`, `""`): all yield a kept-length of **0** (safe
  `reserve(0)`), with normal lyrics (`"hello#"`→5, `"hi$#"`→2, `"go2"`→2) stripped
  correctly. The fix genuinely removes the unsigned-underflow → huge `reserve` → strncpy
  SIGSEGV, and is behaviorally correct.

### (e) Hunt for a remaining crash class — none found
**27 distinct songs total (24 previously-dead + 3 dev) reached live gameplay, 0 crashes.**
Scanned every per-song engine log for `sigsegv|sigabrt|segmentation|terminate|MILO_FAIL|
OSFatal|stack overflow|caught signal` → **NONE**. No song dead-ended at the vignette in my
sample; no different crash class surfaced.

## Residuals / notes (none block the headline)
- `play_one.py` hardcodes the *worktree* binary as its `--bin` default — a reviewer/user
  must pass the main-repo binary explicitly (I did). Cosmetic harness nit.
- The asset symlinks are machine-local + gitignored (by design); reproduce via the impl's
  one-liner on a fresh checkout. The headline "all songs play" is contingent on that local
  step, which is correct for a native-asset wiring (not a code commit).
- `notes_hit` reads 0 off the band-level performer binding in the short-window harness
  (gem-player stats live elsewhere) — the score climb + endgame widget are the
  authoritative PLAY signals, and both fire. Consistent with the impl's note.
- Web song-load remains a flagged follow-up (server.py follows the symlinks; not exercised
  here — native-first per the campaign rule).

## crashReproducesPostFix: false
No song hard-crashed post-fix in 27 distinct songs (2 played to full EOF).
