#include "track/TrackDir.h"
#ifdef HX_NATIVE
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include "rb3_native_settings.h"
// Number of in-use gem tracks for the current game, published by
// TrackPanelDir::ConfigureTracks. Used by the CAMERA_FRAME_FIX to only center
// the camera in the single-player layout (multi-track wants the fan-out).
int gHxNativeNumUsedGemTracks = 0;
#endif
#include "compiler_macros.h"
#include "decomp.h"
#include "obj/ObjMacros.h"
#include "obj/Task.h"
#include "rndobj/Cam.h"
#include "rndobj/Env.h"
#include "ui/PanelDir.h"
#include "utl/Loader.h"
#include "rndobj/Group.h"
#include "track/TrackTest.h"
#include "track/TrackWidget.h"
#include "obj/ObjVersion.h"
#include "utl/Symbols.h"

INIT_REVS(TrackDir)

TrackDir::TrackDir()
    : mRunning(!LOADMGR_EDITMODE), mDrawGroup(this), mAnimGroup(this), mYPerSecond(10.0f),
      mTopY(10.0f), mBottomY(-3.0f), mWarnOnResort(false), mShowingWhenEnabled(this),
      mStationaryBack(this), mKeyShiftStationaryBack(this),
      mStationaryBackAfterKeyShift(this), mMovingBack(this), mKeyShiftMovingBack(this),
      mKeyShiftStationaryMiddle(this), mStationaryMiddle(this), mMovingFront(this),
      mKeyShiftMovingFront(this), mKeyShiftStationaryFront(this), mStationaryFront(this),
      mAlwaysShowing(this), mRotatorCam(this), mTrack(this), mTrackGems(this),
      unk368(1.0f)
#ifdef MILO_DEBUG
      ,
      mTest(new TrackTest(this))
#endif
{
    mActiveWidgets.reserve(50);
    unk2d8.Reset();
    unk308.Reset();
    unk338.Reset();
}

TrackDir::~TrackDir() {
#ifdef MILO_DEBUG
    delete mTest;
#endif
}

// fn_8053E7D4
void TrackDir::SyncObjects() {
    PanelDir::SyncObjects();
    for (ObjDirItr<TrackWidget> it(this, true); it != nullptr; ++it) {
        it->Init();
        it->Clear();
    }
    if (!mTrack)
        mTrack = Find<RndEnviron>("track.env", false);
    if (!mTrackGems)
        mTrackGems = Find<RndEnviron>("track_gems.env", false);
    if (!mShowingWhenEnabled)
        mShowingWhenEnabled = Find<RndGroup>("showing_when_enabled.grp", false);
    if (!mStationaryBack)
        mStationaryBack = Find<RndGroup>("stationary_back.grp", false);
    if (!mKeyShiftStationaryBack)
        mKeyShiftStationaryBack = Find<RndGroup>("key_shift_stationary_back.grp", false);
    if (!mStationaryBackAfterKeyShift)
        mStationaryBackAfterKeyShift =
            Find<RndGroup>("stationary_back_after_key_shift.grp", false);
    if (!mMovingBack)
        mMovingBack = Find<RndGroup>("moving_back.grp", false);
    if (!mKeyShiftMovingBack)
        mKeyShiftMovingBack = Find<RndGroup>("key_shift_moving_back.grp", false);
    if (!mKeyShiftStationaryMiddle)
        mKeyShiftStationaryMiddle =
            Find<RndGroup>("key_shift_stationary_middle.grp", false);
    if (!mStationaryMiddle)
        mStationaryMiddle = Find<RndGroup>("stationary_middle.grp", false);
    if (!mMovingFront)
        mMovingFront = Find<RndGroup>("moving_front.grp", false);
    if (!mKeyShiftMovingFront)
        mKeyShiftMovingFront = Find<RndGroup>("key_shift_moving_front.grp", false);
    if (!mKeyShiftStationaryFront)
        mKeyShiftStationaryFront =
            Find<RndGroup>("key_shift_stationary_front.grp", false);
    if (!mStationaryFront)
        mStationaryFront = Find<RndGroup>("stationary_front.grp", false);
    if (!mAlwaysShowing)
        mAlwaysShowing = Find<RndGroup>("always_showing.grp", false);
}

