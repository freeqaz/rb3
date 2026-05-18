#include "network/Plugins/Key.h"

namespace Quazal {
    Key::Key() {}

    Key::Key(unsigned char *pbyContent, unsigned int uiLength) : mData(uiLength, 0) {
        for (unsigned int i = 0; i < uiLength; i++) {
            mData[i] = pbyContent[i];
        }
    }

    Key::~Key() {}

    Key &Key::operator=(const Key &key) {
        mData = key.mData;
        return *this;
    }

    int Key::GetLength() const { return mData.size(); }

    const unsigned char *Key::GetContentPtr() const {
        if (mData.empty())
            return nullptr;
        else
            return &mData.front();
    }

    unsigned char *Key::PrepareContentPtr(unsigned int size) {
        mData.resize(size);
        return &mData.front();
    }
}