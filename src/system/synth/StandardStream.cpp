#include "math/Decibels.h"
#include "synth/StandardStream.h"
#include "synth/Synth.h"
#include "os/Debug.h"
#include <functional>
#ifdef HX_NATIVE
#include <cstdlib> // getenv/atof for the RB3_STREAM_BUF_SECS native ring-depth knob
#endif
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h> // emscripten_sleep — JSPI yield in Resync's ReadDone wait
#include "os/Timer.h"              // throttle the yield like Loader.cpp PollUntilLoaded
#endif
#ifdef HX_NATIVE
// std::mem_fun was removed in C++17; std::mem_fn is the LP64 replacement.
#define mem_fun mem_fn
#endif
#include <math.h>
#include "utl/Symbols.h"

#ifdef HX_NATIVE
float StandardStream::sAudioOffsetMs = 0.0f;
#endif

#ifdef HX_NATIVE
// Incremental-load-perf (PLAN.md T1) frame-trace: charge the Play() Vorbis prime
// pump (the 500 header-pump + 20 ring-prefill PollStream loops) to
// gAudioPrimeMsThisFrame. HX_NATIVE-only; the Wii build is byte-identical. Read
// + zeroed by RB3FrameTraceRecord (native/src/rb3_frame_trace.cpp).
#include <chrono>
extern bool  gFrameTraceActive;
extern float gAudioPrimeMsThisFrame;
namespace {
    inline double AudioPrimeNowMs() {
        return std::chrono::duration<double, std::milli>(
                   std::chrono::steady_clock::now().time_since_epoch())
            .count();
    }
}
#endif

#ifdef HX_NATIVE
// ---------------------------------------------------------------------------
// Per-stem decoded-PCM dump (SAMPLE-ACCURATE verification hook).
//
// When env RB3_DUMP_STEMS=<dir> is set, every decoded ConsumeData() call appends
// each channel's mono int16 PCM (exactly the samples handed to the audio ring) to
// <dir>/stem_<NN>.s16, sample-aligned across channels (all channels advance by the
// same samplesToConsume per call). A <dir>/stems.json manifest records the sample
// rate, real vs virtual channel count, and the per-stem sample count so an offline
// tool can decode the SAME mogg channel and measure direct-waveform correlation.
//
// Entirely #ifdef HX_NATIVE and a no-op unless the env var is set (the dumper is
// only constructed on first decode when getenv() returns non-NULL), so the Wii
// decomp match build is byte-identical and there is zero overhead otherwise.
// ---------------------------------------------------------------------------
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
namespace {
class StemDumper {
public:
    static StemDumper *Get(int numChannels, int realChannels, int sampleRate) {
        static StemDumper *sInst = NULL;
        static bool sTried = false;
        if (!sTried) {
            sTried = true;
            const char *dir = getenv("RB3_DUMP_STEMS");
            if (dir && dir[0]) {
                sInst = new StemDumper(dir, numChannels, realChannels, sampleRate);
            }
        }
        return sInst;
    }

    StemDumper(const char *dir, int numChannels, int realChannels, int sampleRate)
        : mDir(dir), mNumChannels(numChannels), mRealChannels(realChannels),
          mSampleRate(sampleRate), mTotalSamples(0) {
        mFiles.resize(numChannels, NULL);
        for (int i = 0; i < numChannels; i++) {
            char path[1024];
            snprintf(path, sizeof(path), "%s/stem_%02d.s16", mDir.c_str(), i);
            mFiles[i] = fopen(path, "wb");
        }
        fprintf(stderr,
                "[STEM_DUMP] dir=%s channels=%d real=%d rate=%d (RB3_DUMP_STEMS)\n",
                mDir.c_str(), numChannels, realChannels, sampleRate);
    }

    // Append `count` int16 samples for channel `ch`.
    void Write(int ch, const short *samples, int count) {
        if (ch >= 0 && ch < (int)mFiles.size() && mFiles[ch])
            fwrite(samples, sizeof(short), (size_t)count, mFiles[ch]);
    }

    // Called once per ConsumeData after all channels written (advances clock).
    void Advance(int count) {
        mTotalSamples += count;
        // Flush manifest periodically so the python tool can read it even if the
        // process is SIGKILLed by the capture harness before clean shutdown.
        if ((mTotalSamples & 0xFFFF) < (unsigned)count) WriteManifest();
    }

    void WriteManifest() {
        char path[1024];
        snprintf(path, sizeof(path), "%s/stems.json", mDir.c_str());
        FILE *f = fopen(path, "wb");
        if (!f) return;
        fprintf(f,
                "{\"sample_rate\": %d, \"num_channels\": %d, \"real_channels\": %d, "
                "\"total_samples\": %d, \"bytes_per_sample\": 2, \"format\": \"s16le\"}\n",
                mSampleRate, mNumChannels, mRealChannels, mTotalSamples);
        fclose(f);
    }

private:
    std::string mDir;
    int mNumChannels;
    int mRealChannels;
    int mSampleRate;
    int mTotalSamples;
    std::vector<FILE *> mFiles;
};
} // namespace
#endif

