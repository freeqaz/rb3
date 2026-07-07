#include "meta_band/SongSelectPanel.h"
#include "AppMiniLeaderboardDisplay.h"
#include "game/BandUser.h"
#include "meta_band/Leaderboard.h"
#include "meta_band/MusicLibrary.h"
#include "meta_band/PlayerLeaderboards.h"
#include "meta_band/ProfileMgr.h"
#include "meta_band/SongSortNode.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/ObjMacros.h"
#include "obj/Task.h"
#include "os/ContentMgr.h"
#include "os/Debug.h"
#include "os/JoypadMsgs.h"
#include "ui/UIList.h"
#include "ui/UIPanel.h"
#include "utl/Messages2.h"
#include "utl/Symbols.h"
#include "utl/Symbols3.h"
#ifdef HX_NATIVE
#include "rndobj/Group.h"
#include "utl/Std.h" // FOREACH
#include <cstdio>
#include <cstdlib> // getenv (RB3_SS_GRPLOG, C2a census)
#endif

SongSelectPanel::SongSelectPanel()
    : mLeaderboard(0), mMiniLeaderboard(0), mRotationOffSecs(0), mRotationOnSecs(0), mLeaderboardShowing(0), mLastRotateSecs(-1) {}

void SongSelectPanel::Load() {
    UIPanel::Load();
    TheMusicLibrary->OnLoad();
    TheContentMgr->StartRefresh();
}

bool SongSelectPanel::IsLoaded() const {
    return UIPanel::IsLoaded() && !TheContentMgr->RefreshInProgress();
}

void SongSelectPanel::FinishLoad() {
    UIPanel::FinishLoad();
#ifdef HX_NATIVE
    // The mini-leaderboard display (online scores) is absent from the 360-ARK
    // extract's ui/song_select/song_select.milo, and online leaderboards don't
    // exist offline anyway. Find it non-failing; mMiniLeaderboard stays null and the
    // leaderboard-rotation Poll path is gated on mLastRotateSecs >= 0 (only armed once a
    // real online leaderboard enumerates, which never happens offline), so a null
    // mMiniLeaderboard is safe. (Same tolerant-asset pattern as MetaPanel metamusic.)
    mMiniLeaderboard = mDir->Find<AppMiniLeaderboardDisplay>("leaderboard.mld", false);
#else
    mMiniLeaderboard = mDir->Find<AppMiniLeaderboardDisplay>("leaderboard.mld", true);
#endif
    mRotationOffSecs = TypeDef()->FindFloat("mini_leaderboard_rotation_off");
    mRotationOnSecs = TypeDef()->FindFloat("mini_leaderboard_rotation_on");
#ifdef HX_NATIVE
    // The mini-leaderboard panel group (live_lb.grp, "FRIEND RANKINGS" title +
    // online score rows) is authored showing-by-default in the milo. On the Wii
    // the leaderboard_hide.trg EventTrigger anim fades it out (env alpha) until
    // the rotation timer swaps it in once online scores enumerate. The native
    // renderer doesn't honor that env-alpha fade for hide, so the group stays
    // fully visible and overlaps the difficulty grid — and offline it should
    // never appear at all (online enumerate never completes). Force the same
    // initial state the rotation expects: leaderboard hidden, difficulty grid
    // shown. The Poll() show-path (set_mini_leaderboard_showing 1) re-shows it
    // explicitly when an online leaderboard actually becomes ready.
    SetMiniLeaderboardGroupShowing(false);
#endif
}

#ifdef HX_NATIVE
static void C2LogGroup(ObjectDir *dir, const char *grpName) {
    RndGroup *g = dir->Find<RndGroup>(grpName, false);
    if (!g) {
        fprintf(stderr, "[C2GRP] %s = <not found>\n", grpName);
        return;
    }
    fprintf(stderr, "[C2GRP] %s showing=%d members:\n", grpName, (int)g->Showing());
    FOREACH (it, g->mObjects) {
        Hmx::Object *o = *it;
        fprintf(
            stderr, "[C2GRP]   - %s (%s)\n", o ? o->Name() : "<null>",
            o ? o->ClassName().Str() : "?"
        );
    }
}

void SongSelectPanel::SetMiniLeaderboardGroupShowing(bool showing) {
    if (RndGroup *lb = mDir->Find<RndGroup>("live_lb.grp", false))
        lb->SetShowing(showing);
    if (RndGroup *diffs = mDir->Find<RndGroup>("live_diffs.grp", false))
        diffs->SetShowing(!showing);
    if (getenv("RB3_SS_GRPLOG")) {
        // C2a census: where do difficulty_bg*/raitings_bg background meshes live,
        // and which of live_lb.grp / live_diffs.grp is shown?
        C2LogGroup(mDir, "live_lb.grp");
        C2LogGroup(mDir, "live_diffs.grp");
        static const char *bgNames[] = {
            "difficulty_bg.mesh",   "raitings_bg.mesh",     "rating.grp",
            "details_background.mat", "details_songscores_bg.mesh",
            "difficulty_bg01.mesh", "all.grp",              "song_info.grp",
            "perf_song_only.grp",   "performance.grp",      nullptr
        };
        for (int i = 0; bgNames[i]; i++) {
            RndDrawable *d = mDir->Find<RndDrawable>(bgNames[i], false);
            if (d)
                fprintf(
                    stderr, "[C2BG] %-28s FOUND showing=%d\n", bgNames[i],
                    (int)d->Showing()
                );
            else {
                Hmx::Object *o = mDir->Find<Hmx::Object>(bgNames[i], false);
                fprintf(
                    stderr, "[C2BG] %-28s %s\n", bgNames[i],
                    o ? "FOUND (non-drawable)" : "<not found>"
                );
            }
        }
    }
}
#endif

