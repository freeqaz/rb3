#include "meta_band/AppMiniLeaderboardDisplay.h"
#include "bandobj/BandList.h"
#include "game/Defines.h"
#include "meta_band/AppLabel.h"
#include "meta_band/ProfileMgr.h"
#include "meta_band/Utl.h"
#include "net_band/RockCentral.h"
#include "obj/ObjMacros.h"
#include "obj/Task.h"
#include "os/Debug.h"
#include "rndobj/EventTrigger.h"
#include "rndobj/Group.h"
#include "ui/UI.h"
#include "ui/UIList.h"
#include "ui/UIListProvider.h"
#include "utl/Symbols.h"
#include "utl/Symbols2.h"
#include "utl/Symbols3.h"
#include "utl/Symbols4.h"

void AppMiniLeaderboardDisplay::Init() {
    REGISTER_OBJ_FACTORY(AppMiniLeaderboardDisplay)
}

AppMiniLeaderboardDisplay::AppMiniLeaderboardDisplay()
    : mStatus(kLeaderboardUnloaded), mUIList(nullptr), mLeaderboard(nullptr),
      mSongID(0), mScoreType((ScoreType)2), mUpdateTime(0.0f) {}

AppMiniLeaderboardDisplay::~AppMiniLeaderboardDisplay() {
    if (mUIList) {
        mUIList->SetProvider(mUIList);
    }
    if (mLeaderboard) {
        delete mLeaderboard;
    }
    MILO_ASSERT(mUIList, 0x63);
    mUIList->SetProvider(mUIList);
}

void AppMiniLeaderboardDisplay::Poll() {
    UIComponent::Poll();
    if (mSongID != 0 && mStatus == kLeaderboardUnloaded) {
        float t = TheTaskMgr.UISeconds();
        if (mUpdateTime > t) {
            mUpdateTime = t;
        }
        if (t - mUpdateTime >= 1.0f) {
            UpdateLeaderboardOnline(mSongID);
        }
    }
    if (mLeaderboard) {
        mLeaderboard->Poll();
    }
}

void AppMiniLeaderboardDisplay::DrawShowing() {
    RndDir *d = mResource->Dir();
    MILO_ASSERT(d, 0x8d);
    d->SetWorldXfm(mCache->mFlags & 1 ? WorldXfm_Force() : mWorldXfm);
    d->Draw();
}

void AppMiniLeaderboardDisplay::SetLeaderboardStatus(LeaderboardStatus status) {
    if (status == mStatus)
        return;
    mStatus = status;
    if (mPendingGroup) {
        mPendingGroup->SetShowing(status == kLeaderboardLoading);
    }
    MILO_ASSERT(mUIList, 0x9e);
    mUIList->SetShowing(mStatus == kLeaderboardReady);
    switch (mStatus) {
    case kLeaderboardError:
    case kLeaderboardUnloaded:
        if (mFadeOutTrigger)
            mFadeOutTrigger->Trigger();
        break;
    case kLeaderboardReady:
    case kLeaderboardLoading:
        if (mFadeInTrigger)
            mFadeInTrigger->Trigger();
        break;
    }
}

void AppMiniLeaderboardDisplay::UpdateLeaderboardOnline(int songID) {
    SetLeaderboardStatus(kLeaderboardLoading);
    BandProfile *p = TheProfileMgr.GetPrimaryProfile();
    if (mUIList)
        mUIList->SetProvider(nullptr);
    if (mLeaderboard) {
        mLeaderboard->CancelEnumerate();
    }
    mLeaderboard = nullptr;
    if (p) {
        MILO_ASSERT(mUIList, 0xc9);
        PlayerMiniLeaderboard *lb =
            new PlayerMiniLeaderboard(p, this, mScoreType, mSongID, mUIList->NumDisplay());
        mLeaderboard = lb;
        mUIList->SetProvider(lb);
        lb->StartEnumerate();
    } else {
        Update();
    }
}

