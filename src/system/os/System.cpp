#include "System.h"

#include "math/FileChecksum.h"
#include "math/Geo.h"
#include "math/Rand.h"
#include "math/Trig.h"
#include "obj/Data.h"
#include "obj/DataFile.h"
#include "obj/DataFunc.h"
#include "obj/DataUtl.h"
#include "obj/Dir.h"
#include "obj/Task.h"
#include "os/AppChild.h"
#include "os/Archive.h"
#include "os/ContentMgr.h"
#include "os/DateTime.h"
#include "os/Debug.h"
#include "os/File.h"
#include "os/FileCache.h"
#include "os/HolmesClient.h"
#include "os/Joypad.h"
#include "os/JoypadClient.h"
#include "os/Keyboard.h"
#include "os/Memcard_Wii.h"
#include "os/NetworkSocket.h"
#include "os/CommerceMgr_Wii.h"
#include "os/PlatformMgr.h"
#include "os/ThreadCall.h"
#include "os/Timer.h"
#include "os/VirtualKeyboard.h"
#include "utl/CacheMgr.h"
#include "utl/GlitchFinder.h"
#include "utl/Locale.h"
#include "utl/Loader.h"
#include "utl/TimeConversion.h"
#include "utl/MemMgr.h"
#include "utl/NetCacheMgr.h"
#include "utl/Option.h"
#include "utl/Spew.h"
#include "utl/Str.h"
#include "utl/Symbols.h"

#include <cstring>
#include <cstdio>
#include <vector>

void ArchiveInit();
void CheatsInit();
void CheatsTerminate();
void GeoInit();
void JoypadInit();
void JoypadTerminate();
void MemInit();
void MemTerminate();
DataNode ResetHWM(DataArray *);
DataNode CycleMemConsistencyCheck(DataArray *);
bool InitWiiRSO();
bool RsoInit(const char *);
u32 HolmesClientSysExec(const char *);
void HolmesClientStackTrace(const char *, unsigned int *, int, String &);
void GetMapFileName(String &);

#define kRSOBufferSize 0x10EC00
#define kDefaultRSOBufferSize 0x89460

extern bool (*ParseStack)(const char *, unsigned int *, int, char *);


const char *gNullStr = "";

static Symbol gSystemLanguage;
static DataArray *gSystemConfig;
static DataArray *gSystemTitles;

static int gUsingCD;
static GfxMode gGfxMode;

static int gSystemMs;
static float gSystemFrac;
static Timer gSystemTimer;
static bool gNetUseTimedSleep;

std::vector<char *> TheSystemArgs;
const char *gHostFile;

unsigned char *g_pRSOReserveBuf;
unsigned char *g_pDefaultRSOBuf;

DECOMP_FORCEACTIVE(System, "_unresolved func.\n", "gen/main_%s.hdr")

namespace {
    bool gHasPreconfig = true;
    bool gPreconfigOverride;

    bool CheckForArchive() {
        SetUsingCD(true);
        FileStat stat;
        if (FileGetStat(
                MakeString("gen/main_%s.hdr", PlatformSymbol(TheLoadMgr.GetPlatform())),
                &stat
            )
            < 0) {
            SetUsingCD(false);
        }
    }
}

bool gHostConfig;
bool gHostLogging;
bool gHostCached;

void SetGfxMode(GfxMode mode) {
    gGfxMode = mode;
    HolmesClientReInit();
    DataVariable("gfx_mode") = DataNode((int)mode);
}

GfxMode GetGfxMode() { return gGfxMode; }

DataNode OnSystemLanguage(DataArray *da) { return DataNode(gSystemLanguage); }

DataNode OnSystemExec(DataArray *da) { return DataNode(SystemExec(da->Str(1))); }

DataNode OnUsingCD(DataArray *da) { return DataNode(gUsingCD != 0); }

DataNode OnSupportedLanguages(DataArray *da) {
    return DataNode(SupportedLanguages(false), kDataArray);
}

DataNode OnSystemMs(DataArray *da) { return DataNode(SystemMs()); }

DataNode OnSwitchSystemLanguage(DataArray *da) {
    DataArray *languages = SupportedLanguages(true);

    int i;
    for (i = 0; i < languages->Size(); i++) {
        if (gSystemLanguage == languages->Sym(i)) {
            break;
        }
    }

    i = (i + 1) % languages->Size();
    SetSystemLanguage(languages->Sym(i), true);
    return DataNode(1);
}

