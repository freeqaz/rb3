#pragma push
#pragma inline_depth(255)
// band aid fix: force inlining of the inherited UIScreen vector<PanelRef>
// dtor into ~BudgetScreen (-inline noauto exhausts the caller budget here,
// emitting an out-of-line vector dtor call instead of the inlined loop).
#include "BudgetScreen.h"
#pragma pop

#include "system/os/Debug.h"
#include "system/os/Timer.h"
#include "system/ui/UI.h"
#include "system/utl/Option.h"
#include "system/rndobj/Rnd.h"
#include "system/synth/StandardStream.h"
#include "system/utl/MemMgr.h"
#include "system/utl/SysTest.h"
#include "system/obj/PropSync_p.h"
#include "system/obj/Msg.h"
#include "system/utl/BinStream.h"
#include "system/utl/MakeString.h"

#include "band3/game/SongDB.h"
#include "band3/meta_band/BandSongMgr.h"
#include "band3/meta_band/SessionMgr.h"
#include "band3/meta_band/MetaPerformer.h"
#include "band3/meta_band/ModifierMgr.h"
#include "band3/meta_band/ProfileMgr.h"
#include "band3/game/BandUserMgr.h"
#include "band3/game/BandUser.h"
#include "band3/game/GameConfig.h"
#include "band3/game/Defines.h"
#include "network/net/Net.h"

#include "system/utl/Symbols2.h"

#include <algorithm>
#include "decomp.h"

// Explicit specializations to avoid bool materialization in comparison loops,
// so CW uses blt/bge directly after fcmpo instead of mfcr/srwi./bne.
namespace stlpmtx_std {

template <>
inline less<float> __less<float>(float*) { return less<float>(); }

template <>
float* __unguarded_partition<float*, float, less<float> >(
    float* __first, float* __last, float __pivot, less<float>) {
    for (;;) {
        while (*__first < __pivot)
            ++__first;
        --__last;
        while (__pivot < *__last)
            --__last;
        if (!(__first < __last))
            return __first;
        iter_swap(__first, __last);
        ++__first;
    }
}

template <>
void __unguarded_linear_insert<float*, float, less<float> >(
    float* __last, float __val, less<float>) {
    float* __next = __last;
    --__next;
    while (__val < *__next) {
        *__last = *__next;
        __last = __next;
        --__next;
    }
    *__last = __val;
}

template <>
void __introsort_loop<float*, float, long, less<float> >(
    float* __first, float* __last, float*,
    long __depth_limit, less<float> __comp) {
    while (__last - __first > 16) {
        if (__depth_limit == 0) {
            partial_sort(__first, __last, __last, __comp);
            return;
        }
        ptrdiff_t __len = __last - __first;
        float __a = *__first;
        --__depth_limit;
        float* __mid = __first + __len / 2;
        float __b = *__mid;
        float* __pivot_ptr;
        if (__a < __b) {
            float __c = *(__last - 1);
            if (__b < __c)
                __pivot_ptr = __mid;
            else if (__a < __c)
                __pivot_ptr = __last - 1;
            else
                __pivot_ptr = __first;
        } else if (__a < *(__last - 1)) {
            __pivot_ptr = __first;
        } else if (__b < *(__last - 1)) {
            __pivot_ptr = __last - 1;
        } else {
            __pivot_ptr = __mid;
        }
        float* __cut = __unguarded_partition(__first, __last, *__pivot_ptr, __comp);
        __introsort_loop(__cut, __last, (float*)0, __depth_limit, __comp);
        __last = __cut;
    }
}

} // namespace stlpmtx_std


extern int gMainFree;
bool gUseSsv;

DECOMP_FORCEACTIVE(BudgetScreen, " ", "\n")

Distribution::Distribution(float res) : mRes(res) {
    MILO_ASSERT(mRes > 0.0f, 70);
    Reset();
}

void Distribution::Reset() {
    mCount = 0;
    mTotal = 0;
    mDist.clear();
    mDist.resize((int)(10 / mRes));
}

float Distribution::Pctile(float pct) {
    MILO_ASSERT(pct >= 0.f && pct <= 1.f, 84);

    float fCount = mCount;
    int total = 0;
    for (int i = 0; i < mDist.size(); i++) {
        total += mDist[i];

        float tmp = fCount * pct;
        if (total >= tmp) {
            return mRes * (i + 1);
        }

        if (total == mCount) {
            return mRes * (i + 1);
        }
    }

    MILO_NOTIFY_ONCE("something went wrong in pctile calculation\n");
    return 0.0f;
}

