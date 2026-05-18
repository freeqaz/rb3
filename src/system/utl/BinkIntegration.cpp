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
    BINKENCRYPTIONHEADER *encHeader = &((BINKFILE *)bink->iodata)->mEncryptionHeader;
    File *file = ((BINKFILE *)bink->iodata)->pFile;
    // check if we've read a file header before
    if (encHeader->mSignature == 0) {
        // read the header
        unsigned int encread = file->Read(encHeader, sizeof(BINKENCRYPTIONHEADER));
        // byteswap mSignature through mMagicB from bad endian to big endian
        intelendian(encHeader, 0x14);
        // byteswap mNonce (each 32-bit word independently)
        unsigned int *nonce = (unsigned int *)encHeader->mNonce;
        unsigned int n0 = nonce[0], n1 = nonce[1], n2 = nonce[2], n3 = nonce[3];
        nonce[0] = BSWAP(n0);
        nonce[1] = BSWAP(n1);
        nonce[2] = BSWAP(n2);
        nonce[3] = BSWAP(n3);
        // check if the header is BIKE
        int whatever = encHeader->mSignature - 0x45420000; // 'BI--'
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
    if (bink->BufSize < anothersize) {
        anothersize = bink->BufSize;
    }
    bink->CurBufSize = anothersize;
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

unsigned int BinkFileReadFrame(BINKIO *, unsigned int, int, void *, unsigned int) {}

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
