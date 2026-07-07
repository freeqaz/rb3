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
#include "rndobj/Mesh.h" // RndMesh (C2a census)
#include "utl/Std.h" // FOREACH
#include <cstdio>
#include <cstdlib> // getenv (RB3_SS_GRPLOG, C2a census)
#include <cstring> // strstr (C2a census)
static void C2CensusDir(ObjectDir *dir, int depth); // C2a fwd decl
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
    if (getenv("RB3_SS_CENSUS")) {
        fprintf(stderr, "[C2CENSUS] === SongSelectPanel dir census ===\n");
        C2CensusDir(mDir, 0);
        fprintf(stderr, "[C2CENSUS] === end census ===\n");
    }
    // C2a DIAGNOSTIC (RB3_SS_DETAILS_DIAG): force-show the whole song_select_details
    // PanelDir to observe where its difficulty_bg/raitings_bg backing lands relative
    // to the quick-view live_diffs grid. VERDICT (W4.3-C2a): force-showing the panel
    // (whole OR backing-only) produces NO measurable backing behind the quick-view
    // grid (ROI brightness delta <1% = noise). The difficulty_bg*/raitings_bg meshes
    // belong to the details drill-in page — interleaved in basicstars/prostars/rating
    // groups with details-page stars (A10 entanglement) and positioned/animated for
    // the details layout, not the sidebar. They are NOT a valid backing for the
    // quick-view grid. No game-side backing fix ships (see STATUS.md).
    if (getenv("RB3_SS_DETAILS_DIAG")) {
        if (RndDrawable *det = mDir->Find<RndDrawable>("song_select_details", false)) {
            det->SetShowing(true);
            fprintf(stderr, "[C2DIAG] forced song_select_details showing=1\n");
        }
    }
    // C2a DIAGNOSTIC (RB3_SS_LBBG_DIAG): the only main-dir right-side backing
    // candidates (leaderboards_bg.mesh in right_side.grp, help_bg_rating.mesh in
    // right_side_song.grp) are authored showing=0. Test whether force-showing them
    // yields a right-side panel backing co-located with the live_diffs grid.
    if (getenv("RB3_SS_LBBG_DIAG")) {
        static const char *cand[] = { "leaderboards_bg.mesh", "help_bg_rating.mesh",
                                      nullptr };
        for (int i = 0; cand[i]; i++)
            if (RndDrawable *m = mDir->Find<RndDrawable>(cand[i], false)) {
                m->SetShowing(true);
                fprintf(stderr, "[C2DIAG] forced %s showing=1\n", cand[i]);
            }
    }
    // W4.3-C2b4/C2b-ASM fix: album_art.grp (the picture, album_art.pic) sits
    // positioned too far up-left in the native render, so its top edge rides
    // above/into the header row's stats.grp content (gamertag icon +
    // careerscore.scr + careerstars.sd, census W4.3-C2a census.txt:278-282)
    // instead of sitting cleanly below it as in retail
    // (yt_qRagnZCIMzk_song_select_album_art.png).
    //
    // CORRECTED (W4.3-C2b-ASM, supersedes the C2b4 write-up): album_art.grp's
    // own decorative bezel is NOT album_frame01.mesh. Group draw-membership
    // (census.txt:364-375 lists album_frame01.mesh as a member of
    // album_art.grp for show/hide purposes) is a SEPARATE concept from
    // RndTransformable scene-graph parenting (TransParent()) -- confirmed by
    // TransParent-chain walk (RB3_C2B_ASM_DBG): album_frame01.mesh's real
    // parent chain is `header_goals.grp <- header.grp <- all.grp`, i.e. it is
    // an authored sibling of the CAREER header assembly, not a child of
    // album_art.grp at all, despite the grouping. The original single-node
    // -120 Z fix on album_art.grp alone therefore left album_frame01.mesh
    // behind, revealing it as a bare grey ornate bezel with no picture inside
    // (coordinator E1 hold) -- confirmed by hiding album_frame01.mesh
    // (RB3_C2B_ASM_HIDE) and watching the grey box vanish.
    //
    // The two nodes also do NOT share a coordinate frame: album_art.grp's
    // chain (bone_album_group.mesh <- all.grp) rotates local Z into world Y
    // (measured: local z -= 120 -> world y += 107.17, world x/z unchanged),
    // while album_frame01.mesh's chain (header_goals.grp <- header.grp <-
    // all.grp) has no such rotation (local y maps 1:1 to world y; local z
    // maps 1:1 to world z, a depth axis that barely moves screen position for
    // this camera). So "one whole-assembly move" here means applying the
    // SAME WORLD-SPACE delta via each node's own local axis, not literally
    // the same local offset: album_frame01.mesh needs local Y (not Z) to
    // track album_art.grp's local Z.
    //
    // X calibration (fixes the coordinator E1's second note, a new overlap
    // with the left-column score/star readout that the pure-Z move
    // introduced): moving down in this group's rotated frame also carries a
    // small screen-leftward parallax shift (a perspective-projection
    // side-effect of the rotation, not a bug in the offset itself), which
    // pushes the box further left over the "0/10"-style score column text.
    // Adding a matching local X (which maps ~1:1, no rotation, on both nodes)
    // moves the whole assembly right enough to clear it with a clean gap
    // (measured clean at local x=45/album_art.grp, x=41.2/album_frame01.mesh
    // -- the ratio matches album_art.grp's ~0.915 local->world X scale vs
    // album_frame01.mesh's 1:1).
    //
    // Calibrated empirically (frame-count-settled screenshots, NOT wall-clock
    // sleeps -- wall-clock settling gave a NON-monotonic false trend here
    // because the entrance-animation progress at a fixed sleep duration
    // jitters run-to-run; frame-count settling fixed that). getenv-gated,
    // default-OFF pending coordinator sign-off (RB3_SS_ART_YFIX=1 to enable).
    if (getenv("RB3_SS_ART_YFIX")) {
        if (RndTransformable *g = mDir->Find<RndTransformable>("album_art.grp", false)) {
            Transform &t = g->DirtyLocalXfm();
            t.v.x += 45.0f;
            t.v.z -= 120.0f;
        }
        if (RndTransformable *f = mDir->Find<RndTransformable>("album_frame01.mesh", false)) {
            Transform &t = f->DirtyLocalXfm();
            t.v.x += 41.2f;
            t.v.y += 107.17f;
        }
    }
    // W4.3-C2b-ASM EXPERIMENTAL calibration probe
    // (RB3_C2B_ART_NUDGE=<art_x>,<art_z>,<frame_x>,<frame_y>,<frame_z>):
    // album_art.grp's chain (bone_album_group.mesh <- all.grp) rotates local Z
    // into world Y almost entirely (measured: -120 local z -> +107.17 world
    // y, world x/z unchanged), while album_frame01.mesh's chain
    // (header_goals.grp <- header.grp <- all.grp) has NO such rotation (local
    // z maps 1:1 to world z, a depth axis that barely moves screen position
    // for this camera) — the two nodes do NOT share a coordinate frame, so
    // applying the same "-120 local z" to both does not move them together
    // on screen. This probe independently controls both so the correct
    // per-node axis/magnitude pair can be found empirically. Diagnostic only,
    // default-off, harmless to leave in (never reached with the flag unset).
    if (const char *nudge = getenv("RB3_C2B_ART_NUDGE")) {
        float ax = 0.0f, az = 0.0f, fx = 0.0f, fy = 0.0f, fz = 0.0f;
        sscanf(nudge, "%f,%f,%f,%f,%f", &ax, &az, &fx, &fy, &fz);
        if (RndTransformable *g = mDir->Find<RndTransformable>("album_art.grp", false)) {
            g->DirtyLocalXfm().v.x += ax;
            g->DirtyLocalXfm().v.z += az;
        }
        if (RndTransformable *f = mDir->Find<RndTransformable>("album_frame01.mesh", false)) {
            f->DirtyLocalXfm().v.x += fx;
            f->DirtyLocalXfm().v.y += fy;
            f->DirtyLocalXfm().v.z += fz;
        }
        fprintf(
            stderr, "[C2BNUDGE] album_art.grp local += (%.1f,_,%.1f); album_frame01.mesh local += (%.1f,%.1f,%.1f)\n",
            ax, az, fx, fy, fz
        );
    }
    // W4.3-C2b4 (C2b) diagnosis: album_art.pic (top edge overlaps the header
    // row on quick-view) has no C++-set local_xfm anywhere in this file or
    // TexLoadPanel — its position is purely the authored milo xfm of
    // album_art.grp/album_art.pic, a sibling of header.grp/header_song_bg.grp
    // under all.grp (see W4.3-C2a census.txt:12,14,26,27,30). Log world xfm
    // of all four to compare the authored album-vs-header vertical gap
    // against the retail screenshot's gap.
    if (getenv("RB3_C2B_XFM_DBG")) {
        static const char *names[] = { "album_art.grp", "album_art.pic", "header.grp",
                                        "header_song_bg.grp", "all.grp", nullptr };
        for (int i = 0; names[i]; i++) {
            RndTransformable *t = mDir->Find<RndTransformable>(names[i], false);
            if (t) {
                const Transform &wx = t->WorldXfm();
                const Transform &lx = t->LocalXfm();
                fprintf(
                    stderr,
                    "[C2BXFM] %-20s world.v=(%.2f,%.2f,%.2f) local.v=(%.2f,%.2f,%.2f)\n",
                    names[i], wx.v.x, wx.v.y, wx.v.z, lx.v.x, lx.v.y, lx.v.z
                );
            } else {
                fprintf(stderr, "[C2BXFM] %-20s <not found>\n", names[i]);
            }
        }
    }
    // W4.3-C2b-ASM DIAGNOSTIC (RB3_C2B_ASM_DBG): walk the TransParent() chain
    // (NOT group draw-membership) for the revealed grey-bezel candidates plus
    // album_art.grp, to prove the revealed element is parented outside
    // album_art.grp's own chain (binding A7: group membership != trans
    // parenting).
    if (getenv("RB3_C2B_ASM_DBG")) {
        static const char *names[] = { "album_art.grp", "album_art.pic", "album_frame01.mesh",
                                        "bone_album.mesh", "bone_album_group.mesh",
                                        "line_details01.mesh", "song.sbd",
                                        "header_song_shadow.mesh", "album.mesh",
                                        "album_overlay.mesh", "setlist.lst", "album_bg.mesh",
                                        "career_frame.mesh", "career.pic", "career.mesh",
                                        "goal_desc.lbl", "goal_title.lbl",
                                        "header_goals.grp", "header.grp",
                                        "stats.grp", "all.grp", nullptr };
        for (int i = 0; names[i]; i++) {
            RndTransformable *t = mDir->Find<RndTransformable>(names[i], false);
            if (!t) {
                fprintf(stderr, "[C2BASM] %-20s <not found>\n", names[i]);
                continue;
            }
            const Transform &wx = t->WorldXfm();
            fprintf(stderr, "[C2BASM] %-20s world.v=(%.2f,%.2f,%.2f) chain:", names[i],
                    wx.v.x, wx.v.y, wx.v.z);
            RndTransformable *p = t;
            for (int depth = 0; p && depth < 12; depth++) {
                Hmx::Object *po = dynamic_cast<Hmx::Object *>(p);
                fprintf(stderr, " %s", po ? po->Name() : "?");
                p = p->TransParent();
                if (p)
                    fprintf(stderr, " <-");
            }
            fprintf(stderr, "\n");
        }
    }
    // W4.3-C2b-ASM DIAGNOSTIC (RB3_C2B_ASM_HIDE=<obj name>): force-hide one
    // named RndDrawable, to empirically confirm/refute a revealed-element
    // candidate by A/B screenshot (does the grey bezel disappear when this
    // node is hidden?).
    if (const char *hide = getenv("RB3_C2B_ASM_HIDE")) {
        if (RndDrawable *d = mDir->Find<RndDrawable>(hide, false)) {
            d->SetShowing(false);
            fprintf(stderr, "[C2BASM] forced %s showing=0\n", hide);
        } else {
            fprintf(stderr, "[C2BASM] %s <not found, not hidden>\n", hide);
        }
    }
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

