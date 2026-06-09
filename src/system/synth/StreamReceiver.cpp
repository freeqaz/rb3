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
        if (chunks > kMaxChunks) chunks = kMaxChunks;
        mNumBuffers = chunks;
        mRingSize = chunks * 0xC000;
        mRingFreeSpace = mRingSize;
    }
#endif
}

StreamReceiver::~StreamReceiver() {}

DECOMP_FORCEFUNC(StreamReceiver, StreamReceiver, BytesWriteable())

#pragma push
#pragma force_active on
inline int StreamReceiver::BytesWriteable() { return mRingFreeSpace; }
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