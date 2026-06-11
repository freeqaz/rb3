// rb3_sampleinst_native.cpp — RB3-shaped native SampleInst for one-shot SFX.
//
// Ports the engine's DC3 SampleInst_Native.cpp (milo-native-engine/src/platform/
// SampleInst_Native.cpp) to RB3's older synth API. The engine TU is platform-
// EXCLUDED for rb3 (native/CMakeLists.txt MILO_ENGINE_DECOMP_PLATFORM_EXCLUDE)
// because DC3's SampleInst/SampleData differ from RB3's 2010 shapes:
//   - RB3 SampleInst::StopImpl() takes NO bool; IsPlaying() is non-const;
//     SetADSR(const ADSR&) (not ADSRImpl); the ctor takes no args.
//   - RB3 SampleData exposes raw public members (mData/mNumSamples/mSampleRate/
//     mFormat) instead of DC3's HasData()/NumChannels()/DataPtr()/GetNumSamples();
//     RB3 samples carry no channel count (SFX banks are mono).
//
// Without a definition, SynthSample::NewInst (declared virtual under HX_NATIVE in
// SynthSample.h) resolves to the weak no-op stub in rndobj_synth_link_stubs.s →
// every Sfx/MidiInstrument one-shot is silent. This strong definition wins over
// that stub (same mechanism as rb3_keychain_native.cpp / rb3_stream_receiver_native.cpp).
//
// Mirrors rb3_stream_receiver_native.cpp: bridges onto the engine miniaudio
// AudioDevice (native) / AudioDevice_Web SAB+AudioWorklet (web) by implementing
// AudioSource::RenderAudio. Shared verbatim native+web (HX_NATIVE), no HX_WEB
// divergence except the optional DebugDescribe diagnostic.
//
// Decodes 16-bit PCM (kPCM, little-endian) and big-endian PCM (kBigEndPCM — the
// Xbox-360 `.milo_xbox` SFX bank format; byteswapped at render time on the LE
// host). Console-proprietary formats (XMA/VAG/ATRAC/MP3/NintendoADPCM) are not
// host-decodable here and are skipped with a one-time warning — an ffmpeg/codec
// path (engine FFmpegAudioReader) is the future route for those.
#ifdef HX_NATIVE

#include "synth/SampleInst.h"
#include "synth/SynthSample.h"
#include "synth/SampleData.h"
#include "synth/ADSR.h"
#include "synth/FxSend.h"
#include "audio/AudioDevice.h"

// W5-T2: this TU owns the single stb_vorbis IMPLEMENTATION for the whole port.
// rb3_xma_sidecar.h (below) includes stb_vorbis.h in header-only mode everywhere
// it is pulled in (App.cpp / main_native.cpp); defining RB3_STB_VORBIS_IMPL here
// makes rb3_xma_sidecar.h emit the implementation in exactly this one TU, so the
// decoder symbols link once. Keep this define BEFORE the sidecar include.
#define RB3_STB_VORBIS_IMPL 1
#include "rb3_xma_sidecar.h"  // offline XMA->{PCM,OGG} sidecar loader (native + web)

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

static const int kOutputSampleRate = 44100;

// Fetch one mono 16-bit sample, byteswapping if the source is big-endian PCM
// (Xbox-360 bank on a little-endian host).
static inline float FetchSample(const int16_t *data, int idx, bool bigEndian) {
    int16_t s = data[idx];
    if (bigEndian) {
        uint16_t u = static_cast<uint16_t>(s);
        u = static_cast<uint16_t>((u >> 8) | (u << 8));
        s = static_cast<int16_t>(u);
    }
    return static_cast<float>(s) * (1.0f / 32768.0f);
}

// Linear-interpolated mono fetch for sample-rate conversion (bank SFX are often
// 22/32 kHz; output mixer is 44.1 kHz).
static inline float LerpMono(const int16_t *data, int total, double pos, bool be) {
    int idx0 = static_cast<int>(pos);
    int idx1 = idx0 + 1;
    float frac = static_cast<float>(pos - idx0);
    float s0 = (idx0 >= 0 && idx0 < total) ? FetchSample(data, idx0, be) : 0.0f;
    float s1 = (idx1 >= 0 && idx1 < total) ? FetchSample(data, idx1, be) : 0.0f;
    return s0 + frac * (s1 - s0);
}

static inline void ComputePanGains(float volume, float pan, float &left, float &right) {
    pan = std::max(-1.0f, std::min(1.0f, pan));
    left = volume * (pan <= 0.0f ? 1.0f : 1.0f - pan);
    right = volume * (pan >= 0.0f ? 1.0f : 1.0f + pan);
}

