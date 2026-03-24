#include "net_band/RockCentralJobs.h"
#include "meta_band/BandProfile.h"
#include "meta_band/ProfileMgr.h"
#include "net/WiiFriendMgr.h"
#include "net_band/RockCentral.h"
#include "obj/ObjMacros.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "os/PlatformMgr.h"
#include "utl/Std.h"

void RockCentralJob::Cancel(Hmx::Object *) {
    MILO_FAIL("No support for cancelling a Rock Central Job");
}

UpdateFriendsListJob::UpdateFriendsListJob(int i) : unk24(i), mState(0) {}

UpdateFriendsListJob::~UpdateFriendsListJob() { DeleteAll(mFriends); }

void UpdateFriendsListJob::Start() {
    mState = 1;
    ThePlatformMgr.EnumerateFriends(unk24, mFriends, this);
}

DataNode UpdateFriendsListJob::OnMsg(const PlatformMgrOpCompleteMsg &msg) {
    MILO_ASSERT(mState == kEnumeratingFriends, 0x36);
    GetFriendsListToken();
    int token = 0;
    BandProfile *profile = TheProfileMgr.GetProfileFromPad(unk24);
    if (profile)
        token = profile->GetUploadFriendsToken();
    if (msg.Success() && profile && profile->HasValidSaveData() && unk28 != token) {
        mState = 2;
        TheRockCentral.UpdateFriendList(profile, mFriends, unk34, this);
    } else
        mState = 3;
    return 1;
}

DataNode UpdateFriendsListJob::OnMsg(const RockCentralOpCompleteMsg &msg) {
    MILO_ASSERT(mState == kUpdatingFriends, 0x54);
    if (msg.Success()) {
        BandProfile *profile = TheProfileMgr.GetProfileFromPad(unk24);
        if (profile->HasValidSaveData()) {
            profile->SetUploadFriendsToken(unk28);
        }
    }
    mState = 3;
    return 1;
}

bool UpdateFriendsListJob::IsFinished() { return mState == 3; }

void UpdateFriendsListJob::GetFriendsListToken() {
    unk28 = 0;
    for (int i = 0; i < mFriends.size(); i++) {
        const char *name = mFriends[i]->mName.c_str();
        for (int j = 0; j < strlen(name); j += 4) {
            int hash[4];
            hash[0] = 0;
            int len = strlen(name);
            if (len - j >= 4U) {
                len = 4;
            } else {
                len = strlen(name) - j;
            }
            memcpy(hash, name + j, len);
            unk28 ^= hash[0];
        }
    }
}

BEGIN_HANDLERS(UpdateFriendsListJob)
    HANDLE_MESSAGE(PlatformMgrOpCompleteMsg)
    HANDLE_MESSAGE(RockCentralOpCompleteMsg)
    HANDLE_CHECK(0x7F)
END_HANDLERS

UpdateMasterProfileFriendsListJob::UpdateMasterProfileFriendsListJob() : mState(0) {}

UpdateMasterProfileFriendsListJob::~UpdateMasterProfileFriendsListJob() {
    DeleteAll(mFriends);
}

void UpdateMasterProfileFriendsListJob::Start() {
    mState = 1;
    TheWiiFriendMgr.EnumerateFriends(&unk28, this);
}

DataNode UpdateMasterProfileFriendsListJob::OnMsg(const WiiFriendMgrOpCompleteMsg &msg) {
    int count = (int)unk28.mFriends.size();
    MILO_ASSERT(mState == kEnumeratingFriends, 0x9A);
    for (int i = 0; i < count; i++) {
        WiiFriendProfile *prof = unk28.GetFriendByIdx(i)->MasterProfile();
        if (prof != NULL) {
            Friend *f = new Friend();
            f->SetName(String(prof->mName.c_str()));
            f->mFriendKey = prof->mPrincipalID;
            f->mOnline = false;
            f->SetGame(String(""));
            mFriends.push_back(f);
        }
    }
    GetFriendsListToken();
    if (unk24 != TheProfileMgr.unk5b8) {
        mState = 2;
        TheRockCentral.UpdateFriendList(TheServer.GetMasterProfileID(), mFriends, unk38, this);
    } else {
        mState = 3;
    }
    return 1;
}

DataNode UpdateMasterProfileFriendsListJob::OnMsg(const RockCentralOpCompleteMsg &msg) {
    MILO_ASSERT(mState == kUpdatingFriends, 0xC5);
    if (msg.Success()) {
        TheProfileMgr.SetUploadFriendsToken(unk24);
    }
    mState = 3;
    return 1;
}

bool UpdateMasterProfileFriendsListJob::IsFinished() { return mState == 3; }

void UpdateMasterProfileFriendsListJob::GetFriendsListToken() {
    unk24 = 0;
    for (int i = 0; i < mFriends.size(); i++) {
        const char *name = mFriends[i]->mName.c_str();
        for (int j = 0; j < strlen(name); j += 4) {
            int hash[4];
            hash[0] = 0;
            int len = strlen(name);
            if (len - j >= 4U) {
                len = 4;
            } else {
                len = strlen(name) - j;
            }
            memcpy(hash, name + j, len);
            unk24 ^= hash[0];
        }
    }
}

BEGIN_HANDLERS(UpdateMasterProfileFriendsListJob)
    HANDLE_MESSAGE(WiiFriendMgrOpCompleteMsg)
    HANDLE_MESSAGE(RockCentralOpCompleteMsg)
    HANDLE_CHECK(0xED)
END_HANDLERS