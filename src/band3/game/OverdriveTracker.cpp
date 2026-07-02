#include "game/OverdriveTracker.h"
#include "game/Game.h"
#include "game/Player.h"
#include "game/SongDB.h"
#include "game/TrackerDisplay.h"
#include "math/Utl.h"
#include "obj/Data.h"
#include "os/Debug.h"
#include "os/System.h"
#include "utl/Locale.h"
#include "utl/Symbols.h"
#include "utl/Symbols3.h"
#include "utl/Symbols4.h"
#include "utl/TimeConversion.h"

OverdriveTracker::OverdriveTracker(
    TrackerSource *src, TrackerBandDisplay &banddisp, TrackerBroadcastDisplay &bcdisp
)
    : Tracker(src, banddisp, bcdisp), mWasDeploying(0) {}

OverdriveTracker::~OverdriveTracker() {}

void OverdriveTracker::ConfigureTrackerSpecificData(const DataArray *arr) {
    mChainMultipliers.InitFromDataArray(arr->FindArray(chain_multipliers, false));
}

void OverdriveTracker::TranslateRelativeTargets() {
    int i8 = 0;
    int phrasecount = TheSongDB->NumCommonPhrases();
    for (int i = 0; i < phrasecount; i++) {
        i8 += CountBits(TheSongDB->GetCommonPhraseTracks(i));
    }
    int playercount = mSource->GetPlayerCount();
    if (playercount == 0) {
        i8 = 0;
    } else {
        i8 = i8 / playercount;
    }
    float mult = mChainMultipliers.GetMultiplier(playercount);
    DataArray *cfg = SystemConfig("scoring", "band_energy");
    float deploybeats = cfg->FindFloat("deploy_beats");
    cfg->FindFloat("spotlight_phrase");

    float factor = mult * ((float)playercount * ((float)(i8 / 4) * deploybeats));
    mDeployBeats = deploybeats;
    for (int i = 0; i < mTargets.size(); i++) {
        mTargets[i] = factor * mTargets[i];
    }
}

void OverdriveTracker::FirstFrame_(float) {
    mWasDeploying = false;
    mDeployStartBeat = -1.0f;
    mDeployStartMs = -1.0f;
    mCurrentDurationBeats = 0;
    mCurrentDurationMs = 0;
    mPastDurationBeats = 0;
    mCurrentMultiplier = 1.0f;
    mCurrentMultiplierIndex = -1;
    mLastUpdateMs = 0;
    mBandDisplay.Initialize(overdrive_tracker_description);
    for (TrackerPlayerID id = mSource->GetFirstPlayer(); id.NotNull();
         id = mSource->GetNextPlayer(id)) {
        DeployData data;
        mDeployData[id] = data;
    }
    mBroadcastDisplay.SetType((TrackerBroadcastDisplay::BroadcastDisplayType)1);
    mBroadcastDisplay.SetSecondaryStateLevel(0);
    mBroadcastDisplay.Hide();
    UpdateTimeRemainingDisplay();
}

