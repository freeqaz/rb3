#include "utl/Loader.h"
#include "Std.h"
#include "os/Archive.h"
#include "os/Debug.h"
#include "os/File.h"
#include "os/System.h"
#include "utl/BinStream.h"
#include "utl/MemMgr.h"
#include "utl/Option.h"
#include "obj/DataFunc.h"
#include "rndwii/Rnd.h"

#include "decomp.h"

#ifdef HX_WEB
#include <emscripten/emscripten.h> // emscripten_sleep — JSPI yield in PollUntilLoaded

// --- Boot I/O instrumentation (Step 0, handoff 02-boot-sync-read) ----------
// Per-PATH JSPI-yield counters so we can tell whether the App-ctor load chain
// drives the throttled Poll() yield (Loader.cpp ~:405) or the per-slice
// PollUntilLoaded yield (~:219). native_file.cpp's RB3BootIoStatsDump reads
// these and prints them at appctor_done (gated on RB3_BOOT_IO_STATS). These are
// plain globals (web build is single-threaded); the ms accumulation only runs
// around a yield that actually fires, so the cost is negligible.
int gLoaderPollYields = 0;           // emscripten_sleep(0) from Poll()/PollUntilEmpty
double gLoaderPollYieldMs = 0.0;     // wall-ms spent in those yields
int gLoaderPullSliceYields = 0;      // emscripten_sleep(0) from PollUntilLoaded
double gLoaderPullSliceYieldMs = 0.0;// wall-ms spent in those yields
int gLoaderPullCalls = 0;            // PollUntilLoaded invocations (web arm)
#endif

#ifdef HX_NATIVE
#include <cstdlib> // QW-1: getenv/atof for RB3_LOADER_BUDGET_MS (guarded out of Wii asm)
#endif

LoadMgr TheLoadMgr;
void (*LoadMgr::sFileOpenCallback)(const char *);

Timer gPollFrontLoaderTimer;

#ifdef HX_NATIVE
// TRACK-B (loadperf) attribution counters — native-only, read by the App.cpp
// frame instrument (RB3_FRAME_INSTRUMENT). Each accumulates ms spent THIS frame
// in the named entry point; App.cpp zeroes them at frame start and reports them.
// Zero runtime cost when the instrument is off (the App-side reads are gated).
float gLoadPollMsThisFrame = 0.0f;        // SystemPoll -> TheLoadMgr.Poll() (budgeted)
float gLoadPollUntilMsThisFrame = 0.0f;   // PollUntilLoaded + PollUntilEmpty (sync drains)

// TASK A4 frame-trace asset-event counters (audio-perf-investigation). Defined
// HERE so any native target that compiles this TU links them (rb3-native links
// the recorder rb3_frame_trace.cpp; the minimal rb3-dta tool does not, but still
// compiles Loader.cpp/Synth.cpp's increment hooks). gFrameTraceActive stays
// false until RB3FrameTraceRecord opens a trace file, so the hooks are a single
// not-taken branch otherwise.
bool gFrameTraceActive = false;
int  gFrameTraceLoaderAdds = 0;
int  gFrameTraceStreamOpens = 0;

// Incremental-load-perf (PLAN.md T1) per-frame attribution counters. STRONG
// definitions here (every native target links Loader.cpp); the engine TUs carry
// matching WEAK defs (milo-native-engine PipelineManager.cpp) so the engine
// links standalone, with these strong defs winning at the rb3 final link. Each
// g*MsThisFrame accumulates ms spent THIS frame in one choke point; read +
// zeroed by RB3FrameTraceRecord. See engine platform/FrameTraceCounters.h.
float  gFetchSyncMsThisFrame = 0.0f;
int    gFetchSyncCountThisFrame = 0;
double gFetchSyncBytesThisFrame = 0.0;
float  gDtaParseMsThisFrame = 0.0f;
float  gObjLoadMsThisFrame = 0.0f;
float  gObjLoadWorstMs = 0.0f;
char   gObjLoadWorstName[64] = {0};
float  gAudioPrimeMsThisFrame = 0.0f;
float  gTexUploadMsThisFrame = 0.0f;
int    gTexUploadCountThisFrame = 0;
float  gMeshUploadMsThisFrame = 0.0f;
int    gMeshUploadCountThisFrame = 0;
float  gPipelineCreateMsThisFrame = 0.0f;
int    gPipelineCreateCountThisFrame = 0;
float  gStreamReadMsThisFrame = 0.0f;
#endif