StandardStream::ChannelParams::ChannelParams()
    : mPan(0.0f), mSlipSpeed(1.0f), mSlipEnabled(0), mADSR(), mFaders(0), mFxSend(0, 0),
      mFXCore(kFXCoreNone), mPitchShift(0) {
    mFaders.AddLocal(_parent)->SetVal(0.0f);
    mFaders.AddLocal(_default)->SetVal(0.0f);
}

StandardStream::StandardStream(File *f, float f1, float f2, Symbol sym, bool b1, bool b2)
    : mPollingEnabled(b2) {
    MILO_ASSERT(f, 0x46);
    mExt = sym;
    mFile = f;
    mInfoChannels = -1;
    unkec = -1.0f;
    Init(f1, f2, sym, false);
}

void StandardStream::Init(float f1, float f2, Symbol sym, bool b) {
    ClearLoopMarkers();
    mAccumulatedLoopbacks = 0.0f;
    mGetInfoOnly = false;
    mState = kInit;
    mSampleRate = 0;
    mBufSecs = f2;
    if (mBufSecs == 0.0f) {
        SystemConfig("synth")->FindData("stream_buf_size", mBufSecs, true);
    }
#ifdef HX_NATIVE
    // The Wii's 1.2s stream_buf_size assumed a hardware DSP buffering mNumBuffers
    // slots behind the 0x18000 staging ring. The native/web bridge has no DSP and
    // plays the ring directly, so the ring depth IS mBufSecs. 1.2s leaves almost
    // no slack for the multitrack mix (11-15 vorbis stems decoded on the game/
    // PumpAudio thread) to stay ahead of real-time, and any transient deficit
    // underruns -> zero-fill (dropout/"static"). Deepen it (default 4s; env
    // RB3_STREAM_BUF_SECS overrides). The ring array (mBuffer, 16 chunks ~9.1s)
    // caps the realised depth. Preview (1 stream) was never affected.
    {
        float minSecs = 4.0f;
        const char *envSecs = getenv("RB3_STREAM_BUF_SECS");
        if (envSecs && envSecs[0]) {
            float v = (float)atof(envSecs);
            if (v > 0.0f) minSecs = v;
        }
        if (mBufSecs < minSecs) mBufSecs = minSecs;
    }
#endif
    mFileStartMs = f1;
    mStartMs = f1;
    mLastStreamTime = f1;
    mTimer.Reset(f1);
    mFloatSamples = false;
    if (!b) {
        MILO_ASSERT(mChanParams.empty(), 0x6B);
        mChanParams.resize(0x20);
        for (int i = 0; i < 0x20; i++) {
            mChanParams[i] = new ChannelParams();
        }
        mVirtualChans = 0;
        mSpeed = 1.0f;
    } else {
        while (mChanParams.size() < 0x20) {
            mChanParams.push_back((new ChannelParams()));
        }
    }
    mJumpFromSamples = 0;
    mJumpToSamples = 0;
    mJumpFromMs = 0.0f;
    mJumpToMs = 0.0f;
    mCurrentSamp = 0;
    mThrottle = SystemConfig("synth", "oggvorbis")->FindFloat("throttle");
    if (mPollingEnabled)
        StartPolling();
    mRdr = TheSynth->NewStreamDecoder(mFile, this, sym);
}

void StandardStream::Destroy() {
    RELEASE(mRdr);
    DeleteAll(mChannels);
}

StandardStream::~StandardStream() {
    RELEASE(mFile);
    for (int i = 0; i < unk104.size(); i++) {
        delete[] unk104[i];
    }
    Destroy();
    DeleteAll(mChanParams);
    while (!mVirtBufs.empty()) {
        _MemFree(mVirtBufs.back());
        mVirtBufs.pop_back();
    }
}

const char *StandardStream::GetSoundDisplayName() {
    if (!IsPlaying())
        return gNullStr;
    else if (mFile) {
        return MakeString("StandardStream: %s", FileGetName(mFile->Filename().c_str()));
    } else {
        return MakeString("StandardStream: --no file--");
    }
}

void StandardStream::SynthPoll() { PollStream(); }

