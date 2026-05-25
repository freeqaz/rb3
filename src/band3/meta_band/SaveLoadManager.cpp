#include "SaveLoadManager.h"
#include "game/BandUser.h"
#include "game/BandUserMgr.h"
#include "meta/FixedSizeSaveable.h"
#include "meta/FixedSizeSaveableStream.h"
#include "meta/MemcardMgr_Wii.h"
#include "meta/Profile.h"
#include "meta/WiiProfileMgr.h"
#include "meta_band/BandProfile.h"
#include "meta_band/BandSongMgr.h"
#include "meta_band/ProfileMgr.h"
#include "meta_band/UIEventMgr.h"
#include "utl/MakeString.h"
#include "net/Net.h"
#include "net/Server.h"
#include "net_band/EntityUploader.h"
#include "net_band/RockCentral.h"
#include "net_band/RockCentralMsgs.h"
#include "obj/Data.h"
#include "obj/MessageTimer.h"
#include "obj/ObjMacros.h"
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

class SaveMemcardAction : public MemcardAction {
public:
    SaveMemcardAction(std::vector<BandProfile *, unsigned short> *);
    virtual ~SaveMemcardAction();
    virtual void PreAction();
    virtual void Action();
    virtual void PostAction();
    int unk24;
    int unk28;
};

SaveLoadManager *TheSaveLoadMgr;

SaveLoadManager::SaveLoadManager()
    : mActivated(false), mInitialLoadNotDone(true), mState(kS_Idle),
      mStateAtSelectStart(kS_Idle), mUser(NULL), mLocalUser(NULL),
      unk44(), unk48(0), mCacheID(NULL), mCache(NULL),
      mData(NULL), unk64(0), unk68(false), unk69(false), unk6c(0), unk70(0),
      mRequestFlags(0), unk78(0), unk7c(0), mAction(NULL) {
    mSaveProfiles.reserve(4);
    mUploadProfiles.reserve(4);
    SetName("saveload_mgr", ObjectDir::sMainDir);
    ThePlatformMgr.AddSink(this, SigninChangedMsg::Type());
}

