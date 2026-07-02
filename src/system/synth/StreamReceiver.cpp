#include "synth/StreamReceiver.h"
#include "os/Debug.h"

StreamReceiverFactoryFunc *StreamReceiver::sFactory;

StreamReceiver *StreamReceiver::New(int i1, int i2, bool b3, int i4) {
    MILO_ASSERT(sFactory, 0x1C);
    return sFactory(i1, i2, b3, i4);
}

StreamReceiver::StreamReceiver(int numBuffers, bool slip)
    : mSlipEnabled(slip), mBuffer(), mNumBuffers(numBuffers), mState(kInit),
      mSendTarget(0), mWantToSend(false), mSending(false), mBuffersSent(0),
      mStarving(false), mEndData(false), mDoneBufferCounter(0), mLastPlayCursor(0) {
    MILO_ASSERT(numBuffers > 0, 0x33);
    mRingFreeSpace = 0x18000;
    mRingReadPos = 0;
    mRingWritePos = 0;
    mRingSize = 0x18000;
    mRingWrittenSpace = 0;
#ifdef HX_NATIVE
    // The native/web bridge plays mBuffer directly as the ring (see
    // StreamReceiver.h). Size the PHYSICAL ring to mNumBuffers chunks so it
    // matches the base send-target cursor math AND buffers enough of the
    // multitrack decode that 11-15 stems don't underrun -> zero-fill (dropout/
    // "static"). The Wii build keeps the fixed 0x18000 (2-chunk) DSP-staging ring.
    {
        const int kMaxChunks = (int)(sizeof(mBuffer) / 0xC000);
        int chunks = numBuffers;
        if (chunks < 2) chunks = 2;
        // Off-main web audio (RB3_WEB_OFFMAIN_MIX) decouples the stall budget from
        // output latency by leaning on the decode-ahead depth: the deeper the ring,
        // the longer a main-thread freeze the worklet survives without starving the
        // music mix. The mBuffer already reserves the full 16-chunk (~9 s) span, so
        // use it — a deeper ring is free here (no extra allocation) and strictly
        // improves stall resilience.
        //
        // DEFAULT-ON (deepring): use the full 16-chunk (~9 s) ring on every native/
        // web build. The base decodes the whole ring ahead of the play cursor
        // (mRingWrittenSpace == mRingSize in steady play), so a single main-thread
        // freeze rides a ~7-8 s cushion. The extra depth is free (mBuffer is a fixed
        // 0xC0000 array either way), so a deeper decode-ahead ring strictly helps the
        // native path too. Opt OUT with RB3_WEB_OFFMAIN_MIX=0 to keep the prior
        // 8-chunk footprint. Match-neutral: HX_NATIVE-only block (Wii uses 0x18000).
        {
            const char *om = getenv("RB3_WEB_OFFMAIN_MIX");
            if (!(om && om[0] == '0') && chunks < kMaxChunks)
                chunks = kMaxChunks;
        }
        if (chunks > kMaxChunks) chunks = kMaxChunks;
        mNumBuffers = chunks;
        mRingSize = chunks * 0xC000;
        mRingFreeSpace = mRingSize;
        mTotalWrittenEver = 0;
    }
#endif
}

StreamReceiver::~StreamReceiver() {}

DECOMP_FORCEFUNC(StreamReceiver, StreamReceiver, BytesWriteable())

