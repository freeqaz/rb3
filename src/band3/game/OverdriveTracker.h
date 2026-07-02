#pragma once
#include "Tracker.h"
#include "TrackerDisplay.h"
#include "game/TrackerSource.h"
#include "game/TrackerUtils.h"

class OverdriveTracker : public Tracker {
public:
    class DeployData {
    public:
        bool mCanDeploy; // 0x0
        bool mIsDeploying; // 0x1
    };

    OverdriveTracker(TrackerSource *, TrackerBandDisplay &, TrackerBroadcastDisplay &);
    virtual ~OverdriveTracker();
    virtual void TranslateRelativeTargets();
    virtual void UpdateGoalValueLabel(UILabel &) const;
    virtual void UpdateCurrentValueLabel(UILabel &) const;
    virtual String GetPlayerContributionString(Symbol) const;
    virtual void ConfigureTrackerSpecificData(const DataArray *);
    virtual void FirstFrame_(float);
    virtual void Poll_(float);
    virtual void TargetSuccess(int) const;
    virtual DataArrayPtr GetBroadcastDescription() const;
    virtual DataArrayPtr GetTargetDescription(int idx) const {
        return TrackerDisplay::MakeTimeTargetDescription(mTargets[idx]);
    }
    virtual TrackerChallengeType GetChallengeType() const {
        return (TrackerChallengeType)2;
    }
    virtual float GetCurrentValue() const { return mPastDurationBeats; }
    virtual void SavePlayerStats() const;

    void UpdateTimeRemainingDisplay();
    void LocalEndDeployStreak(float);
    void RemoteEndDeployStreak(Player *, int);

    std::map<TrackerPlayerID, DeployData> mDeployData; // 0x58
    TrackerMultiplierMap mChainMultipliers; // 0x70
    float mDeployBeats; // 0x8c
    float mDeployStartBeat; // 0x90
    float mDeployStartMs; // 0x94
    float mCurrentDurationBeats; // 0x98
    float mCurrentDurationMs; // 0x9c
    float mPastDurationBeats; // 0xa0
    float mCurrentMultiplier; // 0xa4
    int mCurrentMultiplierIndex; // 0xa8
    float mLastUpdateMs; // 0xac
    bool mWasDeploying; // 0xb0
};