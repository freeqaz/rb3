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
#include "os/PlatformMgr.h"
#include "os/User.h"
#include "utl/BufStream.h"
#include "utl/CacheMgr.h"
#include "utl/MemMgr.h"
#include "utl/Symbols2.h"
#include "utl/Symbols3.h"
#include "utl/Symbols4.h"

SaveLoadManager *TheSaveLoadMgr;

SaveLoadManager::SaveLoadManager()
    : mActivated(false), mInitialLoadNotDone(true), mMode(kMode_AutoLoad),
      mState(kS_Idle), mStateAtSelectStart(kS_Idle), mUser(NULL), mLocalUser(NULL),
      unk44(NULL), unk48(0), unk4c(), mCacheID(NULL), mCache(NULL), mData(NULL),
      unk64(0), unk68(false), unk69(false), unk6c(0), unk70(0), mRequestFlags(0),
      unk78(0), unk7c(0), mAction(NULL) {
    unk44 = new DataArray(0);
    mSaveProfiles.reserve(4);
    mUploadProfiles.reserve(4);
    SetName("saveload_mgr", ObjectDir::sMainDir);
    ThePlatformMgr.AddSink(this, SigninChangedMsg::Type());
}

SaveLoadManager::~SaveLoadManager() {
    ThePlatformMgr.RemoveSink(this, SigninChangedMsg::Type());
    unk4c.~String();
    RELEASE(unk44);
}

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

