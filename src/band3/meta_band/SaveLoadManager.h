#pragma once
#include "game/BandUser.h"
#include "meta/MemcardAction.h"
#include "meta/Profile.h"
#include "net_band/RockCentralMsgs.h"
#include "obj/Msg.h"
#include "os/Memcard.h"
#include "os/Timer.h"
#include "utl/Cache.h"
#include "utl/Str.h"
#include <vector>

class BandProfile;

enum SaveLoadMgrStatus {
    kSaveLoadMgrStatus0,
    kSaveLoadMgrStatus_Saving,
    kSaveLoadMgrStatus_Loading,
    kSaveLoadMgrStatus_Start,
    kSaveLoadMgrStatus_Finish,
};

class SaveLoadManager : public MsgSource {
public:
    enum SaveLoadMode {
        kMode_AutoLoad,
        kMode_AutoSave,
        kMode_DisableAutoSave,
        kMode_ManualDelete,
    };
    enum State {
        kS_Idle = 0,
        kS_Start = 1,
        kS_SaveOverwrite = 0x46,
        kS_SaveNoOverwrite = 0x47,
        kS_SaveNotEnoughSpacePS3 = 0x4A,
        kS_ManualLoadInit = 0x5A,
        kS_ManualLoadConfirm = 0x60,
        kS_Finish = 0x6F,
    };

    SaveLoadManager();
    virtual ~SaveLoadManager();
    void AutoSave();
    void AutoLoad();
    void EnableAutosave(LocalBandUser *);
    void DisableAutosave(LocalBandUser *);
    void ManualSave(LocalBandUser *);
    void ManualDelete();
    bool IsAutosaveEnabled(LocalBandUser *);
    DataNode GetDialogMsg();
    Symbol GetDialogOpt1();
    Symbol GetDialogOpt2();
    Symbol GetDialogOpt3();
    int GetDialogFocusOption();
    bool IsInitialLoadDone() const;
    bool IsIdle();
    void Activate();
    void PrintoutSaveSizeInfo();
    void Poll();
    void AutoSaveNow();
    virtual DataNode Handle(DataArray *, bool);
    void HandleEventResponseStart(int i1);
    void HandleEventResponse(LocalUser *, int i1);

    static void Init();

    DataNode OnMsg(const ProfileSwappedMsg &);
    DataNode OnMsg(const DeviceChosenMsg &);
    DataNode OnMsg(const NoDeviceChosenMsg &);
    DataNode OnMsg(const MCResultMsg &);
    DataNode OnMsg(const RockCentralOpCompleteMsg &);
    DataNode OnMsg(const SigninChangedMsg &);

protected:
    BandProfile *GetProfile();
    BandProfile *GetAutosavableProfile();
    BandProfile *GetNewSigninProfile();
    void SetState(State);
    void UpdateStatus(SaveLoadMgrStatus);
    void Start();
    void Finish();
    void SaveLoadErrorSetState();
    void StartSaveAction(bool);
    bool IsReasonToAutosave(bool);
    bool IsReasonToAutoload();
    bool IsReasonToUpload();

    bool mActivated; // 0x1c
    bool mInitialLoadNotDone; // 0x1d
    int mMode; // 0x20
    State mState; // 0x24
    State mStateAtSelectStart; // 0x28
    LocalBandUser *mUser; // 0x2c
    LocalUser *mLocalUser; // 0x30
    std::vector<Profile *, unsigned short> mUploadProfiles; // 0x34
    std::vector<Profile *, unsigned short> mSaveProfiles; // 0x3c
    DataArray *unk44; // 0x44
    int unk48; // 0x48
    String unk4c; // 0x4c
    CacheID *mCacheID; // 0x58
    Cache *mCache; // 0x5c
    void *mData; // 0x60
    int unk64; // 0x64
    bool unk68; // 0x68
    bool unk69; // 0x69
    int unk6c; // 0x6c
    int unk70; // 0x70
    int mRequestFlags; // 0x74
    int unk78; // 0x78
    int unk7c; // 0x7c
    MemcardAction *mAction; // 0x80
    int unk84; // 0x84
    Timer mTimer; // 0x88
};

extern SaveLoadManager *TheSaveLoadMgr;

DECLARE_MESSAGE(SaveLoadMgrStatusUpdateMsg, "saveloadmgr_status_update_msg")
END_MESSAGE
