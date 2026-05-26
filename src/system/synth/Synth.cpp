#include "synth/Synth.h"
#include "rndobj/Rnd.h"
#include "utl/Loader.h"
#include "utl/MakeString.h"
#include "math/Decibels.h"
#include "synth/Faders.h"
#include "synth/Sfx.h"
#include "synth/MidiInstrument.h"
#include "synth/SynthSample.h"
#include "synth/Sequence.h"
#include "synth/Emitter.h"
#include "synth/FxSendReverb.h"
#include "synth/FxSendDelay.h"
#include "synth/FxSendDistortion.h"
#include "synth/FxSendCompress.h"
#include "synth/FxSendEQ.h"
#include "synth/FxSendFlanger.h"
#include "synth/FxSendChorus.h"
#include "synth/FxSendMeterEffect.h"
#include "synth/FxSendPitchShift.h"
#include "synth/FxSendSynapse.h"
#include "synth/FxSendWah.h"
#include "synth/MoggClip.h"
#include "synth/BinkClip.h"
#include "synth/StreamNull.h"
#include "obj/DataFunc.h"
#include "obj/DataFile.h"
#include "os/BufFile.h"
#include "utl/Symbols.h"
#include "KeyChain.h"

MicClientID sNullClientID(-1, -1);

namespace {
    struct DebugGraph {
        DebugGraph(float r, float g, float b, float a) {
            unk0.resize(200);
            unk8 = 0;
            unkc.red = r;
            unkc.green = g;
            unkc.blue = b;
            unkc.alpha = a;
        }

        std::vector<float> unk0;
        int unk8;
        Hmx::Color unkc;
    };

    std::vector<DebugGraph> gDebugGraphs;
}

Synth *TheSynth;

static unsigned char sMasterKeyStr[] = { 0x7a, 0x4d, 0x60, 0x7c, 0xFF };

DataNode returnMasterKey(DataArray *a) {
    unsigned char str[8];
    unsigned char masher[64];
    if (a->Size() > 1) {
        KeyChain::getMasher(masher);
        unsigned char *p = sMasterKeyStr;
        unsigned int word = *((unsigned int *&)p)++;
        *(unsigned int *)(&str[0]) = word;
        str[4] = *p;
        for (int i = 0; i < 5; i++) {
            str[i]++;
        }
        DataArray *data = DataReadString((char *)str);
        int i2 = data->Evaluate(0).Int();
        data->Release();
        int i3 = a->Int(1);
        memcpy((void *)(i2 ^ i3), masher, 0x40);
    }
    return 0;
}

Synth::Synth()
    : mMuted(0), mMicClientMapper(0), mMidiInstrumentMgr(0), unk60(0), unk64(0) {
    SetName("synth", ObjectDir::sMainDir);
    DataArray *cfg = SystemConfig("synth");
    cfg->FindData("mics", mNumMics, true);
    mMidiSynth = new MidiSynth();
    gDebugGraphs.push_back(DebugGraph(1, 0, 0, 1));
    gDebugGraphs.push_back(DebugGraph(0, 1, 0, 1));
    gDebugGraphs.push_back(DebugGraph(1, 1, 0, 1));
    gDebugGraphs.push_back(DebugGraph(1, 1, 1, 1));
    mMicClientMapper = new MicClientMapper();
    mMidiInstrumentMgr = new MidiInstrumentMgr();
    MILO_ASSERT(!TheSynth, 0xBB);
}

Loader *WavFactory(const FilePath &fp, LoaderPos pos) {
    CacheResourceResult res;
    return new FileLoader(fp, CacheWav(fp.c_str(), res), pos, 0, false, true, nullptr);
}

