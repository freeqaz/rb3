# Adversarial review — native AV-calibration audio-sync fix

**Verdict: CONFIRM** (branch `wt-audiosync-avlatency` @ `703c7181`)

Re-measured independently on **native** (the affected platform; the bug is in
shared `band3` decomp, present on both native and web). Did NOT trust the FIX.md
numbers — wrote my own probe (`_review_lead_remeasure.py`), drove to gameplay,
parsed the authoritative `GAME_DBG` clock log (`audioTime = GetAudio()->GetTime()`,
`songMs = 1000*TaskMgr.Seconds(kRealTime)` — the exact highway-driving clock).

## A/B re-measure (lead = audio − track; >0 = audio leads = the bug)

| Build | mean lead | range | every sample positive? | n |
|---|---|---|---|---|
| **FIX OFF** (`RB3_NO_AV_CALIBRATION=1`, = Wii) | **+17.5 ms** | [+4, +36] | YES (41/41) | 41 |
| **FIX ON** (default) | **−2.9 ms** | [−16, +16] | NO (symmetric about 0) | 42 |

- FIX OFF reproduces the bug: a systematic ~+18ms bias, audio ahead on *every*
  sample. (FIX.md said +19.5; +17.5 here — same direction & magnitude, the diff
  is sampling + dejitter quantization noise.)
- FIX ON eliminates the bias: residual is centered near zero with a ±16ms spread.
  Confirmed that spread is the shared-decomp `DeJitter::Apply` one-frame
  quantization (`utl/DeJitter.cpp`, literal `ms + 16.0f`), present identically in
  BOTH legs — NOT a leftover bias. The fix did not over- or under-correct, and did
  not merely relocate the offset.

## Did it preserve hit-detection / scoring? YES.

Source-traced the clock all the way through the hit path:
- `Game::Poll` sets `TheTaskMgr.SetSeconds(songMs)` — the **single** `kRealTime`
  clock that (a) positions gems on the highway AND (b) timestamps the player's
  strums.
- gem visual pos: `GemPlayer.cpp:234/242` read `TheTaskMgr.Seconds(kRealTime)`.
- hit timestamp: `Game::Poll → Band::Poll → Performer::Poll` sets `mPollMs = ms`
  (`Performer.cpp:359`); `GemPlayer::Hit` judges `gem.mMs - (PollMs() + mSyncOffset)`
  (`GemPlayer.cpp:360,402`).
- The fix shifts that one clock by +20ms; gem visual and hit timestamp move
  **together**, so the gem-vs-hit `delta` is invariant. `mSyncOffset` (a separate
  `ProfileMgr.GetSyncOffset` term) and the chart `gem.mMs` are untouched.

Independently ran `scripts/native/scoring-test.py` (full song to NATURAL end,
continuous autohit = the real `GemPlayer::Hit` pipeline) with the fix ON:
```
song ended naturally at songMs=236732; peak band score=156973
RESULT: is_invalid_score=0 endgame_band_score=156973 endgame_band_stars=5  → PASS
```
Autohit climbed through the whole chart → chart-vs-clock timing intact.

## Match-neutral? YES.

`git diff master --name-only`: only `native/src/{rb3_synth_native,main_native,
main_web}.cpp` + the doc. **No `src/` shared Wii decomp byte**, no engine change,
no `MILO_ENGINE_PIN` bump. The Wii -20ms behavior is byte-identical and restorable
with `RB3_NO_AV_CALIBRATION=1` (used as the A/B baseline). Verified the math at
source: ctor `mInGameExtraVideoLatency=70, mSongToTaskMgrMs=50` →
`GetSongToTaskMgrMs=-20`; fix sets `mInGameExtraVideoLatency = GetSongToTaskMgrMsRaw()
= 50` → offset `= 0`. `GetSongToTaskMgrMs(LagContext)` ignores the context arg
(returns `mSongToTaskMgrMs - mInGameExtraVideoLatency`), so kGame and practice
contexts all see the zeroed offset.

## Residual concerns (non-blocking)

1. **Boot-time snapshot.** The fix pins `mInGameExtraVideoLatency` to the *boot*
   value of `mSongToTaskMgrMs` (50). `mSongToTaskMgrMs` CAN change at runtime via
   `SetExcessAudioLag` / `set_song_to_taskmgr_ms` (in-game A/V calibration UI / DTA).
   If a user runs calibration and changes it, the offset is no longer exactly 0
   (it drifts by the calibration delta). Not a regression vs. the prior native
   behavior (which had a fixed -20ms anyway) and calibration is a deliberate user
   action, but a follow-up could re-zero after calibration if that UI is ever wired
   up natively. Today no native path mutates it post-boot, so the snapshot holds.
2. The probe samples GAME_DBG once/sec (60-poll stride); 41–42 samples over ~25s
   is enough to establish the systematic bias but each point carries ±1-frame
   dejitter noise — hence I report means + "all-positive" rather than per-frame
   precision. The sign of the result is unambiguous either way.

## Files
- probe: `docs/native/audio-sync-2026-06-21/_review_lead_remeasure.py`
- logs: `/tmp/rb3-remeasure-FIXON-*.log`, `/tmp/rb3-remeasure-FIXOFF-*.log`,
  `/tmp/rb3-scoring-*.log`
