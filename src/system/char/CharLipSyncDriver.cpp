#include "char/CharLipSyncDriver.h"
#include "char/Char.h"
#include "char/CharClip.h"
#include "char/CharFaceServo.h"
#include "char/CharLipSync.h"
#include "char/CharWeightable.h"
#include "obj/Dir.h"
#include "obj/ObjMacros.h"
#include "obj/Object.h"
#include "obj/Task.h"
#include "os/Timer.h"
#include "rndobj/Poll.h"
#include "rndobj/Rnd.h"
#include "utl/Loader.h"
#include "utl/Symbols.h"
#include "utl/Symbols2.h"
#include "utl/Symbols4.h"
#include <cmath>

INIT_REVS(CharLipSyncDriver)

CharLipSyncDriver::CharLipSyncDriver()
    : mLipSync(this), mClips(this), mBlinkClip(this), mSongOwner(this), mSongOffset(0.0f),
      mLoop(0), mSongPlayer(0), mBones(this), mTestClip(this), mTestWeight(1.0f),
      mOverrideClip(this), mOverrideWeight(0.0f), mOverrideOptions(this),
      mApplyOverrideAdditively(0), mAlternateDriver(this) {}

CharLipSyncDriver::~CharLipSyncDriver() { RELEASE(mSongPlayer); }

void CharLipSyncDriver::Highlight() {
#ifdef MILO_DEBUG
    float highlightY = gCharHighlightY;
    if (-1.0f == highlightY) {
        CharDeferHighlight(this);
    } else {
        Hmx::Color white(1, 1, 1);
        Vector2 v2(5.0f, highlightY);
        float y =
            TheRnd->DrawString(MakeString("%s:", PathName(this)), v2, white, true).y;
        v2.y += y;
        if (mSongPlayer) {
            TheRnd->DrawString(
                MakeString("frame %d", mSongPlayer->mFrame), v2, white, true
            );
            v2.y += y;
            std::vector<CharLipSync::PlayBack::Weight> &weights = mSongPlayer->mWeights;
            for (int i = 0; i < weights.size(); i++) {
                CharLipSync::PlayBack::Weight &curWeight = weights[i];
                float f14 = curWeight.unk14;
                CharClip *clip = curWeight.unk0;
                if (f14 != 0 && clip) {
                    TheRnd->DrawString(
                        MakeString("%s %.4f", clip->Name(), f14), v2, white, true
                    );
                    v2.y += y;
                }
            }
        }
        gCharHighlightY = v2.y + y;
    }
#endif
}

void CharLipSyncDriver::Enter() {
    RndPollable::Enter();
    mOverrideWeight = 0;
    if (mLipSync)
        Sync();
}

void CharLipSyncDriver::SetClips(ObjectDir *clipDir) {
    mClips = clipDir;
    Sync();
}

void CharLipSyncDriver::SetLipSync(CharLipSync *sync) {
    if (sync != mLipSync) {
        mLipSync = sync;
        mLoop = false;
        mSongOffset = 0;
        Sync();
    }
}

void CharLipSyncDriver::Sync() {
    if (mClips) {
        mBlinkClip = mClips->Find<CharClip>("Blink", false);
    } else
        mBlinkClip = nullptr;
    RELEASE(mSongPlayer);
    if (mLipSync && mClips) {
        mSongPlayer = new CharLipSync::PlayBack();
        mSongPlayer->Set(mLipSync, mClips);
        mSongPlayer->Reset();
    }
}

void CharLipSyncDriver::ScaleAddViseme(CharClip *clip, float weight) {
    float fmodResult;
    if (clip->LengthSeconds() != 0.0f) {
        fmodResult = std::fmod(TheTaskMgr.Seconds(TaskMgr::kRealTime), clip->LengthSeconds());
    } else {
        fmodResult = 0.0f;
    }
    float beat = clip->FrameToBeat(fmodResult * clip->mFramesPerSec);
    mBones->ScaleAdd(clip, weight, beat, 0.0f);
}

