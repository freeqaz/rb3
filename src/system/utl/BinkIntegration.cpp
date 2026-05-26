#include "utl/BinkIntegration.h"
#include "obj/DataFile.h"
#include "os/File.h"
#include "os/Debug.h"
#include "os/Endian.h"
#include "os/Timer.h"
#include "KeyChain.h"
#include "synth/Synth.h"
#include "utl/EncryptXTEA.h"
#include <string.h>

extern "C" unsigned int RADTimerRead();

extern "C" {
    void BinkSetMemory(void *(*)(unsigned int), void (*)(void *));
    void BinkSetIO(bool (*)(BINKIO *, const char *, unsigned int));
    extern void *BinkOpenAX;
    void BinkSetSoundSystem(void *, void *);
    void AXSetCompressor(int);
}

struct RADARAMCALLBACKS {
    void *(*alloc)(unsigned int);
    void (*free)(void *);
};

void BinkInit() {
    BinkSetMemory(BinkAlloc, BinkFree);
    BinkSetIO(BinkFileOpen);
    RADARAMCALLBACKS aram_callbacks = {BinkAlloc, BinkFree};
    BinkSetSoundSystem(&BinkOpenAX, &aram_callbacks);
    AXSetCompressor(0);
}

void *BinkAlloc(unsigned int size) { _MemAlloc(size, 128); }

void BinkFree(void *mem) { _MemFree(mem); }

#ifdef __MWERKS__
asm void intelendian(register void *buf, register unsigned int length) {
    // r3 = buf, r4 = length
    addi    r0, r4, 3
    li      r4, 0
    srwi.   r5, r0, 2
    beqlr
    srwi.   r0, r5, 3
    mtctr   r0
    beq-    tail_setup
main_loop:
    lwbrx   r0, r4, r3
    stw     r0, 0(r3)
    addi    r3, r3, 4
    lwbrx   r0, r4, r3
    stw     r0, 0(r3)
    addi    r3, r3, 4
    lwbrx   r0, r4, r3
    stw     r0, 0(r3)
    addi    r3, r3, 4
    lwbrx   r0, r4, r3
    stw     r0, 0(r3)
    addi    r3, r3, 4
    lwbrx   r0, r4, r3
    stw     r0, 0(r3)
    addi    r3, r3, 4
    lwbrx   r0, r4, r3
    stw     r0, 0(r3)
    addi    r3, r3, 4
    lwbrx   r0, r4, r3
    stw     r0, 0(r3)
    addi    r3, r3, 4
    lwbrx   r0, r4, r3
    stw     r0, 0(r3)
    addi    r3, r3, 4
    bdnz+   main_loop
    andi.   r5, r5, 7
    beqlr
tail_setup:
    mtctr   r5
    nop
tail_loop:
    lwbrx   r0, r4, r3
    stw     r0, 0(r3)
    addi    r3, r3, 4
    bdnz+   tail_loop
    blr
}
#else
void intelendian(void *buf, unsigned int length) {
    unsigned int count = (length + 3) >> 2;
    unsigned int *p = (unsigned int *)buf;
    for (unsigned int i = 0; i < count; i++) {
        unsigned int v = p[i];
        p[i] = ((v & 0xff) << 24) | ((v & 0xff00) << 8) |
               ((v & 0xff0000) >> 8) | ((v >> 24) & 0xff);
    }
}
#endif

bool BinkFileOpen(BINKIO *bink, const char *cc, unsigned int ui) {
    memset(bink, 0, sizeof(BINKIO));
    if (ui & 0x800000) {
        const char **binkCCData =
            reinterpret_cast<const char **>(const_cast<unsigned char *>(&bink->iodata[0])
            );
        *binkCCData = cc;
    } else {
        File *file = NewFile(cc, 2);
        ((BINKFILE *)bink->iodata)->pFile = file;
        ((BINKFILE *)bink->iodata)->iCloseFile = 1;
        if (!file)
            return false;
    }
    bink->ReadHeader = BinkFileReadHeader;
    bink->ReadFrame = BinkFileReadFrame;
    bink->GetBufferSize = BinkFileGetBufferSize;
    bink->SetInfo = BinkFileSetInfo;
    bink->Idle = BinkFileIdle;
    bink->Close = BinkFileClose;
    bink->BGControl = BinkFileBGControl;
    return true;
}