#pragma push
#pragma force_active on
inline int StreamReceiver::BytesWriteable() {
#ifdef HX_NATIVE
    // Pre-Play decode cap (see mTotalWrittenEver in the header). Until the
    // first Play() (mState is only ever kInit/kReady before it, and never
    // returns to kReady after), never accept more than ONE ring lap of decoded
    // data in total. The kInit prime frees exactly mNumBuffers chunks
    // (refunding mRingFreeSpace), so without this cap the decoder races a
    // second lap through the count-in and overwrites the song start; playback
    // then begins ~ringSecs (~9.1 s) into the song while the clock reads 0.
    // The decoder handles 0-writable fine — it's the normal ring-full state of
    // steady gameplay. Opt out: RB3_STREAM_PREPLAY_CAP_OFF=1.
    if (mState <= kReady) {
        static int sCapOff = -1;
        if (sCapOff < 0) {
            const char *e = getenv("RB3_STREAM_PREPLAY_CAP_OFF");
            sCapOff = (e && e[0] && e[0] != '0') ? 1 : 0;
        }
        if (!sCapOff) {
            long long remaining = (long long)mRingSize - mTotalWrittenEver;
            if (remaining < 0) remaining = 0;
            if (remaining < (long long)mRingFreeSpace) return (int)remaining;
        }
    }
#endif
    return mRingFreeSpace;
}
#pragma pop

void StreamReceiver::WriteData(const void *data, int bytes) {
    MILO_ASSERT(bytes > 0 && bytes <= BytesWriteable(), 0x55);
    int num = mRingSize - mRingWritePos;
    if (bytes <= num) {
        memcpy(mBuffer + mRingWritePos, data, bytes);
        mRingWritePos += bytes;
        if (mRingWritePos == mRingSize) {
            mRingWritePos = 0;
        }
    } else {
        memcpy(mBuffer + mRingWritePos, data, num);
        char *cData = (char *)data;
        memcpy(mBuffer, cData + num, bytes - num);
        mRingWritePos = bytes - num;
    }
    mRingFreeSpace -= bytes;
    mRingWrittenSpace += bytes;
#ifdef HX_NATIVE
    mTotalWrittenEver += bytes;
#endif
}

void StreamReceiver::ClearAtEndData() {
    if (mRingFreeSpace != 0) {
        if (mRingWritePos + mRingFreeSpace <= mRingSize) {
            memset(mBuffer + mRingWritePos, 0, mRingFreeSpace);
            mRingWritePos += mRingFreeSpace;
            if (mRingWritePos == mRingSize) {
                mRingWritePos = 0;
            }
        } else {
            int firstWipeSize = mRingSize - mRingWritePos;
            int secondWipeSize = mRingFreeSpace - firstWipeSize;
            MILO_ASSERT(firstWipeSize > 0, 0x7D);
            MILO_ASSERT(secondWipeSize > 0, 0x7E);
            memset(mBuffer + mRingWritePos, 0, firstWipeSize);
            memset(mBuffer, 0, mRingFreeSpace - firstWipeSize);
            mRingWritePos = mRingFreeSpace - firstWipeSize;
        }
    }
}

DECOMP_FORCEFUNC(StreamReceiver, StreamReceiver, Ready())

#pragma push
#pragma force_active on
inline bool StreamReceiver::Ready() { return mState != kInit; }
#pragma pop

void StreamReceiver::Play() {
    MILO_ASSERT(Ready(), 0x91);
    // Cross-case fall-through via goto: kStopped does PauseImpl then joins the default
    // case at the `play:` label. Rewriting as duplicated tail or restructured switch
    // changes the jump-table layout and breaks the match.
    switch (mState) {
    case kPlaying:
        break;
    case kStopped:
        PauseImpl(false);
        goto play;
        break;
    default:
        PlayImpl();
    play:
        mState = kPlaying;
        break;
    }
}

void StreamReceiver::Stop() {
    MILO_ASSERT(mState == kPlaying || mState == kStopped, 0xA6);
    if (mState == kPlaying) {
        PauseImpl(true);
        mState = kStopped;
    }
}

