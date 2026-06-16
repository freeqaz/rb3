# task-overpress-harden — debug-verb OVER-PRESS crash hardening (wave 7 close-out)

**Status: DONE.** Both over-press crash classes the wave-6 track-load review
surfaced are fixed, native-only (`#ifdef HX_NATIVE`), Wii byte-identical
(both touched functions objdiff 100.0%). Reproduced each crash with a gdb
backtrace, fixed, and proved inert-on-abuse + normal-flow-intact.

- Branch: `wt-task-overpress-harden` (from master `4e21418b`)
- Worktree: `/home/free/code/milohax/rb3/.claude/worktrees/task-overpress-harden`
- Ports: 9621–9629. Evidence: `/tmp/rp7-overpress-harden/`.

---

## The two crashes (both are abnormal /api/input debug-verb sequences — our own
## review harness's failure mode, NOT normal play; reproduce on CHARTED songs too)

### CRASH (1) — `PlayerTrackConfigList::ChangeDifficulty` std::vector<int> OOB
**Site:** `src/system/beatmatch/PlayerTrackConfigList.cpp:107`
```cpp
mTrackDiffs[cfg.TrackNum()] = i;   // NO bounds check
```
The `difficulty:` debug verb path is
`ExecDifficulty → BandUser::SetDifficulty → Player::ChangeDifficulty →
GameConfig::ChangeDifficulty → PlayerTrackConfigList::ChangeDifficulty`.
Before the song is `Process()`'d (a chartless song whose config never resolved a
track, or any over-pressed config), `cfg.TrackNum()` is the **-1 default** and
`mTrackDiffs` is empty, so `mTrackDiffs[(size_t)-1]` aborts under
`_GLIBCXX_ASSERTIONS` (`vector::_M_range_check` / `__glibcxx_assert_fail`).

**gdb backtrace (deterministic gtest repro, pre-fix):**
```
#3 std::__glibcxx_assert_fail   <- Assertion '__n < this->size()' failed
#4 std::vector<int>::operator[](unsigned long)
#5 PlayerTrackConfigList::ChangeDifficulty(UserGuid const&, int)   <- mTrackDiffs[-1]
#6 OverPress_ChangeDifficultyUnprocessedConfigIsInert_Test::TestBody()
```
(`stl_vector.h:1253: std::vector<int>::operator[]: Assertion '__n < this->size()'
failed.`) This matches the wave-6 reviewer's documented live backtrace
(`…ChangeDifficulty ← GameConfig::ChangeDifficulty ← Player::ChangeDifficulty ←
… ← ExecDifficulty`) frame-for-frame at the crashing call.

### CRASH (2) — `OvershellPanel::EndOverrideFlow` `Debug::Fail("InOverrideFlow(type)")`
**Site:** `src/band3/meta_band/OvershellPanel.cpp` — `MILO_ASSERT(InOverrideFlow(type), 0x1B0)`
(0x1B0 == 432 == the documented `OvershellPanel.cpp:432`).
A **double** `end_override_flow` verb ends a flow that is no longer active: the
first end set `mPanelOverrideFlow = kOverrideFlow_None`, so `InOverrideFlow(type)`
is now false and the assert fatally `Debug::Fail`s.

**gdb-equivalent backtrace (live game, charted `20thcenturyboy`, pre-fix —
symbolized from the in-process crash dump):**
```
OvershellPanel::EndOverrideFlow(type=1)  <- Debug::Fail("InOverrideFlow(type)") @ :432
  <- OvershellPanel::Handle(DataArray*, bool)
  <- (anon)::ExecMsg(ScriptedMsg, UIScreen*)       [the end_override_flow verb]
  <- (anon)::ExecVerb <- RB3GameInputExecVerbMainThread <- RB3HttpServer::HandleInput
  <- RB3HttpServer::ProcessCommands <- RB3HttpServerPoll <- App::RunWithoutDebugging
```
The `GAME_DBG` trace shows it exactly: 1st end `curFlow=1` (ends it → 0), 2nd end
`curFlow=0` → assert fails → SIGABRT.

---

## Fixes (both native-only, Wii path unchanged)

### `src/system/beatmatch/PlayerTrackConfigList.cpp` — `ChangeDifficulty`
Guard the per-track write behind a bounds check on native; Wii keeps the original
unconditional `mTrackDiffs[cfg.TrackNum()] = i;`. The config's difficulty is
already recorded by `cfg.Update(...)` *before* the guard, so a later `Process()`
still applies it — the guard only skips the now-out-of-range scratch write.
```cpp
#ifdef HX_NATIVE
    int trackNum = cfg.TrackNum();
    if (trackNum >= 0 && trackNum < (int)mTrackDiffs.size())
        mTrackDiffs[trackNum] = i;
#else
    mTrackDiffs[cfg.TrackNum()] = i;
#endif
```

