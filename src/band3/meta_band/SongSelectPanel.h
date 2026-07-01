#pragma once
#include "game/BandUser.h"
#include "game/Defines.h"
#include "meta_band/AppMiniLeaderboardDisplay.h"
#include "meta/HeldButtonPanel.h"
#include "meta_band/Leaderboard.h"

class SongSelectPanel : public HeldButtonPanel, public Leaderboard::Callback {
public:
    SongSelectPanel();
    OBJ_CLASSNAME(SongSelectPanel);
    OBJ_SET_TYPE(SongSelectPanel);
    NEW_OBJ(SongSelectPanel);
    virtual DataNode Handle(DataArray *, bool);
    virtual ~SongSelectPanel() {}
    virtual bool Exiting() const;
    virtual void Poll();
    virtual void Load();
    virtual void Unload();
    virtual bool IsLoaded() const;
    virtual void FinishLoad();
    virtual void ResultSuccess(bool, bool, bool);
    virtual void ResultFailure();

    DataNode OnMsg(const ButtonDownMsg &);
    Leaderboard *GetLeaderboard(LocalBandUser *, ScoreType, int, Leaderboard::Mode);
    void RestartLeaderboardTimer();
    void CancelLeaderboardTimer();
#ifdef HX_NATIVE
    // Native-only: directly toggle the mini-leaderboard group (live_lb.grp) vs
    // the difficulty grid (live_diffs.grp). The Wii relies on EventTrigger env-
    // alpha anims the native renderer doesn't honor for hide; see FinishLoad.
    void SetMiniLeaderboardGroupShowing(bool showing);
#endif

    Leaderboard *mLeaderboard; // 0x44
    AppMiniLeaderboardDisplay *unk48; // 0x48
    float unk4c;
    float unk50;
    bool unk54;
    float unk58;
};