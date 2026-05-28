// AUTO-GENERATED weak no-op stubs for the boot-path link symbols defined by the
// ~43 band3/{meta_band,game,tour,net_band,bandtrack} + system/{bandobj,char,
// beatmatch,meta,movie} matched-fork TUs that do NOT yet compile under clang
// LP64 (see native/CMakeLists.txt _NATIVE_FORK_EXCLUDE). Those TUs are excluded
// from the rb3-native link; the boot spine (App.cpp) + the compiled boot-path
// TUs still REFERENCE symbols those excluded TUs would define (BandInit,
// MetaPanel::Init, manager singletons/vtables/typeinfo, static members, …).
// Each name is WEAK -> any real definition wins; the rest resolve to a shared
// no-op. Follows the pattern of rndobj_synth_link_stubs.s but splits FUNCTION
// stubs (point at a .text noop; safe to call) from DATA stubs (each gets its own
// writable .bss reservation; safe to read AND write — singletons/globals/static
// members/vtables/typeinfo). None is on the matched-fork asm-match line.
//
// Regenerate: relink with this file's symbol lists empty + -Wl,--no-demangle,
// collect `undefined reference to` names, classify FUNC vs DATA (c++filt: has
// "(" => fn; _ZT[VIS]/static-data => data; MWCC __..F => fn), emit the pairs.
    .text
    .p2align 4
