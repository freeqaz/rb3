# PAUSE-TIMER lane — triage 2026-07-13

**User report:** "when you pause + resume you will still instantly fail because the timer
continues to advance." I.e. during pause the song clock keeps running, so on resume the
player has missed several seconds of gems and the crowd meter kills them.

**Verdict: DID NOT REPRODUCE on native (HEAD build).** The pause chain is intact. The song
clock (`MasterAudio::GetTime()`) **freezes** during pause and **rolls back** on resume;
`is_game_over` / `lost` stay 0 across a 25-second pause. The mechanism the report describes
is not present in the current native binary. Confidence: **medium** (headless runtime proves
the timer-freeze path; the real-audio-device / web worklet freeze path is verified by code
reading only — headless cannot exercise it).

---

## 1. Reproduction (2 native boots, port 8903)

Harness: `/tmp/triage-0713/pause/pause_repro.py` and `pause_confirm.py` (copied patterns from
`scripts/native/song-end-test.py`; own port 8903, never touched 8080). Boot to guitar/expert
quickplay gameplay, then probe live via `/api/dta/eval` and `/api/health`.

Probes used (all real engine accessors):
- `/api/health` `songMs` == `MasterAudio::GetTime()` (the raw audio-stream clock) — see
  `native/src/rb3_http_handlers.cpp:1257-1265`.
- `{game get_song_ms}` == `Game::GetSongMs()` == `mMaster->GetAudio()->GetTime()`
  (`src/band3/game/Game.cpp:570`).
- `{game get_paused}` == `Game::mIsPaused` (`Game.cpp:1298`).
- `{game is_game_over}`, `{game lost}`, `{game is_playing}` (`GamePanel.cpp:613-616`).

Two pause entry paths tested — both froze the clock:
- Real user path: `overshell:show_options` input verb (the Start-button pause menu).
- Direct: `{game set_overshell_pause TRUE}`.

### Result — confirm run (`confirm_samples.json`, 36 rows)

| phase | t (s) | health songMs | get_paused | is_game_over | lost |
|---|---|---|---|---|---|
| baseline | 0–2 | 2578 → 4656 (~1x) | 0 | 0 | 0 |
| PAUSE (25 s) | 5.5–30.4 | **5895.2, frozen every sample** | 1 | 0 | 0 |
| RESUME | 32.6 | 4077.9 (**rolled back ~1.8 s**) | 0 | 0 | 0 |
| resumed | 33.6–39.8 | 5294 → 11457 (~1x) | 0 | 0 | 0 |

The first repro run (`samples.json`, 34 rows) is identical in character: clock froze at
7033.2 ms for 24 s across both the overshell and direct pause paths, then rolled back to
5641 ms on resume and climbed at ~1x.