inline void Distribution::asdkjf(
    TextStream &stream, float min, float defaultMean, float max, const char *fmt
) {
    float mean = mCount != 0 ? mTotal / mCount : defaultMean;
    stream << MakeString(fmt, min, mean, max);
}

void Distribution::Report(TextStream &stream, const char *tag) {
    MILO_ASSERT(tag || !gUseSsv, 103);

    if (!gUseSsv) {
        // this is stupid lol
        asdkjf(stream, mMin, 0.f, mMax, "Min/Mean/Max: %.2f/%.2f/%.2f\n");
        // float mean = mCount != 0 ? mTotal / mCount : 0.f;
        // stream << MakeString("Min/Mean/Max: %.2f/%.2f/%.2f\n", mMin, mean, mMax);
    }
    if (!gUseSsv) {
        stream << "Distribution:\n";
    }

    int acc = mCount;
    float start = mRes * (mDist.size() - 1);
    float res;
    int i;
    for (i = mDist.size() - 1, res = mRes; i >= 0; i--, res = mRes, start -= mRes) {
        int hitCount = mDist[i];
        if (hitCount == 0)
            continue;

        if (gUseSsv) {
            stream << MakeString(
                "%s;[%.1f,%.1f);%d;%3.1f\n",
                tag,
                start,
                start + res,
                hitCount,
                (float)(acc * 100) / mCount
            );
        } else {
            stream << MakeString(
                " [%.1f, %.1f): %5d (%3.1f%%)\n",
                start,
                start + res,
                hitCount,
                (float)(acc * 100) / mCount
            );
        }

        acc -= mDist[i];
        if (acc == 0) {
            break;
        }

        MILO_ASSERT(acc > 0, 136);
    }
}

void Distribution::operator<<(float value) {
    if (value < 0) {
        return;
    }

    if (mCount == 0) {
        mMax = value;
        mMin = value;
    } else {
        if (value < mMin) {
            mMin = value;
        }
        if (mMax < value) {
            mMax = value;
        }
    }

    int pos = value / mRes;
    if (pos >= mDist.size()) {
        mDist.resize(pos * 2);
    }

    mDist[pos] += 1;
    mCount++;
    mTotal += value;
}

float Average(std::vector<float> &items, bool partial) {
    std::sort(items.begin(), items.end());

    // for whatever reason this needs to be up here to match?
    std::vector<float>::iterator it;

    int size = items.size();
    int endOffset = partial ? (int)(size * 0.3f) : 0;

    float total = 0;
    for (it = items.begin(); it != items.end() - endOffset; ++it) {
        total += *it;
    }

    return total / (size - endOffset);
}

BudgetScreen::BudgetScreen()
    : mTestPanel(nullptr), mLastCpu(0.0), mLastGpu(0.0), mCpuDist(0.1), mGsDist(0.1),
      mHudDist(0.1), mEtcDist(0.1), mRecordStartTick(0), mRecordEndTick(1000000),
      mTests(SystemConfig("tests")), mTestIdx(0),
      mWorstOnly(OptionBool("worst_only", false)), mWorstCpuPctile(0.0),
      mWorstGsPctile(0.0), mSampleCount(0) {
    TheSongMgr.AddSongs(SystemConfig("songs"));
    char _slotpad[8]; (void)_slotpad;
    TheContentMgr->UnregisterCallback(&TheSongMgr, false);

    Symbol logFileSym("log_file");
    const char *logFile = OptionStr("budget_log", SystemConfig()->FindArray(logFileSym)->Str(1));
    mLog = new TextFileStream(logFile, false);

    // yes there's just a random list here lol
    {
        std::list<int> tmp;
        for (int i = 0; i < 10000; i++) {
            tmp.push_front(0);
        }
    }

    Symbol dumpScsvSym("dump_scsv");
    int useSsv = SystemConfig()->FindArray(dumpScsvSym)->Int(1);
    gUseSsv = useSsv;
    StandardStream::sReportLargeTimerErrors = false;
}

