# task-track-load-crash — impl (Wave 6)

**Status: DONE.** The top wave-6 blocking crash is fixed and verified. Branch
`wt-task-track-load-crash`, commit `6dea39f9` (rb3/native-side, HX_NATIVE-only,
Wii byte-identical — no engine-repo change needed).

## Symptom / repro

Selecting almost any song and entering gameplay hard-crashed the whole app with a
SIGABRT on track load:

```
.../bits/stl_vector.h:1272: ... std::vector<SongData::TrackInfo*>::operator[]:
Assertion '__n < this->size()' failed.
RB3 Native: caught SIGABRT (signal 6)
```

Repro harness: `/tmp/rp6-track-load-crash/crashscan.py` (one fresh boot per song;
selects by shortname over HTTP via `{{music_library get_highlighted_node}
get_token}`, fires the `capture_song_gameplay.py` nav, flags an abort iff the
process dies on its own or the log shows an assert). gdb backtrace driver:
`/tmp/rp6-track-load-crash/gdb-driver.py SONG PORT`.

## The crashing songs — it's ~80 of 83, NOT "several"

The reviewer said "several songs"; the empirical scan shows the real scope is
**every song whose `.mid` chart is absent from the extract**. The extract ships a
`.mid` for only **3** songs — `20thcenturyboy`, `25or6to4`, `antibodies` — which
play fine. **The other ~80 all crashed** (`beastandtheharlot`, `bohemianrhapsody`,
`crazytrain`, `freebird`, `rehab`, `imagine`, … — confirmed across the scan, all
SIGABRT at the same site). The "song-specific" read was because earlier reviewers
only happened to hit a few; the discriminator is just chart-present vs chart-absent
(`ls orig-assets/extracted/songs/<name>/*.mid`).

## Backtrace (before the fix)

```
#4 std::vector<SongData::TrackInfo*>::operator[]   (assert __n < size())
#5 SongData::TrackTypeAt(int) const                  <- mTrackInfos[idx], idx OOB
#6 NewTrackWatcherImpl(...)
#7 TrackWatcher::SetImpl()
#8 TrackWatcher::TrackWatcher(...)
#9 BeatMatcher::SetTrack(int)
#10 GemPlayer::SetTrack(int)
#11 Game::PostLoad()
#12 Game::IsLoaded()  ...  App::RunOneFrame
```
(full saved at `/tmp/rp6-track-load-crash/BEFORE-backtrace.txt`)

## Root cause

`SongData::Load` (`src/system/beatmatch/SongData.cpp:127`) hits the native
missing-asset boundary, logs "MIDI chart absent from the extract", and **returns
early without parsing tracks** — so `mNumTracks == 0`, `mTrackInfos` empty, no
tempo map, empty `mGemDBs`/`mTrackDifficulties`/`mPhraseDBs`. The console always
ships a chart, so this state is impossible there and the gameplay pipeline never
guards against it.

Natively the higher-level `Game` machinery still treats the song as loaded and
runs the entire gameplay/gem/track/visual pipeline, every layer of which indexes
the empty vectors. This is **not** a single OOB — it's a cascade. Guarding leaf
accessors only moves the abort to the next layer (proven by iterative gdb):

| layer (each a separate gdb abort) | OOB site |
|---|---|
| watcher setup | `SongData::TrackTypeAt` -> `mTrackInfos[idx]` |
| audio restore (`GamePanel::Enter`->`Game::Restart`->`Band::Restart`->`GemPlayer::JumpReset`->`MasterAudio::RestoreDrums`) | `SongData::GetAudioTrackNum` -> `mTrackInfos[idx]` |
| visual track panel (`TrackPanel::AssignTrack`->`GemTrack::PlayerInit`->`GemPlayer::GetMaxSlots`) | `BeatMatcher::GetMaxSlots` -> `mTrackTypes[mCurTrack]` |
| gem manager init (`GemManager::SetupGems`->`SongDB::GetGems`) | `SongData::GetGemList` -> `mTrackDifficulties[track]` |
| gem-track masks (`GemManager::DrawTrackMasks`->`SongDB::GetCommonPhraseID`) | null `mPhraseAnalyzer`->`GetRawPhrases()` SIGSEGV |
| harness autohit verb | `BeatMatcher::SetAutoplay`->`mWatcher->SetCheating` (null mWatcher) |

The pre-existing `SongData::TrackHasIndependentSlots` HX_NATIVE guard
(`SongData.h:190`) was the same class of bug — it had patched exactly one of these
leaves. A chartless song genuinely cannot play (no notes, no audio), so the only
correct outcome is **graceful no-gameplay**, not aborting the whole app.

## Fix (files + why) — commit `6dea39f9`

All changes are `#ifdef HX_NATIVE`-gated → the Wii MWCC build is byte-identical
(HX_NATIVE is defined only for the native target, `native/CMakeLists.txt:371`).