SaveLoadManager::~SaveLoadManager() {
    ThePlatformMgr.RemoveSink(this, SigninChangedMsg::Type());
    RELEASE(mAction);
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
    case (State)0x27:
        if (!TheCacheMgr->IsDone()) return;
        {
            CacheResult result = TheCacheMgr->GetLastResult();
            unk70 = (int)result;
            if (result == kCache_NoError) {
                SetState((State)0x2e);
            } else if (result == kCache_ErrorCacheNotFound) {
                switch (unk7c) {
                case 0:
                    SetState((State)0x2b);
                    break;
                case 2:
                    SetState((State)0x2c);
                    break;
                default:
                    SetState((State)0x29);
                    break;
                }
            } else {
                SetState((State)0x37);
            }
        }
        break;
    case (State)0x2E:
        if (!TheCacheMgr->IsDone()) return;
        {
            CacheResult result = TheCacheMgr->GetLastResult();
            if (result == kCache_NoError) {
                SetState((State)0x32);
            } else if (result == kCache_ErrorStorageDeviceMissing) {
                SetState((State)0x28);
            } else if (result == kCache_ErrorCorrupt) {
                SetState((State)0x2f);
            } else {
                SetState((State)0x37);
            }
        }
        break;
    case (State)0x30:
        if (!TheCacheMgr->IsDone()) return;
        UpdateStatus(kSaveLoadMgrStatus_Loading);
        SetState((State)0x31);
        break;
    case (State)0x31:
        if (!TheCacheMgr->IsDone()) return;
        {
            CacheResult result = TheCacheMgr->GetLastResult();
            if (result == kCache_NoError) {
                SetState((State)0x33);
            } else if (result == kCache_ErrorStorageDeviceMissing) {
                UpdateStatus(kSaveLoadMgrStatus_Loading);
                SetState((State)0x28);
            } else if (result == kCache_ErrorCorrupt) {
                UpdateStatus(kSaveLoadMgrStatus_Loading);
                SetState((State)0x2f);
            } else {
                UpdateStatus(kSaveLoadMgrStatus_Loading);
                SetState((State)0x37);
            }
        }
        break;
    case (State)0x3B:
        if (!TheCacheMgr->IsDone()) return;
        {
            CacheResult result = TheCacheMgr->GetLastResult();
            if (result == kCache_NoError) {
                unk7c = 2;
                int sz = mCacheID->GetDeviceID();
                unk78 = sz;
                TheCacheMgr->AddCacheID(mCacheID, Symbol(unk4c.c_str()));
                SetState((State)0x3d);
            } else if (result == kCache_ErrorUserCancel) {
                unk7c = 1;
                SetState((State)0x3a);
            } else {
                SetState((State)0x40);
            }
        }
        break;
    case (State)0x3D:
        if (!TheCacheMgr->IsDone()) return;
        {
            CacheResult result = TheCacheMgr->GetLastResult();
            if (result == kCache_NoError) {
                SetState((State)0x3e);
            } else if (result == kCache_ErrorStorageDeviceMissing) {
                UpdateStatus(kSaveLoadMgrStatus_Loading);
                SetState((State)0x3a);
            } else {
                UpdateStatus(kSaveLoadMgrStatus_Loading);
                SetState((State)0x40);
            }
        }
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
        if (newState != (State)0x6d) {
            delete mAction;
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

BandProfile *SaveLoadManager::GetNewSigninProfile() {
    std::vector<BandProfile *> profiles = TheProfileMgr.GetNewlySignedInProfiles();
    if (!profiles.empty()) {
        BandProfile *pProfile = profiles[0];
        MILO_ASSERT(pProfile, 0x484);
        return pProfile;
    }
    return NULL;
}

BandProfile *SaveLoadManager::GetAutosavableProfile() {
    std::vector<BandProfile *> profiles = TheProfileMgr.GetShouldAutosaveProfiles();
    if (!profiles.empty()) {
        BandProfile *pProfile = profiles[0];
        MILO_ASSERT(pProfile, 0x494);
        return pProfile;
    }
    return NULL;
}

Symbol SaveLoadManager::GetDialogOpt1() {
    Symbol sym(gNullStr);
    switch (mState) {
    case (State)0x49:
    case kS_GlobalCreateCorrupt:        // 0x4E
        sym = global_options_button_cancel;
        break;
    case kS_AutoloadNoSaveFound_Msg:    // 0x06
        sym = mc_button_create_data;
        break;
    case kS_ManualLoadConfirm_Yes:      // 0x5F
        sym = mc_button_continue;
        break;
    case kS_AutoloadMultipleSavesFound: // 0x07
    case kS_AutoloadNotOwner:           // 0x0C
    case kS_GlobalCreateNotFound_Msg:   // 0x4C
    case kS_ManualLoadNoDevice:         // 0x5C
    case (State)0x62:
        sym = mc_button_choose_device;
        break;
    case kS_AutoloadCorrupt:            // 0x0E
    case kS_AutoloadObsolete:           // 0x0F
    case kS_AutoloadFuture:             // 0x10
    case kS_AutoloadFuture2:            // 0x11
    case kS_SaveDeviceInvalid:          // 0x48
        sym = mc_button_overwrite;
        break;
    case (State)0x17:
    case (State)0x18:
        sym = song_info_cache_button_create;
        break;
    case (State)0x1C:
        sym = song_info_cache_button_corrupt_overwrite;
        break;
    case (State)0x29:
    case (State)0x2A:
    case (State)0x3A:
        sym = global_options_button_create;
        break;
    case (State)0x2F:
        sym = global_options_button_corrupt_overwrite;
        break;
    case kS_ManualLoadConfirm:          // 0x60
        sym = mc_button_yes;
        break;
    default:
        break;
    }
    return sym;
}

Symbol SaveLoadManager::GetDialogOpt2() {
    Symbol sym(gNullStr);
    switch (mState) {
    case kS_AutoloadNoSaveFound_Msg:
    case kS_AutoloadMultipleSavesFound:
    case kS_AutoloadDeviceMissing:
    case kS_SaveOverwrite:
    case kS_ManualSaveNoDevice:
    case kS_ManualLoadConfirmUnsaved:
    case kS_ManualLoadNoDevice:
        sym = mc_button_cancel;
        break;
    case kS_AutoloadNotOwner:
    case kS_AutoloadCorrupt:
    case kS_AutoloadObsolete:
    case kS_AutoloadFuture:
        sym = mc_button_continue_no_save;
        break;
    case kS_SongCacheCreateNotFound_Msg:
    case kS_SongCacheCreateMissing_Msg:
    case kS_SongCacheCreateCorrupt:
        sym = song_info_cache_button_cancel;
        break;
    case kS_GlobalCreateNotFound_Msg:
    case kS_GlobalCreateMissing_Msg:
    case kS_GlobalCreateCorrupt:
    case kS_GlobalOptionsMissing_Msg:
        sym = global_options_button_cancel;
        break;
    case kS_SaveDeviceInvalid:
        sym = mc_button_disable_autosave;
        break;
    case kS_ManualLoadConfirm:
        sym = mc_button_no;
        break;
    default:
        break;
    }
    return sym;
}

DataNode SaveLoadManager::GetDialogMsg() {
    String profileName(gNullStr);
    int playerNum = -1;
    if (mUser != NULL) {
        profileName = mUser->UserName();
        playerNum = mUser->GetPadNum() + 1;
    }
    switch (mState) {
    case (State)0x6:
        return DataArrayPtr(
            mc_auto_load_no_save_found_fmt, DataArrayPtr(), profileName, playerNum
        );
    case (State)0x7:
        return DataArrayPtr(
            mc_auto_load_multiple_saves_found_fmt,
            DataArrayPtr(),
            profileName,
            playerNum
        );
    case (State)0xC:
        return DataArrayPtr(
            mc_load_device_missing_fmt, DataArrayPtr(), profileName, playerNum
        );
    case (State)0xE: {
        BandProfile *pProfile = GetProfile();
        if (pProfile == NULL) {
            return DataArrayPtr(mc_manual_load_corrupt, DataArrayPtr());
        }
        MILO_ASSERT(pProfile, 0xD4D);
        return DataArrayPtr(
            mc_auto_load_corrupt, DataArrayPtr(), pProfile->GetName()
        );
    }
    case (State)0xF:
        return DataArrayPtr(mc_auto_load_not_owner, DataArrayPtr());
    case (State)0x10:
        if (playerNum != -1) {
            return DataArrayPtr(
                mc_auto_load_obsolete_version_fmt,
                DataArrayPtr(),
                profileName,
                playerNum
            );
        }
        return DataArrayPtr(mc_auto_load_obsolete_version, DataArrayPtr());
    case (State)0x11:
        if (playerNum != -1) {
            return DataArrayPtr(
                mc_auto_load_newer_version_fmt,
                DataArrayPtr(),
                profileName,
                playerNum
            );
        }
        return DataArrayPtr(mc_auto_load_newer_version, DataArrayPtr());
    case (State)0x17:
        return DataArrayPtr(song_info_cache_create, DataArrayPtr());
    case (State)0x18:
        return DataArrayPtr(song_info_cache_missing, DataArrayPtr());
    case (State)0x1C:
        return DataArrayPtr(song_info_cache_corrupt, DataArrayPtr());
    case (State)0x29:
        return DataArrayPtr(global_options_create, DataArrayPtr());
    case (State)0x2A:
        return DataArrayPtr(global_options_missing, DataArrayPtr());
    case (State)0x2F:
        return DataArrayPtr(global_options_corrupt, DataArrayPtr());
    case (State)0x3A:
        return DataArrayPtr(global_options_missing, DataArrayPtr());
    case (State)0x42:
        return DataArrayPtr(mc_autosave_disabled, DataArrayPtr());
    case (State)0x48:
        return DataArrayPtr(mc_save_confirm_overwrite, DataArrayPtr());
    case (State)0x49:
        if (TheMemcardMgr.GetSizeNeeded() > 0) {
            int sz = TheMemcardMgr.GetSizeNeeded();
            if (!TheCacheMgr || !TheCacheMgr->IsDone() ||
                TheCacheMgr->GetLastResult() != kCache_NoError) {
                sz += 0x10;
            }
            return DataArrayPtr(mc_save_not_enough_space_fmt, DataArrayPtr(), sz);
        }
        return DataArrayPtr(mc_save_not_enough_space, DataArrayPtr());
    case (State)0x4C:
        return DataArrayPtr(
            mc_save_device_missing_fmt, DataArrayPtr(), profileName, playerNum
        );
    case (State)0x4E:
        return DataArrayPtr(mc_save_failed, DataArrayPtr());
    case (State)0x4F:
        return DataArrayPtr(mc_save_disabled_by_cheat, DataArrayPtr());
    case (State)0x50:
        return DataArrayPtr(mc_load_failed, DataArrayPtr());
    case (State)0x5C:
        return DataArrayPtr(mc_manual_save_no_selection, DataArrayPtr());
    case (State)0x5F:
        if (playerNum != -1) {
            return DataArrayPtr(
                mc_manual_load_confirm_unsaved_fmt,
                DataArrayPtr(),
                profileName,
                playerNum
            );
        }
        return DataArrayPtr(mc_manual_load_confirm_unsaved, DataArrayPtr());
    case (State)0x60:
        return DataArrayPtr(mc_manual_load_confirm, DataArrayPtr());
    case (State)0x62:
        return DataArrayPtr(mc_manual_load_no_selection, DataArrayPtr());
    case (State)0x63:
        return DataArrayPtr(mc_manual_load_storage_missing, DataArrayPtr());
    case (State)0x65:
        return DataArrayPtr(mc_manual_load_no_file, DataArrayPtr());
    case (State)0x66:
        return DataArrayPtr(mc_manual_load_corrupt, DataArrayPtr());
    case (State)0x67:
        return DataArrayPtr(mc_manual_load_not_owner, DataArrayPtr());
    default:
        MILO_ASSERT(false, 0xE00);
        return DataNode(0);
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

void SaveLoadManager::PrintoutSaveSizeInfo() {
    FixedSizeSaveable::EnablePrintouts(true);
    FormatString fmt("SAVESIZE\n");
    TheDebug << fmt.Str();
    int profileSize = BandProfile::SaveSize(0x97);
    int symbolSize = FixedSizeSaveableStream::GetSymbolTableSize(0x97);
    TheDebug << MakeString<int>("Symbol Table Size = %i\n", symbolSize);
    int wiiSize = WiiProfileMgr::SaveSize(0x97);
    TheDebug << MakeString<int>("SAVESIZE TOTAL = %i \n", (symbolSize + profileSize) + wiiSize);
}

bool SaveLoadManager::IsReasonToUpload() {
    DataNode &var = DataVariable(saveload_skip_upload);
    int skipUpload = var.Int(NULL) != 0;
    bool isConnected = TheNet.mServer->IsConnected();
    bool needsUpload = TheProfileMgr.NeedsUpload();
    bool allUnlocked = TheProfileMgr.mAllUnlocked;
    return !skipUpload && !allUnlocked && isConnected && needsUpload;
}

void SaveLoadManager::StartSaveAction(bool b) {
    UpdateStatus(kSaveLoadMgrStatus_Saving);
    mTimer.Restart();
    bool isOverwrite = (mState == kS_SaveOverwrite || mState == kS_SaveNoOverwrite);
    MILO_ASSERT(isOverwrite, 0x9c9);
    for (BandProfile **p = (BandProfile **)mUploadProfiles.begin(); p != (BandProfile **)mUploadProfiles.end(); p++) {
        TheWiiProfileMgr.SetLocked(*p, true);
    }
    unk69 = true;
    delete mAction;
    mAction = NULL;
    mAction = new SaveMemcardAction(&mUploadProfiles);
    TheMemcardMgr.AddSink(this);
    TheMemcardMgr.OnSaveGame(NULL, mAction, b);
}

DataNode SaveLoadManager::OnMsg(const DeviceChosenMsg &msg) {
    MILO_ASSERT(unk69, 0xa41);
    unk69 = false;
    TheMemcardMgr.RemoveSink(this);
    switch (mState) {
    case kS_AutoloadSetDevice:
    case kS_AutoloadSelectDevice2:
    case kS_AutoloadSelectDevice3:
    case kS_AutoloadStartLoad2:
        unk78 = msg.Device();
        SetState(kS_AutoloadStartLoad);
        break;
    case kS_GlobalCreateMissing_Msg:
        SetState(kS_SaveNoOverwrite);
        break;
    case kS_ManualSaveNoDevice:
        unk78 = msg.Device();
        SetState(kS_SaveChooseDeviceInvalid);
        break;
    case kS_ManualSaveChooseDevice:
        SetState(kS_ManualLoadChooseDevice);
        break;
    case kS_Done:
    case kS_LoadComplete:
    case kS_Finish:
        break;
    default:
        MILO_FAIL(
            "Unhandled DeviceChosenMsg in state %d and mode %d", (int)mState, (int)mMode
        );
        break;
    }
    return DataNode(0);
}

DataNode SaveLoadManager::OnMsg(const NoDeviceChosenMsg &) {
    MILO_ASSERT(unk69, 0xa73);
    unk69 = false;
    TheMemcardMgr.RemoveSink(this);
    switch (mState) {
    case kS_AutoloadSetDevice:
        SetState(kS_AutoloadNoSaveFound_Msg);
        break;
    case kS_AutoloadSelectDevice3:
        SetState(kS_AutoloadMultipleSavesFound);
        break;
    case kS_AutoloadStartLoad2:
        SetState(kS_AutoloadNotOwner);
        break;
    case kS_GlobalCreateMissing_Msg:
        SetState(kS_GlobalCreateNotFound_Msg);
        break;
    case kS_ManualSaveNoDevice:
        SetState(kS_ManualLoadNoDevice);
        break;
    case kS_ManualSaveChooseDevice:
        SetState(kS_GlobalOptionsMissing_Msg);
        break;
    case kS_Done:
    case kS_LoadComplete:
    case kS_Finish:
        break;
    default:
        MILO_FAIL(
            "Unhandled NoDeviceChosenMsg in state %d and mode %d",
            (int)mState,
            (int)mMode
        );
        break;
    }
    return DataNode(0);
}

void MCResultMsg::PrintExtra(TextStream &ts) const {
    ts << "res:" << mData->Int(2);
}

DataNode SaveLoadManager::OnMsg(const MCResultMsg &msg) {
    MILO_ASSERT(unk69, 0xaa3);
    unk69 = false;
    TheMemcardMgr.RemoveSink(this);
    MCResult res = (MCResult)msg.mData->Int(2);
    switch (mState) {
    case (State)0x4:
        unk6c = res;
        break;
    case kS_AutoloadStartLoad: {
        (void)mTimer.Stop();
        switch (res) {
        case kMCNoCard:
            SetState(kS_AutoloadNotOwner);
            break;
        case kMCCorrupt:
            SetState(kS_AutoloadCorrupt);
            break;
        case kMCNotOwner:
            SetState(kS_AutoloadObsolete);
            break;
        case kMCNotEnoughSpace:
        case kMCFileNotFound:
            SetState(kS_SaveOverwrite);
            break;
        case kMCObsoleteVersion:
            SetState(kS_AutoloadFuture);
            break;
        case kMCNewerVersion:
            SetState(kS_AutoloadFuture2);
            break;
        case kMCNoError:
            unk6c = res;
            SetState((State)0x43);
            break;
        default:
            SetState(kS_SaveFailed);
            break;
        }
        break;
    }
    case kS_SaveChooseDeviceInvalid: // 0x45
        switch (res) {
        case kMCNoCard:
            SetState(kS_GlobalCreateNotFound_Msg);
            break;
        case kMCNoError:
        case kMCFileExists:
        case kMCCorrupt:
        case kMCNotOwner:
            SetState(kS_SaveDeviceInvalid);
            break;
        case kMCFileNotFound:
        case kMCNotEnoughSpace:
            SetState(kS_SaveOverwrite);
            break;
        default:
            SetState(kS_GlobalCreateCorrupt);
            break;
        }
        break;
    case kS_SaveOverwrite: // 0x46
    case kS_SaveNoOverwrite: // 0x47
        unk6c = res;
        break;
    case kS_ManualLoadChooseDevice: // 0x64
        switch (res) {
        case kMCNoCard:
            SetState((State)0x63);
            break;
        case kMCFileNotFound:
            SetState((State)0x65);
            break;
        case kMCCorrupt:
            SetState((State)0x66);
            break;
        case kMCNotOwner:
            SetState((State)0x67);
            break;
        case kMCObsoleteVersion:
            SetState(kS_AutoloadFuture);
            break;
        case kMCNewerVersion:
            SetState(kS_AutoloadFuture2);
            break;
        case kMCNoError:
            unk6c = res;
            SetState((State)0x43);
            break;
        default:
            SetState(kS_SaveFailed);
            break;
        }
        break;
    case (State)0x6a:
        if (res == kMCNoError || res == kMCFileNotFound) {
            SetState((State)0x6b);
        } else {
            SetState((State)0x6c);
        }
        break;
    case kS_Done:
    case kS_LoadComplete:
    case kS_Finish:
        break;
    default:
        MILO_FAIL("SaveLoadManager::MCResultMsg in wrong state/mode %d %d", (int)mState, (int)mMode);
        break;
    }
    return DataNode(0);
}

DataNode SaveLoadManager::OnMsg(const RockCentralOpCompleteMsg &) {
    MILO_ASSERT(unk69, 0xb55);
    unk69 = false;
    if ((unsigned int)(mState - 0x6D) <= 2) {
        // Done/LoadComplete/Finish states - do nothing
    } else if (mState == (State)0x58) {
        SetState((State)0x57);
    } else {
        MILO_FAIL("Unexpected RockCentralOpCompleteMsg state");
    }
    return DataNode(0);
}

DataNode SaveLoadManager::OnMsg(const SigninChangedMsg &) {
    switch (mState) {
    case kS_AutoloadNoSaveFound_Msg:
    case kS_AutoloadMultipleSavesFound:
    case kS_AutoloadNotOwner:
    case kS_AutoloadCorrupt:
    case kS_AutoloadObsolete:
    case kS_AutoloadFuture:
    case kS_AutoloadFuture2:
    case (State)0x17:
    case (State)0x18:
    case (State)0x1c:
    case (State)0x29:
    case (State)0x2a:
    case (State)0x2f:
    case (State)0x3a:
    case (State)0x42:
    case kS_SaveDeviceInvalid:
    case (State)0x49:
    case kS_SaveNotEnoughSpacePS3:
    case kS_GlobalCreateNotFound_Msg:
    case kS_GlobalCreateCorrupt:
    case (State)0x4f:
    case kS_SaveFailed:
    case kS_ManualLoadNoDevice:
    case kS_ManualLoadConfirm_Yes:
    case kS_ManualLoadConfirm:
    case kS_GlobalOptionsMissing_Msg:
    case (State)0x63:
    case (State)0x65:
    case (State)0x66:
    case (State)0x67:
        if (!mUser)
            break;
        if (ThePlatformMgr.HasUserSigninChanged(mUser)) {
            bool dismissed = false;
            if (TheUIEventMgr->HasActiveDialogEvent()) {
                if (TheUIEventMgr->CurrentDialogEvent() == saveload_dialog_event) {
                    dismissed = true;
                }
            }
            if (dismissed) {
                TheUIEventMgr->DismissDialogEvent();
            } else {
                int padNum = mUser ? mUser->GetPadNum() : -1;
                TheDebug.Notify(MakeString<int, State>(
                    "Expected active dialog event during signin change on pad %d while in state %d.\n",
                    padNum, mState
                ));
            }
            SetState(kS_LoadComplete);
        }
        break;
    case kS_AutoloadStartLoad:
    case kS_SaveOverwrite:
    case kS_SaveNoOverwrite:
    case kS_ManualLoadChooseDevice:
        SetState(kS_Done);
        break;
    default:
        if (!mUser)
            break;
        if (ThePlatformMgr.HasUserSigninChanged(mUser)) {
            int padNum = mUser ? mUser->GetPadNum() : -1;
            TheDebug.Notify(MakeString<int, State>(
                "Expected active dialog event during signin change on pad %d while in state %d.\n",
                padNum, mState
            ));
            SetState(kS_Done);
        }
        break;
    case kS_Idle:
    case kS_Done:
    case kS_LoadComplete:
    case kS_Finish:
        break;
    }
    return DataNode(0);
}

DataNode SaveLoadManager::OnMsg(const ProfileSwappedMsg &msg) {
    LocalUser *user1 = msg.GetUser1();
    MILO_ASSERT(user1, 0xbcc);
    MILO_ASSERT(user1->GetLocalUser(), 0xbcd);
    LocalBandUser *bandUser1 = BandUserMgr::GetLocalBandUser(user1);
    MILO_ASSERT(bandUser1, 0xbcf);
    LocalUser *user2 = msg.GetUser2();
    MILO_ASSERT(user2, 0xbd1);
    MILO_ASSERT(user2->GetLocalUser(), 0xbd2);
    LocalBandUser *bandUser2 = BandUserMgr::GetLocalBandUser(user2);
    MILO_ASSERT(bandUser2, 0xbd4);
    if (mUser != NULL) {
        if (mUser == bandUser1) mUser = bandUser2;
        else if (mUser == bandUser2) mUser = bandUser1;
    }
    if (mLocalUser != NULL) {
        if (mLocalUser == user1) mLocalUser = user2;
        else if (mLocalUser == user2) mLocalUser = user1;
    }
    return DataNode(1);
}

void SaveLoadManager::HandleEventResponse(LocalUser *localUser, int choiceIdx) {
    State start = mStateAtSelectStart;
    State state = mState;
    mStateAtSelectStart = kS_Idle;
    if (start != state) {
        MILO_WARN(
            "States changed between UIComponentSelectMsg (%d) and UIComponentSelectDoneMsg (%d).\n",
            start,
            state
        );
        return;
    }
    if ((unsigned int)(choiceIdx - 1) > 2U) {
        MILO_FAIL("Bad choice index %i\n", choiceIdx);
        return;
    }
    mLocalUser = localUser;
    int isFirst = (choiceIdx == 1);
    switch (mState) {
    case kS_AutoloadNoSaveFound_Msg: // 0x6
        if (choiceIdx == 1) {
            if (unk7c == 2) {
                SetState(kS_AutoloadSelectDevice2);
            } else {
                SetState(kS_AutoloadSetDevice);
            }
        } else {
            SetState((State)0x42);
        }
        break;
    case (State)0x7:
        SetState(isFirst ? kS_AutoloadSelectDevice3 : (State)0x42);
        break;
    case kS_AutoloadNotOwner: // 0xc
        SetState(isFirst ? kS_AutoloadStartLoad2 : (State)0x42);
        break;
    case kS_SaveChooseDevice: // 0x4b
        SetState(isFirst ? kS_GlobalCreateMissing_Msg : (State)0x42);
        break;
    case kS_AutoloadCorrupt: // 0xe
    case kS_AutoloadObsolete: // 0xf
    case kS_AutoloadFuture: // 0x10
    case kS_AutoloadFuture2: // 0x11
    case kS_SaveNoOverwrite: // 0x47
        SetState(isFirst ? kS_SaveOverwrite : (State)0x42);
        break;
    case (State)0x17:
    case (State)0x18:
        SetState(isFirst ? (State)0x19 : (State)0x24);
        break;
    case (State)0x1c:
        SetState(isFirst ? (State)0x1d : (State)0x24);
        break;
    case (State)0x29:
    case (State)0x2a:
        SetState(isFirst ? (State)0x2b : (State)0x36);
        break;
    case (State)0x2f:
        SetState(isFirst ? (State)0x30 : (State)0x36);
        break;
    case (State)0x39:
        SetState(isFirst ? (State)0x3b : (State)0x40);
        break;
    case kS_GlobalCreateNotFound_Msg: // 0x4d
    case kS_GlobalCreateMissing_Msg: // 0x4e
    case (State)0x4f:
    case (State)0x63:
    case kS_ManualLoadChooseDevice: // 0x64
    case (State)0x65:
        SetState((State)0x42);
        break;
    case (State)0x41:
    case kS_SaveDeviceInvalid: // 0x48
        SaveLoadErrorSetState();
        break;
    case kS_ManualLoadInit: // 0x5a
        SetState(isFirst ? kS_ManualSaveNoDevice : (State)0x42);
        break;
    case kS_ManualLoadStartLoad: // 0x5d
    case kS_ManualLoadConfirmUnsaved: // 0x5e
        if (choiceIdx == 1) {
            SetState(kS_ManualLoadChooseDevice);
        } else {
            SetState((State)0x44);
        }
        break;
    case kS_ManualLoadConfirm: // 0x60
        SetState(isFirst ? kS_ManualSaveChooseDevice : (State)0x42);
        break;
    case (State)0x61:
        SetState((State)0x42);
        break;
    case (State)0x66:
    case (State)0x67:
    default:
        MILO_FAIL(
            "Unhandled UIComponentSelectDoneMsg from choice index %i in state %d and mode %d\n",
            (int)choiceIdx, (int)mState, (int)mMode
        );
        break;
    }
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