void BudgetScreen::Enter(UIScreen *screen) {
    mCpuDist.Reset();
    mGsDist.Reset();
    mHudDist.Reset();
    mEtcDist.Reset();
    TheTaskMgr.ClearTasks();

    mTestPanel = nullptr;
    UIScreen::Enter(screen);
    mTestPanel = TypeDef()->FindArray("test_panel")->Obj<UIPanel>(1);

    TheRnd->SetGSTiming(true);
    TheRnd->BeginDrawing();
    TheRnd->EndDrawing();

    std::vector<float> cpuTimes;
    std::vector<float> gsTimes;

    for (int i = 0; i < 100; i++) {
        TheRnd->BeginDrawing();
        TheRnd->EndDrawing();

        TheRnd->BeginDrawing();
        cpuTimes.push_back(AutoTimer::GetTimer("cpu")->GetLastMs());
        gsTimes.push_back(AutoTimer::GetTimer("gs")->GetLastMs());
        TheRnd->EndDrawing();
    }

    mNullCpu = Average(cpuTimes, false);
    mNullGs = Average(gsTimes, true);

    TheTaskMgr.SetSeconds(0, false);

    TheRnd->BeginDrawing();
    mTestPanel->Draw();
    TheRnd->EndDrawing();

    // TODO: This doesn't seem quite right...
    // Should probably be mEndTime and mFrameInc
    mTime = TheSongDB->GetSongDurationMs() / 1000.0f;
    mEndTime = Property("frame_inc", true)->Float();

    mLastGpu = 0;
    mLastCpu = 0;
    mSampleCount = 0;

    DataArray *test = mTests->Array(mTestIdx)->FindArray("init", false);
    if (test != nullptr) {
        test->ExecuteScript(1, nullptr, nullptr, 1);
    }
}
inline float GetLastTimerMs(const char *name) {
    return AutoTimer::GetTimer(name)->GetLastMs();
}

void BudgetScreen::Poll() {
    UIScreen::Poll();
    START_AUTO_TIMER("budget_screen_poll");

    static DataArray *timerScript = SystemConfig("rnd")->FindArray("timer_script", false);
    if (timerScript)
        timerScript->ExecuteScript(1, nullptr, nullptr, 1);

    float tick = TheSongDB->GetData()->GetTempoMap()->TimeToTick(
        TheTaskMgr.Seconds(TaskMgr::kRealTime) * 1000.0f
    );

    // needs to be used as a local variable
    Timer *slowFrameTimer = &Timer::sSlowFrameTimer;
    float slowFrameTime = slowFrameTimer->SplitMs();

    bool b = slowFrameTime > 0;
    b |= tick < mRecordStartTick || tick >= mRecordEndTick;

    float cpuMs = GetLastTimerMs("cpu") - mNullCpu;
    float animMs = GetLastTimerMs("anim");
    float worldMs = GetLastTimerMs("world");
    float pollMs = GetLastTimerMs("budget_screen_poll");
    float cpuDist = animMs + (cpuMs - worldMs - pollMs);

    float gsMs = GetLastTimerMs("gs") - mNullGs;

    float hudTrackMs = GetLastTimerMs("hud_track");
    float hudDist = animMs + hudTrackMs;

    float gameEtcMs = GetLastTimerMs("game_etc");

    if (!b) {
        if (mSampleCount > 0) {
            mCpuDist << (cpuDist + mLastCpu) / 2;
            mGsDist << (gsMs + mLastGpu) / 2;
            mHudDist << (hudDist + mLastHud) / 2;
            mEtcDist << (gameEtcMs + mLastEtc) / 2;
        } else {
            mCpuDist << cpuDist;
            mGsDist << gsMs;
            mHudDist << hudDist;
            mEtcDist << gameEtcMs;
        }

        mLastCpu = cpuDist;
        mLastGpu = gsMs;
        mLastHud = hudDist;
        mLastEtc = gameEtcMs;
        mSampleCount++;
    }

    if (tick >= mRecordEndTick) {
        if (!gUseSsv) {
            const char *testName = mTests->Array(mTestIdx)->Str(0);
            *mLog << "START TEST: " << testName;

            float gpuPctile = mGsDist.Pctile(0.99f);
            float cpuPctile = mCpuDist.Pctile(0.99f);
            *mLog << " [cpu " << cpuPctile << "] [gpu " << gpuPctile << "]\n";

            *mLog << "Cpu overhead: " << mNullCpu << "\n";
            *mLog << "Gs overhead: " << mNullGs << "\n";

            mLog->mFile.Flush();

            *mLog << "\nCpu ";
            mCpuDist.Report(*mLog, nullptr);
            *mLog << "\nGpu ";
            mGsDist.Report(*mLog, nullptr);
            *mLog << "\nHUD/Track ";
            mHudDist.Report(*mLog, nullptr);
            *mLog << "\nGame Etc. ";
            mEtcDist.Report(*mLog, nullptr);
        }

        gMainFree = HeapFreeSize("main");

        float cpuPctile = mCpuDist.Pctile(0.99f);
        if (cpuPctile > mWorstCpuPctile) {
            mWorstCpuPctile = cpuPctile;
            mWorstCpuName = mTests->Array(mTestIdx)->Str(0);
        }

        float gsPctile = mGsDist.Pctile(0.99f);
        if (gsPctile > mWorstGsPctile) {
            mWorstGsPctile = gsPctile;
            mWorstGsName = mTests->Array(mTestIdx)->Str(0);
        }

        UIScreen *stopScreen = ObjectDir::Main()->Find<UIScreen>("stop_budget", true);
        TheUI.GotoScreen(stopScreen, false, false);
    }
}