void CharLipSyncDriver::Poll() {
    START_AUTO_TIMER("lipsyncdriver");
    if (!mClips || !mBones)
        return;
    if (mTestClip && LOADMGR_EDITMODE) {
        if (!mTestClip->Relative() || mTestWeight < 0.0f)
            return;
        mBones->ScaleAdd(mTestClip, mTestWeight, mTestClip->StartBeat(), 0.0f);
        return;
    }
    if (mLipSync) {
        {
        float weight = Weight();
        if (mOverrideClip && !mApplyOverrideAdditively) {
        if (mOverrideWeight > 0.0f) {
        weight *= 1.0f - mOverrideWeight;
        }
        }
        if (mSongPlayer && mOverrideClip && mOverrideWeight > 0.0f) {
        ScaleAddViseme(mOverrideClip, mOverrideWeight);
        }
        if (weight == 0.0)
        return;
        if (mSongPlayer) {
        float songTime = TheTaskMgr.Seconds(TaskMgr::kRealTime) + mSongOffset;
        if (mLoop) {
        float duration = mSongPlayer->mLipSync->Duration() - 0.001f;
        if (duration == 0.0f) {
        songTime = 0.0f;
        } else {
        songTime = std::fmod(songTime, duration);
        if (songTime < 0.0f)
        songTime += duration;
        }
        }
        if (mAlternateDriver)
        songTime = mAlternateDriver->TopClipFrame();
        mSongPlayer->Poll(songTime);
        unsigned int count = mSongPlayer->mWeights.size();
        for (unsigned int i = 0; i < count; i++) {
        float curWeight = mSongPlayer->mWeights[i].unk14;
        if (curWeight != 0.0f) {
        CharClip *clip = mSongPlayer->mWeights[i].unk0;
        if (clip != mBlinkClip) {
        if (mSongOwner)
        curWeight = 0.0f;
        else
        curWeight *= weight;
        }
        if (clip && curWeight != 0.0f) {
        ScaleAddViseme(clip, curWeight);
        }
        }
        }
        }
        if (mSongOwner && mSongOwner->mSongPlayer) {
        float songTime = TheTaskMgr.Seconds(TaskMgr::kRealTime) + mSongOwner->mSongOffset;
        if (mLoop) {
        float duration = mSongOwner->mSongPlayer->mLipSync->Duration() - 0.001f;
        if (duration == 0.0f) {
        songTime = 0.0f;
        } else {
        songTime = std::fmod(songTime, duration);
        if (songTime < 0.0f)
        songTime += duration;
        }
        }
        mSongOwner->mSongPlayer->Poll(songTime);
        CharLipSync::PlayBack *pb = mSongOwner->mSongPlayer;
        unsigned int count = pb->mWeights.size();
        for (unsigned int i = 0; i < count; i++) {
        float curWeight = weight * pb->mWeights[i].unk14;
        CharClip *clip = pb->mWeights[i].unk0;
        if (curWeight != 0.0f && clip && clip != mSongOwner->mBlinkClip) {
        CharClip *remapped = mClips->Find<CharClip>(clip->Name(), true);
        ScaleAddViseme(remapped, curWeight);
        }
        }
        }
        }
    }
    {
        CharFaceServo *servo = dynamic_cast<CharFaceServo *>(mBones.Ptr());
        if (servo)
            servo->ApplyProceduralWeights();
    }
}

void CharLipSyncDriver::PollDeps(
    std::list<Hmx::Object *> &changedBy, std::list<Hmx::Object *> &change
) {
    change.push_back(mBones);
}

SAVE_OBJ(CharLipSyncDriver, 0x111)

BEGIN_LOADS(CharLipSyncDriver)
    LOAD_REVS(bs)
    ASSERT_REVS(7, 0)
    LOAD_SUPERCLASS(Hmx::Object)
    LOAD_SUPERCLASS(CharWeightable)
    bs >> mBones;
    bs >> mClips;
    if (gRev < 1) {
        FilePath fp;
        bs >> fp;
        MILO_WARN("%s old version, won't load %s", PathName(this), fp);
        String str;
        bs >> str;
    } else
        bs >> mLipSync;
    if (gRev > 1) {
        mTestClip.Load(bs, true, mClips);
        bs >> mTestWeight;
    }
    if (gRev > 2) {
        mOverrideClip.Load(bs, true, mClips);
        if (gRev < 5) {
            int x;
            bs >> x;
        }
        bs >> mOverrideOptions;
    }
    if (gRev > 3)
        bs >> mApplyOverrideAdditively;
    if (gRev > 5)
        bs >> mOverrideWeight;
    if (gRev > 6)
        bs >> mAlternateDriver;
    Sync();
END_LOADS

BEGIN_COPYS(CharLipSyncDriver)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(CharWeightable)
    CREATE_COPY(CharLipSyncDriver)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mBones)
        COPY_MEMBER(mClips)
        COPY_MEMBER(mLipSync)
        COPY_MEMBER(mBlinkClip)
        COPY_MEMBER(mSongOffset)
        COPY_MEMBER(mLoop)
        COPY_MEMBER(mSongOwner)
        COPY_MEMBER(mTestClip)
        COPY_MEMBER(mTestWeight)
        COPY_MEMBER(mOverrideWeight)
        COPY_MEMBER(mOverrideClip)
        COPY_MEMBER(mOverrideOptions)
        COPY_MEMBER(mApplyOverrideAdditively)
        COPY_MEMBER(mAlternateDriver)
    END_COPYING_MEMBERS
END_COPYS

BEGIN_HANDLERS(CharLipSyncDriver)
    HANDLE_ACTION(resync, Sync())
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0x185)
END_HANDLERS

BEGIN_PROPSYNCS(CharLipSyncDriver)
    SYNC_PROP(bones, mBones)
    SYNC_PROP_SET(clips, mClips, SetClips(_val.Obj<ObjectDir>()))
    SYNC_PROP_SET(lipsync, mLipSync, SetLipSync(_val.Obj<CharLipSync>()))
    SYNC_PROP(song_owner, mSongOwner)
    SYNC_PROP(loop, mLoop)
    SYNC_PROP(song_offset, mSongOffset)
    SYNC_PROP(test_clip, mTestClip)
    SYNC_PROP(test_weight, mTestWeight)
    SYNC_PROP(override_clip, mOverrideClip)
    SYNC_PROP(override_weight, mOverrideWeight)
    SYNC_PROP(override_options, mOverrideOptions)
    SYNC_PROP(apply_override_additively, mApplyOverrideAdditively)
    SYNC_PROP(alternate_driver, mAlternateDriver)
    SYNC_SUPERCLASS(CharWeightable)
END_PROPSYNCS