struct LoaderGlitchContext {
    String file;           // 0x0  (vtable + mCap + mStr = 12 bytes)
    const char *name;      // 0xC
    const char *fromState; // 0x10
    LoaderPos toPos;       // 0x14
};

LoadMgr::LoadMgr()
    : mLoaders(), mPlatform(kPlatformWii), mEditMode(0), mCacheMode(0), mFactories(),
      mPeriod(10.0f), mLoading(), mTimer(), mAsyncUnload(0), mLoaderPos(kLoadFront) {}

void LoadMgr::SetEditMode(bool b) {
    mEditMode = b;
    static DataNode &edit_mode = DataVariable("edit_mode");
    edit_mode = DataNode(mEditMode);
}

static DataNode OnLoadMgrPrint(DataArray *da) {
    TheLoadMgr.Print();
    return DataNode(0);
}

static DataNode OnSetLoadMgrDebug(DataArray *da) {
    TheLoadMgr.mCacheMode = da->Int(1) != 0;
    return DataNode(0);
}

static DataNode OnSetEditMode(DataArray *da) {
    TheLoadMgr.SetEditMode(da->Int(1));
    return DataNode(0);
}

static DataNode OnSetLoaderPeriod(DataArray *da) {
    return TheLoadMgr.SetLoaderPeriod(da->Float(1));
}

static DataNode OnSysPlatformSym(DataArray *da) {
    return DataNode(PlatformSymbol(TheLoadMgr.GetPlatform()));
}

void LoadMgr::Init() {
    SetEditMode(false);
    if (OptionBool("null_platform", false))
        mPlatform = kPlatformNone;
    DataRegisterFunc("loadmgr_debug", OnSetLoadMgrDebug);
    DataRegisterFunc("loadmgr_print", OnLoadMgrPrint);
    DataRegisterFunc("set_edit_mode", OnSetEditMode);
    DataRegisterFunc("set_loader_period", OnSetLoaderPeriod);
    DataRegisterFunc("sysplatform_sym", OnSysPlatformSym);
    DataVariable("sysplatform") = DataNode((int)mPlatform);
}

Loader *LoadMgr::ForceGetLoader(const FilePath &fp) {
    if (fp.empty())
        return 0;
    else {
        Loader *gotten = GetLoader(fp);
        if (!gotten) {
            gotten = TheLoadMgr.AddLoader(fp, kLoadFront);
            if (!gotten) {
                MILO_WARN("Don't recognize file %s", FilePath(fp));
            }
        }
        if (gotten) {
            TheLoadMgr.PollUntilLoaded(gotten, 0);
        }
        return gotten;
    }
}

Loader *LoadMgr::GetLoader(const FilePath &fp) const {
    if (fp.empty())
        return 0;
    else {
        Loader *theLoader = 0;
        for (std::list<Loader *>::const_iterator it = mLoaders.begin();
             it != mLoaders.end();
             ++it) {
            if ((*it)->mFile == fp) {
                theLoader = *it;
                break;
            }
        }
        return theLoader;
    }
}

Loader *LoadMgr::AddLoader(const FilePath &file, LoaderPos pos) {
    if (file.empty())
        return NULL;
#ifdef HX_NATIVE
    // TASK A4 frame-trace asset-event hook (rb3_frame_trace.cpp). Count every
    // file/dir/texture/milo loader started this frame; the per-frame recorder
    // reads + zeros the counter. One branch when tracing is off.
    {
        extern bool gFrameTraceActive;
        extern int gFrameTraceLoaderAdds;
        if (gFrameTraceActive)
            gFrameTraceLoaderAdds++;
    }
#endif
    if (sFileOpenCallback != NULL) {
        sFileOpenCallback(file.c_str());
    }
    const char *ext = FileGetExt(file.c_str());
    for (std::list<std::pair<String, LoaderFactoryFunc *> >::iterator it =
             mFactories.begin();
         it != mFactories.end();
         ++it) {
        if (it->first == ext) {
            return (it->second)(file, pos);
        }
    }
    return new FileLoader(file, file.c_str(), pos, 0, false, true, NULL);
}

