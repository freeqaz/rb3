#include "rndobj/ColorXfm.h"
#include "math/Mtx.h"
#include "math/Rot.h"

RndColorXfm::RndColorXfm()
    : mHue(0.0f), mSaturation(0.0f), mLightness(0.0f), mContrast(0.0f), mBrightness(0.0f),
      mLevelInLo(0.0f, 0.0f, 0.0f, 1.0f), mLevelInHi(1.0f, 1.0f, 1.0f, 1.0f),
      mLevelOutLo(0.0f, 0.0f, 0.0f, 1.0f), mLevelOutHi(1.0f, 1.0f, 1.0f, 1.0f) {
    Reset();
}

void RndColorXfm::Reset() { mColorXfm.Reset(); }

int ModChan(int chan) {
    int i = chan % 3;
    if (i < 0)
        return i + 3;
    else
        return i;
}

void RndColorXfm::AdjustHue() {
    Transform tf68;
    tf68.Reset();
    float hue = mHue;
    if (hue >= 120.0f) {
        hue = ((hue - 120.0f) / 120.0f) * 1.5707964f;
        float cosHue = std::cos(hue);
        float sinHue = std::sin(hue);
        for (int i = 0; i < 3; i++) {
            tf68.m[i][i] = 0;
            tf68.m[ModChan(i + 1)][i] = cosHue;
            tf68.m[ModChan(i + 2)][i] = sinHue;
        }
    } else if (hue > 0) {
        hue = (hue / 120.0f) * 1.5707964f;
        float cosHue = std::cos(hue);
        float sinHue = std::sin(hue);
        for (int i = 0; i < 3; i++) {
            tf68.m[i][i] = cosHue;
            tf68.m[ModChan(i + 1)][i] = sinHue;
            tf68.m[ModChan(i + 2)][i] = 0;
        }
    } else if (hue <= -120.0f) {
        hue = ((-hue - 120.0f) / 120.0f) * 1.5707964f;
        float cosHue = std::cos(hue);
        float sinHue = std::sin(hue);
        for (int i = 0; i < 3; i++) {
            tf68.m[i][i] = 0;
            tf68.m[ModChan(i + 1)][i] = sinHue;
            tf68.m[ModChan(i + 2)][i] = cosHue;
        }
    } else if (hue < 0) {
        hue = (-hue / 120.0f) * 1.5707964f;
        float cosHue = std::cos(hue);
        float sinHue = std::sin(hue);
        for (int i = 0; i < 3; i++) {
            tf68.m[i][i] = cosHue;
            tf68.m[ModChan(i + 1)][i] = 0;
            tf68.m[ModChan(i + 2)][i] = sinHue;
        }
    }
    Multiply(mColorXfm, tf68, mColorXfm);
}

void RndColorXfm::AdjustSaturation() {
    Transform tf68;
    tf68.Reset();
    float sat = mSaturation / 100.0f;
    float one = 1.0f;
    if (sat > 0)
        sat = one + sat;
    else {
        sat = -sat * -0.6666666f + one;
    }
    float f2 = (one - sat) * 0.5f;
    for (int i = 0; i < 3; i++) {
        tf68.m[i][i] = sat;
        tf68.m[i][ModChan(i + 1)] = f2;
        tf68.m[i][ModChan(i + 2)] = f2;
    }
    Multiply(mColorXfm, tf68, mColorXfm);
}

void RndColorXfm::AdjustLightness() {
    Transform tf58;
    tf58.Reset();
    float lit = mLightness / 100.0f;
    float f1 = 0;
    float f3;
    if (lit >= 0) {
        f3 = 1.0f - lit;
        f1 = lit;
    } else {
        f3 = lit + 1.0f;
    }
    tf58.m[2][2] = f3;
    tf58.m[1][1] = f3;
    tf58.m[0][0] = f3;
    tf58.v.Set(f1, f1, f1);
    Multiply(mColorXfm, tf58, mColorXfm);
}

void RndColorXfm::AdjustContrast() {
    Transform tf58;
    tf58.Reset();
    float contrast = mContrast / 100.0f;
    float one = 1.0f;
    if (contrast > 0) {
        contrast = one / (contrast * -0.9921875f + one);
    } else {
        contrast = -contrast * -0.992126f + one;
    }
    float f2 = (one - contrast) * 0.5f;
    tf58.m[2][2] = contrast;
    tf58.m[1][1] = contrast;
    tf58.m[0][0] = contrast;
    tf58.v.Set(f2, f2, f2);
    Multiply(mColorXfm, tf58, mColorXfm);
}

void RndColorXfm::AdjustBrightness() {
    Transform tf;
    tf.Reset();
    float set = (mBrightness + 100.0f) / 200.0f + -0.5f;
    tf.v.Set(set, set, set);
    Multiply(mColorXfm, tf, mColorXfm);
}

void RndColorXfm::AdjustLevels() {
    float loBlue = mLevelInLo.blue;
    float loGreen = mLevelInLo.green;
    float diffBlue = mLevelInHi.blue - loBlue;
    float loRed = mLevelInLo.red;
    float diffGreen = mLevelInHi.green - loGreen;
    float diffRed = mLevelInHi.red - loRed;
    float f1 = diffBlue != 0
        ? (mLevelOutHi.blue - mLevelOutLo.blue) / diffBlue
        : 0;
    float f2 = diffGreen != 0
        ? (mLevelOutHi.green - mLevelOutLo.green) / diffGreen
        : 0;
    float f3 = diffRed != 0
        ? (mLevelOutHi.red - mLevelOutLo.red) / diffRed
        : 0;
    float v68z = f1 * -loBlue + mLevelOutLo.blue;
    float v68y = f2 * -loGreen + mLevelOutLo.green;
    float v68x = f3 * -loRed + mLevelOutLo.red;
    Transform tf40;
    tf40.m.x.Set(f3, 0, 0);
    tf40.m.y.Set(0, f2, 0);
    tf40.m.z.Set(0, 0, f1);
    tf40.v.Set(v68x, v68y, v68z);
    Multiply(mColorXfm, tf40, mColorXfm);
}

void RndColorXfm::AdjustColorXfm() {
    mColorXfm.Reset();
    AdjustHue();
    AdjustSaturation();
    AdjustLightness();
    AdjustContrast();
    AdjustBrightness();
    AdjustLevels();
}

bool RndColorXfm::Load(BinStream &bs) {
    int rev;
    bs >> rev;
    if (rev > 0)
        return false;
    else {
        bs >> mColorXfm;
        bs >> mHue >> mSaturation >> mLightness >> mContrast >> mBrightness;
        bs >> mLevelInLo >> mLevelInHi >> mLevelOutLo >> mLevelOutHi;
        return true;
    }
}