void Synth::Init() {
    Fader::Init();
    Sfx::Init();
    MidiInstrument::Init();
    SynthSample::Init();
    Sequence::Init();
    SynthEmitter::Init();
    FxSendReverb::Init();
    FxSendDelay::Init();
    FxSendDistortion::Init();
    FxSendCompress::Init();
    FxSendEQ::Init();
    FxSendFlanger::Init();
    FxSendChorus::Init();
    FxSendMeterEffect::Init();
    FxSendPitchShift::Init();
    FxSendSynapse::Init();
    FxSendWah::Init();
    MoggClip::Init();
    BinkClip::Init();
    mMasterFader = Hmx::Object::New<Fader>();
    mSfxFader = Hmx::Object::New<Fader>();
    mMidiInstrumentFader = Hmx::Object::New<Fader>();
    DataArray *cfg = SystemConfig("synth");
    mMuted = cfg->FindInt("mute");
    TheLoadMgr.RegisterFactory("wav", WavFactory);
    mNullMics.resize(mNumMics);
    for (int i = 0; i < mNullMics.size(); i++) {
        mNullMics[i] = new MicNull();
    }
    mHud = RndOverlay::Find("synth_hud", true);
    mHud->SetCallback(this);
    InitSecurity();
    mMidiInstrumentMgr->Init();
}

DECOMP_FORCEACTIVE(Synth, "TheSynth != NULL", "use_null_synth")

void Synth::InitSecurity() {
    char buf[256];
    buf[1] = 0;
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 4; j++) {
            buf[0] = j + ('A' + i * 4);
            DataRegisterFunc(buf, returnMasterKey);
        }
    }
    buf[0] = 'M';
    DataRegisterFunc(buf, returnMasterKey);
    mGrinder.Init();
}

void Synth::Terminate() {
    DeleteAll(mNullMics);
    RELEASE(mMidiSynth);
    RELEASE(mMasterFader);
    RELEASE(mSfxFader);
    RELEASE(mMidiInstrumentFader);
    RELEASE(mMicClientMapper);
    RELEASE(mMidiInstrumentMgr);
}

void Synth::SetMasterVolume(float vol) { mMasterFader->SetVal(vol); }

float Synth::GetMasterVolume() { return mMasterFader->GetVal(); }

void Synth::Poll() {
    for (int i = 0; i < mLevelData.size(); i++) {
        LevelData &data = mLevelData[i];
        if ((data.mPeak > data.mPeakHold) || ++data.mPeakAge >= 0x3C) {
            data.mPeakHold = data.mPeak;
            data.mPeakAge = 0;
        }
    }
    FaderTask::PollAll();
    if (mMuted)
        mMasterFader->SetVal(-96.0f);
    SynthPollable::PollAll();
    mMidiInstrumentMgr->Poll();
    if (DidMicsChange()) {
        MILO_ASSERT(mMicClientMapper, 0x14B);
        mMicClientMapper->HandleMicsChanged();
        ResetMicsChanged();
    }
}

void Synth::SetFX(const DataArray *data) {
    MILO_ASSERT(data, 0x15F);
    int chainData = data->FindArray("chain")->Int(1);
    SetFXChain(chainData);
    for (int i = 0; i < 2; i++) {
        DataArray *coreArr = data->FindArray(MakeString("core_%i", i), true);
        int mode = coreArr->FindArray("mode")->Int(1);
        float volume = coreArr->FindArray("volume")->Float(1);
        float delay = coreArr->FindArray("delay")->Float(1);
        float feedback = coreArr->FindArray("feedback")->Float(1);
        SetFXMode(i, (FXMode)mode);
        SetFXVolume(i, volume);
        SetFXDelay(i, delay);
        SetFXFeedback(i, feedback);
    }
}

// holy deadstrip
DECOMP_FORCEACTIVE(
    Synth,
    "transpose >= -0x2000 && transpose < 0x2000",
    "name",
    "Sequence%i",
    "random_group_seq",
    "num_seqs",
    "serial_group_seq",
    "parallel_group_seq",
    "sfx_seq",
    "sfx",
    "couldn't find sfx %s",
    "pan",
    "transpose",
    "wait_seq",
    "secs",
    "vol_mod_seq",
    "pan_mod_seq",
    "transpose_mod_seq",
    "loop_mod_seq",
    "wrapped",
    "can't load sequence of type %s",
    "children",
    "Couldn't read bank: %s",
    "sample"
)

int Synth::GetNumMics() const { return mNumMics; }

void Synth::StopPlaybackAllMics() {
    MicManagerInterface *micInterface = mMicClientMapper->mMicManager;
    if (micInterface) {
        micInterface->SetPlayback(false);
    }
}

