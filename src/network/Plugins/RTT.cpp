#include "RTT.h"

namespace Quazal {
    RTT::RTT(uint i) : unk_0x0(i * 8), unk_0x4(0), unk_0x8(i) {}

    RTT::~RTT() {}

    void RTT::Adjust(uint i) {
        uint oldSmoothed = unk_0x0;
        uint oldVariance = unk_0x4;
        unk_0x8 = i;
        int delta = (int)i - (int)(oldSmoothed >> 3);
        int sign = delta >> 31;
        unk_0x0 = oldSmoothed + delta;
        unk_0x4 = oldVariance + ((((sign ^ delta) - sign)) - (int)(oldVariance >> 2));
    }
}