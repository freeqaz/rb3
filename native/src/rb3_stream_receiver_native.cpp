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
//
// CONTINUOUS-PLAY model (V1.1 — fixes the startup-deadlock silence)
//   The Wii DSP plays the pre-filled ring *continuously*; its hardware play
//   cursor advances on its own, and that advance is what triggers the base
//   Poll()'s cursor-driven refill-sends (activeBuf != mSendTarget). The earlier
//   native bridge instead gated RenderAudio on an explicit "send" (mSendActive),
//   which is only ever raised by a send, which only happens once the play cursor
//   advances — a chicken-and-egg deadlock that produced exactly 0.0 RMS for the
//   whole song (mAudioReadPos starts at 0 -> activeBuf 0 == mSendTarget 0 -> no
//   send -> nothing renders -> cursor never moves).
//
//   The consumer now behaves like the DSP: it plays whatever the producer has
//   written ahead of the play cursor, every callback, ungated by mSendActive.
//     - "Available to play" = the ring-forward byte distance from the play
//       cursor mAudioReadPos to the producer write frontier mRingWritePos. We
//       never read past mRingWritePos (that memory is unwritten / stale).
//     - Each callback advances mAudioReadPos by the bytes played. That advance
//       is what GetPlayCursor() exposes to base Poll(), so activeBuf advances,
//       Poll() sets mWantToSend, and the producer's refill pipeline runs — the
//       exact same cursor-driven loop the Wii relied on.
//     - If the play cursor catches up to the write frontier (starvation), the
//       remainder of the callback is zero-filled (a brief glitch, never the old
//       permanent silence).
//
//   Back-pressure (overrun prevention): SendDoneImpl() acknowledges a chunk as
//   freeable (letting base advance mRingReadPos and letting WriteData refill the
//   slot) ONLY once the play cursor has advanced past the END of that chunk.
//   mRingReadPos is the producer's free frontier; gating it behind mAudioReadPos
//   guarantees WriteData refills *behind* the play cursor, never over unplayed
//   data. This both prevents overrun and is what drives the send loop forward.
//
//   kInit handoff: during kInit the base Poll() fast-accepts sends to prime the
//   ring (StuffChannels()/IsReady()); SendDoneImpl returns true immediately so
//   mRingReadPos races through the primed data with no consumer. At the
//   kInit->kPlaying transition (PlayImpl, AddSource) we snapshot the *current*
//   mRingReadPos as the play start — that is exactly the first byte the base has
//   not yet handed off, i.e. the song start still resident in mBuffer ahead of
//   mRingWritePos. mAudioReadPos plays forward from there.
//
//   Thread-safety: mRingWritePos is written by the producer (synth/main thread)
//   and read by the consumer (audio thread); the consumer's read is racy-benign
//   (a stale-low value just under-reports availability for one callback, never
//   reads unwritten memory). mAudioReadPos is owned by the consumer and read
//   back by the producer via GetPlayCursor()/SendDoneImpl() with acquire loads.
//   On web both run on the SAME thread (PumpAudio): the producer Poll() runs,
//   then RenderAudio() runs, with no overlap — and nothing here ever blocks or
//   spin-waits, so the single-thread case can't deadlock.
//
// One StreamReceiver per channel (drums L/R, bass, guitar, vox, backing — ~6
// channels for a song). Each channel runs its own ring; AudioDevice mixes them.
#ifdef HX_NATIVE

#include "synth/StreamReceiver.h"
#include "audio/AudioDevice.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

static inline void ComputePanGains(float volume, float pan, float &left, float &right) {
    pan = std::max(-1.0f, std::min(1.0f, pan));
    left = volume * (pan <= 0.0f ? 1.0f : 1.0f - pan);
    right = volume * (pan >= 0.0f ? 1.0f : 1.0f + pan);
}