void LoadMgr::PollUntilLoaded(Loader *ldr1, Loader *ldr2) {
    const char *funcName = __FUNCTION__;
    SetGPHangDetectEnabled(false, funcName);
    Loader *theLdr = ldr1;
    bool ldr2IsNull = (ldr2 == 0);
#ifdef HX_WEB
    // Web port: the matched-fork body below sets unk1c = 1e30f, which DISABLES
    // the per-state CheckSplit() time-slice and drains the whole loader (and its
    // dep chain) to completion in ONE un-interruptible call. On native that's
    // fine; in the browser the App boot drives MANY such loads (splash →
    // main_hub → song_select chains), and a single multi-chunk milo load runs
    // ~tens of seconds of wasm CPU with NO return to the event loop, so the
    // browser kills the unresponsive tab (the W2a/W2b "web-asset-load wall").
    //
    // The fix mirrors rb3_render_mesh.cpp's LoadMiloDirYielding (the W2 working
    // pattern, now generalised here so the App boot's load path inherits it):
    // use the loader's OWN cooperative time-slice — arm a small split budget +
    // restart the split timer before each poll so CheckSplit() trips after
    // ~kSliceMs of work and PollFrontLoader returns mid-load — then
    // emscripten_sleep(0) yields a full event-loop turn (paint + input) under
    // JSPI before the wasm stack resumes. A hard iteration cap is a final safety
    // valve so a stuck loader can't spin the tab forever (mirrors DC3's web cap).
    const float kSliceMs = 8.0f;
    int maxIter = 200000; // safety valve; ~big multi-chunk milos finish well under
    int polls = 0;
    ++gLoaderPullCalls;
    // Fix B (handoff 02) — yield throttle. The original web arm yielded once PER
    // ~8 ms slice (UN-throttled), unlike Poll() which gates its yield to one per
    // RB3_LOADER_YIELD_MS of accumulated work. Step-0 measurement (RB3_BOOT_IO_STATS)
    // proved the App-ctor load chain drives THIS path almost exclusively (≈1198
    // PollUntilLoaded calls, loaderPollYields=0) and that its per-slice yields cost
    // ~2600 JSPI suspends ≈ 11 s of pure event-loop latency — the bulk of the ~16 s
    // App-ctor idle. Each emscripten_sleep(0) costs a full event-loop round trip
    // (~4-16 ms) under JSPI; firing one per ~8 ms work-slice is pathological.
    //
    // Fix: gate the yield to one per RB3_LOADER_MIN_YIELD_MS of accumulated work
    // (default 16 ms = ~60 fps), exactly mirroring Poll()'s sinceYield throttle.
    // Measured: appctor 16.35 s -> 6.48 s, pullSliceYields 2628 -> 178 (~11 s ->
    // ~0.8 s). One yield per 16 ms still composites the boot overlay at 60 fps
    // (loadperf-responsiveness.mjs gate). Env-gated for A/B: RB3_LOADER_MIN_YIELD_MS=0
    // restores the original per-slice yield.
    static float sMinYieldMs = -1.0f;
    if (sMinYieldMs < 0.0f) {
        sMinYieldMs = 16.0f; // default 16 ms (~60 fps); 0 = original per-slice yield
        if (const char *e = ::getenv("RB3_LOADER_MIN_YIELD_MS")) {
            if (e[0])
                sMinYieldMs = (float)atof(e);
        }
        if (sMinYieldMs < 0.0f)
            sMinYieldMs = 0.0f;
    }
    Timer sinceYield;
    sinceYield.Restart();
    while (!theLdr->IsLoaded()) {
        if (--maxIter <= 0) {
            MILO_WARN("PollUntilLoaded: web iteration cap hit waiting for %s",
                      ldr1->DebugText());
            break;
        }
        bool noFail = ldr2IsNull || ldr2 != mLoading.front();
        if (!noFail) {
#ifdef MILO_DEBUG
            MILO_FAIL(
                "PollUntilLoaded circular dependency %s on %s",
                ldr2->DebugText(),
                ldr1->DebugText()
            );
#endif
        }
        // Arm the cooperative slice (instead of the 1e30f drain-to-completion).
        unk1c = kSliceMs;
        mTimer.Restart();
        PollFrontLoader();
        polls++;
        if ((polls % 200) == 0) {
            Loader *f = mLoading.empty() ? nullptr : mLoading.front();
            MILO_LOG("PollUntilLoaded(web): poll %d front=%s target_loaded=%d\n",
                     polls, f ? f->mFile.c_str() : "(empty)", theLdr->IsLoaded());
        }
        if (!ListFind(mLoading, theLdr))
            break;
        if (mLoading.front()->IsLoaded()) {
            mLoading.pop_front();
        }
        // Throttle gate (Fix B, opt-in): skip this slice's yield if it would fire
        // sooner than sMinYieldMs after the previous one. When sMinYieldMs==0 the
        // condition is always true (>= 0) so we yield every slice as before.
        sinceYield.Split();
        if (Timer::CyclesToMs(sinceYield.mCycles) >= sMinYieldMs) {
            Timer yieldTimer;
            yieldTimer.Restart();
            emscripten_sleep(0); // JSPI: suspend -> browser event loop -> resume
            yieldTimer.Split();
            ++gLoaderPullSliceYields;
            gLoaderPullSliceYieldMs += Timer::CyclesToMs(yieldTimer.mCycles);
            sinceYield.Restart();
        }
    }
    SetGPHangDetectEnabled(true, funcName);
    return;
#endif
#ifdef HX_NATIVE
    // TRACK-B attribution: time this synchronous-contract drain into the
    // sync-drain bucket so the App-side frame instrument can show how much of a
    // long frame is unavoidable blocking load vs. the budgeted background Poll().
    Timer pulTimer;
    pulTimer.Restart();
#endif
    while (!theLdr->IsLoaded()) {
        unk1c = 1e+30f;
        bool noFail = ldr2IsNull || ldr2 != mLoading.front();
        if (!noFail) {
#ifdef MILO_DEBUG
            MILO_FAIL(
                "PollUntilLoaded circular dependency %s on %s",
                ldr2->DebugText(),
                ldr1->DebugText()
            );
#endif
        }
        PollFrontLoader();
        if (!ListFind(mLoading, theLdr))
            break;
        if (mLoading.front()->IsLoaded()) {
            mLoading.pop_front();
        }
    }
#ifdef HX_NATIVE
    pulTimer.Split();
    gLoadPollUntilMsThisFrame += Timer::CyclesToMs(pulTimer.mCycles);
#endif
    SetGPHangDetectEnabled(true, funcName);
}

