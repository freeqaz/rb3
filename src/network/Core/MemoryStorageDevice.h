#pragma once
#include "Platform/qStd.h"
#include "Platform/RootObject.h"

namespace Quazal {
    class MemoryStorageDevice : public RootObject {
    public:
        MemoryStorageDevice();
        ~MemoryStorageDevice();

        void Write(const unsigned char *pData, unsigned int uiLength);
        void Read(unsigned char *pData, unsigned int uiLength);

        qVector<unsigned char> m_oBuffer;
        unsigned int m_uiReadPos;
    };
}