1. **Primary gate — `src/band3/game/Game.cpp`, `Game::IsLoaded()`** (the single
   kLoadingSong→gameplay transition): when `mSongDB->GetData()->GetNumTracks()==0`,
   log once and `return false` to **hold the load incomplete** so the GamePanel
   stays on the loading vignette and never enters the chartless pipeline. App
   stays alive; user backs out. This alone prevents the whole cascade.
2. **Defense-in-depth — `Game.cpp`**: `Game::PostLoad` skips the per-player
   `SetTrack`/`PostLoad` loop; `Game::Poll` early-returns (avoids `CalcSongPos`
   null-tempo-map deref + `mMaster->Poll`); `Game::Restart` early-returns (avoids
   the `Band::Restart`->`RestoreDrums` chain).
3. **`src/system/beatmatch/BeatMatcher.cpp`**: null-guard the `mWatcher` derefs
   reachable when a song never set a track — `Poll`/`Jump`/`Restart`/
   `ResetGemStates`/`SetAutoplay`/`AutoplayCoda`/`SetAutoplayError`/
   `Cycle|SetAutoplayAccuracy`/`Enable`/`FretButtonDown`/`RGFretButtonDown`/
   `FretButtonUp`/`Swing`/`NonStrumSwing` (the last five also avoid
   `GetGemList(mCurTrack==-1)`), plus `GetMaxSlots` returns the 5-slot default for
   `mCurTrack<0`.
4. **`src/system/beatmatch/SongData.{h,cpp}`**: `GetGemList`/`GetGemListByDiff`
   hand back a shared, lazily-allocated, valid EMPTY `GameGemList` (new
   HX_NATIVE-only member `mHxEmptyGemList`, freed in the dtor) when the track is
   out of range, so every gem query degrades to "0 gems".

## Verification

- **Match-neutral (Wii byte-identical):** rebuilt the 3 touched units with MWCC;
  objdiff on every touched function shows `target_size == base_size` and unchanged
  fuzzy% (`Poll`/`SetAutoplay`/`GetGemList`/`GetAudioTrackNum`/`SongData::~`/
  `Game::PostLoad`/`IsLoaded` = 100.0; `GetMaxSlots` 91.x, `Game::Restart` 99.98,
  `Game::Poll` 99.79 — all PRE-EXISTING levels, sizes equal ⇒ zero added
  instructions). Overall report code% 62.89292 (unchanged). The HX_NATIVE code is
  preprocessed away before MWCC sees it.
- **Charted songs still play (no regression):** `20thcenturyboy`, `25or6to4`,
  `antibodies` all reach gameplay (`phase=playing_ok`, `gameplay=True`).
- **No-chart songs no longer crash:** `bohemianrhapsody`, `crazytrain`, `freebird`,
  `rehab` (and the scan's full no-chart set) hold at the loading vignette with
  **zero SIGABRT/SIGSEGV**. gdb on bohemianrhapsody after the fix: 0 aborts,
  process held alive to the 200s timeout (`gdb-bohemian-7.txt` exit 124).
- **App alive + responsive (not hung):** drove bohemianrhapsody into the
  hold-at-loading state and fired the exact `track:guitar`/`difficulty:expert`/
  `end_override_flow` verbs that previously SIGABRTed — `/api/health` kept
  answering, `frame` advanced 2785→5950 (actively rendering), `songMs=0`
  (gameplay never started), `proc_alive=True` throughout, 0 aborts in the log
  (`/tmp/rp6-track-load-crash/alive-check.py`).

## Landing notes

- rb3/native-side only; **no engine-repo change, no pin bump.** Cherry-pick
  `6dea39f9` from `wt-task-track-load-crash` onto master (touches `Game.cpp`,
  `BeatMatcher.cpp`, `SongData.cpp`, `SongData.h`). No overlap with the other
  wave-6 commits (b4bcbe39 synth, 3d2356b8 dta-eval, 736d89d5 scoring-verify).
- The worktree base carries unrelated uncommitted changes from concurrent agents
  (`native/src/rb3_http_handlers.cpp` — a `{rb3_highlighted_song}` repro probe
  from this task's first, rate-limited dispatch; NOT needed by the final fix since
  the harness uses `{{music_library get_highlighted_node} get_token}` directly)
  and committed sibling wave-6 work. Only the 4 fix files were staged/committed.
- Residual (follow-up, NOT a crash): the loading-vignette `back` input doesn't pop
  the `tv3_b_screen` to song-select, so a no-chart selection holds on the vignette
  until the user navigates away another way. A nicer UX would route a chartless
  load straight back to song-select (a song-flow/overshell change). Out of scope
  here — the blocking-crash deliverable (app survives, ~80 songs no longer take it
  down) is done.
- Evidence (lean) under `/tmp/rp6-track-load-crash/`: `BEFORE-backtrace.txt`,
  `AFTER-clean.txt`, `crashscan.py`, `gdb-driver.py`, `alive-check.py`,
  `scan.out`.
```
```
