// Weak zero-filled DATA stubs for the WEB link — the wasm twin of the data
// section in band3_link_stubs.s (et al).
//
// rb3-native links the x86-GAS .s stub files, whose `.weak sym` + `.zero N`
// entries reserve BSS blobs for globals whose defining TU isn't in the port
// (Wii-side main.cpp, store/net managers, Wii-only RTTI, ...). Those .s files
// can't assemble under emcc, so the web link omitted them; the engine's
// missing_stubs.js + -sERROR_ON_UNDEFINED_SYMBOLS=0 safety net covers
// undefined FUNCTIONS (JS import stubs) but NOT data: wasm-ld silently
// resolves an undefined DATA symbol to address 0. Every one of these globals
// therefore aliased address 0 on web — e.g. App::App's `gInitComplete = false`
// was a 1-byte NULL write (ASan: null-pointer-dereference WRITE of size 1 at
// 0x0 in App::App), and on ASan builds reads of the aliased pointers returned
// shadow-gap poison, causing chaotic follow-on faults (a layout-sensitive
// abort in musl pop_arg via a garbage indirect call).
//
// Each entry is a weak, 16-byte-aligned (.p2align 4 twin), zero-initialised
// blob under the exact linker symbol name (all are valid C identifiers, so
// extern "C" is enough — no asm labels needed). Weak matches the .s
// semantics: a real definition anywhere in the web link wins; the blob only
// backstops symbols that would otherwise land at 0.
//
// GENERATED from the .zero entries in native/src/*.s — if you add a data stub
// there for a new native link error, mirror it here for web.

extern "C" {
__attribute__((weak, aligned(16))) char TheAccomplishmentMgr[256] = {};
__attribute__((weak, aligned(16))) char TheGameConfig[256] = {};
__attribute__((weak, aligned(16))) char TheInputMgr[256] = {};
__attribute__((weak, aligned(16))) char TheMemcardMgr[256] = {};
__attribute__((weak, aligned(16))) char TheMusicLibrary[256] = {};
__attribute__((weak, aligned(16))) char TheNet[256] = {};
__attribute__((weak, aligned(16))) char TheNetMessageFactory[256] = {};
__attribute__((weak, aligned(16))) char TheNetSession[256] = {};
__attribute__((weak, aligned(16))) char TheNgStats[256] = {};
__attribute__((weak, aligned(16))) char TheRockCentral[4096] = {};
__attribute__((weak, aligned(16))) char TheSaveLoadMgr[256] = {};
__attribute__((weak, aligned(16))) char TheServer[256] = {};
__attribute__((weak, aligned(16))) char TheSessionMgr[256] = {};
__attribute__((weak, aligned(16))) char TheSongDB[256] = {};
__attribute__((weak, aligned(16))) char TheSplasher[256] = {};
__attribute__((weak, aligned(16))) char TheStoreMetadata[256] = {};
__attribute__((weak, aligned(16))) char TheSyncStore[256] = {};
__attribute__((weak, aligned(16))) char TheTour[256] = {};
__attribute__((weak, aligned(16))) char TheVoiceChatMgr[256] = {};
__attribute__((weak, aligned(16))) char TheWiiContentMgr[256] = {};
__attribute__((weak, aligned(16))) char TheWiiFX[256] = {};
__attribute__((weak, aligned(16))) char TheWiiFriendMgr[256] = {};
__attribute__((weak, aligned(16))) char TheWiiMessenger[256] = {};
__attribute__((weak, aligned(16))) char _ZN11BandCamShot22sHideAllCharactersHackE[256] = {};
__attribute__((weak, aligned(16))) char _ZN14WiiCommerceMgr7mOpNameE[256] = {};
__attribute__((weak, aligned(16))) char _ZN15SongUpgradeData8sSaveVerE[256] = {};
__attribute__((weak, aligned(16))) char _ZN20StoreMetadataManager14mSetlistOffersE[256] = {};
__attribute__((weak, aligned(16))) char _ZN6WiiRnd14mShowAssetNameE[256] = {};
__attribute__((weak, aligned(16))) char _ZN9MetaPanel10sUnlockAllE[256] = {};
__attribute__((weak, aligned(16))) char _ZN9MetaPanel11sIsPlaytestE[256] = {};
__attribute__((weak, aligned(16))) char _ZN9MetaPanel21sLaunchedGoalMsgsOnlyE[256] = {};
__attribute__((weak, aligned(16))) char _ZTI10FileMerger[256] = {};
__attribute__((weak, aligned(16))) char _ZTI10MidiParser[256] = {};
__attribute__((weak, aligned(16))) char _ZTI11SetlistSort[256] = {};
__attribute__((weak, aligned(16))) char _ZTI12OutfitConfig[256] = {};
__attribute__((weak, aligned(16))) char _ZTI12SavedSetlist[256] = {};
__attribute__((weak, aligned(16))) char _ZTI12WiiMultiMesh[256] = {};
__attribute__((weak, aligned(16))) char _ZTI18TourPerformerLocal[256] = {};
__attribute__((weak, aligned(16))) char _ZTI33AccomplishmentDiscSongConditional[256] = {};
__attribute__((weak, aligned(16))) char _ZTI8AppLabel[256] = {};
__attribute__((weak, aligned(16))) char _ZTI8CharHair[256] = {};
__attribute__((weak, aligned(16))) char _ZTI8NodeSort[256] = {};
__attribute__((weak, aligned(16))) char _ZTI8SongSort[256] = {};
__attribute__((weak, aligned(16))) char _ZTV11SetlistSort[256] = {};
__attribute__((weak, aligned(16))) char _ZTV12SavedSetlist[256] = {};
__attribute__((weak, aligned(16))) char _ZTV14SongSortByDiff[256] = {};
__attribute__((weak, aligned(16))) char _ZTV19MatchmakingSettings[256] = {};
__attribute__((weak, aligned(16))) char _ZTV8NodeSort[256] = {};
__attribute__((weak, aligned(16))) char _ZTV8SongSort[256] = {};
__attribute__((weak, aligned(16))) char _ZTVN6Quazal12RBDataClientE[256] = {};
__attribute__((weak, aligned(16))) char _ZTVN6Quazal12RBTestClientE[256] = {};
__attribute__((weak, aligned(16))) char _ZTVN6Quazal14ClientProtocolE[256] = {};
__attribute__((weak, aligned(16))) char _ZTVN6Quazal18RBBinaryDataClientE[256] = {};
__attribute__((weak, aligned(16))) char gCNTThreadInUse[256] = {};
__attribute__((weak, aligned(16))) char gInitComplete[256] = {};
__attribute__((weak, aligned(16))) char gLastErrorDesc[256] = {};
__attribute__((weak, aligned(16))) char gLastErrorReturnValue[256] = {};
__attribute__((weak, aligned(16))) char gRenderTextureSet[256] = {};
__attribute__((weak, aligned(16))) char gStoreMetadataManagerLoadStepName[256] = {};
__attribute__((weak, aligned(16))) char gStoreUIOverlay[256] = {};
__attribute__((weak, aligned(16))) char kInvalidPitch__11VocalPlayer[256] = {};
} // extern "C"
