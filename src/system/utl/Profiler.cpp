#include "Profiler.h"
#include "os/Debug.h"
#include "utl/MakeString.h"

Profiler::Profiler(const char *name, int countMax)
    : mName(name), mMin(3.4028235E+38f), mMax(0), mSum(0), mCount(0),
      mCountMax(countMax) {}

void Profiler::Start() { mTimer.Start(); }

extern const char *FormatTime(float);

void Profiler::Stop() {
    mTimer.Stop();
    float ms = mTimer.Ms();
    if (ms < mMin) {
        mMin = ms;
    }
    ms = mTimer.Ms();
    if (mMax < ms) {
        mMax = ms;
    }
    mSum += mTimer.Ms();
    mCount++;
    if (mCount == mCountMax) {
        if (mCountMax == 1) {
            TheDebug << MakeString("%s: %s\n", mName, FormatTime(mMin));
        } else {
            TheDebug << MakeString(
                "%s: min %s max %s mean %s\n",
                mName,
                FormatTime(mMin),
                FormatTime(mMax),
                FormatTime(mSum / (float)mCount)
            );
        }

        mCount = 0;
        mMin = 3.4028235E+38f;
        mMax = 0;
        mSum = 0;
    }

    mTimer.Reset();
}