class RB3StreamReceiverNative : public StreamReceiver, public AudioSource {
public:
    RB3StreamReceiverNative(int numBuffers, int sampleRate, bool slip, int channel)
        // IMPORTANT: force the base mSlipEnabled to false. V1 does not implement
        // slipstream (SetSlipOffset/SlipStop/SetSlipSpeed are no-ops here), and the
        // base StreamReceiver::Poll GATES its cursor-driven refill-send on
        // !mSlipEnabled (Poll: "if (!mSlipEnabled && activeBuf != mSendTarget)
        // mWantToSend = true;"). With mSlipEnabled=true the slip channels would
        // rely on a slip cursor we never advance, so they'd never request a send,
        // never free a ring chunk, and the decoder would stall after the kInit
        // prime (the ring just loops its first ~1.1s forever). Treating every
        // channel as non-slip routes them all through the standard play-cursor
        // send trigger that this bridge drives. The original `slip` arg is kept
        // for the (no-op) slip vtable methods and debug only.
        : StreamReceiver(numBuffers, false),
          mSampleRate(sampleRate),
          mChannel(channel),
          mVolume(1.0f),
          mPan(0.0f),
          mSpeed(1.0f),
          mPaused(false),
          mRegistered(false),
          mPlayStarted(false),
          mAudioReadPos(0),
          mPlayedTotal(0),
          mSendStartPlayed(0),
          mSendSize(0),
          mSendActive(false) {
        const char *dbg = getenv("RB3_STREAM_AUDIO_DBG");
        mDebug = (dbg && dbg[0] == '1');
        if (mDebug) {
            std::fprintf(stderr, "RB3STREAM ch=%d ctor numBuffers=%d sr=%d slip=%d\n",
                         mChannel, numBuffers, sampleRate, (int)slip);
            std::fflush(stderr);
        }
    }

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
        if (!mPlayStarted) {
            // kInit->kPlaying handoff. During kInit the base fast-accepted sends,
            // so mRingReadPos points at the first byte it has not yet handed off
            // — i.e. the song start still resident in mBuffer, just behind
            // mRingWritePos. Start the play cursor there so we play the buffered
            // song from its beginning. (Subsequent Play() after a pause keeps the
            // existing cursor — only the very first Play arms it.)
            mAudioReadPos.store(mRingReadPos, std::memory_order_release);
            mPlayedTotal.store(0, std::memory_order_release);
            mSendStartPlayed = 0;
            mPlayStarted = true;
            if (mDebug) {
                std::fprintf(stderr, "RB3STREAM ch=%d PlayImpl start: readPos=%d writePos=%d "
                            "written=%d free=%d state=%d\n",
                            mChannel, mRingReadPos, mRingWritePos,
                            mRingWrittenSpace, mRingFreeSpace, (int)mState);
                std::fflush(stderr);
            }
        }
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
    // (already in the base ring). Record the chunk's ring position + size so
    // SendDoneImpl can release it once the PLAY cursor has advanced past its end.
    void StartSendImpl(unsigned char * /*data*/, int size, int /*targetIdx*/) override {
        mSendStartPlayed = mPlayedTotal.load(std::memory_order_acquire);
        mSendSize = size;
        mSendActive.store(true, std::memory_order_release);
    }

    // 5-arg StartSendImpl — the wrap-around case. Same logic; the chunk begins at
    // mRingReadPos and wraps. Total chunk size = size1 + size2.
    void StartSendImpl(unsigned char * /*data1*/, unsigned char * /*data2*/,
                       int size1, int size2, int /*targetIdx*/) override {
        mSendStartPlayed = mPlayedTotal.load(std::memory_order_acquire);
        mSendSize = size1 + size2;
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
        if (mState != kPlaying || !mRegistered) {
            mSendActive.store(false, std::memory_order_release);
            return true;
        }
        // During kPlaying: release the chunk (let base advance mRingReadPos and
        // refund the slot for WriteData) ONLY once the consumer has actually
        // played at least mSendSize bytes since this chunk was submitted. We gate
        // on the MONOTONIC byte counter mPlayedTotal (not the wrapped ring cursor)
        // so the comparison is unambiguous regardless of how many times the play
        // cursor has wrapped the small 2-chunk ring. Never block — report "not
        // yet" and let the base Poll() loop revisit next tick (no spin-wait).
        long long playedNow = mPlayedTotal.load(std::memory_order_acquire);
        if (playedNow - mSendStartPlayed >= (long long)mSendSize) {
            mSendActive.store(false, std::memory_order_release);
            return true;
        }
        return false;
    }

    // ----------------------- AudioSource vtable -----------------------