__hmx_band3_noop_stub:
    xorl %eax, %eax
    ret

    // ---- FUNCTION stubs (call -> harmless no-op returning 0) ----
    // NetSession base virtuals: defining a native NetSession::NetSession() ctor
    // (rb3_netsession_native.cpp, to construct the MsgSource base) forces emission
    // of the abstract NetSession vtable, which slots these non-overridden base
    // virtuals. Their bodies live in the un-globbed network/net/NetSession.cpp.
    // All are online session-state entry points, never reached offline; noop.
    // NetSession base non-pure virtuals (AddLocalToSession/AddRemoteToSession/
    // Remove*FromSession/Poll/Handle + Handle's virtual thunk) and the _ZTI10NetSession
    // typeinfo are now STRONGLY defined by rb3_netsession_native.cpp (the glue defines
    // NetSession's key function `Handle` so clang emits the real RTTI + vtable). Their
    // stubs are REMOVED — the real typeinfo is required so BandUI::Init's
    // ObjDirItr<UIScreen> dynamic_cast over the registered `session` object's RTTI chain
    // does not walk a zeroed _ZTI10NetSession and crash.
    .weak AddRedeemedOffer__21StoreRedemptionsTableFPCc
    .set AddRedeemedOffer__21StoreRedemptionsTableFPCc, __hmx_band3_noop_stub
    .weak AnalyzeBlock__13PitchDetectorFPCcPsiffRfRfRf
    .set AnalyzeBlock__13PitchDetectorFPCcPsiffRfRfRf, __hmx_band3_noop_stub
    .weak BinkDoFrame
    .set BinkDoFrame, __hmx_band3_noop_stub
    .weak BinkGetFrameBuffersInfo
    .set BinkGetFrameBuffersInfo, __hmx_band3_noop_stub
    .weak BinkGetSummary
    .set BinkGetSummary, __hmx_band3_noop_stub
    .weak BinkPause
    .set BinkPause, __hmx_band3_noop_stub
    .weak BinkRegisterFrameBuffers
    .set BinkRegisterFrameBuffers, __hmx_band3_noop_stub
    .weak BinkSetSoundOnOff
    .set BinkSetSoundOnOff, __hmx_band3_noop_stub
    .weak BinkShouldSkip
    .set BinkShouldSkip, __hmx_band3_noop_stub
    .weak BinkWait
    .set BinkWait, __hmx_band3_noop_stub
    .weak JoypadSetCalbertMode
    .set JoypadSetCalbertMode, __hmx_band3_noop_stub
    .weak NoteAt__13VocalNoteListCFf
    .set NoteAt__13VocalNoteListCFf, __hmx_band3_noop_stub
    .weak PitchAt__13VocalNoteListCFf
    .set PitchAt__13VocalNoteListCFf, __hmx_band3_noop_stub
    .weak PlayableBy__9VocalNoteCFi
    .set PlayableBy__9VocalNoteCFi, __hmx_band3_noop_stub
    .weak RbnOffer__9StorePageCFi
    .set RbnOffer__9StorePageCFi, __hmx_band3_noop_stub
    .weak _Z12CntSdRsoInitP15RSOObjectHeader
    .set _Z12CntSdRsoInitP15RSOObjectHeader, __hmx_band3_noop_stub
    .weak _Z13SystemPreInitiPPcPKc
    .set _Z13SystemPreInitiPPcPKc, __hmx_band3_noop_stub
    .weak _Z17CntSdRsoTerminatev
    .set _Z17CntSdRsoTerminatev, __hmx_band3_noop_stub
    .weak _Z18SemitoneToWhiteKeyi
    .set _Z18SemitoneToWhiteKeyi, __hmx_band3_noop_stub
    .weak _Z18WhiteKeyToSemitonei
    .set _Z18WhiteKeyToSemitonei, __hmx_band3_noop_stub
    .weak _Z19JoypadWiiOnUserLeftib
    .set _Z19JoypadWiiOnUserLeftib, __hmx_band3_noop_stub
    .weak _Z21JoypadGetCalbertValueib
    .set _Z21JoypadGetCalbertValueib, __hmx_band3_noop_stub
    .weak _Z26GetWiiJoypadDisconnectTypeiP10JoypadTypeS0_
    .set _Z26GetWiiJoypadDisconnectTypeiP10JoypadTypeS0_, __hmx_band3_noop_stub
    .weak _Z8BandInitv
    .set _Z8BandInitv, __hmx_band3_noop_stub
    .weak _Z8PropSyncR13CharacterTestR8DataNodeP9DataArrayi6PropOp
    .set _Z8PropSyncR13CharacterTestR8DataNodeP9DataArrayi6PropOp, __hmx_band3_noop_stub
    .weak _ZN10FileMerger10FindMergerE6Symbolb
    .set _ZN10FileMerger10FindMergerE6Symbolb, __hmx_band3_noop_stub
    .weak _ZN10FileMerger11MergeActionEPN3Hmx6ObjectES2_P9ObjectDir
    .set _ZN10FileMerger11MergeActionEPN3Hmx6ObjectES2_P9ObjectDir, __hmx_band3_noop_stub
    .weak _ZN10FileMerger15FindMergerIndexE6Symbolb
    .set _ZN10FileMerger15FindMergerIndexE6Symbolb, __hmx_band3_noop_stub
    .weak _ZN10FileMerger16LaunchNextLoaderEv
    .set _ZN10FileMerger16LaunchNextLoaderEv, __hmx_band3_noop_stub
    .weak _ZN10FileMerger5ClearEv
    .set _ZN10FileMerger5ClearEv, __hmx_band3_noop_stub
    .weak _ZN10FileMerger6SelectE6SymbolRK8FilePathb
    .set _ZN10FileMerger6SelectE6SymbolRK8FilePathb, __hmx_band3_noop_stub
    .weak _ZN10FileMerger9StartLoadEb
    .set _ZN10FileMerger9StartLoadEb, __hmx_band3_noop_stub
    .weak _ZN10FileMergerC1Ev
    .set _ZN10FileMergerC1Ev, __hmx_band3_noop_stub
    .weak _ZN10GameConfig10RemoveUserEP8BandUser
    .set _ZN10GameConfig10RemoveUserEP8BandUser, __hmx_band3_noop_stub
    .weak _ZN10GameConfig11AssignTrackEP8BandUser
    .set _ZN10GameConfig11AssignTrackEP8BandUser, __hmx_band3_noop_stub
    .weak _ZN10GameConfig12AssignTracksEv
    .set _ZN10GameConfig12AssignTracksEv, __hmx_band3_noop_stub
    .weak _ZN10GameConfig12SyncPropertyER8DataNodeP9DataArrayi6PropOp
    .set _ZN10GameConfig12SyncPropertyER8DataNodeP9DataArrayi6PropOp, __hmx_band3_noop_stub
    .weak _ZN10GameConfig16ChangeDifficultyEP8BandUseri
    .set _ZN10GameConfig16ChangeDifficultyEP8BandUseri, __hmx_band3_noop_stub
    .weak _ZN10GameConfig16ChangeRandomSeedEv
    .set _ZN10GameConfig16ChangeRandomSeedEv, __hmx_band3_noop_stub
    .weak _ZN10GameConfig19GetFxSwitchPositionEP13LocalBandUser
    .set _ZN10GameConfig19GetFxSwitchPositionEP13LocalBandUser, __hmx_band3_noop_stub
    .weak _ZN10GameConfigC1Ev
    .set _ZN10GameConfigC1Ev, __hmx_band3_noop_stub
    .weak _ZN10GameConfigD1Ev
    .set _ZN10GameConfigD1Ev, __hmx_band3_noop_stub
    .weak _ZN10MidiParser4InitEv
    .set _ZN10MidiParser4InitEv, __hmx_band3_noop_stub
    .weak _ZN10MidiParser5ResetEf
    .set _ZN10MidiParser5ResetEf, __hmx_band3_noop_stub
    .weak _ZN10MidiReader13ReadAllTracksEv
    .set _ZN10MidiReader13ReadAllTracksEv, __hmx_band3_noop_stub
    .weak _ZN10MidiReader14ReadSomeEventsEi
    .set _ZN10MidiReader14ReadSomeEventsEi, __hmx_band3_noop_stub
    .weak _ZN10MidiReaderC1ER9BinStreamR12MidiReceiverPKc
    .set _ZN10MidiReaderC1ER9BinStreamR12MidiReceiverPKc, __hmx_band3_noop_stub
    .weak _ZN10MidiReaderD1Ev
    .set _ZN10MidiReaderD1Ev, __hmx_band3_noop_stub
    .weak _ZN10NetSession10DisconnectEv
    .set _ZN10NetSession10DisconnectEv, __hmx_band3_noop_stub
    // _ZN10NetSession12AddLocalUserEP9LocalUser — now strongly defined in
    // rb3_netsession_native.cpp (offline host-path local-user join), so the weak
    // no-op stub is removed (it gated the splash->main_hub overshell join).
    .weak _ZN10NetSession12SendMsgToAllER10NetMessage10PacketType
    .set _ZN10NetSession12SendMsgToAllER10NetMessage10PacketType, __hmx_band3_noop_stub
    .weak _ZN10NetSession14RegisterOnlineEv
    .set _ZN10NetSession14RegisterOnlineEv, __hmx_band3_noop_stub
    .weak _ZN10NetSession14UpdateUserDataEP4Userj
    .set _ZN10NetSession14UpdateUserDataEP4Userj, __hmx_band3_noop_stub
    .weak _ZN10NetSession15RemoveLocalUserEP9LocalUser
    .set _ZN10NetSession15RemoveLocalUserEP9LocalUser, __hmx_band3_noop_stub
    .weak _ZN10NetSession4JoinEP15NetSearchResult
    .set _ZN10NetSession4JoinEP15NetSearchResult, __hmx_band3_noop_stub
    .weak _ZN10NetSession7EndGameEibf
    .set _ZN10NetSession7EndGameEibf, __hmx_band3_noop_stub
    .weak _ZN10NetSession7SendMsgEP4UserR10NetMessage10PacketType
    .set _ZN10NetSession7SendMsgEP4UserR10NetMessage10PacketType, __hmx_band3_noop_stub
    .weak _ZN10NetSession7SendMsgERKSt6vectorIP10RemoteUserSaIS2_EER10NetMessage10PacketType
    .set _ZN10NetSession7SendMsgERKSt6vectorIP10RemoteUserSaIS2_EER10NetMessage10PacketType, __hmx_band3_noop_stub
    .weak _ZN10NetSession9StartGameEv
    .set _ZN10NetSession9StartGameEv, __hmx_band3_noop_stub
    .weak _ZN10ProfileMgr29GetShouldShowWiiFriendsPromptEv
    .set _ZN10ProfileMgr29GetShouldShowWiiFriendsPromptEv, __hmx_band3_noop_stub
    .weak _ZN10TrackPanel19TrackerDisplayResetEv
    .set _ZN10TrackPanel19TrackerDisplayResetEv, __hmx_band3_noop_stub
    .weak _ZN11ClipDistMap4DrawEffP10CharDriver
    .set _ZN11ClipDistMap4DrawEffP10CharDriver, __hmx_band3_noop_stub
    .weak _ZN11ClipDistMap8SetNodesEPNS_4NodeES1_
    .set _ZN11ClipDistMap8SetNodesEPNS_4NodeES1_, __hmx_band3_noop_stub
    .weak _ZN11ClipDistMap9FindDistsEfP9DataArray
    .set _ZN11ClipDistMap9FindDistsEfP9DataArray, __hmx_band3_noop_stub
    .weak _ZN11ClipDistMap9FindNodesEfff
    .set _ZN11ClipDistMap9FindNodesEfff, __hmx_band3_noop_stub
    .weak _ZN11ClipDistMapC1EP8CharClipS1_ffiPK9DataArray
    .set _ZN11ClipDistMapC1EP8CharClipS1_ffiPK9DataArray, __hmx_band3_noop_stub
    // GameGemList stubs REMOVED — GameGemList.cpp is now compiled and strongly defines all of these.
    // GemTrackDir method stubs REMOVED — GemTrackDir.cpp is now compiled
    // (un-excluded) and strongly defines all of these; the strong defs win.
    .weak _ZN11PlatformMgr14StartProfanityEPPKtiPcPN3Hmx6ObjectE
    .set _ZN11PlatformMgr14StartProfanityEPPKtiPcPN3Hmx6ObjectE, __hmx_band3_noop_stub
    .weak _ZN11PlatformMgr15SetUserSignedInEi
    .set _ZN11PlatformMgr15SetUserSignedInEi, __hmx_band3_noop_stub
    .weak _ZN11PlatformMgr16EnumerateFriendsEiRSt6vectorIP6FriendSaIS2_EEPN3Hmx6ObjectE
    .set _ZN11PlatformMgr16EnumerateFriendsEiRSt6vectorIP6FriendSaIS2_EEPN3Hmx6ObjectE, __hmx_band3_noop_stub
    .weak _ZN11PlatformMgr16SetUserSignedOutEi
    .set _ZN11PlatformMgr16SetUserSignedOutEi, __hmx_band3_noop_stub
    .weak _ZN11PlatformMgr23RegisterSendMsgCallbackEPFbP6FriendPKcS3_R9MemStreamE
    .set _ZN11PlatformMgr23RegisterSendMsgCallbackEPFbP6FriendPKcS3_R9MemStreamE, __hmx_band3_noop_stub
    .weak _ZN11PlatformMgr25RegisterSignInserCallbackEPFbP4UsermE
    .set _ZN11PlatformMgr25RegisterSignInserCallbackEPFbP4UsermE, __hmx_band3_noop_stub
    .weak _ZN11PlatformMgr32RegisterEnumerateFriendsCallbackEPFbiRSt6vectorIP6FriendSaIS2_EEPN3Hmx6ObjectEE
    .set _ZN11PlatformMgr32RegisterEnumerateFriendsCallbackEPFbiRSt6vectorIP6FriendSaIS2_EEPN3Hmx6ObjectEE, __hmx_band3_noop_stub
    .weak _ZN11PlatformMgr4DrawEv
    .set _ZN11PlatformMgr4DrawEv, __hmx_band3_noop_stub
    .weak _ZN11RockCentral10GetArtFileE6StringP6RndTexPjPN3Hmx6ObjectEi
    .set _ZN11RockCentral10GetArtFileE6StringP6RndTexPjPN3Hmx6ObjectEi, __hmx_band3_noop_stub
    .weak _ZN11RockCentral13EncodeMessageE15_WiiMessageTypejPKc
    .set _ZN11RockCentral13EncodeMessageE15_WiiMessageTypejPKc, __hmx_band3_noop_stub
    .weak _ZN11RockCentral14SaveBinaryDataEP6RndTexR6StringPN3Hmx6ObjectEi
    .set _ZN11RockCentral14SaveBinaryDataEP6RndTexR6StringPN3Hmx6ObjectEi, __hmx_band3_noop_stub
    .weak _ZN11RockCentral16UpdateFriendListEP7ProfileSt6vectorIP6FriendSaIS4_EER14DataResultListPN3Hmx6ObjectE
    .set _ZN11RockCentral16UpdateFriendListEP7ProfileSt6vectorIP6FriendSaIS4_EER14DataResultListPN3Hmx6ObjectE, __hmx_band3_noop_stub
    .weak _ZN11RockCentral5OnMsgERK24WiiFriendsListChangedMsg
    .set _ZN11RockCentral5OnMsgERK24WiiFriendsListChangedMsg, __hmx_band3_noop_stub
    .weak _ZN11RockCentral5OnMsgERK25WiiFriendMgrOpCompleteMsg
    .set _ZN11RockCentral5OnMsgERK25WiiFriendMgrOpCompleteMsg, __hmx_band3_noop_stub
    .weak _ZN11SetlistSort16BuildSetlistListEv
    .set _ZN11SetlistSort16BuildSetlistListEv, __hmx_band3_noop_stub
    .weak _ZN11SetlistSort16BuildSetlistTreeERSt3mapI6Symbol13SetlistRecordSt4lessIS1_ESaISt4pairIKS1_S2_EEE
    .set _ZN11SetlistSort16BuildSetlistTreeERSt3mapI6Symbol13SetlistRecordSt4lessIS1_ESaISt4pairIKS1_S2_EEE, __hmx_band3_noop_stub
    .weak _ZN11SingerStats17SetPartPercentageEif
    .set _ZN11SingerStats17SetPartPercentageEif, __hmx_band3_noop_stub
    .weak _ZN11SingerStats21SetPitchDeviationInfoEff
    .set _ZN11SingerStats21SetPitchDeviationInfoEff, __hmx_band3_noop_stub
    .weak _ZN11UIListState8ProviderEv
    .set _ZN11UIListState8ProviderEv, __hmx_band3_noop_stub
    .weak _ZN11VocalPhraseC1Ev
    .set _ZN11VocalPhraseC1Ev, __hmx_band3_noop_stub
    // VocalPlayer non-const stubs REMOVED — VocalPlayer.cpp now compiled
    // (K8 blocker #3); strong defs win.
    .weak _ZN12BandDirector14HarvestDircutsEv
    .set _ZN12BandDirector14HarvestDircutsEv, __hmx_band3_noop_stub
    .weak _ZN12BandDirector19ReadyForMidiParsersEv
    .set _ZN12BandDirector19ReadyForMidiParsersEv, __hmx_band3_noop_stub
    .weak _ZN12BudgetScreenC1Ev
    .set _ZN12BudgetScreenC1Ev, __hmx_band3_noop_stub
    .weak _ZN12MidiReceiver16SkipCurrentTrackEv
    .set _ZN12MidiReceiver16SkipCurrentTrackEv, __hmx_band3_noop_stub
    // MusicLibrary stubs REMOVED — MusicLibrary.cpp is now compiled and strongly defines all of these.
    // Kept 3 stubs below that still resolve as W (no strong def yet):
    .weak _ZN12MusicLibrary11SetlistSizeEv
    .set _ZN12MusicLibrary11SetlistSizeEv, __hmx_band3_noop_stub
    .weak _ZN12MusicLibrary17GetMaxSetlistSizeEv
    .set _ZN12MusicLibrary17GetMaxSetlistSizeEv, __hmx_band3_noop_stub
    .weak _ZN12MusicLibrary20CanHeadersBeSelectedEv
    .set _ZN12MusicLibrary20CanHeadersBeSelectedEv, __hmx_band3_noop_stub
    // OutfitConfig stubs REMOVED — OutfitConfig.cpp is now compiled and strongly defines all of these.
    .weak _ZN12SavedSetlist14SetDescriptionEPKc
    .set _ZN12SavedSetlist14SetDescriptionEPKc, __hmx_band3_noop_stub
    .weak _ZN12SavedSetlist8SetTitleEPKc
    .set _ZN12SavedSetlist8SetTitleEPKc, __hmx_band3_noop_stub
    // TourProgress stubs REMOVED — TourProgress.cpp is now compiled and strongly defines all of these.
    .weak _ZN12VoiceChatMgr16ToggleMuteStatusEP4User
    .set _ZN12VoiceChatMgr16ToggleMuteStatusEP4User, __hmx_band3_noop_stub
    .weak _ZN12WiiFriendMgr16EnumerateFriendsEP13WiiFriendListPN3Hmx6ObjectE
    .set _ZN12WiiFriendMgr16EnumerateFriendsEP13WiiFriendListPN3Hmx6ObjectE, __hmx_band3_noop_stub
    .weak _ZN12WiiFriendMgr16GetCachedFriendsEP13WiiFriendList
    .set _ZN12WiiFriendMgr16GetCachedFriendsEP13WiiFriendList, __hmx_band3_noop_stub
    .weak _ZN12WiiFriendMgr16SetProfileStatusEi6String
    .set _ZN12WiiFriendMgr16SetProfileStatusEi6String, __hmx_band3_noop_stub
    .weak _ZN12WiiFriendMgr17UseConsoleFriendsEb
    .set _ZN12WiiFriendMgr17UseConsoleFriendsEb, __hmx_band3_noop_stub
    .weak _ZN12WiiFriendMgr22SetMasterProfileStatusE6String
    .set _ZN12WiiFriendMgr22SetMasterProfileStatusE6String, __hmx_band3_noop_stub
    .weak _ZN12WiiMessenger11SendMessageEiPKcS1_PN3Hmx6ObjectEi
    .set _ZN12WiiMessenger11SendMessageEiPKcS1_PN3Hmx6ObjectEi, __hmx_band3_noop_stub
    .weak _ZN12WiiMessenger17EnumerateMessagesEP14WiiMessageListPN3Hmx6ObjectE
    .set _ZN12WiiMessenger17EnumerateMessagesEP14WiiMessageListPN3Hmx6ObjectE, __hmx_band3_noop_stub
    .weak _ZN13BandCharacter15NameToDrumVenueEPKc
    .set _ZN13BandCharacter15NameToDrumVenueEPKc, __hmx_band3_noop_stub
    .weak _ZN13BandPatchMesh13ConstructQuadEP6RndTex
    .set _ZN13BandPatchMesh13ConstructQuadEP6RndTex, __hmx_band3_noop_stub
    .weak _ZN13CharacterTest4DrawEv
    .set _ZN13CharacterTest4DrawEv, __hmx_band3_noop_stub
    .weak _ZN13CharacterTest4LoadER9BinStream
    .set _ZN13CharacterTest4LoadER9BinStream, __hmx_band3_noop_stub
    .weak _ZN13CharacterTest4PollEv
    .set _ZN13CharacterTest4PollEv, __hmx_band3_noop_stub
    .weak _ZN13CharacterTestC1EP9Character
    .set _ZN13CharacterTestC1EP9Character, __hmx_band3_noop_stub
    // _ZN13CharForeTwistC1Ev REMOVED — CharForeTwist.cpp now compiled (real ctor).
    .weak _ZN13JsonConverter6NewIntEi
    .set _ZN13JsonConverter6NewIntEi, __hmx_band3_noop_stub
    .weak _ZN13JsonConverter8NewArrayEv
    .set _ZN13JsonConverter8NewArrayEv, __hmx_band3_noop_stub
    .weak _ZN13JsonConverter9NewDoubleEd
    .set _ZN13JsonConverter9NewDoubleEd, __hmx_band3_noop_stub
    .weak _ZN13JsonConverter9NewStringEPKc
    .set _ZN13JsonConverter9NewStringEPKc, __hmx_band3_noop_stub
    .weak _ZN13JsonConverterC1Ev
    .set _ZN13JsonConverterC1Ev, __hmx_band3_noop_stub
    .weak _ZN13JsonConverterD1Ev
    .set _ZN13JsonConverterD1Ev, __hmx_band3_noop_stub
    .weak _ZN13MetaPerformer10SetSetlistEPK12SavedSetlist
    .set _ZN13MetaPerformer10SetSetlistEPK12SavedSetlist, __hmx_band3_noop_stub
    .weak _ZN13MetaPerformer14LockBandOrSoloEv
    .set _ZN13MetaPerformer14LockBandOrSoloEv, __hmx_band3_noop_stub
    .weak _ZN13MetaPerformer16UnlockBandOrSoloEv
    .set _ZN13MetaPerformer16UnlockBandOrSoloEv, __hmx_band3_noop_stub
    .weak _ZN13MetaPerformer17SetCreditsPendingEv
    .set _ZN13MetaPerformer17SetCreditsPendingEv, __hmx_band3_noop_stub
    .weak _ZN13MetaPerformer19GetScoreTypeForUserEP8BandUser
    .set _ZN13MetaPerformer19GetScoreTypeForUserEP8BandUser, __hmx_band3_noop_stub
    .weak _ZN13MetaPerformer7CurrentEv
    .set _ZN13MetaPerformer7CurrentEv, __hmx_band3_noop_stub
    .weak _ZN13MetaPerformer8SetSongsERKSt6vectorI6SymbolSaIS1_EE
    .set _ZN13MetaPerformer8SetSongsERKSt6vectorI6SymbolSaIS1_EE, __hmx_band3_noop_stub
    .weak _ZN13MetaPerformer8SetSongsERKSt6vectorIiSaIiEE
    .set _ZN13MetaPerformer8SetSongsERKSt6vectorIiSaIiEE, __hmx_band3_noop_stub
    .weak _ZN13MetaPerformer9SetBattleEPK18BattleSavedSetlist
    .set _ZN13MetaPerformer9SetBattleEPK18BattleSavedSetlist, __hmx_band3_noop_stub
    .weak _ZN13MidiParserMgr10FinishLoadEv
    .set _ZN13MidiParserMgr10FinishLoadEv, __hmx_band3_noop_stub
    .weak _ZN13MidiParserMgr13GetEventsListEv
    .set _ZN13MidiParserMgr13GetEventsListEv, __hmx_band3_noop_stub
    .weak _ZN13MidiParserMgr4PollEv
    .set _ZN13MidiParserMgr4PollEv, __hmx_band3_noop_stub
    .weak _ZN13MidiParserMgr5ResetEi
    .set _ZN13MidiParserMgr5ResetEi, __hmx_band3_noop_stub
    .weak _ZN13MidiParserMgr5ResetEv
    .set _ZN13MidiParserMgr5ResetEv, __hmx_band3_noop_stub
    .weak _ZN13MidiParserMgr9GetParserE6Symbol
    .set _ZN13MidiParserMgr9GetParserE6Symbol, __hmx_band3_noop_stub
    .weak _ZN13MidiParserMgrC1EP16GemListInterface6Symbol
    .set _ZN13MidiParserMgrC1EP16GemListInterface6Symbol, __hmx_band3_noop_stub
    .weak _ZN13PatchRenderer13InitResourcesEv
    .set _ZN13PatchRenderer13InitResourcesEv, __hmx_band3_noop_stub
    .weak _ZN13PitchDetectorC1Ei
    .set _ZN13PitchDetectorC1Ei, __hmx_band3_noop_stub
    .weak _ZN13PitchDetectorD1Ev
    .set _ZN13PitchDetectorD1Ev, __hmx_band3_noop_stub
    // VocalNoteList non-const stubs REMOVED — VocalNoteList.cpp now compiled
    // (K8 blocker #3); strong defs win.
    .weak _ZN13WiiContentMgr15UnmountContentsE6Symbol
    .set _ZN13WiiContentMgr15UnmountContentsE6Symbol, __hmx_band3_noop_stub
    .weak _ZN13WiiContentMgr9ContentOfE6Symbol
    .set _ZN13WiiContentMgr9ContentOfE6Symbol, __hmx_band3_noop_stub
    .weak _ZN13WiiFriendListC1Ev
    .set _ZN13WiiFriendListC1Ev, __hmx_band3_noop_stub
    .weak _ZN13WiiFriendListD1Ev
    .set _ZN13WiiFriendListD1Ev, __hmx_band3_noop_stub
    .weak _ZN14DataResultList5ClearEv
    .set _ZN14DataResultList5ClearEv, __hmx_band3_noop_stub
    .weak _ZN14DataResultList5PrintER10TextStream
    .set _ZN14DataResultList5PrintER10TextStream, __hmx_band3_noop_stub
    .weak _ZN14DataResultList6UpdateEP7Message
    .set _ZN14DataResultList6UpdateEP7Message, __hmx_band3_noop_stub
    .weak _ZN14DataResultListC1Ev
    .set _ZN14DataResultListC1Ev, __hmx_band3_noop_stub
    .weak _ZN14DataResultListD1Ev
    .set _ZN14DataResultListD1Ev, __hmx_band3_noop_stub
    .weak _ZN14ProfilePictureC1EiPN3Hmx6ObjectE
    .set _ZN14ProfilePictureC1EiPN3Hmx6ObjectE, __hmx_band3_noop_stub
    .weak _ZN14SearchSettingsC1Eibi
    .set _ZN14SearchSettingsC1Eibi, __hmx_band3_noop_stub
    // Synchronizable ctor/dtor/SetSyncDirty stubs REMOVED — network/net/Synchronize.cpp
    // is now compiled into rb3-native (online side-effects HX_NATIVE-gated); strong
    // defs provide a valid base vtable/typeinfo for the OvershellPanel MI dynamic_cast.
    .weak _ZN14WiiCommerceMgr12InitCommerceEPN3Hmx6ObjectE
    .set _ZN14WiiCommerceMgr12InitCommerceEPN3Hmx6ObjectE, __hmx_band3_noop_stub
    .weak _ZN14WiiCommerceMgr12SpecifyOfferEP17StorePurchaseable
    .set _ZN14WiiCommerceMgr12SpecifyOfferEP17StorePurchaseable, __hmx_band3_noop_stub
    .weak _ZN14WiiCommerceMgr15DestroyCommerceEv
    .set _ZN14WiiCommerceMgr15DestroyCommerceEv, __hmx_band3_noop_stub
    .weak _ZN14WiiCommerceMgr15InitPreDownloadEv
    .set _ZN14WiiCommerceMgr15InitPreDownloadEv, __hmx_band3_noop_stub
    .weak _ZN14WiiCommerceMgr15MakeDataTitleIdEPKc
    .set _ZN14WiiCommerceMgr15MakeDataTitleIdEPKc, __hmx_band3_noop_stub
    .weak _ZN14WiiCommerceMgr19SpecifyContentUnitsERKSt6vectorItSaItEE
    .set _ZN14WiiCommerceMgr19SpecifyContentUnitsERKSt6vectorItSaItEE, __hmx_band3_noop_stub
    .weak _ZN14WiiMessageListC1Ev
    .set _ZN14WiiMessageListC1Ev, __hmx_band3_noop_stub
    .weak _ZN14WiiMessageListD1Ev
    .set _ZN14WiiMessageListD1Ev, __hmx_band3_noop_stub
    // BandNetGameData ctor + Poll + virtuals are now strongly defined in
    // rb3_netsession_native.cpp (there is no BandNetGameData.cpp in the decomp);
    // the weak no-op ctor left a garbage vtable that crashed SessionMgr::Handle's
    // HANDLE_MEMBER_PTR(mBandNetGameData) on the local-user-join result message.
    .weak _ZN15DiscErrorMgrWii16RegisterCallbackEPNS_8CallbackE
    .set _ZN15DiscErrorMgrWii16RegisterCallbackEPNS_8CallbackE, __hmx_band3_noop_stub
    .weak _ZN15DiscErrorMgrWii18UnregisterCallbackEPNS_8CallbackE
    .set _ZN15DiscErrorMgrWii18UnregisterCallbackEPNS_8CallbackE, __hmx_band3_noop_stub
    .weak _ZN15SaveLoadManager11AutoSaveNowEv
    .set _ZN15SaveLoadManager11AutoSaveNowEv, __hmx_band3_noop_stub
    .weak _ZN15SaveLoadManager4InitEv
    .set _ZN15SaveLoadManager4InitEv, __hmx_band3_noop_stub
    .weak _ZN15SaveLoadManager8AutoSaveEv
    .set _ZN15SaveLoadManager8AutoSaveEv, __hmx_band3_noop_stub
    .weak _ZN15SessionSearcher13GetNextResultEv
    .set _ZN15SessionSearcher13GetNextResultEv, __hmx_band3_noop_stub
    .weak _ZN15SessionSearcher16GetSearchResultsERSt6vectorIP15NetSearchResultSaIS2_EE
    .set _ZN15SessionSearcher16GetSearchResultsERSt6vectorIP15NetSearchResultSaIS2_EE, __hmx_band3_noop_stub
    .weak _ZN15SessionSettings9SetPublicEb
    .set _ZN15SessionSettings9SetPublicEb, __hmx_band3_noop_stub
    .weak _ZN15VirtualKeyboard17IsKeyboardShowingEv
    .set _ZN15VirtualKeyboard17IsKeyboardShowingEv, __hmx_band3_noop_stub
    .weak _ZN15WaitingUserGate4InitEv
    .set _ZN15WaitingUserGate4InitEv, __hmx_band3_noop_stub
    .weak _ZN15WaitingUserGate4PollEv
    .set _ZN15WaitingUserGate4PollEv, __hmx_band3_noop_stub
    .weak _ZN15WaitingUserGateC1Ev
    .set _ZN15WaitingUserGateC1Ev, __hmx_band3_noop_stub
    .weak _ZN17NetMessageFactory18RegisterNetMessageE6StringPFP10NetMessagevE
    .set _ZN17NetMessageFactory18RegisterNetMessageE6StringPFP10NetMessagevE, __hmx_band3_noop_stub
    .weak _ZN18TourPerformerLocal17ClearCurrentQuestEv
    .set _ZN18TourPerformerLocal17ClearCurrentQuestEv, __hmx_band3_noop_stub
    .weak _ZN18TourPerformerLocal23ClearCurrentQuestFilterEv
    .set _ZN18TourPerformerLocal23ClearCurrentQuestFilterEv, __hmx_band3_noop_stub
    .weak _ZN18TourPerformerLocalC1ER11BandUserMgr
    .set _ZN18TourPerformerLocalC1ER11BandUserMgr, __hmx_band3_noop_stub
    .weak _ZN19MatchmakingSettings19ClearCustomSettingsEv
    .set _ZN19MatchmakingSettings19ClearCustomSettingsEv, __hmx_band3_noop_stub
    .weak _ZN20MovieInternalBuffers3NewESt6vectorIP4BINKSaIS2_EE
    .set _ZN20MovieInternalBuffers3NewESt6vectorIP4BINKSaIS2_EE, __hmx_band3_noop_stub
    .weak _ZN20MovieInternalBuffersD1Ev
    .set _ZN20MovieInternalBuffersD1Ev, __hmx_band3_noop_stub
    .weak _ZN20StoreMetadataManager14GetOfferStatusEPK20StorePackedOfferBase
    .set _ZN20StoreMetadataManager14GetOfferStatusEPK20StorePackedOfferBase, __hmx_band3_noop_stub
    .weak _ZN20StoreMetadataManager14SongStateFlagsEPK15StorePackedSong
    .set _ZN20StoreMetadataManager14SongStateFlagsEPK15StorePackedSong, __hmx_band3_noop_stub
    .weak _ZN20StoreMetadataManager15AddSetlistOfferEi
    .set _ZN20StoreMetadataManager15AddSetlistOfferEi, __hmx_band3_noop_stub
    .weak _ZN20StoreMetadataManager15LoadDynamicPageEP9DataArray
    .set _ZN20StoreMetadataManager15LoadDynamicPageEP9DataArray, __hmx_band3_noop_stub
    .weak _ZN20StoreMetadataManager18ClearSetlistOffersEv
    .set _ZN20StoreMetadataManager18ClearSetlistOffersEv, __hmx_band3_noop_stub
    .weak _ZN20StoreMetadataManager20GetContentStateFlagsEyt
    .set _ZN20StoreMetadataManager20GetContentStateFlagsEyt, __hmx_band3_noop_stub
    .weak _ZN20StoreMetadataManager20UpdateOfferOwnershipEv
    .set _ZN20StoreMetadataManager20UpdateOfferOwnershipEv, __hmx_band3_noop_stub
    .weak _ZN20StoreMetadataManager4LoadEPKc
    .set _ZN20StoreMetadataManager4LoadEPKc, __hmx_band3_noop_stub
    .weak _ZN20StoreMetadataManager6UnloadEv
    .set _ZN20StoreMetadataManager6UnloadEv, __hmx_band3_noop_stub
    .weak _ZN20StoreMetadataManager8LoadPageEt
    .set _ZN20StoreMetadataManager8LoadPageEt, __hmx_band3_noop_stub
    // AccomplishmentManager stubs REMOVED — AccomplishmentManager.cpp is now compiled and strongly defines all of these.
    .weak _ZN22MainHubMessageProvider13AddTickerDataE14TickerDataTypeiibb
    .set _ZN22MainHubMessageProvider13AddTickerDataE14TickerDataTypeiibb, __hmx_band3_noop_stub
    .weak _ZN22MainHubMessageProvider15AddUnlinkedMotdEPKc
    .set _ZN22MainHubMessageProvider15AddUnlinkedMotdEPKc, __hmx_band3_noop_stub
    .weak _ZN22MainHubMessageProvider17IsTickerDataValidE14TickerDataType
    .set _ZN22MainHubMessageProvider17IsTickerDataValidE14TickerDataType, __hmx_band3_noop_stub
    .weak _ZN22MainHubMessageProvider9ClearDataEv
    .set _ZN22MainHubMessageProvider9ClearDataEv, __hmx_band3_noop_stub
    .weak _ZN22MainHubMessageProviderC1EP12MainHubPanel
    .set _ZN22MainHubMessageProviderC1EP12MainHubPanel, __hmx_band3_noop_stub
    // OvershellProfileProvider stubs REMOVED — OvershellSlot.cpp now provides a
    // native HX_NATIVE impl (ctor/dtor/virtuals + Wii-profile method no-ops);
    // strong defs win, giving the object a real vtable for the set_provider cast.
    .weak _ZN33AccomplishmentDiscSongConditionalC2EP9DataArrayi
    .set _ZN33AccomplishmentDiscSongConditionalC2EP9DataArrayi, __hmx_band3_noop_stub
    .weak _ZN33AccomplishmentDiscSongConditionalD2Ev
    .set _ZN33AccomplishmentDiscSongConditionalD2Ev, __hmx_band3_noop_stub
    .weak _ZN4Band10ForceStarsEi
    .set _ZN4Band10ForceStarsEi, __hmx_band3_noop_stub
    .weak _ZN4Band10RemoveUserEP8BandUser
    .set _ZN4Band10RemoveUserEP8BandUser, __hmx_band3_noop_stub
    .weak _ZN4Band11SetGameOverEv
    .set _ZN4Band11SetGameOverEv, __hmx_band3_noop_stub
    .weak _ZN4Band13LocalBlowCodaEP6Player
    .set _ZN4Band13LocalBlowCodaEP6Player, __hmx_band3_noop_stub
    .weak _ZN4Band15DealWithCodaGemEP6Playeribb
    .set _ZN4Band15DealWithCodaGemEP6Playeribb, __hmx_band3_noop_stub
    .weak _ZN4Band16DeployBandEnergyEP8BandUser
    .set _ZN4Band16DeployBandEnergyEP8BandUser, __hmx_band3_noop_stub
    .weak _ZN4Band16GetActivePlayersEv
    .set _ZN4Band16GetActivePlayersEv, __hmx_band3_noop_stub
    .weak _ZN4Band16UpdateBonusLevelEf
    .set _ZN4Band16UpdateBonusLevelEf, __hmx_band3_noop_stub
    .weak _ZN4Band17LocalFinishedCodaEP6Player
    .set _ZN4Band17LocalFinishedCodaEP6Player, __hmx_band3_noop_stub
    .weak _ZN4Band18AddUserDynamicallyEP8BandUser
    .set _ZN4Band18AddUserDynamicallyEP8BandUser, __hmx_band3_noop_stub
    .weak _ZN4Band19SetAccumulatedScoreEi
    .set _ZN4Band19SetAccumulatedScoreEi, __hmx_band3_noop_stub
    .weak _ZN4Band20AddPlayerDynamicallyEP10BeatMasterP8BandUser
    .set _ZN4Band20AddPlayerDynamicallyEP10BeatMasterP8BandUser, __hmx_band3_noop_stub
    .weak _ZN4Band4PollEfR7SongPos
    .set _ZN4Band4PollEfR7SongPos, __hmx_band3_noop_stub
    .weak _ZN4Band7RestartEb
    .set _ZN4Band7RestartEb, __hmx_band3_noop_stub
    .weak _ZN4BandC1EbiP8BandUserP10BeatMaster
    .set _ZN4BandC1EbiP8BandUserP10BeatMaster, __hmx_band3_noop_stub
    .weak _ZN5Movie4Impl12PlatformInitEv
    .set _ZN5Movie4Impl12PlatformInitEv, __hmx_band3_noop_stub
    .weak _ZN5Movie4Impl17PlatformCacheFileEPKc
    .set _ZN5Movie4Impl17PlatformCacheFileEPKc, __hmx_band3_noop_stub
    .weak _ZN5Movie4Impl4DrawEv
    .set _ZN5Movie4Impl4DrawEv, __hmx_band3_noop_stub
    .weak _ZN5Stats10AddSustainEf
    .set _ZN5Stats10AddSustainEf, __hmx_band3_noop_stub
    .weak _ZN5Stats10StreakInfoC1Ev
    .set _ZN5Stats10StreakInfoC1Ev, __hmx_band3_noop_stub
    .weak _ZN5Stats11AddAccuracyEi
    .set _ZN5Stats11AddAccuracyEi, __hmx_band3_noop_stub
    .weak _ZN5Stats11SectionInfoC1Ev
    .set _ZN5Stats11SectionInfoC1Ev, __hmx_band3_noop_stub
    .weak _ZN5Stats12AddOverdriveEf
    .set _ZN5Stats12AddOverdriveEf, __hmx_band3_noop_stub
    .weak _ZN5Stats12EndHitStreakEv
    .set _ZN5Stats12EndHitStreakEv, __hmx_band3_noop_stub
    .weak _ZN5Stats12SetFinalizedEb
    .set _ZN5Stats12SetFinalizedEb, __hmx_band3_noop_stub
    .weak _ZN5Stats13AddCodaPointsEi
    .set _ZN5Stats13AddCodaPointsEi, __hmx_band3_noop_stub
    .weak _ZN5Stats13EndMissStreakEv
    .set _ZN5Stats13EndMissStreakEv, __hmx_band3_noop_stub
    .weak _ZN5Stats14AddScoreStreakEf
    .set _ZN5Stats14AddScoreStreakEf, __hmx_band3_noop_stub
    .weak _ZN5Stats14BuildHitStreakEif
    .set _ZN5Stats14BuildHitStreakEif, __hmx_band3_noop_stub
    .weak _ZN5Stats14LoadForEndGameER9BinStream
    .set _ZN5Stats14LoadForEndGameER9BinStream, __hmx_band3_noop_stub
    .weak _ZN5Stats14MultiplierInfoC1Ev
    .set _ZN5Stats14MultiplierInfoC1Ev, __hmx_band3_noop_stub
    .weak _ZN5Stats14SetHopoGemInfoEiii
    .set _ZN5Stats14SetHopoGemInfoEiii, __hmx_band3_noop_stub
    .weak _ZN5Stats14SetSectionInfoEi6Symbolff
    .set _ZN5Stats14SetSectionInfoEi6Symbolff, __hmx_band3_noop_stub
    .weak _ZN5Stats14UpdateBestSoloEi
    .set _ZN5Stats14UpdateBestSoloEi, __hmx_band3_noop_stub
    .weak _ZN5Stats15AddFailurePointEf
    .set _ZN5Stats15AddFailurePointEf, __hmx_band3_noop_stub
    .weak _ZN5Stats15AddToTimesSavedEff
    .set _ZN5Stats15AddToTimesSavedEff, __hmx_band3_noop_stub
    .weak _ZN5Stats15BuildMissStreakEi
    .set _ZN5Stats15BuildMissStreakEi, __hmx_band3_noop_stub
    .weak _ZN5Stats15DeployOverdriveEfi
    .set _ZN5Stats15DeployOverdriveEfi, __hmx_band3_noop_stub
    .weak _ZN5Stats16SetCurrentStreakEi
    .set _ZN5Stats16SetCurrentStreakEi, __hmx_band3_noop_stub
    .weak _ZN5Stats16SetCymbalGemInfoEiii
    .set _ZN5Stats16SetCymbalGemInfoEiii, __hmx_band3_noop_stub
    .weak _ZN5Stats17AddToPlayersSavedEif
    .set _ZN5Stats17AddToPlayersSavedEif, __hmx_band3_noop_stub
    .weak _ZN5Stats17SetNoScorePercentEf
    .set _ZN5Stats17SetNoScorePercentEf, __hmx_band3_noop_stub
    .weak _ZN5Stats18IncrementTrillsHitEb
    .set _ZN5Stats18IncrementTrillsHitEb, __hmx_band3_noop_stub
    .weak _ZN5Stats19AddBandContributionEf
    .set _ZN5Stats19AddBandContributionEf, __hmx_band3_noop_stub
    .weak _ZN5Stats19EndStreakMultiplierEfi
    .set _ZN5Stats19EndStreakMultiplierEfi, __hmx_band3_noop_stub
    .weak _ZN5Stats19SetPersistentStreakEi
    .set _ZN5Stats19SetPersistentStreakEi, __hmx_band3_noop_stub
    .weak _ZN5Stats21BeginStreakMultiplierEfi
    .set _ZN5Stats21BeginStreakMultiplierEfi, __hmx_band3_noop_stub
    .weak _ZN5Stats22StopDeployingOverdriveEfi
    .set _ZN5Stats22StopDeployingOverdriveEfi, __hmx_band3_noop_stub
    .weak _ZN5Stats23IncrementSustainGemsHitEb
    .set _ZN5Stats23IncrementSustainGemsHitEb, __hmx_band3_noop_stub
    .weak _ZN5Stats24IncrementHighFretGemsHitEb
    .set _ZN5Stats24IncrementHighFretGemsHitEb, __hmx_band3_noop_stub
    .weak _ZN5Stats27SetVocalSingerAndPartCountsEii
    .set _ZN5Stats27SetVocalSingerAndPartCountsEii, __hmx_band3_noop_stub
    .weak _ZN5Stats29SetSoloButtonedSoloPercentageEi
    .set _ZN5Stats29SetSoloButtonedSoloPercentageEi, __hmx_band3_noop_stub
    .weak _ZN5Stats7AddRollEb
    .set _ZN5Stats7AddRollEb, __hmx_band3_noop_stub
    .weak _ZN5Stats7AddSoloEi
    .set _ZN5Stats7AddSoloEi, __hmx_band3_noop_stub
    .weak _ZN5StatsC1Ev
    .set _ZN5StatsC1Ev, __hmx_band3_noop_stub
    .weak _ZN5WiiFX5SetFXEii
    .set _ZN5WiiFX5SetFXEii, __hmx_band3_noop_stub
    .weak _ZN5WiiFX8IsReverbEi
    .set _ZN5WiiFX8IsReverbEi, __hmx_band3_noop_stub
    .weak _ZN5WiiFX9SetReverbEib
    .set _ZN5WiiFX9SetReverbEib, __hmx_band3_noop_stub
    .weak _ZN6Quazal10RootObjectdlEPv
    .set _ZN6Quazal10RootObjectdlEPv, __hmx_band3_noop_stub
    .weak _ZN6Quazal10RootObjectnwEm
    .set _ZN6Quazal10RootObjectnwEm, __hmx_band3_noop_stub
    .weak _ZN6Quazal12RBDataClient13CallDataPointEPNS_19ProtocolCallContextERKNS_6StringEPS3_
    .set _ZN6Quazal12RBDataClient13CallDataPointEPNS_19ProtocolCallContextERKNS_6StringEPS3_, __hmx_band3_noop_stub
    .weak _ZN6Quazal12RBDataClient18CallDataPointNoRetEPNS_19ProtocolCallContextERKNS_6StringE
    .set _ZN6Quazal12RBDataClient18CallDataPointNoRetEPNS_19ProtocolCallContextERKNS_6StringE, __hmx_band3_noop_stub
    .weak _ZN6Quazal13ServiceClient21RegisterExtraProtocolEPNS_8ProtocolEh
    .set _ZN6Quazal13ServiceClient21RegisterExtraProtocolEPNS_8ProtocolEh, __hmx_band3_noop_stub
    .weak _ZN6Quazal15BackEndServices22FormatQErrorCodeStringERKNS_6StringEj
    .set _ZN6Quazal15BackEndServices22FormatQErrorCodeStringERKNS_6StringEj, __hmx_band3_noop_stub
    .weak _ZN6Quazal19ProtocolCallContextC1Ev
    .set _ZN6Quazal19ProtocolCallContextC1Ev, __hmx_band3_noop_stub
    .weak _ZN6Quazal6StringaSEPKc
    .set _ZN6Quazal6StringaSEPKc, __hmx_band3_noop_stub
    .weak _ZN6Quazal6StringC1EPKc
    .set _ZN6Quazal6StringC1EPKc, __hmx_band3_noop_stub
    .weak _ZN6Quazal6StringC1Ev
    .set _ZN6Quazal6StringC1Ev, __hmx_band3_noop_stub
    .weak _ZN6Quazal6StringD1Ev
    .set _ZN6Quazal6StringD1Ev, __hmx_band3_noop_stub
    .weak _ZN6Quazal8ProtocolC2Ej
    .set _ZN6Quazal8ProtocolC2Ej, __hmx_band3_noop_stub
    // Singer non-const stubs REMOVED — Singer.cpp now compiled
    // (K8 blocker #3); strong defs win.
    .weak _ZN6Splash4PollEv
    .set _ZN6Splash4PollEv, __hmx_band3_noop_stub
    .weak _ZN6WiiRnd12GetSharedTexENS_13SharedTexTypeEb
    .set _ZN6WiiRnd12GetSharedTexENS_13SharedTexTypeEb, __hmx_band3_noop_stub
    .weak _ZN6WiiRnd18PrepareRenderAlleyEv
    .set _ZN6WiiRnd18PrepareRenderAlleyEv, __hmx_band3_noop_stub
    .weak _ZN6WiiRnd18RestoreRenderAlleyEv
    .set _ZN6WiiRnd18RestoreRenderAlleyEv, __hmx_band3_noop_stub
    .weak _ZN6WiiRnd20SetTriFrameRenderingEb
    .set _ZN6WiiRnd20SetTriFrameRenderingEb, __hmx_band3_noop_stub
    .weak _ZN6WiiTex13DeleteSurfaceEv
    .set _ZN6WiiTex13DeleteSurfaceEv, __hmx_band3_noop_stub
    .weak _ZN7UILabel12CanHaveFocusEv
    .set _ZN7UILabel12CanHaveFocusEv, __hmx_band3_noop_stub
    .weak _ZN8AppLabel11SetSongNameE6Symbolb
    .set _ZN8AppLabel11SetSongNameE6Symbolb, __hmx_band3_noop_stub
    .weak _ZN8AppLabel11SetUserNameEPK4User
    .set _ZN8AppLabel11SetUserNameEPK4User, __hmx_band3_noop_stub
    .weak _ZN8AppLabel12SetIntroNameEP8BandUser
    .set _ZN8AppLabel12SetIntroNameEP8BandUser, __hmx_band3_noop_stub
    .weak _ZN8AppLabel12SetOfferCostEPK10StoreOffer
    .set _ZN8AppLabel12SetOfferCostEPK10StoreOffer, __hmx_band3_noop_stub
    .weak _ZN8AppLabel12SetOfferNameEPK10StoreOffer
    .set _ZN8AppLabel12SetOfferNameEPK10StoreOffer, __hmx_band3_noop_stub
    .weak _ZN8AppLabel14SetSectionNameERK15PracticeSection
    .set _ZN8AppLabel14SetSectionNameERK15PracticeSection, __hmx_band3_noop_stub
    .weak _ZN8AppLabel14SetViewSettingEPK11ViewSetting
    .set _ZN8AppLabel14SetViewSettingEPK11ViewSetting, __hmx_band3_noop_stub
    .weak _ZN8AppLabel16SetFromCharacterEPK8CharData
    .set _ZN8AppLabel16SetFromCharacterEPK8CharData, __hmx_band3_noop_stub
    .weak _ZN8AppLabel17SetStoreGroupNameEPK18StoreOfferProvideri
    .set _ZN8AppLabel17SetStoreGroupNameEPK18StoreOfferProvideri, __hmx_band3_noop_stub
    .weak _ZN8AppLabel18SetLeaderboardNameERK14LeaderboardRow
    .set _ZN8AppLabel18SetLeaderboardNameERK14LeaderboardRow, __hmx_band3_noop_stub
    .weak _ZN8AppLabel19SetRawStoreShortcutEi
    .set _ZN8AppLabel19SetRawStoreShortcutEi, __hmx_band3_noop_stub
    .weak _ZN8AppLabel20SetViewSettingStatusEPK11ViewSetting
    .set _ZN8AppLabel20SetViewSettingStatusEPK11ViewSetting, __hmx_band3_noop_stub
    .weak _ZN8AppLabel21SetSongNameWithNumberEiiPKc
    .set _ZN8AppLabel21SetSongNameWithNumberEiiPKc, __hmx_band3_noop_stub
    .weak _ZN8AppLabel23SetFromScoreDisplayDataEsiib
    .set _ZN8AppLabel23SetFromScoreDisplayDataEsiib, __hmx_band3_noop_stub
    .weak _ZN8AppLabel23SetNewReleaseEntryText1EPK14StoreMainPanel
    .set _ZN8AppLabel23SetNewReleaseEntryText1EPK14StoreMainPanel, __hmx_band3_noop_stub
    .weak _ZN8AppLabel23SetNewReleaseEntryText2EPK14StoreMainPanel
    .set _ZN8AppLabel23SetNewReleaseEntryText2EPK14StoreMainPanel, __hmx_band3_noop_stub
    .weak _ZN8AppLabel23SetNewReleaseEntryText3EPK14StoreMainPanel
    .set _ZN8AppLabel23SetNewReleaseEntryText3EPK14StoreMainPanel, __hmx_band3_noop_stub
    .weak _ZN8AppLabel24SetTokenRedemptionStringEPK20TokenRedemptionPaneli
    .set _ZN8AppLabel24SetTokenRedemptionStringEPK20TokenRedemptionPaneli, __hmx_band3_noop_stub
    .weak _ZN8AppLabel25SetLeaderboardRankAndNameERK14LeaderboardRow
    .set _ZN8AppLabel25SetLeaderboardRankAndNameERK14LeaderboardRow, __hmx_band3_noop_stub
    .weak _ZN8AppLabel8SetPitchEii
    .set _ZN8AppLabel8SetPitchEii, __hmx_band3_noop_stub
    .weak _ZN8AssetMgr11GetAssetMgrEv
    .set _ZN8AssetMgr11GetAssetMgrEv, __hmx_band3_noop_stub
    .weak _ZN8AssetMgr11StripFinishE6Symbol
    .set _ZN8AssetMgr11StripFinishE6Symbol, __hmx_band3_noop_stub
    .weak _ZN8AssetMgr4InitEv
    .set _ZN8AssetMgr4InitEv, __hmx_band3_noop_stub
    .weak _ZN8CharClip6ExportEP9DataArrayb
    .set _ZN8CharClip6ExportEP9DataArrayb, __hmx_band3_noop_stub
    .weak _ZN8CharHair6HookupER10ObjPtrListI11CharCollide9ObjectDirE
    .set _ZN8CharHair6HookupER10ObjPtrListI11CharCollide9ObjectDirE, __hmx_band3_noop_stub
    .weak _ZN8CharHairC1Ev
    .set _ZN8CharHairC1Ev, __hmx_band3_noop_stub
    .weak _ZN8InputMgr21IsValidButtonForShellE12JoypadButtonP13LocalBandUser
    .set _ZN8InputMgr21IsValidButtonForShellE12JoypadButtonP13LocalBandUser, __hmx_band3_noop_stub
    .weak _ZN8InputMgr21SetInvalidMessageSinkEPN3Hmx6ObjectE
    .set _ZN8InputMgr21SetInvalidMessageSinkEPN3Hmx6ObjectE, __hmx_band3_noop_stub
    .weak _ZN8InputMgr23ClearInvalidMessageSinkEv
    .set _ZN8InputMgr23ClearInvalidMessageSinkEv, __hmx_band3_noop_stub
    .weak _ZN8InputMgr4InitEv
    .set _ZN8InputMgr4InitEv, __hmx_band3_noop_stub
    .weak _ZN8InputMgr7GetUserEv
    .set _ZN8InputMgr7GetUserEv, __hmx_band3_noop_stub
    .weak _ZN8InputMgr9TerminateEv
    .set _ZN8InputMgr9TerminateEv, __hmx_band3_noop_stub
    .weak _ZN8NodeSort10DeleteListEv
    .set _ZN8NodeSort10DeleteListEv, __hmx_band3_noop_stub
    .weak _ZN8NodeSort10DeleteTreeEv
    .set _ZN8NodeSort10DeleteTreeEv, __hmx_band3_noop_stub
    .weak _ZN8NodeSort9FirstCharEPKcb
    .set _ZN8NodeSort9FirstCharEPKcb, __hmx_band3_noop_stub
    .weak _ZN8NodeSortC2Ev
    .set _ZN8NodeSortC2Ev, __hmx_band3_noop_stub
    .weak _ZN8SongSort13BuildSongListEv
    .set _ZN8SongSort13BuildSongListEv, __hmx_band3_noop_stub
    .weak _ZN8SongSort13BuildSongTreeERSt3mapI6Symbol10SongRecordSt4lessIS1_ESaISt4pairIKS1_S2_EEERSt6vectorIP10StoreOfferSaISD_EE
    .set _ZN8SongSort13BuildSongTreeERSt3mapI6Symbol10SongRecordSt4lessIS1_ESaISt4pairIKS1_S2_EEERSt6vectorIP10StoreOfferSaISD_EE, __hmx_band3_noop_stub
    .weak _ZN9JsonArray9AddMemberEP10JsonObject
    .set _ZN9JsonArray9AddMemberEP10JsonObject, __hmx_band3_noop_stub
    .weak _ZN9MetaPanel4InitEv
    .set _ZN9MetaPanel4InitEv, __hmx_band3_noop_stub
    .weak _ZN9PrefabMgr11GetFaceTypeE6Symbol
    .set _ZN9PrefabMgr11GetFaceTypeE6Symbol, __hmx_band3_noop_stub
    .weak _ZN9PrefabMgr12GetPrefabMgrEv
    .set _ZN9PrefabMgr12GetPrefabMgrEv, __hmx_band3_noop_stub
    .weak _ZN9PrefabMgr20PrefabIsCustomizableEv
    .set _ZN9PrefabMgr20PrefabIsCustomizableEv, __hmx_band3_noop_stub
    .weak _ZN9PrefabMgr24PrefabUsesProfilePatchesEv
    .set _ZN9PrefabMgr24PrefabUsesProfilePatchesEv, __hmx_band3_noop_stub
    .weak _ZN9PrefabMgr4InitEP11BandUserMgr
    .set _ZN9PrefabMgr4InitEP11BandUserMgr, __hmx_band3_noop_stub
    .weak _ZN9PrefabMgr9GetPrefabE6Symbol
    .set _ZN9PrefabMgr9GetPrefabE6Symbol, __hmx_band3_noop_stub
    .weak _ZN9SyncStore4PollEv
    .set _ZN9SyncStore4PollEv, __hmx_band3_noop_stub
    .weak _ZNK10DataResult18GetDataResultValueE6StringR8DataNode
    .set _ZNK10DataResult18GetDataResultValueE6StringR8DataNode, __hmx_band3_noop_stub
    .weak _ZNK10GameConfig10CanEndGameEv
    .set _ZNK10GameConfig10CanEndGameEv, __hmx_band3_noop_stub
    .weak _ZNK10GameConfig11GetTrackNumERK8UserGuid
    .set _ZNK10GameConfig11GetTrackNumERK8UserGuid, __hmx_band3_noop_stub
    .weak _ZNK10GameConfig13GetControllerEP8BandUser
    .set _ZNK10GameConfig13GetControllerEP8BandUser, __hmx_band3_noop_stub
    .weak _ZNK10GameConfig16GetSectionBoundsEiRfS0_
    .set _ZNK10GameConfig16GetSectionBoundsEiRfS0_, __hmx_band3_noop_stub
    .weak _ZNK10GameConfig16IsInstrumentUsedE6Symbol
    .set _ZNK10GameConfig16IsInstrumentUsedE6Symbol, __hmx_band3_noop_stub
    .weak _ZNK10GameConfig19GetPracticeSectionsERiS0_
    .set _ZNK10GameConfig19GetPracticeSectionsERiS0_, __hmx_band3_noop_stub
    .weak _ZNK10GameConfig20GetAverageDifficultyEv
    .set _ZNK10GameConfig20GetAverageDifficultyEv, __hmx_band3_noop_stub
    .weak _ZNK10GameConfig20GetSectionBoundsTickEiRiS0_
    .set _ZNK10GameConfig20GetSectionBoundsTickEiRiS0_, __hmx_band3_noop_stub
    .weak _ZNK10JsonObject17GetObjectAsStringEv
    .set _ZNK10JsonObject17GetObjectAsStringEv, __hmx_band3_noop_stub
    .weak _ZNK10NetSession12GetLocalHostEv
    .set _ZNK10NetSession12GetLocalHostEv, __hmx_band3_noop_stub
    .weak _ZNK10NetSession12NumOpenSlotsEv
    .set _ZNK10NetSession12NumOpenSlotsEv, __hmx_band3_noop_stub
    .weak _ZNK10NetSession14IsStartingGameEv
    .set _ZNK10NetSession14IsStartingGameEv, __hmx_band3_noop_stub
    .weak _ZNK10NetSession15IsOnlineEnabledEv
    .set _ZNK10NetSession15IsOnlineEnabledEv, __hmx_band3_noop_stub
    .weak _ZNK10NetSession16GetLocalUserListERSt6vectorIP9LocalUserSaIS2_EE
    .set _ZNK10NetSession16GetLocalUserListERSt6vectorIP9LocalUserSaIS2_EE, __hmx_band3_noop_stub
    .weak _ZNK10NetSession6IsBusyEv
    .set _ZNK10NetSession6IsBusyEv, __hmx_band3_noop_stub
    .weak _ZNK10NetSession7IsLocalEv
    .set _ZNK10NetSession7IsLocalEv, __hmx_band3_noop_stub
    .weak _ZNK10NetSession8IsInGameEv
    .set _ZNK10NetSession8IsInGameEv, __hmx_band3_noop_stub
    .weak _ZNK10NetSession9IsJoiningEv
    .set _ZNK10NetSession9IsJoiningEv, __hmx_band3_noop_stub
    .weak _ZNK11BandSongMgr29WriteCachedMetadataFromStreamER9BinStream
    .set _ZNK11BandSongMgr29WriteCachedMetadataFromStreamER9BinStream, __hmx_band3_noop_stub
    // GameGemList const-method stubs REMOVED — GameGemList.cpp strongly defines all of these.
    .weak _ZNK11PlatformMgr11IsPadAGuestEi
    .set _ZNK11PlatformMgr11IsPadAGuestEi, __hmx_band3_noop_stub
    .weak _ZNK11PlatformMgr15IsGuestOnlineIDEPK8OnlineID
    .set _ZNK11PlatformMgr15IsGuestOnlineIDEPK8OnlineID, __hmx_band3_noop_stub
    .weak _ZNK11PlatformMgr24CanSeeUserCreatedContentEPK8OnlineID
    .set _ZNK11PlatformMgr24CanSeeUserCreatedContentEPK8OnlineID, __hmx_band3_noop_stub
    .weak _ZNK11SetlistSort13NewHeaderNodeEP12LeafSortNode
    .set _ZNK11SetlistSort13NewHeaderNodeEP12LeafSortNode, __hmx_band3_noop_stub
    .weak _ZNK11SetlistSort15NewShortcutNodeEP12LeafSortNode
    .set _ZNK11SetlistSort15NewShortcutNodeEP12LeafSortNode, __hmx_band3_noop_stub
    .weak _ZNK11SetlistSort16NewSubheaderNodeEP12LeafSortNode
    .set _ZNK11SetlistSort16NewSubheaderNodeEP12LeafSortNode, __hmx_band3_noop_stub
    .weak _ZNK11SingerStats11GetRankDataEi
    .set _ZNK11SingerStats11GetRankDataEi, __hmx_band3_noop_stub
    .weak _ZNK11SingerStats21GetPitchDeviationInfoERfS0_
    .set _ZNK11SingerStats21GetPitchDeviationInfoERfS0_, __hmx_band3_noop_stub
    // VocalPlayer const stubs REMOVED — VocalPlayer.cpp now compiled
    // (K8 blocker #3); strong defs win.
    // MusicLibrary const-method stubs REMOVED — MusicLibrary.cpp strongly defines all of these.
    // Kept 2 stubs below that still resolve as W (no strong def yet):
    .weak _ZNK12MusicLibrary16GetMakingSetlistEb
    .set _ZNK12MusicLibrary16GetMakingSetlistEb, __hmx_band3_noop_stub
    .weak _ZNK12MusicLibrary18GetHighlightedNodeEv
    .set _ZNK12MusicLibrary18GetHighlightedNodeEv, __hmx_band3_noop_stub
    // OutfitConfig const-method stubs REMOVED — OutfitConfig.cpp strongly defines all of these.
    .weak _ZNK12SavedSetlist8GetOwnerEv
    .set _ZNK12SavedSetlist8GetOwnerEv, __hmx_band3_noop_stub
    .weak _ZNK12SavedSetlist8IsBattleEv
    .set _ZNK12SavedSetlist8IsBattleEv, __hmx_band3_noop_stub
    .weak _ZNK12SavedSetlist9GetArtTexEv
    .set _ZNK12SavedSetlist9GetArtTexEv, __hmx_band3_noop_stub
    // TourProgress const-method stubs REMOVED — TourProgress.cpp strongly defines all of these.
    .weak _ZNK12VoiceChatMgr7IsMutedEP4User
    .set _ZNK12VoiceChatMgr7IsMutedEP4User, __hmx_band3_noop_stub
    .weak _ZNK13BandStatsInfo12GetBandStatsEv
    .set _ZNK13BandStatsInfo12GetBandStatsEv, __hmx_band3_noop_stub
    .weak _ZNK13DataEventList5EventEi
    .set _ZNK13DataEventList5EventEi, __hmx_band3_noop_stub
    .weak _ZNK13MetaPerformer10HasSetlistEv
    .set _ZNK13MetaPerformer10HasSetlistEv, __hmx_band3_noop_stub
    .weak _ZNK13MetaPerformer10IsLastSongEv
    .set _ZNK13MetaPerformer10IsLastSongEv, __hmx_band3_noop_stub
    .weak _ZNK13MetaPerformer11GetBattleIDEv
    .set _ZNK13MetaPerformer11GetBattleIDEv, __hmx_band3_noop_stub
    .weak _ZNK13MetaPerformer11IsFirstSongEv
    .set _ZNK13MetaPerformer11IsFirstSongEv, __hmx_band3_noop_stub
    .weak _ZNK13MetaPerformer13IsSetCompleteEv
    .set _ZNK13MetaPerformer13IsSetCompleteEv, __hmx_band3_noop_stub
    .weak _ZNK13MetaPerformer14GetSetlistNameEv
    .set _ZNK13MetaPerformer14GetSetlistNameEv, __hmx_band3_noop_stub
    .weak _ZNK13MetaPerformer14IsNoFailActiveEv
    .set _ZNK13MetaPerformer14IsNoFailActiveEv, __hmx_band3_noop_stub
    .weak _ZNK13MetaPerformer14PartPlaysInSetE6Symbol
    .set _ZNK13MetaPerformer14PartPlaysInSetE6Symbol, __hmx_band3_noop_stub
    .weak _ZNK13MetaPerformer15IsRandomSetListEv
    .set _ZNK13MetaPerformer15IsRandomSetListEv, __hmx_band3_noop_stub
    .weak _ZNK13MetaPerformer15PartPlaysInSongE6Symbol
    .set _ZNK13MetaPerformer15PartPlaysInSongE6Symbol, __hmx_band3_noop_stub
    .weak _ZNK13MetaPerformer16GetCompletedSongEv
    .set _ZNK13MetaPerformer16GetCompletedSongEv, __hmx_band3_noop_stub
    .weak _ZNK13MetaPerformer16IsUsingRealDrumsEv
    .set _ZNK13MetaPerformer16IsUsingRealDrumsEv, __hmx_band3_noop_stub
    .weak _ZNK13MetaPerformer17SetHasMissingPartE6Symbol
    .set _ZNK13MetaPerformer17SetHasMissingPartE6Symbol, __hmx_band3_noop_stub
    .weak _ZNK13MetaPerformer18VocalHarmonyInSongEv
    .set _ZNK13MetaPerformer18VocalHarmonyInSongEv, __hmx_band3_noop_stub
    .weak _ZNK13MetaPerformer19GetBattleInstrumentEv
    .set _ZNK13MetaPerformer19GetBattleInstrumentEv, __hmx_band3_noop_stub
    .weak _ZNK13MetaPerformer22IsNowUsingVocalHarmonyEv
    .set _ZNK13MetaPerformer22IsNowUsingVocalHarmonyEv, __hmx_band3_noop_stub
    .weak _ZNK13MetaPerformer22SetlistHasVocalHarmonyEv
    .set _ZNK13MetaPerformer22SetlistHasVocalHarmonyEv, __hmx_band3_noop_stub
    .weak _ZNK13MetaPerformer23GetSetlistMaxVocalPartsEv
    .set _ZNK13MetaPerformer23GetSetlistMaxVocalPartsEv, __hmx_band3_noop_stub
    .weak _ZNK13MetaPerformer25SetHasMissingVocalHarmonyEv
    .set _ZNK13MetaPerformer25SetHasMissingVocalHarmonyEv, __hmx_band3_noop_stub
    .weak _ZNK13MetaPerformer27GetHighestDifficultyForPartE6Symbol
    .set _ZNK13MetaPerformer27GetHighestDifficultyForPartE6Symbol, __hmx_band3_noop_stub
    .weak _ZNK13MetaPerformer27SongEndsWithEndgameSequenceEv
    .set _ZNK13MetaPerformer27SongEndsWithEndgameSequenceEv, __hmx_band3_noop_stub
    .weak _ZNK13MetaPerformer4SongEv
    .set _ZNK13MetaPerformer4SongEv, __hmx_band3_noop_stub
    .weak _ZNK13MetaPerformer7HasSongEv
    .set _ZNK13MetaPerformer7HasSongEv, __hmx_band3_noop_stub
    .weak _ZNK13MetaPerformer8GetSongsEv
    .set _ZNK13MetaPerformer8GetSongsEv, __hmx_band3_noop_stub
    .weak _ZNK13MetaPerformer8GetVenueEv
    .set _ZNK13MetaPerformer8GetVenueEv, __hmx_band3_noop_stub
    .weak _ZNK13MetaPerformer8NumSongsEv
    .set _ZNK13MetaPerformer8NumSongsEv, __hmx_band3_noop_stub
    .weak _ZNK13MetaPerformer9HasBattleEv
    .set _ZNK13MetaPerformer9HasBattleEv, __hmx_band3_noop_stub
    // VocalNoteList const stubs REMOVED — VocalNoteList.cpp now compiled
    // (K8 blocker #3); strong defs win.
    .weak _ZNK13WiiFriendList10GetProfileEi
    .set _ZNK13WiiFriendList10GetProfileEi, __hmx_band3_noop_stub
    .weak _ZNK13WiiFriendList14GetFriendByIdxEi
    .set _ZNK13WiiFriendList14GetFriendByIdxEi, __hmx_band3_noop_stub
    .weak _ZNK14DataResultList13GetDataResultEi
    .set _ZNK14DataResultList13GetDataResultEi, __hmx_band3_noop_stub
    .weak _ZNK15StorePackedPage11DefaultSortEv
    .set _ZNK15StorePackedPage11DefaultSortEv, __hmx_band3_noop_stub
    .weak _ZNK15StorePackedSong12GetDataTitleEv
    .set _ZNK15StorePackedSong12GetDataTitleEv, __hmx_band3_noop_stub
    .weak _ZNK15StorePackedSong12GetShortNameEv
    .set _ZNK15StorePackedSong12GetShortNameEv, __hmx_band3_noop_stub
    .weak _ZNK15StorePackedSong19GetUpgradeDataTitleEv
    .set _ZNK15StorePackedSong19GetUpgradeDataTitleEv, __hmx_band3_noop_stub
    .weak _ZNK15StorePackedSong7GetNameEv
    .set _ZNK15StorePackedSong7GetNameEv, __hmx_band3_noop_stub
    .weak _ZNK15StorePackedSong9DataTitleEv
    .set _ZNK15StorePackedSong9DataTitleEv, __hmx_band3_noop_stub
    .weak _ZNK15StorePackedSong9GetArtistEv
    .set _ZNK15StorePackedSong9GetArtistEv, __hmx_band3_noop_stub
    .weak _ZNK16StorePackedOffer10GetArtPathEv
    .set _ZNK16StorePackedOffer10GetArtPathEv, __hmx_band3_noop_stub
    .weak _ZNK16StorePackedOffer14GetPreviewPathEv
    .set _ZNK16StorePackedOffer14GetPreviewPathEv, __hmx_band3_noop_stub
    .weak _ZNK17NetMessageFactory21GetNetMessageByteCodeE6String
    .set _ZNK17NetMessageFactory21GetNetMessageByteCodeE6String, __hmx_band3_noop_stub
    .weak _ZNK18StoreRbnOfferTable10OfferIndexEPK20StorePackedOfferBase
    .set _ZNK18StoreRbnOfferTable10OfferIndexEPK20StorePackedOfferBase, __hmx_band3_noop_stub
    .weak _ZNK19MatchmakingSettings18GetCustomValueByIDEi
    .set _ZNK19MatchmakingSettings18GetCustomValueByIDEi, __hmx_band3_noop_stub
    .weak _ZNK19StorePackedRBNOffer10GetArtPathEv
    .set _ZNK19StorePackedRBNOffer10GetArtPathEv, __hmx_band3_noop_stub
    .weak _ZNK19StorePackedRBNOffer14GetPreviewPathEv
    .set _ZNK19StorePackedRBNOffer14GetPreviewPathEv, __hmx_band3_noop_stub
    .weak _ZNK20StoreMetadataManager13LoadingFailedEv
    .set _ZNK20StoreMetadataManager13LoadingFailedEv, __hmx_band3_noop_stub
    .weak _ZNK20StoreMetadataManager19FindOfferFromSongIdEi
    .set _ZNK20StoreMetadataManager19FindOfferFromSongIdEi, __hmx_band3_noop_stub
    .weak _ZNK20StoreMetadataManager9LoadErrorEv
    .set _ZNK20StoreMetadataManager9LoadErrorEv, __hmx_band3_noop_stub
    .weak _ZNK20StorePackedOfferBase10GetOfferIdEv
    .set _ZNK20StorePackedOfferBase10GetOfferIdEv, __hmx_band3_noop_stub
    .weak _ZNK20StorePackedOfferBase12GetAlbumNameEv
    .set _ZNK20StorePackedOfferBase12GetAlbumNameEv, __hmx_band3_noop_stub
    .weak _ZNK20StorePackedOfferBase12GetUpgradeIdEv
    .set _ZNK20StorePackedOfferBase12GetUpgradeIdEv, __hmx_band3_noop_stub
    .weak _ZNK20StorePackedOfferBase15IsVariousArtistEv
    .set _ZNK20StorePackedOfferBase15IsVariousArtistEv, __hmx_band3_noop_stub
    .weak _ZNK20StorePackedOfferBase7GetNameEv
    .set _ZNK20StorePackedOfferBase7GetNameEv, __hmx_band3_noop_stub
    .weak _ZNK20StorePackedOfferBase9GetArtistEv
    .set _ZNK20StorePackedOfferBase9GetArtistEv, __hmx_band3_noop_stub
    // AccomplishmentManager const-method stubs REMOVED — AccomplishmentManager.cpp strongly defines all of these.
    .weak _ZNK22MainHubMessageProvider15SetMessageLabelEP8AppLabeli
    .set _ZNK22MainHubMessageProvider15SetMessageLabelEP8AppLabeli, __hmx_band3_noop_stub
    // OvershellProfileProvider const stubs REMOVED — native impl in OvershellSlot.cpp.
    .weak _ZNK33AccomplishmentDiscSongConditional11IsFulfilledEP11BandProfile
    .set _ZNK33AccomplishmentDiscSongConditional11IsFulfilledEP11BandProfile, __hmx_band3_noop_stub
    .weak _ZNK33AccomplishmentDiscSongConditional13CanBeLaunchedEv
    .set _ZNK33AccomplishmentDiscSongConditional13CanBeLaunchedEv, __hmx_band3_noop_stub
    .weak _ZNK33AccomplishmentDiscSongConditional16GetTotalNumSongsEv
    .set _ZNK33AccomplishmentDiscSongConditional16GetTotalNumSongsEv, __hmx_band3_noop_stub
    .weak _ZNK33AccomplishmentDiscSongConditional17IsRelevantForSongE6Symbol
    .set _ZNK33AccomplishmentDiscSongConditional17IsRelevantForSongE6Symbol, __hmx_band3_noop_stub
    .weak _ZNK33AccomplishmentDiscSongConditional20GetNumCompletedSongsEP11BandProfile
    .set _ZNK33AccomplishmentDiscSongConditional20GetNumCompletedSongsEP11BandProfile, __hmx_band3_noop_stub
    .weak _ZNK33AccomplishmentDiscSongConditional21InqIncrementalSymbolsEP11BandProfileRSt6vectorI6SymbolSaIS3_EE
    .set _ZNK33AccomplishmentDiscSongConditional21InqIncrementalSymbolsEP11BandProfileRSt6vectorI6SymbolSaIS3_EE, __hmx_band3_noop_stub
    .weak _ZNK33AccomplishmentDiscSongConditional24HasSpecificSongsToLaunchEv
    .set _ZNK33AccomplishmentDiscSongConditional24HasSpecificSongsToLaunchEv, __hmx_band3_noop_stub
    .weak _ZNK4Band13GetMultiplierEbRiS0_S0_
    .set _ZNK4Band13GetMultiplierEbRiS0_S0_, __hmx_band3_noop_stub
    .weak _ZNK4Band13MainPerformerEv
    .set _ZNK4Band13MainPerformerEv, __hmx_band3_noop_stub
    .weak _ZNK4Band14AnyoneSaveableEv
    .set _ZNK4Band14AnyoneSaveableEv, __hmx_band3_noop_stub
    .weak _ZNK4Band16EnergyCrowdBoostEv
    .set _ZNK4Band16EnergyCrowdBoostEv, __hmx_band3_noop_stub
    .weak _ZNK4Band16EnergyMultiplierEv
    .set _ZNK4Band16EnergyMultiplierEv, __hmx_band3_noop_stub
    .weak _ZNK4Band20EveryoneDoneWithSongEv
    .set _ZNK4Band20EveryoneDoneWithSongEv, __hmx_band3_noop_stub
    .weak _ZNK4Band7GetBandEv
    .set _ZNK4Band7GetBandEv, __hmx_band3_noop_stub
    .weak _ZNK5Stats10GetHarmonyEv
    .set _ZNK5Stats10GetHarmonyEv, __hmx_band3_noop_stub
    .weak _ZNK5Stats10GetSustainEv
    .set _ZNK5Stats10GetSustainEv, __hmx_band3_noop_stub
    .weak _ZNK5Stats11GetAccuracyEv
    .set _ZNK5Stats11GetAccuracyEv, __hmx_band3_noop_stub
    .weak _ZNK5Stats12GetOverdriveEv
    .set _ZNK5Stats12GetOverdriveEv, __hmx_band3_noop_stub
    .weak _ZNK5Stats13FailedNoScoreEv
    .set _ZNK5Stats13FailedNoScoreEv, __hmx_band3_noop_stub
    .weak _ZNK5Stats13GetCodaPointsEv
    .set _ZNK5Stats13GetCodaPointsEv, __hmx_band3_noop_stub
    .weak _ZNK5Stats13GetTambourineEv
    .set _ZNK5Stats13GetTambourineEv, __hmx_band3_noop_stub
    .weak _ZNK5Stats14GetScoreStreakEv
    .set _ZNK5Stats14GetScoreStreakEv, __hmx_band3_noop_stub
    .weak _ZNK5Stats14GetSectionInfoEi
    .set _ZNK5Stats14GetSectionInfoEi, __hmx_band3_noop_stub
    .weak _ZNK5Stats14SaveForEndGameER9BinStream
    .set _ZNK5Stats14SaveForEndGameER9BinStream, __hmx_band3_noop_stub
    .weak _ZNK5Stats16GetCurrentStreakEv
    .set _ZNK5Stats16GetCurrentStreakEv, __hmx_band3_noop_stub
    .weak _ZNK5Stats16GetLongestStreakEv
    .set _ZNK5Stats16GetLongestStreakEv, __hmx_band3_noop_stub
    .weak _ZNK5Stats17GetAverageMsErrorEv
    .set _ZNK5Stats17GetAverageMsErrorEv, __hmx_band3_noop_stub
    .weak _ZNK5Stats19GetBandContributionEv
    .set _ZNK5Stats19GetBandContributionEv, __hmx_band3_noop_stub
    .weak _ZNK5Stats19GetSingerRankedPartEii
    .set _ZNK5Stats19GetSingerRankedPartEii, __hmx_band3_noop_stub
    .weak _ZNK5Stats22GetUnisonPhrasePercentEv
    .set _ZNK5Stats22GetUnisonPhrasePercentEv, __hmx_band3_noop_stub
    .weak _ZNK5Stats25GetSingerRankedPercentageEii
    .set _ZNK5Stats25GetSingerRankedPercentageEii, __hmx_band3_noop_stub
    .weak _ZNK5Stats7GetSoloEv
    .set _ZNK5Stats7GetSoloEv, __hmx_band3_noop_stub
    // Singer const stubs REMOVED — Singer.cpp now compiled
    // (K8 blocker #3); strong defs win.
    .weak _ZNK7Profile9GetPadNumEv
    .set _ZNK7Profile9GetPadNumEv, __hmx_band3_noop_stub
    .weak _ZNK8AssetMgr11GetEyebrowsERSt6vectorI6SymbolSaIS1_EES1_
    .set _ZNK8AssetMgr11GetEyebrowsERSt6vectorI6SymbolSaIS1_EES1_, __hmx_band3_noop_stub
    .weak _ZNK8AssetMgr15GetTypeFromNameE6Symbol
    .set _ZNK8AssetMgr15GetTypeFromNameE6Symbol, __hmx_band3_noop_stub
    .weak _ZNK8AssetMgr16GetEyebrowsCountE6Symbol
    .set _ZNK8AssetMgr16GetEyebrowsCountE6Symbol, __hmx_band3_noop_stub
    .weak _ZNK8AssetMgr8GetAssetE6Symbol
    .set _ZNK8AssetMgr8GetAssetE6Symbol, __hmx_band3_noop_stub
    .weak _ZNK8AssetMgr8HasAssetE6Symbol
    .set _ZNK8AssetMgr8HasAssetE6Symbol, __hmx_band3_noop_stub
    .weak _ZNK8InputMgr20IsActiveAndConnectedE14ControllerType
    .set _ZNK8InputMgr20IsActiveAndConnectedE14ControllerType, __hmx_band3_noop_stub
    .weak _ZNK8NodeSort4TextEiiP11UIListLabelP7UILabel
    .set _ZNK8NodeSort4TextEiiP11UIListLabelP7UILabel, __hmx_band3_noop_stub
    .weak _ZNK8NodeSort6CustomEiiP12UIListCustomPN3Hmx6ObjectE
    .set _ZNK8NodeSort6CustomEiiP12UIListCustomPN3Hmx6ObjectE, __hmx_band3_noop_stub
    .weak _ZNK8NodeSort7NumDataEv
    .set _ZNK8NodeSort7NumDataEv, __hmx_band3_noop_stub
    .weak _ZNK8NodeSort8IsActiveEi
    .set _ZNK8NodeSort8IsActiveEi, __hmx_band3_noop_stub
    .weak _ZNK8SongSort13NewHeaderNodeEP12LeafSortNode
    .set _ZNK8SongSort13NewHeaderNodeEP12LeafSortNode, __hmx_band3_noop_stub
    .weak _ZNK8SongSort15NewShortcutNodeEP12LeafSortNode
    .set _ZNK8SongSort15NewShortcutNodeEP12LeafSortNode, __hmx_band3_noop_stub
    .weak _ZNK8SongSort16NewSubheaderNodeEP12LeafSortNode
    .set _ZNK8SongSort16NewSubheaderNodeEP12LeafSortNode, __hmx_band3_noop_stub
    .weak _ZNK9PrefabMgr10GetPrefabsERSt6vectorIP10PrefabCharSaIS2_EE
    .set _ZNK9PrefabMgr10GetPrefabsERSt6vectorIP10PrefabCharSaIS2_EE, __hmx_band3_noop_stub
    .weak _ZNK9PrefabMgr12GetFaceTypesERSt6vectorI6SymbolSaIS1_EES1_
    .set _ZNK9PrefabMgr12GetFaceTypesERSt6vectorI6SymbolSaIS1_EES1_, __hmx_band3_noop_stub
    .weak _ZNK9PrefabMgr16GetDefaultPrefabEi
    .set _ZNK9PrefabMgr16GetDefaultPrefabEi, __hmx_band3_noop_stub
    .weak _ZNK9PrefabMgr19GetAvailablePrefabsERSt6vectorIP10PrefabCharSaIS2_EE
    .set _ZNK9PrefabMgr19GetAvailablePrefabsERSt6vectorIP10PrefabCharSaIS2_EE, __hmx_band3_noop_stub
    .weak _ZNK9PrefabMgr20GetCharCreatorPrefabE6SymbolS0_
    .set _ZNK9PrefabMgr20GetCharCreatorPrefabE6SymbolS0_, __hmx_band3_noop_stub
    .weak _ZNK9PrefabMgr26GetRandomCharCreatorPrefabE6Symbol
    .set _ZNK9PrefabMgr26GetRandomCharCreatorPrefabE6Symbol, __hmx_band3_noop_stub
    .weak _ZNK9StorePage5OfferEi
    .set _ZNK9StorePage5OfferEi, __hmx_band3_noop_stub
    .weak _ZNK9StorePage7SubmenuEi
    .set _ZNK9StorePage7SubmenuEi, __hmx_band3_noop_stub
    .weak _ZNK9StorePage9BaseOfferEi
    .set _ZNK9StorePage9BaseOfferEi, __hmx_band3_noop_stub
    .weak _ZThn92_N12TexLoadPanel13ContentFailedEPKc
    .set _ZThn92_N12TexLoadPanel13ContentFailedEPKc, __hmx_band3_noop_stub
    .weak _ZThn92_N12TexLoadPanel14ContentMountedEPKcS1_
    .set _ZThn92_N12TexLoadPanel14ContentMountedEPKcS1_, __hmx_band3_noop_stub

    // ---- Menu-bring-up wave (2026-05-27): symbols newly referenced once
    // MetaPanel/MusicLibrary/MetaPerformer/AccomplishmentManager/Band/SongSort/
    // OutfitConfig/AccomplishmentPanel/TourDescPanel/QuestFilterPanel were
    // un-excluded. These belong to DEFERRED gameplay/char/Wii TUs (BandPatchMesh,
    // VocalPlayer, GemTrackDir, ChordShapeGenerator, TourPerformerLocal,
    // AccomplishmentDiscSongConditional, AssetMgr::EquipAssets) or Wii-online-only
    // classes (Wii friends/invites/profile panels, MemcardMgr) that the menu DTA
    // parse never instantiates. Weak no-op so the boot links; a real bring-up of
    // the owning TU overrides them. ----
    // BandPatchMesh (DEFER: bandobj/char, instantiated only for character outfits)
    .weak _ZN13BandPatchMeshC1EPN3Hmx6ObjectE
    .set _ZN13BandPatchMeshC1EPN3Hmx6ObjectE, __hmx_band3_noop_stub
    .weak _ZN13BandPatchMeshC1ERKS_
    .set _ZN13BandPatchMeshC1ERKS_, __hmx_band3_noop_stub
    .weak _ZN13BandPatchMeshaSERKS_
    .set _ZN13BandPatchMeshaSERKS_, __hmx_band3_noop_stub
    .weak _ZN13BandPatchMesh6RenderEP6RndTexP6RndMat
    .set _ZN13BandPatchMesh6RenderEP6RndTexP6RndMat, __hmx_band3_noop_stub
    .weak _ZN13BandPatchMesh9ReProjectEv
    .set _ZN13BandPatchMesh9ReProjectEv, __hmx_band3_noop_stub
    .weak _ZN13BandPatchMesh9PreRenderEP12BandCharDesci
    .set _ZN13BandPatchMesh9PreRenderEP12BandCharDesci, __hmx_band3_noop_stub
    .weak _ZN13BandPatchMesh10PostRenderEv
    .set _ZN13BandPatchMesh10PostRenderEv, __hmx_band3_noop_stub
    .weak _ZN13BandPatchMesh8CompressEP12BandCharDesc
    .set _ZN13BandPatchMesh8CompressEP12BandCharDesc, __hmx_band3_noop_stub
    .weak _ZN13BandPatchMesh16ListDrawChildrenERNSt7__cxx114listIP11RndDrawableSaIS3_EEE
    .set _ZN13BandPatchMesh16ListDrawChildrenERNSt7__cxx114listIP11RndDrawableSaIS3_EEE, __hmx_band3_noop_stub
    .weak _ZrsR9BinStreamR13BandPatchMesh
    .set _ZrsR9BinStreamR13BandPatchMesh, __hmx_band3_noop_stub
    .weak _Z8PropSyncR13BandPatchMeshR8DataNodeP9DataArrayi6PropOp
    .set _Z8PropSyncR13BandPatchMeshR8DataNodeP9DataArrayi6PropOp, __hmx_band3_noop_stub
    // VocalPlayer ctor stub REMOVED — VocalPlayer.cpp now compiled (K8 blocker #3).
    // _ZN11GemTrackDirC1Ev REMOVED — GemTrackDir.cpp now compiled (strong ctor).
    // ChordShapeGenerator stubs (ctor/BuildChordMesh/MakeInvertedMesh) REMOVED —
    // ChordShapeGenerator.cpp now compiled (un-excluded); strong defs win.
    // TourPerformerLocal (DEFER) + AccomplishmentDiscSongConditional + AssetMgr
    .weak _ZN18TourPerformerLocal15SetCurrentQuestE6Symbol
    .set _ZN18TourPerformerLocal15SetCurrentQuestE6Symbol, __hmx_band3_noop_stub
    .weak _ZN18TourPerformerLocal17CheatCycleSetlistEv
    .set _ZN18TourPerformerLocal17CheatCycleSetlistEv, __hmx_band3_noop_stub
    .weak _ZN18TourPerformerLocal19CheatCycleChallengeEv
    .set _ZN18TourPerformerLocal19CheatCycleChallengeEv, __hmx_band3_noop_stub
    .weak _ZN18TourPerformerLocal21SetCurrentQuestFilterE6Symbol15TourSetlistType
    .set _ZN18TourPerformerLocal21SetCurrentQuestFilterE6Symbol15TourSetlistType, __hmx_band3_noop_stub
    .weak _ZN18TourPerformerLocal23SanityCheckQuestFiltersEv
    .set _ZN18TourPerformerLocal23SanityCheckQuestFiltersEv, __hmx_band3_noop_stub
    // AccomplishmentDiscSongConditional + AssetMgr brought up (compiled) — their
    // symbols are now strong defs; stubs removed.
    // Wii online-only: MemcardMgr + WiiFriend query never offline. (The
    // JoinInvitePanel/WiiProfilePanel/WiiFriendsDetailsProvider ctors,
    // WiiFriendsScreen::Init, WiiFriendsProvider::Init/Poll,
    // WiiInvitationsProvider::Init + TheWii*Provider globals are now strongly
    // defined as minimal native glue in MetaPanel.cpp — stubs removed.)
    .weak _ZN10MemcardMgr4InitEv
    .set _ZN10MemcardMgr4InitEv, __hmx_band3_noop_stub
    .weak _ZN25WiiFriendsDetailsProviderC1Ev
    .set _ZN25WiiFriendsDetailsProviderC1Ev, __hmx_band3_noop_stub
    .weak _ZN18WiiFriendsProvider19GetPossessiveSuffixEPKc
    .set _ZN18WiiFriendsProvider19GetPossessiveSuffixEPKc, __hmx_band3_noop_stub
    .weak _ZN18WiiFriendsProvider24IsPossessiveSuffixNeededEPKc
    .set _ZN18WiiFriendsProvider24IsPossessiveSuffixNeededEPKc, __hmx_band3_noop_stub
    .weak _ZNK9WiiFriend10GetProfileEPKc
    .set _ZNK9WiiFriend10GetProfileEPKc, __hmx_band3_noop_stub

    // ---- DATA stubs (each: own 256-byte writable zero-filled reservation) ----
    .bss
    .p2align 4
    // _ZTI10NetSession REMOVED — rb3_netsession_native.cpp now emits the REAL
    // NetSession typeinfo + vtable (NetSession's key function Handle is defined there).
    // BandUI::Init dynamic_casts the registered `session` object via ObjDirItr<UIScreen>,
    // so a zeroed RTTI reservation no longer suffices — the real one is required.
    // _ZTI19ChordShapeGenerator REMOVED — ChordShapeGenerator.cpp now compiled
    // (real RTTI from its OBJ_CLASSNAME/vtable).
    .weak gCNTThreadInUse
