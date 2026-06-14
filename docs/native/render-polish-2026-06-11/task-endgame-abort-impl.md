# Task: endgame / score-screen abort — IMPLEMENTATION (wave 4)

**Status: DONE — verified for BOTH guitar and vocals.**

A full song played to completion aborted at the endgame/score-screen transition
(`ui/endgame/endgame_helpers.dta(64):meta_performer` SIGABRT, exit code 134) for
ALL instruments. Pre-existing, instrument-agnostic; blocked every score-screen
playthrough natively. Now fixed with a one-line native wiring; guitar + vocals
both reach `coop_endgame_popups_screen` and stay stable with zero crashes.

## Root cause

The abort fires in `MetaPerformer`'s song-completion / score-upload path:

```
MetaPerformer::Handle (meta_performer DTA handler)
 -> TriggerSongCompletion -> CompleteSong -> PotentiallyUpdateLeaderboards
 -> SaveAndUploadScores -> UpdateScores
      Server *netServer = TheNet.GetServer();
      MILO_ASSERT(netServer, 0x437);   // <-- OSFatal here, "Error: netServer"
```

`Net::Init()` (`network/net/Net.cpp:73`) normally does `mServer = &TheServer`.
But `network/Net.cpp` is **not compiled** in the native build (the Quazal/DWC
online stack is excluded — Nintendo WFC is dead), so `TheNet` is a zero-filled
**weak data blob** (`band3_link_stubs.s` `.weak TheNet`). Therefore
`TheNet.mServer` stays null and `TheNet.GetServer()` returns null → the
`MILO_ASSERT(netServer, ...)` aborts at the endgame transition.

This is the **same `TheNet`-stub family** as the original vocal-load SIGSEGV that
wave 2 fixed by wiring `TheNet.mSession = TheNetSession` — just a different member
(`mServer` vs `mSession`). `MetaPerformer.cpp` alone has ~15 `TheNet.GetServer()`
derefs (638/654/681/699/743/753/780/1233/1420/1524/1557/1587/1617 …); the
score-upload path hits one unconditionally at song completion.

### Why a server stub already exists but wasn't reached
`native/src/rb3_server_native.cpp` already defines a faithful native OFFLINE
`TheServer` (`NativeOfflineServer`: real vtable, `GetPlayerID(pad)==0`,
`IsConnected()==false`) and binds the `extern Server &TheServer` reference. The
ONLY missing link was `TheNet.mServer` pointing at it — `TheNet.GetServer()`
returns `mServer`, not `TheServer`, and nothing wired the two together.

## The fix (native-only, match-neutral)

`native/src/rb3_netsession_native.cpp`, `RB3InitNativeNetSession()` — added one
wiring line right after the existing `TheNet.mSession = TheNetSession;`:

```cpp
TheNet.mServer = &TheServer;   // mirror Net::Init()'s `mServer = &TheServer`
```

(+ `#include "net/Server.h"` and an explanatory comment block.)

`TheServer` is the offline server, so `GetPlayerID(padnum) == 0` for every pad,
which drives the **local/offline score branch** in `UpdateScores`
(`b8 = false → profile->UpdateScore(songID, …, false)`). That is the correct
offline behavior: a console not signed into online play uploads no leaderboard
score but still saves the local profile score. One line fixes **every**
`TheNet.GetServer()` deref at once with correct semantics — no per-call-site
guarding (which would be whack-a-mole across 15+ sites).

Files changed:
- `native/src/rb3_netsession_native.cpp` (+19 lines, +1 include). HX_NATIVE-only
  TU — **never compiled for the Wii build**. No shared `src/` file touched.

## Branch + commit

- Worktree: `.claude/worktrees/task-endgame-abort`
- Branch: `wt-task-endgame-abort` (from `f08970ea`)
- Commit: `d7efbb16`

## Verification (before/after evidence under `/tmp/rp4-endgame-abort/`)

Built rb3-native in the worktree (clang/clang++, engine pin `469c550`,
`MILO_ENGINE_PATH=/home/free/code/milohax/milo-native-engine`,
`Dawn_DIR=.../dc3-decomp-deps/dawn/lib/cmake/Dawn`). Ports 9041–9044.