Baseline clock rate measured 1.03–1.06x wall-clock (within headless jitter) — **no fast
master clock** on native. (Relevant to the sister ANIM-SPEED lane: the shared-clock-runs-fast
hypothesis is not supported by native data here — but I did not chase that lane's repro.)

Screenshots (`01_gameplay.png`, `02_paused.png`, `03_resumed.png`) are 1280x720 PNGs but
render blank/identical in headless (no offscreen compositing of the game_screen + overshell
overlay). They are not useful visual evidence; the numeric DTA state is authoritative.

---

## 2. Root cause / mechanism map — why native is correct

The reported "timer keeps advancing during pause" would require the song clock source to keep
climbing while `mIsPaused`. It does not, because of **two independent gates**, both wired:

**Gate A — game sim early-return.** `Game::Poll()` returns immediately while paused
(non-realtime): `src/band3/game/Game.cpp:1675-1681`
```
if (mIsPaused) { if (mRealtime && unk148) { /*fall through*/ } else { return; } }
```
So the gem/crowd/fail sim and the `TheTaskMgr.SetSeconds(...)` write at `Game.cpp:1751` do not
run while paused. The task-manager song clock is frozen at the last pre-pause value.

**Gate B — audio-stream clock freeze.** `Game::GetSongMs()` reads `MasterAudio::GetTime()` →
`StandardStream::GetTime()` → `mLastStreamTime` (`StandardStream.cpp:784-792`), which is only
advanced by `UpdateTime()`. Pause propagates:
`Game::UpdatePausedState` (`Game.cpp:1464-1466`, gated on `unk148`)
→ `MasterAudio::SetPaused(true)` → `mSongStream->Stop()`
(`MasterAudio.cpp:517-520`)
→ `StandardStream::Stop()`: `mState=kStopped; mTimer.Stop()` (`StandardStream.cpp:537-545`)
→ per channel `StreamReceiver::Stop()` → `PauseImpl(true)`
(`StreamReceiver.cpp:162-168`).

The clock then cannot climb by either sub-path:
- **Headless (this run):** `mUseTimerFallback` is active, so `GetTime` returns `mTimer.Ms()`
  (`StandardStream.cpp:821-824`), and `mTimer.Stop()` froze it. (This is the path my runtime
  evidence exercised.)
- **Real audio device / web:** `GetRawTime()` = `mChannels[0]->GetBytesPlayed()/2/sampleRate`
  (`StandardStream.cpp:779-781`), driven by the play cursor `mAudioReadPos`. Native
  `RenderAudio` honors pause and does **not** advance the cursor:
  `native/src/rb3_stream_receiver_native.cpp:343`
  ```
  if (mPaused || !mPlayStarted) { memset(output,0,...); return frameCount; }  // no cursor advance
  ```
  Web off-main worklet mirror: `out->paused = mPaused`
  (`rb3_stream_receiver_native.cpp:529`) is published as `bit0=paused`
  (`../milo-native-engine/src/audio/AudioDevice_Web.cpp:1016`), and a paused stem's
  `readTotal` does not advance — "the worklet holds its cursor while paused"
  (`AudioDevice_Web.cpp:925-926`).

**Resume** correctly re-plays and rolls the clock back ~2 s: `Game::UpdatePausedState`
`Rollback(ms, ms-2000)` at `Game.cpp:1468-1481` — matching the observed 5895 → 4078 ms snap.

So the pause chain is faithful to Wii and functions on native.

---

## 3. Why the user still saw it (hypotheses — not runtime-confirmed)

Native does not reproduce, so the report is either stale or a path headless can't drive:

1. **Report predates the current audio-stream work.** Git history shows the pause-honoring
   machinery is recent: off-main worklet stem-mix bridge (`cde73588`), continuous-play ring
   consumer / song-MOGG silence fix (`10af87ab`), content-skip fix (`5c5eb8a8`),
   under-run concealment (`7b8e9394`). A build from before the `RenderAudio` `mPaused` gate /
   the off-main `paused` flag would have kept the audio play-cursor climbing during pause →
   `GetSongMs()` climbs → resume snaps forward → missed gems → instant fail. **Most likely
   the report was filed against such an earlier build and is already fixed.**
2. **Real-audio-device residual (headless blind spot).** My runtime evidence proves the
   headless `mTimer.Stop()` freeze only. Gate B's device sub-path (RenderAudio cursor-hold /
   worklet cursor-hold) is verified by code reading, not runtime. If any web mix path fails to
   publish `mPaused` for a given stem/config, that stem's cursor would climb. Considered
   low-risk (wiring confirmed at `rb3_stream_receiver_native.cpp:529`) but not runtime-proven.

---

## 4. Proposed fix / next step

**No code change is warranted from native evidence** — the defect does not reproduce and the
chain is correct. Recommended disposition:

1. **Confirm the build the reporter used.** If it predates `cde73588`/`5c5eb8a8`, close as
   already-fixed. This is the primary action.
2. **If still reproducible for the reporter (esp. web release):** capture on the actual
   platform — record `MasterAudio::GetTime()` (or `/api/health songMs`) across a pause on that
   build. The single point to verify is that the play cursor (`mAudioReadPos` native /
   worklet `readTotal` web) does not advance while `mPaused`. The one-line assertion to add if
   a regression is found is at the audio render/mix seam, not in shared decomp code.
3. **Decomp-safety note:** `kNoteMissMs` / crowd-meter / fail logic lives in byte-identical
   Wii `band3` game code (`Game.cpp`, `GamePanel.cpp`, `GemPlayer.cpp`) — do **not** edit
   those for a native/web fix. Any correction belongs in the native audio layer
   (`native/src/rb3_stream_receiver_native.cpp`, `../milo-native-engine/src/audio/*`) and must
   be `HX_NATIVE`/`__EMSCRIPTEN__`-gated so the Wii object stays byte-identical. The existing
   gates (`RenderAudio:343`, `out->paused`:529, worklet cursor-hold) are already so gated.

---

## Evidence files
- `/tmp/triage-0713/pause/pause_repro.py`, `pause_confirm.py` — drivers
- `/tmp/triage-0713/pause/samples.json` (34 rows), `confirm_samples.json` (36 rows)
- `/tmp/triage-0713/pause/engine-8903.log`, `engine-confirm-8903.log` — engine logs
- `/tmp/triage-0713/pause/0{1,2,3}_*.png` — headless screenshots (blank; not load-bearing)