DECOMP_FORCEACTIVE(
    System,
    "LanguageInit called, but region has not been initialized",
    "language",
    "system"
)

void LanguageInit() {
    if (ThePlatformMgr.GetRegion() == kRegionNone) {
        MILO_WARN("LanguageInit called, but region has not been initialized");
    }

    // TODO: SystemConfig inlines here; retail confirms its usage
    DataArray *languageConfig = SystemConfig("system", "language");
    Symbol language = GetSystemLanguage("eng");

    DataArray *remap = languageConfig->FindArray("remap", false);
    if (remap != NULL) {
        remap->FindData(language, language, false);
    }

    Symbol force;
    if (languageConfig->FindData("force", force, false) && force != "") {
        language = force;
    }

    const char *languageOption = OptionStr("lang", NULL);
    if (languageOption != NULL) {
        language = languageOption;
    }

    languageOption = OptionStr("language", NULL);
    if (languageOption != NULL) {
        language = languageOption;
    }

    SetSystemLanguage(language, false);
}

Symbol PlatformSymbol(Platform pform) {
    static Symbol sym[6] = { gNullStr, gNullStr, "xbox", "pc", "ps3", "wii" };
    return sym[pform];
}

bool PlatformLittleEndian(Platform p) {
    MILO_ASSERT(p != kPlatformNone, 0x135);
    bool ret = false;
    if (p == kPlatformPC || p == kPlatformNone)
        ret = true;
    return ret;
}

Platform ConsolePlatform() { return kPlatformWii; }

static bool gReadingSystemConfig;

DataArray *ReadSystemConfig(const char *path) {
    gReadingSystemConfig = true;
    DataArray *config = DataReadFile(path, true);
    gReadingSystemConfig = false;
    return config;
}

void InitSystem(const char *);

void PreInitSystem(const char *path) {
    Archive *archive = TheArchive;
    bool usingCD = gUsingCD != 0;

    if (gHostConfig) {
        SetUsingCD(false);
        TheArchive = NULL;
    }

    DataArrayPtr root(1);

    DataSetMacro("HX_WII", root.mData);
    const char *macro;
    while ((macro = OptionStr("define", NULL)) != NULL) {
        DataSetMacro(macro, root.mData);
    }

    const char *config = OptionStr("config", NULL);
    if (config != NULL && !gHasPreconfig) {
        path = config;
    }

    BeginDataRead();
    gSystemConfig = ReadSystemConfig(path);
    MILO_ASSERT(gSystemConfig, 0x1AC);
    DataVariable("syscfg") = DataNode(gSystemConfig, kDataArray);

    DataArray *mem = gSystemConfig->FindArray("mem");

    SetUsingCD(usingCD);
    TheArchive = archive;

    DataRegisterFunc("system_language", OnSystemLanguage);
    DataRegisterFunc("system_exec", OnSystemExec);
    DataRegisterFunc("using_cd", OnUsingCD);
    DataRegisterFunc("supported_languages", OnSupportedLanguages);
    DataRegisterFunc("switch_system_language", OnSwitchSystemLanguage);
    DataRegisterFunc("system_ms", OnSystemMs);

    ThePlatformMgr.mEnableSFX = OptionBool("disable_sfx", false) == 0;
    SetGfxMode(kOldGfx);

    if (config != NULL && gHasPreconfig) {
        InitSystem(config);
        gPreconfigOverride = true;
    }
}

void StripEditorData() {
    Symbol editor("editor");
    Symbol types("types");
    DataArray *objectsCfg = SystemConfig("objects");
    for (int i = 1; i < objectsCfg->Size(); i++) {
        DataArray *objectsArr = objectsCfg->Array(i);
        DataArray *objEditorArr = objectsArr->FindArray(editor, false);
        if (objEditorArr != 0)
            objEditorArr->Resize(1);
        DataArray *typesArr = objectsArr->FindArray(types, false);
        if (typesArr != 0) {
            for (int j = 1; j < typesArr->Size(); j++) {
                DataArray *typesEditorArr = typesArr->Array(j)->FindArray(editor, false);
                if (typesEditorArr != 0)
                    typesEditorArr->Resize(1);
            }
        }
    }
}