void Synth::SetMic(const DataArray *data) {
    for (int i = 0; i < mNumMics; i++) {
        Mic *mic = GetMic(i);
        if (mic)
            mic->Set(data);
    }
    SetMicFX(data->FindArray("fx")->Int(1));
    SetMicVolume(data->FindArray("volume")->Float(1));
}

DECOMP_FORCEACTIVE(Synth, "adsr", "loops", "loop not set for %s", "sequences")

void SynthPreInit() {
    MILO_ASSERT(!TheSynth, 0x2EA);
    DataArray *cfg = SystemConfig("synth");
    bool useNullSynth = cfg->FindArray("use_null_synth")->Int(1);
    if (useNullSynth)
        TheSynth = new Synth();
    else
        TheSynth = Synth::New();
    if (TheSynth->Fail()) {
        RELEASE(TheSynth);
        TheSynth = new Synth();
    }
    TheSynth->PreInit();
}

void SynthInit() {
    if (!TheSynth)
        SynthPreInit();
    DataArray *cfg = SystemConfig("synth");
    TheSynth->Init();
    TheSynth->SetMic(cfg->FindArray("mic"));
    TheSynth->SetFX(cfg->FindArray("fx"));
    TheDebug.AddExitCallback(SynthTerminate);
}

void SynthTerminate() {
    TheSynth->Poll();
    TheDebug.RemoveExitCallback(SynthTerminate);
    TheSynth->Terminate();
    RELEASE(TheSynth);
}

void Synth::ToggleHud() {
    mHud->SetShowing(!mHud->Showing());
    EnableLevels(mHud->Showing());
}

DECOMP_FORCEACTIVE(Synth, "%i", "0", "stream", "chan %i", "Total active Sequences: %d")

void Synth::DrawMeter(float &y, float level, float peakHold, const char *name) {
    Hmx::Color grey(0.5f, 0.5f, 0.5f, 1.0f);
    Hmx::Color black(0.0f, 0.0f, 0.0f, 1.0f);
    Hmx::Color white(1.0f, 1.0f, 1.0f, 1.0f);
    Hmx::Color green(0.5f, 1.0f, 0.0f, 1.0f);
    Hmx::Color red(1.0f, 0.5f, 0.5f, 1.0f);

    Vector2 labelPos((float)TheRnd->Width() * 0.1f, y);
    TheRnd->DrawString(name, labelPos, white, true);

    float barLeft = (float)TheRnd->Width() * 0.2f;
    float barWidth = (float)TheRnd->Width() * 0.7f;
    Hmx::Rect bgRect(barLeft, y, barWidth, 12.0f);
    TheRnd->DrawRect(bgRect, black, 0, 0, 0);

    static float levelMin = 0.0f;
    static float levelMax = 1.0f;
    float levelNorm = (level + 40.0f) / 40.0f;
    if (levelNorm < levelMin) {
        levelNorm = levelMin;
    } else if (levelNorm > levelMax) {
        levelNorm = levelMax;
    }
    Hmx::Rect levelRect(barLeft, y, levelNorm * barWidth, 12.0f);
    TheRnd->DrawRect(levelRect, grey, 0, 0, 0);

    static float peakMin = 0.0f;
    static float peakMax = 1.0f;
    float peakNorm = (peakHold + 40.0f) / 40.0f;
    if (peakNorm < peakMin) {
        peakNorm = peakMin;
    } else if (peakNorm > peakMax) {
        peakNorm = peakMax;
    }
    Hmx::Color *peakColor = &green;
    if (peakNorm == 1.0f) {
        peakColor = &red;
    }
    Hmx::Rect peakRect(barLeft + peakNorm * barWidth, y, 8.0f, 12.0f);
    TheRnd->DrawRect(peakRect, *peakColor, 0, 0, 0);

    Hmx::Color white2(1.0f, 1.0f, 1.0f, 1.0f);
    Vector2 dbLabelPos(barLeft + barWidth, y);
    TheRnd->DrawString(MakeString("%i", (int)peakHold), dbLabelPos, white2, true);

    y += 16.0f;
}

