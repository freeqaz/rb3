// Timer's 64-bit cycle count, read one 32-bit word at a time.
//
// Timer::CyclesToMs and Timer::Ms() turn the count into milliseconds from its
// two words:
//
//     unsigned long lowCycles = cycles;
//     long highCycles = cycles >> 32;
//     return lowCycles * sLowCycles2Ms + highCycles * sHighCycles2Ms;
//
// Those are 4-byte types on the Wii. As 8-byte types (LP64: Linux, macOS) the
// "low word" is the whole count and the high word is added a second time, so
// from 2^32 cycles the reading is too large by high * 4,294,967 ms.
//
// Natively that is reachable. A cycle is a microsecond (HxNativeMftb), the
// factors Timer::Init leaves are sLowCycles2Ms = 1e-3 and sHighCycles2Ms =
// 1e-3 * UINT_MAX, and mCycles is a 64-bit sum of 32-bit differences
// (Split/Stop/Pause add `cycle - mStart`). A Timer that is started once and
// split every frame therefore passes 2^32 cycles after 71.6 minutes, and with
// 8-byte words it then gains 71.6 minutes in one frame, again every 71.6
// minutes. TaskMgr's seconds clock is such a Timer (TaskMgr::mTime, restarted
// once in TaskMgr::Init, split in every TaskMgr::Poll).
//
// The same defect and repair in dc3-decomp: 5c0055000,
// Lp64Literals.TimerCyclesToMs*. rb3 has the text twice (CyclesToMs and Ms()).

#include "test_helpers.h"

#include "obj/Task.h"
#include "os/Timer.h"

#include <climits>
#include <cstring>
#include <unistd.h>

