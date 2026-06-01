#include "utl/MultiTempoTempoMap.h"
#include "os/Debug.h"
#include "math/Utl.h"
#include "utl/MemMgr.h"
#include "utl/Std.h"
#include <algorithm>

MultiTempoTempoMap::MultiTempoTempoMap() : mStartLoopTick(-1.0f), mEndLoopTick(-1.0f) {}

MultiTempoTempoMap::~MultiTempoTempoMap() {}

float MultiTempoTempoMap::GetTempo(int tick) const {
    const TempoInfoPoint *pt = PointForTick(tick);
#ifdef HX_NATIVE
    if (pt != nullptr)
#else
    if (pt != mTempoPoints.end())
#endif
        return (float)pt->mTempo / 1000.0f;
    else
        return 800.0f;
}

int MultiTempoTempoMap::GetTempoInMicroseconds(int tick) const {
    const TempoInfoPoint *pt = PointForTick(tick);
#ifdef HX_NATIVE
    if (pt != nullptr)
#else
    if (pt != mTempoPoints.end())
#endif
        return pt->mTempo;
    else
        return 800000;
}

float MultiTempoTempoMap::GetTempoBPM(int tick) const {
    return 60000.0f / GetTempo(tick);
}

// float ConvertTimeBase(float value, float srcStart, float srcEnd, float destStart, float
// destEnd,) {
//     float loopTickLength = srcEnd - srcStart;
//     float loopTick = value - srcEnd;
//     float loopPercent = std::floor(loopTick / loopTickLength);

//     float loopTimeLength = destEnd - destStart;
//     float loopTime = loopTimeLength * loopPercent + destEnd;
//     loopTime += TickToTime(srcStart + -(loopTickLength * loopPercent - loopTick)) -
//     destStart; return loopTime;
// }

float MultiTempoTempoMap::TickToTime(float tick) const {
    if (tick == 0.0f)
        return 0.0f;

    // need to load up-front to prevent a re-load of the value in the `else` block
    float startTick = mStartLoopTick;

    if (startTick < 0.0f || tick <= mEndLoopTick) {
        const TempoInfoPoint *pt = PointForTick(tick);
#ifdef HX_NATIVE
        if (pt == nullptr)
#else
        if (pt == mTempoPoints.end())
#endif
            return 0.0f;
        else
            return pt->mMs
                + (pt->mTempo * ((tick - (float)pt->mTick) / 480.0f) / 1000.0f);
    } else {
        float loopTickLength = mEndLoopTick - startTick;
        float loopTick = tick - mEndLoopTick;
        float loopPercent = std::floor(loopTick / loopTickLength);

        float loopTimeLength = mEndLoopTime - mStartLoopTime;
        float loopTime = loopTimeLength * loopPercent + mEndLoopTime;
        loopTime += TickToTime(startTick + -(loopTickLength * loopPercent - loopTick))
            - mStartLoopTime;
        return loopTime;
    }
}

float MultiTempoTempoMap::TimeToTick(float time) const {
    if (time == 0.0f)
        return 0.0f;

    // startTick pre-loaded; endTick and endTime loaded lazily in condition
    float startTick = mStartLoopTick;
    float startTime;
    float endTick;
    float endTime;

    if (startTick < 0.0f || (endTick = mEndLoopTick) < 0.0f
        || time <= (endTime = mEndLoopTime)) {
        const TempoInfoPoint *pt = PointForTime(time);
        return pt->mTick + ((time - pt->mMs) * 1000.0f / (float)pt->mTempo) * 480.0f;
    } else {
        startTime = mStartLoopTime;

        float loopTimeLength = endTime - startTime;
        float loopTime = time - endTime;
        float loopPercent = std::floor(loopTime / loopTimeLength);

        float loopTickLength = endTick - startTick;
        float loopTick = loopTickLength * loopPercent + endTick;
        loopTick += TimeToTick(startTime + -(loopTimeLength * loopPercent - loopTime))
            - mStartLoopTick;
        return loopTick;
    }
}

// fn_80358694
bool MultiTempoTempoMap::AddTempoInfoPoint(int tick, int tempo) {
    if (mTempoPoints.empty()) {
        if (tick != 0) {
            return false;
        }
    } else if (tick < mTempoPoints.back().mTick) {
        return false;
    }

    MemDoTempAllocations tmp(true, false);
    mTempoPoints.push_back(TempoInfoPoint(TickToTime(tick), tick, tempo));
    return true;
}

