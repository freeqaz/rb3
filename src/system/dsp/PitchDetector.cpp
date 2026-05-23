#include "dsp/PitchDetector.h"
#include "dsp/IIRFilter.h"
#include "obj/Data.h"
#include "os/Debug.h"
#include "utl/MemMgr.h"
#include "utl/Symbol.h"
#include <math.h>
#include <string.h>

// dtk-extracted helpers in SndAnalysis.cpp / this TU
float ShiftedDotProduct(const float *buf, int len, float *out, bool extra);
int FindCCPeak(const float *autocorr, const float *peaks, int len, int minPeriod);
float RefinePeriod2(const float *buf, const float *autocorr, const float *peaks, int len, int period);
void dump(float *data, int len);

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
    static DataNode &PD_FLOOR_BOTTOM = DataVariable("PD_FLOOR_BOTTOM");
    static DataNode &PD_FLOOR_RATIO = DataVariable("PD_FLOOR_RATIO");
    static DataNode &PD_FLOOR_TOP = DataVariable("PD_FLOOR_TOP");
    static DataNode &PD_GATE_RATIO = DataVariable("PD_GATE_RATIO");
    static DataNode &PD_FLOOR_SECONDS = DataVariable("PD_FLOOR_SECONDS");
    static DataNode &PD_FIXED_GAIN = DataVariable("PD_FIXED_GAIN");
    static int sDump = 0;

    int overlap = mFrameSize - (mIdx - (mIdx / mFrameSize) * mFrameSize) - numSamples;
    int dec_size = (overlap - 1) / mFrameSize + 1;
    if (overlap == 0 || numSamples == (overlap - (overlap - 1) / mFrameSize * mFrameSize)) {
        dec_size = 0;
    }

    float lastVal = 0.0f;
    int ixDecim = 0;
    int begIxDecim = 0;
    if (dec_size > 0 && dec_size < mFrameSize) {
        int keep = mFrameSize - dec_size;
        memcpy(mDecimBuf, mDecimBuf + dec_size, keep * 4);
        lastVal = mCorrBuf[dec_size - 1];
        int i = 0;
        if (keep > 0) {
            for (; i < keep; i++) {
                mCorrBuf[i] = mCorrBuf[i + dec_size] - lastVal;
            }
        }
        MILO_ASSERT(keep > 0, 0xBA);
        lastVal = mCorrBuf[keep - 1];
    }

    if (dec_size > mFrameSize) {
        int extra = (dec_size - mFrameSize) * mDecimRate;
        dec_size = mFrameSize;
        numSamples -= extra;
        samples += extra * 2;
    }

    mFilter->Begin();
    begIxDecim = ixDecim;
    float decimAccum = gain * mFilter->FilterSlow((float)samples[0]);
    int sampleIdx = 0;
    static float kPropFilter = 0.3f;
    int decimWriteIdx = ixDecim * 4;
    while (sampleIdx < numSamples) {
        float filtered = mFilter->FilterSlow((float)samples[0]) * gain;
        if (ixDecim < mFrameSize) {
            if (((sampleIdx + mIdx) % mDecimRate) == 0) {
                mDecimBuf[ixDecim] = decimAccum;
                float sq = decimAccum * decimAccum + lastVal;
                mCorrBuf[ixDecim] = sq;
                lastVal = mCorrBuf[ixDecim];
                ixDecim++;
            }
            decimAccum = kPropFilter * (filtered - decimAccum) + decimAccum;
        }
        samples += 1;
        sampleIdx += 1;
    }
    mFilter->End();
    if (sDump) {
        dump(mDecimBuf, 192);
    }

    MILO_ASSERT(ixDecim - begIxDecim == dec_size, 0x102);

    int newIdx = (numSamples + mIdx);
    mIdx = newIdx - (newIdx / mDecimRate) * mDecimRate;
    float level = sqrt(mCorrBuf[mFrameSize - 1]);
    unk34 = (float)level / ((float)(mFrameSize) - 0.0f);
    if (mEnablePitchDetection) {
        ShiftedDotProduct(mDecimBuf, mFrameSize, mPeakBuf, true);
        int period = FindCCPeak(mPeakBuf, mCorrBuf, mFrameSize, mMaxPeriod);
        unk30_period = RefinePeriod2(mDecimBuf, mCorrBuf, mPeakBuf, mFrameSize, period);
    }

    float floorRatio = 0.1f;
    if (PD_FLOOR_RATIO.Float(NULL) > 0.0f) {
        floorRatio = PD_FLOOR_RATIO.Float(NULL);
    }
    float floorBottom = 0.06f;
    if (PD_FLOOR_BOTTOM.Float(NULL) > 0.0f) {
        floorBottom = PD_FLOOR_BOTTOM.Float(NULL);
    }
    float floorTop = 5.0f;
    if (PD_FLOOR_TOP.Float(NULL) > 0.0f) {
        floorTop = PD_FLOOR_TOP.Float(NULL);
    }
    float gateRatio = 2.0f;
    if (PD_GATE_RATIO.Float(NULL) > 0.0f) {
        gateRatio = PD_GATE_RATIO.Float(NULL);
    }
    float fixedGain = 10.0f;
    if (PD_FIXED_GAIN.Float(NULL) > 0.0f) {
        fixedGain = PD_FIXED_GAIN.Float(NULL);
    }
    float floorSeconds = 10.0f;
    if (PD_FLOOR_SECONDS.Float(NULL) > 0.0f) {
        floorSeconds = PD_FLOOR_SECONDS.Float(NULL);
    }

    if (floorSeconds != unk4C) {
        float alpha = 1.0f;
        if (floorSeconds > 0.0f) {
            alpha = 1.0f - (float)exp(-1.0f / (floorSeconds * 60.0f));
        }
        unk48 = alpha;
        unk4C = floorSeconds;
    }

    if ((unsigned)unk14 > 60) {
        if (unk34 > 0.0f) {
            float candidate = unk34 * floorRatio;
            if (candidate < unk38) {
                unk38 = candidate;
            } else if (unk30_period == 0.0f || unk34 >= unk38 * gateRatio) {
                unk38 = unk38 + unk48 * (unk34 - unk38);
            }
        } else if (unk30_period == 0.0f || unk34 >= unk38 * gateRatio) {
            unk38 = unk38 + unk48 * (unk34 - unk38);
        }
    }

    if (floorBottom > unk38) {
        unk38 = floorBottom;
    }
    if (floorTop < unk38) {
        unk38 = floorTop;
    }

    if (unk34 < unk38 * gateRatio) {
        unk30_period = 0.0f;
        unk34 = 0.0f;
    } else if (unk34 > 30.0f) {
        unk34 = 30.0f;
    }

    if (unk30_period == 0.0f) {
        unk2C = 0.0f;
    } else {
        float pitchHz = (float)mSamplesPerSec / (float)mDecimRate / unk30_period;
        if (pitchHz <= 0.0f) {
            confidenceOut = 0.0f;
            pitchOut = 0.0f;
            unk2C = 0.0f;
            unk30_period = 0.0f;
            return;
        }
        unk2C = 39.863136f + -36.376316f * (float)log10(pitchHz);
    }
    unk14++;
    unk18 += numSamples;
    pitchOut = unk2C;
    confidenceOut = fixedGain * pitchHint * unk34 / unk38;
    gateOut = unk34;
}
