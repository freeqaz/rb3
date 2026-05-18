#include "network/Plugins/EncryptionAlgorithm.h"
#include "Platform/qStd.h"

namespace Quazal {
    EncryptionAlgorithm::EncryptionAlgorithm(unsigned int minKeyLength, unsigned int maxKeyLength)
        : mMinKeyLength(minKeyLength), mMaxKeyLength(maxKeyLength) {}

    EncryptionAlgorithm::~EncryptionAlgorithm() {}

    bool EncryptionAlgorithm::SetKey(const Key &key) {
        if ((unsigned int)key.GetLength() >= mMinKeyLength && (unsigned int)key.GetLength() <= mMaxKeyLength) {
            mKey = key;
            KeyHasChanged();
            return true;
        }
        return false;
    }

    void EncryptionAlgorithm::KeyHasChanged() {}

    bool EncryptionAlgorithm::Encrypt(Buffer *buf) {
        Buffer tmp(0x400);
        bool ret = Encrypt(*buf, &tmp);
        if (!ret) *buf = tmp;
        return ret;
    }

    bool EncryptionAlgorithm::Decrypt(Buffer *buf) {
        Buffer tmp(0x400);
        bool ret = Decrypt(*buf, &tmp);
        if (!ret) *buf = tmp;
        return ret;
    }

    bool EncryptionAlgorithm::GetErrorString(unsigned int errorCode, char *out, unsigned int outSize) {
        if (strlen("Encryption Error") >= outSize)
            return false;
        strcpy(out, "Encryption Error");
        return true;
    }
}
