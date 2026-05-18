#include "game/GameMic.h"
#include "obj/Data.h"
#include "os/Debug.h"
#include "synth/MicNull.h"
#include "synth/Synth.h"
#include "utl/FileStream.h"
#include "utl/MemStream.h"
#include "utl/Wav.h"
#include "utl/WaveFile.h"

bool gIdxTaken[6];

GameMic::GameMic(int id)
    : mMicID(id), unk8(1), unk9(1), mWriteWav(0), unkc(0), mStoredAudio(0), mDetector(0),
      mNullMic(0), unk24(1), unk8030(0), unk8034(0), unk8038(0) {
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
    unk28 = 0;
    unk1c = 0;
    unk2c = -1;
    unk20 = -1;
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
    unkc = 0;
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
    if (unkc) {
        return unkc;
    }
    return GetMyMic()->GetSampleRate();
}

void GameMic::SetInputFile(const char *filename) {
    int sampleRate;
    if (filename == nullptr) {
        sampleRate = GetMyMic()->GetSampleRate();
        unkc = 0;
    } else {
        mWriteWav = false;
        FileStream fs(filename, FileStream::kRead, true);
        WaveFile wav(fs);
        unkc = wav.mSamplesPerSec;
        WaveFileData data(wav);
        if (!mStoredAudio) {
            mStoredAudio = new MemStream();
        }
        mStoredAudio->Resize(
            (int)(wav.mNumChannels * wav.mNumSamples * wav.mBitsPerSample) / 8
        );
        data.Read(
            mStoredAudio->mBuffer.begin(),
            (int)(wav.mNumChannels * wav.mNumSamples * wav.mBitsPerSample) / 8
        );
        sampleRate = unkc;
    }
    delete mDetector;
    mDetector = nullptr;
    mDetector = new PitchDetector(sampleRate);
}

void GameMic::AccessContinuousSamples(const short *&s, int &i) const {
    s = mSamplesContinuous;
    i = unk8034;
}