void LoadMgr::PollUntilEmpty() {
    char *funcName = (char *)__FUNCTION__;
    SetGPHangDetectEnabled(false, funcName);
    float savedPeriod = TheLoadMgr.mPeriod;
    TheLoadMgr.unk1c = 1e+30f;
    TheLoadMgr.mPeriod = 1e+30f;
#ifdef HX_NATIVE
    // TRACK-B: PollUntilEmpty forces the unbudgeted drain (period = 1e30) so
    // Poll()'s budget never trips — its time should be attributed to the
    // synchronous-drain bucket, not the budgeted background bucket. Snapshot the
    // Poll bucket and move the delta over so the App-side attribution is honest.
    float prePollMs = gLoadPollMsThisFrame;
#endif
    TheLoadMgr.Poll();
    TheLoadMgr.mPeriod = savedPeriod;
    TheLoadMgr.unk1c = savedPeriod;
#ifdef HX_NATIVE
    float delta = gLoadPollMsThisFrame - prePollMs;
    gLoadPollMsThisFrame = prePollMs;       // un-attribute from background bucket
    gLoadPollUntilMsThisFrame += delta;     // re-attribute to sync-drain bucket
#endif
    SetGPHangDetectEnabled(true, funcName);
}

void LoadMgr::Print() {
    for (std::list<Loader *>::iterator it = mLoading.begin(); it != mLoading.end();
         it++) {
        TheDebug << (*it)->mFile.c_str() << " " << LoaderPosString((*it)->mPos, 0)
                 << "\n";
    }
}

const char *LoadMgr::LoaderPosString(LoaderPos pos, bool abbrev) {
    static const char *names[4] = {
        "kLoadFront", "kLoadBack", "kLoadFrontStayBack", "kLoadStayBack"
    };
    static const char *abbrevs[4] = { "F", "B", "FSB", "SB" };
    MILO_ASSERT(pos >= 0 && pos <= kLoadStayBack, 0x11D);
    if (abbrev)
        return abbrevs[pos];
    else
        return names[pos];
}

