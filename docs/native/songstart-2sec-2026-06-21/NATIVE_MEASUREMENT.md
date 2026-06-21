# Song-start "2-second" A/V gap — NATIVE measurement pass (2026-06-21)

**Worktree:** rb3 `wt-introsync-2sec` (from `ecc29d6c`); engine worktree clean
(untouched, at pinned `b8f3cfa`). Native-only diagnostic instrumentation, no
engine change, no MILO_ENGINE_PIN bump.

## TL;DR — the gap is REAL, and it's ~6.8s, NOT ~2s

Driving the FULL song-start (boot → song_select → confirm → part/diff →
game_screen, **not** quick-jumping past the intro) and tracing every frame:

| marker | value |
|---|---|
| **t_track_start** (f=0) | trackRtMs = **−6666.7 ms** (run A) / **−6833 ms** (run B) — the song clock starts deep negative |
| **t_audio_audible** (audio clock first advances, `streamPlaying` 1) | **wallMs ≈ 6659 ms** (run A) / **6815 ms** (run B) |
| **measured GAP (track-start → audio-audible)** | **6659 ms (run A) / 6815 ms (run B)** |
| steady-state once playing | trackRtMs == songMs (AV-cal locked), audioMs ≈ 16 ms behind — the prior d6f56d23 per-frame fix is intact |

So the song-start gap is **~6.7s**, even larger than the user's "~2s" estimate.
This is the song's **intro / count-in window** and it is the *same root cause*
as the prior pass's logged-but-unchased clue ("countdown scrolls the track while
audio is silent, then audio starts at the first playing frame").

## The mechanism (state machine, confirmed in source + trace)

The intro window is the engine's **negative-clock real-time count-in**:

1. DTA fires `set_intro_real_time <negative>` → `Game::SetIntroRealTime(f1)`
   (`src/band3/game/Game.cpp:630`): `TheTaskMgr.SetSeconds(f1, true)` seeds the
   clock at e.g. **−6.667 s**, `mHasIntro = (f1<0) = true`, `SetRealtime(true)`.
2. While `mHasIntro`, `Game::Poll` runs `mRealtime` so the clock = wall-clock
   (`mTimeOffset + Timer::CyclesToMs`) — it counts **up from −6.667 s toward 0 in
   real time**. The note highway / venue-intro cinematic advances during this
   whole window. **Audio is NOT playing** (`MasterAudio::Play()` has not been
   called); `MasterAudio::GetTime()` (== `/api/health songMs`) is pinned at 0.
3. `GamePanel::Poll`: when the realtime clock crosses ~0
   (`TheTaskMgr.Seconds(kRealTime) > −0.025`, `GamePanel.cpp:371`) →
   `StartGame()` → `mGame->Start()` → **`Game::Go()`** (`Game.cpp:483`) →
   `mMaster->GetAudio()->Play()`. Audio starts here, **at clock ≈ 0**, in sync
   with the gems at the now-bar.

Trace events (run A), absolute steady_clock ms:
```
INTROSYNC EVENT SetIntroRealTime(-6.667 s = -6666.7 ms)   ← intro window opens
INTROSYNC EVENT Game::Go() trackRtMs=-21.4 hasIntro=0     ← clock reached ~0
INTROSYNC EVENT MasterAudio::Play()                       ← audio finally starts
INTROSYNC f=541 wallMs=6659 trackRt=-10.5 audio=1.1 streamPlaying=1  ← first audible
```

## Is audio LATE or is the track EARLY? → the TRACK is EARLY (by design), and the intro is missing its OWN audio

- The **song stems** are NOT late relative to clock 0 — they start essentially at
  audioMs≈0 the moment `Go()` fires, and once playing the gems and music are in
  sync (steady-state matches the prior pass). Sample 0 of the mogg == song clock
  0; there is genuinely no song-stem audio before clock 0. **This negative
  count-in is faithful to the Wii** (chart-driven `set_intro_real_time`).
- The bug the user feels is that **the entire ~6.8s intro plays with NO audio at
  all** — no crowd ambience, no venue-intro cue, no count-in. On the real RB3
  this window has the venue/crowd audio (the **`CrowdAudio` venue_intro mogg +
  crowd-excitement loops**, `src/system/bandobj/CrowdAudio.cpp`:
  `mShouldPlayVenueIntro` / `mVenueIntro = streamArr->FindArray("venue_intro")` /
  `PlayLoop`). Visually the intro is the **venue/band cinematic** (screenshots
  `intro_00..08` = venue doorway + band on stage; the note highway only appears
  at `intro_10` when audio starts). The song is "Antibodies / Poni Hoax".

### Evidence the crowd/venue-intro audio source is simply absent on native
In the full intro run, the **only** mogg decrypted/streamed is the SONG mogg
(`MOGG_DBG` chain). There is **zero** `CrowdAudio` / `venue_intro` / crowd-loop
mogg activity in the log. So the intro is silent because the crowd/venue-intro
audio source is never loaded/played in the native port — a separate, missing
audio source from the song stems.

## Two distinct issues (do NOT conflate)
1. **Faithful (leave alone):** the ~6.8s negative-clock count-in itself. Song
   stems correctly start at clock 0; this matches the Wii. The prior d6f56d23
   per-frame AV-cal (~20 ms) is a third, separate, already-fixed issue.
2. **The real gap (the "no audio during intro" / "2s+ lag" report):** the intro
   window has **no crowd/venue-intro audio** on native. Candidate root cause =
   `CrowdAudio` (`venue_intro` mogg + excitement loops) not being loaded/polled
   in the native/web audio path. This is the thing to fix; it's an engine
   `src/audio`/synth + `CrowdAudio` bring-up question, **native-only** (the Wii
   song-start byte stays untouched).

## How to reproduce / re-measure
Instrumentation added (native-only, `#ifdef HX_NATIVE`, zero cost when env unset):
- `src/band3/game/Game.cpp` — `RB3_INTROSYNC=1` env gates: a per-frame
  `INTROSYNC f=… trackRtMs/songMs/audioMs/streamPlaying/realtime/hasIntro/gameState`
  line in `Game::Poll`, plus `INTROSYNC EVENT` lines at `SetIntroRealTime`,
  `Game::Go()`, and `MasterAudio::Play()`. Helper `RB3IntroSyncWallMs()`
  (steady_clock ms) is file-local under `#ifdef HX_NATIVE`.

Run:
```bash
RB3_INTROSYNC=1 GAME_DBG=1 python3 scripts/native/keyboard-to-gameplay.py \
    --diff hard --out /tmp/introsync-native --port 8856
# then parse INTROSYNC lines from /tmp/rb3-kbd2game-8856.log
```
Intro-window screenshots (proves the venue cinematic is silent across t=0..6.8s):
`/tmp/introsync_capture.py` → `/tmp/introsync-intro-shots/intro_*_song*.png`
(filenames encode elapsed-ms + the audio clock; `song0` = silent, `song903` =
audio started).

## Next (for the web + fix passes)
- WEB: A/B `RB3_WEB_OFFMAIN_MIX=0` vs default-on to see if off-main
  prime/decode-ahead widens the first-audible gap at the clock-0 handoff (the
  song-stem `Go()` moment). Native shows the stems start promptly at clock 0, so
  off-main can only matter for the first few hundred ms of the song proper, not
  the 6.8s intro — but confirm.
- FIX: bring up `CrowdAudio` venue-intro + crowd loops on native/web so the intro
  window is not silent (engine `src/audio`/synth + `CrowdAudio` source loading).