void BudgetScreen::EndTest() {
    MILO_ASSERT(mTestPanel->GetState() == UIPanel::kUnloaded, 415);

    int mainRam = (HeapFreeSize("main") - gMainFree) / 1024;

    if (gUseSsv) {
        *mLog
            << "Category;Test;CPU (99th pctile);GPU (99th pctile);CPU Overhead;"
            << "GPU Overhead;CPU Min;CPU Mean;CPU Max;GPU Min;GPU Mean;GPU Max;"
            << "Main RAM";
        *mLog << "\n";
        *mLog << "Summary";

        *mLog << ";" << mTests->Array(mTestIdx)->Str(0);
        *mLog << ";" << mCpuDist.Pctile(0.99f);
        *mLog << ";" << mGsDist.Pctile(0.99f);
        *mLog << ";" << mNullCpu;
        *mLog << ";" << mNullGs;

        *mLog << ";" << mCpuDist.mMin;
        *mLog << ";" << (mCpuDist.mCount != 0 ? mCpuDist.mTotal / mCpuDist.mCount : 0.f);
        *mLog << ";" << mCpuDist.mMax;
        *mLog << ";" << mGsDist.mMin;
        *mLog << ";" << (mGsDist.mCount != 0 ? mGsDist.mTotal / mGsDist.mCount : 0.f);
        *mLog << ";" << mGsDist.mMax;
        *mLog << ";" << mainRam;
        *mLog << "\n\n";

        *mLog << "Category;Range (ms);Frames;Percentile\n";

        const char *testName = mTests->Array(mTestIdx)->Str(0);
        String cpuStr(testName); cpuStr += " (CPU distribution)";
        mCpuDist.Report(*mLog, cpuStr);
        String gsStr(testName); gsStr += " (GS distribution)";
        mGsDist.Report(*mLog, gsStr);
        String hudStr(testName); hudStr += " (HUD/Track CPU distribution)";
        mHudDist.Report(*mLog, hudStr);
        String etcStr(testName); etcStr += " (Game Etc. CPU distribution)";
        mEtcDist.Report(*mLog, etcStr);
        *mLog << "\n";
    } else {
        *mLog << "Main RAM: " << mainRam << "\n";
        *mLog << "END TEST\n\n";
    }

    mLog->mFile.Flush();

    UIScreen *startScreen = ObjectDir::Main()->Find<UIScreen>("start_budget", true);
    TheUI.GotoScreen(startScreen, false, false);
}

