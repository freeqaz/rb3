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
