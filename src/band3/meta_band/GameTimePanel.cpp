#include "meta_band/GameTimePanel.h"
#include "game/GamePanel.h"
#include "obj/Task.h"
#include "ui/UIPanel.h"
#include "utl/DeJitter.h"
#include "utl/Loader.h"

GameTimePanel::GameTimePanel() : mTempo(0) {}

void GameTimePanel::Load() {
    UIPanel::Load();
    mPeriod = TheLoadMgr.SetLoaderPeriod(10.0f);
}

void GameTimePanel::Unload() {
    UIPanel::Unload();
    TheLoadMgr.SetLoaderPeriod(mPeriod);
    TheGamePanel->unk150 = true;
}

void GameTimePanel::Exit() { UIPanel::Exit(); }

void GameTimePanel::Enter() {
    UIPanel::Enter();
    TheGamePanel->unk150 = false;
    mTempo = TheTaskMgr.DeltaBeat() / TheTaskMgr.DeltaSeconds();
    mTimer.Restart();
}

void GameTimePanel::Poll() {
    if (!TheGamePanel->unk150) {
        mTimer.Split();
        float secs = Timer::CyclesToMs(mTimer.mCycles) / 1000.0f;
        float dejittered = TheGamePanel->mDeJitter.Apply(
            1000.0f * (secs + TheTaskMgr.Seconds(TaskMgr::kRealTime)), secs
        );
        secs *= 0.001f;
        float dejitteredSecs = dejittered * 0.001f;
        TheTaskMgr.SetSecondsAndBeat(
            dejitteredSecs,
            TheTaskMgr.Beat() + mTempo * secs,
            false
        );
    }
    mTimer.Restart();
}