void SaveLoadManager::Poll() {
    if (!mActivated) return;
    if (mState == kS_Idle) {
        int flags = mRequestFlags;
        if (flags & 8) {
            mMode = kMode_ManualLoad;
            Start();
            mRequestFlags &= ~8;
            return;
        }
        if (flags & 1) {
            mMode = kMode_ManualDelete;
            Start();
            mRequestFlags &= ~1;
            return;
        }
        if (flags & 4) {
            mMode = kMode_AutoSave;
            Start();
            mRequestFlags &= ~4;
            return;
        }
        if (flags & 2) {
            if (IsReasonToAutosave(true)) {
                mMode = kMode_AutoSave;
                Start();
                return;
            }
            mMode = kMode_AutoLoad;
            Start();
            mRequestFlags &= ~2;
            return;
        }
        TheProfileMgr.PurgeOldData();
        AutoLoad();
        return;
    }
    if ((unsigned int)mState > 0x6f) return;
    switch (mState) {
    case kS_Start:
        switch (mMode) {
        case kMode_AutoLoad:
            SetState((State)0x2);
            break;
        case kMode_AutoSave:
            SetState((State)0x56);
            break;
        case kMode_DisableAutoSave:
            mUser = NULL;
            SetState((State)0x42);
            break;
        case kMode_ManualDelete:
            SetState((State)0x69);
            break;
        case kMode_ManualLoad:
            SetState((State)0x51);
            break;
        default:
            MILO_LOG("Unknown SaveLoadMode: %d\n", mMode);
            SetState((State)0x6e);
            break;
        }
        break;
    case (State)0x4:
        if (unk69) return;
        switch (unk6c) {
        case 7:
            SetState((State)0xb);
            break;
        case 8:
            SetState((State)0x5);
            break;
        case 9:
            SetState((State)0x7);
            break;
        default:
            SetState((State)0x42);
            break;
        }
        break;
    case (State)0x1A:
        if (ThePlatformMgr.mGuideShowing) return;
        SetState((State)0x19);
        break;
    case (State)0x2D:
        if (ThePlatformMgr.mGuideShowing) return;
        SetState((State)0x2b);
        break;
    case (State)0x3C:
        if (ThePlatformMgr.mGuideShowing) return;
        SetState((State)0x3b);
        break;
    case (State)0x14:
        if (!TheCacheMgr->IsDone()) return;
        {
            CacheResult result = TheCacheMgr->GetLastResult();
            unk70 = (int)result;
            if (result == kCache_NoError) {
                TheCacheMgr->AddCacheID(mCacheID, Symbol(unk4c.c_str()));
                SetState((State)0x1b);
            } else if (result == kCache_ErrorCacheNotFound) {
                SetState((State)0x15);
            } else {
                SetState((State)0x25);
            }
        }
        break;
    case (State)0x19:
        if (!TheCacheMgr->IsDone()) return;
        {
            CacheResult result = TheCacheMgr->GetLastResult();
            if (result == kCache_NoError) {
                unk7c = 2;
                int sz = mCacheID->GetDeviceID();
                unk78 = sz;
                TheCacheMgr->AddCacheID(mCacheID, Symbol(unk4c.c_str()));
                SetState((State)0x20);
            } else if (result == kCache_ErrorUserCancel) {
                unk7c = 1;
                SetState((State)0x17);
            } else {
                SetState((State)0x25);
            }
        }
        break;
    case (State)0x1B:
        if (!TheCacheMgr->IsDone()) return;
        {
            CacheResult result = TheCacheMgr->GetLastResult();
            if (result == kCache_NoError) {
                SetState((State)0x1e);
            } else if (result == kCache_ErrorStorageDeviceMissing) {
                SetState((State)0x16);
            } else if (result == kCache_ErrorCorrupt) {
                SetState((State)0x1c);
            } else {
                SetState((State)0x25);
            }
        }
        break;
    case (State)0x1D:
        if (!TheCacheMgr->IsDone()) return;
        UpdateStatus(kSaveLoadMgrStatus_Loading);
        SetState((State)0x20);
        break;
    case (State)0x20:
        if (!TheCacheMgr->IsDone()) return;
        {
            CacheResult result = TheCacheMgr->GetLastResult();
            if (result == kCache_NoError) {
                SetState((State)0x21);
            } else if (result == kCache_ErrorStorageDeviceMissing) {
                UpdateStatus(kSaveLoadMgrStatus_Loading);
                SetState((State)0x16);
            } else if (result == kCache_ErrorCorrupt) {
                UpdateStatus(kSaveLoadMgrStatus_Loading);
                SetState((State)0x1c);
            } else {
                UpdateStatus(kSaveLoadMgrStatus_Loading);
                SetState((State)0x25);
            }
        }
        break;
    case (State)0x22:
        if (!TheCacheMgr->IsDone()) return;
        if (TheCacheMgr->GetLastResult() == kCache_NoError) {
            SetState((State)0x26);
        } else {
            SetState((State)0x25);
        }
        break;
    case (State)0x23:
        if (!TheCacheMgr->IsDone()) return;
        UpdateStatus(kSaveLoadMgrStatus_Loading);
        if (unk70 == 0) {
            unk70 = (int)TheCacheMgr->GetLastResult();
        }
        if (unk70 == 0) {
            SetState((State)0x26);
        } else {
            SetState((State)0x25);
        }
        break;
    case (State)0x1E:
        if (!mCache->IsDone()) return;
        {
            CacheResult result = mCache->GetLastResult();
            if (result == kCache_NoError) {
                SetState((State)0x1f);
            } else if (result == kCache_ErrorStorageDeviceMissing) {
                SetState((State)0x16);
            } else {
                SetState((State)0x25);
            }
        }
        break;
    case (State)0x1F:
        if (!mCache->IsDone()) return;
        if (mCache->GetLastResult() == kCache_NoError) {
            BufStream stream(mData, unk64, true);
            TheSongMgr.LoadCachedSongInfo(stream);
            SetState((State)0x22);
        } else {
            SetState((State)0x25);
        }
        break;
    case (State)0x21:
        if (!mCache->IsDone()) return;
        unk70 = (int)mCache->GetLastResult();
        SetState((State)0x23);
        break;
    case (State)0x33:
        if (!mCache->IsDone()) return;
        unk70 = (int)mCache->GetLastResult();
        SetState((State)0x35);
        break;
    case (State)0x3E:
        if (!mCache->IsDone()) return;
        unk70 = (int)mCache->GetLastResult();
        SetState((State)0x3f);
        break;
    case (State)0x32:
        if (!TheCacheMgr->IsDone()) return;
        if (TheCacheMgr->GetLastResult() == kCache_NoError) {
            SetState((State)0x32);
        } else {
            SetState((State)0x37);
        }
        break;
    case (State)0x34:
        if (!TheCacheMgr->IsDone()) return;
        if (TheCacheMgr->GetLastResult() == kCache_NoError) {
            TheProfileMgr.SetGlobalOptionsSaveState(kMetaProfileLoaded);
        } else {
            TheProfileMgr.SetGlobalOptionsSaveState(kMetaProfileError);
        }
        SetState((State)0x38);
        break;
    case (State)0x35:
        if (!TheCacheMgr->IsDone()) return;
        UpdateStatus(kSaveLoadMgrStatus_Loading);
        if (unk70 == 0) {
            unk70 = (int)TheCacheMgr->GetLastResult();
        }
        if (unk70 == 0) {
            TheProfileMgr.SetGlobalOptionsSaveState(kMetaProfileLoaded);
        } else {
            TheProfileMgr.SetGlobalOptionsSaveState(kMetaProfileError);
        }
        SetState((State)0x38);
        break;
    case (State)0x3F:
        if (!TheCacheMgr->IsDone()) return;
        UpdateStatus(kSaveLoadMgrStatus_Loading);
        if (unk70 == 0) {
            unk70 = (int)TheCacheMgr->GetLastResult();
        }
        if (unk70 == 0) {
            TheProfileMgr.SetGlobalOptionsSaveState(kMetaProfileLoaded);
        } else {
            TheProfileMgr.SetGlobalOptionsSaveState(kMetaProfileError);
        }
        SetState((State)0x41);
        break;
    case (State)0x54:
        if (TheSongMgr.IsSongCacheWriteDone()) {
            SetState((State)0x54);
        }
        break;
    case (State)0x68:
    case kS_Finish:
        if (unk69) return;
        if (mCache != NULL) {
            if (!mCache->IsDone()) return;
            TheCacheMgr->UnmountAsync(&mCache, NULL);
        } else {
            if (!TheCacheMgr->IsDone()) return;
            if (mState == (State)0x6d) {
                SetState((State)0x6e);
            } else {
                SetState(kS_Idle);
            }
        }
        break;
    default:
        break;
    }
}

