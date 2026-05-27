// rb3_stream_receiver_native.cpp — RB3-shaped native StreamReceiver.
//
// X6: V1 audio backend bring-up. Bridges RB3's matched-fork StreamReceiver
// (rb3/src/system/synth/StreamReceiver.{h,cpp}) onto the engine's miniaudio
// AudioDevice mixer (milo-native-engine/src/audio/AudioDevice.h).
//
// Why we don't reuse the engine's StreamReceiverNative (platform/StreamReceiver_Native.cpp):
//   The engine class declares `IsOutputDrained()` and `SetSlipOffset(float)`
//   overrides whose base shapes only exist in DC3's StreamReceiver. RB3's 2010
//   header (StreamReceiver.h above) has neither, so the engine TU fails the
//   `override` check and is excluded from the rb3-native build via
//   MILO_ENGINE_DECOMP_PLATFORM_EXCLUDE. This file is the RB3-shaped equivalent.
//
// Threading model
//   - Producer side (main / synth thread): StandardStream::ConsumeData writes
//     decoded PCM into base mBuffer via WriteData(); StreamReceiver::Poll then
//     calls our StartSendImpl(data = mBuffer + mRingReadPos, size = 0xC000, …)
//     to "submit" the chunk and loops on SendDoneImpl() until true. On true the
//     base advances mRingReadPos += 0xC000 and refunds mRingFreeSpace.
//   - Consumer side (miniaudio audio thread): AudioDevice's data callback calls
//     RenderAudio(); we read 16-bit mono samples directly from base mBuffer at
//     our own audio-thread cursor mAudioReadPos and convert to stereo float.
//   - Back-pressure: SendDoneImpl() returns true only once the audio thread has
//     consumed >= mSendSize bytes since the last StartSendImpl. That gates the
//     base from advancing mRingReadPos faster than the audio thread plays, so
//     the next WriteData() can't overwrite a slot the audio thread still needs.
//
// One StreamReceiver per channel (drums L/R, bass, guitar, vox, backing — ~6
// channels for a song). Each channel runs its own ring; AudioDevice mixes them.
#ifdef HX_NATIVE

#include "synth/StreamReceiver.h"
#include "audio/AudioDevice.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>

namespace {

class RB3StreamReceiverNative : public StreamReceiver, public AudioSource {
public:
    RB3StreamReceiverNative(int numBuffers, int sampleRate, bool slip, int channel)
        : StreamReceiver(numBuffers, slip),
          mSampleRate(sampleRate),
          mChannel(channel),
          mVolume(1.0f),
          mPan(0.0f),
          mSpeed(1.0f),
          mPaused(false),
          mRegistered(false),
          mAudioReadPos(0),
          mBytesConsumedSinceSend(0),
          mSendSize(0),
          mSendActive(false) {}

    ~RB3StreamReceiverNative() override {
        if (mRegistered) {
            AudioDevice::GetInstance().RemoveSource(this);
            mRegistered = false;
        }
    }

    // ----------------------- StreamReceiver vtable -----------------------

    void SetVolume(float v) override { mVolume = v; }
    void SetPan(float p) override { mPan = p; }
    void SetSpeed(float s) override { mSpeed = s; }

    // No slip support in V1 (slipstream is an advanced beat-matching feature for
    // the song-shift-while-playing path; offline playback doesn't need it).
    void SetSlipOffset(float) override {}
    void SlipStop() override {}
    void SetSlipSpeed(float) override {}
    float GetSlipOffset() override { return 0.0f; }

    void PlayImpl() override {
        mPaused = false;
        if (!mRegistered) {
            AudioDevice::GetInstance().AddSource(this);
            mRegistered = true;
        }
    }

    void PauseImpl(bool pause) override {
        // Suspend rendering but stay in the mixer — Resume needs the same instance.
        mPaused = pause;
    }

    int GetPlayCursor() override {
        // Base mRingReadPos is the producer-side cursor (chunks fully consumed
        // by the audio thread + acknowledged via SendDoneImpl). That's what the
        // base Poll() uses for active-buffer math, and matches the engine's
        // GetBytesPlayed() bookkeeping.
        int pos = mAudioReadPos.load(std::memory_order_acquire);
        mLastPlayCursor = pos;
        return pos;
    }

    // 3-arg StartSendImpl — the no-wrap case. `data` is mBuffer + mRingReadPos
    // (already in the base ring). We just record the chunk size + reset the
    // audio-thread byte counter so SendDoneImpl can gate on it.
    void StartSendImpl(unsigned char * /*data*/, int size, int /*targetIdx*/) override {
        mSendSize = size;
        mBytesConsumedSinceSend.store(0, std::memory_order_release);
        mSendActive.store(true, std::memory_order_release);
    }

    // 5-arg StartSendImpl — the wrap-around case. Same logic; we don't care
    // about the split because the audio thread reads with its own wrapping
    // cursor (mAudioReadPos). Total chunk size = size1 + size2.
    void StartSendImpl(unsigned char * /*data1*/, unsigned char * /*data2*/,
                       int size1, int size2, int /*targetIdx*/) override {
        mSendSize = size1 + size2;
        mBytesConsumedSinceSend.store(0, std::memory_order_release);
        mSendActive.store(true, std::memory_order_release);
    }