class RB3SampleInstNative : public SampleInst, public AudioSource {
public:
    RB3SampleInstNative(SynthSample *sample, bool loop, int startSample, int endSample)
        : SampleInst(),
          mSampleObj(sample),
          mPCMData(nullptr),
          mPCMSamples(0),
          mStartSample(startSample > 0 ? startSample : 0),
          mEndSample(endSample),
          mPlayPos(startSample > 0 ? static_cast<double>(startSample) : 0.0),
          mLoop(loop),
          mBigEndian(false),
          mPaused(false),
          mRegistered(false),
          mPlaying(false),
          mInstVolume(1.0f),
          mInstPan(0.0f),
          mInstSpeed(1.0f),
          mSrcSampleRate(kOutputSampleRate),
          mFxSend(nullptr),
          mOwnedPCM(nullptr) {}

    ~RB3SampleInstNative() override {
        // Always detach from the mixer — the audio thread (native) may still hold
        // us in its source list after RenderAudio set mPlaying=false.
        AudioDevice::GetInstance().RemoveSource(this);
        mPlaying.store(false, std::memory_order_release);
        // Free any sidecar PCM we decoded for a kXMA sample (the bank-resident
        // mData is owned by SampleData; mOwnedPCM is ours).
        if (mOwnedPCM) {
            std::free(mOwnedPCM);
            mOwnedPCM = nullptr;
        }
    }

    // ----------------------- SampleInst vtable (RB3 shape) -----------------------
    bool IsPlaying() override { return mPlaying.load(std::memory_order_acquire); }
    void Pause(bool p) override { mPaused = p; }
    void SetADSR(const ADSR &) override {}
    void SetFXCore(FXCore) override {}
    void StartImpl() override;
    void StopImpl() override;
    void SetVolumeImpl(float v) override { mInstVolume = v; }
    void SetPanImpl(float p) override { mInstPan = p; }
    void SetSpeedImpl(float s) override { mInstSpeed = s; }
    void SetSendImpl(FxSend *s) override { mFxSend = s; }

    // ----------------------- AudioSource vtable (engine) -----------------------
    int RenderAudio(float *output, int frameCount) override;
    bool IsFinished() const override { return !mPlaying.load(std::memory_order_acquire); }
#ifdef HX_WEB
    void DebugDescribe(char *buf, size_t bufSize) const override {
        if (bufSize == 0) return;
        const char *name = (mSampleObj && mSampleObj->Name()) ? mSampleObj->Name() : "";
        const char *file = mSampleObj ? mSampleObj->mFile.c_str() : "";
        std::snprintf(buf, bufSize,
                      "sfx this=%p sample=%p name='%s' file='%s' %s registered=%d paused=%d "
                      "pos=%d/%d @%dHz vol=%.2f pan=%.2f speed=%.2f%s",
                      (const void *)this, (const void *)mSampleObj, name, file,
                      mPlaying.load(std::memory_order_acquire) ? "play" : "stop",
                      (int)mRegistered, (int)mPaused,
                      static_cast<int>(mPlayPos), mEndSample, mSrcSampleRate,
                      mInstVolume, mInstPan, mInstSpeed,
                      mLoop ? " loop" : "");
    }
#endif

private:
    SynthSample *mSampleObj;
    const int16_t *mPCMData;   // pointer into SampleData's payload (lives as long as the bank)
    int mPCMSamples;           // total mono samples in the payload
    int mStartSample;          // loop restart point
    int mEndSample;            // stop point (<=0 → end of data)
    double mPlayPos;           // fractional read cursor (for resampling)
    bool mLoop;
    bool mBigEndian;
    bool mPaused;
    bool mRegistered;
    std::atomic<bool> mPlaying;
    float mInstVolume;
    float mInstPan;
    float mInstSpeed;
    int mSrcSampleRate;
    FxSend *mFxSend;
    int16_t *mOwnedPCM;        // sidecar PCM we own (kXMA path); nullptr otherwise
};