    int RenderAudio(float *output, int frameCount) override {
        const int totalOutSamples = frameCount * 2; // interleaved stereo
        // Inert until the first Play() arms the cursor, or while paused: emit
        // silence but keep the source alive for IsFinished().
        if (mPaused || !mPlayStarted) {
            std::memset(output, 0, totalOutSamples * sizeof(float));
            return frameCount;
        }

        // Per-frame source samples: 1 (mono int16). Bytes per frame = 2.
        const int bytesPerFrame = 2;
        const int bytesNeeded = frameCount * bytesPerFrame;
        const int ringSize = mRingSize; // base, fixed at 0x18000

        // CONTINUOUS PLAY: render up to the data the producer has buffered ahead
        // of the play cursor. This is NOT gated on mSendActive — that gate was
        // the deadlock.
        //
        // The valid, not-yet-freed data is the span [mRingReadPos, mRingWritePos)
        // of length mRingWrittenSpace (producer bookkeeping). The play cursor
        // mAudioReadPos lives inside that span (back-pressure keeps mRingReadPos
        // from advancing past it). So:
        //     consumedFromReadPos = (mAudioReadPos - mRingReadPos) mod ringSize
        //     available           = mRingWrittenSpace - consumedFromReadPos
        // Using mRingWrittenSpace (not writePos - readPos) is essential: at the
        // kInit->kPlaying handoff the ring is FULL (mRingWrittenSpace == ringSize,
        // mRingWritePos == mRingReadPos), where a bare writePos-readPos distance
        // would read 0 and re-introduce the silence. mRingWrittenSpace and
        // mRingReadPos are producer-owned ints; reading them here is racy-benign
        // (a stale value only under-reports availability for one callback, never
        // points the play cursor at unwritten/freed memory).
        int readPos = mAudioReadPos.load(std::memory_order_acquire);
        int baseReadPos = mRingReadPos;          // producer free frontier
        int written = mRingWrittenSpace;         // valid buffered bytes
        int consumed = readPos - baseReadPos;    // bytes played past the free frontier
        if (consumed < 0) consumed += ringSize;
        int available = written - consumed;
        if (available < 0) available = 0;        // cursor lapped a just-freed chunk (guard)
        if (available > ringSize) available = ringSize;

        int bytesToRead = std::min(bytesNeeded, available);
        int framesToRender = bytesToRead / bytesPerFrame;
        if (framesToRender < 0) framesToRender = 0;

        // Volume + pan (constant-amplitude-ish: just scale L/R independently).
        float volL = 0.0f;
        float volR = 0.0f;
        ComputePanGains(mVolume, mPan, volL, volR);

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

        // Zero-fill any remainder (ring starved: caught up to write frontier).
        for (int i = framesToRender; i < frameCount; i++) {
            output[i * 2 + 0] = 0.0f;
            output[i * 2 + 1] = 0.0f;
        }

        // Publish the advanced play cursor. The producer reads this via
        // GetPlayCursor() (drives the send loop) and SendDoneImpl() (back-pressure).
        mAudioReadPos.store(readPos, std::memory_order_release);
        if (framesToRender > 0) {
            mPlayedTotal.fetch_add(framesToRender * bytesPerFrame,
                                   std::memory_order_acq_rel);
        }

        if (mDebug) {
            static int sCounter = 0;
            if ((sCounter++ % 200) == 0) {
                std::fprintf(stderr, "RB3STREAM ch=%d render: state=%d read=%d baseRead=%d "
                            "writtenSpace=%d avail=%d frames=%d/%d played=%lld vol=%.2f\n",
                            mChannel, (int)mState, readPos, baseReadPos, written, available,
                            framesToRender, frameCount,
                            (long long)mPlayedTotal.load(std::memory_order_acquire),
                            mVolume);
                std::fflush(stderr);
            }
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

#ifdef HX_WEB
    void DebugDescribe(char *buf, size_t bufSize) const override {
        if (bufSize == 0) return;
        std::snprintf(buf, bufSize,
                      "stream ch=%d this=%p state=%d registered=%d paused=%d "
                      "read=%d write=%d played=%lld sendActive=%d done=%d",
                      mChannel, (const void *)this, (int)mState, (int)mRegistered,
                      (int)mPaused, mAudioReadPos.load(std::memory_order_acquire),
                      mRingWritePos,
                      (long long)mPlayedTotal.load(std::memory_order_acquire),
                      (int)mSendActive.load(std::memory_order_acquire),
                      mDoneBufferCounter);
    }
#endif

private:
    int mSampleRate;
    int mChannel;
    float mVolume;
    float mPan;
    float mSpeed;
    bool mPaused;
    bool mRegistered;
    bool mPlayStarted;                 // armed by the first PlayImpl (kInit->kPlaying)
    bool mDebug;                       // RB3_STREAM_AUDIO_DBG=1

    // Audio-thread play cursor into base mBuffer (bytes, mod mRingSize). The
    // consumer owns this; the producer reads it via GetPlayCursor()/SendDoneImpl().
    std::atomic<int> mAudioReadPos;

    // Monotonic total bytes ever played by the consumer since PlayImpl. Written
    // by the audio/render thread, read by the producer (SendDoneImpl) for
    // wrap-free back-pressure, so it must be atomic.
    std::atomic<long long> mPlayedTotal;

    // Snapshot of mPlayedTotal captured when the current chunk was submitted
    // (StartSendImpl). SendDoneImpl releases the chunk once the consumer has
    // played mSendSize more bytes than this. Both StartSendImpl and SendDoneImpl
    // run on the producer (Poll) thread, so a plain member is sufficient.
    long long mSendStartPlayed;
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