void StreamReceiver::Poll() {
    if ((unsigned int)(mState - kPlaying) > 1U) {
        if (mState != kInit) {
            if (mState == kReady) {
            } else {
                goto state_bad;
            }
        } else {
            mWantToSend = true;
        }
    } else {
        int playCursor = GetPlayCursor();
        int activeBuf = playCursor / 0xC000;
        mLastPlayCursor = playCursor;
        if (activeBuf < 0 || activeBuf > mNumBuffers) {
            playCursor = GetPlayCursor();
            mLastPlayCursor = playCursor;
            activeBuf = playCursor / 0xC000;
            if (activeBuf < 0 || activeBuf > mNumBuffers) {
                Stop();
                Play();
                goto block_17;
            }
        }
        if (!mSlipEnabled && activeBuf != mSendTarget) {
            mWantToSend = true;
        }
        int diff = activeBuf - mSendTarget;
        if (diff == mNumBuffers / 2 || diff == -mNumBuffers / 2) {
            mWantToSend = true;
        }
    }
    goto block_17;
state_bad:
    MILO_FAIL("bad state logic.\n");
block_17:
    if (mWantToSend && mState != kInit && mRingFreeSpace != 0) {
        mStarving = true;
    }
    if (mWantToSend && mRingWrittenSpace >= 0xC000 && !mSending) {
        if (mRingReadPos + 0xC000 <= mRingSize) {
            StartSendImpl(mBuffer + mRingReadPos, 0xC000, mSendTarget);
        } else {
            int firstChunk = mRingSize - mRingReadPos;
            StartSendImpl(mBuffer + mRingReadPos, mBuffer, firstChunk, 0xC000 - firstChunk, mSendTarget);
        }
        mBuffersSent++;
        if (mBuffersSent >= 100000) {
            mBuffersSent -= mNumBuffers;
        }
        int sendTarget = mSendTarget + 1;
        mWantToSend = false;
        mSending = true;
        mSendTarget = sendTarget;
        if (sendTarget == mNumBuffers) {
            mSendTarget = 0;
        }
    } else if (mWantToSend && mRingFreeSpace == 0 && !mSending) {
        if (mRingWrittenSpace > 0) {
            if (mRingReadPos + 0xC000 <= mRingSize) {
                StartSendImpl(mBuffer + mRingReadPos, 0xC000, mSendTarget);
            } else {
                int firstChunk = mRingSize - mRingReadPos;
                StartSendImpl(mBuffer + mRingReadPos, mBuffer, firstChunk, 0xC000 - firstChunk, mSendTarget);
            }
        } else {
            StartSendImpl(mBuffer, 0xC000, mSendTarget);
        }
        mBuffersSent++;
        if (mBuffersSent >= 100000) {
            mBuffersSent -= mNumBuffers;
        }
        int sendTarget2 = mSendTarget + 1;
        mWantToSend = false;
        mSending = true;
        mSendTarget = sendTarget2;
        if (sendTarget2 == mNumBuffers) {
            mSendTarget = 0;
        }
    }
    if (mSending && SendDoneImpl()) {
        mSending = false;
        mStarving = false;
        if (mSendTarget == 0 && mState == kInit) {
            mState = kReady;
            mWantToSend = false;
        }
        mRingWrittenSpace -= 0xC000;
        mRingFreeSpace += 0xC000;
        mRingReadPos += 0xC000;
        if (mRingReadPos >= mRingSize) {
            mRingReadPos -= mRingSize;
        }
        if (mEndData) {
            ClearAtEndData();
            mRingFreeSpace = 0;
            mDoneBufferCounter++;
        }
    }
}

void StreamReceiver::EndData() {
    if (!mEndData) {
        if (mRingFreeSpace != 0) {
            ClearAtEndData();
            mRingFreeSpace = 0;
        }
        mEndData = true;
    }
}

unsigned long long StreamReceiver::GetBytesPlayed() {
    if (mState == kInit)
        return 0;
    unsigned long long buffersSent = (unsigned long long)mBuffersSent;
    unsigned long long numBuffers = (unsigned long long)mNumBuffers;
    unsigned long long bufferOffset = buffersSent * 0xC000;
    unsigned long long totalPlayed =
        (unsigned long long)mLastPlayCursor + (buffersSent / numBuffers) * numBuffers * 0xC000;
    for (; bufferOffset <= totalPlayed; totalPlayed -= numBuffers * 0xC000)
        ;
    return totalPlayed;
}