void StandardStream::PollStream() {
    if (mRdr) {
        mFrameTimer.Restart();
        mRdr->Poll(
            Max(mFrameTimer.GetLastMs() * mThrottle, mState == kBuffering ? 8.0f : 1.0f)
        );
        std::for_each(
            mChannels.begin(), mChannels.end(), std::mem_fun(&StreamReceiver::Poll)
        );
#ifdef HX_NATIVE
        {
            static int sLastState = -99;
            if ((int)mState != sLastState) {
                MILO_LOG("STREAM_DBG: StandardStream::PollStream state %d -> %d (chans=%d)\n",
                         sLastState, (int)mState, (int)mChannels.size());
                sLastState = (int)mState;
            }
        }
#endif
        switch (mState) {
        case kInit:
        case kReady:
        case kFinished:
            break;
        case kBuffering:
            if (StuffChannels()) {
#ifdef HX_NATIVE
                MILO_LOG("STREAM_DBG: kBuffering -> kReady (StuffChannels returned true)\n");
#endif
                mState = kReady;
            }
            break;
        case kPlaying:
        case kSuspended:
        case kStopped:
            StuffChannels();
            if (mChannels[0]->mDoneBufferCounter > mChannels[0]->mNumBuffers + 2) {
                mState = kFinished;
            }
            break;
        default:
            MILO_FAIL("bad state logic.");
            break;
        }
        if (mState != kInit && mJumpFromSamples != 0) {
            int jumpFrom = mJumpFromSamples;
            if (jumpFrom < 0) {
                if (mRdr->Done()) {
                    DoJump();
                }
            } else if (jumpFrom > 0) {
                if (jumpFrom < mJumpToSamples) {
                    if (mCurrentSamp >= jumpFrom && mCurrentSamp < mJumpToSamples) {
                        DoJump();
                    }
                } else if (jumpFrom > mJumpToSamples) {
                    if (mCurrentSamp >= jumpFrom) {
                        DoJump();
                    }
                }
            }
        }
        UpdateVolumes();
        UpdateTime();
    }
}

void StandardStream::InitInfo(int i1, int sampleRate, bool floatSamples, int i4) {
    unk144 = i4;
    unkec = (float)i4 / (float)sampleRate;
    int numChannels = i1 + mVirtualChans;
    mInfoChannels = numChannels;
    if (!mGetInfoOnly) {
        if (mSampleRate == 0) {
            mSampleRate = sampleRate;
            mFloatSamples = floatSamples;
            const int kStreamBufSize = 0xC000;
            int bufBytes = (mBufSecs * (float)sampleRate * 2.0f);
            bufBytes = bufBytes + (2*kStreamBufSize - bufBytes % (2*kStreamBufSize));
            MILO_ASSERT(bufBytes % (2*kStreamBufSize) == 0, 0x142);
            int i38 = bufBytes / kStreamBufSize;

            int i3c =
                (((mSampleRate * SystemConfig("synth", "iop")->FindInt("max_slip")) / 1000
                  << 1)
                 / 0xC000)
                    * 2
                + 4;
            MaxEq(i3c, i38);
            for (int i = 0; i < numChannels; i++) {
                mChannels.push_back(
                    StreamReceiver::New(i38, sampleRate, mChanParams[i]->mSlipEnabled, i)
                );
            }
            for (int i = 0; i < mVirtualChans; i++) {
                mVirtBufs.push_back(_MemAlloc((mFloatSamples ? 4 : 2) << 0xB, 0));
            }
            mState = kBuffering;
        } else {
            MILO_ASSERT(numChannels == mChannels.size(), 0x161);
            MILO_ASSERT(mSampleRate == sampleRate, 0x162);
            MILO_ASSERT(mFloatSamples == floatSamples, 0x163);
        }
        if (mJumpSamplesInvalid) {
            setJumpSamplesFromMs(mJumpFromMs, mJumpToMs);
        }
        MILO_ASSERT(mChanParams.size() >= numChannels, 0x16C);
        int i = 0;
        for (; i < numChannels; i++) {
            mChannels[i]->SetPan(mChanParams[i]->mPan);
            UpdateSpeed(i);
            mChannels[i]->SetADSR(mChanParams[i]->mADSR);
            mChannels[i]->SetFXCore(mChanParams[i]->mFXCore);
            mChannels[i]->SetPitchShift(mChanParams[i]->mPitchShift);
        }
        for (; i < mChanParams.size(); i++) {
            delete mChanParams[i];
        }
        mChanParams.resize(numChannels);
        numChannels = MsToSamp(mFileStartMs);
        mCurrentSamp = numChannels;
        if (numChannels != 0) {
            mRdr->Seek(numChannels);
        }
        Faders()->SetDirty();
        UpdateFXSends();
    }
}

bool StandardStream::StuffChannels() {
    bool ret = true;
    for (int i = 0; i < mChannels.size(); i++) {
        if (!mChannels[i]->Ready())
            ret = false;
    }
    if (mRdr && mRdr->Done() && mJumpFromSamples == 0) {
        std::for_each(
            mChannels.begin(), mChannels.end(), std::mem_fun(&StreamReceiver::EndData)
        );
    }
    return ret;
}

#define DIM(lol) 0x1EU