gCNTThreadInUse:
    .zero 256
    .weak gInitComplete
gInitComplete:
    .zero 256
    .weak gLastErrorDesc
gLastErrorDesc:
    .zero 256
    .weak gLastErrorReturnValue
gLastErrorReturnValue:
    .zero 256
    .weak gRenderTextureSet
gRenderTextureSet:
    .zero 256
    .weak gStoreMetadataManagerLoadStepName
gStoreMetadataManagerLoadStepName:
    .zero 256
    .weak gStoreUIOverlay
gStoreUIOverlay:
    .zero 256
    .weak kInvalidPitch__11VocalPlayer
kInvalidPitch__11VocalPlayer:
    .zero 256
    .weak TheAccomplishmentMgr
TheAccomplishmentMgr:
    .zero 256
    .weak TheGameConfig
TheGameConfig:
    .zero 256
    .weak TheInputMgr
TheInputMgr:
    .zero 256
    .weak TheMusicLibrary
TheMusicLibrary:
    .zero 256
    .weak TheNet
TheNet:
    .zero 256
    .weak TheNetMessageFactory
TheNetMessageFactory:
    .zero 256
    .weak TheNetSession
TheNetSession:
    .zero 256
    .weak TheNgStats
TheNgStats:
    .zero 256
    .weak TheSaveLoadMgr
