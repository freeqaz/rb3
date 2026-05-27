// AUTO-GENERATED weak no-op stubs for off-path symbols not on the DTA
// parse path (rendering / audio / Bink / MIDI / RSO / Wii managers /
// vtables / typeinfo). Each is WEAK so any real definition wins; the
// rest resolve to a shared no-op (functions return; data reads yield a
// valid non-null pointer). Regenerate via native/scripts if the set
// changes. See NATIVE_PORT notes.
    .text
    .p2align 4
__hmx_native_noop_stub:
    xorl %eax, %eax
    ret
    // rb3-dta compiles os/System.cpp (whose native SystemInit now calls
    // RB3InitNativeNetSession) but does not link rb3_netsession_native.cpp; rb3-dta
    // never calls SystemInit, so this weak no-op never executes. rb3-native links
    // the strong def, which wins.
    .weak _Z23RB3InitNativeNetSessionv
    .set _Z23RB3InitNativeNetSessionv, __hmx_native_noop_stub
    .weak AXSetCompressor
    .set AXSetCompressor, __hmx_native_noop_stub
    .weak BinkOpenAX
    .set BinkOpenAX, __hmx_native_noop_stub
    .weak BinkSetIO
    .set BinkSetIO, __hmx_native_noop_stub
    .weak BinkSetMemory
    .set BinkSetMemory, __hmx_native_noop_stub
    .weak BinkSetSoundSystem
    .set BinkSetSoundSystem, __hmx_native_noop_stub
    .weak DVDInit
    .set DVDInit, __hmx_native_noop_stub
    .weak FileDelete
    .set FileDelete, __hmx_native_noop_stub
    .weak FileEnumerate
    .set FileEnumerate, __hmx_native_noop_stub
    .weak FileGetStat
    .set FileGetStat, __hmx_native_noop_stub
    .weak FileMkDir
    .set FileMkDir, __hmx_native_noop_stub
    .weak JoypadSetActuatorsImp
    .set JoypadSetActuatorsImp, __hmx_native_noop_stub
    .weak ParseStack
    .set ParseStack, __hmx_native_noop_stub
    .weak RADTimerRead
    .set RADTimerRead, __hmx_native_noop_stub
    .weak RSOGetFarCodeSize
    .set RSOGetFarCodeSize, __hmx_native_noop_stub
    .weak RSOGetImportSymbolName
    .set RSOGetImportSymbolName, __hmx_native_noop_stub
    .weak RSOGetJumpCodeSize
    .set RSOGetJumpCodeSize, __hmx_native_noop_stub
    .weak RSOGetNumImportSymbols
    .set RSOGetNumImportSymbols, __hmx_native_noop_stub
    .weak RSOIsImportSymbolResolved
    .set RSOIsImportSymbolResolved, __hmx_native_noop_stub
    .weak RSOIsImportSymbolResolvedAll
    .set RSOIsImportSymbolResolvedAll, __hmx_native_noop_stub
    .weak RSOLinkFar
    .set RSOLinkFar, __hmx_native_noop_stub
    .weak RSOLinkJump
    .set RSOLinkJump, __hmx_native_noop_stub
    .weak RSOLinkList
    .set RSOLinkList, __hmx_native_noop_stub
    .weak RSOListInit
    .set RSOListInit, __hmx_native_noop_stub
    .weak RSOMakeJumpCode
    .set RSOMakeJumpCode, __hmx_native_noop_stub
    .weak RSOUnLinkList
    .set RSOUnLinkList, __hmx_native_noop_stub
    .weak TheBaseSongManger
    .set TheBaseSongManger, __hmx_native_noop_stub
    .weak TheContentMgr
    .set TheContentMgr, __hmx_native_noop_stub
    .weak TheFakeSongMgr
    .set TheFakeSongMgr, __hmx_native_noop_stub
    .weak TheMC
    .set TheMC, __hmx_native_noop_stub
    .weak TheMidiParserMgr
    .set TheMidiParserMgr, __hmx_native_noop_stub
    .weak TheSynth
    .set TheSynth, __hmx_native_noop_stub
    .weak TheWiiCommerceMgr
    .set TheWiiCommerceMgr, __hmx_native_noop_stub
    .weak TheWiiProfileMgr
    .set TheWiiProfileMgr, __hmx_native_noop_stub
    .weak _Z10CDGetErrorv
    .set _Z10CDGetErrorv, __hmx_native_noop_stub
    .weak _Z10CDReadDonev
    .set _Z10CDReadDonev, __hmx_native_noop_stub
    .weak _Z10JoypadInitv
    .set _Z10JoypadInitv, __hmx_native_noop_stub
    .weak _Z10JoypadPollv
    .set _Z10JoypadPollv, __hmx_native_noop_stub
    .weak _Z10ThreadCallP14ThreadCallback
    .set _Z10ThreadCallP14ThreadCallback, __hmx_native_noop_stub
    .weak _Z11FileIsLocalPKc
    .set _Z11FileIsLocalPKc, __hmx_native_noop_stub
    .weak _Z11JoypadResetv
    .set _Z11JoypadResetv, __hmx_native_noop_stub
    .weak _Z11UsingHolmesi
    .set _Z11UsingHolmesi, __hmx_native_noop_stub
    .weak _Z12EndianSwapEqIiEvRT_
    .set _Z12EndianSwapEqIiEvRT_, __hmx_native_noop_stub
    .weak _Z12KeyboardInitv
    .set _Z12KeyboardInitv, __hmx_native_noop_stub
    .weak _Z12KeyboardPollv
    .set _Z12KeyboardPollv, __hmx_native_noop_stub
    .weak _Z13CacheResourcePKcPN3Hmx6ObjectE
    .set _Z13CacheResourcePKcPN3Hmx6ObjectE, __hmx_native_noop_stub
    .weak _Z13RndGxDrawDonev
    .set _Z13RndGxDrawDonev, __hmx_native_noop_stub
    .weak _Z14CDReadExternalRP11DVDFileInfoiy
    .set _Z14CDReadExternalRP11DVDFileInfoiy, __hmx_native_noop_stub
    .weak _Z14GetMapFileNameR6String
    .set _Z14GetMapFileNameR6String, __hmx_native_noop_stub
    .weak _Z14ThreadCallInitv
    .set _Z14ThreadCallInitv, __hmx_native_noop_stub
    .weak _Z14ThreadCallPollv
    .set _Z14ThreadCallPollv, __hmx_native_noop_stub
    .weak _Z15HolmesResolveIPv
    .set _Z15HolmesResolveIPv, __hmx_native_noop_stub
    .weak _Z15InitDefaultHeapv
    .set _Z15InitDefaultHeapv, __hmx_native_noop_stub
    .weak _Z15JoypadTerminatev
    .set _Z15JoypadTerminatev, __hmx_native_noop_stub
    .weak _Z16GetWiiJoypadTypei
    .set _Z16GetWiiJoypadTypei, __hmx_native_noop_stub
    .weak _Z16HolmesClientInitv
    .set _Z16HolmesClientInitv, __hmx_native_noop_stub
    .weak _Z16HolmesClientOpenPKciRjRi
    .set _Z16HolmesClientOpenPKciRjRi, __hmx_native_noop_stub
    .weak _Z16HolmesClientPollv
    .set _Z16HolmesClientPollv, __hmx_native_noop_stub
    .weak _Z16HolmesClientReadiiiPvP4File
    .set _Z16HolmesClientReadiiiPvP4File, __hmx_native_noop_stub
    .weak _Z17CaptureStackTraceiPj
    .set _Z17CaptureStackTraceiPj, __hmx_native_noop_stub
    .weak _Z17GetSystemLanguage6Symbol
    .set _Z17GetSystemLanguage6Symbol, __hmx_native_noop_stub
    .weak _Z17HolmesClientCloseP4Filei
    .set _Z17HolmesClientCloseP4Filei, __hmx_native_noop_stub
    .weak _Z17HolmesClientPrintPKc
    .set _Z17HolmesClientPrintPKc, __hmx_native_noop_stub
    .weak _Z17HolmesClientWriteiiiPKv
    .set _Z17HolmesClientWriteiiiPKv, __hmx_native_noop_stub
    .weak _Z17KeyboardTerminatev
    .set _Z17KeyboardTerminatev, __hmx_native_noop_stub
    .weak _Z17ThreadCallPreInitv
    .set _Z17ThreadCallPreInitv, __hmx_native_noop_stub
    .weak _Z18HolmesClientReInitv
    .set _Z18HolmesClientReInitv, __hmx_native_noop_stub
    .weak _Z18PlatformDebugBreakv
    .set _Z18PlatformDebugBreakv, __hmx_native_noop_stub
    .weak _Z19HolmesClientSysExecPKc
    .set _Z19HolmesClientSysExecPKc, __hmx_native_noop_stub
    .weak _Z19ThreadCallTerminatev
    .set _Z19ThreadCallTerminatev, __hmx_native_noop_stub
    .weak _Z20HolmesClientReadDoneP4File
    .set _Z20HolmesClientReadDoneP4File, __hmx_native_noop_stub
    .weak _Z20HolmesClientTruncateii
    .set _Z20HolmesClientTruncateii, __hmx_native_noop_stub
    .weak _Z21FileQualifiedFilenamePciPKc
    .set _Z21FileQualifiedFilenamePciPKc, __hmx_native_noop_stub
    .weak _Z21HolmesClientTerminatev
    .set _Z21HolmesClientTerminatev, __hmx_native_noop_stub
    .weak _Z22HolmesClientStackTracePKcPjiR6String
    .set _Z22HolmesClientStackTracePKcPjiR6String, __hmx_native_noop_stub
    .weak _Z22SetGPHangDetectEnabledbPKc
    .set _Z22SetGPHangDetectEnabledbPKc, __hmx_native_noop_stub
    .weak _Z6CDReadiiiPv
    .set _Z6CDReadiiiPv, __hmx_native_noop_stub
    .weak _Z8CacheWavPKcR19CacheResourceResult
    .set _Z8CacheWavPKcR19CacheResourceResult, __hmx_native_noop_stub
    .weak _Z9FileIsDLCPKc
    .set _Z9FileIsDLCPKc, __hmx_native_noop_stub
    .weak _ZN10MemcardWii4InitEv
    .set _ZN10MemcardWii4InitEv, __hmx_native_noop_stub
    .weak _ZN10MemcardWii9TerminateEv
    .set _ZN10MemcardWii9TerminateEv, __hmx_native_noop_stub
    .weak _ZN10MidiParser4PollEv
    .set _ZN10MidiParser4PollEv, __hmx_native_noop_stub
    .weak _ZN10MidiParser8sParsersB5cxx11E
    .set _ZN10MidiParser8sParsersB5cxx11E, __hmx_native_noop_stub
    .weak _ZN10RndOverlay4FindE6Symbolb
    .set _ZN10RndOverlay4FindE6Symbolb, __hmx_native_noop_stub
    .weak _ZN10WiiContent9EnumerateEPKcPFvS1_S1_EbS1_
    .set _ZN10WiiContent9EnumerateEPKcPFvS1_S1_EbS1_, __hmx_native_noop_stub
    .weak _ZN11ByteGrinder10GrindArrayEllPhii
    .set _ZN11ByteGrinder10GrindArrayEllPhii, __hmx_native_noop_stub
    .weak _ZN11CacheMgrWiiC1Ev
    .set _ZN11CacheMgrWiiC1Ev, __hmx_native_noop_stub
    .weak _ZN11FakeSongMgr13GetSongConfigE6Symbol
    .set _ZN11FakeSongMgr13GetSongConfigE6Symbol, __hmx_native_noop_stub
    .weak _ZN11PlatformMgr10RegionInitEv
    .set _ZN11PlatformMgr10RegionInitEv, __hmx_native_noop_stub
    .weak _ZN11PlatformMgr11ContentDoneEv
    .set _ZN11PlatformMgr11ContentDoneEv, __hmx_native_noop_stub
    .weak _ZN11PlatformMgr12SetConnectedEb
    .set _ZN11PlatformMgr12SetConnectedEb, __hmx_native_noop_stub
    .weak _ZN11PlatformMgr12SetDiskErrorE9DiskError
    .set _ZN11PlatformMgr12SetDiskErrorE9DiskError, __hmx_native_noop_stub
    .weak _ZN11PlatformMgr13ClearDWCErrorEv
    .set _ZN11PlatformMgr13ClearDWCErrorEv, __hmx_native_noop_stub
    .weak _ZN11PlatformMgr13ClearNetErrorEv
    .set _ZN11PlatformMgr13ClearNetErrorEv, __hmx_native_noop_stub
    .weak _ZN11PlatformMgr13OnSignInUsersEPK9DataArray
    .set _ZN11PlatformMgr13OnSignInUsersEPK9DataArray, __hmx_native_noop_stub
    .weak _ZN11PlatformMgr14ContentStartedEv
    .set _ZN11PlatformMgr14ContentStartedEv, __hmx_native_noop_stub
    .weak _ZN11PlatformMgr14SetScreenSaverEb
    .set _ZN11PlatformMgr14SetScreenSaverEb, __hmx_native_noop_stub
    .weak _ZN11PlatformMgr14StartProfanityEPKcPN3Hmx6ObjectE
    .set _ZN11PlatformMgr14StartProfanityEPKcPN3Hmx6ObjectE, __hmx_native_noop_stub
    .weak _ZN11PlatformMgr15EnableProfanityEb
    .set _ZN11PlatformMgr15EnableProfanityEb, __hmx_native_noop_stub
    .weak _ZN11PlatformMgr15GetLastDNSErrorEv
    .set _ZN11PlatformMgr15GetLastDNSErrorEv, __hmx_native_noop_stub
    .weak _ZN11PlatformMgr15GetLastDWCErrorEv
    .set _ZN11PlatformMgr15GetLastDWCErrorEv, __hmx_native_noop_stub
    .weak _ZN11PlatformMgr16ContentCancelledEv
    .set _ZN11PlatformMgr16ContentCancelledEv, __hmx_native_noop_stub
    .weak _ZN11PlatformMgr16PrintParentalPinEv
    .set _ZN11PlatformMgr16PrintParentalPinEv, __hmx_native_noop_stub
    .weak _ZN11PlatformMgr17GetLastNHTTPErrorEv
    .set _ZN11PlatformMgr17GetLastNHTTPErrorEv, __hmx_native_noop_stub
    .weak _ZN11PlatformMgr17GetNetErrorStringEb
    .set _ZN11PlatformMgr17GetNetErrorStringEb, __hmx_native_noop_stub
    .weak _ZN11PlatformMgr18RunNetStartUtilityEv
    .set _ZN11PlatformMgr18RunNetStartUtilityEv, __hmx_native_noop_stub
    .weak _ZN11PlatformMgr18SetHomeMenuEnabledEb
    .set _ZN11PlatformMgr18SetHomeMenuEnabledEb, __hmx_native_noop_stub
    .weak _ZN11PlatformMgr19SetNotifyUILocationE14NotifyLocation
    .set _ZN11PlatformMgr19SetNotifyUILocationE14NotifyLocation, __hmx_native_noop_stub
    .weak _ZN11PlatformMgr22InitNintendoConnectionEv
    .set _ZN11PlatformMgr22InitNintendoConnectionEv, __hmx_native_noop_stub
    .weak _ZN11PlatformMgr23CloseNintendoConnectionEbb
    .set _ZN11PlatformMgr23CloseNintendoConnectionEbb, __hmx_native_noop_stub
    .weak _ZN11PlatformMgr24IsEthernetCableConnectedEv
    .set _ZN11PlatformMgr24IsEthernetCableConnectedEv, __hmx_native_noop_stub
    .weak _ZN11PlatformMgr25SetPartyMicOptionsShowingEb
    .set _ZN11PlatformMgr25SetPartyMicOptionsShowingEb, __hmx_native_noop_stub
    .weak _ZN11PlatformMgr28GetNetErrorStringAsDataArrayEb
    .set _ZN11PlatformMgr28GetNetErrorStringAsDataArrayEb, __hmx_native_noop_stub
    .weak _ZN11PlatformMgr4InitEv
    .set _ZN11PlatformMgr4InitEv, __hmx_native_noop_stub
    .weak _ZN11PlatformMgr4PollEv
    .set _ZN11PlatformMgr4PollEv, __hmx_native_noop_stub
    .weak _ZN11PlatformMgr5OnMsgERK11ButtonUpMsg
    .set _ZN11PlatformMgr5OnMsgERK11ButtonUpMsg, __hmx_native_noop_stub
    .weak _ZN11PlatformMgr5OnMsgERK13ButtonDownMsg
    .set _ZN11PlatformMgr5OnMsgERK13ButtonDownMsg, __hmx_native_noop_stub
    .weak _ZN11PlatformMgr7PreInitEv
    .set _ZN11PlatformMgr7PreInitEv, __hmx_native_noop_stub
    .weak _ZN11PlatformMgr7WiiPollEv
    .set _ZN11PlatformMgr7WiiPollEv, __hmx_native_noop_stub
    .weak _ZN11PlatformMgrC1Ev
    .set _ZN11PlatformMgrC1Ev, __hmx_native_noop_stub
    .weak _ZN11PlatformMgrD0Ev
    .set _ZN11PlatformMgrD0Ev, __hmx_native_noop_stub
    .weak _ZN11PlatformMgrD1Ev
    .set _ZN11PlatformMgrD1Ev, __hmx_native_noop_stub
    .weak _ZN12MidiReceiverC2Ev
    .set _ZN12MidiReceiverC2Ev, __hmx_native_noop_stub
    .weak _ZN12NetLoaderWiiC1ERK6String
    .set _ZN12NetLoaderWiiC1ERK6String, __hmx_native_noop_stub
    .weak _ZN13CameraManager4PollEv
    .set _ZN13CameraManager4PollEv, __hmx_native_noop_stub
    .weak _ZN13NetworkSocket11GetHostNameEv
    .set _ZN13NetworkSocket11GetHostNameEv, __hmx_native_noop_stub
    .weak _ZN13NetworkSocket6CreateEb
    .set _ZN13NetworkSocket6CreateEb, __hmx_native_noop_stub
    .weak _ZN13RndAnimatable12SyncPropertyER8DataNodeP9DataArrayi6PropOp
    .set _ZN13RndAnimatable12SyncPropertyER8DataNodeP9DataArrayi6PropOp, __hmx_native_noop_stub
    .weak _ZN13RndAnimatable4CopyEPKN3Hmx6ObjectENS1_8CopyTypeE
    .set _ZN13RndAnimatable4CopyEPKN3Hmx6ObjectENS1_8CopyTypeE, __hmx_native_noop_stub
    .weak _ZN13RndAnimatable4LoadER9BinStream
    .set _ZN13RndAnimatable4LoadER9BinStream, __hmx_native_noop_stub
    .weak _ZN13RndAnimatable4SaveER9BinStream
    .set _ZN13RndAnimatable4SaveER9BinStream, __hmx_native_noop_stub
    .weak _ZN13RndAnimatable6HandleEP9DataArrayb
    .set _ZN13RndAnimatable6HandleEP9DataArrayb, __hmx_native_noop_stub
    .weak _ZN13RndAnimatableC2Ev
    .set _ZN13RndAnimatableC2Ev, __hmx_native_noop_stub
    .weak _ZN14ProfilePicture13FetchUserDataEv
    .set _ZN14ProfilePicture13FetchUserDataEv, __hmx_native_noop_stub
    .weak _ZN14ProfilePicture15ReceiveUserDataEv
    .set _ZN14ProfilePicture15ReceiveUserDataEv, __hmx_native_noop_stub
    .weak _ZN14ProfilePicture16FetchUserPictureEv
    .set _ZN14ProfilePicture16FetchUserPictureEv, __hmx_native_noop_stub
    .weak _ZN14ProfilePicture18ReceiveUserPictureEv
    .set _ZN14ProfilePicture18ReceiveUserPictureEv, __hmx_native_noop_stub
    .weak _ZN14WiiCommerceMgr4InitEv
    .set _ZN14WiiCommerceMgr4InitEv, __hmx_native_noop_stub
    .weak _ZN15VirtualKeyboard12PlatformPollEv
    .set _ZN15VirtualKeyboard12PlatformPollEv, __hmx_native_noop_stub
    .weak _ZN15VirtualKeyboard14ShowKeyboardUIEPK9LocalUseri6StringS3_S3_ii
    .set _ZN15VirtualKeyboard14ShowKeyboardUIEPK9LocalUseri6StringS3_S3_ii, __hmx_native_noop_stub
    .weak _ZN15VirtualKeyboard17PlatformTerminateEv
    .set _ZN15VirtualKeyboard17PlatformTerminateEv, __hmx_native_noop_stub
    .weak _ZN16WiiNetworkSocket4InitEv
    .set _ZN16WiiNetworkSocket4InitEv, __hmx_native_noop_stub
    .weak _ZN18LightPresetManager4PollEv
    .set _ZN18LightPresetManager4PollEv, __hmx_native_noop_stub
    .weak _ZN18LightPresetManager5EnterEv
    .set _ZN18LightPresetManager5EnterEv, __hmx_native_noop_stub
    .weak _ZN3Hmx6Object7PreLoadER9BinStream
    .set _ZN3Hmx6Object7PreLoadER9BinStream, __hmx_native_noop_stub
    .weak _ZN5Synth10StopAllSfxEb
    .set _ZN5Synth10StopAllSfxEb, __hmx_native_noop_stub
    .weak _ZN5Synth15GetMasterVolumeEv
    .set _ZN5Synth15GetMasterVolumeEv, __hmx_native_noop_stub
    .weak _ZN5Synth15SetMasterVolumeEf
    .set _ZN5Synth15SetMasterVolumeEf, __hmx_native_noop_stub
    .weak _ZN7HDCache10WriteAsyncEiiPKv
    .set _ZN7HDCache10WriteAsyncEiiPKv, __hmx_native_noop_stub
    .weak _ZN7Memcard9TerminateEv
    .set _ZN7Memcard9TerminateEv, __hmx_native_noop_stub
    .weak _ZN8KeyChain6getKeyEiPhS0_
    .set _ZN8KeyChain6getKeyEiPhS0_, __hmx_native_noop_stub
    .weak _ZN9AsyncFile10WriteAsyncEPKvi
    .set _ZN9AsyncFile10WriteAsyncEPKvi, __hmx_native_noop_stub
    .weak _ZN9AsyncFile16UncompressedSizeEv
    .set _ZN9AsyncFile16UncompressedSizeEv, __hmx_native_noop_stub
    .weak _ZN9AsyncFile3EofEv
    .set _ZN9AsyncFile3EofEv, __hmx_native_noop_stub
    .weak _ZN9AsyncFile3NewEPKci
    .set _ZN9AsyncFile3NewEPKci, __hmx_native_noop_stub
    .weak _ZN9AsyncFile4FailEv
    .set _ZN9AsyncFile4FailEv, __hmx_native_noop_stub
    .weak _ZN9AsyncFile4ReadEPvi
    .set _ZN9AsyncFile4ReadEPvi, __hmx_native_noop_stub
    .weak _ZN9AsyncFile4SeekEii
    .set _ZN9AsyncFile4SeekEii, __hmx_native_noop_stub
    .weak _ZN9AsyncFile4SizeEv
    .set _ZN9AsyncFile4SizeEv, __hmx_native_noop_stub
    .weak _ZN9AsyncFile4TellEv
    .set _ZN9AsyncFile4TellEv, __hmx_native_noop_stub
    .weak _ZN9AsyncFile5FlushEv
    .set _ZN9AsyncFile5FlushEv, __hmx_native_noop_stub
    .weak _ZN9AsyncFile5WriteEPKvi
    .set _ZN9AsyncFile5WriteEPKvi, __hmx_native_noop_stub
    .weak _ZN9AsyncFile8ReadDoneERi
    .set _ZN9AsyncFile8ReadDoneERi, __hmx_native_noop_stub
    .weak _ZN9AsyncFile9ReadAsyncEPvi
    .set _ZN9AsyncFile9ReadAsyncEPvi, __hmx_native_noop_stub
    .weak _ZN9AsyncFile9TerminateEv
    .set _ZN9AsyncFile9TerminateEv, __hmx_native_noop_stub
    .weak _ZN9AsyncFile9WriteDoneERi
    .set _ZN9AsyncFile9WriteDoneERi, __hmx_native_noop_stub
    .weak _ZN9AsyncFileC2EPKci
    .set _ZN9AsyncFileC2EPKci, __hmx_native_noop_stub
    .weak _ZNK11PlatformMgr11GetOnlineIDEiP8OnlineID
    .set _ZNK11PlatformMgr11GetOnlineIDEiP8OnlineID, __hmx_native_noop_stub
    .weak _ZNK11PlatformMgr15GetOwnerOfGuestEi
    .set _ZNK11PlatformMgr15GetOwnerOfGuestEi, __hmx_native_noop_stub
    .weak _ZNK11PlatformMgr15IsUserAWiiGuestEPK9LocalUser
    .set _ZNK11PlatformMgr15IsUserAWiiGuestEPK9LocalUser, __hmx_native_noop_stub
    .weak _ZNK11PlatformMgr16IsSignedIntoLiveEi
    .set _ZNK11PlatformMgr16IsSignedIntoLiveEi, __hmx_native_noop_stub
    .weak _ZNK11PlatformMgr18HasOnlinePrivilegeEi
    .set _ZNK11PlatformMgr18HasOnlinePrivilegeEi, __hmx_native_noop_stub
    .weak _ZNK11PlatformMgr7GetNameEi
    .set _ZNK11PlatformMgr7GetNameEi, __hmx_native_noop_stub
    .weak _ZNK13WiiProfileMgr12IsIndexValidEi
    .set _ZNK13WiiProfileMgr12IsIndexValidEi, __hmx_native_noop_stub
    .weak _ZNK13WiiProfileMgr13GetIdForIndexEi
    .set _ZNK13WiiProfileMgr13GetIdForIndexEi, __hmx_native_noop_stub
    .weak _ZNK13WiiProfileMgr15GetIndexForUserEPK9LocalUser
    .set _ZNK13WiiProfileMgr15GetIndexForUserEPK9LocalUser, __hmx_native_noop_stub
    .weak _ZNK7SongMgr17GetSongsInContentE6SymbolRSt6vectorIiSaIiEE
    .set _ZNK7SongMgr17GetSongsInContentE6SymbolRSt6vectorIiSaIiEE, __hmx_native_noop_stub
    .weak _ZThn64_N11PlatformMgr11ContentDoneEv
    .set _ZThn64_N11PlatformMgr11ContentDoneEv, __hmx_native_noop_stub
    .weak _ZThn64_N11PlatformMgr14ContentStartedEv
    .set _ZThn64_N11PlatformMgr14ContentStartedEv, __hmx_native_noop_stub
    .weak _ZThn64_N11PlatformMgr16ContentCancelledEv
    .set _ZThn64_N11PlatformMgr16ContentCancelledEv, __hmx_native_noop_stub
    .weak _ZThn64_N11PlatformMgrD0Ev
    .set _ZThn64_N11PlatformMgrD0Ev, __hmx_native_noop_stub
    .weak _ZThn64_N11PlatformMgrD1Ev
    .set _ZThn64_N11PlatformMgrD1Ev, __hmx_native_noop_stub
    .weak _ZTI10WiiContent
    .set _ZTI10WiiContent, __hmx_native_noop_stub
    .weak _ZTI11RndPollable
    .set _ZTI11RndPollable, __hmx_native_noop_stub
    .weak _ZTI13RndAnimatable
    .set _ZTI13RndAnimatable, __hmx_native_noop_stub
    .weak _ZTI8WorldDir
    .set _ZTI8WorldDir, __hmx_native_noop_stub
    .weak _ZTI9AsyncFile
    .set _ZTI9AsyncFile, __hmx_native_noop_stub
    .weak _ZTT13RndAnimatable
    .set _ZTT13RndAnimatable, __hmx_native_noop_stub
    .weak _ZTv0_n104_N13RndAnimatable4LoadER9BinStream
    .set _ZTv0_n104_N13RndAnimatable4LoadER9BinStream, __hmx_native_noop_stub
    .weak _ZTv0_n24_N11PlatformMgrD0Ev
    .set _ZTv0_n24_N11PlatformMgrD0Ev, __hmx_native_noop_stub
    .weak _ZTv0_n24_N11PlatformMgrD1Ev
    .set _ZTv0_n24_N11PlatformMgrD1Ev, __hmx_native_noop_stub
    .weak _ZTv0_n72_N13RndAnimatable6HandleEP9DataArrayb
    .set _ZTv0_n72_N13RndAnimatable6HandleEP9DataArrayb, __hmx_native_noop_stub
    .weak _ZTv0_n80_N13RndAnimatable12SyncPropertyER8DataNodeP9DataArrayi6PropOp
    .set _ZTv0_n80_N13RndAnimatable12SyncPropertyER8DataNodeP9DataArrayi6PropOp, __hmx_native_noop_stub
    .weak _ZTv0_n88_N13RndAnimatable4SaveER9BinStream
    .set _ZTv0_n88_N13RndAnimatable4SaveER9BinStream, __hmx_native_noop_stub
    .weak _ZTv0_n96_N13RndAnimatable4CopyEPKN3Hmx6ObjectENS1_8CopyTypeE
    .set _ZTv0_n96_N13RndAnimatable4CopyEPKN3Hmx6ObjectENS1_8CopyTypeE, __hmx_native_noop_stub
    .weak _ZTV9AsyncFile
    .set _ZTV9AsyncFile, __hmx_native_noop_stub