int StandardStream::ConsumeData(void **v, int numSamples, int startSamp) {
    if (mGetInfoOnly)
        return 0;
    int numChannels = mChannels.size();
    int realChannels = numChannels - mVirtualChans;
    MILO_ASSERT(numChannels != 0, 0x1A9);
    if (startSamp >= 0 && startSamp != mCurrentSamp) {
        MILO_LOG("sample mismatch: expected %i, got %i\n", mCurrentSamp, startSamp);
        mCurrentSamp = startSamp;
    }
    void *pcm[0x1E];
    MILO_ASSERT(numChannels < DIM(pcm), 0x1B3);

    for (int i = 0; i < numChannels; i++) {
        if (i < realChannels) {
            pcm[i] = v[i];
        } else {
            pcm[i] = mVirtBufs[i - realChannels];
        }
    }

    int samplesToConsume = 0x800;
    if (numSamples < 0x800)
        samplesToConsume = numSamples;

    if (mJumpFromSamples > 0) {
        if (mJumpFromSamples < mJumpToSamples) {
            if (mCurrentSamp < mJumpToSamples) {
                if (mCurrentSamp <= mJumpFromSamples) {
                    int remaining = mJumpFromSamples - mCurrentSamp;
                    if ((unsigned int)remaining < (unsigned int)samplesToConsume)
                        samplesToConsume = remaining;
                } else {
                    samplesToConsume = 0;
                }
            }
        } else if (mJumpFromSamples > mJumpToSamples) {
            MILO_ASSERT(mCurrentSamp <= mJumpFromSamples, 0x1CF);
            int remaining = mJumpFromSamples - mCurrentSamp;
            if ((unsigned int)remaining < (unsigned int)samplesToConsume)
                samplesToConsume = remaining;
        }
    }

    for (std::vector<StreamReceiver *>::iterator it = mChannels.begin();
         it != mChannels.end(); ++it) {
        int bytes = (unsigned int)(*it)->BytesWriteable() >> 1;
        if (bytes < samplesToConsume)
            samplesToConsume = bytes;
    }

    if (samplesToConsume != 0) {
        int bytesPerSample = mFloatSamples ? 4 : 2;
        int copySize = bytesPerSample * samplesToConsume;
        for (std::vector<std::pair<int, int> >::iterator mapIt = mChanMaps.begin();
             mapIt != mChanMaps.end(); ++mapIt) {
            memcpy(pcm[mapIt->second], pcm[mapIt->first], copySize);
        }

#ifdef HX_NATIVE
        StemDumper *stemDump =
            StemDumper::Get(numChannels, realChannels, mSampleRate);
#endif
        short convBuf[0x800];
        for (int chIdx = 0; chIdx < numChannels; chIdx++) {
            if (mFloatSamples) {
                float *src = (float *)pcm[chIdx];
                short *dst = convBuf;
                for (unsigned int j = 0; j < (unsigned int)samplesToConsume; j++) {
                    float f = 32767.0f * src[j];
                    if (f > 32767.0f)
                        f = 32767.0f;
                    else if (f < -32767.0f)
                        f = -32767.0f;
                    *dst = (short)f;
#ifndef HX_NATIVE
                    // Wii-match ONLY: the original loop indexes src[j] AND advances src,
                    // so it actually reads src[2j] — decimation-by-2. DEAD on Wii because
                    // Tremor decodes to int16 (mFloatSamples=false → the else branch runs),
                    // so this never executed there and the decomp matched. On native
                    // (libvorbis → mFloatSamples=true) this branch IS live; the double
                    // advance halves the sample rate → 2x-pitch "chipmunk" chirps +
                    // broadband aliasing static across the whole upper spectrum. DC3's
                    // identical code reads [j] only (StandardStream.cpp:893). Drop src++.
                    src++;
#endif
                    dst++;
                }
                mChannels[chIdx]->WriteData(convBuf, samplesToConsume << 1);
#ifdef HX_NATIVE
                if (stemDump) stemDump->Write(chIdx, convBuf, samplesToConsume);
#endif
            } else {
                mChannels[chIdx]->WriteData(pcm[chIdx], samplesToConsume << 1);
#ifdef HX_NATIVE
                if (stemDump)
                    stemDump->Write(chIdx, (const short *)pcm[chIdx], samplesToConsume);
#endif
            }
        }
#ifdef HX_NATIVE
        if (stemDump) stemDump->Advance(samplesToConsume);
#endif
    }

    mCurrentSamp += samplesToConsume;
    return samplesToConsume;
}

bool StandardStream::Fail() { return mRdr && mRdr->Fail(); }

bool StandardStream::IsReady() const {
    return mState == kReady || mState == kPlaying || mState == kStopped;
}

bool StandardStream::IsFinished() const { return mState == kFinished; }
bool StandardStream::IsPlaying() const { return mState == kPlaying; }
bool StandardStream::IsPaused() const { return mState == kStopped; }
int StandardStream::GetNumChannels() const { return mChannels.size(); }
int StandardStream::GetNumChanParams() const { return mChanParams.size(); }

void StandardStream::Play() {
#ifdef HX_NATIVE
    // On console, a background decode thread parses Vorbis headers between
    // stream creation and Play(). On native there's no decode thread, so we
    // must pump the reader until the stream transitions from kInit -> kReady.
    if (mState == kInit && mRdr) {
        double ftPrime = gFrameTraceActive ? AudioPrimeNowMs() : 0.0;
        for (int i = 0; i < 500 && !IsReady(); i++) {
            PollStream();
        }
        if (gFrameTraceActive)
            gAudioPrimeMsThisFrame += (float)(AudioPrimeNowMs() - ftPrime);
    }
#endif
    MILO_ASSERT(IsReady() || mState == kSuspended, 0x227);
    UpdateVolumes();
#ifdef HX_NATIVE
    // Pre-fill ring buffers before the audio device starts consuming. The
    // IsReady pump above only runs until header parsing completes (kReady);
    // without pre-filling, the first audio callback fires on empty buffers,
    // causing an audible gap and delayed timing.
    {
        double ftFill = gFrameTraceActive ? AudioPrimeNowMs() : 0.0;
        for (int i = 0; i < 20; i++) {
            PollStream();
        }
        if (gFrameTraceActive)
            gAudioPrimeMsThisFrame += (float)(AudioPrimeNowMs() - ftFill);
    }
#endif
    std::for_each(mChannels.begin(), mChannels.end(), std::mem_fun(&StreamReceiver::Play));
    mState = kPlaying;
    mTimer.Start();
}