int LoadMgr::AsyncUnload() const { return mAsyncUnload; }
void LoadMgr::StartAsyncUnload() { mAsyncUnload++; }
void LoadMgr::FinishAsyncUnload() { mAsyncUnload--; }

void LoadMgr::Poll() {
    if (mPeriod <= 0)
        return;
    mTimer.Restart();
    unk1c = mPeriod;
#ifdef HX_WEB
    // Web port (TRACK-B smooth-load): this is the per-FRAME background loader
    // drain, called once per RunOneFrame from SystemPoll. The matched-fork /
    // HX_NATIVE drain below pumps the WHOLE mLoading queue to completion in one
    // un-interruptible call (it disables CheckSplit via unk1c = 1e30f). The old
    // web arm did better — it sliced and emscripten_sleep(0)'d between slices —
    // but it STILL looped `while (!mLoading.empty())`, i.e. it drained the entire
    // queue before returning. Returning is what matters: only when this Poll()
    // RETURNS does the rest of RunOneFrame run (TheUI.Draw() repaints the loading
    // UI) and does mainLoop() return to JS so requestAnimationFrame can schedule
    // the next tick and the browser composites the CSS loading overlay. Draining
    // the whole queue here meant a multi-second background load (e.g. the song
    // milo chain) ran across one RunOneFrame: the canvas froze on its last frame
    // and the overlay's compositor animation could stall — the perceived freeze.
    //
    // Fix: bound the work to a small per-FRAME budget (RB3_LOADER_BUDGET_MS,
    // default 8) and RETURN once exceeded, exactly like the native arm below.
    // The queue is re-entered next frame. emscripten_sleep(0) is kept as a
    // within-frame courtesy yield between slices (cheap under JSPI), but the
    // budget break is the load-bearing change: it caps a single frame's load CPU.
    //
    // NOTE: HX_WEB implies HX_NATIVE on this build, so this arm must come BEFORE
    // the HX_NATIVE block to win. PollUntilEmpty (which sets mPeriod = 1e30f as
    // an "unbudgeted, drain-to-empty" sentinel) deliberately bypasses the
    // per-frame budget break — it has a synchronous contract and must empty the
    // queue — but it STILL slices + emscripten_sleep(0)'s so the tab keeps
    // compositing while it drains. Only the SystemPoll-driven background Poll()
    // (mPeriod = mPeriodNormal, a small number) is frame-bounded.
    {
        static float sBudgetMs = -1.0f;
        static float sYieldMs = -1.0f;
        if (sBudgetMs < 0.0f) {
            sBudgetMs = 8.0f;
            if (const char *e = ::getenv("RB3_LOADER_BUDGET_MS")) {
                if (e[0])
                    sBudgetMs = (float)atof(e);
            }
            if (sBudgetMs <= 0.0f)
                sBudgetMs = 8.0f;
            // Yield interval for the synchronous drain (PollUntilLoaded/Empty).
            // Decouples work-slice granularity (sBudgetMs, drives CheckSplit
            // responsiveness) from browser-yield frequency (sYieldMs): the drain
            // does real work for sYieldMs of wall-clock between emscripten_sleep(0)
            // yields, instead of the old code's yield-on-EVERY-slice. The win is
            // avoiding pathological sub-millisecond yield spam when PollFrontLoader
            // returns fast (front loader blocked) — each emscripten_sleep(0) costs
            // a full event-loop turn (~4-16ms). NOTE: measurement (see
            // docs/native/web-loadperf-findings-2026-06-03.md) showed this is
            // NEUTRAL for total boot time — the boot bottleneck is the App ctor's
            // ~7s async wait, which is GPU-INDEPENDENT (validated: real RTX 3090 ==
            // SwiftShader == 12.3s App ctor) and is the web async-file-I/O / JSPI
            // suspend-per-read model, not loader-yield overhead. Kept as a clarity +
            // spin-overhead refactor + the RB3_LOADER_YIELD_MS tunable.
            sYieldMs = 16.0f;
            if (const char *e = ::getenv("RB3_LOADER_YIELD_MS")) {
                if (e[0])
                    sYieldMs = (float)atof(e);
            }
            if (sYieldMs < sBudgetMs)
                sYieldMs = sBudgetMs;
        }
        // mPeriod >= sentinel ⇒ PollUntilEmpty's unbudgeted drain: empty the
        // whole queue (no per-frame break), yielding only every sYieldMs.
        bool drainToEmpty = (mPeriod >= 1e29f);
        // Per-iteration mTimer drives each state func's internal CheckSplit();
        // budgetTimer bounds the cumulative per-frame cost; sinceYield gates how
        // often we pay for a browser composite yield.
        Timer budgetTimer;
        budgetTimer.Restart();
        Timer sinceYield;
        sinceYield.Restart();
        int maxIter = 400000; // safety valve (PollUntilEmpty can have long queues)
        while (!mLoading.empty()) {
            if (--maxIter <= 0) {
                MILO_WARN("LoadMgr::Poll(web): iteration cap hit; %d loaders pending",
                          (int)mLoading.size());
                break;
            }
            unk1c = sBudgetMs;
            mTimer.Restart();
            PollFrontLoader();
            if (!mLoading.empty() && mLoading.front()->IsLoaded()) {
                mLoading.pop_front();
            }
            budgetTimer.Split();
            if (!drainToEmpty && Timer::CyclesToMs(budgetTimer.mCycles) > sBudgetMs) {
                // Per-frame background budget spent — RETURN so RunOneFrame
                // repaints + mainLoop returns + RAF re-ticks (the frame return is
                // the composite yield; no manual sleep needed here).
                break;
            }
            // Synchronous drain: yield to the browser only every sYieldMs of
            // accumulated work, not on every slice (avoids sub-ms yield spam).
            sinceYield.Split();
            if (Timer::CyclesToMs(sinceYield.mCycles) >= sYieldMs) {
                Timer yieldTimer; // Step-0 instrumentation (handoff 02)
                yieldTimer.Restart();
                emscripten_sleep(0);
                yieldTimer.Split();
                ++gLoaderPollYields;
                gLoaderPollYieldMs += Timer::CyclesToMs(yieldTimer.mCycles);
                sinceYield.Restart();
            }
        }
        return;
    }
#endif
#ifdef HX_NATIVE
    // QW-1 (loader-performance.md): time-budget the native drain. The old native
    // arm set unk1c = 1e30f and drained the WHOLE mLoading queue per frame, which
    // froze the frame loop at boot (App-ctor PollUntilEmpty), at per-song loads
    // (tv3 -> game_screen), and during the splash crowd/extras merge burst
    // (~14fps observed). This is the SAME cooperative-slice shape the WEB arm
    // above and the Wii `#else` path below use, minus emscripten_sleep: arm a
    // small per-state split budget so a state func's internal CheckSplit() trips
    // and PollFrontLoader returns mid-load, accumulate wall time across the whole
    // Poll, and break once the per-frame budget is exceeded so we return to the
    // frame loop (re-entering Poll next frame). Bytes still arrive synchronously
    // (LW-1 moves I/O off-thread), but the per-frame loss is bounded.
    //
    // PollUntilEmpty / PollUntilLoaded keep their synchronous-contract drain
    // (they set unk1c = 1e30f themselves) — only the global SystemPoll ->
    // TheLoadMgr.Poll() goes through this budgeted path.
    //
    // The budget is env-gated (RB3_LOADER_BUDGET_MS, default 8). Set it huge
    // (e.g. 100000) to restore the old unbudgeted drain-to-completion behavior.
    {
        static float sBudgetMs = -1.0f;
        if (sBudgetMs < 0.0f) {
            sBudgetMs = 8.0f;
            if (const char *e = ::getenv("RB3_LOADER_BUDGET_MS")) {
                if (e[0])
                    sBudgetMs = (float)atof(e);
            }
            if (sBudgetMs <= 0.0f)
                sBudgetMs = 8.0f;
        }
        // mPeriod >= sentinel ⇒ PollUntilEmpty's unbudgeted drain: empty the whole
        // queue this call (synchronous contract). Otherwise this is the per-frame
        // background drain and the budget break returns to the frame loop.
        bool drainToEmpty = (mPeriod >= 1e29f);
        // Cumulative wall-clock across the whole Poll() call: mTimer is restarted
        // per-iteration (so each state func's CheckSplit() sees a fresh slice), so
        // a separate timer is needed to bound the total per-frame cost.
        Timer budgetTimer;
        budgetTimer.Restart();
        // Safety valve: if a freshly-front loader's state func lacks a CheckSplit()
        // yield it could spin within one PollFrontLoader; cap iterations so a stuck
        // loader can't peg the frame loop (mirrors the web arm's cap).
        int maxIter = 400000;
        while (!mLoading.empty()) {
            if (--maxIter <= 0) {
                MILO_WARN(
                    "LoadMgr::Poll(native): iteration cap hit; %d loaders pending",
                    (int)mLoading.size()
                );
                break;
            }
            unk1c = sBudgetMs;
            mTimer.Restart();
            PollFrontLoader();
            if (!mLoading.empty() && mLoading.front()->IsLoaded()) {
                mLoading.pop_front();
            }
            budgetTimer.Split();
            if (!drainToEmpty && Timer::CyclesToMs(budgetTimer.mCycles) > sBudgetMs)
                break;
        }
        budgetTimer.Split();
        gLoadPollMsThisFrame += Timer::CyclesToMs(budgetTimer.mCycles);
        return;
    }
#endif
    while (!mLoading.empty()) {
        PollFrontLoader();
        if (!mLoading.empty()) {
            if (mLoading.front()->IsLoaded()) {
                mLoading.pop_front();
            }
        }
        mTimer.Split();
        if (Timer::CyclesToMs(mTimer.mCycles) > unk1c)
            return;
    }
}