TheSaveLoadMgr:
    .zero 256
    .weak TheServer
TheServer:
    .zero 256
    .weak TheSessionMgr
TheSessionMgr:
    .zero 256
    .weak TheSongDB
TheSongDB:
    .zero 256
    .weak TheSplasher
TheSplasher:
    .zero 256
    .weak TheStoreMetadata
TheStoreMetadata:
    .zero 256
    .weak TheSyncStore
TheSyncStore:
    .zero 256
    .weak TheTour
TheTour:
    .zero 256
    .weak TheVoiceChatMgr
TheVoiceChatMgr:
    .zero 256
    .weak TheWiiContentMgr
TheWiiContentMgr:
    .zero 256
    .weak TheWiiFriendMgr
TheWiiFriendMgr:
    .zero 256
    .weak TheWiiFX
TheWiiFX:
    .zero 256
    .weak TheWiiMessenger
TheWiiMessenger:
    .zero 256
    .weak _ZN11BandCamShot22sHideAllCharactersHackE
_ZN11BandCamShot22sHideAllCharactersHackE:
    .zero 256
    .weak _ZN14WiiCommerceMgr7mOpNameE
_ZN14WiiCommerceMgr7mOpNameE:
    .zero 256
    .weak _ZN15SongUpgradeData8sSaveVerE