int AppMiniLeaderboardDisplay::UpdateLeaderboard(int songID, ScoreType scoreType) {
    if (songID == mSongID && scoreType == mScoreType)
        return 0;
    mScoreType = scoreType;
    mSongID = songID;
    if (mTitleLabel) {
        mTitleLabel->SetTextToken(mini_leaderboards_title_friends);
    }
    if (mIconsLabel) {
        mIconsLabel->SetTextToken(gNullStr);
        if (mScoreType == kScoreBand) {
            for (int i = 0; i < 4; i++) {
                mIconsLabel->AppendIcon(*GetFontCharFromTrackType((TrackType)i, 0));
            }
        } else {
            mIconsLabel->AppendIcon(
                *GetFontCharFromTrackType(ScoreTypeToTrackType(mScoreType), 0)
            );
        }
    }
    if (mSongID == 0)
        return 1;
    CancelOldServerRequest();
    mScoreType = scoreType;
    SetLeaderboardStatus(kLeaderboardUnloaded);
    mUpdateTime = TheTaskMgr.UISeconds();
    return 1;
}

bool AppMiniLeaderboardDisplay::IsReady() { return mStatus == kLeaderboardReady; }

bool AppMiniLeaderboardDisplay::HasRows() {
    if (mLeaderboard && mLeaderboard->NumData()) {
        return true;
    }
    return false;
}

void AppMiniLeaderboardDisplay::ResultSuccess(bool, bool, bool) {
    MILO_ASSERT(mUIList, 0x118);
    mUIList->Refresh(false);
    int selfRow = mLeaderboard->GetSelfRow();
    if (selfRow >= 0) {
        mUIList->SetSelected(selfRow, -1);
    } else {
        mUIList->SetSelected(0, -1);
    }
    SetLeaderboardStatus(kLeaderboardReady);
}

void AppMiniLeaderboardDisplay::ResultFailure() {
    SetLeaderboardStatus(kLeaderboardError);
}

void AppMiniLeaderboardDisplay::Update() {
    UIComponent::Update();
    DataArray *typeDef = const_cast<DataArray *>(TypeDef());
    MILO_ASSERT(typeDef, 0x132);
    ObjectDir *dir = mResource->Dir();
    MILO_ASSERT(dir, 0x135);
    mUIList = dynamic_cast<BandList *>(
        dir->FindObject(typeDef->FindArray(leaderboard, true)->Str(1), false)
    );
    mTitleLabel = dynamic_cast<AppLabel *>(
        dir->FindObject(typeDef->FindArray(title_label, true)->Str(1), false)
    );
    mIconsLabel = dynamic_cast<AppLabel *>(
        dir->FindObject(typeDef->FindArray(icons_label, true)->Str(1), false)
    );
    mResetTrigger = dynamic_cast<EventTrigger *>(
        dir->FindObject(typeDef->FindArray(reset_trigger, true)->Str(1), false)
    );
    mFadeInTrigger = dynamic_cast<EventTrigger *>(
        dir->FindObject(typeDef->FindArray(fade_in_trigger, true)->Str(1), false)
    );
    mFadeOutTrigger = dynamic_cast<EventTrigger *>(
        dir->FindObject(typeDef->FindArray(fade_out_trigger, true)->Str(1), false)
    );
    mPendingGroup = dynamic_cast<RndGroup *>(
        dir->FindObject(typeDef->FindArray(pending_group, true)->Str(1), false)
    );
    if (mResetTrigger) {
        mResetTrigger->Trigger();
    }
}

void AppMiniLeaderboardDisplay::Exit() {
    UIComponent::Exit();
    CancelOldServerRequest();
    mSongID = 0;
    mUpdateTime = 0.0f;
}

void AppMiniLeaderboardDisplay::CancelOldServerRequest() {
    if (mLeaderboard) {
        mLeaderboard->CancelEnumerate();
    }
}

BEGIN_HANDLERS(AppMiniLeaderboardDisplay)
    HANDLE_ACTION(fade_in, mFadeInTrigger ? mFadeInTrigger->Trigger() : (void)0)
    HANDLE_ACTION(fade_out, mFadeOutTrigger ? mFadeOutTrigger->Trigger() : (void)0)
    HANDLE_EXPR(update_leaderboard, UpdateLeaderboard(_msg->Int(2), (ScoreType)_msg->Int(3)))
    HANDLE_SUPERCLASS(MiniLeaderboardDisplay)
    HANDLE_CHECK(0x16c)
END_HANDLERS

void PlayerMiniLeaderboard::EnumerateFromID() {
    mDataResultList.Clear();
    std::vector<int> ids;
    GetPlayerIds(ids);
    TheRockCentral.GetLeaderboardByPlayer(
        ids, 0, mScoreType, kSong, ModeToLeaderboardMode(mMode), sPageSize,
        mDataResultList, this
    );
}
