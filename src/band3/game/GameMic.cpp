#include "game/GameMic.h"
#include "decomp.h"
#include "math/Utl.h"
#include "obj/Data.h"
#include "obj/Task.h"
#include "os/Debug.h"
#include "os/Timer.h"
#include "synth/MicNull.h"
#include "synth/Synth.h"
#include "utl/FileStream.h"
#include "utl/MemStream.h"
#include "utl/Wav.h"
#include "utl/WaveFile.h"
#include <time.h>

extern "C" void AnalyzeBlock__13PitchDetectorFPCcPsiffRfRfRf(
    PitchDetector *self,
    const char *name,
    const short *samples,
    int numSamples,
    float sensitivity,
    float gain,
    float &livePitch,
    float &energy,
    float &outc
);

bool gIdxTaken[6];

DECOMP_FORCEBLOCK(GameMic, (Mic *m), { void (Mic::*mfp)(bool) = &Mic::SetMute; (void)mfp; (void)m; })

GameMic::GameMic(int id)
    : mMicID(id), mUSB(1), mPlayback(1), mWriteWav(0), mPlaybackSampleRate(0), mStoredAudio(0), mDetector(0),
      mNullMic(0), mMicVolumeClamp(1), mNumSamplesRecent(0), mNumSamplesContinuous(0), mSpursActive(0) {
    if (id == -1) {
        mNullMic = new MicNull();
    }
    mFonixIdx = -1;
    for (int i = 0; i < 6; i++) {
        if (!gIdxTaken[i]) {
            mFonixIdx = i;
            gIdxTaken[i] = true;
            break;
        }
    }
    MILO_ASSERT(mFonixIdx != -1, 0x5D);
    mWriteWav = DataVariable("do_record").Int();
    SetInputFile(nullptr);
    mLastEnergy = 0;
    mEnergy = 0;
    mLastPitch = -1;
    mPitch = -1;
    if (mWriteWav) {
        mStoredAudio = new MemStream();
        mStoredAudio->Reserve(0x1c00000);
    }
    memset(mSamplesRecent, 0, sizeof(mSamplesRecent));
    memset(mSamplesContinuous, 0, sizeof(mSamplesContinuous));
}

GameMic::~GameMic() {
    if (mWriteWav) {
        WriteWav(
            "mic_output_fonix.wav",
            16000,
            mStoredAudio->Buffer(),
            mStoredAudio->BufferSize()
        );
    }
    RELEASE(mStoredAudio);
    mPlaybackSampleRate = 0;
    gIdxTaken[mFonixIdx] = false;
    delete mDetector;
}

void GameMic::SetEnablePitchDetection(bool enable) {
    if (mDetector) {
        mDetector->mEnablePitchDetection = enable;
    }
}

Mic *GameMic::GetMyMic() {
    if (mNullMic) {
        return mNullMic;
    }
    return TheSynth->GetMic(mMicID);
}

int GameMic::GetDataSampleRate() {
    if (mPlaybackSampleRate) {
        return mPlaybackSampleRate;
    }
    return GetMyMic()->GetSampleRate();
}

void GameMic::SetInputFile(const char *filename) {
    MemStream * &_ref0 = mStoredAudio;
    int sampleRate;
    if (nullptr == filename) {
        auto _tmp0 = GetMyMic()->GetSampleRate();
        sampleRate = _tmp0;
        mPlaybackSampleRate = 0;
    } else {
        mWriteWav = false;
        FileStream fs(filename, FileStream::kRead, true);
        WaveFile wav(fs);
        mPlaybackSampleRate = wav.mSamplesPerSec;
        WaveFileData data(wav);
        if (!_ref0) {
            _ref0 = new MemStream();
        }
        _ref0->Resize(
            (int)(wav.mNumChannels * wav.mNumSamples * wav.mBitsPerSample) / 8
        );
        data.Read(
            _ref0->mBuffer.begin(),
            (int)(wav.mNumChannels * wav.mNumSamples * wav.mBitsPerSample) / 8
        );
        sampleRate = mPlaybackSampleRate;
    }
    delete mDetector;
    mDetector = nullptr;
    mDetector = new PitchDetector(sampleRate);
}

