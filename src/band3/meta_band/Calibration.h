#pragma once
#include "meta_band/InputMgr.h"
#include "obj/Data.h"
#include "os/UsbMidiKeyboardMsgs.h"
#include "rndobj/Mesh.h"
#include "synth/Faders.h"
#include "synth/Stream.h"
#include "ui/UIListProvider.h"
#include "ui/UIPanel.h"

enum TestState {
    tsIdle = 0,
    tsPreRoll = 1,
    tsTesting = 2,
    tsPostTest = 3
};

class CalibrationPanel : public UIPanel {
public:
    CalibrationPanel();
    OBJ_CLASSNAME(CalibrationPanel);
    OBJ_SET_TYPE(CalibrationPanel);
    virtual DataNode Handle(DataArray *, bool);
    virtual ~CalibrationPanel();
    virtual void Draw();
    virtual void Enter();
    virtual void Exit();
    virtual void Poll();
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);

    float GetAudioTimeMs() const;
    void SetTestState(TestState);
    void StopAudio();
    void UpdateAnimation();
    void UpdateLabel();
    void UpdateStream();
    int GetTestRep() const;
    void ScanHardwareModeInputs();
    float ReshapeTime(float);
    float HandlePreAndPostTestAnim(float);
    void UpdateProgress(bool);
    void InitializeVisuals();
    void StartAudio();
    void PrepareHwCalibrationState();
    void TerminateHwCalibrationState();
    void EndTest();
    void TriggerCalibration(int);
    int GetAverageTestTime();
    float GetSampleSpread() const;
    int GetTestQuality() const;

    DataNode OnInitializeContent(DataArray *);
    DataNode OnStartTest(DataArray *);
    DataNode OnMsg(const ButtonDownMsg &);
    DataNode OnMsg(const KeyboardKeyPressedMsg &);

    static float kAnimPerceptualOffset;
    NEW_OBJ(CalibrationPanel);
    static void Init() { REGISTER_OBJ_FACTORY(CalibrationPanel); }

    float mCycleTimeMs; // 0x38
    Stream *mStream; // 0x3c
    Fader *mFader; // 0x40
    bool mStartASAP; // 0x44
    std::vector<float> mTestSamples; // 0x48
    bool mHalfOffAnim; // 0x50
    bool mEnableVideo; // 0x51
    int mNumHits; // 0x54
    bool mEnableAudio; // 0x58
    float mLastStreamMs; // 0x5c
    TestState mTestState; // 0x60
    float mPrevFrame; // 0x64
    bool mHardwareMode; // 0x68
    float mAnimCycleFrames; // 0x6c
    int mAnimNumCycles; // 0x70
    int mMaxSlack; // 0x74
    float mRestingFrame; // 0x78
    int mStartRep; // 0x7c
    float mPostTestStartTime; // 0x80
    float mVolDb; // 0x84
    int mShowNumTimes; // 0x88
    float mAccel; // 0x8c
    int mTimeOutRep; // 0x90
    float mPrevLX; // 0x94
    int unk98; // 0x98 (DWARF: float mPrevLY; kept int, decomp-inferred type differs)
    int mPad; // 0x9c
    bool mAllowGreenButton; // 0xa0
    float mXV[5]; // 0xa4
    float mYV[5]; // 0xb8
    bool mVibrationEnabled; // 0xcc
    float mLastTime; // 0xd0
    float mSensorSigma; // 0xd4
    float mAveragePeak; // 0xd8
    int mAveragePeakCount; // 0xdc
    bool mAdams; // 0xe0
    float mSensorDeltaPeakFollow; // 0xe4
    int mTopOutliers; // 0xe8
    int mBottomOutliers; // 0xec
};

class CalibrationModesProvider : public UIListProvider, public Hmx::Object {
public:
    CalibrationModesProvider();
    virtual ~CalibrationModesProvider() {}
    virtual void Text(int, int, UIListLabel *, UILabel *) const;
    virtual RndMat *Mat(int, int, UIListMesh *) const;
    virtual int DataIndex(Symbol s) const;
    virtual int NumData() const;
    virtual void InitData(RndDir *);
    virtual DataNode Handle(DataArray *, bool);

    void Cleanup();
    Symbol GetCalibrationMode(int);

    std::vector<Symbol> mModes; // 0x20
    RndMat *mAutoCalibrateMat; // 0x28
    RndMat *mAutoCalibrateDisabledMat; // 0x2c
    RndMat *mManualCalibrateMat; // 0x30
    RndMat *mEnterNumbersMat; // 0x34
};

class CalibrationWelcomePanel : public UIPanel {
public:
    CalibrationWelcomePanel() {}
    OBJ_CLASSNAME(CalibrationWelcomePanel);
    OBJ_SET_TYPE(CalibrationWelcomePanel);
    virtual DataNode Handle(DataArray *, bool);
    virtual ~CalibrationWelcomePanel() {}
    virtual void Enter();
    virtual void Exit();

    DataNode OnMsg(const InputStatusChangedMsg &);

    static bool HaveCalbertConnected();
    NEW_OBJ(CalibrationWelcomePanel);
    static void Init() { REGISTER_OBJ_FACTORY(CalibrationWelcomePanel); }

    CalibrationModesProvider mModesProvider; // 0x38
};