| scenario | bin | result | crash sigs |
|---|---|---|---|
| **BEFORE** guitar full-song (`song-end-test --require-endgame`, 9041) | baseline | **FAIL** exit 134, `MetaPerformer.cpp Error: netServer`, never reaches endgame | abort |
| **AFTER** guitar full-song (`song-end-test --require-endgame`, 9042) | fixed | **PASS** — `coop_endgame_popups_screen` stable 25 s, 3127 frames | **0** |
| **AFTER** vocals full-song (`vocal_to_end.py --track vocals`, 9043) | fixed | **PASS** — `coop_endgame_popups_screen` stable 30 s, 3329 frames | **0** |
| **AFTER** guitar normal song-end (no `--require-endgame`, 9044) | fixed | **PASS** — game-over reached, no regression | **0** |

- BEFORE abort backtrace symbolized (addr2line): `MetaPerformer::UpdateScores`
  → `Debug::Fail` → `OSFatal` — matches the wave-3 verify-vocals.md finding
  exactly.
- AFTER engine logs (`/tmp/rb3-song-end-9042.log`, `/tmp/rb3-v2e-9043.log`):
  `grep -acE 'OSFatal|SIGABRT|SIGSEGV|APP FAILED|Error: netServer|Data Stack
  Trace|not implemented'` → **0** each.
- Endgame screen rendering captured: `/tmp/rp4-endgame-abort/vocals/endgame_*.png`
  — the `coop_endgame_popups_screen` celebration scene (crowd arms-raised, "MENU"
  bottom-left, "CONNECT CONTROLLER" prompts) renders and holds stable.

Evidence index:
- `/tmp/rp4-endgame-abort/before_run.log` + `/tmp/rb3-song-end-9041.log` — baseline abort
- `/tmp/rp4-endgame-abort/after_guitar.log` + `/tmp/rb3-song-end-9042.log` — guitar PASS
- `/tmp/rp4-endgame-abort/after_vocals.log` + `/tmp/rb3-v2e-9043.log` — vocals PASS
- `/tmp/rp4-endgame-abort/after_noreq.log` + `/tmp/rb3-song-end-9044.log` — normal song-end PASS
- `/tmp/rp4-endgame-abort/vocals/endgame_{00,05,10,15,20,25,final}.png` — endgame screenshots

## Match-neutrality / Wii build

Only `native/src/rb3_netsession_native.cpp` changed (`git diff --stat`: 1 file,
+19). It is entirely inside `#ifdef HX_NATIVE` and is a native-only TU that is
**never part of the Wii decomp build**, so the Wii binary is byte-identical by
construction. No shared `src/band3/` or `src/system/` file was touched, so no
objdiff delta is possible. `wiiByteIdentical = true` (trivially — no shared edit).

## Landing notes

- **Single file, single line of real logic.** Cherry-pick `d7efbb16` (or just add
  `TheNet.mServer = &TheServer;` + the `net/Server.h` include to
  `native/src/rb3_netsession_native.cpp`'s `RB3InitNativeNetSession()`).
- **No engine change.** Engine pin unchanged (`469c550`).
- **No conflict with sibling wave-4 tasks** — they touch `Rnd_Wgpu_RB3.cpp` /
  `standard_wgsl.inc` / `Part.cpp` (engine) and shader/lighting code; this is a
  rb3-native shim TU none of them touch. No land-order constraint.
- The new regression gate is `scripts/native/song-end-test.py --require-endgame`
  (guitar) — it FAILED before, PASSES now. For vocals, `vocal_to_end.py --track
  vocals` (uses `RB3_BIN_OVERRIDE`).

## Follow-ups revealed by the now-reachable score screen (documented, NOT fixed)

The abort (the blocker) is fixed and the score-screen sequence is reachable. The
endgame screen we reach is `coop_endgame_popups_screen` — the celebration/popups
screen. Headless, it holds there (no controller dismiss; the "CONNECT
CONTROLLER" prompt is the expected no-input state, same as gameplay's mic prompt).
It does NOT auto-advance to the score-detail/results breakdown, which in normal
play is gated on a controller "advance" input. This is expected headless behavior,
NOT a new native gap — but the deeper results screens (star breakdown, per-player
score detail) have not been exercised end-to-end and may have their own native
gaps once advanced past the popup. Suggested follow-up: drive the popup forward
with a controller "confirm"/"start" verb and capture the actual score-detail
screen to surface any base-vs-subclass cast / null-label gaps (the
`coop_player_widget`/`coop_endgame` cast-abort class the song-end-test header
mentions). Out of scope for this task (the assigned blocker is fixed).