void OverdriveTracker::Poll_(float ms) {
    MILO_ASSERT(TheGame, 0x81);
    if (!TheGame->InRollback()) {
    if (mSource->IsFinished()) {
        float f98 = mCurrentDurationBeats;
        if (f98 > 0) {
            mCurrentDurationBeats = 0;
            mPastDurationBeats = f98 * mCurrentMultiplier + mPastDurationBeats;
        }
    } else {
        bool b4 = false;
        Player *pLocalPlayer = nullptr;
        bool t5 = false;
        for (TrackerPlayerID id = mSource->GetFirstPlayer(); id.NotNull();
             id = mSource->GetNextPlayer(id)) {
            Player *player = mSource->GetPlayer(id);
            MILO_ASSERT(player, 0x9E);
            const TrackerPlayerDisplay &disp = GetPlayerDisplay(id);
            bool candeploy = player->CanDeployOverdrive();
            bool c1 = mDeployData[id].mCanDeploy;
            bool isdeploying = player->IsDeployingBandEnergy();
            DeployData &data = mDeployData[id];
            bool d1 = data.mIsDeploying;
            if (isdeploying) {
                if (c1) {
                    disp.LoseFocus(true);
                }
                t5 = true;
            } else if (candeploy && !c1) {
                disp.GainFocus(false);
            } else if (d1 && player->IsLocal()) {
                b4 = true;
                pLocalPlayer = player;
            }
            mDeployData[id].mCanDeploy = candeploy;
            mDeployData[id].mIsDeploying = isdeploying;
        }
        float beat = MsToBeat(ms);
        if (t5) {
            if (mDeployStartBeat == -1.0f) {
                mDeployStartBeat = beat;
                mDeployStartMs = ms;
                mCurrentDurationBeats = 0;
            } else {
                mCurrentDurationBeats = beat - mDeployStartBeat;
                mCurrentDurationMs = ms - mDeployStartMs;
            }
        }
        if (mCurrentDurationBeats > 0) {
            int multidx = mChainMultipliers.GetMultiplierIndex(mCurrentDurationBeats / mDeployBeats);
            if (multidx != mCurrentMultiplierIndex) {
                mBroadcastDisplay.SetSecondaryStateLevel(multidx);
                mCurrentMultiplierIndex = multidx;
            }
        }
        if (!t5 && mWasDeploying) {
            if (b4) {
                MILO_ASSERT(pLocalPlayer, 0xE9);
                float prod = mCurrentDurationBeats * mCurrentMultiplier;
                LocalEndDeployStreak(prod);
                static Message endStreakMsg("send_tracker_end_deploy_streak", 0.0f);
                endStreakMsg[0] = (int)(prod * 10000.0f);
                pLocalPlayer->HandleType(endStreakMsg);
            }
        } else if (t5 && !mWasDeploying) {
            mBroadcastDisplay.Show();
        }
        UpdateTimeRemainingDisplay();
        mWasDeploying = t5;
    }
    }
}

void OverdriveTracker::RemoteEndDeployStreak(Player *, int i) {
    LocalEndDeployStreak((float)i / 10000.0f);
}

void OverdriveTracker::LocalEndDeployStreak(float f) {
    mPastDurationBeats += f;
    mCurrentDurationBeats = 0;
    mDeployStartBeat = -1.0f;
    mCurrentMultiplier = 1.0f;
    mBroadcastDisplay.SetSecondaryStateLevel(0);
    mCurrentMultiplierIndex = 0;
    mBroadcastDisplay.Hide();
}

void OverdriveTracker::UpdateGoalValueLabel(UILabel &label) const {
    int min, sec;
    TrackerDisplay::MsToMinutesSeconds(mTargets.front(), min, sec);
    label.SetTokenFmt(tour_goal_od_timer_goal_format, min, sec);
}

void OverdriveTracker::UpdateCurrentValueLabel(UILabel &label) const {
    int min, sec;
    TrackerDisplay::MsToMinutesSeconds(0.0f, min, sec);
    label.SetTokenFmt(tour_goal_od_timer_result_format, min, sec);
}

String OverdriveTracker::GetPlayerContributionString(Symbol s) const {
    TrackerPlayerID pid = mSource->GetIDFromInstrument(s);
    float f1 = 0;
    if (pid.NotNull()) {
        Player *pPlayer = mSource->GetPlayer(pid);
        MILO_ASSERT(pPlayer, 0x13F);
        f1 = pPlayer->mStats.mTrackerContribution;
    }
    int min, sec;
    TrackerDisplay::MsToMinutesSeconds(f1, min, sec);
    return MakeString(Localize(tour_goal_od_timer_result_format, 0), min, sec);
}

void OverdriveTracker::SavePlayerStats() const {
    for (TrackerPlayerID id = mSource->GetFirstPlayer(); id.NotNull();
         id = mSource->GetNextPlayer(id)) {
        Player *pPlayer = mSource->GetPlayer(id);
        MILO_ASSERT(pPlayer, 0x157);
        pPlayer->mStats.mTrackerContribution = pPlayer->mStats.mTotalOverdriveDurationMs;
    }
}

void OverdriveTracker::TargetSuccess(int) const {}

DataArrayPtr OverdriveTracker::GetBroadcastDescription() const {
    return DataArrayPtr(overdrive_tracker_explanation);
}

void OverdriveTracker::UpdateTimeRemainingDisplay() {
    if (mCurrentDurationMs != mLastUpdateMs) {
        int min, sec;
        TrackerDisplay::MsToMinutesSeconds(mCurrentDurationMs, min, sec);
        mBroadcastDisplay.SetBandMessage(
            DataArrayPtr(overdrive_deploy_tracker_progress, min, sec)
        );
        mLastUpdateMs = mCurrentDurationMs;
    }
}