DECOMP_FORCEACTIVE(
    StandardStream, "mState == kStopped || mState == kPlaying || mState == kSuspended"
)

void StandardStream::Stop() {
    if (mState == kPlaying) {
        std::for_each(
            mChannels.begin(), mChannels.end(), std::mem_fun(&StreamReceiver::Stop)
        );
        mState = kStopped;
        mTimer.Stop();
    }
}

void StandardStream::Resync(float f) {
    int bytes;
#ifdef HX_NATIVE
    // Resync must wait for any in-flight read to drain before it Destroy()s the
    // reader and re-Seek()s the file — that contract is unchanged. What changes
    // is HOW we wait. The matched Wii body is `while (!mFile->ReadDone(bytes));`,
    // a zero-yield spin. On native that completes (synchronous stdio I/O), but
    // on the web build mFile may be a WebRangeFile whose ReadDone() can only flip
    // to true after the browser event loop runs the emscripten_fetch callback —
    // and this loop never returns control to the event loop, so the tab hangs
    // forever (latent web deadlock; matrix doc 07 suspect c). Route the wait
    // through the same JSPI yield pattern Loader.cpp's PollUntilLoaded uses:
    // throttle a full event-loop turn (emscripten_sleep(0)) into the wait so the
    // fetch can land, gated to ~one yield per RB3_LOADER_MIN_YIELD_MS of spin so
    // the canvas keeps compositing without pathological suspend churn. A hard
    // iteration cap is a final safety valve. On plain native (no __EMSCRIPTEN__)
    // the wait is the original spin, byte-faithful in spirit (I/O is sync there).
    // Escape hatch: RB3_RESYNC_YIELD_OFF=1 restores the bare spin.
#ifdef __EMSCRIPTEN__
    static int sYieldOff = -1;
    if (sYieldOff < 0) {
        const char *e = ::getenv("RB3_RESYNC_YIELD_OFF");
        sYieldOff = (e && e[0] && e[0] != '0') ? 1 : 0;
    }
    static float sMinYieldMs = -1.0f;
    if (sMinYieldMs < 0.0f) {
        sMinYieldMs = 16.0f; // mirror Loader.cpp PollUntilLoaded's default
        if (const char *e = ::getenv("RB3_LOADER_MIN_YIELD_MS")) {
            if (e[0])
                sMinYieldMs = (float)atof(e);
        }
        if (sMinYieldMs < 0.0f)
            sMinYieldMs = 0.0f;
    }
    if (sYieldOff) {
        while (!mFile->ReadDone(bytes))
            ;
    } else {
        int maxIter = 2000000; // safety valve: a permanently-stalled fetch can't
                               // hang the tab forever (watchdog-class backstop)
        Timer sinceYield;
        sinceYield.Restart();
        while (!mFile->ReadDone(bytes)) {
            if (--maxIter <= 0) {
                MILO_WARN("StandardStream::Resync: ReadDone wait cap hit");
                break;
            }
            sinceYield.Split();
            if (Timer::CyclesToMs(sinceYield.mCycles) >= sMinYieldMs) {
                emscripten_sleep(0); // JSPI: suspend -> event loop (fetch lands) -> resume
                sinceYield.Restart();
            }
        }
    }
#else
    // Plain native: synchronous I/O makes ReadDone complete promptly; keep the
    // bare spin (matches the Wii contract, no event loop to yield to).
    while (!mFile->ReadDone(bytes))
        ;
#endif
#else
    while (!mFile->ReadDone(bytes))
        ;
#endif
    Destroy();
    mFile->Seek(0, 0);
    float f88 = mJumpFromMs;
    float f8c = mJumpToMs;
    String str94 = mJumpFile;
    Init(f, mBufSecs, mExt, true);
    if (f88 != 0) {
        SetJump(f88, f8c, str94.c_str());
    }
}

void StandardStream::LoadMarkerList(const char *cc) {
    ClearMarkerList();
    FileStream stream(cc, FileStream::kRead, 1);
    int i94 = 0;
    stream >> i94;
    int i98 = 0;
    stream >> i98;
    for (int i = 0; i < i98; i++) {
        Marker marker;
        stream >> marker.name;
        stream >> marker.position;
        marker.posMS = ((float)marker.position * 1000.0f) / (float)i94;
        AddMarker(marker);
    }
}

void StandardStream::ClearMarkerList() { mMarkerList.clear(); }

void StandardStream::AddMarker(Marker marker) { mMarkerList.push_back(marker); }

