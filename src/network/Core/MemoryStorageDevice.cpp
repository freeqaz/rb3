#include "network/Platform/qStd.h"

namespace Quazal {
    static qVector<unsigned char> s_oInstantiator;
    void MemoryStorageDevice_FillInsert(unsigned int uiPos, unsigned int uiLen,
                                        unsigned char ucFill) {
        s_oInstantiator.insert(s_oInstantiator.begin() + uiPos,
                               (qVector<unsigned char>::size_type)uiLen, ucFill);
    }
}
