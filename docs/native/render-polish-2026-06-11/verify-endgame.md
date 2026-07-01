# Verify: endgame / score-screen abort — WAVE-4 adversarial verification

**Verdict: CONFIRM.** The endgame/score-screen abort (`MetaPerformer::UpdateScores`
`MILO_ASSERT(TheNet.GetServer())` SIGABRT, exit 134) is fixed on the composed
wave-4 build for BOTH guitar and vocals. A full song to completion reaches
`coop_endgame_popups_screen` and holds stable with zero crash signatures. The
other three wave-4 brightness fixes (fret-sphere, venue soft-clip, menu-fog) show
no regression in a full menu→gameplay sweep.

Verifier: wave-4 independent (Opus rigor), read-only on shared source. Composed
build: rb3 master `9c1f4449`, engine pin `58254f7`, binary built 2026-06-14
21:41:59 (≈10 s after fix commit `c9c2cad8` @ 21:41:49 — the fix IS in this binary).
Ports 9131-9137. Evidence under `/tmp/rp4rev-endgame/` and
`/home/free/tmp/rp4rev-endgame/` (the latter because the `/tmp` tmpfs user quota
of 38270M was already maxed by sibling agents' dirs — screenshots/logs were
redirected to the home fs, which is unrelated to the fix).

---

## Fix under test

`native/src/rb3_netsession_native.cpp` (`c9c2cad8`): one line in
`RB3InitNativeNetSession()`,

```cpp
TheNet.mServer = &TheServer;   // mirror Net::Init()'s `mServer = &TheServer`
```

plus `#include "net/Server.h"`. `TheServer` is the native offline
`NativeOfflineServer` (`rb3_server_native.cpp`). Confirmed present in the source
of the composed build (grep: line 25 include, line 325 the wiring). HX_NATIVE-only
TU, never compiled for Wii → Wii byte-identical by construction (report unchanged
81.865%; accepted, no shared `src/` file touched).

---

## (a) GUITAR full song → score screen — PASS

`scripts/native/song-end-test.py --port 9131 --require-endgame` (the NEW
regression gate that FAILED before this fix):

```
PASS: song ended -> game-over reached [state: {game is_game_over} == 1]
endgame screen reached: frame=3092 screen='coop_endgame_popups_screen'
PASS: endgame screen STABLE for 25s (3520 frames advanced, no abort).
```

- Engine log `/tmp/rb3-song-end-9131.log` (8150 lines):
  `grep -acE 'OSFatal|SIGABRT|SIGSEGV|Error: netServer|Data Stack Trace|meta_performer'`
  → **0**.
- The gate that the impl doc says "FAILED before, PASSES now" does indeed PASS on
  the composed binary. Confirmed.

## (b) VOCALS full song → score screen — PASS

`/tmp/rp3-vocals/vocal_to_end.py --port 9132 --track vocals` (the wave-3
all-instruments harness; log/screenshots redirected to home fs for quota):

```
gameplay underway frame=4223 songMs=2465
song ended -> ('state', 'is_game_over==1')
endgame screen reached: (5481, 600003.6, 'coop_endgame_popups_screen')
endgame STABLE 30s: 4505 frames advanced, no crash
```

- Engine log `/home/free/tmp/rp4rev-endgame/rb3-v2e-9132.log` (11593 lines):
  crash-sig scan → **0**.
- Guitar via the same vocal-style harness (`--track guitar`, port 9133) also
  reaches `coop_endgame_popups_screen`, stable 30 s / 4056 frames, log clean (0).

**This is the instrument-agnostic path** that the wave-3 verify-vocals.md flagged
as the pre-existing blocker. It is now fixed: the same `meta_performer` /
`MetaPerformer::UpdateScores` site that aborted for both guitar AND vocals no
longer fires, because `TheNet.GetServer()` now returns the offline `TheServer`
rather than null.

## (c) Score-screen rendering quality

The reached screen is `coop_endgame_popups_screen` — the celebration/popups screen
(crowd, foreground band characters, "MENU" bottom-left, three "CONNECT CONTROLLER"
prompts). Screenshots: `/home/free/tmp/rp4rev-endgame/vocals/endgame_{00,final}.png`
and `/home/free/tmp/rp4rev-endgame/guitar/endgame_{00,final}.png`. It renders and
holds stable. This matches the impl-doc claim exactly.