// C2a census (RB3_SS_CENSUS): full recursive walk of the panel dir + every
// inlined subdir, logging each drawable's name/class/showing plus the group
// membership of the backing meshes. Answers never-submitted vs
// submitted-and-dropped for difficulty_bg*/raitings_bg/details_background, and
// gives the A10 sub-panel content census (song_select_details inlined subdir).
static bool C2NameOfInterest(const char *n) {
    if (!n)
        return false;
    static const char *keys[] = { "bg",       "background", "rating",  "raiting",
                                  "difficulty", "detail",   "occlud",  "highlight",
                                  "album",    "star",       "perf",    "rank",
                                  "header",   "backing",    nullptr };
    for (int i = 0; keys[i]; i++)
        if (strstr(n, keys[i]))
            return true;
    return false;
}

static void C2CensusDir(ObjectDir *dir, int depth) {
    if (!dir)
        return;
    char ind[24];
    int n = depth * 2;
    if (n > 20)
        n = 20;
    for (int i = 0; i < n; i++)
        ind[i] = ' ';
    ind[n] = 0;
    const char *pn = dir->GetPathName();
    fprintf(
        stderr, "[C2DIR]%s DIR '%s' isSubDir=%d nSub=%d\n", ind, pn ? pn : "?",
        (int)dir->IsSubDir(), (int)dir->mSubDirs.size()
    );
    // objects directly in THIS dir (no subdir recursion: b=false)
    for (ObjDirItr<Hmx::Object> it(dir, false); it; ++it) {
        Hmx::Object *o = it;
        if (!o)
            continue;
        RndGroup *g = dynamic_cast<RndGroup *>(o);
        if (g) {
            fprintf(
                stderr, "[C2OBJ]%s   GRP %s showing=%d nmemb=%d\n", ind, o->Name(),
                (int)g->Showing(), g->mObjects.size()
            );
            FOREACH (mit, g->mObjects) {
                Hmx::Object *m = *mit;
                fprintf(
                    stderr, "[C2GRPMEMB]%s     <%s> %s (%s)\n", ind, o->Name(),
                    m ? m->Name() : "<null>", m ? m->ClassName().Str() : "?"
                );
            }
            continue;
        }
        // Nested PanelDir/ObjectDir object (e.g. song_select_details) — its
        // content is NOT in mSubDirs; recurse in to census difficulty_bg etc.
        ObjectDir *nested = dynamic_cast<ObjectDir *>(o);
        if (nested && nested != dir) {
            RndDrawable *nd = dynamic_cast<RndDrawable *>(o);
            fprintf(
                stderr, "[C2OBJ]%s   NESTED-DIR %s (%s) showing=%d\n", ind, o->Name(),
                o->ClassName().Str(), nd ? (int)nd->Showing() : -1
            );
            C2CensusDir(nested, depth + 1);
            continue;
        }
        RndDrawable *d = dynamic_cast<RndDrawable *>(o);
        bool interest = C2NameOfInterest(o->Name());
        if (d && (interest || dynamic_cast<RndMesh *>(o))) {
            fprintf(
                stderr, "[C2OBJ]%s   DRW %s (%s) showing=%d\n", ind, o->Name(),
                o->ClassName().Str(), (int)d->Showing()
            );
        } else if (interest) {
            fprintf(
                stderr, "[C2OBJ]%s   OBJ %s (%s)\n", ind, o->Name(),
                o->ClassName().Str()
            );
        }
    }
    for (int i = 0; i < (int)dir->mSubDirs.size(); i++)
        C2CensusDir(dir->mSubDirs[i], depth + 1);
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
#ifdef HX_NATIVE
    // W4.3-C2b4 (C2b): re-log every Poll (not just FinishLoad) so the values
    // reflect the settled post-entrance-animation position, not a possibly
    // mid-transition-in snapshot taken right after load.
    if (getenv("RB3_C2B_XFM_DBG")) {
        static const char *names[] = { "album_art.grp", "album_art.pic", "header.grp",
                                        "header_song_bg.grp", "all.grp", nullptr };
        for (int i = 0; names[i]; i++) {
            RndTransformable *t = mDir->Find<RndTransformable>(names[i], false);
            if (t) {
                const Transform &wx = t->WorldXfm();
                fprintf(
                    stderr, "[C2BXFMPOLL] %-20s world.v=(%.2f,%.2f,%.2f)\n", names[i],
                    wx.v.x, wx.v.y, wx.v.z
                );
            }
        }
    }
#endif
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