int StandardStream::MarkerListSize() const { return mMarkerList.size(); }

bool StandardStream::MarkerAt(int idx, Marker &marker) const {
    if (idx >= MarkerListSize() || idx < 0)
        return false;
    else {
        marker = mMarkerList[idx];
        return true;
    }
}

void StandardStream::SetLoop(String &str1, String &str2) {
    for (int i = 0; i < mMarkerList.size(); i++) {
        if (mMarkerList[i].name == str1) {
            mStartMarker = mMarkerList[i];
        }
        if (mMarkerList[i].name == str2) {
            mEndMarker = mMarkerList[i];
        }
    }
    SetJump(mEndMarker.posMS, mStartMarker.posMS, 0);
}

bool StandardStream::CurrentLoopPoints(Marker &start, Marker &end) {
    if (mJumpFromSamples == 0)
        return false;
    else {
        start = mStartMarker;
        end = mEndMarker;
        return true;
    }
}

void StandardStream::AbandonLoop() { ClearJump(); }

void StandardStream::ClearLoopMarkers() {
    mEndMarker.position = 0;
    mEndMarker.name = "";
    mStartMarker.position = 0;
    mStartMarker.name = "";
    mAccumulatedLoopbacks = 0;
}

void StandardStream::SetJump(float fromMs, float toMs, const char *file) {
    MILO_ASSERT(toMs >= 0, 0x2C4);
    MILO_ASSERT(fromMs >= 0 || fromMs == kStreamEndMs, 0x2C5);
    mJumpFromMs = fromMs;
    mJumpToMs = toMs;
    mJumpFile = file;
    if (!mJumpFile.empty()) {
        mJumpFile += ".";
        mJumpFile += mExt.mStr;
    }
    if (GetSampleRate() == 0)
        mJumpSamplesInvalid = true;
    else
        setJumpSamplesFromMs(fromMs, toMs);
}

void StandardStream::setJumpSamplesFromMs(float f1, float f2) {
    mJumpFromSamples = kStreamEndSamples;
    mJumpToSamples = 0;
    if (kStreamEndMs != f1) {
        mJumpFromSamples = MsToSamp(f1);
    }
    if (f2 != 0) {
        mJumpToSamples = MsToSamp(f2);
    }
    if (unk144 != -1) {
        if (mJumpFromSamples >= unk144) {
            MILO_WARN(
                "%s: JumpFromSamples (%g sec) exceeds the length of the stream (%g sec)!",
                mFile ? mFile->Filename() : String("SynthStream"),
                f1 / 1000.0f,
                SampToMs(unk144) / 1000.0f
            );
        }
        if (mJumpToSamples >= unk144) {
            MILO_WARN(
                "%s: JumpToSamples (%g sec) exceeds the length of the stream (%g sec)!",
                mFile ? mFile->Filename() : String("SynthStream:"),
                f2 / 1000.0f,
                SampToMs(unk144) / 1000.0f
            );
        }
    }
}

void StandardStream::SetJumpSamples(int fromSamples, int toSamples, const char *file) {
    MILO_ASSERT(toSamples >= 0, 0x2F4);
    MILO_ASSERT(fromSamples >= 0 || fromSamples == kStreamEndSamples, 0x2F5);
    MILO_ASSERT(file || fromSamples > toSamples || fromSamples == kStreamEndSamples, 0x2F6);
    MILO_ASSERT(mJumpFromSamples == 0, 0x2F8);
    mJumpFromSamples = fromSamples;
    mJumpToSamples = toSamples;
    mJumpFile = file;
    if (!mJumpFile.empty()) {
        mJumpFile += ".";
        mJumpFile += mExt.mStr;
    }
    mJumpSamplesInvalid = false;
}

void StandardStream::ClearJump() {
    mJumpFromSamples = 0;
    mJumpFromMs = 0;
    mJumpToSamples = 0;
    mJumpToMs = 0;
}

void StandardStream::DoJump() {
    MILO_ASSERT(mJumpFromSamples != 0, 0x314);
    if (!mJumpFile.empty()) {
        delete mFile;
        delete mRdr;
        mFile = NewFile(mJumpFile.c_str(), 2);
        if (!mFile)
            MILO_FAIL("\nCould not open %s", mJumpFile.c_str());
        mRdr = TheSynth->NewStreamDecoder(mFile, this, mExt);
        mFileStartMs = SampToMs(mJumpToSamples);
        mCurrentSamp = 0;
        ClearJump();
    } else {
        if (mJumpFromSamples != mJumpToSamples) {
            if (mRdr)
                mRdr->Seek(mJumpToSamples);
            mCurrentSamp = mJumpToSamples;
        }
    }
    mAccumulatedLoopbacks -= mEndMarker.posMS - mStartMarker.posMS;
}

void StandardStream::EnableReads(bool b) {
    if (mRdr)
        mRdr->EnableReads(b);
}

float StandardStream::GetRawTime() {
    float bytes = mChannels[0]->GetBytesPlayed() / 2;
    return (bytes / (float)mSampleRate) * 1000.0f + mStartMs;
}