void TrackDir::SetupKeyShifting(RndDir *rnddir) {
    mRotatorCam = rnddir->Find<RndTransformable>("rotator_cam.trans", true);
    float order = 1 / rnddir->mLocalXfm.m.x.x;
    unk2d8.Reset();
    Scale(Vector3(order, 1.0f, 1.0f), unk2d8.m, unk2d8.m);
    unk308.Reset();
    unk308.v = mRotatorCam->WorldXfm().v;
    unk338.Reset();
    Invert(unk308, unk338);
}

void TrackDir::ResetKeyShifting() {
    mRotatorCam = 0;
    unk2d8.Reset();
    unk308.Reset();
    unk338.Reset();
}

BEGIN_COPYS(TrackDir)
    COPY_SUPERCLASS(PanelDir)
    CREATE_COPY(TrackDir)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mDrawGroup)
        COPY_MEMBER(mAnimGroup)
        COPY_MEMBER(mYPerSecond)
        COPY_MEMBER(mTopY)
        COPY_MEMBER(mBottomY)
        COPY_MEMBER(mSlots)
        COPY_MEMBER(mWarnOnResort)
#ifdef MILO_DEBUG
        COPY_MEMBER(mTest->mWidget)
        COPY_MEMBER(mTest->mSlot)
#endif
    END_COPYING_MEMBERS
END_COPYS

SAVE_OBJ(TrackDir, 0x90)

BEGIN_LOADS(TrackDir)
    PreLoad(bs);
    PostLoad(bs);
END_LOADS

void TrackDir::PreLoad(BinStream &bs) {
    LOAD_REVS(bs);
    ASSERT_REVS(6, 0);
    PushRev(packRevs(gAltRev, gRev), this);
    PanelDir::PreLoad(bs);
}

void TrackDir::PostLoad(BinStream &bs) {
    PanelDir::PostLoad(bs);
    int revs = PopRev(this);
    gRev = getHmxRev(revs);
    gAltRev = getAltRev(revs);
    if (!IsProxy()) {
        if (gRev != 0) {
            bs >> mDrawGroup;
            if (gRev > 1)
                bs >> mAnimGroup;
            bs >> mYPerSecond >> mTopY >> mBottomY;
        }
        if (gRev > 2) {
            if (gRev > 5)
                bs >> mSlots;
            else {
                unsigned int num = 0;
                bs >> num;
                mSlots.resize(num);
                for (int i = 0; i < num; i++) {
                    mSlots[i].Reset();
                    bs >> mSlots[i].v.x >> mSlots[i].v.z;
                }
            }
        }
        if (gRev > 4)
            bs >> mWarnOnResort;
#ifdef MILO_DEBUG
        if (gRev > 3) {
            bs >> mTest->mWidget >> mTest->mSlot;
        }
#else
        if (gRev > 3) {
            String s;
            int x;
            bs >> s >> x;
        }
#endif
    }
    for (ObjDirItr<TrackWidget> it(this, true); it != nullptr; ++it) {
        it->SetTrackDir(this);
    }
}