_ZN15SongUpgradeData8sSaveVerE:
    .zero 256
    .weak _ZN20StoreMetadataManager14mSetlistOffersE
_ZN20StoreMetadataManager14mSetlistOffersE:
    .zero 256
    .weak _ZN6WiiRnd14mShowAssetNameE
_ZN6WiiRnd14mShowAssetNameE:
    .zero 256
    .weak _ZN9MetaPanel10sUnlockAllE
_ZN9MetaPanel10sUnlockAllE:
    .zero 256
    .weak _ZN9MetaPanel11sIsPlaytestE
_ZN9MetaPanel11sIsPlaytestE:
    .zero 256
    .weak _ZN9MetaPanel21sLaunchedGoalMsgsOnlyE
_ZN9MetaPanel21sLaunchedGoalMsgsOnlyE:
    .zero 256
    .weak _ZTI10FileMerger
_ZTI10FileMerger:
    .zero 256
    .weak _ZTI10MidiParser
_ZTI10MidiParser:
    .zero 256
    // _ZTI11GemTrackDir REMOVED — GemTrackDir.cpp now compiled (real RTTI).
    .weak _ZTI11SetlistSort
_ZTI11SetlistSort:
    .zero 256
    // _ZTI11VocalPlayer REMOVED — VocalPlayer.cpp now compiled (real RTTI;
    // this fixes the K8 dynamic_cast<VocalPlayer*> walking a zeroed RTTI stub).
    .weak _ZTI12OutfitConfig
