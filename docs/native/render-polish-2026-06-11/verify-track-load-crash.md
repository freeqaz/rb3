# verify-track-load-crash — independent adversarial review (Wave 6)

**Verdict: CONFIRM.** The no-chart song track-load SIGABRT (the documented
`SongData::TrackInfo*` vector OOB) is **gone** on the composed master build
(`7a0b9595`, fix `7f7262d0`, engine pin `15ce606`). I could not reproduce the
documented crash post-fix; I reproduced it cleanly on a self-built pre-fix binary;
the 3 charted songs still play. `crashReproducesPostFix = false`.

Reviewer: independent Opus, read-only on shared source, own evidence under
`/tmp/rp6rev-track-load-crash/` (lean; large engine logs deleted after analysis).
Ports 9501–9509. Pre-fix worktree built + torn down.

---

## What I verified (independently, not trusting the impl doc)

### (0) Premise check — "~80 of 83 songs lack a .mid"
Enumerated `orig-assets/extracted/songs/*/`: **84 dirs, exactly 3 ship a `.mid`**
(`20thcenturyboy`, `25or6to4`, `antibodies`), **81 do not**. The impl's "3 charted /
~80 chartless" discriminator is exactly right.

### (a) Chartless songs no longer SIGABRT — 9/9 HELD ALIVE
Drove each through the **exact documented crash-trigger sequence** (scroll to song
by `{{music_library get_highlighted_node} get_token}`, `select_highlighted_node`,
wait for `part_difficulty_screen`, fire `track:guitar` / `difficulty:expert` /
`msg:overshell:end_override_flow:1:0` / `nofail` / `autohit`), then watched 90s for
gameplay-start vs hold vs death (`/tmp/rp6rev-track-load-crash/probe.py`):

| song | result |
|---|---|
| bohemianrhapsody | HELD ALIVE — `tv3_c_screen`, songMs=0, frame 1622→117579, **SIGABRT=0**, no-chart gate fired |
| crazytrain | HELD ALIVE — `tv3_a_screen`, songMs=0, frame→129115, gate fired |
| freebird | HELD ALIVE — songMs=0, frame→128956, gate fired |
| rehab | HELD ALIVE — songMs=0, frame→130189, gate fired |
| imagine | HELD ALIVE — songMs=0, frame→128492, gate fired |
| beastandtheharlot | HELD ALIVE — songMs=0, frame→116339, gate fired |
| beencaughtstealing | HELD ALIVE — songMs=0, frame→115520, gate fired |
| smokeonthewater | HELD ALIVE — songMs=0, frame→103612, gate fired |
| plush | HELD ALIVE — songMs=0, frame→106992, gate fired |

Every chartless log shows **0 `caught SIGABRT`** and the `Game::IsLoaded — selected
song has no chart … Holding at the loading screen` gate message fired exactly once.
App rendering (frame advancing tens of thousands), songMs pinned at 0 (gameplay
never entered) — i.e. the documented hold-at-loading-vignette behavior, app alive.

### (b) Charted songs still load AND play into gameplay — 3/3
All three reached **`game_screen` with songMs > 2000ms**:
`20thcenturyboy` 2220, `25or6to4` 2206, `antibodies` 2211. No regression.
Plus `scripts/native/scoring-test.py` (plays `20thcenturyboy` to **natural EOF**, no
jump) PASSED: peak band score **156,973 pts, 5 stars**, reached `coop_endgame_screen`,
EXIT 0 — the full charted gameplay→scoring→endgame chain is intact post-fix.

### (c) Pre-fix attribution — built a BEFORE binary, it DID SIGABRT
Worktree `wt-prefix-trackload` at `b23fc390` (parent of `7f7262d0` — fix grepped
ABSENT), **same shared engine HEAD `15ce606` = the pinned engine**, built with clang
(`-DCMAKE_CXX_COMPILER=clang++`, `-DDawn_DIR=…`). So the only delta vs the composed
build is the 4-file rb3 fix.

Ran the same chartless `bohemianrhapsody` sequence on the pre-fix binary → it
**SIGABRTed**, and the symbolized backtrace matches the documented one **frame-for-
frame** (`/tmp/rp6rev-track-load-crash/PREFIX-CRASH-backtrace.txt`):

```
std::vector<SongData::TrackInfo*>::operator[] const  <- Assertion '__n < this->size()' failed; SIGABRT (signal 6)
SongData::TrackTypeAt(int) const
NewTrackWatcherImpl(...)  ->  TrackWatcher::SetImpl()  ->  TrackWatcher::TrackWatcher(...)
BeatMatcher::SetTrack(int)  ->  GemPlayer::SetTrack(int)
Game::PostLoad()  ->  Game::IsLoaded()  ->  Game::IsReady()  ->  GamePanel::PollForLoading()
```
Decisive attribution: the crash is real, lives exactly where the doc says, and the
gate at `Game::IsLoaded`/`PostLoad` is precisely where the pre-fix backtrace enters
the chartless pipeline. The fix eliminates it.

### (d) Sanity — gating + match-neutrality + no new charted crash
- All added code is `#ifdef HX_NATIVE`-gated: balanced `#ifdef HX_NATIVE`/`#endif`
  pairs in all 4 files (Game.cpp 4/4, BeatMatcher.cpp 6/6, SongData.cpp 5/5,
  SongData.h 1/1). The new `mHxEmptyGemList` member is **appended after the matched
  layout** (Wii offsets unshifted), null-init in ctor, `RELEASE`d in dtor — sound.
  (I did not re-run objdiff; the gating + append-after-layout is the byte-identical
  guarantee, and the impl's per-fn `target_size==base_size` claim is consistent with
  HX_NATIVE-only edits.)
- `scoring-test.py` green = no new crash on the charted path.

---

## New issue surfaced (NOT the track-load crash; out-of-scope here)

A deliberately **aggressive "hammer"** probe — spamming out-of-order/redundant debug
verbs through `/api/input` (`difficulty:hard`, repeated `track:` switches, **double**
`end_override_flow`) — triggered two *separate* aborts:

- chartless `freebird`: `std::vector<int>::operator[]` OOB in
  `PlayerTrackConfigList::ChangeDifficulty` ← `GameConfig::ChangeDifficulty` ←
  `Player::ChangeDifficulty` ← … ← `ExecDifficulty` (the debug `difficulty:` verb).
- **charted** `20thcenturyboy`: `Debug::Fail("InOverrideFlow(type)")`
  (`OvershellPanel.cpp:432`) in `OvershellPanel::EndOverrideFlow`, from firing
  `end_override_flow` when not in an override flow.

These are **NOT** the documented track-load SIGABRT (different vector, different call
chain via the debug verb handler), and the second one reproduces on a **charted**
song too — so they are independent of the no-chart condition. They are the
pre-existing **"over-press" / debug-verb-misuse class** already on the wave-6 backlog
(`score-detail chain: over-press load_nextsong SIGSEGV`; `/api/dta/eval` debug-handler
hardening). They do not arise from the natural song-load flow my probe.py exercised
(which held alive on all 9 chartless songs). Flagging as a residual, not a reject.

## Documented UX residual (confirmed, not a crash)
Chartless selections hold on the loading vignette (`tv3_*_screen`); `back` doesn't pop
to song-select. Confirmed (songMs stays 0, screen stays `tv3_*`). Follow-up, as noted.

---

**verdict: CONFIRM** — track-load SIGABRT gone (pre-fix crashes, post-fix holds alive),
charted songs unaffected, scoring chain intact. `crashReproducesPostFix = false`.
Residuals: the debug-verb over-press aborts (separate, partly charted-song too) +
the no-back-out UX hold.