float StandardStream::GetTime() {
    if (mChannels.empty() || mSampleRate == 0)
        return mStartMs;
#ifdef HX_NATIVE
    return mLastStreamTime + sAudioOffsetMs;
#else
    return mLastStreamTime;
#endif
}

void StandardStream::UpdateTime() {
    if (mChannels.empty() || mSampleRate == 0) {
        mLastStreamTime = mStartMs;
        return;
    }
    float rawTime = GetRawTime();
#ifdef HX_NATIVE
    // In headless mode (no real audio device) the audio callback fires very
    // slowly, so rawTime lags far behind wall-clock time. Use an independent
    // wall-clock timer (not mTimer, which gets drift-corrected toward rawTime)
    // to detect this; once detected, bypass drift correction permanently.
    if (mState == kPlaying && !mUseTimerFallback) {
        if (!mWallClockStarted) {
            mWallClock.Start();
            mWallClockStarted = true;
        }
        mWallClock.Split();
        float wallElapsed = mWallClock.Ms();
        if (wallElapsed > 500.0f) {
            float audioElapsed = rawTime - mStartMs;
            if (audioElapsed < wallElapsed * 0.1f) {
                // Audio output is < 10% real-time -- switch to wall-clock timing.
                mUseTimerFallback = true;
                mTimer.Reset(mStartMs + wallElapsed);
            }
        }
    }
    if (mUseTimerFallback) {
        mLastStreamTime = mTimer.Ms();
        return;
    }
#endif
    float n1 = (float)floor(rawTime / 5.3333335f + 0.5f);
    float quantized0 = n1 * 5.3333335f;
    float rawMinusQuantized = rawTime - quantized0;
    float quantized =
        (float)floor((rawTime - rawMinusQuantized) / 5.3333335f + 0.5f) * 5.3333335f;
    float timerMs = mTimer.Ms();
    float adjusted = timerMs - rawMinusQuantized;
    float adjustedQuantized = (float)floor(adjusted / 5.3333335f);
    if (quantized != adjustedQuantized * 5.3333335f) {
        float drift = quantized - adjusted;
        if (drift < 0.0f) {
            drift += 5.3333335f;
        }
        if (fabsf(drift) < 5.3333335f) {
            drift *= 0.1f;
        }
        mTimer.Reset(mTimer.Ms() + drift);
        if (fabsf(drift) > 50.0f && sReportLargeTimerErrors) {
            MILO_WARN("timer error is large: %f\n", drift);
        }
    }
    mLastStreamTime = mTimer.Ms();
}

void StandardStream::UpdateTimeByFiltering() {
    if (mChannels.empty() || mSampleRate == 0) {
        mLastStreamTime = mStartMs;
        return;
    }

    float drift = GetRawTime() - mTimer.Ms();

    if (fabsf(drift) > 50.0f) {
        if (sReportLargeTimerErrors) {
            MILO_WARN("timer error is large: %f\n", drift);
        }
    } else {
        drift *= 0.1f;
    }

    mTimer.Reset(mTimer.Ms() + drift);
    mLastStreamTime = mTimer.Ms();
}

float StandardStream::GetJumpBackTotalTime() { return mAccumulatedLoopbacks; }

float StandardStream::GetInSongTime() { return GetTime() + GetJumpBackTotalTime(); }

int StandardStream::GetLoopInstances() { return 0; }

void StandardStream::SetVolume(int chan, float vol) {
    MILO_ASSERT_RANGE(chan, 0, mChanParams.size(), 0x3D0);
    mChanParams[chan]->mFaders.FindLocal(_default, true)->SetVal(vol);
}

void StandardStream::SetPan(int chan, float pan) {
    MILO_ASSERT_RANGE(chan, 0, mChanParams.size(), 0x3D8);
    mChanParams[chan]->mPan = pan;
    if (!mChannels.empty()) {
        mChannels[chan]->SetPan(pan);
    }
}

void StandardStream::SetSpeed(float speed) {
    mSpeed = speed;
    for (int i = 0; i < mChannels.size(); i++) {
        UpdateSpeed(i);
    }
    mTimer.SetSpeed(speed);
}

void StandardStream::SetADSR(int chan, const ADSR &adsr) {
    MILO_ASSERT_RANGE(chan, 0, mChanParams.size(), 0x3EC);
    mChanParams[chan]->mADSR = adsr;
    if (!mChannels.empty()) {
        mChannels[chan]->SetADSR(adsr);
    }
}

float StandardStream::GetVolume(int chan) const {
    MILO_ASSERT_RANGE(chan, 0, mChanParams.size(), 0x3F6);
    return mChanParams[chan]->mFaders.GetVal();
}

float StandardStream::GetPan(int chan) const {
    MILO_ASSERT_RANGE(chan, 0, mChanParams.size(), 0x3FE);
    return mChanParams[chan]->mPan;
}

float StandardStream::GetSpeed() const { return mSpeed; }

int StandardStream::MsToSamp(float ms) {
    MILO_ASSERT(mSampleRate, 0x40B);
    return mSampleRate * (ms / 1000.0f);
}

