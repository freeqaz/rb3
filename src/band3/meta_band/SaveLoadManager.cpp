#include "SaveLoadManager.h"
#include "game/BandUser.h"
#include "meta/Profile.h"
#include "meta_band/ProfileMgr.h"
#include "net_band/RockCentralMsgs.h"
#include "obj/Data.h"
#include "obj/MessageTimer.h"
#include "obj/ObjMacros.h"
#include "meta/MemcardMgr_Wii.h"
#include "meta/WiiProfileMgr.h"
#include "meta_band/BandSongMgr.h"
#include "net_band/EntityUploader.h"
#include "os/Debug.h"
#include "os/Memcard.h"
#include "os/User.h"
#include "utl/MemMgr.h"
#include "utl/Symbols2.h"
#include "utl/Symbols3.h"
#include "utl/Symbols4.h"

SaveLoadManager *TheSaveLoadMgr;

bool SaveLoadManager::IsInitialLoadDone() const { return !mInitialLoadNotDone; }

bool SaveLoadManager::IsIdle() {
    bool idle = false;
    if (mState == kS_Idle && mRequestFlags == 0) {
        idle = true;
    }
    return idle;
}

int SaveLoadManager::GetDialogFocusOption() {
    int ret = 1;
    if (mState == kS_ManualLoadConfirm) {
        ret = 2;
    }
    return ret;
}

void SaveLoadManager::Activate() {
    if (!mActivated) {
        mActivated = true;
        mRequestFlags |= 2;
    }
}

void SaveLoadManager::HandleEventResponseStart(int) { mStateAtSelectStart = mState; }

void SaveLoadManager::Start() {
    mUser = NULL;
    mLocalUser = NULL;
    SetState(kS_Start);
    if (mMode == kMode_AutoLoad) {
        UpdateStatus(kSaveLoadMgrStatus_Start);
    }
}

void SaveLoadManager::Finish() {
    if (mMode == kMode_AutoLoad) {
        UpdateStatus(kSaveLoadMgrStatus_Finish);
    }
    SetState(kS_Finish);
}

void SaveLoadManager::AutoSave() {
    if (IsReasonToAutosave(false)) {
        mRequestFlags |= 4;
        UpdateStatus(kSaveLoadMgrStatus_Saving);
    }
}

void SaveLoadManager::AutoLoad() {
    if (IsReasonToAutoload()) {
        mRequestFlags |= 2;
    }
}

void SaveLoadManager::ManualDelete() {
    MILO_LOG("Manual Delete has been called\n");
    mRequestFlags |= 1;
}

void SaveLoadManager::Init() {
    MILO_ASSERT(!TheSaveLoadMgr, 0x57);
    TheSaveLoadMgr = new SaveLoadManager();
}

void SaveLoadManager::UpdateStatus(SaveLoadMgrStatus status) {
    static SaveLoadMgrStatusUpdateMsg msg(-1);
    msg[0] = (int)status;
    Export(msg, true);
}

bool SaveLoadManager::IsReasonToAutosave(bool fromAutoSaveNow) {
    bool songCache = false;
    if (TheSongMgr.SongCacheNeedsWrite() && !unk68) {
        songCache = true;
    }
    if (songCache) {
        return true;
    }
    if (TheMemcardMgr.IsDisableWriting()) {
        return false;
    }
    if (GetAutosavableProfile()) {
        return true;
    }
    if (IsReasonToUpload() && !fromAutoSaveNow) {
        return true;
    }
    if (TheProfileMgr.GlobalOptionsNeedsSave()) {
        return true;
    }
    return TheWiiProfileMgr.NeedsSave();
}

void SaveLoadManager::AutoSaveNow() {
    if (IsReasonToAutosave(true)) {
        int i = 0x20;
        mRequestFlags |= 8;
        TheEntityUploader.Abort();
        do {
            TheEntityUploader.Poll();
            Poll();
            i--;
        } while (mState != kS_Idle && i > 0);
    }
}

Symbol SaveLoadManager::GetDialogOpt3() {
    Symbol sym(gNullStr);
    if (mState == kS_SaveNotEnoughSpacePS3) {
        sym = mc_button_continue_no_save;
    }
    return sym;
}

BandProfile *SaveLoadManager::GetProfile() {
    return TheProfileMgr.GetProfileForUser(mUser);
}