void LoadMgr::PollFrontLoader() {
    const AutoTimer at(&gPollFrontLoaderTimer, 50.0f, NULL, NULL);
    Loader *front = mLoading.front();
    LoaderPos savedPos = mLoaderPos;
    mLoaderPos = front->mPos;

    LoaderGlitchContext ctx;
    ctx.file = front->mFile.c_str();
    ctx.toPos = front->mPos;
    ctx.name = front->StateName();

    MemPushHeap(front->mHeap);
    if (UsingCD()) {
        front->PollLoading();
        if (!ListFind(mLoading, front)) {
            ctx.fromState = "deleted";
        } else {
            ctx.fromState = front->StateName();
        }
    } else {
        front->PollLoading();
    }
    MemPopHeap();
    mLoaderPos = savedPos;
}

void LoadMgr::RegisterFactory(const char *cc, LoaderFactoryFunc *func) {
    for (std::list<std::pair<String, LoaderFactoryFunc *> >::iterator it =
             mFactories.begin();
         it != mFactories.end();
         it++) {
        if ((*it).first == cc) {
            MILO_WARN("More than one LoadMgr factory for extension \"%s\"!", cc);
        }
    }
    mFactories.push_back(std::pair<String, LoaderFactoryFunc *>(String(cc), func));
}