void GameMic::AccessContinuousSamples(const short *&s, int &i) const {
    s = mSamplesContinuous;
    i = mNumSamplesContinuous;
}

void GameMic::ThreadProcessOneFrame() {
    float livePitch = 0.0f;
    float energy = 0.0f;
    TheTaskMgr.Seconds(TaskMgr::kRealTime);
    clock();
    int droppedSamples = 0;
    if (!mPlaybackSampleRate) {
        Mic *mic = GetMyMic();
        droppedSamples = mic->GetDroppedSamples();
        char *recentBuf = mic->GetRecentBuf(mNumSamplesRecent);
        MinEq(mNumSamplesRecent, 8192);
        memcpy(mSamplesRecent, recentBuf, mNumSamplesRecent * 2);
        char *continuousBuf = mic->GetContinuousBuf(mNumSamplesContinuous);
        MinEq(mNumSamplesContinuous, 8192);
        memcpy(mSamplesContinuous, continuousBuf, mNumSamplesContinuous * 2);
    }
    Mic *myMic = GetMyMic();
    float outc = 0.0f;
    float micGain = myMic->unk8;
    const char *micName = myMic->GetName().mStr;
    AnalyzeBlock__13PitchDetectorFPCcPsiffRfRfRf(
        mDetector,
        micName,
        mSamplesRecent,
        mNumSamplesRecent,
        myMic->GetSensitivity(),
        micGain,
        livePitch,
        energy,
        outc
    );
    energy = Clamp(0.0f, 1.0f, energy / (mMicVolumeClamp * 500.0f));
    float rate = (energy > mEnergy) ? 0.3f : 0.1f;
    mEnergy = rate * energy + (1.0f - rate) * mEnergy;
    mPitch = livePitch;
    if (mWriteWav && TheTaskMgr.Seconds(TaskMgr::kRealTime) >= 0.0f) {
        if ((unsigned int)(droppedSamples - 1) <= 0xbb7e) {
            short *zeros = new short[droppedSamples];
            memset(zeros, 0, droppedSamples * 2);
            mStoredAudio->Write(zeros, droppedSamples * 2);
            delete[] zeros;
        }
        mStoredAudio->Write(mSamplesContinuous, mNumSamplesContinuous * 2);
    }
    if (!mPlaybackSampleRate && GetMyMic()->IsConnected() == 0) {
        mPitch = 0.0f;
        mEnergy = 0.0f;
    }
}

void GameMic::Update() {
    START_AUTO_TIMER("fonix_update");
    ThreadProcessOneFrame();
    if (mPlaybackSampleRate) {
        float sampleRate = GetDataSampleRate();
        int maxSamples;
        int desired = (int)(TheTaskMgr.Seconds(TaskMgr::kRealTime) * sampleRate);
        maxSamples = mStoredAudio->Size() / 2;
        int tellSamples = mStoredAudio->Tell() / 2;
        if (desired < tellSamples) {
            desired = tellSamples;
        } else if (desired > maxSamples) {
            desired = maxSamples;
        }
        mNumSamplesContinuous = desired - ((unsigned int)mStoredAudio->Tell() >> 1);
        MinEq(mNumSamplesContinuous, 8192);
        mStoredAudio->Read(mSamplesContinuous, mNumSamplesContinuous * 2);
        for (int i = 0; i < mNumSamplesContinuous; i++) {
            mSamplesContinuous[i] =
                (mSamplesContinuous[i] << 8) | ((mSamplesContinuous[i] >> 8) & 0xFF);
        }
    }
    mLastEnergy = mEnergy;
    mLastPitch = mPitch;
    mUSB = GetMyMic()->GetType() != 1;
}