#include "VoiceWii.h"
#include "math/Decibels.h"
#include "os/Debug.h"
#include "utl/MakeString.h"
#include "utl/MemMgr.h"

static int active_voice_count;
static Voice *gVoices[96];

extern "C" void *AXAcquireVoice(unsigned long, void (*)(void *), unsigned long);
static void VoiceTakeoverCallback(void *);

Voice::Voice(const void *buffer, int bufBytes, bool bUseInPlace, bool bUseMEM2)
    : mEnvelope() {
    MILO_ASSERT(bufBytes > 0, 0x43);
    MILO_ASSERT(buffer, 0x44);

    mFormat = SampleData::kBigEndPCM;
    mState = 1;
    mBufferStart = (char *)buffer;
    mBufferBytes = bufBytes;
    mStartByte = -1;
    mLoopByte = -1;
    mSampleRate = 48000;
    mSpeed = 1.0f;

    mVolume = 10.0f * RatioToDb(1.0f);
    mPan = 0.0f;
    mNextBufferSyncPtr = 0;
    mNextBufferSyncSize = 0;
    mFXCore = kFXCoreNone;
    mFXActive = false;
    mPitchEffect = 0;
    mMixDirty = true;
    mLastPitchEffectWritePtr = 0;
    mFirstPoll = true;

    mVoice = (_AXVPB *)AXAcquireVoice(0xf, VoiceTakeoverCallback, 0);
    if (mVoice != 0) {
        active_voice_count++;
        gVoices[mVoice->index] = this;
        mUseInPlace = bUseInPlace;
        if (bUseInPlace) {
            if (((unsigned int)buffer & 31) != 0) {
                TheDebug << MakeString("buffer %x is not 32-byte aligned\n", (unsigned long)buffer);
                TheDebug.Fail(MakeString(kAssertStr, __FILE__, 0x67, "((u32)buffer & 31) == 0"));
            }
            MILO_ASSERT(((unsigned int)buffer & 31) == 0, 0x69);
            mVoiceBuffer = (unsigned char *)buffer;
            mVoiceBufferSize = mBufferBytes;
        } else if (bUseMEM2) {
            int size = 0x4000;
            if (mBufferBytes >= 0x4000) {
                size = mBufferBytes;
            }
            mVoiceBufferSize = size;
            mVoiceBuffer = (unsigned char *)_MemAlloc(size, 0x20);
            MILO_ASSERT(mVoiceBuffer, 0x72);
        } else {
            int size = 0x4000;
            if (mBufferBytes >= 0x4000) {
                size = mBufferBytes;
            }
            mVoiceBufferSize = size;
            mVoiceBuffer = (unsigned char *)_MemAlloc(size, 0x20);
            MILO_ASSERT(mVoiceBuffer, 0x7a);
        }
        SyncBuffer(0, mBufferBytes);
    } else {
        FormatString fs("VoiceWii failed to acquire a voice.\n");
        TheDebug << fs.Str();
        mVoiceBuffer = 0;
        mVoiceBufferSize = 0;
    }
}

bool Voice::IsPaused() { return mState == 3; }

bool Voice::IsPlaying() {
    if (mVoice == 0) {
        return false;
    }
    if (mState == 3) {
        return true;
    }

    return mVoice->pb.state != 0;
}

int Voice::SampToByte(int samp, bool b) {
    if (b != 0) {
        samp++;
    }
    if ((mFormat) == 1) {
        return samp << 1;
    }
    if ((mFormat) == 6) {
        return samp / 2;
    }

    return 0;
}

void Voice::SetFX(bool enabled) {
    mFXActive = enabled;
    mMixDirty = true;
}

void Voice::SetFormat(SampleData::Format format) { mFormat = format; }

void Voice::SetLoopSamp(int samp) {
    int byte = SampToByte(samp, false);
    mLoopByte = byte;
}

void Voice::SetStartSamp(int samp) {
    int byte = SampToByte(samp, false);
    mStartByte = byte;
}

void Voice::SetVolume(float volume) { SetVolume(volume, true); }