void InitSystem(const char *path) {
    if (!gPreconfigOverride && path != NULL) {
        bool usingCD = gUsingCD != 0;
        Archive *archive = TheArchive;

        if (gHostConfig) {
            SetUsingCD(false);
            TheArchive = NULL;
        }

        DataArray *systemConfig = ReadSystemConfig(path);
        MILO_ASSERT(systemConfig, 0x22C);
        DataMergeTags(systemConfig, gSystemConfig);
        DataReplaceTags(systemConfig, gSystemConfig);
        gSystemConfig->Release();
        gSystemConfig = systemConfig;
        DataVariable("syscfg") = DataNode(gSystemConfig, kDataArray);

        SetUsingCD(usingCD);
        TheArchive = archive;
        StripEditorData();
    }

    // why is this split between here and PreInitSystem lol
    FinishDataRead();
}

void SystemTerminate() {
    TheDebug.RemoveExitCallback(SystemTerminate);
    TheVirtualKeyboard.Terminate();
    CacheMgrTerminate();
    NetCacheMgrTerminate();
    FileCache::Terminate();
    TheLocale.Terminate();
    TheMC.Terminate();
    CheatsTerminate();
    KeyboardTerminate();
    JoypadTerminate();
    SpewTerminate();
    ThreadCallTerminate();
    TheTaskMgr.Terminate();
    ObjectDir::Terminate();
    TheContentMgr->Terminate();
    TrigTableTerminate();
    FileTerminate();
    gSystemConfig->Release();
    DataTerminate();
    Symbol::Terminate();
    MemTerminate();
    AppChild::Terminate();
    TheSystemArgs.clear();
    TerminateMakeString();
}

bool InitWiiRSO() {
    String s(TheSystemArgs[0]);
    s.replace(s.find(".elf"), sizeof(".elf") - 1, ".sel");
    return RsoInit(s.c_str());
}