It does NOT auto-advance to the star/score-detail breakdown — that transition is
gated on a controller "advance" input that headless does not drive by default. I
attempted to drive the popup forward with confirm/start verbs (`advance_endgame.py`,
ports 9134/9135) but both attempts raced the boot sequence (pressed inputs while
still on `intro_movie_screen`/`splash_screen`) and never reached the popup — a
flake in my throwaway probe, **not** a crash (0 crash sigs in those logs). So the
deeper score-detail screens (star breakdown, per-player widget) remain
**un-exercised end-to-end** — listed below as a wave-5 follow-up, not a defect of
this fix. The success criterion for THIS task (abort fixed, score screen reached)
is met.

### Score-screen visual notes (wave-5 follow-ups, NOT this task's scope)
- The crowd in the endgame scene has the known greenish/dark tint + a slightly
  washed/foggy look — consistent with the open crowd-render residual and venue
  lighting class, NOT an endgame-specific gap.
- The score-detail/results breakdown screen (past the popup) is unverified; may
  carry its own native gaps (base-vs-subclass casts / null labels on
  `coop_player_widget` per the song-end-test header) once advanced.

---

## Interaction sweep (the other three wave-4 brightness fixes) — interactionsOk = true

Full nav via the canonical `scripts/native/keyboard-to-gameplay.py --port 9137
--diff hard --game-burst 20` (boot → main_hub → song_select → part_difficulty →
gameplay/lit venue), 20 in-gameplay bursts, log `/tmp/rb3-kbd2game-9137.log` clean
(0 crash sigs):

- **song_select** (`01_song_select.png`): clean music-library list, album art,
  FRIEND RANKINGS panel. No fog wash. (menu-fog OK.)
- **part_difficulty** (`02_part_difficulty.png`): instrument select renders clean,
  no wash. (diff-grid / menu-fog OK.)
- **gameplay, lit club venue** (`burst_05.png`, `burst_10.png`): venue backdrop
  (dartboard, posters, coherent band character on the right) is NORMALLY LIT — no
  lighting blowout / clip wash (venue soft-clip OK). The note highway shows
  per-color gems (green/red/yellow/blue/orange) and fret buttons with **no
  white-sphere fret-glow** (fret-sphere fix OK). Highway head-on, gems saturated.

No new visual regression observed from the composed brightness trio. The endgame
fix is rb3-native-only and does not touch any shader/Rnd file, so no shader
interaction was expected — and none was seen.

---

## VERDICT: CONFIRM

The endgame abort is fixed for ALL instruments (guitar + vocals both reach
`coop_endgame_popups_screen`, stable, zero crash signatures across 4 clean runs
totalling ~40k+ frames). The fix is a single native wiring line, Wii byte-identical
by construction. The other wave-4 fixes are intact in a full nav sweep.

### Residual gaps / wave-5 follow-ups (documented, out of scope here)
1. **Score-detail/results breakdown un-exercised** — the popup does not auto-advance
   headless; a confirm/start-driven probe of the star breakdown + per-player widget
   should be run to surface any base-vs-subclass cast / null-label gaps past the popup.
2. **Endgame crowd tint/wash** — crowd greenish/dark + foggy in the celebration
   scene; same class as the open crowd-render + venue lighting residuals, not
   endgame-specific.

## Evidence index
- `/tmp/rb3-song-end-9131.log` + `/tmp/rp4rev-endgame/guitar_require_endgame.log`
  — guitar `--require-endgame` PASS (0 crash sigs).
- `/home/free/tmp/rp4rev-endgame/rb3-v2e-9132.log`
  + `/home/free/tmp/rp4rev-endgame/vocals_to_end.log` — vocals full-song → endgame PASS.
- `/home/free/tmp/rp4rev-endgame/rb3-v2e-9133.log` — guitar via vocal-style harness PASS.
- `/home/free/tmp/rp4rev-endgame/vocals/endgame_{00,final}.png`,
  `/home/free/tmp/rp4rev-endgame/guitar/endgame_{00,final}.png` — score-screen renders.
- `/tmp/rb3-kbd2game-9137.log` + `/home/free/tmp/rp4rev-endgame/kbd2game/*.png`
  — full nav sweep (menu → gameplay lit venue), interactionsOk evidence.
- `/home/free/tmp/rp4rev-endgame/rb3-adv-{9134,9135}.log` — popup-advance probes
  (raced boot, no crash; deeper screen un-reached → wave-5 follow-up).
