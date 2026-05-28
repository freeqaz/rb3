# RB3 Native Port — Divergence Audit (read-only)

**Date:** 2026-05-27
**Scope:** `rb3/native/src/`, `rb3/native/CMakeLists.txt`, `rb3/src/system/**`, `rb3/src/band3/**`, `rb3/src/App.cpp`, and the engine `platform/` seam.
**Reference standard:** dc3-decomp native (371/371 matched, audio-playing) + the principle from `rb3/CLAUDE.md` and `SONG_LOAD_ACHIEVED.md`: *retain the actual game code; diverge only where native platform differences force it, gated `#ifdef HX_NATIVE`.*

---

## 1. Executive summary

The rb3-native port is, on the whole, **clean for a Wave-3 work-in-progress**: ~430 `HX_NATIVE` blocks across the matched fork, and ~3,090 weak no-op link stubs across three `.s` files. The overwhelming majority of those are legitimate platform divergences — Wii SDK calls, RVL_SDK headers, GX render backend, Quazal/DWC online stack, BinkPlayer, MWCC paired-singles asm, and STLport/POSIX collisions. The native shim TUs (`rb3_keychain_native.cpp`, `rb3_netsession_native.cpp`, `rb3_platform_native.cpp`, `rb3_stream_receiver_native.cpp`, `rb3_synth_native.cpp`) are well-scoped: each one fills a *specific* matched-fork TU that cannot compile clang-clean and documents the contract it preserves. The native shim files only grow into "real implementations" where the matched-fork sister is genuinely uncompilable (NetSession lives in un-globbed `network/net/`, KeyChain's RB3 shape differs from DC3's, `BandNetGameData` has no `.cpp` anywhere in the decomp).

That said, there are **a handful of real divergence hacks** that should be cleaned up before V1 acceptance. The biggest are the two cosmetic-venue / vignette-completion shortcuts (`WorldInstance::SyncDir` skip + `InterstitialPanel::Exiting` short-circuit), the lenient `MILO_FAIL_DTA → return-default` paths in `DataNode::Int/Sym`, and the empty-source-list `add_executable(rb3-native …)` block, which itself is a divergence pattern — it grew rather than being maintained as a single principled rule. Diagnostic logs (`GAME_DBG`, `MOGG_DBG`, `STREAM_DBG`, etc.) are flagged for post-V1 retirement. Two synthetic mechanisms (`rb3_game_input.cpp`, the `NativeSaveLoadStub` + `NativeNetCacheMgrStub` registration) are intentional and on the V1-acceptance road; they are characterized as `DIAGNOSTIC` / `HACK-CLEAN` and need replacement by the natural input/save-game paths post-V1.

Overall divergence profile: **~5% hacks, ~10% diagnostic, ~85% legitimate.** No `Co-Authored-By` lines anywhere in `src/` or `native/`.

---

## 2. Per-pattern findings

### Pattern 1 — Forcing game state imperatively in C++ that should flow through DTA scripts