_ZTI12OutfitConfig:
    .zero 256
    .weak _ZTI12SavedSetlist
_ZTI12SavedSetlist:
    .zero 256
    .weak _ZTI12WiiMultiMesh
_ZTI12WiiMultiMesh:
    .zero 256
    // _ZTI13CharForeTwist REMOVED — CharForeTwist.cpp now compiled (real RTTI).
    // _ZTI14Synchronizable REMOVED — Synchronize.cpp now compiled (real RTTI);
    // the zeroed stub crashed __dynamic_cast through OvershellPanel's MI hierarchy.
    .weak _ZTI18TourPerformerLocal
_ZTI18TourPerformerLocal:
    .zero 256
    .weak _ZTI33AccomplishmentDiscSongConditional
_ZTI33AccomplishmentDiscSongConditional:
    .zero 256
    .weak _ZTI8AppLabel
_ZTI8AppLabel:
    .zero 256
    .weak _ZTI8CharHair
_ZTI8CharHair:
    .zero 256
    .weak _ZTI8NodeSort
_ZTI8NodeSort:
    .zero 256
    .weak _ZTI8SongSort
_ZTI8SongSort:
    .zero 256
    .weak _ZTV11SetlistSort
_ZTV11SetlistSort:
    .zero 256
    .weak _ZTV12SavedSetlist