void TrackDir::DrawShowing() {
    if (IsEnabled()) {
        if (mRunning || IsProxy()) {
            Vector3 v148;
            v148.Zero();
            RndEnvironTracker tracker(mTrack, &v148);
            RndCam *i7 = nullptr;
            RndCam *cur;
            RndCam *i6 = GetCam();
            if (i6) {
                cur = RndCam::sCurrent;
                i6->Select();
                i7 = cur;
            } else {
                MILO_ASSERT(TheLoadMgr.EditMode(), 0x104);
                i6 = RndCam::sCurrent;
            }
#ifdef HX_NATIVE
            // CAMERA_FRAME_FIX: center the gameplay camera. The milo
            // "N_player_<aspect>" configuration apply-handler that should call
            // set_track_offset/set_side_angle (= 0 for a lone player, zeroing
            // rotater.grp's lateral fan-out) does not execute in the native
            // port, so the camera keeps the authored MULTI-player default —
            // rotater.grp at x=-34.5 with a side-angle — which swings game.cam
            // off-axis and renders the highway edge-on at the right frame edge.
            //
            // i6 is the EXACT camera this proxy track renders through, so walk
            // up its parent chain to the rotater.grp genuinely in the chain and
            // zero the lateral offset, leaving the authored down-the-highway
            // pitch/height intact. Done once per camera object (the rig is
            // shared/static across frames). Skipped if this is a multi-track
            // game (the fan-out is wanted there).
            {
                extern int gHxNativeNumUsedGemTracks;  // set in TrackPanelDir
                if (i6 && gHxNativeNumUsedGemTracks == 1) {
                    // The per-player FAN-OUT groups (rotater.grp lateral offset +
                    // rotater_roll.grp side-yaw/roll) carry the milo's authored
                    // MULTI-player default; the lone-player apply-handler that
                    // would zero them (set_track_offset/set_side_angle 0) never
                    // runs natively, so game.cam ends up off-axis. Neutralize
                    // them on the groups genuinely in the rendered camera's
                    // chain, preserving the base down-the-highway pitch (which
                    // lives in scaler.grp + rotater.grp's rotation). Re-applied
                    // every frame because the proxy/rig re-syncs from the
                    // template. The lateral centering offset (rotater.grp local
                    // x) defaults to -4.0 — empirically the value that centres
                    // the guitar surface. Read LIVE from NativeSettings.camRotX
                    // (seeded once at startup from the CAM_ROTX env var, then
                    // mutable at runtime via the HTTP /api/settings endpoint) so
                    // re-tuning takes effect on the next frame with no rebuild.
                    const float sRotX = TheNativeSettings().camRotX;
                    for (RndTransformable *t = i6; t; t = t->TransParent()) {
                        const char *tn = t->Name();
                        if (!tn) continue;
                        if (strcmp(tn, "rotater.grp") == 0) {
                            Transform lx(t->LocalXfm());
                            if (lx.v.x != sRotX) {
                                lx.v.x = sRotX;
                                t->SetLocalXfm(lx);
                            }
                        } else if (strcmp(tn, "rotater_roll.grp") == 0) {
                            // Per-player camera ROLL group; for a lone player the
                            // apply-handler leaves it un-rolled. Authored default
                            // carries a roll/yaw that tilts the highway off
                            // center — neutralize its rotation (it has no
                            // translation).
                            const Transform &rr = t->LocalXfm();
                            if (rr.m.x.x != 1.0f || rr.m.y.y != 1.0f) {
                                Transform lx(rr);
                                lx.m.Identity();
                                t->SetLocalXfm(lx);
                            }
                        }
                    }
                    // The authored multi-player viewport offset (screenRect.x,
                    // ~0.235) shifts the lone player's view to the right of
                    // frame; set_screen_rect_x 0 from the apply-handler would
                    // clear it. Reset it on the rendered camera directly.
                    if (i6->mScreenRect.x != 0.0f) {
                        i6->mScreenRect.x = 0.0f;
                        i6->UpdateLocal();
                    }
                }
            }
            // CAM_DBG: dump the gameplay-camera (game.cam) pose during the
            // highway draw. The per-frame render trace reports world.cam
            // (origin) because TrackDir restores the prior cam after this
            // scope; this is the only point at which game.cam is actually
            // current. Set CAM_DBG=1 to enable.
            if (getenv("CAM_DBG")) {
                static int sCamDbg = 0;
                if ((sCamDbg++ % 60) == 0 && i6) {
                    const Transform &wx = i6->WorldXfm();
                    fprintf(stderr,
                            "CAM_DBG: dir='%s' cam='%s' pos=(%.2f,%.2f,%.2f) "
                            "fwd=(%.3f,%.3f,%.3f) up=(%.3f,%.3f,%.3f) "
                            "yfov=%.3f near=%.1f far=%.1f\n",
                            Name() ? Name() : "?",
                            i6->Name() ? i6->Name() : "?",
                            wx.v.x, wx.v.y, wx.v.z,
                            wx.m.y.x, wx.m.y.y, wx.m.y.z,
                            wx.m.z.x, wx.m.z.y, wx.m.z.z,
                            i6->mYFov, i6->mNearPlane, i6->mFarPlane);
                }
            }
#endif
            PreDraw();
            if (mShowingWhenEnabled->Showing()) {
                Transform tf50(i6->WorldXfm());
                float mult = mYPerSecond * TheTaskMgr.Seconds(TaskMgr::kRealTime);
#ifdef HX_NATIVE
                if (getenv("CLOCK_DBG")) {
                    static int sCDbg = 0;
                    if ((sCDbg++ % 60) == 0)
                        MILO_LOG("CLOCK_DBG: TrackDir::DrawShowing rt=%.3f mult=%.2f yps=%.2f\n",
                                 TheTaskMgr.Seconds(TaskMgr::kRealTime), mult, mYPerSecond);
                }
#endif
                Transform tf80(tf50);
                tf80.v.y += mult;
                bool b2 = (mKeyShiftStationaryBack->Showing() && mRotatorCam);
                Transform tfb0;
                Transform tfe0;
                if (b2) {
                    Transform tf110;
                    tf110.v.Zero();
                    tf110.m = mRotatorCam->WorldXfm().m;
                    tfb0 = tf50;
                    Multiply(tfb0, unk2d8, tfb0);
                    Multiply(tfb0, unk338, tfb0);
                    Multiply(tfb0, tf110, tfb0);
                    Multiply(tfb0, unk308, tfb0);
                    tfe0 = tfb0;
                    tfe0.v.y += mult;
                }
                mStationaryBack->DrawShowing();
                if (b2) {
                    i6->SetWorldXfm(tfb0);
                    i6->Select();
                    mKeyShiftStationaryBack->DrawShowing();
                    i6->SetWorldXfm(tf50);
                    i6->Select();
                }
                mStationaryBackAfterKeyShift->DrawShowing();
                i6->SetWorldXfm(tf80);
                i6->Select();
                mMovingBack->DrawShowing();
                if (b2) {
                    i6->SetWorldXfm(tfe0);
                    i6->Select();
                    mKeyShiftMovingBack->DrawShowing();
                    i6->SetWorldXfm(tfb0);
                    i6->Select();
                    mKeyShiftStationaryMiddle->DrawShowing();
                }
                i6->SetWorldXfm(tf50);
                i6->Select();
                mStationaryMiddle->DrawShowing();
                {
                    RndEnvironTracker tracker2(mTrackGems, &v148);
                    if (!b2) {
                        i6->SetWorldXfm(tf80);
                        i6->Select();
                        mMovingFront->DrawShowing();
                    } else {
                        i6->SetWorldXfm(tfe0);
                        i6->Select();
                        mKeyShiftMovingFront->DrawShowing();
                    }
                }
                if (b2) {
                    i6->SetWorldXfm(tfb0);
                    i6->Select();
                    mKeyShiftStationaryFront->DrawShowing();
                }
                i6->SetWorldXfm(tf50);
                i6->Select();
                mStationaryFront->DrawShowing();
            }
            mAlwaysShowing->DrawShowing();
            PostDraw();
            if (i7) {
                i7->Select();
            }
        } else
            PanelDir::DrawShowing();
    }
}

