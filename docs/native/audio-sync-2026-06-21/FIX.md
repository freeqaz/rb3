# Audio-vs-track sync fix — native AV-calibration (2026-06-21)

## Symptom
At song start (and every gameplay frame), the song **audio leads the visual note
highway** by ~13–29 ms. The track/gems scroll a hair behind the music, so the
sync "feels off." Reported "NOW" because the default-ON web off-main worklet mix
(engine `805e701`/`2aed9e5`) cleaned up song-start audio, making the
always-present track lag newly *noticeable* — but off-main is NOT the cause (A/B
with `RB3_WEB_OFFMAIN_MIX=0` vs `=1` showed identical audio onset).

## Root cause (per the diagnosis, confirmed in source + empirically)
The note highway is driven from the **audio clock minus a fixed Wii A/V
calibration term**, applied every frame:

- `src/band3/game/Game.cpp:1712-1716`
  ```
  audioMs += GetSongToTaskMgrMs();           // = audioTime + offset
  TheTaskMgr.SetSeconds(songMs / 1000.0f);   // drives BOTH track scroll AND
                                             // the kRealTime hit-timing clock
  ```
- `src/band3/meta_band/ProfileMgr.cpp:1218-1220`
  ```
  GetSongToTaskMgrMs(kGame) = mSongToTaskMgrMs - mInGameExtraVideoLatency
  ```
- Boot defaults (`ProfileMgr` ctor, `ProfileMgr.cpp:62-63,74-75`):
  - `mPlatformAudioLatency = 0`, `mPlatformVideoLatency = 50`
  - `mSongToTaskMgrMs = mPlatformVideoLatency - mPlatformAudioLatency = 50`
  - `mInGameExtraVideoLatency = 70`
  - ⇒ `GetSongToTaskMgrMs(kGame) = 50 - 70 = **-20 ms**`

So the visual highway (and the hit-timing clock) runs ~20 ms **behind** the audio
every frame. On the real Wii the 70 ms `mInGameExtraVideoLatency` compensated the
display + GX pipeline latency; the native/web WebGPU renderer has **no** such
pipeline latency, so the -20 ms is uncompensated and reads as "audio leads."

This is **NOT a recent code regression** — the -20 ms is byte-identical to the
Wii. `mInGameExtraVideoLatency` is set ONLY in the `ProfileMgr` ctor and the
`SetInGameExtraVideoLatency` setter; it is **never reloaded from save data**, so a
one-time boot override sticks. (`ExcessVideoLag` / `window.rb3SetExcessVideoLag`
only move `mSyncOffset`, NOT this term, so they cannot zero it.)

## Platform
**Both native and web.** The lag is in shared `band3` decomp, not the web-only
off-main mixer. Native and web share `src/`, so the fix is in shared native glue
called from both boot paths.

## Fix (native-only, match-neutral — the Wii decomp byte stays untouched)
New free function **`RB3ApplyNativeAVCalibration()`** in
`native/src/rb3_synth_native.cpp` raises `mInGameExtraVideoLatency` to equal
`mSongToTaskMgrMsRaw()` so the offset term is exactly **0**, locking the track
clock to the audio clock:

```cpp
float target = TheProfileMgr.GetSongToTaskMgrMsRaw();   // = 50 (mSongToTaskMgrMs)
TheProfileMgr.SetInGameExtraVideoLatency(target);       // 70 -> 50
// ⇒ GetSongToTaskMgrMs(kGame) = 50 - 50 = 0
```

Called once at boot, right after `TheNativeSettings().InitFromEnv()`, from BOTH:
- `native/src/main_native.cpp` `RunGame()` (native)
- `native/src/main_web.cpp` boot (web)

Opt out with **`RB3_NO_AV_CALIBRATION=1`** to restore the literal Wii -20 ms
behaviour (used here as the A/B baseline).

Why this stays correct for **scoring/hit detection**: the `kRealTime` TaskMgr
clock is BOTH what positions the gems AND what timestamps the player's hits, so
shifting it by +20 ms keeps the input-vs-gem-visual relationship internally
consistent — and now the gem visual sits at the audio position, so a player who
hits to the audio also hits to the gem. Verified: full-song autohit scored
156,973 / 5 stars (see below).

Note: an alternative (`StandardStream::sAudioOffsetMs = +20` via the existing
`synth.dta audio_offset_ms` hook) was rejected — it shifts the AUDIO clock that
hit-detection reads, which is riskier; this fix only touches the derived track
clock.

## Measurement (native, headless, GAME_DBG; `audioTime - songMs` per frame)
Driven to gameplay via `scripts/native/keyboard-to-gameplay.py` (hard guitar,
20thcenturyboy / quickplay first song), lead = `audioTime - songMs` over the
playing frames (`streamPlaying=1`):

| Build | mean lead (audio − track) | range | verdict |
|---|---|---|---|
| **FIX OFF** (`RB3_NO_AV_CALIBRATION=1`, = Wii) | **+19.50 ms** | +4 … +36 | audio leads ~20 ms every frame |
| **FIX ON** (default) | **+2.92 ms** | −16 … +16 | locked; residual is ±1-frame dejitter noise |

The systematic ~20 ms bias is eliminated; the remaining ±16 ms is the 60 fps
dejitter frame-quantization (`TheGamePanel->mDeJitter.Apply`), centered on zero.

Boot log confirms the applied value:
```
RB3 AV-cal: locked track to audio (mInGameExtraVideoLatency=50.0 -> GetSongToTaskMgrMs(kGame)=0)
```

## Scoring / hit-detection regression check — PASS
`scripts/native/scoring-test.py` (full song to NATURAL end, continuous autohit —
the same `GemPlayer::Hit` pipeline a real strum drives, gated on the TaskMgr
clock) with the fix ON:
```
song ended naturally at songMs=236586; peak in-game band score=156973
RESULT: is_invalid_score=0 endgame_band_score=156973 endgame_band_stars=5
PASS: native scorer produced a real, non-zero endgame score (156973 pts, 5 stars).
```
Autohit climbed steadily across the whole song → chart-vs-audio hit timing intact.

## Files changed (all rb3 native glue; NO engine change, NO MILO_ENGINE_PIN bump)
- `native/src/rb3_synth_native.cpp` — `RB3ApplyNativeAVCalibration()` (+ `RB3_NO_AV_CALIBRATION` opt-out). Uses plain `printf` (runs before the Milo string machinery is up — MILO_LOG there segfaults at boot).
- `native/src/main_native.cpp` — call it in `RunGame()` after `InitFromEnv()`.
- `native/src/main_web.cpp` — call it in web boot after `InitFromEnv()`.

## Worktree
- rb3 branch: `wt-audiosync-avlatency`
- engine worktree: clean (untouched, still at the pinned engine SHA)