void SystemPreInit(const char *config) {
#ifdef HX_NATIVE
    // Native (clang LP64) curated PreInit, modeled on DC3's HX_NATIVE SystemPreInit.
    // Skips the Wii-only machinery that is fatal or meaningless on the host:
    //   - the RSO reserve buffers + their `<= 0x91000000` Wii-memory-map asserts
    //     (host malloc returns high addresses → the assert would always fail)
    //   - MemInit (the Wii heap mgr uses 32-bit pointer arithmetic)
    //   - WiiNetworkSocket::Init / ThePlatformMgr.PreInit / TheContentMgr->PreInit
    //     (TheContentMgr is a no-op link stub here — calling a virtual on it is UB)
    //   - CheckForArchive / ArchiveInit: RB3 native loads loose extracted files,
    //     not the .ark (UsingCD() controls .dta -> gen/.dtb rewrite, see below).
    // KEEPS DataInit() (→ ObjectDir::PreInit sets sMainDir + DataSetThis, the dir
    // context the boot-script {func} directives need) and PreInitSystem(config)
    // (loads band_preinit_keep.dta into gSystemConfig).
    InitMakeString();
    if (!gStringTable) {
        Symbol::PreInit(600000, 75000);
    }
    // RB3 native loads from a loose extracted tree (not the .ark). For the
    // existing pre-flattened extract at `orig-assets/extracted/` the engine
    // reads flat `config/<name>.dta` text directly, so leave UsingCD off
    // (CachedDataFile rewrites logical .dta to gen/*.dtb only when
    // UsingCD()&&!FileIsLocal(); see DataFile.cpp:620). The full arkhelper
    // extract at `orig-assets/extracted-xbox-full/` has raw binary .dtb under
    // gen/ but the bytes appear to be missing the HMX encryption wrapper —
    // attempting to read them via the cached-stream path fails on the magic
    // byte check. Until that asset-shape issue is resolved (likely needs
    // dtab-converted dta text, or a different arkhelper output mode), keep
    // the old-extract path here. Audio files (.mogg/.mid) from the full
    // extract can be merged into the old layout via symlink/copy.
    SetUsingCD(false);
    ThePlatformMgr.RegionInit();
    OptionInit();
    TimeConversionInit();
    Timer::Init();
    FileInit();
    AppChild::Init();
    DateTimeInit();
    DateTime dt;
    GetDateAndTime(dt);
    SeedRand(dt.mSec + dt.mMin * 60 + dt.mHour * 3600);
    srand(RandomInt());
    TheDebug.Init();
    DataInit();
    // DataInit()->ObjectDir::PreInit() only enables DirLoader cache-mode when
    // UsingCD() — i.e. running off the Wii disc, where logical `foo/bar.milo`
    // paths must be rewritten to the extracted `foo/gen/bar.milo_<plat>` form.
    // Native loads the SAME on-disc-shaped extracted tree (every milo lives under
    // a sibling `gen/` as `*.milo_<plat>`), but with SetUsingCD(false) cache-mode
    // would stay off and logical milo loads (ui/meta_panel.milo, …) would fail to
    // resolve to their gen/*.milo_xbox files. Force it on for the native load
    // path — same effect as the Wii UsingCD() boot branch (Dir.cpp:741).
    DirLoader::SetCacheMode(true);
    PreInitSystem(config);
    LanguageInit();
    TheLoadMgr.Init();
    JoypadInit();
    KeyboardInit();
    AutoTimer::Init();
    ThreadCallPreInit();
    TheTaskMgr.Init();
    TheDebug.AddExitCallback(SystemTerminate);
#else
    MemInit();
    g_pRSOReserveBuf = (unsigned char *)_MemAlloc(kRSOBufferSize, 0x20);
    MILO_ASSERT((char*)g_pRSOReserveBuf + kRSOBufferSize <= (char*)0x91000000, 0x2C8);
    g_pDefaultRSOBuf = (unsigned char *)_MemAlloc(kDefaultRSOBufferSize, 0x20);
    MILO_ASSERT(
        (char*)g_pDefaultRSOBuf + kDefaultRSOBufferSize <= (char*)0x91000000, 0x2CC
    );
    InitMakeString();
    if (!gStringTable) {
        Symbol::PreInit(600000, 75000);
    }
    if (OptionBool("force_ark", false)) {
        SetUsingCD(true);
    }
    if (OptionBool("force_cd", true)) {
        CheckForArchive();
    }
    ThePlatformMgr.RegionInit();
    TheContentMgr->PreInit();
    OptionInit();
    if (OptionBool("no_checksum", false)) {
        ClearFileChecksumData();
    }
    TimeConversionInit();
    Timer::Init();
    gHostConfig = OptionBool("host_config", false);
    gHostLogging = OptionBool("host_logging", false);
    gHostFile = OptionStr("host_file", NULL);
    if (gHostFile != NULL) {
        gHostConfig = true;
    }
    gHostCached = OptionBool("host_cached", false);
    if (gUsingCD == 0 || gHostConfig || gHostLogging) {
        WiiNetworkSocket::Init();
    }
    FileInit();
    AppChild::Init();
    DateTimeInit();
    DateTime dt;
    GetDateAndTime(dt);
    SeedRand(dt.mSec + dt.mMin * 60 + dt.mHour * 3600);
    srand(RandomInt());
    ArchiveInit();
    ThePlatformMgr.PreInit();
    TheDebug.Init();
    String commandLine;
    for (unsigned int i = 0; i < TheSystemArgs.size(); ++i) {
        commandLine += ' ';
        commandLine += TheSystemArgs[i];
    }
    MILO_LOG("SystemInit Params:%s\n", commandLine);
    DataInit();
    PreInitSystem(config);
    LanguageInit();
    TheLoadMgr.Init();
    JoypadInit();
    KeyboardInit();
    AutoTimer::Init();
    ThreadCallPreInit();
    TheTaskMgr.Init();
    TheDebug.AddExitCallback(SystemTerminate);
#endif
}

#ifdef HX_NATIVE
// 3-arg SystemPreInit: the real definition lives in System_Wii.cpp (a thin Wii
// wrapper — InitGQR + SetSystemArgs + the 1-arg SystemPreInit + AutoHangHelper),
// but that TU is platform-excluded from the native link, so the App ctor's
// `SystemPreInit(argc, argv, "config/...")` call would otherwise resolve to a
// weak no-op stub and never create ObjectDir::sMainDir (Rnd::PreInit's first
// SetName("rnd", sMainDir) then asserts on a null dir). Provide the native
// equivalent: drop the Wii GQR/hang machinery, keep SetSystemArgs + the curated
// 1-arg SystemPreInit. Mirrors the System_Wii wrapper shape.
void SystemPreInit(int argc, char **argv, const char *preinit) {
    SetSystemArgs(argc, argv);
    SystemPreInit(preinit);
}
#endif