void SaveLoadManager::SetState(State newState) {
    State oldState = mState;
    if (oldState == newState) return;

    bool wasIdle = false;
    // Exit-state cleanup based on which state we're leaving.
    switch ((int)oldState) {
    case 0x0:
        wasIdle = true;
        break;
    case 0xb:
    case 0x46:
    case 0x47:
    case 0x64:
    case 0x6d:
        // Note: original frees mAction via virtual delete; MemcardAction
        // dtor is private in our header, so we leave the slot null instead.
        if (newState != (State)0x6d) {
            mAction = NULL;
        }
        break;
    case 0x1f:
    case 0x21:
    case 0x32:
    case 0x33:
    case 0x3e:
        if (newState != (State)0x6f) {
            if (mData != NULL) {
                _MemFree(mData);
                mData = NULL;
            }
        }
        break;
    case 0x6f:
        if (mData != NULL) {
            _MemFree(mData);
            mData = NULL;
        }
        break;
    default:
        break;
    }

    mState = newState;
    if (wasIdle) {
        UpdateStatus((SaveLoadMgrStatus)0);
    }
    if ((unsigned int)mState > 0x6e) return;

    // Entry handlers (partial implementation - large state machine)
    switch ((int)mState) {
    case 0x0:
        UpdateStatus((SaveLoadMgrStatus)5);
        break;
    case 0x1:
        unk7c = 0;
        break;
    case 0x2:
        if (mInitialLoadNotDone) {
            SetState((State)0x14);
        } else {
            SetState((State)0x3);
        }
        break;
    default:
        break;
    }
}

void SaveLoadManager::SaveLoadErrorSetState() {
    switch (mMode) {
    case kMode_AutoLoad:
        SetState(kS_AutoloadSelectProfile);
        break;
    case kMode_AutoSave:
    case kMode_ManualLoad:
        SetState(kS_SaveCheckProfile);
        break;
    case kMode_DisableAutoSave:
        SetState(kS_SaveCheckAutosave);
        break;
    default:
        break;
    }
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