Loader::Loader(const FilePath &fp, LoaderPos pos) : mPos(pos), mFile(fp) {
    mHeap = GetCurrentHeapNum();
    MILO_ASSERT(MemNumHeaps() == 0 || (mHeap != kNoHeap && mHeap != kSystemHeap), 446);
    MILO_ASSERT(!streq(MemHeapName(mHeap), "fast"), 448);
    TheLoadMgr.mLoaders.push_front(this);
    if (mPos == kLoadFront) {
        TheLoadMgr.mLoading.push_front(this);
    } else if (mPos == kLoadStayBack) {
        TheLoadMgr.mLoading.push_back(this);
    } else {
        std::list<Loader *>::iterator it = TheLoadMgr.mLoading.end();
        while (it != TheLoadMgr.mLoading.begin()) {
            --it;
            if ((*it)->GetPos() <= 1) {
                ++it;
                break;
            }
        }
        TheLoadMgr.mLoading.insert(it, this);
    }
}

Loader::~Loader() {
    TheLoadMgr.mLoading.remove(this);
    TheLoadMgr.mLoaders.remove(this);
}

FileLoader::FileLoader(
    const FilePath &loaderFile,
    const char *file,
    LoaderPos pos,
    int flags,
    bool temp,
    bool warn,
    BinStream *bs
)
    : Loader(loaderFile, pos), mFile(NULL), mStream(bs), mBuffer(NULL), mBufLen(0),
      mAccessed(0), mTemp(temp), mWarn(warn), mFlags(flags), mFilename(file), unk3c(0),
      unk40(-1), mState(0) {
    if (mStream) {
        mState = &FileLoader::LoadStream;
    } else {
        mState = &FileLoader::OpenFile;
    }
}