    bool SendDoneImpl() override {
        if (!mSendActive.load(std::memory_order_acquire)) {
            return true;
        }
        // Pre-Play (kInit) phase: the receiver is not yet registered with the
        // AudioDevice (PlayImpl() drives AddSource), so no audio thread is
        // consuming chunks. Accept submissions immediately so the
        // StandardStream / StreamReceiver state machine can advance:
        //   kInit -> kReady -> (Play() called) -> kPlaying.
        // Without this, mSongStream->IsReady() never flips, the
        // BeatMasterLoader spins forever in LoadMgr::Poll(), and Game::mLoadState
        // never advances past kLoadingSong (V1 audio-loading hang).
        //
        // During kPlaying, gate on the audio thread actually draining the chunk
        // to prevent ring overruns (real back-pressure).
        if (mState != kPlaying || !mRegistered) {
            mSendActive.store(false, std::memory_order_release);
            return true;
        }
        int consumed = mBytesConsumedSinceSend.load(std::memory_order_acquire);
        if (consumed >= mSendSize) {
            mSendActive.store(false, std::memory_order_release);
            return true;
        }
        return false;
    }

    // ----------------------- AudioSource vtable -----------------------

    int RenderAudio(float *output, int frameCount) override {
        const int totalOutSamples = frameCount * 2; // interleaved stereo
        if (mPaused) {
            std::memset(output, 0, totalOutSamples * sizeof(float));
            return frameCount;
        }

        // Per-frame source samples: 1 (mono int16). Bytes per frame = 2.
        const int bytesPerFrame = 2;
        const int bytesNeeded = frameCount * bytesPerFrame;
        const int ringSize = mRingSize; // base, fixed at 0x18000

        // We only render up to mSendSize - mBytesConsumedSinceSend so we never
        // overrun into an as-yet-unsubmitted chunk. If nothing is currently
        // submitted, render silence (keeps the source alive for IsFinished()).
        int allowedBytes = 0;
        if (mSendActive.load(std::memory_order_acquire)) {
            int consumed = mBytesConsumedSinceSend.load(std::memory_order_acquire);
            allowedBytes = mSendSize - consumed;
            if (allowedBytes < 0) allowedBytes = 0;
        }
        int bytesToRead = std::min(bytesNeeded, allowedBytes);
        int framesToRender = bytesToRead / bytesPerFrame;
        if (framesToRender < 0) framesToRender = 0;

        // Volume + pan (constant-amplitude-ish: just scale L/R independently).
        const float volL = mVolume * std::max(0.0f, 1.0f - mPan);
        const float volR = mVolume * std::max(0.0f, 1.0f + mPan);

        int readPos = mAudioReadPos.load(std::memory_order_acquire);
        const int16_t *ringS16 = reinterpret_cast<const int16_t *>(mBuffer);

        for (int i = 0; i < framesToRender; i++) {
            // readPos is in BYTES into mBuffer; convert to sample index.
            int sampleIdx = readPos >> 1; // /2
            int16_t s = ringS16[sampleIdx];
            float fs = static_cast<float>(s) * (1.0f / 32768.0f);
            output[i * 2 + 0] = fs * volL;
            output[i * 2 + 1] = fs * volR;
            readPos += bytesPerFrame;
            if (readPos >= ringSize) readPos -= ringSize;
        }

        // Zero-fill any remainder (ring starved or no chunk submitted).
        for (int i = framesToRender; i < frameCount; i++) {
            output[i * 2 + 0] = 0.0f;
            output[i * 2 + 1] = 0.0f;
        }

        mAudioReadPos.store(readPos, std::memory_order_release);
        if (framesToRender > 0) {
            mBytesConsumedSinceSend.fetch_add(framesToRender * bytesPerFrame,
                                              std::memory_order_acq_rel);
        }
        return frameCount;
    }

    bool IsFinished() const override {
        // Drained == state stopped + nothing pending to play. Base sets mState
        // to kStopped via Stop(); after EndData the producer stops writing and
        // mDoneBufferCounter ticks up in StreamReceiver::Poll's SendDone branch
        // when mEndData is set. We mirror StandardStream::PollStream's gate
        // (mDoneBufferCounter > mNumBuffers + 2) so we don't drop early.
        if (mState != kStopped) return false;
        if (mSendActive.load(std::memory_order_acquire)) return false;
        return mDoneBufferCounter > mNumBuffers + 2;
    }

private:
    int mSampleRate;
    int mChannel;
    float mVolume;
    float mPan;
    float mSpeed;
    bool mPaused;
    bool mRegistered;

    // Audio-thread read position into base mBuffer (bytes, mod mRingSize).
    std::atomic<int> mAudioReadPos;

    // Bytes consumed by RenderAudio since the most recent StartSendImpl.
    // SendDoneImpl gates on this >= mSendSize.
    std::atomic<int> mBytesConsumedSinceSend;
    int mSendSize;                     // size of current pending chunk (bytes)
    std::atomic<bool> mSendActive;     // is there a pending chunk?
};

} // anonymous namespace

// C++ linkage (matched by extern decl in rb3_synth_native.cpp). The signature
// must match StreamReceiverFactoryFunc (typedef in StreamReceiver.h).
StreamReceiver *RB3CreateNativeStreamReceiver(int numBuffers, int sampleRate,
                                              bool slip, int channel) {
    return new RB3StreamReceiverNative(numBuffers, sampleRate, slip, channel);
}

#endif // HX_NATIVE
