#include "synth/BinkReader.h"
#include "lib/binkwii/binkread.h"
#include "os/Debug.h"
#include "os/Timer.h"
#include "rndwii/Rnd.h"
#include "sdk/RVL_SDK/revolution/os/OSCache.h"
#include "ui/UI.h"
#include "utl/BinkIntegration.h"
#include "utl/MakeString.h"
#include "utl/MemMgr.h"

extern "C" {
void BinkNextFrame(BINK *);
unsigned int BinkGetTrackData(BINKTRACK *, void *);
void BinkGoto(BINK *, unsigned int, int);
int Ntsc__6WiiRndFv(WiiRnd *);
}

extern bool gDebugFullQuota;

int BinkReader::mPlaying;
int BinkReader::sHeap = 1;
static int gTempLastDecodeSize = -1;
static int gTempPrevFrameSize = -1;
static int gTempCurFrameSize;

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

#pragma pool_data off
void BinkReader::PollPlay() {
    unsigned int lastTrackBytes;
    int budget;
    int outerIter = 0;
    int innerCount = 0;
    do {
        if (mSamplesReady > 0) {
            {
                static Timer *_t = AutoTimer::GetTimer("bink_consume");
                if (_t) _t->Start();
            }
            int iSamplesConsumed = mStream->ConsumeData(
                (void **)mPCMOffsets, mSamplesReady, mSampleCurrent
            );
            MILO_ASSERT(iSamplesConsumed <= mSamplesReady, 0xF7);
            mSampleCurrent += iSamplesConsumed;
            mSamplesReady -= iSamplesConsumed;
            for (unsigned char i = 0; i < mBink->NumTracks; i++) {
                mPCMOffsets[i] += iSamplesConsumed * 2;
            }
            {
                static Timer *_t = AutoTimer::GetTimer("bink_consume");
                if (_t) _t->Stop();
            }
            if (mDecodeTrack == mBink->NumTracks) {
                START_AUTO_TIMER("bink_read");
                mState = (mBink->FrameNum == mBink->Frames) ? kDone : kPlay;
                if (mState == kPlay) {
                    BinkNextFrame(mBink);
                }
                mDecodeTrack = 0;
            }
        }
        if (mSamplesReady <= 0) {
            {
                static Timer *_t = AutoTimer::GetTimer("bink_decode");
                if (_t) _t->Start();
            }
            lastTrackBytes = 0;
            if (gDebugFullQuota) {
                budget = mBink->NumTracks;
            } else {
                budget = (int)mBink->NumTracks / 2;
                if (TheWiiRnd.GetProgressiveScan() || Ntsc__6WiiRndFv(&TheWiiRnd)) {
                    if (!TheUI.unkb4) {
                        static bool skip;
                        budget -= skip;
                        skip = !skip;
                    }
                }
            }
            innerCount = budget < 2 ? 2 : budget;
            while (innerCount-- > 0) {
                if (mDecodeTrack == mBink->NumTracks) {
                    gTempPrevFrameSize = gTempCurFrameSize;
                    gTempCurFrameSize = 0;
                    break;
                }
                DCZeroRange(
                    mPCMBuffers[mDecodeTrack], mBinkTracks[mDecodeTrack]->MaxSize
                );
                lastTrackBytes = BinkGetTrackData(
                    mBinkTracks[mDecodeTrack], mPCMBuffers[mDecodeTrack]
                );
                gTempCurFrameSize += lastTrackBytes;
                if ((unsigned int)gTempLastDecodeSize != lastTrackBytes) {
                    gTempLastDecodeSize = lastTrackBytes;
                }
                mPCMOffsets[mDecodeTrack] =
                    mPCMBuffers[mDecodeTrack] + mSamplesJump * 2;
                mDecodeTrack++;
            }
            {
                static Timer *_t = AutoTimer::GetTimer("bink_decode");
                if (_t) _t->Stop();
            }
            if (mDecodeTrack == mBink->NumTracks) {
                START_AUTO_TIMER("bink_read");
                mSamplesReady = (int)(lastTrackBytes >> 1) - mSamplesJump;
                mSampleCurrent += mSamplesJump;
                mSamplesJump = 0;
                mState = (mBink->FrameNum == mBink->Frames) ? kDone : kPlay;
                if (innerCount > 0 && mState == kPlay) {
                    BinkNextFrame(mBink);
                    mDecodeTrack = 0;
                }
            }
        }
        outerIter++;
    } while (innerCount > 0 && outerIter < 3);
}
#pragma pool_data reset

void BinkReader::Poll(float) {
    START_AUTO_TIMER("bink_audio");
    switch (mState) {
    case kOpenTracks:
        PollOpenTracks();
        break;
    case kInitStream:
        PollInitStream();
        break;
    case kPlay:
        PollPlay();
        break;
    }
    if (mState != kFailure && mBink->ReadError != 0) {
        MILO_WARN("BinkReader::Poll() failed from read error!\n");
        mState = kFailure;
    }
}

void BinkReader::Init() {
    MILO_ASSERT(mStream, 0x1F9);
    mStream->InitInfo(mBink->NumTracks, mBinkTracks[0]->Frequency, false, -1);
}

void BinkReader::Seek(int iSample) {
    MILO_ASSERT(iSample >= 0, 0x1B1);
    if (mBink != nullptr) {
        if (mState == kFailure)
            return;
        float kfBinkFreq = (float)mBinkTracks[0]->Frequency;
        float kfBinkRate = (float)mBink->FrameRate / (float)mBink->FrameRateDiv;
        int kiSampleFrame = (int)(kfBinkRate * ((float)iSample / kfBinkFreq - 0.75f)) + 1;
        if (kiSampleFrame < 1) {
            kiSampleFrame = 1;
        } else if ((unsigned int)kiSampleFrame >= mBink->Frames) {
            MILO_WARN(
                "BinkReader: Seek past last frame (seek to %d, there are %d)",
                kiSampleFrame,
                mBink->Frames
            );
            iSample = 0;
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
        if (iSample == 0) {
            mSamplesJump = 0;
            mSampleCurrent = 0;
        } else {
            mSamplesJump = iSample - samplesAfterSeek;
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
            MILO_ASSERT(mSamplesJump < (kfBinkFreq / kfBinkRate), 0x1EE);
        }
        MILO_ASSERT(mSamplesJump >= 0, 0x1F0);
        mState = kPlay;
    }
}