void TrackDir::Poll() {
    if (IsEnabled()) {
        RndDir::Poll();
        float secs = mYPerSecond * TheTaskMgr.Seconds(TaskMgr::kRealTime);
        if (mAnimGroup && mRunning) {
            mAnimGroup->SetFrame(secs, 1.0f);
        }
        PollActiveWidgets();
    }
}

void TrackDir::PollActiveWidgets() {
    int count = 0;
    for (int i = 0; i < mActiveWidgets.size(); i++) {
        TrackWidget *widget = mActiveWidgets[i];
        MILO_ASSERT(widget, 0x1B3);
        if (!widget->Empty())
            widget->Poll();
        if (widget->Empty()) {
            widget->SetInactive();
            count++;
            mActiveWidgets[i] = 0;
        } else {
            if (count > 0) {
                mActiveWidgets[i - count] = widget;
                mActiveWidgets[i] = 0;
            }
        }
    }
    if (count > 0) {
        mActiveWidgets.resize(mActiveWidgets.size() - count);
    }
}

float TrackDir::TopSeconds() const { return mTopY / mYPerSecond; }

float TrackDir::BottomSeconds() const { return mBottomY / mYPerSecond; }

FORCE_LOCAL_INLINE
float TrackDir::SecondsToY(float f) const { return f * mYPerSecond; }
END_FORCE_LOCAL_INLINE

float TrackDir::YToSeconds(float f) const { return f / mYPerSecond; }

float TrackDir::CutOffY() const {
    if (LOADMGR_EDITMODE) {
        return mBottomY;
    } else {
        float secs = TheTaskMgr.Seconds(TaskMgr::kRealTime);
        float bias = SecondsToY(secs);
        return mBottomY + bias;
    }
}

// fn_8053FC48
void TrackDir::SetSlotXfm(int i, const Transform &tf) {
    if (i >= mSlots.size()) {
        Transform t48;
        t48.Reset();
        while (i >= mSlots.size())
            mSlots.push_back(t48);
    } else {
        if (i >= vec2.size()) {
            Transform t78;
            t78.Reset();
            while (i >= vec2.size())
                vec2.push_back(t78);
        }
        vec2[i] = mSlots[i];
    }
    mSlots[i] = tf;
}