void NormalizeSystemArgs() {
    for (unsigned int i = 0; i < TheSystemArgs.size(); i++) {
        char *p = TheSystemArgs[i];
        while (*p != '\0') {
            if (*p == (char)0x96) {
                *p = '-';
            }
            if (*p == (char)0x93 || *p == (char)0x94) {
                *p = '"';
            }
            p++;
        }
    }
}

void SetSystemArgs(int argc, char **argv) {
    TheSystemArgs.reserve(argc);
    TheSystemArgs.erase(TheSystemArgs.begin(), TheSystemArgs.end());
    for (int i = 0; i < argc; i++) {
        TheSystemArgs.push_back(argv[i]);
    }
    NormalizeSystemArgs();
}

int SystemMs() {
    gSystemTimer.Restart();
    float lastMs = gSystemTimer.GetLastMs();
    int ms = gSystemFrac + lastMs;
    gSystemFrac = (gSystemFrac + lastMs) - ms;
    gSystemMs += ms;
    return gSystemMs;
}

bool UsingCD() { return gUsingCD != 0; }

void SetUsingCD(bool b) { gUsingCD = b; }

DataArray *SystemConfig() { return gSystemConfig; }

static DataArray *GetSystemConfigWith3Syms(Symbol s1, Symbol s2, Symbol s3) {
    return SystemConfig(s1, s2, s3);
}

#pragma push
#pragma force_active on
inline DataArray *SystemConfig(Symbol s) { return gSystemConfig->FindArray(s); }

inline DataArray *SystemConfig(Symbol s1, Symbol s2) {
    return gSystemConfig->FindArray(s1)->FindArray(s2);
}
inline DataArray *SystemConfig(Symbol s1, Symbol s2, Symbol s3) {
    return gSystemConfig->FindArray(s1)->FindArray(s2)->FindArray(s3);
}
#pragma pop

DataArray *SystemConfig(Symbol s1, Symbol s2, Symbol s3, Symbol s4) {
    return gSystemConfig->FindArray(s1)->FindArray(s2)->FindArray(s3)->FindArray(s4);
}

DataArray *SystemConfig(Symbol s1, Symbol s2, Symbol s3, Symbol s4, Symbol s5) {
    return gSystemConfig->FindArray(s1)
        ->FindArray(s2)
        ->FindArray(s3)
        ->FindArray(s4)
        ->FindArray(s5);
}

Symbol SystemLanguage() { return gSystemLanguage; }

DataArray *SupportedLanguages(bool b) {
    static Symbol system("system");
    return SystemConfig(system, language, b ? cheat_supported : supported)->Array(1);
}

bool IsSupportedLanguage(Symbol s, bool b) {
    DataArray *languages = SupportedLanguages(b);
    for (int i = 0; i < languages->Size(); i++) {
        if (languages->Sym(i) == s)
            return true;
    }
    return false;
}

int SystemExec(const char *args) {
    if (gUsingCD)
        return -1;
    else
        return HolmesClientSysExec(args);
}

#pragma pool_data off
void SetSystemLanguage(Symbol lang, bool cheats) {
    if (!IsSupportedLanguage(lang, cheats)) {
        static Symbol system("system");
        static Symbol default_sym("default");

        DataArray *arr = gSystemConfig->FindArray(system, true)->FindArray(language, true);
        arr = arr->FindArray(default_sym, false);
        if (arr != 0) {
            Symbol arrLang = arr->Node(1).Sym(arr);
            if (IsSupportedLanguage(arrLang, cheats)) {
                lang = arrLang;
            } else {
                MILO_WARN(
                    "Both %s and the default language (%s) are not supported!\n",
                    lang,
                    arrLang
                );
                return;
            }
        } else {
            MILO_WARN(
                "Language %s is not supported, and there is no default language found!\n",
                lang
            );
            return;
        }
    }

    if (lang != gSystemLanguage) {
        TheLocale.Terminate();
        gSystemLanguage = lang;
        TheLocale.Init();
    } else {
        gSystemLanguage = lang;
    }
}
#pragma pool_data on