void Synth::DrawMeterScale(float &y) {
    float db = -40.0f;
    float height = (float)TheRnd->Width();
    Hmx::Color color(1.0f, 1.0f, 1.0f, 1.0f);
    float left = height * 0.2f;
    float width = height * 0.7f;
    Vector2 pos(left, y);
    TheRnd->DrawString(MakeString("%i", (int)db), pos, color, true);
    db *= 0.5f;
    Vector2 pos2(left + width * 0.5f, y);
    TheRnd->DrawString(MakeString("%i", (int)db), pos2, color, true);
    Vector2 pos3(left + width, y);
    TheRnd->DrawString("0", pos3, color, true);
    y += 16.0f;
}

float Synth::UpdateOverlay(RndOverlay *, float y) {
    float yPos = y;
    yPos += 0.265f;
    Hmx::Color white(1.0f, 1.0f, 1.0f, 1.0f);
    yPos = (float)TheRnd->Height() * yPos;
    if (unk64) {
        DrawMeterScale(yPos);
        DrawMeter(yPos, ((Stream *)unk64)->Faders()->GetVal(), 0.0f, "stream");
        for (int i = 0; i < ((Stream *)unk64)->GetNumChannels(); i++) {
            DrawMeter(
                yPos,
                ((Stream *)unk64)->ChannelFaders(i)->GetVal(),
                0.0f,
                MakeString("chan %i", i)
            );
        }
    }
    if (!mLevelData.empty()) {
        DrawMeterScale(yPos);
    }
    for (int i = 0; i < mLevelData.size(); i++) {
        float rms = RatioToDb(mLevelData[i].mRMS);
        float peakHold = RatioToDb(mLevelData[i].mPeakHold);
        if (rms > 2.0f) {
            rms = -30.0f;
        }
        DrawMeter(yPos, rms, peakHold, mLevelData[i].mName.c_str());
    }
    char buf[64];
    int count = 0;
    std::list<SynthPollable *>::iterator it = SynthPollable::sPollables.begin();
    for (std::list<SynthPollable *>::iterator it2 = it;
         it2 != SynthPollable::sPollables.end();
         ++it2) {
        ++count;
    }
    sprintf(buf, "Total active Sequences: %d", count);
    TheRnd->DrawString(buf, Vector2(100, yPos), white, true);
    yPos += 12.0f;
    for (; it != SynthPollable::sPollables.end(); ++it) {
        const char *name = (*it)->GetSoundDisplayName();
        if (*name != '\0') {
            TheRnd->DrawString(name, Vector2(100, yPos), white, true);
            yPos += 12.0f;
        }
    }
    return yPos / (float)TheRnd->Height();
}

DataNode Synth::OnStartMic(const DataArray *a) {
    GetMic(a->Int(2))->Start();
    return 0;
}

DataNode Synth::OnStopMic(const DataArray *a) {
    GetMic(a->Int(2))->Stop();
    return 0;
}

DataNode Synth::OnNumConnectedMics(const DataArray *) { return GetNumConnectedMics(); }

DataNode Synth::OnSetMicVolume(const DataArray *a) {
    SetMicVolume(a->Float(2));
    return 0;
}

DataNode Synth::OnSetFX(const DataArray *a) {
    SetFX(a->Array(2));
    return 0;
}

DataNode Synth::OnSetFXVol(const DataArray *a) {
    SetFXVolume(a->Int(2), a->Float(3));
    return 0;
}

void Synth::StopAllSfx(bool b) {
    const std::list<SynthPollable *> &polls = SynthPollable::sPollables;
    for (std::list<SynthPollable *>::const_iterator it = polls.begin(); it != polls.end();
         ++it) {
        Sequence *seq = dynamic_cast<Sequence *>(*it);
        if (seq)
            seq->Stop(b);
    }
}

void Synth::PauseAllSfx(bool b) {
    const std::list<SynthPollable *> &polls = SynthPollable::sPollables;
    for (std::list<SynthPollable *>::const_iterator it = polls.begin(); it != polls.end();
         ++it) {
        Sfx *sfx = dynamic_cast<Sfx *>(*it);
        if (sfx)
            sfx->Pause(b);
    }
}

DataNode Synth::OnPassthrough(DataArray *a) {
    if (!CheckCommonBank(false))
        return 0;
    else {
        const char *str = a->Str(2);
        Hmx::Object *obj = Find<Hmx::Object>(str, false);
        if (obj)
            obj->Handle(a, true);
        else
            MILO_WARN(
                "Synth::OnPassthrough() - %s not found in %s", str, unk40->GetPathName()
            );
        return 0;
    }
}

