# K8 — Gem stream + scoring HUD: blocker analysis

**Status:** root cause identified, three surgical fixes landed, next blocker
documented. Multi-day matched-fork bring-up required to finish.

## Summary

The Opus V5 visual review called gameplay rendering "0% recognizable" because
no gems flow down the highway and the HUD shows `%d%%` instead of a real
score. K8 dispatch hypothesized this was a gameplay-tick-loop wiring problem.

The real cause is upstream of the tick loop: **the band has no active players
during gameplay** because the synthetic-input flow never assigns a track type
to the local user. With `mActivePlayers.size()==0`, nothing in the gem-stream
chain runs:

- `Band::Poll` iterates an empty `mActivePlayers` -> no `GemPlayer::Poll`
- `TrackPanel::Poll` sees `mTracks.size()==0` -> no `GemTrack::Poll`
- No `GemTrack::Poll` -> no `GemManager::PollHelper` -> no visible gems
- No `GemPlayer` -> no scoring callbacks -> `solo_percent.lbl` stays at its
  template default text (`%d%%`)

The Opus review's "no gems, no score" is one and the same symptom.

## Diagnostic findings

With `K8_DBG=1` env var, every link in the chain is instrumented (additive
HX_NATIVE-only logs in `TrackPanel::Poll`, `GemTrack::Poll`,
`GemManager::PollHelper`, `Band::Poll`, `Band::Band`, `BandUser::SetTrackType`,
`GameConfig::AssignTrack`, `Game::PostLoad`). The baseline (no `track:`) shows:

```
K8_DBG: Band::Band participatingUsers=1 perf=... bbb=0 ... autoVocals=0
K8_DBG:   user[0]=... trackSym=none participating=1 partPlays=0 -> SKIP
K8_DBG: Band::Band POST mActivePlayers.size=0
K8_DBG: TrackPanel::Poll gate=0 ... mTracks.size=0 ...
K8_DBG: Band::Poll ms=... mActivePlayers.size=0 totalScore=0 bandPerf.score=0
```

`trackSym=none` because `BandUser::mTrackType` defaults to `kTrackNone`
(BandUser.cpp:55). In the real flow, `OvershellSlot::SelectPart` ->
`pUser->SetTrackType(track)` runs when you pick guitar/bass/drum/vocals on the
part_difficulty screen. The synthetic-input flow
(`msg:music_library:select_highlighted_node` -> `msg:overshell:end_override_flow`)
skips the part_difficulty screen, so the synth user keeps `kTrackNone` and
`MetaPerformer::PartPlaysInSong(none)==false` -> the user is filtered out of
`Band::Band`.

## Fix #1 — synthetic-input `track:<sym>` directive (landed)

`rb3/native/src/rb3_game_input.cpp` — adds `track:guitar` (etc.) directive
that calls `BandUser::SetTrackType` and `BandUser::SetDifficulty`. The
difficulty call is required: `BandUser::IsFullyInGame()` is gated on a bit
(`unk_0xC`) that ONLY `SetDifficulty` toggles. `TrackPanel::CreateTracks`
filters users on `IsParticipating() && IsFullyInGame()` — without difficulty,
the user is dropped from the track list, `GemPlayer::HookupTrack` hits
`MILO_ASSERT(mTrack, 0x890)` and the run crashes.

Tested reproducer:
```bash
RB3_GAME_INPUT="@10:start,@30:confirm,@140:select:pn_quickplay.btn,
@220:select:qp_quickplay.btn,@320:down,
@350:msg:music_library:select_highlighted_node,
@360:track:guitar,
@450:msg:overshell:end_override_flow:1:0"
```

With this, `K8_DBG=1` shows the chain reaching all the way to PostLoad with a
real player:
```
RB3 input: frame 360  track set: ... -> TrackType=1 diff=expert (IsFullyInGame=1)
K8_DBG: GameConfig::AssignTrack ... partPlays=1
K8_DBG: Band::Band POST mActivePlayers.size=1
K8_DBG: Game::PostLoad player=... trackNum=3 userSym=guitar
```

(Note: track type must be set AFTER the part_difficulty screen enters, because
part_difficulty entry resets track to `kTrackNone`. Frame 360 in the script —
between `select_highlighted_node` at 350 and `end_override_flow` at 450 —
works.)