void AppendStackTrace(char *buf) {
    unsigned int trace[50];
    memset(trace, 0, 200);
    CaptureStackTrace(50, trace);
    unsigned int *traceBase = trace;
    int idx = 0;
    unsigned int *tracePtr = traceBase;
    while (idx < 50 && *tracePtr) {
        tracePtr++;
        idx++;
    }
    String mapName;
    GetMapFileName(mapName);
    strcat(buf, "Stack Trace: \r\n");
    bool parse;
    if (gUsingCD || FileIsLocal(mapName.c_str())) {
        if (TheArchive && TheArchive->mIsPatched) {
            parse = false;
        } else {
            parse = ParseStack(mapName.c_str(), traceBase, idx, buf);
        }
    } else {
        String holmesStr;
        HolmesClientStackTrace(mapName.c_str(), traceBase, idx, holmesStr);
        strcat(buf, holmesStr.c_str());
        parse = !holmesStr.empty();
    }
    if (!parse) {
        strcat(buf, " (map file unavailable)");
        for (int i = 0; i < idx; i++) {
            strcat(buf, "\n   ");
            sprintf(buf + strlen(buf), "%08x", traceBase[i]);
        }
    }
    strcat(buf, "\r\n");
}

void AppendThreadStackTrace(char *buf, unsigned int *stack) {
    strcat(buf, "\n\n-- Thread failure, no stack yet --");
    int idx = 0;
    unsigned int *stackPtr = stack;
    while (idx < 50 && *stackPtr) {
        stackPtr++;
        idx++;
    }
    unsigned int *ptr = stack;
    strcat(buf, " (map file unavailable)");
    while ((ptr - stack) < idx) {
        strcat(buf, "\n   ");
        auto _tmp0 = strlen(buf);
        sprintf(buf + _tmp0, "%08x", *ptr);
        ptr++;
    }
}

void SystemPoll(bool b1) {
    Timer::ClearSlowFrame();
    SystemMs();
    TheDebug.Poll();
#ifndef HX_NATIVE
    TheMC.Poll();
#endif
    JoypadPoll();
    JoypadClientPoll();
    KeyboardPoll();
    ThreadCallPoll();
    FileCache::PollAll();
    TheLoadMgr.Poll();
#ifndef HX_NATIVE
    // TheCacheMgr/TheNetCacheMgr/ThePlatformMgr/TheVirtualKeyboard/TheContentMgr
    // are Wii/online managers; on native they are no-op link stubs (not real
    // singletons), so polling them would deref junk. They are not initialized on
    // the native boot path (see SystemInit HX_NATIVE branch).
    TheCacheMgr->Poll();
    TheNetCacheMgr->Poll();
#endif
    if (TheAppChild) TheAppChild->Poll();
    if (b1) TheTaskMgr.Poll();
    if (!gUsingCD) HolmesClientPoll();
#ifndef HX_NATIVE
    ThePlatformMgr.Poll();
    TheVirtualKeyboard.Poll();
    TheContentMgr->PollRefresh();
    ThePlatformMgr.WiiPoll();
#endif
}