#define BSWAP(i)                                                                         \
    (((i) & 0xff) << 24 | ((i) & 0xff00) << 8 | ((i) & 0xff0000) >> 8                    \
     | ((i) >> 24) & 0xff)

unsigned int BinkFileReadHeader(BINKIO *bink, int, void *header, unsigned int length) {
    File *file = ((BINKFILE *)bink->iodata)->pFile;
    BINKENCRYPTIONHEADER *encHeader = &((BINKFILE *)bink->iodata)->mEncryptionHeader;
    // check if we've read a file header before
    if (encHeader->mSignature == 0) {
        // read the header
        unsigned int encread = file->Read(encHeader, sizeof(BINKENCRYPTIONHEADER));
        // byteswap mSignature through mMagicB from bad endian to big endian
        intelendian(encHeader, 0x14);
        // check if the header is BIKE (after first swap, before nonce swap)
        int whatever = encHeader->mSignature - 0x45420000; // 'BI--'
        // byteswap mNonce (each 64-bit word independently)
        unsigned long long n0 = encHeader->mNonce[0];
        unsigned long long n1 = encHeader->mNonce[1];
        encHeader->mNonce[0] = EndianSwap(n0);
        encHeader->mNonce[1] = EndianSwap(n1);
        if (whatever == 0x494B) { // '--KE'
            XTEABlockEncrypter *decrypter = new XTEABlockEncrypter;
            ((BINKFILE *)bink->iodata)->pXTEADecrypter = decrypter;

            // perform the key derivation step
            DataArray *arr = DataReadString("{Na 42 'O32'}");
            unsigned int iEval = arr->Evaluate(0).Int();
            arr->Release();

            char i6 = (iEval % 13);
            i6 = i6 + 'A';
            char script[256];
            unsigned char masterKey[256];
            sprintf(script, "{%c %d %c}", i6, (int)masterKey ^ iEval, i6);
            DataArray *buf118Arr = DataReadString(script);
            buf118Arr->Evaluate(0);
            buf118Arr->Release();

            unsigned char key[0x10];
            KeyChain::getKey(encHeader->mKeyIndex, key, masterKey);
            TheSynth->mGrinder.GrindArray(
                encHeader->mMagicA, encHeader->mMagicB, key, 0x10, 0x1D
            );
            for (int i = 0; i < 16; i++) {
                key[i] ^= encHeader->mKeyMask[i];
            }

            // endian swap the key for use in XTEA and prepare the decrypter
            intelendian(key, 0x10);
            ((BINKFILE *)bink->iodata)->pXTEADecrypter->SetKey(key);
            ((BINKFILE *)bink->iodata)->pXTEADecrypter->SetNonce(encHeader->mNonce, 0);
            ((BINKFILE *)bink->iodata)->iFileBufPos += encread;
        } else {
            // it's not an encrypted BIK, seek back in the file and act like this never
            // happened
            memset(encHeader, 0, encread);
            file->Seek(-encread, SEEK_CUR);
            BINK *curBink = bink->bink;
            // only warn if this is a multitrack?
            if (curBink != NULL && curBink->NumTracks > 2 && curBink->Width <= 7) {
                MILO_WARN("Attempting read of unsecure Bink song file!\n");
            }
        }
    }
    // read the actual bink file header
    unsigned int r = file->Read(header, length);
    if (r != length) {
        bink->ReadError = 1;
    }
    ((BINKFILE *)bink->iodata)->iHeaderSize += r;
    ((BINKFILE *)bink->iodata)->iFileBufPos += r;
    unsigned int size = file->Size();
    unsigned int anothersize = size - ((BINKFILE *)bink->iodata)->iFileBufPos;
    bink->CurBufSize = (bink->BufSize < anothersize) ? bink->BufSize : anothersize;
    intelendian(header, r);
    return r;
}