float StandardStream::SampToMs(int samps) {
    MILO_ASSERT(mSampleRate, 0x412);
    float ms = (float)samps / (float)mSampleRate;
    return ms * 1000.0f;
}

void StandardStream::UpdateVolumes() {
    if (Faders()->Dirty()) {
        float val = Faders()->GetVal();
        for (std::vector<ChannelParams *>::iterator it = mChanParams.begin();
             it != mChanParams.end();
             ++it) {
            (*it)->mFaders.FindLocal(_parent, true)->SetVal(val);
        }
        Faders()->ClearDirty();
    }
    for (int i = 0; i < mChannels.size(); i++) {
        if (mChanParams[i]->mFaders.Dirty()) {
            float ratio = DbToRatio(mChanParams[i]->mFaders.GetVal());
            ClampEq(ratio, 0.0f, 1.0f);
            mChannels[i]->SetVolume(ratio);
            mChanParams[i]->mFaders.ClearDirty();
        }
    }
}

void StandardStream::UpdateFXSends() {
    for (int i = 0; i < mChannels.size(); i++) {
        mChannels[i]->SetFXSend(mChanParams[i]->mFxSend);
    }
}

FaderGroup *StandardStream::ChannelFaders(int channel) {
    MILO_ASSERT_RANGE(channel, 0, mChanParams.size(), 0x43F);
    return &mChanParams[channel]->mFaders;
}

void StandardStream::AddVirtualChannels(int i) {
    MILO_ASSERT(mChannels.empty(), 0x447);
    mVirtualChans += i;
}

void StandardStream::RemapChannel(int i1, int i2) {
    mChanMaps.push_back(std::make_pair(i1, i2));
}

void StandardStream::UpdateSpeed(int chn) {
    MILO_ASSERT_RANGE(chn, 0, mChanParams.size(), 0x454);
    mChannels[chn]->SetSpeed(mSpeed);
    if (mChanParams[chn]->mSlipEnabled) {
        mChannels[chn]->SetSlipSpeed((float)mSpeed * mChanParams[chn]->mSlipSpeed);
    }
}

void StandardStream::EnableSlipStreaming(int channel) {
    if (mChannels.empty()) {
        MILO_ASSERT_RANGE(channel, 0, mChanParams.size(), 0x46C);
        mChanParams[channel]->mSlipEnabled = true;
    }
}

void StandardStream::SetSlipOffset(int channel, float offset) {
    MILO_ASSERT_RANGE(channel, 0, mChanParams.size(), 0x473);
    mChannels[channel]->SetSlipOffset(offset);
}

void StandardStream::SlipStop(int channel) {
    MILO_ASSERT_RANGE(channel, 0, mChanParams.size(), 0x47A);
    mChannels[channel]->SlipStop();
}

void StandardStream::SetSlipSpeed(int channel, float speed) {
    MILO_ASSERT_RANGE(channel, 0, mChanParams.size(), 0x481);
    MILO_ASSERT(mChanParams[channel]->mSlipEnabled, 0x482);
    mChanParams[channel]->mSlipSpeed = speed;
    if (!mChannels.empty()) {
        UpdateSpeed(channel);
    }
}

float StandardStream::GetSlipOffset(int channel) {
    MILO_ASSERT_RANGE(channel, 0, mChanParams.size(), 0x48B);
    return mChannels[channel]->GetSlipOffset();
}

void StandardStream::SetFXSend(int channel, FxSend *send) {
    MILO_ASSERT_RANGE(channel, 0, mChanParams.size(), 0x495);
    mChanParams[channel]->mFxSend = send;
    if (!mChannels.empty()) {
        mChannels[channel]->SetFXSend(send);
    }
}

void StandardStream::SetFX(int channel, bool fx) {
    MILO_ASSERT_RANGE(channel, 0, mChanParams.size(), 0x4A1);
    // TODO: replace printf with MILO_LOG/MILO_WARN/some MILO macro
    printf("mChanParams.size() == %d\n", mChanParams.size());
    int numChannels = mChannels.size();
    printf("mChannels.size() == %d\n", numChannels);
    if (numChannels > channel) {
        mChannels[channel]->SetFX(fx);
    }
}

bool StandardStream::GetFX(int channel) const {
    MILO_ASSERT_RANGE(channel, 0, mChanParams.size(), 0x4B6);
    return mChannels[channel]->GetFX();
}

void StandardStream::SetFXCore(int channel, FXCore core) {
    MILO_ASSERT_RANGE(channel, 0, mChanParams.size(), 0x4BD);
    mChanParams[channel]->mFXCore = core;
}

FXCore StandardStream::GetFXCore(int channel) const {
    MILO_ASSERT_RANGE(channel, 0, mChanParams.size(), 0x4C4);
    return mChanParams[channel]->mFXCore;
}

void StandardStream::SetPitchShift(int channel, bool shift) {
    if (channel < mChanParams.size() && channel < mChannels.size()) {
        mChanParams[channel]->mPitchShift = shift;
        mChannels[channel]->SetPitchShift(shift);
    }
}