void TrackDir::MakeSecondsXfm(float secs, Transform &tf) const {
    tf.Reset();
    tf.v.y = SecondsToY(secs);
}

void TrackDir::MakeWidgetXfm(int slot, float secs, Transform &tf) const {
    MakeSlotXfm(slot, tf);
    tf.v.y = SecondsToY(secs);
}

void TrackDir::MakeSlotXfm(int slot, Transform &tf) const {
    MILO_ASSERT(slot < mSlots.size(), 0x220);
    tf = mSlots[slot];
}

DECOMP_FORCEACTIVE(TrackDir, "gem_mash", "drum_mash")

void TrackDir::SetScrollSpeed(float f) {
    if (f > 0)
        mYPerSecond = mTopY / f;
}

float TrackDir::ViewTimeSeconds() const {
    if (mYPerSecond > 0)
        return mTopY / mYPerSecond;
    else
        return 0;
}

void TrackDir::ClearAllWidgets() {
    for (ObjDirItr<TrackWidget> it(this, true); it != nullptr; ++it) {
        it->Clear();
    }
}

// fn_8053FED4
void TrackDir::ClearAllGemWidgets() {
    for (ObjDirItr<TrackWidget> it(this, true); it != nullptr; ++it) {
        if (strncmp(it->Name(), "gem_", 4) == 0)
            it->Clear();
        else if (strncmp(it->Name(), "drum_", 5) == 0)
            it->Clear();
        else if (strncmp(it->Name(), "rg_", 3) == 0)
            it->Clear();
        else if (strncmp(it->Name(), "real_", 5) == 0)
            it->Clear();
        else if (strncmp(it->Name(), "fret_", 5) == 0)
            it->Clear();
        else if (strncmp(it->Name(), "chord_", 5) == 0)
            it->Clear();
    }
}

void TrackDir::ToggleRunning() { SetRunning(!mRunning); }

void TrackDir::SetRunning(bool running) {
    if (mRunning && !running) {
        mAnimGroup->SetFrame(0, 1);
    }
    mRunning = running;
}

void TrackDir::AddTestWidget(TrackWidget *widget, int slot) {
    if (!mRunning)
        MILO_WARN("Track is not running");
    else if (!widget)
        MILO_WARN("No test widget selected");
    else if (slot >= mSlots.size())
        MILO_WARN("Can't add widget on slot %i, only %i slots", slot, mSlots.size());
    else {
        Transform tf;
        MakeWidgetXfm(slot, TheTaskMgr.Seconds(TaskMgr::kRealTime), tf);
        tf.v.y += mTopY;
        widget->AddInstance(tf, 0);
    }
}

void TrackDir::AddActiveWidget(TrackWidget *widget) {
    std::vector<TrackWidget *>::iterator it =
        std::find(mActiveWidgets.begin(), mActiveWidgets.end(), widget);
    MILO_ASSERT(it == mActiveWidgets.end(), 0x2A5);
    if (mActiveWidgets.size() == mActiveWidgets.capacity()
        && mActiveWidgets.size() == mActiveWidgets.capacity()) {
        MILO_FAIL(
            "Number of active widgets exceeds capacity %d", mActiveWidgets.capacity()
        );
    }
    mActiveWidgets.push_back(widget);
}

BEGIN_HANDLERS(TrackDir)
    HANDLE_ACTION(toggle_running, ToggleRunning())
    HANDLE_ACTION(add_test_widget, AddTestWidget(_msg->Obj<TrackWidget>(2), _msg->Int(3)))
    HANDLE_ACTION(clear_all, ClearAllWidgets())
    HANDLE_SUPERCLASS(PanelDir)
    HANDLE_CHECK(699)
END_HANDLERS

#include "utl/ClassSymbols.h"

BEGIN_PROPSYNCS(TrackDir)
    SYNC_PROP(draw_group, mDrawGroup)
    SYNC_PROP(anim_group, mAnimGroup)
    SYNC_PROP(y_per_second, mYPerSecond)
    SYNC_PROP(top_y, mTopY)
    SYNC_PROP(bottom_y, mBottomY)
    SYNC_PROP(slots, mSlots)
    SYNC_PROP(warn_on_resort, mWarnOnResort)
#ifdef MILO_DEBUG
    SYNC_PROP(TrackTesting, *mTest)
#endif
    SYNC_SUPERCLASS(PanelDir)
END_PROPSYNCS
