#include "dsp/VibratoDetector.h"
#include <string.h>



VibratoDetector::VibratoDetector(int i1, int i2)
    : mBufIdx(0), mHi(0), mY0(0.0), mY1(0.0), mY2(0.0), mSample(0) ,mLastDetect(0),
    mMaxPeriod(i2), mMinPeriod(i1)

    {
        memset(mBuffer, 0, 20);
        memset(mPitches , 0, 20);
    }

VibratoDetector::~VibratoDetector() {

}

int VibratoDetector::Analyze(float f1) {
    int vibratoLength = 0;
    if (f1 == 0.0f) {
        ++mSample;
        return 0;
    }

    else {
        mY0 = (0.300000001f * f1) + (1.0f - 0.300000001f) * mY0;
        if(mY1 > mY0 && mY1 > mY2 || (mY1 < mY0) && mY1 < mY2) {
            mBuffer[mBufIdx % 5] = mSample;
            mPitches[mBufIdx % 5] = mY1;
            mBufIdx++;
            int result = Detect();
            if (result) {
                int elapsed = mSample - mLastDetect;
                vibratoLength = (elapsed < result) ? elapsed : result;
                mLastDetect = mSample;
            }
        }
        mY2 = mY1;
        mY1 = mY0;
        ++mSample;
        return vibratoLength;
    }
}

int VibratoDetector::Detect() {

}
