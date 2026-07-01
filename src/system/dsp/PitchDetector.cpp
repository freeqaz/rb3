#include "dsp/PitchDetector.h"
#include "dsp/IIRFilter.h"
#include "obj/Data.h"
#include "os/Debug.h"
#include "utl/MakeString.h"
#include "utl/MemMgr.h"
#include "utl/Symbol.h"
#include <math.h>
#include <string.h>

// dtk-extracted helpers in SndAnalysis.cpp
float ShiftedDotProduct(const float *buf, int len, float *out, bool extra);
int FindCCPeak(const float *autocorr, const float *peaks, int len, int minPeriod);
float RefinePeriod2(const float *buf, const float *autocorr, const float *peaks, int len, int period);

void dump(float *data, int len) {
    const char *space = " ";
    for (int i = 0.0f; len > i; i++) {
        int n = (int)(data[i] / 700.0f) + 30;
        int j = 0;
        if (n > 0) {
            do {
                FormatString fs(space);
                TheDebug << fs.Str();
                j++;
            } while (j < n);
        }
        TheDebug << MakeString("* %d\n", i);
    }
}

PitchDetector::PitchDetector(int sampleRate) {
    mSamplesPerSec = 0;
    unk14 = 0;
    unk18 = 0;
    mDecimBuf = 0;
    mCorrBuf = 0;
    mPeakBuf = 0;
    mAveEnergy = 0.0f;
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
    static float kPropFilter = 0.3f;
    static int sDump = 0;

    int offset = (mDecimRate - mIdx) - (mDecimRate - mIdx) / mDecimRate * mDecimRate;
    int dec_size = (numSamples - offset - 1) / mDecimRate + 1;
    if (numSamples == 0 || numSamples == offset) {
        dec_size = 0;
    }

    float lastVal = 0.0f;
    int ixDecim = 0;
    if (dec_size != 0 && dec_size < mFrameSize) {
        int overlap = mFrameSize - dec_size;
        memcpy(mDecimBuf, mDecimBuf + dec_size, overlap * 4);
        lastVal = mCorrBuf[dec_size - 1];
        if (overlap > 0) {
            for (int i = 0; i < overlap; i++) {
                mCorrBuf[i] = mCorrBuf[i + dec_size] - lastVal;
            }
        }
        MILO_ASSERT(overlap>0, 0xBA);
        lastVal = mCorrBuf[overlap - 1];
    }

    if (dec_size > mFrameSize) {
        int extra = (dec_size - mFrameSize) * mDecimRate;
        dec_size = mFrameSize;
        numSamples -= extra;
        samples += extra;
    }
    int begIxDecim = ixDecim;

    mFilter->Begin();
    float decimAccum = gain * mFilter->FilterSlow((float)samples[0]);
    int writeOff = ixDecim * 4;
    int sampleIdx = 0;
    while (sampleIdx < numSamples) {
        decimAccum = kPropFilter * (mFilter->FilterSlow((float)samples[0]) * gain - decimAccum) + decimAccum;
        if (ixDecim < mFrameSize && ((sampleIdx + mIdx) % mDecimRate) == 0) {
            ixDecim++;
            *((float *)((char *)mDecimBuf + writeOff)) = decimAccum;
            float sq = decimAccum * decimAccum + lastVal;
            *((float *)((char *)mCorrBuf + writeOff)) = sq;
            lastVal = *((float *)((char *)mCorrBuf + writeOff));
            writeOff += 4;
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
    mAveEnergy = (float)level / ((float)(mFrameSize) - 0.0f);
    if (mEnablePitchDetection) {
        ShiftedDotProduct(mDecimBuf, mFrameSize, mPeakBuf, true);
        int period = FindCCPeak(mPeakBuf, mCorrBuf, mFrameSize, mMaxPeriod);
        mPeriod = RefinePeriod2(mDecimBuf, mCorrBuf, mPeakBuf, mFrameSize, period);
    }

    float floorRatio = 1.1f;
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
    float fixedGain = 12.0f;
    if (PD_FIXED_GAIN.Float(NULL) > 0.0f) {
        fixedGain = PD_FIXED_GAIN.Float(NULL);
    }
    float floorSeconds = 10.0f;
    if (PD_FLOOR_SECONDS.Float(NULL) > 0.0f) {
        floorSeconds = PD_FLOOR_SECONDS.Float(NULL);
    }

    if (floorSeconds != unk4C) {
        float alpha;
        if (floorSeconds > 0.0f) {
            alpha = 1.0f - (float)exp(-1.0f / (floorSeconds * 60.0f));
        } else {
            alpha = 1.0f;
        }
        unk48 = alpha;
        unk4C = floorSeconds;
    }

    if ((unsigned)unk14 > 60) {
        float level = mAveEnergy;
        float candidate;
        if ((level > 0.0f) && (candidate = level * floorRatio, candidate < unk38)) {
            unk38 = candidate;
        } else if (mPeriod == 0.0f || level < unk38 * gateRatio) {
            float floorVal = unk38;
            unk38 = unk48 * (floorTop - floorVal) + floorVal;
        }
    }

    float *floorBottomPtr;
    if (floorBottom < unk38) {
        floorBottomPtr = &unk38;
    } else {
        floorBottomPtr = &floorBottom;
    }
    unk38 = *floorBottomPtr;
    float *floorTopPtr;
    if (unk38 < floorTop) {
        floorTopPtr = &unk38;
    } else {
        floorTopPtr = &floorTop;
    }
    unk38 = *floorTopPtr;

    if (mAveEnergy < unk38 * gateRatio) {
        mPeriod = 0.0f;
        mAveEnergy = 0.0f;
    } else if (mAveEnergy > 30.0f) {
        mAveEnergy = 30.0f;
    }

    if (mPeriod == 0.0f) {
        mPitch = 0.0f;
    } else {
        float pitchHz = (float)(mSamplesPerSec / mDecimRate) / mPeriod;
        if (pitchHz <= 0.0f) {
            confidenceOut = 0.0f;
            pitchOut = 0.0f;
            mPitch = 0.0f;
            mPeriod = 0.0f;
            return;
        }
        mPitch = 39.863136f + -36.376316f * (float)log10(pitchHz);
    }
    unk14++;
    unk18 += numSamples;
    pitchOut = mPitch;
    confidenceOut = fixedGain * (pitchHint * mAveEnergy) / unk38;
    gateOut = mAveEnergy;
}

void PitchDetector::SetSampleRate(int sampleRate) {
    if (mSamplesPerSec != sampleRate) {
        mSamplesPerSec = sampleRate;
        MILO_ASSERT(mSamplesPerSec, 0x1B2);
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