void Synth::Play(const char *name, float f1, float f2, float f3) {
    if (CheckCommonBank(false)) {
        Sequence *seq = Find<Sequence>(name, false);
        if (seq)
            seq->Play(f1, f2, f3);
        else
            MILO_WARN(
                "Synth::Play() - Sequence %s not found in %s", name, unk40->GetPathName()
            );
    }
}

bool Synth::CheckCommonBank(bool notify) {
    bool ret = false;
    if (unk40 && unk40.IsLoaded())
        ret = true;
    if (!ret && notify) {
        MILO_LOG("Synth::Find() - Common sound bank not loaded!\n");
    }
    return ret;
}

#pragma push
#pragma dont_inline on
BEGIN_HANDLERS(Synth)
    HANDLE(play, OnPassthrough)
    HANDLE(stop, OnPassthrough)
    HANDLE(start_mic, OnStartMic)
    HANDLE(stop_mic, OnStopMic)
    HANDLE_ACTION(stop_playback_all_mics, StopPlaybackAllMics())
    HANDLE(num_connected_mics, OnNumConnectedMics)
    HANDLE_EXPR(did_mics_change, DidMicsChange())
    HANDLE_ACTION(reset_mics_changed, ResetMicsChanged())
    HANDLE(set_mic_volume, OnSetMicVolume)
    HANDLE(set_fx, OnSetFX)
    HANDLE(set_fx_vol, OnSetFXVol)
    HANDLE_ACTION(stop_all_sfx, StopAllSfx(_msg->Size() == 3 ? _msg->Int(2) : false))
    HANDLE_ACTION(pause_all_sfx, PauseAllSfx(_msg->Int(2)))
    HANDLE_EXPR(master_vol, GetMasterVolume())
    HANDLE_ACTION(set_master_vol, SetMasterVolume(_msg->Float(2)))
    HANDLE_EXPR(find, Find<Hmx::Object>(_msg->Str(2), true))
    HANDLE_ACTION(toggle_hud, ToggleHud())
    HANDLE_EXPR(
        get_sample_mem, GetSampleMem(_msg->Obj<ObjectDir>(2), (Platform)_msg->Int(3))
    )
    HANDLE_EXPR(spu_overhead, GetSPUOverhead())
    HANDLE_ACTION(set_headset_target, 0)
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0x446)
END_HANDLERS
#pragma pop

Stream *Synth::NewStream(const char *, float f1, float, bool) {
    return new StreamNull(f1);
}

Stream *Synth::NewBufStream(const void *, int, Symbol, float f1, bool) {
    return new StreamNull(f1);
}

void Synth::NewStreamFile(const char *cc, File *&file, Symbol &sym) {
    static char gFakeFile[16];
    file = new BufFile(gFakeFile, sizeof(gFakeFile));
    sym = "fake";
}

int Synth::GetSampleMem(ObjectDir *dir, Platform plat) {
    int size = 0;
    for (ObjDirItr<SynthSample> it(dir, true); it != nullptr; ++it) {
        size += it->GetPlatformSize(plat);
    }
    return size;
}

int Synth::GetFXOverhead() {
    int overheads[10] = { 0x80,   0x26c0, 8000,    0x4c28,  0x6fe0,
                          0xade0, 0xf6c0, 0x18040, 0x18040, 0x3c00 };
    DataArray *cfg = SystemConfig("synth");
    int mode = cfg->FindArray("fx")->FindArray("core_0")->FindInt("mode");
    return overheads[mode] + 0x20000;
}

int Synth::GetSPUOverhead() {
    DataArray *cfg = SystemConfig("synth");
    int spuBufs = cfg->FindArray("iop")->FindInt("spu_buffers");
    spuBufs *= 0x800;
    spuBufs += 0x5010;
    return spuBufs + GetFXOverhead();
}

FxSendPitchShift *Synth::CreatePitchShift(int stage, SendChannels chans) {
    FxSendPitchShift *fx = Hmx::Object::New<FxSendPitchShift>();
    fx->SetStage(stage);
    fx->SetChannels(chans);
    return fx;
}

void Synth::DestroyPitchShift(FxSendPitchShift *fx) { delete fx; }