void MultiTempoTempoMap::ClearLoopPoints() {
    mStartLoopTick = -1.0f;
    mEndLoopTick = -1.0f;
    mStartLoopTime = -1.0f;
    mEndLoopTime = -1.0f;
}

void MultiTempoTempoMap::SetLoopPoints(int start, int end) {
    mStartLoopTick = start;
    mEndLoopTick = end;
    mStartLoopTime = TickToTime(mStartLoopTick);
    mEndLoopTime = TickToTime(mEndLoopTick);
}

int MultiTempoTempoMap::GetLoopTick(int tick, int &asdf) const {
    float startLoopTick = mStartLoopTick;
    if (startLoopTick < 0.0f) {
        return tick;
    }

    float endLoopTick = mEndLoopTick;
    int startTick = startLoopTick;
    int endTick = endLoopTick;

    asdf = 0;
    if (!(tick < endLoopTick)) {
        if (startLoopTick == endLoopTick) {
            return tick;
        }

        int loopTick = tick - startTick;
        int loopLength = endTick - startTick;
        int newTick = (loopTick % loopLength) + startTick;
        asdf = tick - newTick;
        return newTick;
    }
}

int MultiTempoTempoMap::GetLoopTick(int tick) const {
    int ok;
    return GetLoopTick(tick, ok);
}

float MultiTempoTempoMap::GetTimeInLoop(float time) {
    if (mStartLoopTick == -1.0f) {
        return time;
    }

    float startTime = TickToTime(mStartLoopTick);
    if (time < startTime) {
        return time;
    }

    float endTime = TickToTime(mEndLoopTick);

    float loopLength = endTime - startTime;
    float timeFromStart = time - startTime;
    MILO_ASSERT(timeFromStart >= 0.0f, 0xE3);

    float a = std::floor(timeFromStart / loopLength);
    return startTime + -(loopLength * a - timeFromStart);
}

int MultiTempoTempoMap::GetNumTempoChangePoints() const { return mTempoPoints.size(); }

int MultiTempoTempoMap::GetTempoChangePoint(int index) const {
    MILO_ASSERT(index < mTempoPoints.size(), 0xF7);
    return mTempoPoints[index].mTick;
}

void MultiTempoTempoMap::Finalize() { TrimExcess(mTempoPoints); }

const MultiTempoTempoMap::TempoInfoPoint *MultiTempoTempoMap::PointForTick(float tick
) const {
    TempoInfoPoint pt;
    pt.mMs = tick;

    if (mTempoPoints.empty()) {
        MILO_WARN("Tempo map is empty; at least one tempo map entry is required");
#ifdef HX_NATIVE
        return nullptr;
#else
        return mTempoPoints.end();
#endif
    }

#ifdef HX_NATIVE
    auto it2 =
        std::upper_bound(mTempoPoints.begin(), mTempoPoints.end(), pt.mMs, CompareTick);
    if (it2 != mTempoPoints.begin())
        it2--;
    return &*it2;
#else
    const TempoInfoPoint *pt2 =
        std::upper_bound(mTempoPoints.begin(), mTempoPoints.end(), pt.mMs, CompareTick);
    if (pt2 != mTempoPoints.begin()) {
        pt2--;
    }
    return pt2;
#endif
}

const MultiTempoTempoMap::TempoInfoPoint *MultiTempoTempoMap::PointForTime(float time
) const {
    TempoInfoPoint pt;
    pt.mMs = time;
    MILO_ASSERT(mTempoPoints.size() >= 1, 0x121);

#ifdef HX_NATIVE
    auto it2 =
        std::upper_bound(mTempoPoints.begin(), mTempoPoints.end(), pt.mMs, CompareTime);
    if (it2 != mTempoPoints.begin())
        it2--;
    return &*it2;
#else
    const TempoInfoPoint *pt2 =
        std::upper_bound(mTempoPoints.begin(), mTempoPoints.end(), pt.mMs, CompareTime);
    if (pt2 != mTempoPoints.begin()) {
        pt2--;
    }
    return pt2;
#endif
}

bool MultiTempoTempoMap::CompareTick(
    float tick, const MultiTempoTempoMap::TempoInfoPoint &pt
) {
    return tick < pt.mTick;
}

bool MultiTempoTempoMap::CompareTime(
    float time, const MultiTempoTempoMap::TempoInfoPoint &pt
) {
    return time < pt.mMs;
}