void ReadFunc(BINKIO *bink, bool startNewRead) {
    BINKFILE *bf = (BINKFILE *)bink->iodata;
    if (bink->DoingARead) {
        int lengthRead = 0;
        if (!bf->pFile->ReadDone(lengthRead))
            return;
        bink->DoingARead = 0;
        if (bf->mEncryptionHeader.mVersion == 2) {
            START_AUTO_TIMER("XTEA");
            for (XTEABlock *blk = (XTEABlock *)bf->pBufBack;
                 (unsigned char *)blk < bf->pBufBack + lengthRead;
                 blk++) {
                XTEABlock outBlock;
                unsigned int *p = (unsigned int *)blk;
                unsigned int p0 = p[0];
                unsigned int p1 = p[1];
                p[1] = BSWAP(p0);
                p[0] = BSWAP(p1);
                unsigned int p2 = p[2];
                unsigned int p3 = p[3];
                p[3] = BSWAP(p2);
                p[2] = BSWAP(p3);
                bf->pXTEADecrypter->Encrypt(blk, &outBlock);
                unsigned int *outU = (unsigned int *)&outBlock;
                p[0] = outU[1];
                p[1] = outU[0];
                p[2] = outU[3];
                p[3] = outU[2];
            }
        } else {
            intelendian(bf->pBufBack, lengthRead);
        }
        bf->pBufBack += lengthRead;
        if (bf->pBufBack >= bf->pBufEnd) {
            bf->pBufBack = bf->pBuffer;
        }
        bf->iBufEmpty -= lengthRead;
        bink->CurBufUsed += lengthRead;
        bink->BytesRead += lengthRead;
        if (bink->CurBufUsed > bink->BufHighUsed) {
            bink->BufHighUsed = bink->CurBufUsed;
        }
        bf->iShowSpeed = RADTimerRead() - bf->iShowSpeed;
        bink->TotalTime += bf->iShowSpeed;
        if (bink->Suspended != 0)
            return;
    }
    if (startNewRead) {
        int fileSize = bf->pFile->Size();
        int filePos = bf->pFile->Tell();
        unsigned int diff = fileSize - filePos;
        if (bf->iBufEmpty >= 0x8000 && !bf->pFile->Eof()) {
            bink->DoingARead = 1;
            if (diff > 0x8000)
                diff = 0x8000;
            bf->pFile->ReadAsync(bf->pBufBack, diff);
        } else {
            bink->CurBufSize = bink->CurBufUsed;
        }
    }
}

unsigned int BinkFileReadFrame(
    BINKIO *bink, unsigned int, int iOffset, void *pDest, unsigned int iReadSize
) {
    if (bink->ReadError) {
        return 0;
    }
    BINKFILE *bf = (BINKFILE *)bink->iodata;
    int adjustedOffset =
        iOffset + ((bf->mEncryptionHeader.mSignature != 0) ? 0x38 : 0);
    auto _tmp0 = bf->pFile->Size();
    if ((unsigned int)(adjustedOffset + iReadSize) > _tmp0) {
        bink->ReadError = 1;
        return 0;
    }
    unsigned int iTimerReadStart = RADTimerRead();
    unsigned int totalRead = 0;
    if (adjustedOffset != -1
        && (unsigned int)bf->iFileBufPos != (unsigned int)adjustedOffset) {
        unsigned int unaligned = 0;
        if ((unsigned int)adjustedOffset > bf->iFileBufPos
            && adjustedOffset <= bf->pFile->Tell()) {
            int diff = adjustedOffset - bf->iFileBufPos;
            bf->pBufPos += diff;
            if ((unsigned int)bf->pBufPos > (unsigned int)bf->pBufEnd) {
                bf->pBufPos -= bink->BufSize;
            }
            bf->iBufEmpty += diff;
            bink->CurBufUsed -= diff;
            goto after_seek;
        }
        while (bink->DoingARead) {
            ReadFunc(bink, false);
        }
        bf->iBufEmpty = bink->BufSize;
        bink->CurBufUsed = 0;
        bf->pBufPos = bf->pBuffer;
        bf->pBufBack = bf->pBuffer;
        int seekTo = adjustedOffset;
        if (bf->mEncryptionHeader.mVersion == 2) {
            unsigned int delta =
                (adjustedOffset - 0x38) - bf->iHeaderSize;
            unsigned int aligned = delta & ~0xf;
            seekTo = (bf->iHeaderSize + (aligned + 0x38));
            unaligned = delta & 0xf;
            bf->pBufPos = bf->pBuffer + unaligned;
            bf->pXTEADecrypter->SetNonce(bf->mEncryptionHeader.mNonce, delta >> 4);
        }
        bf->pFile->Seek(seekTo, 0);
        bink->DoingARead = 0;
    after_seek:
        bf->iFileBufPos = adjustedOffset + unaligned;
    }
    if (bf->pBuffer == NULL) {
        unsigned int readStart = RADTimerRead();
        unsigned int r =
            bf->pFile->Read(pDest, iReadSize);
        if (r < iReadSize) {
            bink->ReadError = 1;
        }
        totalRead = r;
        bink->BytesRead += r;
        bf->iFileBufPos += r;
        unsigned int now = RADTimerRead();
        bink->TotalTime += (now - readStart);
        bink->TotalTime += (now - iTimerReadStart);
        intelendian(pDest, r);
    } else {
        unsigned int remaining = iReadSize;
        unsigned char *dst = (unsigned char *)pDest;
        while (remaining != 0 && bink->ReadError == 0) {
            ReadFunc(bink, true);
            unsigned int chunk = bink->CurBufUsed;
            if (chunk > remaining) {
                chunk = remaining;
            }
            if (chunk == 0) continue;
            remaining -= chunk;
            totalRead += chunk;
            bf->iFileBufPos += chunk;
            unsigned int wrap = bf->pBufEnd - bf->pBufPos;
            if (wrap <= chunk) {
                memcpy(dst, bf->pBufPos, wrap);
                dst += wrap;
                chunk -= wrap;
                bf->pBufPos = bf->pBuffer;
                bink->CurBufUsed -= wrap;
                bf->iBufEmpty += wrap;
            }
            if (chunk != 0) {
                memcpy(dst, bf->pBufPos, chunk);
                dst += chunk;
                bf->pBufPos += chunk;
                bink->CurBufUsed -= chunk;
                bf->iBufEmpty += chunk;
            }
        }
        bink->ForegroundTime += (RADTimerRead() - iTimerReadStart);
    }
    unsigned int fileSize = bf->pFile->Size();
    unsigned int avail = fileSize - bf->iFileBufPos;
        avail = Min(avail, bink->BufSize);
    bink->CurBufSize = avail;
    if (bink->CurBufSize < bink->CurBufUsed + 0x8000) {
        bink->CurBufSize = bink->CurBufUsed;
    }
    return totalRead;
}