## Fix #2 — SongParser::AnalyzeTrackList missing assignment (landed, HX_NATIVE)

`rb3/src/system/beatmatch/SongParser.cpp:1764` — the decompiled
`AnalyzeTrackList` has a fall-through where the success branch (`num != -1`,
meaning the audio track was found in songs.dta) does NOT call
`audio_track_num.Set(num)`. Result: every part's `audio_track_num` stays at
the `PartInfo` ctor default of `-1`, which `SongParser::OnTrackName` then hands
to `SongData::AddTrack` as the `AudioTrackNum a` parameter. Crash chain:

```
Game::PostLoad -> player->SetTrack(playerTrackNum=3) ->
BeatMatcher::SetTrack(i=3) -> MasterAudio::SetTrack(uguid, i=3) ->
TrackNumAt(3) = mSongData->mTrackInfos[3]->mAudioTrackNum = -1 ->
mTrackData[(size_t)-1] -> vector OOB assertion
```

Diagnostic verified `AudioTrackNum=-1 mTrackData.size=5` for the guitar
player. Fix is a 1-line additive HX_NATIVE `else` clause that mirrors the
fake-audio branch's `.Set(num)`. With the fix `AudioTrackNum=2` (within range)
and `MasterAudio::SetTrack` succeeds.

This is likely either a permuter-elided assignment (a `cfg.mTrackNum = num`
line that the permuter dropped because it generates equivalent asm against the
matched PPC binary) or a real decomp regression. The HX_NATIVE block is
additive, so it's safe even if the matched PPC code path was correct via some
other mechanism — on native we just need the value populated.

## Fix #3 — `dynamic_cast<VocalPlayer*>` RTTI stub crash (landed)

`rb3/src/band3/game/Game.cpp:813` — `Game::SetVocalPercussionBank(Player*)`
unconditionally `dynamic_cast`s to `VocalPlayer*`. `VocalPlayer.cpp` is in
`_NATIVE_FORK_EXCLUDE` so its RTTI is a zeroed-out stub
(`band3_link_stubs.s:1272 _ZTI11VocalPlayer: .zero 256`). Walking
__dynamic_cast through that stub segfaults during Game::FinishLoad before
gameplay ever starts. Gated the cast on `p->GetTrackType() != kTrackVocals`
under HX_NATIVE — non-vocal players short-circuit. Safe for V1 (vocals deferred
per V1_REMAINING_PLAN §4).

## Blocker — SIGILL at game_panel_load (NEW V1 FRONTIER)

With all three fixes above, the run advances from "0 players, no gameplay,
silent frames 08-11" to "1 player wired, gameplay-setup begins, SIGILL at
~frame 456 inside game_panel_load". The last successful frames before the
crash show:

```
K8_DBG: Game::PostLoad player=... trackNum=3 userSym=guitar
mChanParams.size() == 15
mChannels.size() == 15
GAME_DBG: *** Game::mLoadState = kReady — audio loaded, PushAllOptions next ***
STREAM_DBG: StandardStream::PollStream state 1 -> 2 (chans=15)
BandRnd: frame drawn — 6 meshes, 2040 tris
RB3 Native: frame 455 complete
game_panel_load: 107.39
... NOTIFY: stagekit_reset / set_song_name / set_user_name unhandled ...
[SIGILL]
```

The SIGILL is in matched-fork code reached only when a player is actually
populated (the previous "silent gameplay frames" path bypassed every
script-handler that derefs the player). It's not catchable by the existing
SIGSEGV/SIGABRT handler in `main_native.cpp`, so the stack isn't dumped.

### Top-3 specific bring-ups needed to clear K8

1. **Catch SIGILL in the signal handler** (10 min). Add SIGILL to the
   `sigaction` list in `rb3/native/src/main_native.cpp` so the backtrace
   prints. This is mechanical and unblocks #2 + #3.

