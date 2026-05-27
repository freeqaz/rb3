# RB3 native — Boot-to-Song milestone tracker

> **🏁 STATUS: ACHIEVED (2026-05-27).** The real game boots and steps through
> the full sequence to `Game::LoadSong()`, stopping gracefully at the documented
> `.mid`/`.mogg` asset bound. **Next milestone (v1, one song end-to-end) lives
> in [V1_ONE_SONG.md](V1_ONE_SONG.md).** This file remains as the durable
> per-layer fix history for the boot-to-song layer (~1100 lines of task log).

**Goal:** Get a song loaded in RB3 by booting the game and stepping through the
full *real* sequence — App init → `SystemPreInit`/`SystemInit` → `UIManager` DTA
boot script → main menu → menu navigation → song select → song load.

**Guiding principle (non-negotiable):** avoid hacks. Retain the actual game code
as much as possible. Diverge only where native platform differences force it,
gated under `#ifdef HX_NATIVE`. Mirror DC3's boot-to-gameplay approach (DC3 is
the model — it already reached gameplay + audio). When a DC3 sister file has an
`HX_NATIVE` block for the same problem, port *that*, not an ad-hoc fix.

This is the durable handoff artifact for this milestone. Subagents read this for
context and append findings. The roadmap (`docs/native/NATIVE_PORT_ROADMAP.md`)
gets a one-line status entry when the milestone moves.

---

## Baseline (verified 2026-05-27, clean HEAD)

- Build: `cmake --build rb3/native/build-native` — green.
- `RB3_BOOT=1 MILO_HEADLESS=1 RB3_DATA=<extracted> rb3-native` →
  `SystemPreInit('config/band_preinit_keep.dta')` + `SystemInit('config/band_keep.dta')`
  succeed; `gSystemConfig` = **227 object type-defs + ui config**. Clean exit.
- `RB3_RENDER_MESH=1 <milo>` renders real geometry (synthetic harness, NOT the
  game flow).
- Matched-fork clang-clean status: rndobj 64/64, synth 47/47, ui 37/37, world
  16/16, bandobj 47/60, char 56/61. (ui/world/bandobj pre-cleaned but NOT yet
  globbed onto the native link — globbing them broke a render via a `Load()`
  stream desync at `DirLoader:997`; each added class's `Load()` must be
  native-correct.)
- Engine pin: `9ad4e13`.

## The shift this milestone makes

The current harness (`RB3_BOOT`/`RB3_RENDER_MESH`) is scaffolding that proves
subsystems work in isolation. This milestone replaces it with the **real game
boot loop**: construct RB3's actual `App` subclass and let the DTA-driven boot
script drive `UIManager` to the main menu, then navigate to song select and load
a song — the way the shipped game does, the way DC3 native does.

---

## Asset constraint (verified 2026-05-27) — READ BEFORE SCOPING "song load"

`rb3/orig/SZBE69_B8/` has only `main.dol` + a zero-magic `band_r_wii.sel`
placeholder + `.map` (25 MB) — **no `.ark` data**. The 4 GB
`orig-assets/extracted/` tree (from a 360-ARK fallback, per roadmap 0.3 log)
contains song **visual milos** (293 `songs/*/gen/*.milo_xbox`) but **zero
`.mogg` audio and zero `.mid` charts** anywhere on disk.

Implication for this milestone's target:
- **In scope (achievable now):** boot the real game → DTA menu flow → song
  select → trigger the song-load sequence → the game *loads the song* (parses
  its metadata, loads the song `.milo`, sets up the gameplay/venue scene). This
  is "a song loaded."
- **Out of scope until assets exist:** actual `.mogg` audio playback and `.mid`
  gem-chart playback (Phase 3 audio + Phase 5 gameplay). That is the separate v1
  "one song end-to-end" milestone and needs the Wii `.ark` audio data (or a
  substitute song with audio) which is not currently extracted.
- Handle missing audio/MIDI **gracefully** (the real game code path, guarded —
  not a hack that skips the load). If a single song's `.mogg`/`.mid` can be
  obtained later, the same code path plays it.

## Plan (from Opus planning agent, 2026-05-27)

### DC3 model (the precedent)
DC3 boots entirely in `App`'s ctor (synchronous), then a frame loop. It registers
smart-default `Hmx::Object` stub managers (`saveload_mgr`, `profile_mgr`,
`platform_mgr`, `content_mgr`, `challenges`, `speech_mgr` — `dc3 App.cpp:75-196`
classes, registered `App.cpp:505-520`) into `ObjectDir::Main()` so DTA handlers
resolve, and keeps the UI/flow/anim/song stack REAL. Frame loop
(`App.cpp:1056-1156`, HX_NATIVE branch): `SystemPoll(false); TheUI->Poll();
TheTaskMgr.Poll(); TheSynth->Poll(); TheRnd.BeginDrawing(); TheUI->Draw();
TheRnd.EndDrawing();` with `MILO_MAX_FRAMES` exit + `sigsetjmp` draw guard.

### RB3 specifics (verified)
- **`rb3/src/App.cpp:117-307`** is RB3's real boot spine — already calls the real
  sequence (`SystemPreInit`→`TheRnd->PreInit`→`SynthInit`→`Movie::Init`→`TheRnd->Init`
  →`SystemInit`→`FixedSizeSaveable::Init`→`BandUserMgrInit`→…→`CharInit`→`BeatMatchInit`
  →`TrackInit`→`WorldInit`→`BandInit`→`TheSongMgr.Init`→`MetaPanel::Init`→`GameInit`
  →`ContextCheckerInit`→…→**`TheUI.Init()`** (`:272`)→`TheQuestMgr.Init`). **Zero
  `HX_NATIVE` blocks today.** Wii/online calls to gate `#ifndef HX_NATIVE`: VI*,
  CustomSplash, `TheNet`/`TheRockCentral`/`TheEntityUploader`, GameMic/UsbMidi,
  `TheWiiProfileMgr`, MemPushHeap, disc-error check.