unsigned int BinkFileGetBufferSize(BINKIO *bink, unsigned int size) {
    unsigned int result = (size + 0x7fff) & 0xffff8000;
    if (result < 0x10000) {
        result = 0x10000;
    }
    return result;
}

void BinkFileClose(BINKIO *bink) {
    BINKFILE *bf = (BINKFILE *)bink->iodata;
    if (bf->iCloseFile != 0) {
        delete bf->pFile;
        bf->pFile = NULL;
    }
    if (bf->mEncryptionHeader.mVersion == 2) {
        delete bf->pXTEADecrypter;
    }
}

unsigned int BinkFileIdle(BINKIO *bink) {
    if (bink->ReadError) {
        return 0;
    }
    if (bink->Suspended) {
        return 0;
    }
    if (bink->DoingARead) {
        ReadFunc(bink, false);
    }
    return bink->DoingARead;
}

void BinkFileSetInfo(
    BINKIO *bink,
    void *pBuffer,
    unsigned int iBufferSize,
    unsigned int unk3,
    unsigned int unk4
) {
    int bufSize = iBufferSize & 0xFFFF8000;
    BINKFILE *bf = (BINKFILE *)bink->iodata;
    bf->pBuffer = (unsigned char *)pBuffer;
    bink->BufSize = bufSize;
    bf->pBufPos = (unsigned char *)pBuffer;
    bf->pBufBack = (unsigned char *)pBuffer;
    bf->pBufEnd = (unsigned char *)pBuffer + bufSize;
    bf->iBufEmpty = bufSize;
    bink->CurBufUsed = 0;
    bf->iSimulateBPS = unk4;
}

int BinkFileBGControl(BINKIO *bink, unsigned int flags) {
    if ((flags & 1) != 0) {
        if (bink->Suspended == 0) {
            bink->Suspended = 1;
        }
        if (flags & 0x80000000)
            while (((volatile BINKIO *)bink)->DoingARead)
                ;
    } else {
        if (flags & 2) {
            if (bink->Suspended == 1) {
                bink->Suspended = 0;
            }
            if (flags & 0x80000000) {
                BinkFileIdle(bink);
            }
        }
    }
    return bink->Suspended;
}