void BudgetScreen::NextTest() {
    mTestIdx++;
    if (mTestIdx < mTests->Size()) {
        DataArray *test = mTests->Array(mTestIdx);

        if (mWorstOnly) {
            Symbol worst("worst");
            if (test->FindArray(worst, true)->Int(1) == 0) {
                NextTest();
            }
        }

        TheNet.GetNetSession()->Clear();

        Symbol startSym("start");
        mRecordStartTick = test->FindArray(startSym, true)->Int(1);

        Symbol endSym("end");
        mRecordEndTick = test->FindArray(endSym, true)->Int(1);

        DataArray *sevenArr = test->FindArray(Symbol("seven_player_mode"), false);
        bool sevenPlayerMode = sevenArr && sevenArr->Int(1) != 0;

        if (sevenPlayerMode) {
            static Message msg(enable_auto_vocals);
            TheModifierMgr->Handle(msg.mData, true);
        } else {
            TheModifierMgr->DisableAutoVocals();
        }

        DataArray *players = test->FindArray(Symbol("players"), true);
        for (int i = 1; i < players->Size(); i++) {
            LocalBandUser *user = TheBandUserMgr->GetUserFromPad(i - 1);
            TheSessionMgr->AddLocalUser(user);
            Symbol trackSym = players->Sym(i);
            user->SetTrackType(trackSym);
            user->SetControllerType(TrackTypeToControllerType(user->GetTrackType()));
            user->SetDifficulty(kDifficultyExpert);

            if (user->GetTrackType() == kTrackDrum) {
                DataArray *proDrumArr = test->FindArray(Symbol("pro_drums"), false);
                bool proDrums = proDrumArr && proDrumArr->Int(1) != 0;
                if (proDrums) {
                    user->SetPreferredScoreType(kScoreRealDrum);
                    TheProfileMgr.SetCymbalConfiguration(0x1C);
                } else {
                    user->SetPreferredScoreType(kScoreDrum);
                    TheProfileMgr.SetCymbalConfiguration(0);
                }
            }
        }

        std::vector<Symbol> songs;
        Symbol songSym("song");
        Symbol songName = test->FindArray(songSym, true)->Sym(1);
        songs.push_back(songName);
        MetaPerformer::Current()->SetSongs(songs);

        TheGameConfig->AutoAssignMissingSlots();

        DataArray *preInit = test->FindArray(Symbol("pre_init"), false);
        if (preInit) {
            preInit->ExecuteScript(1, nullptr, nullptr, 1);
        }
    } else {
        if (gUseSsv) {
            Symbol columnInfoSym("column_info");
            LogColumnInfo(mLog, SystemConfig(columnInfoSym), true);
            *mLog << "Category;CategoryName;AlwaysShow\n";
            *mLog << "category_info;Summary;1\n";
            for (int i = 1; i < mTests->Size(); i++) {
                const char *testName = mTests->Array(i)->Str(0);
                String cpuName(testName);
                cpuName += " (CPU distribution)";
                *mLog << "category_info;";
                *mLog << cpuName;
                *mLog << ";0\n";
                String gsName(testName);
                gsName += " (GS distribution)";
                *mLog << "category_info;";
                *mLog << gsName;
                *mLog << ";0\n";
            }
        } else {
            *mLog << "\nWORST CASE 99th PCTILES:\n";
            *mLog << "   CPU: ";
            *mLog << mWorstCpuName;
            *mLog << " @ ";
            *mLog << mWorstCpuPctile;
            *mLog << " ms/frame\n";
            *mLog << "   GS: ";
            *mLog << mWorstGsName;
            *mLog << " @ ";
            *mLog << mWorstGsPctile;
            *mLog << " ms/frame\n";
        }
        *mLog << "\nDone\n";
        mLog->mFile.Flush();
        delete mLog;
        FormatString msg("TestIsFinishedNow\n");
        TheDebug << msg.Str();
        TheDebug.Exit(0, true);
    }
}

// Must be down here to avoid data pooling in Poll
int gMainFree;

int BudgetScreen::HeapFreeSize(const char *name) {
    int total = 0;
    for (int i = 0; i < MemNumHeaps(); i++) {
        if (streq(MemHeapName(i), name)) {
            int a, b, free, d;
            MemFreeBlockStats(i, a, b, free, d);
            total += free;
        }
    }
    return total;
}

Symbol end_test("end_test");
Symbol next_test("next_test");

BEGIN_HANDLERS(BudgetScreen)
    HANDLE_ACTION(end_test, EndTest())
    HANDLE_ACTION(next_test, NextTest())
    HANDLE_SUPERCLASS(UIScreen)
    HANDLE_CHECK(663)
END_HANDLERS

BEGIN_PROPSYNCS(BudgetScreen)
    SYNC_PROP(test_panel, mTestPanel)
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

void Hmx::Object::PreLoad(BinStream &bs) { Load(bs); }