void SystemInit(const char *config) {
    gSystemTimer.Start();
#ifdef HX_NATIVE
    // Native (clang LP64) curated Init, modeled on DC3's HX_NATIVE SystemInit.
    // Skips: InitWiiRSO/MILO_FAIL, WiiNetworkSocket, TheMC (memcard), CacheMgr /
    // NetCacheMgr / ThePlatformMgr / TheWiiCommerceMgr / TheVirtualKeyboard /
    // GlitchFinder (Wii/online managers), and TheContentMgr->Init() (TheContentMgr
    // is a no-op link stub here — calling a virtual on it is UB; the content_mgr
    // DTA stub is registered by the harness instead).
    // KEEPS InitSystem(config) — the band_keep.dta load that populates
    // gSystemConfig with the (objects ...) type-defs property-sync needs (this is
    // what completes critical-path Step 1's full object-graph load). sMainDir was
    // already created by DataInit()->ObjectDir::PreInit() in SystemPreInit, so the
    // {func} directives in the merged object configs have a valid dir context.
    Symbol::Init();
    InitSystem(config);
    gSystemTitles = SystemConfig("system", "titles");
    ObjectDir::Init();
    TrigTableInit();
    ThreadCallInit();
    GeoInit();
    TrigInit();
    SpewInit();
    TheLocale.Terminate();
    TheLocale.Init();
    FileCache::Init();
    // CheatsInit() skipped on native: RB3's cheats.dta binds punctuation keyboard
    // keys (`.` `/` `?`) that the flex lexer parses as the float 0.0, so
    // InitKeyCheats' `cheat->Str(0)` key read fatals ("Data 0.00 is not String").
    // Cheats are irrelevant to boot/render; the DTA funcs it registers
    // (set_cheat_mode, …) resolve to the DataFunc not-found warn-guard if used.
    DataRegisterFunc("reset_hwm", ResetHWM);
    DataRegisterFunc("cycle_mem_consistency_check", CycleMemConsistencyCheck);
    // Bring up the Wii/online manager globals whose ctors live in EXCLUDED Wii
    // TUs (PlatformMgr_Wii/ContentMgr_Wii). Their objects are constructed natively
    // (ThePlatformMgr via rb3_platform_native.cpp's PlatformMgr ctor; TheContentMgr
    // = new base ContentMgr there too). Run their Init() here — the real game's
    // SystemInit slot for both — now that Main()/sMainDir exists, so they
    // SetName("platform_mgr"/"content_mgr", Main()) and DTA resolves them. The
    // base ContentMgr::Init sets mState=kDone (NeverRefreshed→true, offline-safe).
    // PlatformMgr::Init's Wii body (JoypadSubscribe/TheHttpWii/SOHeapInit) is
    // weak-stubbed to a no-op on native; we just need the SetName, so call the
    // SetName directly to avoid the stubbed Wii Init side-effects.
    ThePlatformMgr.SetName("platform_mgr", ObjectDir::sMainDir);
    TheContentMgr->Init();
    // Native has no disc/SD content-discovery refresh (the Wii ContentMgr's
    // StartRefresh/PollRefresh scan loop lives in the excluded ContentMgr_Wii TU,
    // and there is no disc). On console that refresh runs at boot and settles at
    // mState=kDiscoveryEnumerating (the post-refresh idle state — RefreshDone()
    // true, RefreshInProgress()/InDiscoveryState() false). Songs are loaded
    // directly via BandSongMgr::AddSongs natively, so the content set is already
    // "discovered"; put the ContentMgr in that settled state so the screens that
    // gate IsLoaded() on TheContentMgr->RefreshDone() (e.g. SelectDifficultyPanel,
    // the part_difficulty screen — the meta->game gate) complete their load. The
    // base ContentMgr::Init left it at kDone (NeverRefreshed→true) which never
    // advances natively, hanging part_difficulty_screen forever in transition.
    TheContentMgr->mState = ContentMgr::kDiscoveryEnumerating;
    // TheNetSession (`session`): the concrete impl + global live in the excluded
    // network/ subsystem, so construct a minimal offline native session here
    // (rb3_netsession_native.cpp) — referenced pervasively (LockStepMgr ctor,
    // NetSync/SessionMgr/BandUI Init, many panels) so we build the real global
    // rather than gate each site. SystemConfig("net","session") is available now.
    extern void RB3InitNativeNetSession();
    RB3InitNativeNetSession();
    // NOTE: SystemTerminate is already registered in SystemPreInit (line ~401).
    // Re-registering here would push it to the head of mExitCallbacks (push_front),
    // ahead of SynthTerminate, so it would run FIRST — tearing down TheTaskMgr
    // (mTimelines = nullptr) before Synth's exit callback calls Synth::Poll(),
    // which in turn calls TheTaskMgr.Seconds() via RandomIntervalGroupSeqInst::Poll
    // and crashes on the null mTimelines. The non-HX_NATIVE branch below
    // (SystemInit's #else) does NOT register SystemTerminate — only SystemPreInit
    // does. Stay consistent with that for the teardown order to be correct.
#else
    if (!InitWiiRSO()) MILO_FAIL("_unresolved func.\n");
    if (gUsingCD && !gHostConfig && !gHostLogging) WiiNetworkSocket::Init();
    Symbol::Init();
    InitSystem(config);
    gSystemTitles = SystemConfig("system", "titles");
    ObjectDir::Init();
    TrigTableInit();
    ThreadCallInit();
    GeoInit();
    TrigInit();
    SpewInit();
    TheLocale.Terminate();
    TheLocale.Init();
    TheMC.Init();
    FileCache::Init();
    CacheMgrInit();
    NetCacheMgrInit();
    ThePlatformMgr.Init();
    TheWiiCommerceMgr.Init();
    TheVirtualKeyboard.Init();
    TheContentMgr->Init();
    GlitchFinder::Init();
    CheatsInit();
    DataRegisterFunc("reset_hwm", ResetHWM);
    DataRegisterFunc("cycle_mem_consistency_check", CycleMemConsistencyCheck);
#endif
}
