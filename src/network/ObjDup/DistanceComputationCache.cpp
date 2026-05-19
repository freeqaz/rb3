#include "ObjDup/DistanceComputationCache.h"

namespace Quazal {
    Time DistanceComputationCache::s_tMaximumUpdateDelay(1000);

    DistanceComputationCache::DistanceComputationCache() {
        m_unk4 = 0;
        m_unk8 = 0;
        m_unk14 = 0;
        m_unk10 = 0;
        m_fLastDistance = -1.0f;
    }

    DistanceComputationCache::~DistanceComputationCache() {}
}
