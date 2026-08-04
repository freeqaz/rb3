#include "utl/EncryptXTEA.h"

#include <string.h>

XTEABlockEncrypter::XTEABlockEncrypter() {
    mNonce[0] = 0;
    mNonce[1] = 0;
}

void XTEABlockEncrypter::SetKey(const unsigned char *uc) { memcpy(mKey, uc, 0x10); }

void XTEABlockEncrypter::SetNonce(const unsigned long long *nonce, unsigned int shift) {
    mNonce[0] = nonce[0] + shift;
    mNonce[1] = nonce[1] + shift;
}

void XTEABlockEncrypter::Encrypt(const XTEABlock *in, XTEABlock *out) {
    for (int i = 0; i < 2; i++) {
        out->mData[i] = in->mData[i] ^ Encipher(mNonce[i], mKey);
        mNonce[i]++;
    }
}

unsigned long long
XTEABlockEncrypter::Encipher(unsigned long long nonce, unsigned int *key) {
    unsigned long v0 = nonce >> 32;
    unsigned long v1 = nonce & 0xFFFFFFFF;
    unsigned int sum = 0;
    for (int i = 0; i < 4; i++) {
        v1 += (v0 + (v0 << 4 ^ v0 >> 5)) ^ sum + (key[(sum & 3)]);
        sum += 0x9E3779B9;
        v0 += (v1 + (v1 << 4 ^ v1 >> 5)) ^ sum + key[(sum >> 11) & 3];
    }
    return ((unsigned long long)v0 << 32) | v1;
}