void RB3SampleInstNative::StartImpl() {
    if (!mSampleObj) return;
    SampleData &data = mSampleObj->mSampleData;

    // SFX disabled at load (ThePlatformMgr.AreSFXEnabled()==false) or empty bank
    // entry → no payload; create-but-silent (keeps Sfx bookkeeping consistent).
    if (!data.mData || data.mNumSamples <= 0) return;

    SampleData::Format fmt = data.GetFormat();

    // kXMA (Xbox-360 compressed SFX): the bank's mData is raw XMA, not host-
    // decodable at runtime (no ffmpeg on web). Load the offline-converted PCM
    // sidecar (rb3-xma-convert) keyed by a content hash of the raw payload. If a
    // sidecar exists, play it as little-endian mono PCM; otherwise skip (the
    // bank wasn't converted yet — warn once). Native + web (HX_NATIVE).
    if (fmt == SampleData::kXMA) {
        rb3_xma::SidecarPCM side =
            rb3_xma::TryLoad(data.mData, data.mSizeBytes, data.GetSampleRate());
        if (!side.data) {
            static int sLoggedXma = 0;
            if (!sLoggedXma) {
                std::printf("rb3 SFX: kXMA sample has no PCM sidecar key=%016llx "
                            "(run scripts/assets/convert_xma_banks.sh; set "
                            "RB3_SFX_PCM_DIR=%s) — SFX skipped\n",
                            (unsigned long long)rb3_xma::PayloadKey(
                                data.mData, data.mSizeBytes, data.GetSampleRate()),
                            rb3_xma::SidecarDir().c_str());
                sLoggedXma = 1;
            }
            return;
        }
        if (mOwnedPCM) std::free(mOwnedPCM);
        mOwnedPCM = side.data; // freed in dtor
        mBigEndian = false;    // sidecar PCM is little-endian
        mSrcSampleRate = side.sampleRate > 0 ? side.sampleRate : kOutputSampleRate;
        mPCMData = mOwnedPCM;
        mPCMSamples = side.numSamples;  // mono sample count
        if (mEndSample <= 0 || mEndSample > mPCMSamples) mEndSample = mPCMSamples;
        if (mPlayPos < 0.0 || mPlayPos >= static_cast<double>(mPCMSamples)) mPlayPos = 0.0;

        static bool sLoggedXmaOk = false;
        if (!sLoggedXmaOk) {
            std::printf("rb3 SFX: playing XMA->PCM sidecar (%d samples @ %d Hz)\n",
                        mPCMSamples, mSrcSampleRate);
            sLoggedXmaOk = true;
        }
        mPlaying.store(true, std::memory_order_release);
        if (!mRegistered) {
            AudioDevice::GetInstance().AddSource(this);
            mRegistered = true;
        }
        return;
    }

    if (fmt != SampleData::kPCM && fmt != SampleData::kBigEndPCM) {
        static int sLoggedFmt = -2;
        if (fmt != sLoggedFmt) {
            std::printf("rb3 SFX: unsupported sample format %d (VAG/ADPCM/etc "
                        "not host-decodable) — SFX skipped\n", static_cast<int>(fmt));
            sLoggedFmt = fmt;
        }
        return;
    }

    mBigEndian = (fmt == SampleData::kBigEndPCM);
    mSrcSampleRate = data.GetSampleRate() > 0 ? data.GetSampleRate() : kOutputSampleRate;
    mPCMData = static_cast<const int16_t *>(data.mData);
    mPCMSamples = data.mNumSamples;
    if (mEndSample <= 0 || mEndSample > mPCMSamples) mEndSample = mPCMSamples;
    if (mPlayPos < 0.0 || mPlayPos >= static_cast<double>(mPCMSamples)) mPlayPos = 0.0;

    // One-time confirmation so the bank's real format is observable from a
    // headless run (answers "are RB3 SFX host-decodable PCM or console XMA?").
    static bool sLoggedOk = false;
    if (!sLoggedOk) {
        std::printf("rb3 SFX: playing PCM one-shot (%d samples @ %d Hz, %s)\n",
                    mPCMSamples, mSrcSampleRate, mBigEndian ? "big-endian" : "little-endian");
        sLoggedOk = true;
    }

    mPlaying.store(true, std::memory_order_release);
    if (!mRegistered) {
        AudioDevice::GetInstance().AddSource(this);
        mRegistered = true;
    }
}

void RB3SampleInstNative::StopImpl() {
    if (mRegistered) {
        AudioDevice::GetInstance().RemoveSource(this);
        mRegistered = false;
    }
    mPlaying.store(false, std::memory_order_release);
}

int RB3SampleInstNative::RenderAudio(float *output, int frameCount) {
    const int totalOut = frameCount * 2;
    if (!mPlaying.load(std::memory_order_acquire) || mPaused || !mPCMData) {
        std::memset(output, 0, totalOut * sizeof(float));
        return frameCount;
    }

    const int endPos = (mEndSample > 0) ? mEndSample : mPCMSamples;
    float volL = 0.0f;
    float volR = 0.0f;
    ComputePanGains(mInstVolume, mInstPan, volL, volR);
    // Source samples consumed per output sample (rate conversion + speed).
    const double rateRatio =
        static_cast<double>(mSrcSampleRate) / static_cast<double>(kOutputSampleRate) *
        static_cast<double>(mInstSpeed);

    for (int i = 0; i < frameCount; i++) {
        if (mPlayPos >= static_cast<double>(endPos)) {
            if (mLoop) {
                mPlayPos = static_cast<double>(mStartSample);
            } else {
                for (int j = i; j < frameCount; j++) {
                    output[j * 2 + 0] = 0.0f;
                    output[j * 2 + 1] = 0.0f;
                }
                mPlaying.store(false, std::memory_order_release);
                return frameCount;
            }
        }
        float s = LerpMono(mPCMData, mPCMSamples, mPlayPos, mBigEndian);
        output[i * 2 + 0] = s * volL;
        output[i * 2 + 1] = s * volR;
        mPlayPos += rateRatio;
    }
    return frameCount;
}

} // namespace

// SynthSample::NewInst — RB3 native/web implementation. Strong def wins over the
// weak rndobj_synth_link_stubs.s no-op. Signature matches the HX_NATIVE decl in
// SynthSample.h (bool loop, int startSample, int endSample).
SampleInst *SynthSample::NewInst(bool loop, int startSample, int endSample) {
    return new RB3SampleInstNative(this, loop, startSample, endSample);
}

#endif // HX_NATIVE