bool SaveLoadManager::IsReasonToAutoload() {
    if (TheMemcardMgr.IsDisableWriting()) {
        return false;
    }
    bool reason = false;
    if (GetNewSigninProfile() || mInitialLoadNotDone) {
        reason = true;
    }
    return reason;
}

bool SaveLoadManager::IsAutosaveEnabled(LocalBandUser *user) {
    Profile *profile = TheProfileMgr.GetProfileForUser(user);
    if (!profile) {
        MILO_WARN("Tried to get autosave enabled status without a valid profile.\n");
        return false;
    }
    return profile->IsAutosaveEnabled();
}

void SaveLoadManager::EnableAutosave(LocalBandUser *user) {
    Profile *profile = TheProfileMgr.GetProfileForUser(user);
    if (!profile) {
        MILO_WARN("Tried to enable autosave without a valid profile.\n");
        return;
    }
    TheMemcardMgr.DisableWriting(false);
    profile->SetSaveState(kMetaProfileLoaded);
    ManualSave(user);
}

void SaveLoadManager::DisableAutosave(LocalBandUser *user) {
    Profile *profile = TheProfileMgr.GetProfileForUser(user);
    if (!profile) {
        MILO_WARN("Tried to disable autosave without a valid profile.\n");
        return;
    }
    bool idle = false;
    if (mState == kS_Idle && mRequestFlags == 0) {
        idle = true;
    }
    if (!idle) {
        MILO_WARN("Tried to disable autosave while saveloadmgr is not idle.\n");
        return;
    }
    profile->SetSaveState(kMetaProfileError);
}

void SaveLoadManager::ManualSave(LocalBandUser *user) {
    if (mState != kS_Idle) {
        MILO_WARN(
            "Attempted to perform a manual save, but saveloadmgr is not idle (state = %d).\n",
            mState
        );
        return;
    }
    mUser = user;
    mLocalUser = user;
    TheMemcardMgr.AddSink(this);
    SetState(kS_ManualLoadInit);
}

#pragma push
#pragma dont_inline on
BEGIN_HANDLERS(SaveLoadManager)
    HANDLE_ACTION(autosave, AutoSave())
    HANDLE_ACTION(autoload, AutoLoad())
    HANDLE_ACTION(delete_saves, ManualDelete())
    HANDLE_ACTION(manual_save, ManualSave(_msg->Obj<LocalBandUser>(2)))
    HANDLE_EXPR(is_autosave_enabled, IsAutosaveEnabled(_msg->Obj<LocalBandUser>(2)))
    HANDLE_ACTION(enable_autosave, EnableAutosave(_msg->Obj<LocalBandUser>(2)))
    HANDLE_ACTION(disable_autosave, DisableAutosave(_msg->Obj<LocalBandUser>(2)))
    HANDLE_ACTION(handle_eventresponse_start, HandleEventResponseStart(_msg->Int(2)))
    HANDLE_ACTION(
        handle_eventresponse, HandleEventResponse(_msg->Obj<LocalUser>(2), _msg->Int(3))
    )
    HANDLE_EXPR(get_dialog_msg, GetDialogMsg())
    HANDLE_EXPR(get_dialog_opt1, GetDialogOpt1())
    HANDLE_EXPR(get_dialog_opt2, GetDialogOpt2())
    HANDLE_EXPR(get_dialog_opt3, GetDialogOpt3())
    HANDLE_EXPR(get_dialog_focus_option, GetDialogFocusOption())
    HANDLE_EXPR(is_initial_load_done, IsInitialLoadDone())
    HANDLE_EXPR(is_idle, IsIdle())
    HANDLE_ACTION(activate, Activate())
    HANDLE_ACTION(printout_savesize_info, PrintoutSaveSizeInfo())
    HANDLE_MESSAGE(ProfileSwappedMsg)
    HANDLE_MESSAGE(DeviceChosenMsg)
    HANDLE_MESSAGE(NoDeviceChosenMsg)
    HANDLE_MESSAGE(MCResultMsg)
    HANDLE_MESSAGE(RockCentralOpCompleteMsg)
    HANDLE_MESSAGE(SigninChangedMsg)
    HANDLE_SUPERCLASS(MsgSource)
    HANDLE_CHECK(0xF27)
END_HANDLERS
#pragma pop