- **UI manager identity (Open Q#2 RESOLVED): RB3 uses `BandUI : public UIManager,
  MsgSource`** (`band3/meta_band/BandUI.h:29`), bound at static init:
  `BandUI.cpp:44-45` `BandUI TheBandUI; UIManager &TheUI = TheBandUI;`. So
  `TheUI.Init/Poll/Draw` already dispatch through BandUI — **no `TheUI=&...`
  assignment to add.** **No `GotoFirstScreen` in RB3** — first screen is DTA:
  `TheUI.Init()`→`init_msg`→`ui/ui.dta:37-45` `{ui goto_screen $first_screen}`
  (`first_screen=intro_movie_screen`), wired at `UI.cpp:912 HANDLE(goto_screen,
  OnGotoScreen)`. The bypass must KEEP the DTA goto_screen path, not add a C++ call.
  RB3 has **no `TheFlowMgr`** — drop that poll from the loop.
- **First screen** = `intro_movie_screen` (Bink, no native decoder → auto-skip),
  then `main_hub_screen` (`ui/main/main_hub.dta`). No attract/main_screen.
- **RB3 stub set** (by DTA ref frequency): genuinely-platform → stub (`session_mgr`,
  `platform_mgr`, `content_mgr`, `net_cache_mgr`); real RB3 singletons that resolve
  by name after their `Init()` → `profile_mgr`, `saveload_mgr`, `song_mgr`,
  `training_mgr`, `closet_mgr`, `user_mgr`, `ui_event_mgr`.

### `DirLoader:997` desync rule
A globbed class with a stubbed/incorrect `Load()` desyncs the positional stream
and corrupts every later object. Bring up classes **one milo at a time** (start
`meta_panel.milo` → `main_hub.milo`), verifying each class's `Load()`/`Save()`
byte symmetry (`INIT_REVS`/`ASSERT_REVS` + field-read symmetry). DC3 precedent:
Sessions 12 (FlowAnimate), 19 (UIListLabel), 63 (SkeletonViz).

### Task graph
| # | Task | Tag | Deps |
|---|------|-----|------|
| T1 | CMake + link bring-up of the menu source set (App.cpp + ui/world/track/beatmatch/meta/movie + band3/meta_band boot subset); link stubs for off-path syms. Link clean only. | Opus | — |
| T2 | App.cpp `HX_NATIVE` init-path guards (VI/Splash/Net/RockCentral/GameMic/UsbMidi/WiiProfile/MemPushHeap/disc) + native frame loop in `RunWithoutDebugging`. | Opus | T1 |
| T3 | Install `BandRnd` as `TheRnd`; route App's PreInit/Init/Draw through it; wire `BandRenderHook`. | Sonnet | T1 |
| T4 | RB3 DTA-manager stub set + UI-init bypasses (mirror DC3 App.cpp:505-520); `mSink`-direct-set, `IsAnimating()` bypass, screen auto-advance. | Opus | T2,T3 |
| T5 | `ui/**` `Load()` correctness for `meta_panel.milo` → `main_hub.milo` (one milo at a time). | Opus | T4 |
| T6 | `band3/meta_band` menu panel/provider `Load()` correctness (MetaPanel/OvershellPanel/MusicLibrary/nav lists). | Opus | T4 (∥ T5) |
| T7 | Boot-to-menu integration + headless screenshot of `main_hub_screen` (auto-skip intro movie). | Sonnet | T5,T6 |
| T8 | Song-select navigation (keyboard→BandUI→nav; song_select panel classes + BandSongMgr content). | Opus | T7 |
| T9 | Song load (`Game::LoadSong`; venue/char/bandobj Load bring-up). | Opus | T8 |

### Key files
- `rb3/src/App.cpp` — boot spine: HX_NATIVE guards + native frame loop + stub block.
- `rb3/native/src/main_native.cpp` — add `RB3_GAME=1` mode (`App app(argc,argv); app.Run();`).
- `rb3/native/CMakeLists.txt` — expand source set + link stubs.
- `rb3/src/band3/meta_band/BandUI.cpp` — TheUI binding; net/session HX_NATIVE bypass.
- `rb3/src/system/ui/UI.cpp` — `UIManager::Init`→init_msg→goto_screen; mSink/auto-advance bypasses.

## Verified repro: song-milo load desync (2026-05-27)

`RB3_BOOT=1 <song>.milo_xbox` (boots real config, then `DirLoader::LoadObjects`)
on `songs/20thcenturyboy/gen/20thcenturyboy.milo_xbox`:

```
Can't make BandSongPref          <- band3 class, factory not registered
Can't make CharLipSync  (x4)     <- char class, not brought up/registered
FAIL: DirLoader.cpp:997  t == TempEof   <- stream desync
free(): invalid size             <- heap corruption follows the desync
```

**Root cause (confirmed, not a parser bug):** when a referenced class has no
registered factory, `NewObject` returns null, the object's bytes aren't consumed,
and the `ChunkStream` desyncs → the post-load `t == TempEof` assert fires. In the
shipped game **every** class is registered, so this never happens. The fix is the
**real per-class bring-up** (compile clang-clean + register factory + correct
`Load()`), exactly like the rndobj/synth work — NOT skipping classes. Classes the
song-load path needs first: `BandSongPref` (band3), `CharLipSync` + char deps.
This is the same `DirLoader:997` the roadmap flagged for ui/world/bandobj globbing.

## Task log

### main_hub → song_select REACHED with the real 138-song list; navigated to + selected real songs (`20thcenturyboy`/`25or6to4`, all gates pass); drove `PlaySetlist`→`move_on_quickplay`→`SyncScreen(part_difficulty_screen)` — STOP at the music-library-task/making-setlist mode + part_difficulty load (the meta→game threshold, just before `Game::LoadSong`). Root-caused & fixed the venue crash (cosmetic-proxy deferral) + the headless UI-clock freeze (transitions never advanced) + a CharBonesObject use-after-free teardown + ~12 offline-default/asset-tolerance fixes along the whole path (2026-05-27, Opus)

**Headline:** drove the entire `splash → main_hub → song_select_enter → song_select_screen` flow and into the song-load path: scripted input picks a real song from the 138-song list, `MusicLibrary::SelectNode(kNodeSong)` fires for `20thcenturyboy` (id 1011) / `25or6to4` (id 1013) — **all gates pass** (`full=0 restricted=0 allowed=1 enabled=1`) — `PlaySetlist` → `move_on_quickplay` → `NetSync::SyncScreen({music_library get_next_screen}=part_difficulty_screen)`. STOP at the meta→game transition (music-library `mTask`/`mMakingSetlist` mode setup + `part_difficulty_screen` load), the threshold just before `Game::LoadSong`. The frame loop runs clean to completion at every milestone (only a known shutdown-only `App::~App`→`BandUI::Terminate` SIGSEGV after `Run() returned cleanly`).

**Three load-bearing root-cause fixes (the milestone movers):**

1. **Headless UI clock was frozen → every screen transition stalled forever.** `os/Timer.h`'s `TIMER_GET_CYCLES` macro is PPC `mftb`; `ASM_BLOCK(...)` expands to nothing on clang so `cycle` was uninitialized → the Wii time-base never advanced → `TheTaskMgr.UISeconds()` pinned at 0 → every time-based `UITrigger` (e.g. the splash `exit-animation.trg`, a 2 s blocking anim) never completed → `UIScreen::Exiting()` stayed true → the splash→main_hub transition (and every later one) hung in `kTransitionTo` waiting on the old screen to finish Exiting(). Fix: (a) native `TIMER_GET_CYCLES` → `HxNativeMftb()` (std::chrono monotonic microseconds, kept in `os/Timer.cpp` so `<chrono>`/`<ctime>`'s `time` doesn't collide with `utl/TimeSymbol.h`'s `time` Symbol); native `Timer::Init` sets `sLowCycles2Ms=1e-3` (the Wii `OS_TIME_SPEED` calibration reads hardware reg `0x800000F8`, garbage natively). (b) Deterministic headless clock in `UIManager::Poll` (`ui/UI.cpp`): when `MILO_HEADLESS`, advance `sHeadlessFakeUISeconds += 1/30` per Poll instead of wall-clock (the native loop runs uncapped, so wall-clock advances UISeconds far slower than 30 fps; a 2 s anim would take thousands of frames). Mirrors dc3-decomp's `os/Timer.h __mftb()` + `ui/UI.cpp sHeadlessFakeUISeconds`. **This was the actual blocker the prior session mislabeled as "the sv3 inlined-proxy WorldInstance::SyncDir venue load" — that crash is real too (fix #2) but transitions wouldn't have advanced even past it.**

2. **The sv3/sv8 cosmetic-venue `WorldInstance::SyncDir` proxy-instancing crash (the prior STOP boundary) — chose deferral (option B).** Root-caused the `MILO_ASSERT(p->from->Dir(), 0x2CA)` (`world/Instance.cpp:714`): for an inlined-cached-shared proxy (e.g. `world/shared/amps/classic_blacktriple.milo`, an sv3 amp prop) the SyncDir instancing loop creates fresh `NewObject`+`CopyObject` copies whose `Dir()` is null (RB3's `Copy`/`CopyObject` never `SetName` them); on Wii these are FOUND (not created) so the assert never fires — a deep inlined-cached-shared object-resolution gap (disasm-confirmed RB3's SyncDir is byte-faithful, so the divergence is upstream in the cached-proxy object load, not Instance.cpp). Verified via instrumentation: `mDir->InlineSubDirType()==kInlineNever`, `objs-before-delete=1`, `LoadPersistentObjects count=1` while `mDir` has 4 objects, all `FindObject→null`. Per task guidance (>5-6 (A) attempts → prefer B), **deferred the cosmetic venue backdrop cleanly**: `WorldInstance::SyncDir` skips instancing (leaves a clean empty proxy, no half-instance corruption) for proxies whose `mDir.GetFile()` is under `world/vignette/` or `world/shared/` (the cosmetic shell-venue/prop tree). Scoped, not a blanket loader filter.

3. **CharBonesObject use-after-free crashed the splash-venue teardown on the main_hub transition.** Unloading the sv8 splash venue's char backdrop (`Character→RndDir::DeleteObjects→CharDriver::~`) destructs a `CharDriver` whose `mBones` ObjPtr still points at an already-freed `CharBonesObject`; `~ObjPtr` does `mPtr->Release(this)`, and because `CharBonesObject` inherits `Hmx::Object` *virtually*, computing the `Hmx::Object*` for that non-virtual call reads the vbase offset out of the freed object's vtable → SIGSEGV *before* Release runs (so a guard inside Release is too late). Fix: a native freed-address registry (`obj/Object.cpp` `HxNoteFreedAddr`/`HxAddrWasFreed`/`HxNoteReusedAddr`, bounded ring) recorded from `CharBonesObject::~CharBonesObject` keyed by the *CharBonesObject\** representation (the same value the ObjPtr stores as `mPtr`), and `ObjPtr::~ObjPtr` (`obj/ObjPtr_p.h`) checks the RAW `mPtr` bits (no vtable read) and skips Release if freed. RB3 analog of dc3-decomp's `SafeReleaseFromRing`. (This let the transition complete cleanly instead of a sigsetjmp-guarded partial-unload that corrupted shared sink lists.)

**The full song path now reaches (and where each step was unblocked):**
- splash → main_hub (timer fix #1 + venue deferral #2 + CharBonesObject fix #3) → main_hub **SETTLES** (not "in transition")
- main_hub song button: drove the real flow via two native scripting directives added to `rb3_game_input.cpp` — `select:<button>` (focuses a named UIComponent + `SendSelect` = the real Confirm/SELECT_MSG, bypassing the milo d-pad nav graph) and `msg:<object>:<action>` (sends `{action $synthUser}` to a named `ObjectDir::Main()` object = the real DTA-handler path). Sequence: `select:pn_quickplay.btn` → `select:qp_quickplay.btn` → `set_override kMainHubOverride_Waiting` → the `MainHubPanel` waiting-lock (LockStepMgr) `StartLock`→`RespondToLock`→`AdvanceAll`→`advance`→`{ui sync_screen song_select_enter_screen}`.
- → `song_select_enter_screen` → `song_select_screen` (the real song-select screen).
- song list populated by loading `songs/songs.dta` directly into `TheSongMgr` via the real `BandSongMgr::AddSongs` (the console disc/content scan isn't wired natively → list was empty / "no valid songs"); now **83 ranked songs**. Navigate `down` + `msg:music_library:select_highlighted_node` → `SelectNode(kNodeSong)` fires for real songs → `PlaySetlist`→`move_on_quickplay`→`SyncScreen(part_difficulty_screen)`.

**Supporting offline-default / asset-tolerance fixes (additive `#ifdef HX_NATIVE`; matched `#else` byte-identical; file:line):**
- `os/PlatformMgr.cpp:IsSignedIn`/`HasUserSigninChanged` — return false for a pad-less native user (`GetPadNum()==-1`) instead of `MILO_FAIL("PadNum=-1")` (the 3 unassociated BandUserMgr users; reached via `check_rewards_and_hints`→`acc_mgr has_new_awards`→`CanSaveData`→`IsUserSignedIn` on the main_hub transition_complete).
- `meta_band/MainHubPanel.cpp` + `meta_band/MusicLibrary.cpp` — gate `TheServer.AddSink`/`RemoveSink` `#ifndef HX_NATIVE` (`TheServer`=`gWiiServer` is a zeroed weak stub → AddSink walks a garbage `mSinks` list → crash; no login server offline).
- `meta_band/Matchmaker.cpp:OnMsg(ModeChangedMsg)` — null `SessionSettings` guard (same offline-no-Quazal-session case as the prior `UpdateMatchmakingSettings` guard; reached from the quickplay `{gamemode set_mode qp_coop}`).
- `net_band/RockCentral.cpp:FailAllOutstandingCalls`/`CancelOutstandingCalls` — null `mContextWrapperPool` guard (allocated only in `RockCentral::Init()`, gated off offline; reached from `NetSync::AttemptTransition` on every transition).
- `network/net/NetSession.cpp:HasUser` — provided the real impl natively (`rb3_netsession_native.cpp`; was a weak no-op stub→false), so the quickplay `NetSync::SyncScreen` leader-user gate (`u->IsLocal() && HasUser(u)`) passes — otherwise the song_select transition was silently BLOCKED.
- `meta_band/SongSelectPanel.cpp:FinishLoad` (`leaderboard.mld` Find non-failing + Poll null-guard), `meta_band/AppScoreDisplay.cpp:UpdateDisplay` (null combined-label), `meta_band/SongSortMgr.cpp:BuildSetlistList` (tolerate empty `mSetlists` — 360-ARK `internal_setlists` shortnames don't match the extract's songs.dta), `obj/DataNode.cpp:Sym`/`LiteralSym` (`MILO_FAIL_DTA`→WARN + null-Symbol, like `Int`; 360-ARK `config/song_select.dta` schema mismatch), `bandobj/InlineHelp.cpp:OnSetConfig` (read the inner per-action `loopArr->Node(1/2)` + bound-check; the matched build's `arr->Node(1)` over-reads the 1-element outer help-config array), `meta_band/MusicLibrary.cpp:Text` + `meta_band/SongSetlistProvider.cpp:Text` + `meta_band/ViewSetting.cpp` (the 360-ARK song.lst uses plain UILabels where the code expects AppLabel → skip cosmetic list-text formatting on a null cast) — all cosmetic-asset/online-UI tolerance for the 360-ARK extract.
- `rndobj/Wind.cpp:GetWind` — clamp the `sWindField` index for NaN/Inf `x` (an uninitialized CharHair sim time on native OOB-read the wind table → SIGSEGV during a venue-char Poll, blocking the song_select transition).
- `meta_band/MusicLibrary.h:ContentDir` — return a non-null sentinel natively (the base returns null; `AppendToSetlist`/`PlaySetlist` only test it for null-ness to pick the LOCAL vs net-request path — offline there is no net leader, so the song must be appended+played locally).
- `meta_band/NetSync.cpp:SyncScreen` — skip starting a nested UI lock-step when one is already in-lock (the `play_setlist`→`move_on_quickplay` sync fired before the song_select-enter sync's lock released; offline the prior lock completes on its own, so don't abort on `MILO_ASSERT(!IsBlockingTransition())`).

**Native scripting + song-load hooks (`native/src/rb3_game_input.cpp`):** added the `select:<button>` and `msg:<object>:<action>` directives (above); load `songs/songs.dta` → `TheSongMgr.AddSongs` after `TheUI.Init()` (in `RB3RegisterNativeManagerStubs`); per-frame focus trace in the input log.

**Classes brought up (K2):** none newly globbed; native `NetSession::HasUser` impl added (header-only/un-globbed network/), weak stub removed from `band3_link_stubs.s`.

**STOP boundary:** the meta→game transition — `SyncScreen(part_difficulty_screen)` is attempted but doesn't settle. Remaining: (a) the music-library `mTask`/`mMakingSetlist` mode for native quickplay (`GetMakingSetlist`=true sent the kNodeSong select to the append-only branch; the explicit `play_setlist` then reached `move_on_quickplay`), and (b) the `part_difficulty_screen` load + its transition. This is the threshold just before `Game::LoadSong` (`game/Game.cpp:235`); the song-data + selection + PlaySetlist all execute, so the next agent picks up at `part_difficulty_screen` → difficulty-select → the meta→game (`MetaPerformer`/`Game`) transition.

**Run / stop:**
```
RB3_GAME=1 MILO_HEADLESS=1 RB3_DATA=<extract> MILO_MAX_FRAMES=800 \
  RB3_GAME_INPUT="@10:start,@30:confirm,@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn,@320:down,@350:msg:music_library:select_highlighted_node,@430:msg:music_library:play_setlist" \
  rb3-native
  → intro_movie → splash → main_hub (SETTLES) → song_select_enter → song_select_screen
    (83 ranked songs) → down + select_highlighted_node: SelectNode(kNodeSong 20thcenturyboy,
    all gates pass) → play_setlist → PlaySetlist → move_on_quickplay → SyncScreen(part_difficulty
    _screen) [STOP — meta→game transition threshold, just before Game::LoadSong] → Run() clean.
```
(`MILO_MAX_FRAMES=300 RB3_GAME_INPUT="@10:start,@30:confirm,@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn"` reaches song_select_screen with the 83-song list — the stable headline. `SONG_DBG=1`/`HUB_DBG=1` env dumps were used during bring-up and removed.)

**Regressions GREEN:** `RB3_BOOT` → "SystemInit OK — (227 entries)" + "boot complete." `RB3_RENDER_MESH ui/track/gen/tracksystem_meshes.milo_xbox` → "129 meshes, 27878 tris". `rb3-dta songs/songs.dta` → "138 top-level nodes". rb3-native + rb3-dta build clean.

**Files (all additive `#ifdef HX_NATIVE`; matched `#else` byte-identical):** `src/system/os/Timer.h`+`Timer.cpp` (native mftb + calibration), `src/system/ui/UI.cpp` (headless 30fps UI clock), `src/system/world/Instance.cpp` (cosmetic-venue-proxy deferral), `src/system/obj/Object.cpp`+`obj/ObjPtr_p.h` (freed-object guard), `src/system/char/CharBones.cpp`+`CharBones.h` (record freed CharBonesObject), `src/system/os/PlatformMgr.cpp` (pad-less IsSignedIn), `src/system/rndobj/Wind.cpp` (GetWind index clamp), `src/system/obj/DataNode.cpp` (Sym/LiteralSym MILO_FAIL_DTA), `src/system/bandobj/InlineHelp.cpp` (OnSetConfig loopArr), `src/band3/game/CharDriver` n/a, `src/band3/meta_band/{MainHubPanel,MusicLibrary,Matchmaker,SongSelectPanel,AppScoreDisplay,SongSortMgr,SongSetlistProvider,ViewSetting}.cpp` + `MusicLibrary.h`, `src/band3/net_band/RockCentral.cpp`, `src/band3/meta_band/NetSync.cpp`. NEW native: `native/src/rb3_game_input.cpp` (select/msg directives + songs.dta load), `native/src/rb3_netsession_native.cpp` (NetSession::HasUser). **Stubs:** removed `_ZNK10NetSession7HasUserEPK4User` from `band3_link_stubs.s`. **CMake:** unchanged.

### Headless synthetic-input mechanism added — the FULL splash→main_hub flow now works: a scripted `start`+`confirm` drives the splash state machine, overshell add-user/local-user-join, calibration-skip, and `{ui goto_screen main_hub_screen}`; main_hub venue (sv3) begins loading; STOP at the sv3 inlined-proxy `WorldInstance::SyncDir` venue-load (deep world-subsystem boundary, NOT input/flow) (2026-05-27, Opus)

**Headline:** built the headless synthetic-input driver the milestone called for and used it to drive the entire splash→main_hub logical sequence. The splash state machine, SaveLoadManager gate, overshell add-user + local-user join, ProfileMgr, first-time-calibration skip all resolve; `{ui goto_screen main_hub_screen}` fires and the main_hub venue (sv3) starts loading. STOP at a main_hub-venue inlined-proxy world-load assert (`WorldInstance::SyncDir`), a deep world-subsystem load-correctness boundary distinct from this task's input/screen-flow scope.

**Synthetic-input mechanism (native-only, no DTA/matched-flow edits) — `native/src/rb3_game_input.cpp` (NEW TU):**
- `RB3GameInputPoll(int frame)` called once/frame from the App native frame loop (`App.cpp:490`, after `TheUI.Poll()`). Parses `RB3_GAME_INPUT="@<frame>:<action>,..."` (actions: `start`/`confirm`/`cancel`/`up`/`down`/`left`/`right`/`option`). For each scheduled frame it builds a `ButtonDownMsg(user, button, action, padNum=0)` and `TheUI.Handle(msg, false)` — *exactly* the path `Automator::Poll` uses (`UI.cpp:204-212`): UIManager → `HANDLE_MEMBER_PTR(mCurrentScreen)` → UIScreen → `HANDLE_MEMBER_PTR(FocusPanel())` → focus `UIButton::OnMsg(ButtonDownMsg)` → `SendSelect` → `UIComponentSelectMsg` (= the DTA `SELECT_MSG`/`component_select`) → the panel's `SELECT_MSG` handler. Action enum: `os/Joypad.h:11` (`kAction_Confirm=1`, `kAction_Start=4`, `kAction_Down=8`…). `SELECT_MSG`/`BUTTON_DOWN_MSG` are DTA macros (`config/macros.dta:1104,1119`).
- **Per-frame screen-flow trace** (`RB3 screen: frame N  currentScreen = '...'`) on every `TheUI.CurrentScreen()` change. **Optional `RB3_INPUT_DEBUG=1` dump** of `splash_panel`'s `splash_state` + overshell `is_any_slot_allowing_input_to_shell` (how I localized each gate).
- **Synthetic user setup (`SynthUser()`):** binds `TheBandUserMgr->GetLocalBandUsers()[0]` to pad 0 (`AssociateUserAndPad`), marks **pad 0 only** connected (`JoypadGetPadData(0)->mConnected=true` — NOT the global `fake_controllers` DataVariable, which makes the 3 *unassociated* BandUsers also report connected and then crash in `DebugGetControllerTypeOverride(GetPadNum()=-1)`), and sets a debug controller-type override (`DebugSetControllerTypeOverride(kControllerGuitar)`). Also calls `JoypadInitCommon(SystemConfig("joypad"))` — the hardware-free half of the Wii-only `JoypadInit()` (excluded `Joypad_Wii.cpp`) — to populate `gControllersCfg`/`gButtonMeanings` (else `JoypadControllerTypePadNum`/`ShellInputInterceptor::FilterAction` assert on null `gControllersCfg`).

**Drive sequence:** `RB3_GAME_INPUT="@10:start,@20:confirm"`. `@10:start` on `splash_screen` fires the splash `SELECT_MSG` → `kSplashScreen_Entered`→`ActivateSaveLoad`→(saveload idle)→`StartOvershell` (overshell attempt_to_add_user + set_active_status kOvershellInShell → user QUEUED then joined) → `WaitOvershell` (`splash_state=3`). `@20:confirm` on the overshell `kState_ChooseProfile` view selects `overshell_continue_without_profile` → `leave_options` → `kState_JoinedDefault` (the one slot state with `allows_input_to_shell TRUE`, `slot_states.dta:67`) → overshell sends `{ui overshell_allowing_input TRUE}` → splash `kSplashScreen_EndOvershell` → `{ui goto_screen main_hub_screen}`. Verified live: `splash_state` 0→3→4, `overshell_allowing` 0→1.

**Fixes landed (additive `#ifdef HX_NATIVE`; matched `#else` byte-identical; file:line):**
1. **Native DTA-manager stubs (DTA_MANAGER_STUBS §4)** — `RB3RegisterNativeManagerStubs()` (in `rb3_game_input.cpp`, called from `App.cpp:312` after `TheUI.Init()`): `saveload_mgr` (`SaveLoadManager.cpp` is `_NATIVE_FORK_EXCLUDE`'d — 2266 lines tied to MemcardMgr_Wii/WiiProfileMgr; the `NativeSaveLoadStub` `is_idle→1`/`activate→0` is the sanctioned §4 fallback) + `net_cache_mgr` (`NativeNetCacheMgrStub` `is_ready/is_done_loading→1`). Registered by name into `ObjectDir::Main()` (no-op if a real singleton claimed the name). Same call site sets `TheProfileMgr.SetHasSeenFirstTimeCalibration(true)` (the real flag the calibration screen sets on completion) so EndOvershell skips the interactive A/V-sync calibration screen and goes straight to `goto_screen main_hub_screen`.
2. **`NetSession::AddLocalUser` native host-path** (`native/src/rb3_netsession_native.cpp`) — the non-virtual method `SessionMgr::AddLocalUserImpl` calls (`mSession->AddLocalUser`); its real body is in the un-globbed `network/net/NetSession.cpp` so it was a weak no-op → the local-user join NEVER completed → SessionMgr never fired `AddLocalUserResultMsg` → the overshell slot never reached an input-allowing state → splash stuck at `WaitOvershell`. Mirror the real `IsHost()` path: push the user to `mUsers` + fire `AddUserResultMsg successMsg(1)` via `Handle`. Removed its weak stub from `band3_link_stubs.s`.
3. **`BandNetGameData` native impl** (`rb3_netsession_native.cpp`) — there is NO `BandNetGameData.cpp` in the decomp (header only), so `new BandNetGameData()` in `SessionMgr`'s ctor (`SessionMgr.cpp:44`) ran a weak no-op ctor → garbage vtable. `SessionMgr::Handle`'s trailing `HANDLE_MEMBER_PTR(mBandNetGameData)` (`SessionMgr.cpp:507`) forwarded the unhandled `AddLocalUserResultMsg` to it → SIGSEGV (vtable+0x38). Provided a minimal native ctor (constructs the `Hmx::Object` base → valid vtable + sink list) + `Handle`→unhandled + offline-no-op pure virtuals. Removed its weak ctor/Poll stubs.
4. **`BandMatchmaker::UpdateMatchmakingSettings` null-settings guard** (`meta_band/Matchmaker.cpp:264`) — `TheNetSession->GetSessionSettings()` is null offline (`mSettings==0`); the local-user-join path (`AddLocalUserResultMsg`→`BandUserMgr::SetSlot`→`UpdateMatchmakingSettings`) then derefs it. Early-return on null (matchmaking settings only exist with a Quazal session).

**STOP boundary (deep world-subsystem load, NOT input/flow):** after `{ui goto_screen main_hub_screen}`, the main_hub_screen panels `(meta sv3_panel main_hub_panel accomplishments_status_panel)` load; the **sv3 venue** (`world/vignette/shell/sv3`, the main_hub 3D backdrop — distinct from splash's sv8 which loaded without this path) loads an **inlined-proxy `WorldInstance`** whose `mDir` sub-dir has a null parent `Dir()` → `MILO_FAIL Instance.cpp:714 p->from->Dir()` (`WorldInstance::SyncDir` proxy-instancing, `0x2CA`). Root: the inlined-dir parent (`ObjectDir::mDir` field) isn't wired by the native inlined-dir load path (`ObjectDir::PostLoad`/`AddedSubDir`/`PostLoadInlined` — `Dir.cpp:152,390,565`). A native-guard that skips the assert just leaves the venue half-instanced → moves the crash to `DeleteTransientObjects` ("Could not find classic_blacktriple.mesh"), so it was reverted — the venue needs the real inlined-proxy parent-wiring fix (or a clean sv3_panel deferral). This is Phase-2-render-adjacent world-load decomp work, separate from the synthetic-input/screen-flow milestone, which is DONE up to `goto_screen main_hub_screen`.

**Classes brought up (K2):** none new globbed; native impls added for `NetSession::AddLocalUser` + `BandNetGameData` (both have no compiled decomp `.cpp` — header-only / un-globbed network/).

**Regressions GREEN:** `RB3_BOOT` → "SystemInit OK — … (227 entries)" + "boot complete." `RB3_RENDER_MESH ui/track/gen/tracksystem_meshes.milo_xbox` → "129 meshes, 27878 tris" + PNG. `rb3-dta songs/songs.dta` → "138 top-level nodes … 10 song(s)." rb3-native + rb3-dta build clean.

**Files:** NEW `native/src/rb3_game_input.cpp` (input driver + screen trace + manager stubs + calibration-seen); `native/src/rb3_netsession_native.cpp` (NetSession::AddLocalUser + BandNetGameData); `src/band3/meta_band/Matchmaker.cpp` (UpdateMatchmakingSettings null-settings guard, HX_NATIVE); `src/App.cpp` (frame-loop `RB3GameInputPoll(frame)` call + `RB3RegisterNativeManagerStubs()` after `TheUI.Init()`, HX_NATIVE). **CMake:** added `rb3_game_input.cpp` to rb3-native. **Stubs (`band3_link_stubs.s`):** removed `_ZN10NetSession12AddLocalUserEP9LocalUser` + `BandNetGameData` ctor/Poll (now strong).

**Run / stop:**
```
RB3_GAME=1 MILO_HEADLESS=1 RB3_DATA=<extract> MILO_MAX_FRAMES=140 \
  RB3_GAME_INPUT="@10:start,@20:confirm" rb3-native
  → intro_movie_screen → splash_screen → @10:start: splash_state 0→3 (WaitOvershell,
    overshell user joined) → @20:confirm: overshell ChooseProfile→JoinedDefault,
    overshell_allowing 0→1, splash_state→4 (EndOvershell) → {ui goto_screen main_hub_screen}
    → main_hub panels + sv3 venue load → FAIL Instance.cpp:714 (sv3 inlined-proxy
    WorldInstance::SyncDir — main_hub venue world-load boundary)
```
(Add `RB3_INPUT_DEBUG=1` for the per-frame `splash_state`/`overshell_allowing` dump.)

**Next:** the main_hub **sv3 venue** inlined-proxy world-load — wire the inlined sub-dir's parent `Dir()` in the native `ObjectDir::PostLoad`/`PostLoadInlined` path so `WorldInstance::SyncDir` instances cleanly (or defer the `sv3_panel` venue backdrop on native, since it's only the 3D scene, not the main_hub UI/logic). Once main_hub settles, drive `main_hub.dta`'s song button (`qp_quickplay.btn`/`career_songs.btn` → `{ui goto_screen song_select_enter_screen}`) with another scripted `confirm`/nav, then a song confirm → the meta→game transition → `Game::LoadSong` (Game.cpp:235). The synthetic-input driver is ready for that nav.

### `world/shared/chars.milo` (rev-15 band-preview) FULLY LOADS — the desync was NOT in Character/RndDir PostLoad: it was `CharacterTest::Load` (weak-stubbed → consumed 0 bytes) + cached cube-texture mip bytes never consumed; +2 downstream LP64 fixes (BandCharacter bones-loop OOB, MetaPanel fade-fader). Boot now reaches the **frame loop on `splash_screen`** (venue + all band chars load+SyncObjects clean). STOP at splash→main_hub state machine (needs "press start" + overshell/saveload drive) + a venue-char **Draw()** GFX crash (Phase-2 render blocker, caught by the draw guard). (2026-05-27, Opus)

**Headline (root cause + the 4 fixes):** the rev-15 `world/shared/chars.milo` desync that the prior agent localized to "`Character::PostLoad`/`RndDir::PostLoad`, BandCharDesc a victim" was actually **two distinct stream-consumption gaps inside the BandCharacter objects**, not a Character/RndDir field-read bug (Character/RndDir/ObjectDir PostLoad are byte-correct — confirmed by `Tell()` bracketing every level). Both were "engine reads fewer bytes than the on-disk cached stream contains":

1. **`CharacterTest::Load` was a weak no-op stub → consumed 0 bytes** (the primary fix). `Character::PostLoad`, for a proxy char at gRev>0xF, calls `mTest->Load(bs)` (CharacterTest, the in-dir test/driver block: `main.drv` CharDriver + clips + walkpath + flags). `CharacterTest.cpp` was in `_NATIVE_FORK_EXCLUDE`, so `mTest->Load` resolved to the `band3_link_stubs.s` weak no-op and read **0** bytes — but the milo serialized ~53 bytes there (the `Character::PostLoad` Tell was identical before/after `mTest->Load`). `BandCharDesc::Load` then started 53 bytes early and read a garbage Symbol → the `String chars 4128769 > 512` SIGABRT. Diagnosed by tracing Tell through BandCharacter/Character/RndDir/ObjectDir PostLoad + a forward raw-byte dump at the BandCharDesc start (showed `main.drv`/`none`/`male` — i.e. the un-consumed CharacterTest+Character-tail bytes). **Fix: brought up `CharacterTest.cpp`** (un-excluded; one native edit — the bare `sync` Symbol in `HANDLE_ACTION(sync, Sync())` collides with POSIX `sync()` under clang overload resolution, gated to `HANDLE_ACTION(Symbol("sync"), …)` `#ifdef HX_NATIVE`). Added a `ClipDistMap::Draw` weak stub (referenced only by `CharacterTest::Draw`, off the load path). The 10 CharacterTest weak stubs stay (displaced by the strong defs; links clean).

2. **Cached cube-texture mip bytes never consumed** (`rndobj/Bitmap.cpp`/`Bitmap.h`). Past fix #1, the load hit `MILO_FAIL Bitmap.cpp:439 !paletteBytes` in `RndCubeTex::PostLoad` → `mBitmap[i].Load(bs)` (cube faces load from the **shared** milo stream, unlike RndTex which loads from a standalone file buffer). `RndBitmap::LoadHeader` **zeroes `numMips`** (matched: the Wii GX path regenerates mips at runtime), so `RndBitmap::Load`'s mip-read loop never runs — but the cached `.milo_xbox` bitmap serializes a full mip chain after the base level (traced `rev=1 rawMips=3` for `eyes.cube` face 0; face 1 then read into face-0 mip bytes → garbage `bpp=0` header → palette assert). Fix: `LoadHeader` stashes the real cached mip count in a new trailing native-only `mNativeCachedMips`; `RndBitmap::Load` reads+**discards** those mip bytes (scratch buffer, NOT linked into `mMip` so `NumMips()` stays 0 and the existing `MILO_ASSERT(!mNumMips)` in RndTex still holds). Gated on `bs.Cached()` so the RndTex BufStream path (Cached()==false) is untouched — RB3_RENDER_MESH unaffected.

3. **`BandCharacter::SyncObjects` bones-loop OOB read** (`bandobj/BandCharacter.cpp:621`). After the whole milo loaded, `DirLoader::Cleanup`→`SyncObjects` walked `static const char *bones[8] = {…8 names…}` with `for (ptr=bones; *ptr != 0; ptr++)` — no null sentinel. The Wii image happens to have a 0 in the datum after the array; under clang LP64 it reads `bones[8]` (OOB) as a garbage non-null pointer → `Find<RndTransformable>(garbage)` SIGSEGV (faulted at `0xbf800000`). Fix: bound the walk to `ptr != bones + 8` `#ifdef HX_NATIVE`.

4. **`MetaPanel::FinishLoad` fade-fader hard-fail** (`meta_band/MetaPanel.cpp:369`). `TheSynth->Find<Fader>("fade", true)` MILO_FAILs because `sfx/common_bank.milo` synth banks aren't in the 360-ARK extract (same gap the existing metamusic-absent path handles). Native passes `false`; `MetaMusic::AddFader` already null-tolerates (MILO_WARN). Splash `meta` panel finishes load.

**Boot now reaches** (`RB3_GAME=1 MILO_HEADLESS=1 MILO_MAX_FRAMES=N`): App ctor → … → intro_movie→`movie_done`→splash_screen → splash venue (sv8) + scenery + **all band characters + `world/shared/chars.milo` (player0..player3 + crowd) load AND SyncObjects clean** → **App native frame loop runs all N frames, `Run() returned; exiting cleanly`**, sitting on `currentScreen=splash_screen`. A `caught crash in Draw()` fires from the frame the venue chars first render (`Character::DrawLodOrShadow`→`RndMesh::SetUpdateApproxLight`, Mesh.h:347 — the GFX/render Phase-2 blocker; the sigsetjmp draw guard skips the frame, loop continues). A teardown SIGSEGV fires after the clean frame-loop exit (shutdown-only).

**STOP boundary (two distinct, both past the load):** (a) **splash→main_hub state machine** — `ui/splash/splash.dta` requires a SELECT/Start button (`SELECT_MSG`/`BUTTON_DOWN_MSG kAction_Start→Confirm`) to advance `kSplashScreen_Entered`→`kSplashScreen_ActivateSaveLoad`, then drives `saveload_mgr activate`/`overshell attempt_to_add_user`/`profile_mgr set_primary_profile`→`kSplashScreen_WaitOvershell`→(`overshell_allowing_input`)→`kSplashScreen_EndOvershell`→`{ui goto_screen main_hub_screen}`. Headless has no input → sits on `kSplashScreen_Entered` forever. Reaching main_hub needs a native auto-advance (inject the splash confirm) + the overshell/saveload/profile stub managers to fire their completion callbacks (T7 menu-flow). (b) The venue-char **Draw()** GFX crash (Phase-2 render). **No main_hub PNG yet** — blocked on (a)+(b).

**T8 song-select recon (for the next agent):** from `main_hub.dta`, the song path is the `qp_quickplay.btn`/`qp_setlist.btn`/`career_songs.btn` → `{ui goto_screen song_select_enter_screen}` → `song_select_screen`. Assets present: `ui/song_select/{song_select.dta,song_select_extras.dta}`, `ui/song_select/gen/{song_select,song_select_details,song_select_filter,song_select_shortcut}.milo_xbox`, `ui/resource/list/gen/list_song_select_{browser,setlist,setlist_scores}.milo_xbox`. Key class: **`song_select_panel`** (+ BandSongMgr content). `songs/songs.dta` parses 138 nodes / 10 songs (rb3-dta).

**Class brought up (K2): `CharacterTest`** (`char/CharacterTest.cpp`, un-excluded `native/CMakeLists.txt`). New stub: `ClipDistMap::Draw` (`band3_link_stubs.s`).

**Regressions GREEN:** `RB3_BOOT` → "SystemInit OK — … (227 entries)" + "boot complete." `RB3_RENDER_MESH ui/track/gen/tracksystem_meshes.milo_xbox` → "129 meshes, 27878 tris" + PNG. `rb3-dta <abs>/songs/songs.dta` → "138 top-level nodes … 10 song(s)." rb3-native + rb3-dta build clean.

**Files (all additive `#ifdef HX_NATIVE`; matched `#else` byte-identical):** `src/system/char/CharacterTest.cpp` (sync Symbol), `src/system/rndobj/Bitmap.cpp`+`Bitmap.h` (cached cube-mip discard + `mNativeCachedMips`), `src/system/bandobj/BandCharacter.cpp` (SyncObjects bones-loop bound), `src/band3/meta_band/MetaPanel.cpp` (fade-fader tolerant). **CMake:** un-excluded `CharacterTest`. **Stubs:** added `ClipDistMap::Draw` (off-load-path gameplay). No `Character.cpp`/`RndDir`/`ObjectDir`/`CubeTex.cpp` source changes (they were confirmed byte-correct — the fix was the un-consumed CharacterTest + cube-mip bytes).

**Run / stop:**
```
RB3_GAME=1 MILO_HEADLESS=1 RB3_DATA=<extract> MILO_MAX_FRAMES=4 rb3-native
  → … → splash venue + ALL band chars + world/shared/chars.milo load+SyncObjects CLEAN →
    frame loop runs all frames, "Run() returned; exiting cleanly", currentScreen=splash_screen
    (Draw() of venue chars crashes → caught by draw guard, skipped — Phase-2 GFX blocker)
```

**Next:** (1) auto-advance the splash past `kSplashScreen_Entered` on native (inject the SELECT/confirm; mirror DC3 menu drive) + make the overshell/saveload/profile stubs report idle/ready so the state machine reaches `{ui goto_screen main_hub_screen}`; (2) the venue-char `Character::DrawLodOrShadow`→`RndMesh::SetUpdateApproxLight` Draw crash (Phase-2 render). Then emit the main_hub PNG via the RB3_RENDER_MESH WritePNG/ReadbackHeadlessFrame path from the App frame loop (add `RB3_GAME_SCREENSHOT=`).

### T9 venue char-Load FIXED — root cause was the Xbox *cached* (16-byte-padded) CharBonesSamples sample layout, not endianness; venue band characters (visemes/extras/clips/crowd/CharHair) now load clean; brought up CharHair; STOP at a different deep boundary: `Character::PostLoad`/`RndDir::PostLoad` desync in the rev-15 `world/shared/chars.milo` (2026-05-27, Opus)

**Headline (root cause + fix):** the CharClip/CharBonesSamples desync was **NOT** an endianness
problem — RB3's `BinStream::ReadEndian` already byte-swaps BE `.milo_xbox` data on the LE host
(its existing `#ifdef HX_NATIVE` swaps when `!mLittleEndian`), so per-element `bs >> *p` values
came out correct. The desync was the **on-disk sample LAYOUT**: the extracted assets are *cached*
Xbox 360 milos (`ChunkStream::Eof` sets `mIsCached=true`+`kPlatformXBox` for `.milo_xbox`), and the
Xbox/PS3 cached `Save` path stores `CharBonesSamples` sample data in a **16-byte-padded layout**
(mirrors `dc3-decomp CharBonesSamples::Save`'s `cached` branch): every uncompressed-POS/SCALE
`Vector3` is followed by a zero pad float (→ 16 bytes on disk), and each sample block is rounded
out to a 16-byte boundary. RB3's matched-fork `LoadData` reads the **unpadded Wii layout** → it
under-consumed by the padding (e.g. mOne off[END]=608 but the cached on-disk sample = 752),
desyncing the stream so the viseme CharClip's `mZeros` vector then read a garbage length and the
next clip's `CharBones::Bone` Symbol blew up (`String chars 23173 > 512` at `CharBones.cpp:1355`).

Diagnosed by instrumenting `CharBonesSamples::Load`/`LoadData` + `CharClip::Load` with `Tell()`
deltas (LoadData consumed exactly the *unpadded* size while the next read desynced → the file is
padded), then dumping the raw decompressed `.milo_xbox` chunk stream in Python to confirm the
per-`Vector3` pad floats and the per-sample alignment.

**Fixes (all additive `#ifdef HX_NATIVE`; matched `#else` byte-identical; file:line):**

1. **`CharBonesSamples::LoadData` cached path** (`char/CharBonesSamples.cpp:551`) — when
   `bs.Cached() && (plat==XBox||PS3) && gVer>0xE`, read the same per-element POS/SCALE/QUAT/ROT
   sections (still via `bs >>`, which auto-swaps) but **consume the cached padding**: a pad float
   after each uncompressed POS/SCALE `Vector3`, then round the *actual consumed bytes* up to 16.
   **Key correction vs DC3's Save formula:** the per-sample alignment delta must be computed from
   the **padded running total** (`((consumed+0xF)&~0xF)-consumed` measured at runtime), NOT DC3
   Save's unpadded `mOffsets[END]`-based delta — the latter over-pads clips whose padded data is
   already 16-aligned (e.g. a 6-Vec3 + 2-ShortQuat viseme = 96+16=112, already aligned, delta must
   be 0, not the unpadded-based 8). The matched non-cached per-element loop below is untouched.
2. **`CharClip::StartBeat()`/`EndBeat()` empty-guard** (`char/CharClip.h:188`) — some viseme/pose
   clips load with an empty `mBeatTrack` (rev>0x11 reads count 0). Retail STLport `vector::front()/
   back()` read OOB benignly (immediately compared equal in the load-time `EndBeat()==StartBeat()`
   check); clang's hardened libstdc++ asserts. Guard to 0.
3. **`CharClip::BeatToSample` empty-guard** (`char/CharClip.cpp:456`) — same empty-`mBeatTrack`
   `back()` assert, but at animation-play time; treat last-key frame as 0 (the no-divide branch).

**Class brought up (K2): `CharHair`** (`char/CharHair.cpp`) — un-excluded from `_NATIVE_FORK_EXCLUDE`
(`native/CMakeLists.txt`). Three native edits: (a) gated `#include "stl/_function_base.h"` →
`<functional>` on native (only uses `std::list::sort(ByRadius())`); (b) gated the unconditional
MWCC paired-singles `StrandMultiply` asm body `#ifndef HX_NATIVE`, providing a native
`StrandMultiply(a,b,out){ Multiply(a,b,out); }` using the standard `math/Mtx.h` matrix multiply
(same substitution `CharForeTwist` uses; the guarded local `Multiply` block stays
`#ifdef CHARHAIR_LOCAL_MULTIPLY`, undefined here); (c) added the missing `return bs;` to
`operator>>(BinStream&, CharHair::Point&)` and `operator>>(BinStream&, CharHair::Strand&)` —
matched-fork omits them (MWCC tolerates the fall-through; clang LP64 emits `ud2`/SIGILL, same class
as the `CharBones::SetStart`/`ObjDirPtr::operator=` missing-return fixes). The weak CharHair
ctor/`Hookup`/`_ZTI8CharHair` stubs in `band3_link_stubs.s` are left in place (now displaced by the
strong defs; build links clean — no stub edits needed).

**Result (verified):** the splash venue's band-character backdrop now loads clean — the viseme
clips (`Base`/`brow_*`/…), `char/extras/{male,female}_extras0*.milo`, `char/extras/clips/{male,
female}/*.milo`, `char/crowd/*`, `world/shared/extras/*` and the CharHair-bearing characters all
load through `CharClip::Load`/`CharBonesSamples::Load`/`CharBones::Load`/`CharHair::Load` with no
desync (111 char-milo loads, was crashing on the *first* viseme clip).

**STOP boundary (a DIFFERENT deep boundary — NOT char-clip/bones):** `world/shared/chars.milo` (the
rev-**15** band-member preview cache, pulled in by the venue's `vignette_chars`/`chars` subdir
chain — and distinct from the `CharCache::InitMe` copy which is still `#ifndef HX_NATIVE`-deferred)
desyncs in **`Character::PostLoad` → `RndDir::PostLoad`** (`char/Character.cpp:742`): the desync
surfaces downstream as `BandCharacter::PostLoad`→`BandCharDesc::Load`→`Outfit`/`OutfitPiece`
reading a garbage Symbol (`String chars 4128769 > 512`). Confirmed by Python dump of `chars.milo`:
the bytes `BandCharDesc::Load` reads as `mGender`/`mSkinColor`/`mHead` are already shifted (the real
"male"/"none" Symbols appear *inside* the mHead window), i.e. `BandCharDesc::Load` is a desync
*victim* — the misalignment originates upstream in the `Character`/`RndDir` directory PostLoad
chain (a different class family from the char-clip/bones/hair byte-symmetry this task targeted, and
the same `Character::PostLoad` shape DC3 leaves matched, with only runtime-draw `HX_NATIVE` guards).
No main_hub PNG yet — blocked by this `Character::PostLoad` desync in the venue char milo.

**Run / stop:**
```
RB3_GAME=1 MILO_HEADLESS=1 RB3_DATA=<extract> MILO_MAX_FRAMES=5 rb3-native
  → … frame loop → splash venue (sv8) + scenery + ALL band-character backdrops load clean
    (visemes/extras/clips/crowd/CharHair — the T9 char-Load desync is FIXED) →
    world/shared/chars.milo: Character::PostLoad/RndDir::PostLoad desync (rev-15 band-preview milo)
    → BandCharDesc::Outfit garbage-Symbol (String chars 4128769 > 512) — a different deep boundary
```

**Regressions GREEN:** `RB3_BOOT` → "SystemInit OK — … (227 entries)" + "boot complete." `RB3_RENDER_MESH
ui/track/gen/tracksystem_meshes.milo_xbox` → "129 meshes, 27878 tris". `rb3-dta songs/songs.dta` → "138
top-level nodes … 10 song(s)." rb3-native + rb3-dta both link + build clean.

**Files (all additive HX_NATIVE; matched `#else` byte-identical):** `src/system/char/CharBonesSamples.cpp`
(LoadData cached-padded path), `src/system/char/CharClip.cpp` (BeatToSample empty-guard),
`src/system/char/CharClip.h` (StartBeat/EndBeat empty-guard), `src/system/char/CharHair.cpp`
(StrandMultiply native multiply + Point/Strand `operator>>` missing-return + `<functional>` seam).
**CMake:** un-excluded `CharHair` (`native/CMakeLists.txt`). **No stub edits** (weak CharHair stubs
stay; strong defs win).

**Next:** root-cause the `world/shared/chars.milo` `Character::PostLoad`/`RndDir::PostLoad` desync (a
proxy/subdir-ref or ObjVector read in the directory PostLoad of this rev-15 milo) — OR defer/gate
`vignette_chars`/`chars.milo` if it's the band-PREVIEW (not venue-render) path, the way
`CharCache::InitMe` already defers the cache copy — to advance the venue load → splash completes →
main_hub renders → emit the PNG via the RB3_RENDER_MESH WritePNG/ReadbackHeadlessFrame path.

### Ported DC3's native (synchronous) loader to RB3 — the splash `meta_panel.milo` front-loader stall is FIXED; boot drives the splash venue load + brought up CharForeTwist; STOP at the T9 venue char-Load (CharClip/CharBonesSamples) desync (2026-05-27, Opus)

**Headline (root cause + fix):** the stall was the **Wii async time-sliced loader** running
on a fast host. RB3's `DirLoader::PollLoading`/`FileLoader::PollLoading` advance only
`while (!CheckSplit() && GetFirstLoading()==this && !IsLoaded())`. `LoadMgr::Poll` sets the
budget `unk1c = mPeriod` (10ms) and `CheckSplit()` (`mTimer.Split(); CyclesToMs > unk1c`) goes
true once 10ms of a Poll pass elapses. On the host the budget is exhausted by **earlier loaders
in the same Poll pass**, so when the splash `meta` MetaPanel's *second* `ui/meta_panel.milo`
DirLoader (kLoadBack, created by `UIPanel::Load`) finally reached the front, `CheckSplit()` was
**already true at entry → the while-body never ran → it sat in `OpenFile` forever**. Diagnosed
by tracing `PollLoading`: `state=OpenFile split=1 first=1 loaded=0` repeating every frame.
**DC3 ported the loader to synchronous native loading** — DC3's `DirLoader::PollLoading` and
`FileLoader::PollLoading` are just `{ (this->*mState)(); }` (one state step, NO CheckSplit gate),
and DC3's native `PollFrontLoader` is just `mLoading.front()->PollLoading()`. RB3 was missing all
of this (RB3's `DirLoader.cpp` had **0** `HX_NATIVE` blocks; DC3's has 12).

**DC3 loader blocks ported (additive `#ifdef HX_NATIVE`, matched `#else` byte-identical; file:line):**
1. **`DirLoader::PollLoading`** (`obj/DirLoader.cpp:243`) — native: advance one state step
   (`if (!IsLoaded() && GetFirstLoading()==this) (this->*mState)();`), mirroring DC3's
   `DirLoader::PollLoading` form. Removes the CheckSplit entry-gate that froze a freshly-front
   loader. The internal per-state `GetFirstLoading()!=this`/`CheckSplit()` re-entrancy guards stay
   intact (cooperate with sub-loaders pushed to the front during LoadResources).
2. **`FileLoader::PollLoading`** (`utl/Loader.cpp:384`) — same native one-step form (mirrors DC3's
   `FileLoader::PollLoading() { (this->*mState)(); }`).
3. **`LoadMgr::Poll`** (`utl/Loader.cpp:190`) — native: drain `mLoading` to completion in one
   synchronous pass with the split budget disabled (`unk1c = 1e30f`), so the per-state
   `CheckSplit()` guards never bail mid-load. Without this, the one-step PollLoading still made
   only ~1 object/frame (LoadObjs' internal CheckSplit bailed each pass) → panel milos took dozens
   of frames or never finished within MILO_MAX_FRAMES. DC3's `PollUntilLoaded` already sets the
   period to 1e30; this extends that to the frame-loop `Poll` on native.
4. **STUB-vtable guard in `DirLoader::CreateObjects`** (`obj/DirLoader.cpp:531`, mirrors DC3
   DirLoader.cpp:929-943) — a class whose factory was registered (its `::Init()` runs in a compiled
   TU) but whose `.cpp` is still `_NATIVE_FORK_EXCLUDE`'d has a weak-no-op ctor → `NewObject` returns
   an object with a zeroed/garbage vtable → deref crash. Detect `!vptr || !vptr[0]` and null the
   object so `LoadObjs` ReadDead-skips its bytes (rev>1), exactly like an unregistered class. General
   native safety net for any not-yet-brought-up class.

**Result of the loader fix (verified):** the splash `meta` MetaPanel's `ui/meta_panel.milo` second
DirLoader now drives `OpenFile→LoadHeader→CreateObjects→LoadDir→LoadObjs→DoneLoading` synchronously
and the `meta` panel reaches `kDown` (the `[SCR.blk] panel='meta' state=0` block that previously
repeated forever now clears after ~1 frame). The splash→main_hub transition begins loading the
splash venue backdrop (`sv8_panel` → `world/vignette/shell/sv8_a.milo` + scenery: cityscape,
camera, clouds, wind all load clean).

**Class brought up (K2):** **`CharForeTwist`** (`char/CharForeTwist.cpp`) — un-excluded from
`_NATIVE_FORK_EXCLUDE` (`native/CMakeLists.txt:297`); the file `#define CHARHAIR_LOCAL_MULTIPLY`
(which suppresses Mtx.h's matrix-multiply inline so its own MWCC paired-single `Multiply` is used)
is now gated `#ifndef HX_NATIVE`, so on clang it uses the standard out-of-line `Multiply(Matrix3,
Matrix3,Matrix3)` (declared in Mtx.h, defined in Rot.cpp:577). Removed its weak ctor + `_ZTI13Char
ForeTwist` typeinfo stubs from `band3_link_stubs.s` (now strongly defined). This advanced the venue
char load past the stub-vtable factory crash to the real T9 desync below.

**STOP boundary (documented "deep multi-step desync needing decomp work" — T9):** the splash venue
backdrop animates band characters (extras + crowd + visemes), pulled in by `world/vignette/shell/
sv8_a.milo`'s resource/subdir loads (`world/shared/extras/*`, `char/extras/*`, `char/crowd/*`,
`char/main/shared/viseme_*`, `world/shared/chars*`). Loading those drives **`CharClip::Load`/
`CharBonesSamples::Load`/`CharClipGroup::Load`, which version-desync under clang LP64**:
`CharBonesSamples::Load` reads a garbage `gVer` → `MILO_ASSERT(gVer>12 && gVer<=16, 0x2A0)`
(`CharBonesSamples.cpp:457`); `CharClip::Load` leaves `mBeatTrack` empty → `EndBeat()` derefs
`std::vector::back()` on empty (`CharClip.h:190`, CharClip.cpp:898); `operator>>(CharBones::Bone)`
reads a garbage-length String (`CharBones.cpp:1355`) → heap corruption → abort in the MILO_FAIL
string dtor. This is the genuine multi-step char-Load byte-symmetry work (T9), the same family the
prior `CharCache::InitMe`/`BandHeadShaper`/`BandCharDesc` deferrals flagged. **Tried a path-based
deferral** (`IsDeferredCharLoad` in DirLoader::OpenFile/LoadSubDir/LoadResources skipping the venue
char milos) but it's whack-a-mole across the many load paths AND breaks the parent venue WorldDir's
`PostLoad` (which reads back subdir/proxy refs from its own stream and desyncs when a subdir is
nulled — crash at `world/Dir.cpp:227`→`obj/Dir.cpp:458`), and over-broad filters regressed legit
loads (`char/shared/viseme_resource.milo` loads fine and is needed). **Reverted the deferral** — the
venue char system needs real CharClip/CharBonesSamples Load decomp work, not a load-skip. The
loader fix (the task's core deliverable) is the keeper.

**Run / stop:**
```
RB3_GAME=1 MILO_HEADLESS=1 RB3_DATA=<extract> MILO_MAX_FRAMES=5 rb3-native
  → App ctor → … → TheUI.Init() → BandUI::Init (panel preload) → frame loop →
    goto_screen intro_movie_screen → movie_done → goto_screen splash_screen →
    splash transition: meta_panel.milo loads (STALL FIXED), splash venue sv8_a +
    scenery loads → CharClip/CharBonesSamples Load desync on the venue char backdrop
    (CharBonesSamples.cpp:457 / CharBones.cpp:1355) — T9 char-Load decomp boundary
```
(No main_hub PNG yet — blocked pre-main_hub by the T9 venue char-Load desync, the documented STOP.)

**Regressions GREEN:** `RB3_BOOT` → "SystemInit OK — … (227 entries)" + "boot complete." `RB3_RENDER_MESH
ui/track/gen/tracksystem_meshes.milo_xbox` → "129 meshes, 27878 tris". `rb3-dta songs/songs.dta` → "138
top-level nodes … 10 song(s)." All build clean. (The loader change affects ALL milo loads → these guards
matter; all unaffected.)

**Files (all additive HX_NATIVE; matched `#else` byte-identical):** `src/system/utl/Loader.cpp`
(LoadMgr::Poll + FileLoader::PollLoading), `src/system/obj/DirLoader.cpp` (DirLoader::PollLoading +
CreateObjects stub-vtable guard), `src/system/char/CharForeTwist.cpp` (CHARHAIR_LOCAL_MULTIPLY gate).
**CMake:** un-excluded `CharForeTwist` (`native/CMakeLists.txt`). **Stubs (`band3_link_stubs.s`):**
removed `_ZN13CharForeTwistC1Ev` ctor + `_ZTI13CharForeTwist` typeinfo (now strong).

**Next:** the T9 venue char-Load. Bring `CharClip`/`CharBonesSamples`/`CharClipGroup` `Load` byte-correct
under clang LP64 (the `gVer` read at CharBonesSamples.cpp:456 desyncs → upstream object in the CharClip
mis-consumes bytes — trace the first desyncing read in CharClip::Load's `mFull.Load`/`mOne.Load` path).
That unblocks the splash/main-hub venue backdrop, then the splash→main_hub transition completes →
emit the main_hub PNG via the RB3_RENDER_MESH WritePNG/ReadbackHeadlessFrame path from the App frame loop.

### Boot reaches the App frame loop + auto-advances intro_movie→splash; 13 fixes past the GemTrackDir crash (gameplay-HUD bring-up, MI-RTTI, online-gating, Movie graceful-done, NetSync offline-transition); STOP at the splash `meta_panel.milo` loader stall (2026-05-27, Opus)

**Headline:** drove the init.dta `{new …Panel}` preload chain all the way past the
`GemTrackDir::NewObject` crash, through `BandUI::Init`/`InitPanels`, into the **App
native frame loop** (renders + exits cleanly across 200 frames), and through the
**`intro_movie_screen` → `movie_done` → `splash_screen` auto-advance** (Movie graceful-done).
Current STOP: the `splash_screen` transition stalls because the `meta` MetaPanel's
`ui/meta_panel.milo` DirLoader sits permanently at the front of `LoadMgr::mLoading`
without progressing (a frontload-shared-milo re-load / loader-ownership issue — a deep
multi-step loader investigation, the documented stop boundary). main_hub not yet reached.

**Fixes (all additive `#ifdef HX_NATIVE`, matched `#else` byte-identical; file:line):**

1. **GemTrackDir clang-bring-up (K2)** — un-excluded `GemTrackDir.cpp` (gameplay track HUD,
   loaded by `coop_track_panel`/`track/trackpanel.milo` preloaded via `game.dta`). Gated
   `rndwii/Mesh.h` include + the 2 `dynamic_cast<WiiMesh*>(...)->mDisplays.Clear()` Wii-GX
   derefs in `PrepareChordMesh` `#ifndef HX_NATIVE` (`bandobj/GemTrackDir.cpp:7,1191,1196`).
   Removed its 38 weak method stubs + ctor + `_ZTI11GemTrackDir` from `band3_link_stubs.s`
   (now strongly defined). Added FUNC stubs for ChordShapeGenerator's `BuildChordMesh`/
   `MakeInvertedMesh` (runtime gameplay, off-menu-path).
2. **ChordShapeGenerator clang-bring-up (K2)** — un-excluded `ChordShapeGenerator.cpp`
   (its factory was registered but ctor weak-stubbed → garbage object → SetName crash in
   `chord_shape_generator.milo` load). Fixed the `vector<RndMesh::Face, unsigned short>`
   2-arg STLport size-hint idiom → plain `vector<RndMesh::Face>` on native
   (`bandobj/ChordShapeGenerator.cpp:631`). Removed its ctor/BuildChordMesh/MakeInvertedMesh
   stubs + `_ZTI19ChordShapeGenerator` (now strong).
3. **OvershellProfileProvider native impl** (`meta_band/OvershellSlot.cpp` end, HX_NATIVE) —
   this Wii-profile UIListProvider has a header but **NO decomp .cpp** anywhere; OvershellSlot
   `new`s one and `set_provider`s it → `dynamic_cast<UIListProvider*>` on its garbage vtable
   crashed. Provided minimal native ctor/dtor/virtuals + Wii-profile method no-ops (mirrors
   MetaPanel's JoinInvitePanel/WiiProfilePanel glue). Removed its 8 weak stubs.
4. **`setupProviders[2]` int-0 → object-null** (`OvershellSlot.cpp:81`) — the bare `0`
   bound `DataNode(int)` (kDataInt) on clang LP64 (MWCC picked the `Hmx::Object*` overload);
   `{invite_friends.lst set_provider $invite_provider}` then called `DataNode::GetObj`→
   `LiteralStr` on an int node → MILO_FAIL. Cast to `(Hmx::Object*)0` → kDataObject null →
   `SetProvider(0)` = empty list.
5. **`UIList::Refresh` null-provider guard** (`ui/UIList.cpp:386`) — a list given a null
   provider (the invite list above) reached `Refresh(true)`→`mListState.Provider()->IsActive`
   on null. Guard `b && mListState.Provider()`.
6. **OvershellSlot `TheServer` AddSink/RemoveSink gated** (`OvershellSlot.cpp:98,103`) —
   zeroed network DATA stub; gated `#ifndef HX_NATIVE` (mirrors OvershellPanel ctor).
7. **`BandMatchmaker::IsFinding` offline** (`meta_band/Matchmaker.cpp:177`) — `TheNetSession->
   mSettings->mPublic` derefs null `mSettings`; return `mSearching` only on native.
8. **`SaveLoadStatusPanel::FinishLoad` gated** (`meta_band/SaveLoadStatusPanel.cpp:16`) —
   `TheSaveLoadMgr` (excluded TU → null DATA stub) `->AddSink`; gated `#ifndef HX_NATIVE`.
9. **`Synchronizable` compiled** — added `network/net/Synchronize.cpp` to the rb3-native
   source set (online side-effects `TheSyncStore`/`SynchronizeIfDirty` gated `#ifndef HX_NATIVE`).
   It had a `.cpp` but wasn't globbed; its zeroed `_ZTI14Synchronizable` stub crashed
   `__dynamic_cast<UIPanel*>` through OvershellPanel's MI hierarchy
   (`UIPanel,Synchronizable,MsgSource`). Removed its ctor/dtor/SetSyncDirty + typeinfo stubs.
10. **Real NetSession RTTI** (`rb3_netsession_native.cpp`) — defined NetSession's key fn
    (`Handle`) + non-pure virtuals so clang emits the real `_ZTI10NetSession`/vtable (the
    zeroed stub crashed `BandUI::Init`'s `ObjDirItr<UIScreen>` dynamic_cast over the registered
    `session` object). Also gave the native Handle a session-control DTA dispatch (`{session
    clear}`/disconnect/end_game offline no-ops + is_local/num_users queries; msg sym = `Sym(1)`)
    so those don't fall through to `Hmx::Object`'s property handlers and `Array(2)` out-of-range
    MILO_FAIL. Removed the 6 base-virtual stubs + `_ZTI10NetSession` data stub.
11. **`MILO_FAIL_DTA` macro** (`os/Debug.h`, mirrors DC3) — WARN on native / FAIL on console
    for DTA data-type mismatches. Used in `DataNode::Int`/`LiteralInt` (`obj/DataNode.cpp:86,106`)
    with HX_NATIVE coerce-to-0 recovery: a PropAnim keyframe animating UILabel `alignment`
    passed a non-int node → `_val.Int()` aborted in the frame-loop AnimTask poll.
12. **`CharSync::UpdateCharCache` deferred** (`meta_band/CharSync.cpp:48`) — runs on every
    screen-transition-complete; `TheBandDirector->IsMusicVideo()` derefs null (no venue at
    menu) and `CopyCharDesc` derefs garbage NPC BandCharDescs (deferred char milos). Early-return
    on native (mirrors CharCache::InitMe/BandCharDesc/BandHeadShaper char deferrals).
13. **`UIStats::MaybePublish` null-server guard** (`meta_band/UIStats.cpp:62`) — `TheNet.mServer`
    null on native; treat as not-connected (offline has nothing to publish).
14. **`Movie::Ready()`→true / `Movie::Poll()`→false** (`movie/Movie.cpp:206,231`, DTA_MANAGER_STUBS
    §5.3) — no native Bink decoder + `intro_movie.milo` absent; the intro_movie_screen now loads
    then instantly fires `movie_done` → `goto_screen splash_screen`.
15. **`NetSync::IsTransitionAllowed`→true offline** (`meta_band/NetSync.cpp:209`) — offline
    single-player has no leader user / synchronized net-UI, so the console condition returned
    false and `BandUI::GotoScreen`'s `IsTransitionAllowed` gate blocked EVERY screen transition
    (the boot `goto_screen` never started). Allow once UIEventMgr has no blocking dialog.
16. **`MetaPanel::Load` metamusic tolerant** (`meta_band/MetaPanel.cpp:313`) — the 360-ARK
    `config/synth.dta` lacks the `metamusic_loop` array the RB3-Wii code expects (different
    schema) → hard-failing 3-arg `SystemConfig` aborted. Non-failing FindArray; construct mMusic
    but skip the file Load when absent. `IsLoaded()` gated on `mMusic->mFilename.empty()` so the
    splash_screen `meta` panel can finish loading without menu music (no audio assets anyway).

**Boot now reaches** (`RB3_GAME=1 MILO_HEADLESS=1 RB3_DATA=<extract> MILO_MAX_FRAMES=N`):
App ctor → … → BandUI::Init (panel preload chain: GemTrackDir/ChordShapeGenerator/trackpanel/
overshell milos all load + construct; InitPanels dynamic_casts OK) → **App native frame loop
(renders, 200 frames clean, "Run() returned; exiting cleanly")** → `goto_screen
intro_movie_screen` → screen loads (Movie graceful) → `movie_done` → `goto_screen splash_screen`
→ **splash transition STALLS**: the `meta` MetaPanel's `ui/meta_panel.milo` DirLoader stays at
the front of `LoadMgr::mLoading` permanently without advancing (the milo opens + reads its
header fine on the first poll, but the front loader never progresses/pops afterward — a
frontload-shared-milo loader-ownership issue, deep multi-step). `splash_screen` `CheckIsLoaded()`
is blocked by the `meta` panel → never transitions in → main_hub not reached. **No PNG yet**
(stuck pre-main_hub). A teardown SIGSEGV in `BandUI::Terminate` fires AFTER the clean frame-loop
exit (shutdown-only, not a boot blocker).

**STOP reason:** the `meta_panel.milo` loader stall is the documented "deep multi-step loader
investigation" boundary (two DirLoader instances for the same frontload-shared milo; the front
one's `PollLoading` while-body stops executing after the first poll despite `split=0 first=1
loaded=0`). 16 fixes landed. Needs loader-sharing decomp work next.

**Regressions GREEN:** `RB3_BOOT` → "SystemInit OK — … (227 entries)" + "boot complete." (exit 0).
`RB3_RENDER_MESH ui/track/gen/tracksystem_meshes.milo_xbox` → "129 meshes, 27878 tris" + PNG.
Both build clean.

**Files (all additive HX_NATIVE):** `src/system/bandobj/GemTrackDir.cpp`, `ChordShapeGenerator.cpp`,
`src/band3/meta_band/{OvershellSlot,Matchmaker,SaveLoadStatusPanel,CharSync,UIStats,NetSync,
MetaPanel,BandUI}.cpp`, `src/network/net/Synchronize.cpp`, `src/system/ui/UIList.cpp`,
`src/system/obj/DataNode.cpp`, `src/system/os/Debug.h` (MILO_FAIL_DTA), `src/system/movie/Movie.cpp`.
**Glue:** `native/src/rb3_netsession_native.cpp` (NetSession RTTI + session Handle).
**CMake:** un-excluded GemTrackDir+ChordShapeGenerator; added `network/net/Synchronize.cpp`.
**Stubs (`band3_link_stubs.s`):** removed GemTrackDir(×40)/ChordShapeGenerator(×4)/
OvershellProfileProvider(×8)/Synchronizable(×4)/NetSession-base(×7) now-strong syms; added
ChordShapeGenerator BuildChordMesh/MakeInvertedMesh/typeinfo (off-menu-path gameplay).

**Next:** root-cause the `meta_panel.milo` front-loader stall (frontload-shared dir re-load /
DirLoader ownership in `LoadMgr::PollFrontLoader`/`DirLoader::PollLoading`) so `splash_screen`
finishes loading + transitions in → main_hub_screen. Then emit the main_hub PNG via the
RB3_RENDER_MESH WritePNG/ReadbackHeadlessFrame path from the App frame loop. Fix the
`BandUI::Terminate` teardown SIGSEGV (shutdown-only).

### Root-cause: the orphaned Hmx::Object::PreLoad — every leaf object's Load() was silently skipped; +3 LP64 font fixes → real menu text renders, boot loads 11+ panel milos (2026-05-27, Opus)

**Headline root cause (the big one):** `Hmx::Object::PreLoad(BinStream&)` — the base
PreLoad that dispatches `{ Load(bs); }` — has its **only matched-fork definition at the
bottom of `src/BudgetScreen.cpp:611`** (a UIScreen test harness TU NOT on the native link
line). So on native it resolved to the **weak no-op stub** in `dta_link_stubs.s:304`. Effect:
`DirLoader::LoadObjs` calls `obj->PreLoad → Hmx::Object::PreLoad → Load`, but for **every leaf
object that doesn't override PreLoad** (RndText, RndFont, RndMat, RndTex, CharClip, …) PreLoad
did nothing → **`Load()` was never called**. The Text objects in font milos were created
(`NewObject("Text")` OK, no "Can't make") but never loaded, so `RndText::mFont` stayed null →
`UILabel::Font()` `MILO_ASSERT(t->mFont,0x44A)` (= `UILabel.cpp:1096` decimal = assert 0x448→0x44A).
Diagnosed by tracing: the genned font's only back-refs were from `UILabelDir` (never from a
`Text`), and `RndText::Load` fired **0 times** the whole boot. **Fix:** strong
`void Hmx::Object::PreLoad(BinStream &bs){ Load(bs); }` added to `obj/Object.cpp` (after
`Hmx::Object::Load`, `#ifdef HX_NATIVE`) — Object.cpp is the natural home + is compiled; strong
def displaces the weak stub. This single fix unblocked the **entire object-Load machinery** and
advanced the boot from the first font label all the way into rendering real menu text.

**Then 3 LP64 fixes in the now-exercised font/text Load+render path:**
1. **`CharBones::SetStart` missing return → SIGILL** (`char/CharBones.h:107`): declared `char*`
   return, no return statement; MWCC tolerated, clang LP64 emits `ud2`. Added `return ptr;`
   (HX_NATIVE; value unused by callers). Same class as the `ObjDirPtr::operator=` fix.
2. **`RndText::mMeshMap` pointer-truncation** (`rndobj/Text.h:191`, ~10 sites in Text.cpp):
   the mesh map is keyed by `(unsigned int)fontptr` — pointer-width on console, but TRUNCATES a
   64-bit host pointer; casting back `(RndFont*)key` yields a bogus pointer → null-deref in
   `WrapText`'s `font->mCellSize` loop (`Text.cpp:950`). Added `RndText::FontKey` typedef
   (HX_NATIVE = `unsigned long`, else `unsigned int`); changed the map + all key casts to it.
   Layout-safe — mMeshMap is a runtime-only field (built by SetFont/UpdateText, never read
   byte-for-byte from disk).
3. **`KerningTable` `memset(mTable,0,0x80)` half-clears the table on LP64** (`rndobj/Font.h`,
   3 sites: ctor, SetKerning, Load): `mTable` is `Entry*[32]` = 128 B on console (0x80) but
   **256 B on LP64**; the hardcoded `0x80` left buckets `mTable[16..31]` uninitialized → garbage
   `Entry*` → `RndFont::Kerning` (`Font.cpp:140`) crashed walking `entry->next`. Changed to
   `memset(mTable, 0, sizeof(mTable))` (HX_NATIVE; matched `#else` keeps `0x80`).

**Plus 2 off-menu-path char-load defers** (the loads now actually execute Load() post-PreLoad-fix
and hit the deferred T9 char-Load desync; null-tolerated, mirror CharCache::InitMe precedent):
- **`BandCharDesc::Init`** (`bandobj/BandCharDesc.cpp:90`): deferred the `deform_path` char-deform
  milo load `#ifndef HX_NATIVE` (`gDeforms=0`; `GetDeformClip` already null-returns). The deform
  milo's `CharBonesSamples::Load` version-desyncs (`CharBonesSamples.cpp:457` `gVer>12 && gVer<=16`).
- **`BandHeadShaper::Init`** (`bandobj/BandHeadShaper.cpp`): deferred the head_male/head_female
  char milo loads (`_tmp0=_tmp1=false` on native); their `CharClip::Load` desyncs at the
  `CharBones::Bone` vector read (`CharBones.cpp:1354` `String chars 23173>512`). Existing HX_NATIVE
  null-guards in the fn (GetNum/FindSubdir/gVisemes) already handle the null heads.

**Boot now reaches** `App ctor → … → BandInit → MetaPanel::Init → … → TheUI.Init()
(= BandUI::Init, App.cpp:311) → UIManager::Init (UI.cpp:472) → init_msg DTA chain → `{new …Panel}`
→ UIPanel::OnLoad → DirLoader loads panel + font milos**: fonts (pentatonic / pentatonic_display /
default) load + **real menu text renders through the full RndText/RndFont path** (e.g. wraps
"ROCK BAND FAILED"), `review_display` panel loads + preloads labels (clean), NextSongPanel/
TrackPanel/MainHub-family panels instantiate.

**STOP point (excluded-class bring-up, NOT a font/Load-correctness gap):** SIGSEGV at
`GemTrackDir::NewObject` (`bandobj/GemTrackDir.h:121` `NEW_OBJ`) while a `TrackPanel`
(`bandtrack`, in-game gameplay track HUD) milo loads in the init.dta chain — `GemTrackDir.cpp`
(1405 lines) is in `_NATIVE_FORK_EXCLUDE` (`native/CMakeLists.txt:299`) so its ctor is weak-stubbed
→ NewObject returns a half-constructed object. This is a per-class clang bring-up (K2), a large
gameplay-track class off the main-menu path — next step is either bring GemTrackDir up clang-clean
+ register its real factory, or (if the init DTA loads TrackPanel only as a preload) gate it.

**Run / stop:**
```
RB3_GAME=1 MILO_HEADLESS=1 RB3_DATA=<extract> MILO_MAX_FRAMES=3 rb3-native
  → App ctor → SystemPreInit → … → BandInit (BandCharDesc/BandHeadShaper char-deform loads
    deferred) → MetaPanel::Init → GameInit → TheUI.Init() → UIManager::Init → init_msg →
    init.dta `{new …Panel}` chain: font milos load (pentatonic/default), RndText/RndFont render
    real text, review_display panel loads → SIGSEGV GemTrackDir::NewObject (excluded TU, K2)
```

**Regressions GREEN:** `RB3_BOOT` → "SystemInit OK — … (227 entries)" + "boot complete." (exit 0).
`rb3-dta <abs>/songs/songs.dta` → "138 top-level nodes … Showed 10 song(s)." Both targets build clean.

**Files (all additive HX_NATIVE; matched `#else` byte-identical):** `src/system/obj/Object.cpp`
(PreLoad), `src/system/char/CharBones.h` (SetStart return), `src/system/rndobj/Text.h` +
`Text.cpp` (FontKey/mMeshMap), `src/system/rndobj/Font.h` (KerningTable memset ×3),
`src/system/bandobj/BandCharDesc.cpp` + `BandHeadShaper.cpp` (defer char loads),
`src/system/ui/UIFontImporter.cpp` (FindTextForFont accepts RndText classname alias — kept; correct
but the real blocker was PreLoad). **No stubs added/removed; no CMake changes.**

**Next:** bring `GemTrackDir` clang-clean (K2) + register factory, OR gate the TrackPanel preload,
to advance the init.dta panel chain toward `main_hub_screen`. Then the deferred char-Load
desyncs (CharClip/CharBonesSamples version byte-symmetry, T9) when in-game chars are needed.

### Boot reaches UI-panel loading in BandUI::Init (2026-05-27, orchestrator)
Drove the boot from `Can't make Tex/Text` into actual UI-panel loading:
1. **rndobj legacy class-name aliases (Tex/Text/Dir → RndTex/RndText/RndDir).**
   RB3's 2010 milos serialize these 3 under bare names; the decomp named the
   classes with the `Rnd` prefix. Only these 3 diverge (Mat/Mesh/Group/Cam/Trans/
   MultiMesh/Font already use bare OBJ_CLASSNAMEs). Added shared
   `RB3RegisterLegacyRndAliases()` in `native/src/rb3_band_rnd.cpp` (Tex+Text+Dir;
   render harness `PreInitRender` had only Tex+Dir) + call it from `RunGame`
   (main_native.cpp) on the real game path. Fixes `Can't make Tex/Text`.
2. **`OBJ_SET_TYPE` config lookup made tolerant (HX_NATIVE, `obj/ObjMacros.h`).**
   `SystemConfig("objects", StaticClassName(), "types")` HARD-FAILED for classes
   absent from the incomplete 360-extract objects config (`Couldn't find 'RndTex'
   in array`). Native variant decomposes into non-failing `FindArray`s → missing
   type-def ⇒ `SetTypeDef(0)` (matches the macro's existing graceful path).
   High-leverage; RB3_BOOT stays green (227).

**Boot now reaches** `BandUI::Init`→`UIManager::Init`(UI.cpp:472)→DTA `init_msg`
`{new …Panel}`→`UIPanel::OnLoad`→`DirLoader` loads panel milos→`UILabel::PostLoad`
→`Update`→`UILabel::Font()` (UILabel.cpp:614) **`MILO_ASSERT(t->mFont,0x44A)`** —
the label's Text obj loads but its `mFont` (RndFont*) is null. No `Can't make Font`
in log; `Rnd::PreInit`(Rnd.cpp:329) does `RndFont::Init()` so Font should register
→ the null `mFont` is a font-RESOURCE-load subtlety (how the font milo's RndFont
attaches to the label dir's `TextObj::mFont`). **Next T5 step.**

### T5/T6 menu panel classes registered + cache-path fixed — boot loads the menu milos, runs the DTA init chain, stops at rndobj Tex/Text class-name registration (2026-05-27, Opus)

**Status: BOTH TASK GOALS REACHED.** (A) All menu panel factories register — **no more
`Unknown class`** (the original `ChooseColorPanel` stop is gone; the init.dta chain now
instantiates `ChooseColorPanel`/`MetaPanel`/`OvershellPanel`/`GamePanel`/`PassiveMessagesPanel`/
`JoinInvitePanel`/… via their registered factories). (B) **`ui/meta_panel.milo` LOADS** from
`ui/gen/meta_panel.milo_xbox` (cache-path fix), as do `ui/global/meta_loading_icon.milo`,
`ui/global/dialog_common.milo`, `ui/resource/fonts/pentatonic.milo`. Boot advances through the
whole `*Init` cluster → CharInit/BandInit/MetaPanel::Init/ProfileMgr::Init/SessionMgr::Init →
**`TheUI.Init()` → `UIManager::Init` → init_msg → `ui/init.dta` screen-include chain executing**
(many `{new …Panel}` DTA directives run cleanly) → font-resource milo loads.

**STOP point (documented Load-correctness boundary, T5 rndobj):** `MILO_FAIL UILabel.cpp:1096
Error: t` inside `ui/resource/fonts/pentatonic.milo` (a milo that DID open + load). Root cause is
the rndobj **class-name discrepancy**: RB3 milos serialize text/texture objects as classes
**`Tex`** and **`Text`**, but the engine registers `RndTex`/`RndText` (`OBJ_CLASSNAME(RndTex)`
→ Symbol "RndTex"). So `NewObject("Tex")`/`NewObject("Text")` return null → `Can't make Tex`/
`Can't make Text` (graceful per-object, no stream desync) → the font milo's `RndText` text objects
are absent → `UILabel::Font` (`UILabel.cpp:613`, assert 0x448) fails on the null text object
when `loading.lbl` updates during PostLoad. This is rndobj per-class registration/Load-correctness
(T5), NOT menu-panel registration — needs a `Tex`/`Text` (and likely `Mat`/`Mesh`/`Group`/`Font`)
class-name alias so RB3-format milos resolve their rndobj classes. Out of this task's scope
(menu PANEL classes + cache path), cleanly captured here.

Run / stop:
```
RB3_GAME=1 MILO_HEADLESS=1 RB3_DATA=<extract> MILO_MAX_FRAMES=3 rb3-native
  → … App ctor → SystemPreInit (cache-mode ON) → … → CharInit → BandInit →
    MetaPanel::Init → GameInit → AssetMgr::Init → CharCache::Init → TheUI.Init()
  → UIManager::Init → init_msg → ui/init.dta screen-include chain:
    ui/meta_panel.milo LOADS (Can't make Tex — graceful), {new ChooseColorPanel/MetaPanel/
    OvershellPanel/GamePanel/PassiveMessagesPanel/JoinInvitePanel} all construct,
    AccomplishmentManager/ProfileMgr/SessionMgr configs parse → loads
    meta_loading_icon.milo, dialog_common.milo, pentatonic.milo (font) →
    FAIL UILabel.cpp:1096 "t"  (rndobj "Tex"/"Text" class not registered — T5)
```

**(A) Menu panel TUs brought up clang-clean (un-excluded from `_NATIVE_FORK_EXCLUDE`), per-TU
HX_NATIVE fix (K2 patterns; matched `#else`/asm path byte-identical):**
- **MetaPanel** (the hub — registers `ChooseColorPanel` + 61 other panel factories at
  `MetaPanel.cpp:195`+). Clean after the StoreMenuPanel.h fix below. Also added minimal native
  ctor glue for the Wii-online panels the init DTA instantiates but that have **no decomp impl**:
  `JoinInvitePanel`/`WiiProfilePanel` ctors → `UIPanel()`, `WiiFriendsScreen::Init`/
  `WiiFriendsProvider::Init`/`Poll`/`WiiInvitationsProvider::Init` no-ops, `TheWiiFriendsProvider`/
  `TheWiiInvitationsProvider` globals (`MetaPanel.cpp` HX_NATIVE block) — so `{new JoinInvitePanel}`
  yields a valid empty UIPanel instead of a garbage-vtable crash.
- **MusicLibrary** — ~9 switch jump-over-init → braced case bodies (`Text`/`Mat`/`FakeWinNode`/
  `AllSetlistSongsHaveScoreType`); `HANDLE_ACTION(on_exit,…)` → `Symbol("on_exit")` (POSIX `on_exit`).
- **AccomplishmentManager** / **AccomplishmentDiscSongConditional** / **AccomplishmentPanel** /
  **CampaignGoalsLeaderboardChoicePanel** / **TourDescPanel** — gated the STLport-internal
  `namespace stlpmtx_std { _Temporary_buffer<Symbol*>/__rotate<Symbol*> }` (and AccomplishmentPanel's
  `_STLP_BEGIN_NAMESPACE` `_Temporary_buffer` class) `#ifndef HX_NATIVE` (host std::sort uses host alloc).
- **AssetMgr** — gated `stlpmtx_std::__less<Symbol>` specialization `#ifndef HX_NATIVE`.
- **Stats** (`band3/game`) — gated `stlpmtx_std` sort-algorithm specializations (`__adjust_heap`/
  `__unguarded_partition`/`__insertion_sort` for `PartPercentageSorter`). Needed: `Stats` ctor
  constructs `PerformanceData::mStats`'s `std::vector<float>` members (was weak-stubbed → garbage
  vector ptrs → `resize(3)` crash in `BandProfile`/`ProfileMgr::Init`).
- **SongSort** (defines `NodeSort`) — `mTree.insert(found.first,…)` (raw `ShortcutNode**` from
  `equal_range`) → `insert(mTree.begin()+(found.first-mTree.data()),…)` (host vector::insert needs iter).
- **MetaPerformer** / **QuestFilterPanel** — `== random` Symbol (gated out of Symbols4.h on native)
  → `== Symbol("random")` per-site (POSIX `random()` collision).
- **MainHubMessageProvider** / **InputMgr** / **SongSortByDiff** / **GameConfig** / **Band** —
  switch jump-over-init → braced case bodies.
- **PrefabMgr** — `MSL_Common/extras.h` → `<cstring>` (HX_NATIVE).
- **FileMerger** — `HANDLE(select,…)` → `Symbol("select")` (POSIX `select()`).
- **OutfitConfig** — gated `rndwii/Tex.h` + `<revolution/gx/GXMisc.h>` includes & the
  `WiiTex::bComposingOutfitTexture`/`GXPixModeSync()` calls `#ifndef HX_NATIVE`; gated the
  `stlpmtx_std` `OldColorOption` vector helper specializations; `SYNC_PROP(index,…)` →
  `Symbol("index")` (POSIX `index()`).
- **BandDirector** — `char *str = strstr(...)` → `const char *` (host strstr returns const).
- **TourProgress** / **StoreMenuPanel.h** — `#include "stl/pointers/_vector.h"` (STLport pointer-
  vector internals) → `#include <vector>` on native (engine STL seam).
- **AccomplishmentGroup::Configure** — `(instrument_icon 3)` reads a bare-digit glyph as `kDataInt`;
  `FindData(...,String&)`→`DataNode::Str` MILO_FAILs under native MILO_DEBUG (retail is non-debug
  + coerces). HX_NATIVE: read the value node and stringify int-or-string.

**Other boot-advancement fixes (additive HX_NATIVE, asm path intact):**
- **`obj/Dir.h:152` `ObjDirPtr::operator=(const ObjDirPtr&)`** missing a `return *this;` — MWCC
  tolerated the fall-through (chained `operator=` leaves `*this` in the return reg); clang LP64
  traps (`ud2`→SIGILL) in `ObjectDir::PostLoad`. Added the explicit `return *this;` on native.
  (Unblocked CharInit's now-cache-resolved char-resource milo load.)
- **`bandobj/BandHeadShaper`** — the female-head branch reuses `gHeadMale` for its load (matched-fork
  transcription), leaving `gHeadFemale` null → `GetNum`/`FindSubdir`/`gVisemes[i]->SetName` null-deref.
  Null-guarded `GetNum`, `FindSubdir`, the `gVisemes[i]` SetName loop.
- **`meta_band/Matchmaker`** (`BandMatchmaker` ctor/dtor) — gated `TheNet.GetSearcher()->Add/RemoveSink`
  (null online searcher) `#ifndef HX_NATIVE`.
- **`meta_band/ProfileMgr::Init`** — gated `TheServer`/`TheGameMicManager`/`TheSaveLoadMgr` AddSinks
  (off-link/excluded; unconstructed MsgSource base) `#ifndef HX_NATIVE`; kept `TheNetSession`/
  `TheRockCentral`/`ThePlatformMgr` (real). Mirrors BandUI::Init.
- **`meta_band/PassiveMessenger`** ctor/dtor — gated `TheVoiceChatMgr` (network/ off-link) AddSink/RemoveSink.
- **`meta_band/OvershellPanel`** ctor — gated `TheServer.AddSink` (network/ off-link).
- **`meta_band/CharCache::InitMe`** — deferred the `world/shared/chars.milo` band-character preview
  cache load `#ifndef HX_NATIVE` (it opens but `BandCharDesc`/`BandCharacter` Load desyncs — `String
  chars 4128769 > 512`, deferred T9 char-Load correctness). Null-guarded `GetCharacter`. Not on the
  menu path; re-enable once char Load is native-correct.

**(B) Cache-path fix (file:line):** `os/System.cpp` native `SystemPreInit` (HX_NATIVE branch, after
`DataInit()`): added `DirLoader::SetCacheMode(true)`. `ObjectDir::PreInit` (`Dir.cpp:741`) only sets
cache-mode when `UsingCD()` — true on the Wii disc boot, where logical `foo/bar.milo` paths must be
rewritten to the extracted `foo/gen/bar.milo_<plat>` form. Native sets `SetUsingCD(false)` (loose
extracted files), so cache-mode stayed off and logical milo loads (`ui/meta_panel.milo`, char milos,
…) never resolved to their `gen/*.milo_xbox`. Forcing it on mirrors the Wii UsingCD() branch.
`TheLoadMgr.GetPlatform()` is `kPlatformXBox` on this path (set in `main_native.cpp:529` before the
App ctor) → suffix `.milo_xbox`. Confirmed: `ui/meta_panel.milo` now loads from `ui/gen/meta_panel.milo_xbox`.

**Stubs (`native/src/band3_link_stubs.s`):** ADDED weak FUNC stubs for symbols newly referenced once
the menu TUs were un-excluded but whose owning TUs stay DEFERRED (BandPatchMesh ×11, VocalPlayer/
GemTrackDir/ChordShapeGenerator ctors, TourPerformerLocal ×5) or are Wii-online-only (MemcardMgr::Init,
WiiFriend::GetProfile, WiiFriendsProvider GetPossessiveSuffix/IsPossessiveSuffixNeeded,
WiiFriendsDetailsProvider ctor) + DATA stub `TheMemcardMgr`. REMOVED stubs now strongly defined by
brought-up/glued TUs: `AccomplishmentDiscSongConditional` ctor, `AssetMgr::EquipAssets`,
`JoinInvitePanel`/`WiiProfilePanel` ctors, `WiiFriendsScreen::Init`, `WiiFriendsProvider::Init`/`Poll`,
`WiiInvitationsProvider::Init`, `TheWiiFriendsProvider`/`TheWiiInvitationsProvider` globals.

**Regressions GREEN:** `RB3_BOOT` → "SystemInit OK — … (227 entries)" + "boot complete." (exit 0).
`rb3-dta songs/songs.dta` → parses songs (10 shown). Both targets build clean.

**Next (T5 rndobj class-name reconciliation):** register `Tex`/`Text` (and audit `Mat`/`Mesh`/`Group`/
`Font`/etc.) class-name aliases so RB3-format milos resolve their rndobj object classes — then the
font/UILabel chain completes and the init.dta screen flow can reach `main_hub_screen`. Then resume
per-class `Load()` correctness for the menu milos. The char-cache `BandCharDesc` Load desync (deferred
here) is the T9 char-Load item.

### T4 manager-globals brought up — boot reaches TheUI.Init() + DTA init script (2026-05-27, Opus)

**Status: GOAL REACHED.** `RB3_GAME=1` now drives the real boot through the entire
`*Init` cluster and into **`TheUI.Init()` (= `BandUI::Init`, App.cpp:311) running**;
`UIManager::Init()` fires the DTA `init_msg` (`UI.cpp:472`) which loads `ui/init.dta`
→ `#include choose_color.dta` → instantiates a `ChooseColorPanel`. Boot stops there
at the **T5/T6 class-registration boundary**: `MILO_FAIL "Unknown class
ChooseColorPanel"` (`obj/Object.cpp:36`) because `ChooseColorPanel`'s factory is
registered by `REGISTER_OBJ_FACTORY(ChooseColorPanel)` at **`MetaPanel.cpp:195`**,
and `MetaPanel.cpp` is in `_NATIVE_FORK_EXCLUDE` (clang-bring-up gap). This is the
documented STOP point — the menu-milo/DTA class-registration boundary, not a
manager-global issue. **No more Wii/net-manager-global crashes on the boot path.**

Run / stop:
```
RB3_GAME=1 MILO_HEADLESS=1 RB3_DATA=<extract> MILO_MAX_FRAMES=3 rb3-native
  → … App ctor → SystemPreInit → TheRnd PreInit/Init → SynthInit → Movie::Init
  → SystemInit (227 cfg) → BandUserMgrInit → … → CharInit → BeatMatchInit →
    TrackInit → WorldInit → BandInit → TheSongMgr.Init → MetaPanel::Init →
    GameInit → ContextCheckerInit → PatchDir::Init → **TheUI.Init() (BandUI::Init)**
  → UIManager::Init → init_msg → ui/init.dta → choose_color.dta →
    FAIL "Unknown class ChooseColorPanel"  (MetaPanel.cpp:195 factory excluded — T5/T6)
```

**Manager globals brought up (each: how + file:line):**

1. **`ThePlatformMgr` (`platform_mgr`) — native-glue ctor.** Ctor lived in excluded
   `os/PlatformMgr_Wii.cpp:125` → weak-stubbed → zeroed → `MsgSource` base garbage
   → `BandUserMgr` ctor `ThePlatformMgr.AddSink(this, signin_changed)`
   (`game/BandUserMgr.cpp:58`) faulted. **Fix:** strong native
   `PlatformMgr::PlatformMgr()`/`~PlatformMgr()`/`RegionInit()` in NEW
   **`native/src/rb3_platform_native.cpp`** — minimal offline-default field inits;
   the compiler now emits the `MsgSource`/`ContentMgr::Callback` base ctors so
   `mSinks` is a valid empty list. `mHomeMenuWii` = a calloc'd zeroed `HomeMenu`
   block (compiled TUs read `mHomeMenuWii->mHomeMenuActive` → false safely);
   `mDiscErrorMgr` left null (its boot deref is already `#ifndef HX_NATIVE`-gated).
   `RegionInit()` → `SetRegion(kRegionNA)` (clears the "region not initialized"
   notify). Strong defs win over the weak ctor/dtor stubs in `dta_link_stubs.s`
   (left in place for rb3-dta). SetName done in native `SystemInit`
   (`os/System.cpp`, see below).

2. **`TheContentMgr` (`content_mgr`) — construct the real base global.** `ContentMgr
   *TheContentMgr` + its `= &TheWiiContentMgr` initializer live in excluded
   `os/ContentMgr_Wii.cpp:70-74` → null → `BandSongMgr::Init`
   (`meta_band/BandSongMgr.cpp:63`) `TheContentMgr->RegisterCallback` faulted on
   null. **Fix:** `ContentMgr *TheContentMgr = new ContentMgr();` (the base
   `os/ContentMgr.cpp` class, COMPILED, fully offline-safe — `StartRefresh` no-op,
   `RefreshDone`/`NeverRefreshed` from `mState`, `IsMounted`/`MountContent`→true)
   in `rb3_platform_native.cpp`. `ContentMgr::Init()` (sets `mState=kDone` +
   `SetName("content_mgr", Main())`) is called from native `SystemInit`.

3. **`TheNetSession` (`session`) — minimal concrete native subclass.** The concrete
   impl + `NetSession *TheNetSession` live in the un-globbed `network/` Quazal
   subsystem → null → pervasive (`~28` compiled TUs) `TheNetSession->AddSink(this)`
   faulted (first at `LockStepMgr` ctor `meta_band/LockStepMgr.cpp:35`, reached via
   `NetSync::NetSync` ← `BandUI::Init`). NetSession is abstract (10 pure virtuals).
   **Fix:** NEW **`native/src/rb3_netsession_native.cpp`** — a STRONG native
   `NetSession::NetSession()`/`~NetSession()` (so the `MsgSource` base + `mJobMgr`
   construct; idle/offline scalar init), strong offline query methods
   (`IsLocal`→idle&&!online=true, `IsInGame`/`IsBusy`→false, `IsHost`→true,
   `IsOnlineEnabled`/`IsJoining`/`IsStartingGame`→false), and a concrete
   `RB3NativeNetSession : NetSession` overriding ALL virtuals as offline no-ops.
   Constructed by `RB3InitNativeNetSession()` from native `SystemInit`. Defining the
   base ctor forces emission of the abstract `NetSession` vtable → added weak stubs
   for its non-overridden base virtuals (`Handle`/`Poll`/`Add*`/`Remove*ToSession`
   + the `Handle` v-thunk) and a `_ZTI10NetSession` typeinfo data-stub in
   `band3_link_stubs.s` (nothing dynamic_casts to NetSession on boot). A weak
   `RB3InitNativeNetSession` no-op was added to `dta_link_stubs.s` so rb3-dta (which
   compiles System.cpp but doesn't link the glue) still links.

**Manager-global wiring added to native `SystemInit`** (`os/System.cpp`, additive
`#ifdef HX_NATIVE` slot — the real game's SystemInit slot for these): after the
existing curated init, `ThePlatformMgr.SetName("platform_mgr", sMainDir)` +
`TheContentMgr->Init()` + `RB3InitNativeNetSession()`. (The previous comment there
said these were "no-op link stubs — UB to call"; now they're real objects.)

**Other boot blockers fixed en route (NOT manager globals):**

- **`#pragma pack(1)` leak corrupting `Synth` layout (ODR bug — root-caused).**
  After PlatformMgr, boot crashed in `Synth::SetUnk40` (App.cpp:269) — App.cpp's TU
  placed `Synth::unk40` at offset **141** while the engine's `Synth.cpp` placed it
  at **152**, an 11-byte layout divergence. Root cause: `meta/StoreOffer.h` and
  `meta/StorePackedMetadata.h` bracket a `#pragma pack(1)` with the **MWCC
  `#pragma push`/`#pragma pop`**, which clang treats as no-ops for the *pack* stack
  → the `pack(1)` **leaked** for the rest of any TU that transitively includes
  StoreOffer.h before `synth/Synth.h` (App.cpp does; Synth.cpp doesn't), mis-packing
  every later class. **Fix (additive `#ifdef HX_NATIVE`):** use the standard
  `#pragma pack(push,1)`/`#pragma pack(pop)` on clang in both headers; MWCC path
  (`#else`) byte-identical. (Confirmed: `offsetof(Synth,unk40)` now 152 in App.cpp's
  context.) This unblocked a large stretch of boot (synth common-bank → SaveLoad →
  Char/BeatMatch/Track/World/Band init).

- **`PatchDir::Init` missing-asset assert** (`bandobj/PatchDir.cpp:132`,
  `MILO_ASSERT(sResource, 0xBA)`): the art-maker `patch_layer` milo
  (`../patchcreator/og/patch_warpmesh.milo`) is original-gen (Wii); the 360-ARK
  extract only has `patchcreator/ng/gen/*.milo_xbox`. Gated null-tolerant under
  `#ifdef HX_NATIVE` (mirrors RndUtlInit's sphere.milo tolerance) — patch stickers
  are off the boot-to-menu path.

- **`BandUI::Init` online-event AddSinks gated** (`meta_band/BandUI.cpp:60-66`):
  `TheNetSession`/`TheNet.GetSearcher()`/`TheGameMicManager`/`TheSaveLoadMgr`
  AddSink lines `#ifndef HX_NATIVE` (their targets are null/excluded: NetSession is
  now real but these specific online-join/invite/USB-mic/saveload-dialog event
  subscriptions have no native meaning; `TheGameMicManager` Init is gated out,
  `TheSaveLoadMgr`/SaveLoadManager is excluded). KEPT the AddSinks to globals that
  ARE real natively: `ThePlatformMgr` (3×), `TheRockCentral` (its ctor compiles),
  `TheContentMgr->SetReadFailureHandler`.

**New glue TUs:** `native/src/rb3_platform_native.cpp`,
`native/src/rb3_netsession_native.cpp` (both added to `add_executable(rb3-native)`).
**Stubs added:** `band3_link_stubs.s` — NetSession base virtuals + `_ZTI10NetSession`
typeinfo. `dta_link_stubs.s` — weak `RB3InitNativeNetSession` no-op (rb3-dta link).
**No stubs removed** (the PlatformMgr/ContentMgr/NetSession ctor stubs are weak;
strong native defs win automatically, so the weak entries stay for rb3-dta).

**Regressions GREEN:** `RB3_BOOT` → "SystemInit OK — objects-cfg=… (227 entries)"
+ "boot complete." (exit 0). `rb3-dta songs/songs.dta` → parses songs (10 shown).
Both targets build clean.

**Next (T5/T6):** bring `MetaPanel.cpp` clang-clean (or at minimum register the
panel factories it owns — `ChooseColorPanel` + siblings) so the `ui/init.dta`
screen-include chain loads. Then per-class `Load()` correctness for the menu milos
(`meta_panel.milo` → `main_hub.milo`) per the `DirLoader:997` rule.

### T1/T2/T3 landed + boot advanced through SystemInit (2026-05-27, orchestrator)
Agent A delivered T1+T2+T3: `RB3_GAME=1` links RB3's real `App` (ui/world/track/
beatmatch/meta/bandobj/char + band3/meta_band/game/tour/bandtrack + App.cpp; 43
uncompilable TUs excluded, 730 syms stubbed in `band3_link_stubs.s` with writable
.bss for DATA syms), App.cpp HX_NATIVE init-guards + native frame loop, `BandRnd`
as `TheRnd`. Also found+fixed: RB3's 3-arg `SystemPreInit` lived in a
platform-excluded Wii TU (added HX_NATIVE 3-arg `SystemPreInit` to os/System.cpp).

Then the orchestrator drove the boot forward through a sequence of blockers:
1. **Missing-asset desync (`rndobj/sphere.milo`)** → `RndUtlInit`'s synchronous
   milo load aborted at `DirLoader` `t==TempEof` (assert 0x3E5). Root cause: native
   `NewFile` returns a non-null *failing* File for a missing path, so `ChunkStream`'s
   `mFail=!mFile` stayed false and it read a garbage header. **Fix:** `ChunkStream`
   ctor (HX_NATIVE) `mFail = !mFile || mFile->Fail()` — a failed binary open now
   reports Fail() so `DirLoader::OpenFile` cleans up (null dir; `RndUtlInit` already
   null-tolerates the sphere). Deliberately **kept** `NewFile` lenient (non-null
   failing handle) so a missing DTA `#include` (e.g. dev-only `ui/dev_only/selvenue.dta`,
   absent from the 360-ARK extract) still parses as empty — DTA text reads go
   through the File directly, not ChunkStream. (First tried a global NewFile
   null-on-fail like DC3; it regressed `RB3_BOOT` by hard-failing the selvenue
   `#include` → reverted to the ChunkStream-local fix.)
2. **`ThePlatformMgr.mDiscErrorMgr->mActive` (App.cpp:158)** — Wii disc-error mgr,
   null on native → gated `#ifndef HX_NATIVE`.
3. **`CreateNativeSynth` weak-stubbed → null** (SynthPreInit crash). Engine's
   `Synth_Stub.cpp` NativeSynth is excluded (RB3 StandardStream shape differs).
   **Fix:** `native/src/rb3_synth_native.cpp` returns base headless `Synth` (==
   `use_null_synth`; no audio assets anyway). Removed its weak stub.
4. **`RemoteBandUser` ctor `TheWiiFriendMgr.AddSink`** (BandUser.cpp:528) — Wii
   online friends mgr, unconstructed stub → gated `#ifndef HX_NATIVE` (ctor+dtor).

**Boot now reaches** (`RB3_GAME=1 MILO_HEADLESS=1 RB3_DATA=<extract> MILO_MAX_FRAMES=3`):
GpuDevice → App ctor → SystemPreInit → TheRnd->PreInit (sphere handled) → SynthInit
→ Movie::Init → TheRnd->Init → **SystemInit (227 cfg + full ui.dta parse)** →
`BandUserMgrInit` → **CURRENT BLOCKER: `BandUserMgr` ctor `ThePlatformMgr.AddSink`
(BandUserMgr.cpp:58) SIGSEGV** — `MsgSource::RemoveSink` walks `ThePlatformMgr.mSinks`
which is zeroed garbage.

**Root mechanism (the systematic T2/T4 crux):** `ThePlatformMgr` (`PlatformMgr.cpp:11`,
base class) has its **ctor `PlatformMgr::PlatformMgr()` defined in the EXCLUDED
`PlatformMgr_Wii.cpp:125`** → the static initializer calls a weak-stubbed noop ctor
→ the global is zeroed, never constructed → `MsgSource` base (`mSinks`) is garbage →
any `AddSink`/method faults. The same applies to the other Wii/net manager globals
RB3 derefs directly (`TheNetSession`/`session`, `TheContentMgr`, `TheWiiFriendMgr`,
…). **Faithful fix = keep-and-gate / construct-the-real-global:** provide minimal
native ctors/impls so each manager global is a properly-constructed object (valid
`MsgSource` base) answering DTA with the safe offline defaults from
[DTA_MANAGER_STUBS.md](DTA_MANAGER_STUBS.md). NOT per-call-site gating (whack-a-mole)
— the globals are used pervasively. `RB3_BOOT` regression stays GREEN (227 entries).

**De-risk — menu milos EXIST (asset-viable, unlike audio/sphere/selvenue gaps):**
`ui/gen/meta_panel.milo_xbox` (184KB), `ui/main/gen/main_hub.milo_xbox` (5.3MB —
the main menu), `ui/splash/gen/splash.milo_xbox`, `ui/gen/meta_loading.milo_xbox`
all PRESENT. Only `ui/splash/gen/intro_movie.milo_xbox` is MISSING — consistent
(Bink intro, auto-skipped natively). So boot→main-menu is achievable; T5/T7 viable.

### T4 research DONE (2026-05-27) — strategy inverted from DC3
Spec written: [DTA_MANAGER_STUBS.md](DTA_MANAGER_STUBS.md). **Headline: RB3 is the
inverse of DC3.** Where DC3 registered ~6 bare-`Hmx::Object` smart-stubs by name,
**RB3's `*_mgr` objects are nearly all REAL C++ singletons that `SetName`
themselves on the existing `App::App` init spine** (`MetaPanel::Init` @ App.cpp:248
is the hub; `BandUI::Init`/`BandUserMgrInit`/`SessionMgr::Init` wire the rest). So
the faithful native strategy (which the goal demands) is **keep-and-gate (outcome
A)**: let the real singletons construct, gate only their Wii/online *side-effects*
under `#ifndef HX_NATIVE`. The real managers already answer DTA correctly offline
(`session_mgr is_local`→true via `NetSession::IsLocal` at idle; `NetCacheMgr::IsReady`→1).
Genuine-stub set shrinks to: `session`, `content_mgr`, `net_cache_mgr` (+`saveload_mgr`
fallback) — and ONLY if T1/T2 exclude their subsystem from the link.
**Key RB3-vs-DC3 trap:** `BandUI::Init` derefs `TheNetSession`/`TheContentMgr`/
`TheSaveLoadMgr` typed globals directly (`BandUI.cpp:60-70`) — so if a subsystem
is excluded, name-registration is insufficient; the `TheX` global must be set to a
safe object too. Hence **prefer keep-and-gate.**
UI bypasses (per spec §5): `mSink`/`set_sink` NOT needed on RB3; the IsAnimating
analog is `Entering()`/`PanelDir::Entering()` (`UI.cpp:551-553`) — gate only if
diagnosed; the real native needs are (1) Bink intro-movie graceful-done in
`Movie::Ready/Poll` (`#ifdef HX_NATIVE` → Ready=true,Poll=false so
`intro_movie_screen` loads + instantly fires `movie_done`), and (2) a synthetic
Start/Confirm on `splash_screen` (native input source, not a DTA edit).
Single hard gate to verify: `saveload_mgr is_idle` must reach idle with no Wii
memcard (else force-idle, mirroring DC3 NativeSaveLoadStub).

### T8/T9 song-load path mapped (read-only recon, 2026-05-27)
Song-select: `MusicLibrary` drives `song_select_panel` (`MusicLibrary.cpp:197`),
entered via `start_in_setlist_browser` from main_hub. Picking a song →
**`Game` ctor** (`game/Game.cpp:168`): sets `mLoadState=kLoadingSong`, builds a
`BeatMaster` (`:177`, from `beatmatch/`), then **`LoadSong()`** (`:202`→`:235`),
which ends in `TheSongDB->PostLoad(GetBeatMaster()->GetMidiParserMgr()->GetEventsList())`
(`:264`). Per-instrument note play = `GemPlayer` (`game/GemPlayer.h`) /
`RealGuitarGemPlayer`. T9 scope = bring up `Game`/`BeatMaster`/`GemPlayer`/
`beatmatch`/MidiParser + the venue/char/bandobj Load.
**Asset bound (precise):** the code path *reaches* `Game::LoadSong`, but
`BeatMaster`/MidiParser need the song `.mid` (absent) and playback needs `.mogg`
(absent). So the achievable T9 = the song-LOAD code path executes (Game
constructed, venue/HUD milo loaded via FileMerger); a real chart/audio playthrough
needs the missing Wii `.ark` song data. Handle missing `.mid`/`.mogg` via the real
code path's graceful empty-song handling, not a skip-hack.

### T1+T2+T3 DONE (2026-05-27, Opus) — real App links, constructs, runs the frame loop

**Status: DONE.** `rb3-native` now physically links RB3's real `App`; the App ctor
runs the real boot spine; `RB3_GAME=1` constructs the App and enters the native
frame loop. The boot reaches its first natural stopping point: the documented
`DirLoader:997` desync. Render backend = BandRnd (`TheRnd`); GpuDevice up headless
before boot.

**T1 — CMake link bring-up.** Key finding: the 295/553-TU compile wall was **not**
header rot — **236 of 295 were `'X' file not found`** from two missing include
roots. Added `${REPO_ROOT}/src/band3` (resolves `game/ tour/ meta_band/ net_band/
bandtrack/`) + `${REPO_ROOT}/src/network` (resolves `net/ Platform/ Protocol/`) to
both the engine-injection and target include dirs (`native/CMakeLists.txt`) →
failures collapsed to **43 TUs**. Two 1-line additive `#ifdef HX_NATIVE` header
fixes for the STLport `vector<T,unsigned short>` size-hint idiom (host libstdc++
rejects it): `bandobj/ChordShapeGenerator.h:19`, `meta_band/SaveLoadManager.h:137-138`.
  - Source set added to `NATIVE_FORK_SOURCES`: `system/{ui 37/37, world 16/16,
    track 5/5, beatmatch 44/46, meta 28/30, movie 2/4, bandobj 55/60, char 56/61}`
    **+** `band3/{meta_band 153/172, game 69/74, net_band 4/11, tour 23/27,
    bandtrack 13/13}` **+** `src/App.cpp` (counts are compiled/total; `_Wii`/`_Xbox`
    platform TUs excluded too).
  - **43 boot-path TUs don't yet compile under clang LP64** (Wave-1/2 matched-fork
    gaps: switch jump-over-init, more `vector<T,N>`, DC3-vs-RB3 header shapes).
    EXCLUDED via `_NATIVE_FORK_EXCLUDE` (anchored `/Name.cpp$`); their **730 link
    symbols stubbed** in new `native/src/band3_link_stubs.s` — **665 FUNC →`.text`
    noop**, **65 DATA → own writable `.bss` reservation** (so written-to globals/
    singletons/static-members/vtables/typeinfo are read+write safe; this is the
    differentiator vs the all-`.text` rndobj stubs). Critical EXCLUDES: `Band.cpp`
    (BandInit), `MetaPanel.cpp` (MetaPanel::Init), `MusicLibrary/AssetMgr/PrefabMgr/
    SaveLoadManager/FileMerger/GameConfig/InputMgr`. Critical COMPILED:
    **`BandUI.cpp` (TheUI binding)**, `BandSongMgr/ContextChecker/BandUserMgr/Game/
    Char/World/Track/BeatMatch/UI/UIEventMgr`.
  - Also: `file(GLOB)` pulled the permuter's hidden `.permuter_work_*.cpp` scratch
    files → added `_FORK_EXCLUDE_REGEX_DOTFILE`.

**T2 — App.cpp HX_NATIVE guards + native frame loop** (`src/App.cpp`, all additive;
asm-match `#else` byte-identical). Gated includes (MWCC/Wii-GX): `MSL_Common/
null_def.h`, `<revolution/VI.h>`, `movie/CustomSplash_Wii.h`, `rndwii/{Env,Rnd}.h`.
Gated calls (`#ifndef HX_NATIVE`): VISetBlack/VIFlush; the whole `Splash`/
`CustomSplash`/`spl.*`/`splasher_time` machinery; `TheNet/TheRockCentral/
TheEntityUploader.Init` + `GameMicManager/UsbMidi*::Init`; `TheWiiProfileMgr.Init`;
the `NewFile("charnames.zbm")` disc probe; `MemPushHeap(MemFindHeap(...))`; the
`PollTriFrame` Wii-GX tri-frame block. KEPT REAL: SystemPreInit, TheRnd->PreInit/
Init, SynthInit, Movie::Init, SystemInit, the full `*Init` cluster, **TheUI.Init()**,
TheQuestMgr.Init. Native frame loop at top of `RunWithoutDebugging`: `SystemPoll;
TheUI.Poll; TheTaskMgr.Poll; TheSynth->Poll; TheRnd->BeginDrawing; TheUI.Draw;
TheRnd->EndDrawing` bounded by `MILO_MAX_FRAMES` (default 5) + `sigsetjmp` draw
guard. **No TheFlowMgr.** Also gated `movie/Splash.cpp` (`<revolution/VI.h>` +
`VISetBlack`/`VIFlush` in `Splash::Draw`) so it compiles — `Splash.cpp` is in the
exclude list but `TheSplasher`/PollTheSplasher need it; left it stubbed (splash is
gated out anyway).

**T3 — BandRnd as TheRnd + RB3_GAME mode.** `rb3_band_rnd.cpp` already strong-defines
`Rnd* TheRnd = &gBandRnd`, so the App path renders through BandRnd with no new
wiring; `TheRnd->PreInit()` runs the real `Rnd::PreInit`. New `RB3_GAME=1` mode in
`native/src/main_native.cpp`: GpuDevice up (1280×720 headless) BEFORE chdir, then
`chdir(RB3_DATA)`, `SetSystemArgs`, `mPlatform=kPlatformXBox`, `App app; app.Run()`.
Added `gDrawJmpBuf`/`gDrawJmpBufSet` + sigaction(SIGSEGV/ABRT/BUS) handler. All
prior modes intact.

**Native-glue fix found booting:** App calls 3-arg `SystemPreInit(argc,argv,config)`
whose only def is in platform-EXCLUDED `os/System_Wii.cpp` → resolved to a weak
no-op → `ObjectDir::sMainDir` never created → `Rnd::PreInit`'s `SetName("rnd",
sMainDir)` asserted (`Object.cpp:224`). Added additive HX_NATIVE 3-arg
`SystemPreInit` to `os/System.cpp` (SetSystemArgs + curated 1-arg SystemPreInit).

**Exact stop (reproducible):** App ctor → `TheRnd->PreInit()` (`rndobj/Rnd.cpp:295,
387`) → **`RndUtlInit()` (`rndobj/Utl.cpp:1166`)** → `DirLoader::LoadObjects(
FilePath(FileSystemRoot(), "rndobj/sphere.milo"))` → **`obj/DirLoader.cpp:997
(t == TempEof)`** → SIGABRT. Root cause: **`rndobj/sphere.milo` is absent from the
extracted asset tree** (360-ARK fallback omits it) → ChunkStream over the missing
file desyncs at the documented `DirLoader:997` boundary. T4/T5 next: provide/guard
the sphere asset (or HX_NATIVE-guard `RndUtlInit`'s synchronous load), then per-class
`Load()` for the milos the boot loads after.

**Regressions green:** rb3-dta parses songs; RB3_BOOT → 227 type-defs clean exit;
RB3_RENDER_MESH `ui/track/gen/tracksystem_meshes.milo_xbox` → **129 meshes** to PNG
(unchanged); RB3_RENDER_TRI + RB3_GPU_SMOKE OK; default header dump OK.

**Surprises vs DC3:** (1) RB3's 3-arg SystemPreInit lives in a platform-excluded Wii
TU (silent-no-op trap); DC3's is compiled. (2) RB3 hit the **asset gap**
(`sphere.milo` missing) during `Rnd::PreInit→RndUtlInit` — DC3's renderer init does
no synchronous milo load, so DC3 reached DTA-manager territory first. (3) No
first-screen C++ call added (RB3's first screen is DTA `goto_screen`, per plan).