| Site | What it does | Severity | Suggested fix |
|---|---|---|---|
| `rb3/native/src/rb3_game_input.cpp:271` `TheProfileMgr.SetHasSeenFirstTimeCalibration(true)` | Sets the calibration-seen flag at boot so EndOvershell skips the first-time-calibration screen and goes straight to `main_hub_screen`. | `HACK-CLEAN` | This IS the real flag the calibration screen sets on completion — calling the real setter is one notch better than a DTA edit. The cleaner fix is to drive the `first_time_calibration` screen to completion synthetically (it's a 3-step audio/video sync wizard). Acceptable as a V1 shortcut. |
| `rb3/native/src/rb3_game_input.cpp:280-297` direct `TheSongMgr.AddSongs(DataReadFile("songs/songs.dta"))` | Loads songs from disk into the song manager directly, bypassing `TheContentMgr`'s disc/content scan. | `HACK-BLOCKING` | Wire `TheContentMgr` (the base `ContentMgr` constructed by `rb3_platform_native.cpp`) to scan the extracted `songs/` directory and fire its `RefreshDone` callback chain. The base `ContentMgr` already has the no-op vtable for this; we need a `NativeContentMgr` that calls `TheSongMgr.AddSongs` on `StartRefresh`. Mirrors how the real path works without bypassing the manager. |
| `rb3/native/src/rb3_game_input.cpp:199-202` `JoypadGetPadData(0)->mConnected=true` + `DebugSetControllerTypeOverride(kControllerGuitar)` | Marks pad 0 connected and forces a guitar instrument so the overshell add-user flow succeeds. | `HACK-CLEAN` | Real fix is a `Joypad_Native` shim that reports a connected guitar on pad 0 from the regular joypad poll path. (`Joypad_Native.cpp` is excluded from the link because its DC3 shape differs from RB3's — a small `rb3_joypad_native.cpp` would replace this.) |
| `rb3/native/src/rb3_game_input.cpp:188` `JoypadInitCommon(SystemConfig("joypad"))` called from `SynthUser()` | The hardware-free half of `JoypadInit()` is invoked from the synthetic-user binding instead of from the real boot spine. | `HACK-CLEAN` | Move this call into `App.cpp`'s native frame loop init (or a real `JoypadInit` HX_NATIVE block) so a synthetic-input-disabled native run still has `gControllersCfg` / `gButtonMeanings` populated. |
| `rb3/native/src/rb3_game_input.cpp:222-245` `NativeSaveLoadStub::Handle` returns `is_idle=1` / `is_initial_load_done=1` directly | A bare `Hmx::Object` subclass answers the DTA query with constants instead of going through the real `SaveLoadManager::IsIdle()` state machine. | `HACK-CLEAN` | The matched-fork `SaveLoadManager.cpp` is excluded (`_NATIVE_FORK_EXCLUDE`) because it's tied to `MemcardMgr_Wii`/`WiiProfileMgr` — DTA_MANAGER_STUBS §4 explicitly sanctions this fallback. Bring up a Wii-free `SaveLoadManager` TU that reaches `kIdle` naturally via a native `MemcardMgr_Native` stub; pursue post-V1 once the boot path stabilizes. |
| `rb3/src/system/world/Instance.cpp` (sv3 cosmetic-venue proxy deferral) | `WorldInstance::SyncDir` SKIPS proxy instancing entirely for venues under `world/vignette/` and `world/shared/` (a behavior switch, not a code-path leniency). | `HACK-BLOCKING` | The instance-proxy parent-`Dir()` wiring is a real bug in the inlined-cached-shared object load path (the real game does cosmetic venue instancing identically). Fix `ObjectDir::PostLoadInlined` so the inlined sub-dir's parent `Dir()` is wired before `SyncDir` runs. This deferral was the documented "option B" path; option A (real wiring) is the V1 cleanup target. |
| `rb3/src/band3/meta_band/InterstitialPanel.cpp` `Exiting()` HX_NATIVE short-circuit | The vignette-camshot completion gate (`mCamshotDone` + `unk88>=3`) is bypassed because the venue render+anim doesn't drive `transition_camshot_done.trg`. | `HACK-BLOCKING` | Same root cause as Instance.cpp deferral above — once cosmetic venues instance cleanly, the camshot anim runs and this gate flips naturally. Until then, the bypass is necessary but not principled (it bypasses a DTA-driven gate via C++). |
| `rb3/src/band3/meta_band/InterstitialPanel.cpp` `BackdropPanel::Exiting()` outro short-circuit | `mOutroDone` is forced via outro skip because the venue `.anim/.trg` for `vignette_outro_done` doesn't fire. | `HACK-BLOCKING` | Same as above. |
| `rb3/src/system/obj/DirLoader.cpp` `PollLoading()` synchronous override | Replaces the matched-fork time-sliced `LoadMgr::Poll` budget gate with "always advance one state per poll". | `LEGITIMATE` | The matched-fork `while (!CheckSplit() && …)` gate depends on a Wii vsync-paced 10ms budget; on a fast host `CheckSplit()` is already true on entry and the body never runs. DC3 native does the same. Listed here so it isn't mistaken for a hack. |

### Pattern 2 — Duplicating decomp code instead of calling/using it

| Site | What it does | Severity | Suggested fix |
|---|---|---|---|
| `rb3/native/src/rb3_keychain_native.cpp:32-195` (full keychain algorithm) | Re-implements the entire keygen scrambler (`shuffle1..6`, `revealKey`, `random`, `mash`, `getKeyImpl`) verbatim from the matched fork's `keygen_xbox.cpp`. | `LEGITIMATE` | The matched-fork `src/keygen_wii.cpp` lives outside the globbed `src/system/**` source set, and the engine's `Keygen_Stub.cpp` is DC3-namespace-shaped (not RB3 class-shaped). File header documents both. This is a textbook shim, not duplication; only deltas from DC3 are the C++ signature shape. |
| `rb3/native/src/rb3_netsession_native.cpp:40-95` strong native `NetSession` ctor + non-pure virtuals + offline query methods | Replaces the un-globbed `network/net/NetSession.cpp` with a minimal offline subclass. | `LEGITIMATE` (file is well-documented) | The matched fork's `NetSession.cpp` is in the Quazal subsystem that is intentionally off the V1 link line. The shim's offline semantics (`IsLocal()` true iff idle, `HasUser` walks `mUsers`) mirror the real `NetSession.cpp`. Track for future cleanup when `network/net/NetSession.cpp` is brought up clang-clean (probably never — Nintendo WFC is dead). |
| `rb3/native/src/rb3_netsession_native.cpp:221-231` `BandNetGameData` ctor + virtuals | There is no `BandNetGameData.cpp` in the decomp (header only), so a strong native impl is required. | `LEGITIMATE` | Not a duplicate — the file simply doesn't exist anywhere else in the codebase. |
| `rb3/native/src/rb3_platform_native.cpp:47-92` native `PlatformMgr` ctor + `RegionInit` + `TheContentMgr` base ctor | Reimplements the offline-default field-init list for `PlatformMgr` because `PlatformMgr_Wii.cpp` is platform-excluded. | `LEGITIMATE` | Mirrors `PlatformMgr_Wii.cpp:125` field inits verbatim (offline = all signed-out, no net, no disk error). The MsgSource-base ctor reason is real (without this the sink list is uninit). |
| `rb3/native/src/rb3_stream_receiver_native.cpp:42-235` `RB3StreamReceiverNative : StreamReceiver, AudioSource` | Bridges RB3's matched-fork StreamReceiver onto the engine's miniaudio `AudioDevice`. Reimplements producer/consumer cursoring + back-pressure rather than reusing the engine's `StreamReceiver_Native.cpp`. | `LEGITIMATE` | The engine's `StreamReceiver_Native.cpp` is excluded because its `override`'d `IsOutputDrained()` / `SetSlipOffset()` only exist in DC3's `StreamReceiver` shape, not RB3's 2010 header. The audit found NO subtle ABI breakage — file header explains the dual-inheritance dance. |
| `rb3/native/src/rb3_synth_native.cpp:25-57` `NativeSynth : Synth` | Adds `StreamReceiver::sFactory = &RB3CreateNativeStreamReceiver` + `AudioDevice::GetInstance().Init(44100)`. | `LEGITIMATE` | RB3's base `Synth::NewStream/NewStreamDecoder` are correct (uses `StandardStream` + `VorbisReader`); the engine's `Synth_Stub.cpp` would re-override them with DC3's shape. File header explicitly says "we deliberately do NOT override those — RB3's base is fine." Clean. |
| `rb3/native/src/rb3_band_rnd.cpp` (746 lines) | A complete `BandRnd : Rnd` renderer using WebGPU, paralleling DC3's `Rnd_Wgpu.cpp` but RB3-shape-specific. | `LEGITIMATE` | The README and engine `CLAUDE.md` document the RB3 vs DC3 rndobj header divergence (RB3 2010-era `rndobj/Mesh.h` cannot compile against the WebGPU-coupled engine TUs). `MILO_ENGINE_BUILD_GPU_BACKENDS=OFF` keeps the engine's NgRnd-coupled backends out; `BandRnd` fills that hole. Not on the V1 audio path (currently no-op'd; only RB3_RENDER_MESH path exercises it). |

### Pattern 3 — Hardcoded `return true`/`return 0`/no-op stubs that should call into real code

The three `.s` files contain ~3,090 weak no-op stubs (`band3_link_stubs.s`: 1,365; `dta_link_stubs.s`: 392; `rndobj_synth_link_stubs.s`: 110). Most are platform-excluded TU symbols (correct: see Pattern 4 stub-retirement table). The ones worth flagging for matched-fork bring-up:

| Symbol family | Stub file | Bring-up effort | Severity |
|---|---|---|---|
| `_ZN13MidiParserMgr*` (MidiParserMgr ctor/Poll/Reset/FinishLoad/GetEventsList/GetParser) | `band3_link_stubs.s:438-451` | Medium — header exists in `system/beatmatch/`; MidiParser.cpp / MidiReader.cpp are already brought up. Should be 1–2 STLport iterator fixes. | `HACK-BLOCKING` for V1 audio gameplay |
| `_ZN15SaveLoadManager*` (Init, AutoSaveNow, AutoSave) | `band3_link_stubs.s:535-540` | Hard — file is 2,266 lines, deeply tied to `MemcardMgr_Wii`/`WiiProfileMgr`. Documented as a known §4 stub-acceptance site. | `HACK-CLEAN` (offline-default stub is correct for V1) |
| `_ZN18TourPerformerLocal*` (ClearCurrentQuest, ClearCurrentQuestFilter, ctor) | `band3_link_stubs.s:557-561` | Medium — likely STLport iterators. | `HACK-CLEAN` |
| `_ZN15WaitingUserGate*` (Init, Poll, ctor) | `band3_link_stubs.s:549-553` | Likely small — meta_band/WaitingUserGate.cpp is short. | `HACK-CLEAN` |
| `_ZN13VocalNoteList*` (all 14 methods) + `_ZN11VocalPlayer*` (all 14 methods) + `_ZN11VocalPhrase*` | `band3_link_stubs.s:228-486` | Medium — vocal subsystem (`VocalNoteList.cpp`, `VocalPlayer.cpp`) is `_NATIVE_FORK_EXCLUDE`'d for V1 single-instrument. Bring-up is part of V2 vocals support. | `HACK-CLEAN` (post-V1) |
| `_ZN11ClipDistMap*` (4 methods) | `band3_link_stubs.s:159-168` | Low — only `Draw` is on the load path; it's off the path otherwise. | `HACK-CLEAN` |
| `_ZN12TourProgress*` (15 methods) | `band3_link_stubs.s:345-378` | Medium — `TourProgress.cpp` already has HX_NATIVE blocks. Investigate why it's still excluded. | `HACK-CLEAN` |
| `BinkOpen*` / `BinkClose*` / `BinkDoFrame` / `BinkGetSummary` / `BinkPause` / `BinkRegisterFrameBuffers` / `BinkSetSoundOnOff` / `BinkShouldSkip` / `BinkWait` | `band3_link_stubs.s:40-55` + `dta_link_stubs.s` + `rndobj_synth_link_stubs.s:15-34` | Legitimate — Bink is a closed-source RAD video library. | `LEGITIMATE` |
| `_ZN11RockCentral*` (UpdateFriendList, GetArtFile, EncodeMessage, etc.) | `band3_link_stubs.s:207-218` | Legitimate — Quazal/online stack. | `LEGITIMATE` |
| `_ZN12WiiFriendMgr*` / `_ZN12WiiMessenger*` / `_ZN14WiiCommerceMgr*` / `_ZN12VoiceChatMgr*` / `_ZN15DiscErrorMgrWii*` | `band3_link_stubs.s` | Legitimate — Wii NAND / DWC online managers. | `LEGITIMATE` |
| `_Z8BandInitv` (BandInit) | `band3_link_stubs.s:82` | Easy — `Band.cpp` `BandInit()` is the band-character init function. Likely small STLport/POSIX fix. | `HACK-CLEAN` |
| `_Z18WhiteKeyToSemitonei` / `_Z18SemitoneToWhiteKeyi` | `band3_link_stubs.s:72-75` | Trivial — short standalone functions; investigate why they're stubbed. | `HACK-CLEAN` |
| `_ZN13MetaPerformer*` (8 methods) | `band3_link_stubs.s:420-437` | Medium — MetaPerformer is needed for the meta→game transition; gem track playback requires it. | `HACK-BLOCKING` for V1 gem track |
| `_ZN13MidiParserMgr*` + `_ZN12BandDirector14HarvestDircutsEv` + `_ZN12BandDirector19ReadyForMidiParsersEv` | `band3_link_stubs.s:255-258, 438-451` | Medium. | `HACK-BLOCKING` for V1 gem track |
| `_ZN11GameGemList*` (8 methods) | `band3_link_stubs.s:169-188` | These should be removable — `GameGemList.cpp` was already brought up clang-clean per `SONG_LOAD_ACHIEVED.md` X7b. Investigate stale stubs. | `HACK-CLEAN` (stale) |
| `_ZN10GameConfig*` (10 methods) | `band3_link_stubs.s:103-119` | Should be straightforward — `GameConfig.cpp` is on the V1 critical path per `V1_PATH_DECISION.md` X8. | `HACK-BLOCKING` for V1 gem track |
| `_ZN12MusicLibrary*` (~30 methods including critical `OnLoad`, `OnUnload`, `SetTask`, `AppendToSetlist`, `SendSetlistToMetaPerformer`) | `band3_link_stubs.s:263-328` | `MusicLibrary.cpp` is on the compiled set and works for song-select (per BOOT_TO_SONG.md). These method stubs look like they predate the file's bring-up — likely STALE stubs from earlier. | `HACK-CLEAN` (verify whether stale; if MusicLibrary.cpp is in the link, strong defs should win — but the unused weak stubs muddy auditing) |
| `_ZN21AccomplishmentManager*` (~25 methods) | `band3_link_stubs.s:589+` | `AccomplishmentManager.cpp` has HX_NATIVE blocks (verified). Should compile; check why stubs remain. | `HACK-CLEAN` |

### Pattern 4 — HX_NATIVE blocks that BYPASS DTA-driven behavior

| Site | What it does | Severity | Suggested fix |
|---|---|---|---|
| `rb3/src/system/beatmatch/SongData.cpp:401-405` `PostLoadVocals` HX_NATIVE early-return | Skips vocal post-load entirely. | `LEGITIMATE` (for now) | VocalNoteList / VocalPlayer / Singer TUs are in `_NATIVE_FORK_EXCLUDE` — calling methods on stub-vtable VocalNoteList* dereferences invalid memory. Comment explicitly says "When the vocals TUs come up clang-LP64-clean, drop this guard." V2 cleanup. |
| `rb3/src/system/beatmatch/SongData.cpp:417-430` `AddKeyboardRangeShift` warn+clamp on non-positive span | MILO_FAIL → MILO_WARN + clamp. | `HACK-CLEAN` | This is a real authoring artifact in PART REAL_KEYS_X charts; keys aren't on the V1 playback path. The fix should be a one-line keys-disabled gate, not a lenient assert conversion. |
| `rb3/src/system/obj/DataNode.cpp:213-228` `DataNode::Int` MILO_FAIL_DTA → return 0 fallback | `MILO_FAIL_DTA("Data is not Int")` falls through to coerce float→int or return 0. | `HACK-CLEAN` | The 360-ARK config DTAs have schema-mismatched fields. The clean fix is to fix the DTA-schema mismatch in the consumer config files, not to make `DataNode::Int` lenient. (Comment justifies this as "mirroring DataNode::Int's recovery" which is circular.) |
| `rb3/src/system/obj/DataNode.cpp:266-280` `DataNode::Sym` / `LiteralSym` MILO_FAIL_DTA → null Symbol fallback | Same pattern. | `HACK-CLEAN` | Same. |
| `rb3/src/system/obj/DataNode.cpp:171-184` `DataNode::Evaluate` property null-guard | `Property(true)` returns null instead of MILO_FAIL'ing; native handles it. | `LEGITIMATE` | Native `Debug::Fail` can return (not noreturn like the matched PPC build). The guard is correct C++; it's a platform difference not a hack. |
| `rb3/src/band3/meta_band/AppScoreDisplay.cpp:?` `UpdateDisplay` null-label skip | If combined-score AppLabel is absent, skip the update. | `HACK-CLEAN` | The 360-ARK score display milo is missing a label. Either fix the asset or make the dynamic_cast-and-return-null the standard path (the matched assert is over-strict for offline). |
| `rb3/src/band3/meta_band/SongSortMgr.cpp` empty-`mSetlists` tolerance | Don't abort when the internal_setlists shortname mismatch leaves `mSetlists` empty. | `HACK-CLEAN` | Same: 360-ARK schema vs RB3-Wii code expectation. Fix asset reconciliation or rationalize the assert. |
| `rb3/src/band3/meta_band/MusicLibrary.cpp` `Text`/`SongSetlistProvider`'s `Text` — null dynamic_cast skip | The 360-ARK song.lst uses plain UILabels where RB3 code expects AppLabel. | `HACK-CLEAN` | Asset reconciliation — the song_select.milo from the 360-ARK extract should be patched to match RB3-Wii's, or AppLabel's signature should tolerate plain UILabel. |
| `rb3/src/band3/meta_band/NetSync.cpp` "nested lock-step skip" + "offline transition allowed" | Skip starting a nested `IsBlockingTransition()` lock and unconditionally allow transitions offline. | `LEGITIMATE` | Single-machine offline has no peers to lock-step with; the matched assert is for multi-machine sync only. Correct platform divergence. |
| `rb3/src/band3/meta_band/MainHubPanel.cpp` `TheServer.AddSink` gate | `TheServer` (gWiiServer) is null offline. | `LEGITIMATE` | Online login server is genuinely absent offline. |
| `rb3/src/band3/meta_band/Matchmaker.cpp` `UpdateMatchmakingSettings` null-settings guard + `IsFinding` offline override | Skip null-SessionSettings deref + always-false-finding. | `LEGITIMATE` | Matchmaker only exists with an active Quazal session. |
| `rb3/src/band3/net_band/RockCentral.cpp` null `mContextWrapperPool` guard | RockCentral is gated off offline. | `LEGITIMATE` | Same. |
| `rb3/src/system/os/PlatformMgr.cpp:IsSignedIn`/`HasUserSigninChanged` MILO_FAIL → return false on pad-less user | Returns false instead of fataling for `GetPadNum()==-1`. | `HACK-CLEAN` | Real fix: the synthetic 3 unassociated BandUsers are an artifact of `rb3_game_input.cpp` only associating user[0] to pad 0; on a real Wii every user has a pad. If we drove all 4 users' pads correctly this guard wouldn't be needed. |

### Pattern 5 — Symbol forwarding / aliasing that hides matched-fork code

| Site | What it does | Severity | Suggested fix |
|---|---|---|---|
| `rb3/native/src/rb3_band_rnd.cpp:30` `Rnd* TheRnd = &gBandRnd;` over weak `TheRnd` stub | Replaces the weak stub with a strong def. | `LEGITIMATE` | Standard strong-def-wins pattern. |
| `band3_link_stubs.s` keeping `_ZN8KeyChain*` stubs after `rb3_keychain_native.cpp` strong defs | Stubs are still in `rndobj_synth_link_stubs.s:115-118` but documented as "displaced by strong defs". | `HACK-CLEAN` | Remove the displaced stubs (the file claims they "stay" but having both a weak no-op and a strong def causes confusing `--allow-multiple-definition` resolution). |
| Many comments like "stubs REMOVED — X is now compiled" with the stubs commented out instead of deleted | Files like `band3_link_stubs.s:189-190`, `508-510`, `527-530` keep dead "REMOVED" markers. | `DIAGNOSTIC` | Cosmetic — could be cleaned at the end of each milestone. |

### Pattern 6 — Engine-side workarounds for things RB3's matched fork can do correctly

| Site | What it does | Severity | Suggested fix |
|---|---|---|---|
| `milo-native-engine/src/audio/AudioDevice.cpp:142-151` `MILO_AUDIO=1` env override | Allows opening the audio device even with `MILO_HEADLESS=1`. | `LEGITIMATE` | Standard test-harness opt-in; documented inline. |
| `MILO_ENGINE_DECOMP_PLATFORM_EXCLUDE` list in `rb3/native/CMakeLists.txt:152-178` (15 platform/audio/file/char TUs) | Excludes DC3-shaped engine TUs that fail the `override` checks or pull DC3-only headers. | `LEGITIMATE` | Every entry has a 1-line rationale; this is the right `MILO_ENGINE_DECOMP_PLATFORM_EXCLUDE` seam usage. |
| `MILO_ENGINE_BUILD_GPU_BACKENDS=OFF` | RB3's 2010-era rndobj/ can't compile against the engine's DC3-wired Wgpu backends. | `LEGITIMATE` | Documented in CMakeLists. |

### Pattern 7 — Synthetic input / scripted flow vs natural input path

| Site | What it does | Severity | Suggested fix |
|---|---|---|---|
| `rb3/native/src/rb3_game_input.cpp` whole file (487 lines) — `RB3_GAME_INPUT` env-var DSL with `start`/`confirm`/`select:button`/`msg:object:action` directives | Drives the entire menu flow from a colon-separated script instead of from a real input source. | `DIAGNOSTIC` | This is the explicit V1 test harness — replacement by a real joypad/keyboard path is a separate post-V1 milestone (it requires the engine's Joypad_Native to be brought up on RB3's joypad shape). Keep + retire post-V1. |
| `select:<button>` directive bypassing milo d-pad nav graph | The synthetic select skips milo nav and focuses directly on a named UIComponent. | `HACK-CLEAN` | Replace with `up`/`down`/`confirm` once the d-pad nav graph is verified clang-clean. |
| `msg:<object>:<action>` directive sending raw DTA messages | Direct DTA-message injection bypasses both nav AND the SELECT_MSG path. | `HACK-BLOCKING` | This is the most-divergent of the three (it doesn't represent real input at all). It's only used for the `music_library:select_highlighted_node` + `music_library:play_setlist` + `overshell:end_override_flow` cases. Replace with the real `confirm` button-press path once those panels' confirm handlers run cleanly. |

### Pattern 8 — Diagnostic logs left in matched-fork

61 sites with `STREAM_DBG` / `BEATMASTER_DBG` / `VORBIS_DBG` / `MOGG_DBG` / `GAME_DBG` / `HUB_DBG` / `SONG_DBG` / `PART_DBG` / `INTERSTITIAL_DBG` / `UISCREEN_DBG`. All are gated on `getenv()` checks so they're cost-free at runtime unless explicitly enabled.

| File | Logs | Severity | Suggested fix |
|---|---|---|---|
| `rb3/src/system/synth/VorbisReader.cpp` (multiple `MOGG_DBG` dumps lines 29-66, 223, 231, 255, 268, 429, 435) | mogg key/decrypt/header trace | `DIAGNOSTIC` | Retire post-V1 once audio is stable. |
| `rb3/src/system/synth/StandardStream.cpp` (`STREAM_DBG`) | state machine transitions | `DIAGNOSTIC` | Keep through V1; retire after gem-track playback is verified. |
| `rb3/src/system/beatmatch/BeatMaster.cpp` (`BEATMASTER_DBG`) | LoaderPoll / audio load | `DIAGNOSTIC` | Retire post-V1. |
| `rb3/src/band3/game/Game.cpp`, `GamePanel.cpp` (`GAME_DBG`) | mLoadState transitions, IsLoaded gates | `DIAGNOSTIC` | Keep through V1 gem-track; retire after end-to-end song completes. |
| `rb3/src/band3/meta_band/NetSync.cpp`, `MusicLibrary.cpp`, `OvershellPanel.cpp` (`GAME_DBG`) | screen-transition + setlist flow | `DIAGNOSTIC` | Retire post-V1. |
| `rb3/src/system/bandobj/BandDirector.cpp` (`GAME_DBG`) | ReadyForMidiParsers / OnFileLoaded | `DIAGNOSTIC` | Retire post-V1. |
| `rb3/src/band3/meta_band/InterstitialPanel.cpp` (`INTERSTITIAL_DBG`) | BackdropPanel::Exiting flag changes | `DIAGNOSTIC` | Retire post-V1. |
| `rb3/src/band3/game/SyncGameStartPanel.cpp` (`UISCREEN_DBG`) | mState transitions | `DIAGNOSTIC` | Retire post-V1. |

### Pattern 9 — `Co-Authored-By` lines or off-convention artifacts

None found in `src/` or `native/`. Convention upheld.

---

## 3. Top 5 hacks to fix first (ranked by impact-to-effort ratio)

1. **`rb3/native/src/rb3_game_input.cpp:280-297` — direct `TheSongMgr.AddSongs` instead of `ContentMgr` refresh.**
   *Fix:* write a tiny `NativeContentMgr : ContentMgr` subclass in `rb3_platform_native.cpp` that, on `StartRefresh()`, scans `$RB3_DATA/songs/` and calls `TheSongMgr.AddSongs(DataReadFile)`, then fires `RefreshDone`. Replaces the imperative load with the real DTA-driven `{content_mgr start_refresh}` flow. *Effort:* ~30 lines + ~1 hour. *Impact:* removes the most visible divergence from the boot path and unblocks future content-pack support.

2. **`rb3/src/system/world/Instance.cpp` cosmetic-venue proxy deferral + `InterstitialPanel::Exiting` short-circuits.** **[V5b INVESTIGATION 2026-05-28: ATTEMPTED — defer to V2 milestone.]**
   *Original fix proposal:* wire the inlined sub-dir's parent `Dir()` in `ObjectDir::PostLoadInlined` (`Dir.cpp:565`).
   *V5b finding:* the single-line wire-up surfaces three structural obstructions:
     (a) `Instance.cpp:191-212` `LoadPersistentObjects` save/restore of `mDir->Dir()` mid-load corrupts the proxy's hash table if parent-wiring happens before LoadPersistentObjects;
     (b) shared dirs (e.g. `classic_blacktriple.milo`) are reused across MULTIPLE WorldInstance proxies — single `mDir` pointer can only name one parent; structural single-parent assumption breaks;
     (c) `Hmx::Object::Copy` doesn't copy `mDir`, so `foundObj`'s `mDir==null` even with `sharedDir->Dir()==this`; the next assertion chain fails.
   *Proper fix:* (a) defer parent-wiring until after LoadPersistentObjects, (b) many-to-one parent-chain abstraction (per-proxy shadow dirs), (c) explicit `HxSetDir`+`SetName` reconciliation. ~1 week of deep decomp work.
   *Status:* defer to V2 rendering milestone. HX_NATIVE `HxSetDir(ObjectDir*)` helper landed in `Object.h` as a tested seam for the future fix. V1 keeps the cosmetic-venue HX_NATIVE deferral as the working baseline. Visible cost: gameplay frames render the gem highway in a black void (no venue/band/crowd).

3. **Stale boot-path stubs in `band3_link_stubs.s` for already-compiled TUs (`MusicLibrary`, `AccomplishmentManager`, `GameGemList`, `TourProgress`, `OutfitConfig`).**
   *Fix:* audit each TU vs the stubs — strong defs already win, but the stub presence muddies the audit + slows linker resolution. Delete the displaced weak stubs (mirror how `CharacterTest`, `Synchronizable`, `BandNetGameData` were cleaned up). *Effort:* ~1 hour scripted (build a list of strong-defined symbols via `nm -t`, cross-ref with the weak stubs). *Impact:* recovers ~200 stubs of audit clarity.

4. **`rb3/src/system/obj/DataNode.cpp:213-280` — lenient `DataNode::Int/Sym/LiteralSym` for 360-ARK schema mismatches.**
   *Fix:* the underlying problem is 360-ARK `config/song_select.dta` schema vs RB3-Wii code expectations. Either (a) preprocess the 360-ARK DTAs to match RB3-Wii's schema during asset extraction, or (b) gate these handlers tighter so the falsy-return only fires on the specific schema-mismatch case, not for every `MILO_FAIL_DTA`. *Effort:* ~2 hours for the schema preprocessing fix. *Impact:* removes a class-wide lenient-cast that could hide real bugs.

5. **Native joypad shim (`rb3_joypad_native.cpp`).**
   *Fix:* small TU mirroring the engine's `Joypad_Native.cpp` but for RB3's 2010 `Joypad.h` shape. Removes the `JoypadGetPadData(0)->mConnected=true`, `DebugSetControllerTypeOverride`, and `JoypadInitCommon` synthetic-input hacks. Lets the synthetic-input layer drive a real joypad device instead of poking globals. *Effort:* ~100 lines + ~half a day. *Impact:* converts `rb3_game_input.cpp` from a "fake-state injector" into a "synthetic-input source" — a much cleaner abstraction.

---

## 4. Top 5 legitimate divergences (do NOT regress these)

1. **`rb3/src/system/utl/PoolAlloc.cpp` `_PoolAlloc(classSize, reqSize)` `max(classSize, reqSize)` LP64 multi-inheritance fix.** Load-bearing — fixes `VorbisReader : StreamReader, CriticalSection` under-allocation. The matched-fork `MILO_ASSERT(reqSize == classSize)` was always wrong but silently passed on Wii because MI tables happened to align. Mark this as **inviolable**.

2. **`rb3/src/system/os/Timer.cpp` + `Timer.h` `HxNativeMftb` + `sLowCycles2Ms = 1e-3` calibration.** Replaces a PPC `mftb` opcode that has no equivalent natively. Without this every screen transition stalls. Inviolable.

3. **`rb3/src/system/obj/Object.cpp` + `obj/ObjPtr_p.h` freed-object guard (`HxNoteFreedAddr`/`HxAddrWasFreed`).** Native-only ring buffer that prevents `~ObjPtr` from dereferencing a freed `CharBonesObject` through virtual base inheritance. RB3 analog of dc3-decomp's `SafeReleaseFromRing`. Inviolable.

4. **`rb3/src/system/char/CharBonesSamples.cpp` cached Xbox 360 16-byte-padded sample layout.** The on-disk 360 cached layout pads each `Vector3` with a zero float and rounds each sample to 16 bytes. The matched-fork Wii path reads an unpadded layout; reading 360 cached data needs the padded form. Inviolable.

5. **`rb3/src/system/rndobj/Bitmap.cpp` `mNativeCachedMips` cached-cube-mip discard.** Cached Xbox `.milo_xbox` bitmaps serialize a full mip chain after the base level; the Wii path zeroes `numMips` and the mip bytes are simply skipped in the file buffer — but `RndCubeTex` loads its faces from the *shared* stream, so the skipped mip bytes desync every later object. Native-only mip stash+discard. Inviolable.

(Honorable mention: `rb3/src/system/synth/VorbisReader.cpp` `KeyChain::getMasher` LP64 pointer-truncation fix — the matched path passes `(int)masterKey` through DTA pointer math, which truncates 64-bit pointers. Inviolable.)

---

## 5. Stub retirement priority list

Ordered by combined impact (boot-path blocking) × bring-up effort (lower = easier):

| Priority | Symbol family | Stub file | Reason | Effort |
|---|---|---|---|---|
| 1 | `_Z8BandInitv` (BandInit) | `band3_link_stubs.s:82` | Boot-path; small fn. | 1–2 h |
| 2 | `_ZN10GameConfig*` (10 methods) | `band3_link_stubs.s:103-119` | V1 gem-track critical path (V1_REMAINING_PLAN K7+). | Half day |
| 3 | `_ZN13MetaPerformer*` (8 methods) | `band3_link_stubs.s:420-437` | Meta→game transition. | Half day |
| 4 | `_ZN13MidiParserMgr*` + `BandDirector::HarvestDircuts/ReadyForMidiParsers` | `band3_link_stubs.s:255-258,438-451` | Gem-chart bring-up. | 1 day |
| 5 | Stale `_ZN12MusicLibrary*` / `_ZN21AccomplishmentManager*` / `_ZN11GameGemList*` / `_ZN12TourProgress*` / `_ZN12OutfitConfig*` | `band3_link_stubs.s` | Already-compiled TUs; strong defs win but stubs muddy audit. | 1 h (delete) |
| 6 | `_ZN18TourPerformerLocal*` (3 methods) | `band3_link_stubs.s:557-561` | Career path. | Half day |
| 7 | `_ZN15WaitingUserGate*` (Init, Poll, ctor) | `band3_link_stubs.s:549-553` | Multi-user join. | 2 h |
| 8 | `_ZN15SaveLoadManager*` (Init, AutoSaveNow, AutoSave) | `band3_link_stubs.s:535-540` | Save persistence (offline-default stub OK for V1). | Multi-day (Wii-deep) |
| 9 | `_ZN13VocalNoteList*` + `_ZN11VocalPlayer*` | `band3_link_stubs.s:228-486` | V2 vocals — defer. | Multi-day |
| 10 | All `Bink*` / `Wii*Mgr*` / `RockCentral*` / `_ZN10NetSession*` | All three `.s` files | Legitimate platform exclusions; keep. | n/a |

---

## 6. Open questions

1. **Cosmetic-venue proxy fix (Top-5 hack #2):** is the inlined-cached-shared `ObjectDir::PostLoadInlined` parent-Dir wiring fix worth a week of decomp work pre-V1, or should we accept the deferral + Exiting() short-circuits until post-V1 cosmetic-venue render? The current state is functional for headless V1 but blocks any visual V1 demo.

2. **360-ARK schema vs RB3-Wii code:** is asset-side preprocessing (rewriting the 360-ARK `config/song_select.dta` to RB3-Wii's expected schema during extraction) acceptable, or is "the native code tolerates 360-ARK schema mismatches" a desired property? The lenient `DataNode::Int/Sym/LiteralSym` paths could be tightened either way.

3. **Synthetic-input layer (`rb3_game_input.cpp`):** the `msg:<object>:<action>` directive is the most-divergent piece. Is replacing it with real button presses (now that we have d-pad nav) viable for V1, or is the directive needed for `play_setlist` / `end_override_flow` cases that have no clean button equivalent?

4. **Stale `band3_link_stubs.s` entries:** is there a script (or should we write one) that diffs the stub file against the strong-def symbol set from `nm build-native/rb3-native`? Would automate Priority-5 cleanup permanently.

5. **`PlatformMgr_Wii.cpp` ctor field list (`rb3/native/src/rb3_platform_native.cpp:47-66`):** the field list mirrors `PlatformMgr_Wii.cpp:125` by hand. Should we add a build-time assertion that the field count + offsets match (via `static_assert(offsetof(PlatformMgr, mIgnorePowerOperations) == …)`) so a header drift doesn't silently leave fields uninit?

---

*Audit performed read-only; no source files modified.*

---
---

# PART II — Convergence Plan (V12–V33 update, 2026-05-28 evening)

**Date:** 2026-05-28 (read-only convergence-audit subagent, post-V33).
**Scope of this addendum:** the in-song **venue / characters / camera / HUD**
divergence hacks introduced V12–V33 (after Part I was written, 2026-05-28
00:31). Part I above is still accurate for the **boot / audio / song-load**
layer; this part adds the rendering/gameplay-presentation layer and re-checks
the Part I "top 5 hacks" for what has since landed.

## A. What changed since Part I

| Part I item | Status now |
|---|---|
| **Top hack #1** — direct `TheSongMgr.AddSongs` in `rb3_game_input.cpp` | **FIXED / converged.** Replaced by `NativeContentMgr : ContentMgr` in `native/src/rb3_platform_native.cpp:112` whose `StartRefresh()` override reads `songs/songs.dta` → `BandSongMgr::AddSongs` → fires `ContentDone` to registered callbacks. `rb3_game_input.cpp:677` now calls the real `TheContentMgr->StartRefresh()` entry point. The imperative AddSongs hack is gone. This is the model convergence pattern: a glue-layer subclass that runs the real callback chain. |
| **Top hack #2** — Instance.cpp cosmetic-venue deferral + InterstitialPanel short-circuits | **STILL OPEN, deeper-than-thought.** `WorldInstance::SyncDir` deferral now lives at `Instance.cpp:361-375` (gated to `world/vignette/` + `world/shared/` via `IsDeferredVenueProxy`). The V5b investigation (documented inline at `Instance.cpp:326-350`) found the single-line `PostLoadInlined` fix is blocked by a many-to-one shared-dir parent problem; deferred to V2. The InterstitialPanel short-circuits were not re-verified this pass (Part I §Pattern-1 still applies; lower priority than the gameplay-presentation hacks below). |
| **Top hack #5** — native joypad shim | **STILL OPEN.** `rb3_game_input.cpp:327` `pad0->mConnected=true` + `:328` `DebugSetControllerTypeOverride(kControllerGuitar)` + `:314` `JoypadInitCommon` are all still present. |
| **Top hack #3/#4** — stale link stubs, lenient `DataNode::Int/Sym` | Not re-audited this pass; Part I stands. |

## B. The V12–V33 divergence-hack table

All sites are **committed at HEAD `f8a3a379`** as additive `#ifdef HX_NATIVE`
blocks (the permuter wiped them between V32/V33; the commit re-applies the
inseparable set). Classification key: **LB** = legit-bridge (keep until the
data/real path exists), **PO** = paper-over-to-remove (provably redundant or
masking nothing), **DD** = data-driven-fix-available (a real asset/script fix
retires it), **TH** = test-harness, **MF→glue** = movable to permuter-safe glue.

| # | Site (file:line) | What it does | Why it exists (the gap) | Class | Convergence action |
|---|---|---|---|---|---|
| **V19a** | `BandDirector.cpp:524-535` `EnterVenue` | Force-loads `world/venue/.../small_club_01.milo` synchronously when `mVenue.Dir()` is null. | Retail's data-driven `load_venue <sym>` dispatch never fires natively — the only DTA `load_venue` is the editor-only `load_and_play_song` preview. So `mVenue`/`mCurWorld`/`TheBandWardrobe` all stay null. | **LB / DD** | The real fix is to find/wire what dispatches `load_venue` for an actual song-start in retail (a game-mode/data path, not editor). The `HANDLE_ACTION(load_venue, ...)` at `BandDirector.cpp:907` already exists — the gap is the **caller**. Until found, this bridge is load-bearing. |
| **V19b** | `BandDirector.cpp:615` `ReadyForMidiParsers` gate | (gate adjustment for the force-load timing). | Same venue-load gap. | **LB** | Inseparable from V19a. |
| **V19c** | `Env.h:74` `RndEnviron::SetFogEnable` | Adds `return enable;` to a non-void function whose matched body falls off the end. | Matched-fork `SetFogEnable` is `bool` but the PPC body never returns → clang emits `ud2` SIGILL on the venue light-preset path. | **LB (platform)** | A genuine clang-vs-PPC codegen divergence; equivalent to Part I §Pattern-4 `DataNode::Evaluate`. Keep. Could become permuter-safe if the upstream decomp signature is fixed to actually return. |
| **V19d** | `BandDirector.cpp:615-620` `GetVenuePath` strstr const-cast | `const char* str = strstr(...)` vs matched `char*`. | Host `<cstring>` `strstr(const char*)` returns `const char*`. | **LB (platform)** | Pure C++ type-correctness; keep. |
| **V20a** | `BandPatchMesh.cpp:130,518` `#ifndef HX_NATIVE` around `namespace stlpmtx_std` | Excludes the explicit STLport `__introsort_loop`/`__adjust_heap` specializations so `std::sort` falls through to host libstdc++. | clang's libstdc++ has no such symbols → "no function template matches". | **LB (platform)** | Same pattern as `GameGemList.cpp`. Keep. **Interdep:** required by V20b. |
| **V20b** | `native/CMakeLists.txt:~307` un-exclude `BandPatchMesh` | Removes the TU from `_NATIVE_FORK_EXCLUDE` so strong defs exist. | With the TU stubbed, `ObjVector<BandPatchMesh>::resize` constructs/destructs objects whose ObjPtr members were never constructed (no-op `operator>>` stub) → `~ObjPtr → mRefs.rbegin()` SEGV. | **LB** | Durable (CMake is permuter-safe). **Interdep:** needs V20a's gates or the TU won't compile. Document the linkage (SALVAGE_V33 §Phase-2 item 4). |
| **V21** | `Mtx.h:640` `Multiply(Vector3,Matrix3,Vector3)` C++ body | Provides a real C++ body. | The matched-fork PPC `ASM_BLOCK` is empty under clang → garbage `vout` → CharServoBone bone-Y runaway to inf. | **LB (platform)** | Inviolable, like the Part I §4 math fixes. Keep. |
| **V22** | `BandDirector.cpp:317` `DrawShowing` venue-cam-follow | Camera-follow during venue draw. | Camera-ownership conflict from the force-loaded venue. | **LB** | Inseparable from V19/V23. |
| **V23a** | `BandDirector.cpp:558-563` `EnterVenue` LoadCharacters bridge | Calls `TheBandWardrobe->LoadCharacters(mVenue.Name(), ...)` after the force-load. | Retail runs this from `OnFileLoaded(song)`, but natively `mVenue.Name()` is null at that earlier moment (venue deferred to V19a) → char proxies collapse onto a stand-in. | **LB** | Inseparable from V19a (positioned exactly post-force-load, pre-SetVenueDir). |
| **V23b** | `BandDirector.cpp:589-595` `EnterVenue` HarvestDircuts re-run | Re-runs `HarvestDircuts()` once `mVenue.Dir()` is real. | The earlier harvest bailed at its `mPropAnim && mVenue.Dir()` gate (venueDir was nil). | **LB** | Inseparable. Retires automatically once V19a's venue-load timing is data-driven (the first harvest would then succeed). |
| **V23c** | `BandWardrobe.cpp:~694` `LoadMainCharacters` mic→vocals | Adds `if (inst=="mic") inst="vocals";` to the venue-name loop. | Vocalist `mInstrumentType` is `mic` but venue proxies/closeup targets are named `player_vocals0_*` (14681 refs vs 0 `player_mic0`). | **DD** | This is a **data/naming reconciliation** — the 360-ARK venue milo uses `vocals` naming where RB3-Wii code emits `mic`. Could be retired by a name-map driven from data, or accepted as a small platform remap. Low risk; keep. |
| **V23d** | `BandCharacter.cpp:1730` `ReplaceRefs` realloc-safe | Index-based rewrite of the ref-replace loop (re-reads `Refs()` each iter, walks high→low). | Matched-fork caches `std::vector<ObjRef*>` iterators; `ref->Replace()` erases from `mRefs` → libstdc++ realloc invalidates the cached iterator → dangling deref SIGSEGV. | **LB (platform), broad correctness win** | A genuine STL-ABI difference (MWCC vector ≠ libstdc++ realloc behavior). Semantics identical. Keep — this is a correctness fix, not a hack; arguably move to Part I §4 inviolable list. |
| **V25** | `GamePanel.cpp:270-285` `StartGame` HUD force-show | `GetTrackPanelDir()->SetShowing(true)` after `mGameState=kGamePlaying`. | At V25 time the milo `play_intro → PlayIntro() → SetShowing(!mPerformanceMode)` fired late/inconsistently, so `draw_order.grp` could stay hidden. | **PO (provably redundant) / MF→glue** | **HUD_DETERMINISM (V30) proved this redundant**: across 22 runs the milo `play_intro` path reliably shows the HUD at frame ~1130 with V25 absent. **QUICK WIN:** delete it, OR (per SALVAGE_V33 §Phase-2 #2) move to a glue per-frame `Poll` hook edge-detecting `kGameNeedStart→kGamePlaying` so it survives the permuter as an idempotent safety net. Either removes one block from the matched-fork wipe surface. |
| **V26** | `Rot.cpp:485` `MakeRotQuat` half-angle factors | Re-adds the `0.5` half-angle factors. | Matched-fork dropped them → quat √2-too-big → `MakeRotMatrix` det≈8 → CharIKHand flings fingertips → teal shards. | **LB (platform)** | Math correctness fix. Keep / inviolable. |
| **V29** | `BandTrack.cpp:141,445-457` `solo_percent.lbl` seed | `SetTokenFmt(me_percent_format, 0)` so the label renders `0%` not `%d%%`. | The load-time `SetTextToken` path calls `SetTokenFmtImp` with zero args, leaving the `%d%%` format string unsubstituted. | **DD / PO** | Part I §Pattern-4 class: a text-token substitution gap (cf. TEXT_TOKENS.md). The clean fix is to seed the token through the normal label-load path; this imperative seed is a paper-over. Low risk; low priority. |
| **V31** | `TrackPanelDir.cpp:294-349` `ConfigureTracks` `right.grp.x` neutralization | Zeros the `right.grp`/`left.grp` anchor x-offset for single-player so the BandScoreboard renders top-CENTER not top-RIGHT. | The milo `1_player_wide` config object's `apply` handler **executes** (returns `kDataObject`) but its authored `[objects]/[visibles]/[xfms]` save-arrays are **EMPTY** in the loaded asset, so nothing neutralizes the authored multi-player `right.grp` offset. | **DD (real fix available) / LB until then** | **The key convergence target.** Root cause is the empty save-arrays (APPLY_HANDLER V31.2). Three hypotheses: (a) author left them empty by design, (b) wrong milo-asset path, (c) the native binstream `PostLoad` is **dropping** them. If (c), populating them lets the authored script position the HUD and **supersedes V31 entirely**. Gated `RB3_APPLY_HANDLER_FIX_OFF=1`. Keep until the save-array question is resolved. |
| **V12** | `TrackPanelDir.cpp:382-413` `ConfigureTracks` (hide unused pool tracks) + `TrackDir.cpp:208-273` `DrawShowing` (rotater.grp / rotater_roll.grp / screenRect.x camera neutralization) | Two halves: (1) hide unused-pool gem tracks; (2) neutralize the per-player camera fan-out (`rotater.grp.x → camRotX=-4.0`, `rotater_roll.grp → identity`, `mScreenRect.x → 0`) so the lone-player highway renders centered/on-axis. | **Same root cause as V31** — the `apply` handler's `set_track_offset`/`set_side_angle`/`set_screen_rect_x = 0` commands don't execute natively, so each gem track keeps the authored multi-player camera fan-out. | **DD (same fix as V31) / LB until then** | Inseparable from V31 conceptually (both bridge the un-executed `apply` command array). Per HUD_DETERMINISM: once the authored `apply` commands run, **both V12 and V31 become redundant**. **Concern:** `TrackDir.cpp:241` reads `TheNativeSettings().camRotX` — a **glue→matched-fork runtime dependency** baked into a permuter-wiped file; if the permuter wipes TrackDir.cpp the camera fix silently dies. Add `CAMERA_FRAME_FIX_OFF=1` gate (APPLY_HANDLER follow-up #4) for A/B + eventual deletion. |
| **V32** | `CharIKHand.cpp:31` `IK_TGT_DBG` | env-gated IK-target diagnostic. | debugging the V21/V26 IK math. | **TH / diagnostic** | Cost-free (env-gated). Retire post-V1 with the other `*_DBG` blocks. |
| **K9_APPLY** | `TrackPanelDirBase.cpp:89` `K9_APPLY_DBG` | env-gated apply-handler dispatch trace. | localizing the V31 apply gap. | **TH / diagnostic** | Keep until the apply-handler save-array question (V31.2) is resolved — it is the instrument for that fix. Then retire. |

## C. Test-harness leakage check (RB3_GAME_INPUT / nofail / synthetic input)

**Result: the synthetic-input harness does NOT leak into game logic.**
- `RB3GameInputPoll(frame)` is called unconditionally from the native frame
  loop (`App.cpp:525`, inside the `#ifdef HX_NATIVE` block), but it is **inert
  without the `RB3_GAME_INPUT` env var** — `ParseScript()` produces no verbs, so
  no buttons/messages/nofail fire. A normal (env-less) native run is unaffected.
- `nofail` (`rb3_game_input.cpp:443` `ExecNoFail`) calls the **real**
  `MetaPerformer::SetBandNoFail(true)` — the same API the retail no-fail cheat
  uses. It is harness-scheduled but not a game-logic hack.
- `gHxNativeNumUsedGemTracks` (the one global the V12 fix shares between
  `TrackPanelDir.cpp` and `TrackDir.cpp`) is set/read **only inside matched-fork
  HX_NATIVE blocks** — it is part of the V12 camera fix, not the harness, and
  does not leak from the test layer.
- The `select:<button>` / `msg:<object>:<action>` directives (Part I
  §Pattern-7) remain the most-divergent harness pieces but are confined to the
  harness TU; they don't appear in `src/`.

So the only test-harness items needing convergence are the **Part I §Pattern-7**
ones (replace `msg:`/`select:` with real input once the joypad shim lands) — no
new leakage was introduced by V12–V33.

## D. Prioritized cleanup list (convergence value × durability ÷ risk)

### (a) Delete now — provably redundant, zero risk
1. **V25 `GamePanel::StartGame` HUD force-show.** HUD_DETERMINISM proved it
   redundant across 22 runs; the milo `play_intro` path reliably shows the HUD.
   *Either* delete it *or* (preferred, #b below) move it to glue as an
   idempotent safety net. Removes one matched-fork wipe-surface block.

### (b) Move to glue — permuter-safe, low risk
2. **V25 → glue Poll hook** (the alternative to deleting). Edge-detect
   `kGameNeedStart→kGamePlaying` in a per-frame hook in `native/src/` and call
   `GetTrackPanelDir()->SetShowing(true)`. ~10 LOC. Survives permuter wipes;
   keeps the safety net without a matched-fork block. (SALVAGE_V33 §Phase-2 #2.)
3. **Build the durability audit script** (SALVAGE_V33 §Phase-2 #1) —
   `scripts/native/audit_hx_native_blocks.sh` that greps each session doc's
   cited `file:line` blocks and reports INTACT/WIPED/SHIFTED. Not a code hack
   but the highest-leverage durability investment: 8/25 blocks were wiped this
   session and re-discovery is the dominant re-apply cost.

### (c) Needs a real data-driven fix to retire — high convergence value
4. **V31 + V12 (the `apply`-handler gap).** Single highest-value convergence
   target: it retires **three** hacks at once (V31 `right.grp.x`, V12
   `rotater.grp`/`screenRect`, and the implicit camera fan-out). The work:
   trace whether the `1_player_wide` config object's empty
   `[objects]/[visibles]/[xfms]` save-arrays are (c) dropped by the native
   binstream `PostLoad`. If so, fixing the load makes the authored script
   position both HUD and camera, and V12+V31 delete cleanly. Use the existing
   `K9_APPLY_DBG` probe. **Risk:** medium — touches the binstream load path;
   blast radius includes the V19–V23 highway framing. Gate both with
   `*_OFF=1` envs first (V31 already has one; add `CAMERA_FRAME_FIX_OFF=1`).
5. **V19a venue force-load → wire the real `load_venue` caller.** Find what
   dispatches `load_venue` for a real (non-editor) song-start in retail and
   wire that path; the `HANDLE_ACTION(load_venue)` handler already exists. This
   retires V19a/b, V22, V23a/b as a set (they are positioned around the
   force-load). **Risk:** medium-high; **interdep:** the whole V19/V20/V23
   cluster is inseparable (SALVAGE_V33) — do not remove piecemeal.
6. **Instance.cpp cosmetic-venue deferral** (Part I top hack #2 / V5b). The
   proper fix needs a many-to-one shared-dir parent abstraction;
   ~1 week, deferred to V2. Until then the deferral + InterstitialPanel
   short-circuits stay. Lower priority than #4/#5 for V1 (cosmetic backdrops).
7. **V23c mic→vocals + V29 solo_percent + native joypad shim** — smaller
   data/naming reconciliations and the Part I top-hack-5 joypad shim. Retire
   opportunistically.

### (d) Legit — keep / do not regress
- **V19c SetFogEnable, V19d strstr, V20a stlpmtx gates, V21 Multiply, V23d
  ReplaceRefs, V26 MakeRotQuat** — all genuine clang-vs-PPC/MWCC platform
  divergences or STL-ABI correctness fixes. V21/V23d/V26 belong on the Part I §4
  inviolable list. **V20b** (CMake un-exclude) is already permuter-safe.
- The `*_DBG` / `K9_APPLY_DBG` / `IK_TGT_DBG` probes are env-gated and
  cost-free; retire post-V1 (keep K9_APPLY_DBG until #4 lands).

## E. Interdependencies (do not split these)
- **V19 / V20 / V23 are one inseparable set** (SALVAGE_V33): without V20's
  `stlpmtx_std` gates + CMake un-exclude, V19/V23's `LoadCharacters` bridge
  SEGVs in `~ObjPtr → mRefs.rbegin()`. V22/V23a/b are positioned around V19a's
  force-load.
- **V12 and V31 share one root cause** (the un-executed milo `apply` command
  array) but touch **disjoint subtrees** (V12 = camera rig under the gem-track
  proxy; V31 = HUD plate parents under `draw_order.grp`). A real apply-handler
  fix retires both; neither can be removed alone without the other's gap
  resurfacing.
- **V20a ↔ V20b**: the CMake un-exclude requires the matched-fork `stlpmtx_std`
  gates; a future agent re-excluding the TU must keep both in sync.

## F. Durability note (the meta-hack)
The single biggest non-code finding: **8 of 25 matched-fork HX_NATIVE blocks
were silently wiped by the permuter between V32 and V33** and had to be
re-applied (committed at `f8a3a379`). Every `(c)`-class fix that can become
**data-driven** (V31/V12 via the save-arrays; V19 via the real `load_venue`
caller) removes a matched-fork block from the permuter's wipe surface entirely —
that is the highest durability payoff, above moving things to glue. Items that
can't go data-driven should prefer glue (V25) over matched-fork. The audit
script (D#3) makes the remaining matched-fork blocks cheap to re-verify.

*Part II audit performed read-only; no source files modified, nothing
committed/staged.*