### `src/band3/meta_band/OvershellPanel.cpp` — `EndOverrideFlow`
Make a redundant / mismatched end an inert no-op on native (early return when the
flow isn't active), instead of the fatal `MILO_ASSERT`. Wii keeps the assert.
```cpp
#ifdef HX_NATIVE
    if (!InOverrideFlow(type)) {
        if (getenv("GAME_DBG")) MILO_LOG("... no-op — flow not active ...");
        return;
    }
#endif
    MILO_ASSERT(InOverrideFlow(type), 0x1B0);
```
(This replaces the wave-6 GAME_DBG entry log with a focused no-op log.)

### `native/tests/test_overpress.cpp` (NEW) + `native/CMakeLists.txt`
A GoogleTest regression suite (`OverPress.*`, links the real rb3 source via the
existing `rb3-tests` target) that drives `PlayerTrackConfigList::ChangeDifficulty`
with the EXACT crashing condition — the deterministic, reproducible repro for
crash (1) (the live chartless-song path can't be booted headlessly; see the
"asset overlay" note below). 3 tests:
- `ChangeDifficultyUnprocessedConfigIsInert` — the crash-(1) repro. **Pre-fix: aborts.
  Post-fix: inert** + difficulty still recorded on the config.
- `ChangeDifficultyProcessedConfigUpdatesTrackDiff` — the NORMAL path still writes
  `mTrackDiffs[trackNum]` at the valid index after `Process()`.
- `ChangeDifficultySpamUnprocessedIsInert` — 24-verb burst stays inert.

---

## Verification (evidence under /tmp/rp7-overpress-harden/)

> GOTCHA that cost time, recorded for the next agent: the prior wave's
> harness scripts hardcode `REPO = "/home/free/code/milohax/rb3"` (the MAIN
> repo) and launch a RELATIVE `native/build-native/rb3-native` with `cwd=REPO`,
> so they run the **main repo's stale master binary**, not the worktree's. Always
> pass the ABSOLUTE worktree binary path (arg 3) AND verify the running binary's
> strings/md5. The hammer copies in /tmp are repointed at the worktree.

**Crash (1) — gtest (deterministic):**
- Pre-fix: `OverPress.ChangeDifficultyUnprocessedConfigIsInert` SIGABRTs
  (`vector::operator[] Assertion '__n < this->size()' failed`), gdb backtrace above.
- Post-fix: **3/3 OverPress tests OK**; full suite (`-CharLoad5b.*`) **17/17 PASSED**
  (no regression).
- In-game charted (worktree binary): in-GAMEPLAY `difficulty:` spam ×24 (out-of-
  order easy/medium/hard/expert/0/3) → **0 crashes**, difficulty applies normally
  (`GetDifficulty` tracks the request); at part_difficulty ×20 → 0 crashes.

**Crash (2) — live game (charted 20thcenturyboy / 25or6to4):**
- Pre-fix (main-repo binary): double `end_override_flow` → **SIGABRT, Line 432
  InOverrideFlow(type)** (reproduced reliably).
- Post-fix (worktree binary): double/×4 `end_override_flow:1:0` + variant types
  (`1:1`, `2:0`, `0:0`) → **0 crashes, HELD ALIVE**, the no-op gate fires 5× on the
  redundant ends; the FIRST end still properly ends the active SongSettings flow
  (`curFlow 1→0`). Repeated 3× across two charted songs, all clean.

**Wii byte-identity (objdiff, both 100.0% — match-neutral):**
- `PlayerTrackConfigList::ChangeDifficulty` — **100.0%** (56/56 instructions equal).
- `OvershellPanel::EndOverrideFlow` — **100.0%** (70/70 instructions equal).
- Both edits are strictly inside `#ifdef HX_NATIVE`; the Wii (non-HX_NATIVE)
  compile is unchanged.

### Note — live chartless repro vs gtest
All 84 songs in the current extract now ship a `.mid` (the extract was refreshed
since wave 6, so the live chartless-song path that the reviewer hit isn't
reproducible from assets right now). Recreating it via a private `RB3_DATA`
overlay (symlink the extract, drop one song's `.mid`) fails to boot — the native
DTA loader hits "Empty merge file (possibly a re-included file)" during App
construction whenever `RB3_DATA` differs from the canonical extract location (a
DTA re-include path quirk, orthogonal to this task). The gtest is therefore the
authoritative crash-(1) repro: it exercises the EXACT crashing function with the
EXACT unprocessed-config condition (`TrackNum()==-1`, empty `mTrackDiffs`), which
is what the in-game `difficulty:` verb chain bottoms out in.

## landingNotes
- 2 source files (`PlayerTrackConfigList.cpp`, `OvershellPanel.cpp`) +
  1 new test (`native/tests/test_overpress.cpp`) + the CMake wire-in. All
  shared-src edits are `#ifdef HX_NATIVE`; Wii byte-identical (objdiff 100.0%
  both fns), so this composes cleanly with any other landing and needs no engine
  pin bump.
- `git commit -- <explicit-path>` only the 4 files (shared repo).
- The `OvershellPanel.cpp` wave-6 `GAME_DBG` entry log is replaced by a focused
  no-op log; if a future review wants the old per-call entry log back, re-add it
  above the guard (still under `getenv("GAME_DBG")`).
- Keeps the debug HTTP surface robust for every future review wave (abnormal
  /api/input verb sequences are now inert instead of fatal).