_ZTV12SavedSetlist:
    .zero 256
    .weak _ZTV14SongSortByDiff
_ZTV14SongSortByDiff:
    .zero 256
    .weak _ZTV19MatchmakingSettings
_ZTV19MatchmakingSettings:
    .zero 256
    .weak _ZTV8NodeSort
_ZTV8NodeSort:
    .zero 256
    .weak _ZTV8SongSort
_ZTV8SongSort:
    .zero 256
    .weak _ZTVN6Quazal12RBDataClientE
_ZTVN6Quazal12RBDataClientE:
    .zero 256
    .weak _ZTVN6Quazal12RBTestClientE
_ZTVN6Quazal12RBTestClientE:
    .zero 256
    .weak _ZTVN6Quazal14ClientProtocolE
_ZTVN6Quazal14ClientProtocolE:
    .zero 256
    .weak _ZTVN6Quazal18RBBinaryDataClientE
_ZTVN6Quazal18RBBinaryDataClientE:
    .zero 256
    // Wii online-singleton globals referenced by the menu TUs (their real defs
    // live in Wii-only/excluded TUs). Zeroed reservation; the offline code paths
    // that read these are gated/never-hit on the native menu boot. (TheWii*Provider
    // are now defined in MetaPanel.cpp native glue — removed here.)
    .weak TheMemcardMgr
TheMemcardMgr:
    .zero 256