2. **GamePanel::pick_intro script handler chain** (1-2 days). The crash
   appears to be inside `GamePanel::Handle` processing the `pick_intro_msg`
   handler chain. The DTA script (`ui/game.dta:30 pick_intro`) does
   `{handle ($this intro_start)}` ->
   `{{gamemode get game_screen} set_showing TRUE}`. Something in this
   script-dispatched chain hits a stubbed class. Candidates:
   - `RB3WiiManager` (Wii-platform manager, stubbed)
   - `TrackPanelDir` virtual dispatch (rndobj-coupled — partially compiled?)
   - Stagekit (`stagekit_reset` NOTIFY shows this is also unhandled)

   Need a real backtrace from #1 first to narrow down.

3. **`_NATIVE_FORK_EXCLUDE` cleanup for gameplay-required TUs** (2-3 days).
   The Player-populated path likely also hits other zeroed-RTTI / stubbed
   class crashes in:
   - `BandPatchMesh` (band visual)
   - `Singer` / `VocalPlayer` / `VocalNoteList` (if `kTrackVocals` ever
     reached)
   - `TourPerformerLocal` (in tour mode — quickplay should bypass)

   The `Game::SetVocalPercussionBank` pattern (Fix #3 above) is the template:
   gate the dynamic_cast on track-type or null-check first. Audit all
   `dynamic_cast<VocalPlayer*>` / `dynamic_cast<Singer*>` / etc. sites and
   apply the same gating pattern.

## What this dispatch shipped

Per-file changes:

- `rb3/native/src/rb3_game_input.cpp` — `track:<sym>` directive (~25 lines)
- `rb3/src/system/beatmatch/SongParser.cpp` — HX_NATIVE additive (~15 lines
  including comment)
- `rb3/src/band3/game/Game.cpp` — VocalPlayer cast gate (~10 lines additive)
- Diagnostic logs (all `K8_DBG=1`-gated, env-off = zero cost):
  - `rb3/src/band3/bandtrack/TrackPanel.cpp` — entry/gate logs
  - `rb3/src/band3/bandtrack/GemTrack.cpp` — entry log
  - `rb3/src/band3/bandtrack/GemManager.cpp` — visible gem count
  - `rb3/src/band3/game/Band.cpp` — player count + ctor user filter
  - `rb3/src/band3/game/BandUser.cpp` — SetTrackType audit
  - `rb3/src/band3/game/GameConfig.cpp` — AssignTrack partPlays
  - `rb3/src/band3/game/Game.cpp` — PostLoad trackNum

## Reproducer (post-K8)

```bash
# Path A (V5 baseline, completes 1500 frames, no gameplay):
RB3_GAME=1 MILO_HEADLESS=1 MILO_AUDIO=1 \
  RB3_DATA=$RB3_EXTRACTED MILO_MAX_FRAMES=1500 \
  RB3_GAME_INPUT="@10:start,@30:confirm,@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn,@320:down,@350:msg:music_library:select_highlighted_node,@450:msg:overshell:end_override_flow:1:0" \
  rb3-native

# Path B (K8 frontier — players wired, gameplay-setup SIGILLs ~frame 456):
RB3_GAME=1 MILO_HEADLESS=1 MILO_AUDIO=1 K8_DBG=1 \
  RB3_DATA=$RB3_EXTRACTED MILO_MAX_FRAMES=1500 \
  RB3_GAME_INPUT="@10:start,@30:confirm,@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn,@320:down,@350:msg:music_library:select_highlighted_node,@360:track:guitar,@450:msg:overshell:end_override_flow:1:0" \
  rb3-native
```

## Honest assessment

**Did gems appear?** No. The gameplay-render path SIGILLs before any
gem-track Poll can run.

**Did `%d%%` resolve?** No. The label is rendered by the same panel chain
that doesn't run because the player-load chain crashes before reaching it.

**New V1 frontier:** the next-fix is to land SIGILL in the signal handler,
get a real backtrace, then either (a) gate the offending dynamic_cast (like
Fix #3) if it's another stubbed-RTTI cast, or (b) bring up the offending TU
from `_NATIVE_FORK_EXCLUDE` (per V1_REMAINING_PLAN K7).

The K8 dispatch found and named the actual root cause and shipped two of the
three "land it" fixes. The visible-screenshot result is unchanged from V5
because we held the baseline path intact (the `track:guitar` directive is
opt-in). The K8 diagnostic infrastructure remains in place for the next
dispatch to use.
