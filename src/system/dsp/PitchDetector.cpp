#include "dsp/PitchDetector.h"
#include "dsp/IIRFilter.h"
#include "os/Debug.h"
#include "utl/MemMgr.h"
#include <string.h>

PitchDetector::PitchDetector(int sampleRate) {
    mSamplesPerSec = 0;
    unk14 = 0;
    unk18 = 0;
    mDecimBuf = 0;
    mCorrBuf = 0;
    mPeakBuf = 0;
    unk34 = 0.0f;
    unk38 = 5.0f;
    mEnablePitchDetection = true;
    unk40 = 0;
    unk44 = 1.0f;
    unk48 = 0.0f;
    unk4C = -1.0f;
    SetSampleRate(sampleRate);
    float b[5] = {
        0.046581834f, 0.186327335f, 0.279491007f, 0.186327335f, 0.046581834f
    };
    float a[5] = {
        1.0f, -0.781814635f, 0.680165708f, -0.182484567f, 0.030120272f
    };
    mFilter = new IIR4PoleFilter(b, a);
}

PitchDetector::~PitchDetector() {
    Deallocate();
    delete mFilter;
}

void PitchDetector::Deallocate() {
    _MemFree(mDecimBuf);
    _MemFree(mCorrBuf);
    _MemFree(mPeakBuf);
}

void PitchDetector::SetSampleRate(int sampleRate) {
    if (mSamplesPerSec != sampleRate) {
        mSamplesPerSec = sampleRate;
        MILO_ASSERT(sampleRate, 0x1B2);
        mDecimRate = sampleRate / 6000;
        int decimated = mSamplesPerSec / mDecimRate;
        mMaxPeriod = decimated / 1320;
        mFrameSize = (((decimated / 65) * 2) + 15) & ~15;
        Deallocate();
        mDecimBuf = (float *)_MemAlloc(mFrameSize * 4, 0x10);
        mCorrBuf = (float *)_MemAlloc(mFrameSize * 4, 0x10);
        mPeakBuf = (float *)_MemAlloc(mFrameSize * 4, 0x10);
        memset(mDecimBuf, 0, mFrameSize * 4);
        memset(mCorrBuf, 0, mFrameSize * 4);
        memset(mPeakBuf, 0, mFrameSize * 4);
        mIdx = 0;
    }
}

void PitchDetector::AnalyzeBlock(
    const char *label,
    short *samples,
    int numSamples,
    float gain,
    float pitchHint,
    float &pitchOut,
    float &confidenceOut,
    float &gateOut
) {
    pitchOut = -1.0f;
    confidenceOut = 0.0f;
    gateOut = 0.0f;
}
