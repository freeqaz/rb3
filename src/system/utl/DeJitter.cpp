#include "DeJitter.h"
#include "obj/Data.h"

float DeJitter::sTimeScale = 1;

DeJitter::DeJitter() {
    Reset();
    unk_0x0.resize(32);
}

float DeJitter::Apply(float ms, float &delta) {
    float filteredValue = 1e30f;
    float sample = ms;

    if (sTimeScale != 1.0f) {
        if (unk_0x18 != 1e30f) {
            delta = ms - unk_0x18;
        } else {
            delta = 0.0f;
        }
        unk_0x18 = ms;
        delta *= sTimeScale;
        unk_0x14 += delta;
        return unk_0x14;
    }

    static DataNode &dejitter_disable = DataVariable("dejitter_disable");

    if (!dejitter_disable.Int()) {
        if (unk_0xC > 8) {
            int prevPos = (unk_0x8 - 1) & 0x1F;
            int historyPos = (prevPos - unk_0xC) & 0x1F;
            float f1 = (unk_0x0[prevPos] - unk_0x0[historyPos]) / (float)unk_0xC;
            if (unk_0x10 == 0.0f) {
                unk_0x10 = f1;
            }
            float f3 = unk_0x10;
            float f4 = unk_0x14;
            filteredValue = ms + 16.0f;
            f1 = (f1 - f3) * 0.1f + f3;
            unk_0x10 = f1;
            f1 = f4 + f1;
            if (f1 <= filteredValue) {
                filteredValue = ms - 16.0f;
                if (f1 >= filteredValue) {
                    filteredValue = f1;
                }
            }
            if (filteredValue < f4) {
                filteredValue = f4;
            }
        }
    }

    unk_0x0[unk_0x8] = sample;
    if (filteredValue != 1e30f) {
        sample = filteredValue;
    }
    unk_0x8 = (unk_0x8 + 1) & 0x1F;

    if (unk_0xC == -2) {
        delta = 16.666f;
    } else {
        delta = sample - unk_0x14;
    }

    if (unk_0xC < 30) {
        unk_0xC = unk_0xC + 1;
    }

    unk_0x14 = sample;
    return sample;
}

void DeJitter::Reset() {
    unk_0x8 = 0;
    unk_0xC = -2;
    unk_0x10 = 0;
    unk_0x14 = 0;
    unk_0x18 = 1e30;
}