void FileLoader::AllocBuffer() {
    if (mTemp) {
        mBuffer = (char *)_MemAllocTemp(mBufLen, 0);
    } else {
        mBuffer = (char *)_MemAlloc(mBufLen, 32);
    }
}

void FileLoader::OpenFile() {
    Archive *old = TheArchive;
    const char *fname = mFilename.c_str();
    bool oldusingcd = UsingCD();
    bool b1 = gHostFile && FileMatch(fname, gHostFile);
    if (b1) {
        SetUsingCD(false);
        TheArchive = nullptr;
    }
    {
        MemDoTempAllocations mem(true, false);
        mFile = NewFile(fname, mFlags | 2);
    }
    if (b1) {
        SetUsingCD(oldusingcd);
        TheArchive = old;
    }

    if (!mFile && *fname != '\0' && mWarn) {
        MILO_WARN(
            "Could not load: %s (actually %s)",
            FileLocalize(Loader::mFile.c_str(), 0),
            fname
        );
    }
    if (mFile && !mFile->Fail()) {
        mBufLen = mFile->Size();
        AllocBuffer();
        mFile->ReadAsync((void *)mBuffer, mBufLen);
        mState = &FileLoader::LoadFile;
    } else {
        mState = &FileLoader::DoneLoading;
    }
}

const char *FileLoader::DebugText() {
    return MakeString("FileLoader: %s", Loader::mFile.c_str());
}

void FileLoader::LoadFile() {
    int asdf;
    if (mFile->ReadDone(asdf)) {
        if (mFile->Fail()) {
            mBufLen = 0;
            _MemFree((void *)mBuffer);
            mBuffer = nullptr;
        }
        RELEASE(mFile);
        mState = &FileLoader::DoneLoading;
    }
}

void FileLoader::DoneLoading() {}

FileLoader::~FileLoader() {
    if (!mAccessed) {
        _MemFree((void *)mBuffer);
        delete mFile;
    }
}

int FileLoader::GetSize() { return mBufLen; }

const char *FileLoader::GetBuffer(int *size) {
    MILO_ASSERT(IsLoaded(), 0x287);
    if (size)
        *size = mBufLen;
    mAccessed = true;
    return (const char *)mBuffer;
}

bool FileLoader::IsLoaded() const { return mState == &FileLoader::DoneLoading; }

void FileLoader::PollLoading() {
#ifdef HX_NATIVE
    // Native = synchronous: always advance one state step (mirrors DC3's
    // `FileLoader::PollLoading() { (this->*mState)(); }`). See DirLoader::PollLoading
    // for why the Wii time-sliced `#else` form stalls a freshly-front loader on a
    // fast host. The LoadStream/LoadFile state functions still honor their internal
    // CheckSplit() guards.
    if (!IsLoaded() && TheLoadMgr.GetFirstLoading() == this) {
        (this->*mState)();
    }
    return;
#endif
    while (!TheLoadMgr.CheckSplit() && TheLoadMgr.GetFirstLoading() == this && !IsLoaded()
    ) {
        (this->*mState)();
    }
}

void FileLoader::LoadStream() {
    EofType t;
    while (t = mStream->Eof(), t != NotEof) {
        MILO_ASSERT(t == TempEof, 0x2A8);
        if (TheLoadMgr.CheckSplit())
            return;
    }
    if (!mBuffer) {
        int size;
        *mStream >> size;
        if (size == -1) {
            *mStream >> unk40;
            *mStream >> mBufLen;
        } else {
            unk40 = 0;
            mBufLen = size;
        }
        AllocBuffer();
    }
    int i2 = unk40 > 0 ? 0x10000 : mBufLen;
    while (true) {
        int i3 = Min(mBufLen - unk3c, i2);
        while (t = mStream->Eof(), t != NotEof) {
            MILO_ASSERT(t == TempEof, 0x2C9);
            if (TheLoadMgr.CheckSplit())
                return;
        }
        if (i3 == 0)
            break;
        mStream->Read((void *)(mBuffer + unk3c), i3);
        unk3c += i3;
    }
    mState = &FileLoader::DoneLoading;
}