namespace {

const long long kWord = 1LL << 32;

// 18.5 hours, 36.4 hours and 30 days of microseconds.
const long long kLongRuns[] = { 66647490000LL, 131072000000LL, 2592000000000LL };

// Counts with no high word.
const long long kShortRuns[] = { 0,       1,        999,          16667,
                                 1000000, 60000000, 3600000000LL, kWord - 1 };

class TimerLp64 : public ::testing::Test {
protected:
    // The real factors: whatever Timer::Init sets in this build.
    static void SetUpTestSuite() { Timer::Init(); }
};

// What the tests below rest on: a native cycle is a microsecond, and the high
// word's factor is not zero (a zero there would hide the second count).
TEST_F(TimerLp64, InitLeavesMicrosecondFactors) {
    EXPECT_EQ(Timer::sLowCycles2Ms, 1.0e-3f);
    EXPECT_EQ(Timer::sHighCycles2Ms, (float)(1.0e-3 * UINT_MAX));
    EXPECT_NEAR(Timer::sHighCycles2Ms, 4294967.295, 0.5);
}

TEST_F(TimerLp64, CyclesToMsCountsTheHighWordOnce) {
    // No step where the high word comes in.
    EXPECT_NEAR(Timer::CyclesToMs(kWord - 1), 4294967.295, 1.0);
    EXPECT_NEAR(Timer::CyclesToMs(kWord), 4294967.296, 1.0);
    for (long long cycles : kLongRuns) {
        const double ms = (double)cycles * 0.001;
        EXPECT_NEAR(Timer::CyclesToMs(cycles), ms, ms * 1e-6) << cycles << " cycles";
    }
}

// Ms() is its own copy of the same three lines, not a call to CyclesToMs.
TEST_F(TimerLp64, MsCountsTheHighWordOnce) {
    Timer timer;
    timer.mCycles = kWord - 1;
    EXPECT_NEAR(timer.Ms(), 4294967.295, 1.0);
    timer.mCycles = kWord;
    EXPECT_NEAR(timer.Ms(), 4294967.296, 1.0);
    for (long long cycles : kLongRuns) {
        timer.mCycles = cycles;
        const double ms = (double)cycles * 0.001;
        EXPECT_NEAR(timer.Ms(), ms, ms * 1e-6) << cycles << " cycles";
    }
}

// Below 2^32 cycles there is no high word: the reading is the low word's
// product, bit for bit, whichever width the two locals have.
TEST_F(TimerLp64, BelowTheHighWordIsTheLowWordProduct) {
    Timer timer;
    for (long long cycles : kShortRuns) {
        const float expected = (float)(unsigned int)cycles * 0.001f;
        const float fromStatic = Timer::CyclesToMs(cycles);
        timer.mCycles = cycles;
        const float fromMember = timer.Ms();
        EXPECT_EQ(memcmp(&fromStatic, &expected, sizeof(expected)), 0)
            << cycles << " cycles: CyclesToMs " << fromStatic << " vs " << expected;
        EXPECT_EQ(memcmp(&fromMember, &expected, sizeof(expected)), 0)
            << cycles << " cycles: Ms() " << fromMember << " vs " << expected;
    }
}

// How the count gets there: a running Timer 1 ms short of 2^32 cycles is split
// on the real clock. The sum carries into the high word (so mCycles is the
// true count, not a 32-bit one), and the reading moves by the few
// milliseconds slept, not by 71.6 minutes.
TEST_F(TimerLp64, SplitAcrossTheWordBoundaryDoesNotJump) {
    Timer timer;
    timer.Restart();
    timer.mCycles = kWord - 1000;
    const float before = timer.Ms();
    usleep(3000);
    timer.Split();
    ASSERT_GE(timer.mCycles, kWord) << "the split did not carry into the high word";
    const float after = timer.Ms();
    EXPECT_GE(after, before);
    // 60 s of slack for a stalled box; the defect is a step of 4,294,967 ms.
    EXPECT_LT(after - before, 60000.0f) << "before " << before << " ms, after " << after;
}

// The clock that runs that long: TaskMgr's automatic seconds, read from
// TaskMgr::mTime by the real TaskMgr::Poll.
class TimerLp64Engine : public EngineTestFixture {};

TEST_F(TimerLp64Engine, TaskMgrSecondsDoNotJumpAtTheWordBoundary) {
    const Timer savedTime = TheTaskMgr.mTime;
    const bool savedAuto = TheTaskMgr.mAutoSecondsBeats;
    const float savedSeconds = TheTaskMgr.Seconds(TaskMgr::kRealTime);
    const float savedBeat = TheTaskMgr.Beat();

    TheTaskMgr.mAutoSecondsBeats = true;
    TheTaskMgr.mTime.Restart();
    TheTaskMgr.mTime.mCycles = kWord - 1000000; // one second short of 2^32
    TheTaskMgr.Poll();
    const long long cyclesBefore = TheTaskMgr.mTime.mCycles;
    const float before = TheTaskMgr.Seconds(TaskMgr::kRealTime);

    TheTaskMgr.mTime.mCycles = kWord - 1000; // one millisecond short
    usleep(3000);
    TheTaskMgr.Poll();
    const long long cyclesAfter = TheTaskMgr.mTime.mCycles;
    const float after = TheTaskMgr.Seconds(TaskMgr::kRealTime);

    TheTaskMgr.mTime = savedTime;
    TheTaskMgr.mAutoSecondsBeats = savedAuto;
    TheTaskMgr.SetSecondsAndBeat(savedSeconds, savedBeat, true);
    TheTaskMgr.mAutoSecondsBeats = savedAuto;

    // Poll returns before the split when a fixed clock drives it
    // (RB3_FIXED_CLOCK, RB3_REPLAY_FIXED_CLOCK): then this measured nothing.
    ASSERT_LT(cyclesBefore, kWord);
    ASSERT_GE(cyclesAfter, kWord) << "TaskMgr::Poll did not split its Timer";
    EXPECT_NEAR(before, 4293.97, 1.0);
    // About one second passes on the clock; the defect is a step of 4,295 s.
    EXPECT_GE(after, before);
    EXPECT_LT(after - before, 60.0f) << "before " << before << " s, after " << after;
}

} // namespace