bool SongSelectPanel::Exiting() const {
    return UIPanel::Exiting() || TheMusicLibrary->IsExiting();
}

void SongSelectPanel::Unload() {
    RELEASE(mLeaderboard);
    mMiniLeaderboard = nullptr;
    TheMusicLibrary->OnUnload();
    UIPanel::Unload();
}

DataNode SongSelectPanel::OnMsg(const ButtonDownMsg &) {
    if (TheMusicLibrary->IsPurchasing()) {
        return 1;
    } else if (TheContentMgr->RefreshInProgress()) {
        static Message msg("set_blocking", 1);
        UIPanel *clp = ObjectDir::Main()->Find<UIPanel>("content_loading_panel", true);
        MILO_ASSERT(clp, 0x6C);
        clp->Handle(msg, true);
        return 1;
    } else
        return DataNode(kDataUnhandled, 0);
}

Leaderboard *SongSelectPanel::GetLeaderboard(
    LocalBandUser *u, ScoreType s, int i, Leaderboard::Mode m
) {
    RELEASE(mLeaderboard);
    switch (TheMusicLibrary->GetHighlightedNode()->GetType()) {
    case kNodeSong:
        mLeaderboard =
            new PlayerSongLeaderboard(TheProfileMgr.GetProfileForUser(u), this, s, i);
        break;
    case kNodeSetlist:
        mLeaderboard =
            new PlayerBattleLeaderboard(TheProfileMgr.GetProfileForUser(u), this, i);
        break;
    default:
        MILO_FAIL("No leaderboard for the highlighted SongNodeType!");
        break;
    }
    MILO_ASSERT(mLeaderboard, 0x8A);
    mLeaderboard->SetMode(m, false);
    mLeaderboard->StartEnumerate();
    return mLeaderboard;
}

void SongSelectPanel::ResultSuccess(bool b1, bool b2, bool b3) {
    static Message success("lb_success", 0, 0, 0);
    success[0] = b1;
    success[1] = b2;
    success[2] = b3;
    HandleType(success);
}

void SongSelectPanel::ResultFailure() { HandleType(lb_failure_msg); }

void SongSelectPanel::Poll() {
    HeldButtonPanel::Poll();
    if (mLeaderboard)
        mLeaderboard->Poll();
    if (mLastRotateSecs >= 0.0f && GetState() == kUp) {
        float diff = TheTaskMgr.UISeconds() - mLastRotateSecs;
        if (!mLeaderboardShowing && diff > mRotationOffSecs && mMiniLeaderboard && mMiniLeaderboard->IsReady() && mMiniLeaderboard->HasRows()) {
            mLastRotateSecs = TheTaskMgr.UISeconds();
            static Message msg(set_mini_leaderboard_showing, 0);
            mLeaderboardShowing = true;
            msg[0] = 1;
            HandleType(msg);
#ifdef HX_NATIVE
            // Online + ready: swap the leaderboard group in (env-alpha anim alone
            // doesn't reveal it natively). Only reached when scores enumerate.
            SetMiniLeaderboardGroupShowing(true);
#endif
        } else if (mLeaderboardShowing && diff > mRotationOnSecs) {
            RestartLeaderboardTimer();
        }
    }
}

void SongSelectPanel::RestartLeaderboardTimer() {
    mLastRotateSecs = TheTaskMgr.UISeconds();
    static Message msg(set_mini_leaderboard_showing, 0);
    mLeaderboardShowing = false;
    msg[0] = 0;
    HandleType(msg);
#ifdef HX_NATIVE
    SetMiniLeaderboardGroupShowing(false);
#endif
}

void SongSelectPanel::CancelLeaderboardTimer() {
    mLastRotateSecs = -1.0f;
    static Message msg(set_mini_leaderboard_showing, 0);
    mLeaderboardShowing = false;
    msg[0] = 0;
    HandleType(msg);
#ifdef HX_NATIVE
    SetMiniLeaderboardGroupShowing(false);
#endif
}

BEGIN_HANDLERS(SongSelectPanel)
    HANDLE_EXPR(
        get_leaderboard,
        GetLeaderboard(
            _msg->Obj<LocalBandUser>(2),
            (ScoreType)_msg->Int(3),
            _msg->Int(4),
            (Leaderboard::Mode)_msg->Int(5)
        )
    )
    HANDLE_ACTION(set_to_starting_lb_ix, {
        MILO_ASSERT(mLeaderboard, 0xB6);
        _msg->Obj<UIList>(2)->SetSelected(mLeaderboard->GetStartingRow(), -1);
    })
    HANDLE_ACTION(set_leaderboard_mode, {
        MILO_ASSERT(mLeaderboard, 0xB8);
        mLeaderboard->SetMode((Leaderboard::Mode)_msg->Int(2), true);
    })
    HANDLE_ACTION_IF(
        select_lb_row, mLeaderboard,
        mLeaderboard->OnSelectRow(_msg->Int(2), _msg->Obj<BandUser>(3))
    )
    HANDLE_ACTION(restart_leaderboard_timer, RestartLeaderboardTimer())
    HANDLE_ACTION(cancel_leaderboard_timer, CancelLeaderboardTimer())
    HANDLE_EXPR(scroll_lb_up, mLeaderboard && mLeaderboard->EnumerateLowerRankRange())
    HANDLE_EXPR(scroll_lb_down, mLeaderboard && mLeaderboard->EnumerateHigherRankRange())
    HANDLE_MESSAGE(ButtonDownMsg)
    HANDLE_SUPERCLASS(HeldButtonPanel)
    HANDLE_CHECK(0xC5)
END_HANDLERS