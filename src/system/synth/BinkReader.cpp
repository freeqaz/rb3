#include "synth/BinkReader.h"
#include "lib/binkwii/binkread.h"
#include "utl/BinkIntegration.h"
#include "utl/MemMgr.h"

extern "C" {
void BinkNextFrame(BINK *);
unsigned int BinkGetTrackData(BINKTRACK *, void *);
void BinkGoto(BINK *, unsigned int, int);
}

int BinkReader::mPlaying;
int BinkReader::sHeap = 1;
int gTempLastDecodeSize = -1;
int gTempPrevFrameSize = -1;

struct BinkHeapEntry {
    void *ptr;
    bool inUse;
};

static const int kBinkHeapSize = 0x10E00;
static const int kBinkHeapCount = 10;
static const int kBinkHeapAlign = 0x80;
static BinkHeapEntry gBinkReaderHeap[kBinkHeapCount];

void BinkReaderHeapInit() {
    for (int i = 0; i < kBinkHeapCount; i++) {
        gBinkReaderHeap[i].ptr = _MemAlloc(kBinkHeapSize, kBinkHeapAlign);
        gBinkReaderHeap[i].inUse = false;
    }
}

void *BinkReaderHeapAlloc(int size) {
    if (size <= kBinkHeapSize) {
        for (int i = 0; i < kBinkHeapCount; i++) {
            if (!gBinkReaderHeap[i].inUse) {
                gBinkReaderHeap[i].inUse = true;
                return gBinkReaderHeap[i].ptr;
            }
        }
    }
    MILO_LOG("BinkReaderHeapAlloc: warning: fallback to normal malloc\n");
    return _MemAlloc(size, kBinkHeapAlign);
}

void BinkReaderHeapFree(void *ptr) {
    for (int i = 0; i < kBinkHeapCount; i++) {
        if (gBinkReaderHeap[i].ptr == ptr) {
            MILO_ASSERT(gBinkReaderHeap[i].inUse, 0x4F);
            gBinkReaderHeap[i].inUse = false;
            return;
        }
    }
    MILO_LOG("BinkReaderHeapAlloc: warning: fallback to normal free\n");
    _MemFree(ptr);
}

BinkReader::BinkReader(File *f, StandardStream *s)
    : mFile(f), mStream(s), mDecodeTrack(0), mSamplesReady(0), mSampleCurrent(0),
      mSamplesJump(0), mState(kOpenBink), mHeap(sHeap) {
    mPlaying++;
    BinkInit();
    BinkSetSoundTrack(0, 0);
    mBink = BinkOpen(f, 0x2804400);
    if (mBink) {
        mState = kOpenTracks;
        BinkSetVideoOnOff(mBink, 0);
    } else {
        MILO_WARN("Error opening Bink audio file: %s\n", BinkGetError());
        mState = kFailure;
    }
}

BinkReader::~BinkReader() {
    if (mState > kOpenTracks && mBink) {
        for (unsigned char i = 0; i < mBink->NumTracks; i++) {
            if (mBinkTracks[i]) {
                BinkCloseTrack(mBinkTracks[i]);
            }
            if (mPCMBuffers[i]) {
                MILO_LOG("BinkReader: 0x%08x free %d\n", this, i);
                BinkReaderHeapFree(mPCMBuffers[i]);
            }
        }
        BinkClose(mBink);
    }
    mPlaying--;
}

void BinkReader::PollOpenTracks() {
    MILO_ASSERT(mBink->NumTracks < BINK_AUDIO_CHANNEL_MAX, 0xA4);
    if (mBink->NumTracks == 0) {
        mState = kDone;
    }
    MemPushHeap(mHeap);
    for (unsigned char i = 0; i < mBink->NumTracks; i++) {
        BINKTRACK *hBinkTrack = BinkOpenTrack(mBink, i);
        mBinkTracks[i] = hBinkTrack;
        MILO_ASSERT(hBinkTrack->Bits == 16, 0xB9);
        MILO_ASSERT(hBinkTrack->Channels == 1, 0xBD);
        MILO_LOG("BinkReader: 0x%08x alloc %d %d\n", this, i, hBinkTrack->MaxSize);
        unsigned char *data = (unsigned char *)BinkReaderHeapAlloc(hBinkTrack->MaxSize);
        mPCMBuffers[i] = data;
        mPCMOffsets[i] = data;
    }
    MemPopHeap();
    mState = kInitStream;
}

void BinkReader::PollInitStream() {
    mState = kPlay;
    Init();
    gTempLastDecodeSize = -1;
}

void BinkReader::PollPlay() {}

void BinkReader::Poll(float) {}

void BinkReader::Init() {
    MILO_ASSERT(mStream, 0x1F9);
    mStream->InitInfo(mBink->NumTracks, mBinkTracks[0]->Frequency, false, -1);
}

void BinkReader::Seek(int targetSample) {
    MILO_ASSERT(targetSample >= 0, 0x1B1);
    if (mBink != nullptr) {
        if (mState == kFailure)
            return;
        float kfBinkFreq = (float)mBinkTracks[0]->Frequency;
        float kfBinkRate = (float)mBink->FrameRate / (float)mBink->FrameRateDiv;
        int kiSampleFrame = (int)(kfBinkRate * ((float)targetSample / kfBinkFreq - 0.75f)) + 1;
        if (kiSampleFrame < 1) {
            kiSampleFrame = 1;
        } else if ((unsigned int)kiSampleFrame >= mBink->Frames) {
            MILO_WARN(
                "BinkReader: Seek past last frame (seek to %d, there are %d)",
                kiSampleFrame,
                mBink->Frames
            );
            targetSample = 0;
            kiSampleFrame = 1;
        }
        BinkGoto(mBink, kiSampleFrame, 1);

        float fSamplesAfterSeek;
        if (mBink->FrameNum == 1) {
            fSamplesAfterSeek = 0.0f;
        } else {
            fSamplesAfterSeek = 0.75f + ((float)mBink->FrameNum - 1.0f) / kfBinkRate;
        }
        int samplesAfterSeek = (int)(fSamplesAfterSeek * kfBinkFreq);
        mSampleCurrent = samplesAfterSeek;
        if (targetSample == 0) {
            mSamplesJump = 0;
            mSampleCurrent = 0;
        } else {
            mSamplesJump = targetSample - samplesAfterSeek;
        }
        if (mBink->FrameNum == 1) {
            int maxJump = (int)(0.75f * kfBinkFreq) - 1;
            if (mSamplesJump > maxJump) {
                MILO_WARN(
                    "BinkReader::Seek mSamplesJump %d > %d", mSamplesJump, maxJump
                );
                mSamplesJump = maxJump;
            }
        } else {
            MILO_ASSERT((float)mSamplesJump < kfBinkFreq / kfBinkRate, 0x1EE);
        }
        MILO_ASSERT(mSamplesJump >= 0, 0x1F0);
        mState = kPlay;
    }
}