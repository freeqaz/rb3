#include "char/CharClipDriver.h"
#include "char/CharClip.h"
#include "math/Rand.h"
#include "math/Utl.h"
#include "obj/Msg.h"
#include "obj/Task.h"

static Symbol enter("enter");

CharClipDriver::CharClipDriver(
    Hmx::Object *owner,
    CharClip *clip,
    int mask,
    float blendwidth,
    CharClipDriver *next,
    float f2,
    float f3,
    bool multclips
)
    : mPlayFlags(clip->PlayFlags()), mBlendWidth(blendwidth), mTimeScale(1.0f), mDBeat(0),
      mAdvanceBeat(0), mClip(owner, clip), mNext(next), mNextEvent(-1),
      mPlayMultipleClips(multclips) {
    if (mask & 0xF0U)
        CharClip::SetDefaultLoopFlag(mPlayFlags, mask & 0xF0U);
    if (mask & 0xFU)
        CharClip::SetDefaultBlendFlag(mPlayFlags, mask & 0xFU);
    if (mask & 0xF600U)
        CharClip::SetDefaultBeatAlignModeFlag(mPlayFlags, mask & 0xF600U);
    while (mNext && mNext->mBlendFrac == 0) {
        mNext = mNext->Exit(false);
    }
    if (f2 != kHugeFloat) {
        mBeat = f2;
        mRampIn = f3;
        mBlendFrac = 0;
    } else {
        if (mNext && (mPlayFlags & 0xF) == 2) {
            mNext = mNext->Exit(true);
        }
        if (mNext) {
            CharGraphNode *gNode =
                mNext->mClip->FindNode(mClip, mNext->mBeat, mPlayFlags, mBlendWidth);
            if (gNode) {
                mBeat = gNode->nextBeat;
                float cur = gNode->curBeat;
                float nextBeat = mNext->mBeat;
                mBlendFrac = 0;
                mRampIn = cur - nextBeat;
                goto next;
            }
        }
        mBeat = clip->StartBeat();
        mRampIn = 0;
        mBlendFrac = 1;
    }
next:
    if (mPlayMultipleClips || (mPlayFlags & 0xF) == 8) {
        mBlendFrac = 1e-06f;
    }
    if (mBlendFrac == 1) {
        if (mClip->Range() > 0) {
            float f7 = RandomFloat(0, mClip->Range());
            float beatOff = mBeat + f7;
            float f10 = mClip->EndBeat() + mClip->StartBeat();
            f10 /= 2.0f;
            float f8 = mClip->StartBeat();
            mBeat = ModRange(f8, f10, beatOff);
        }
    }
    mWeight = 0;
}

CharClipDriver::CharClipDriver(Hmx::Object *o, const CharClipDriver &driver)
    : mClip(o, driver.mClip) {
    mPlayFlags = driver.mPlayFlags;
    mBlendWidth = driver.mBlendWidth;
    mTimeScale = driver.mTimeScale;
    mRampIn = driver.mRampIn;
    mBeat = driver.mBeat;
    mDBeat = driver.mDBeat;
    mBlendFrac = driver.mBlendFrac;
    mAdvanceBeat = driver.mAdvanceBeat;
    mWeight = driver.mWeight;
    mNextEvent = driver.mNextEvent;
    mEventData = driver.mEventData;
    if (driver.mNext)
        mNext = new CharClipDriver(o, *driver.mNext);
    else
        mNext = nullptr;
}

CharClipDriver::~CharClipDriver() {}

CharClipDriver *CharClipDriver::Exit(bool b) {
    static Symbol exit("exit");
    if (b && mNext) {
        mNext = mNext->Exit(b);
    }
    CharClipDriver *ret = mNext;
    ExecuteEvent(exit);
    RndAnimatable *syncanim = mClip->SyncAnim();
    if (syncanim)
        syncanim->EndAnim();
    delete this;
    return ret;
}

void CharClipDriver::DeleteStack() {
    if (mNext)
        mNext->DeleteStack();
    delete this;
}

CharClipDriver *CharClipDriver::DeleteClip(Hmx::Object *obj) {
    if (mClip == obj)
        return Exit(false);
    else if (mNext)
        mNext = mNext->DeleteClip(obj);
    return this;
}

void CharClipDriver::ScaleAdd(CharBones &bones, float f) {
    if (f != 0) {
        mWeight = f * Sigmoid(mBlendFrac);
        bones.ScaleAdd(mClip, mWeight, mBeat, mDBeat);
        if (mPlayMultipleClips) {
            if (mNext)
                mNext->ScaleAdd(bones, f);
        } else {
            if (mNext)
                mNext->ScaleAdd(bones, f - mWeight);
        }
    }
}

void CharClipDriver::RotateTo(CharBones &bones, float f) {
    if (f != 0) {
        mWeight = f * Sigmoid(mBlendFrac);
        mClip->RotateTo(bones, mWeight, mBeat);
        if (mNext)
            mNext->RotateTo(bones, f - mWeight);
    }
}

#pragma push
#pragma pool_data off
void CharClipDriver::ExecuteEvent(Symbol s) {
    if (!s.Null()) {
        if (mClip->TypeDef()) {
            static DataNode &dude(DataVariable("clip.dude"));
            dude = DataNode(mClip.RefOwner()->Dir());
            static Message h(s);
            h.SetType(s);
            mClip->HandleType(h);
        }
    }
}
#pragma pop

float CharClipDriver::AlignToBeat(float oldBeat) {
    float align = (float)((mPlayFlags >> 12) & 0xF);
    if (align != 0.0f && mTimeScale == 1.0f && (mPlayFlags & 0xF0) != 0x20) {
        float delta = Modulo(oldBeat - mBeat, align);
        if (delta > align * 0.5f) {
            delta -= align;
            if (delta + mBeat < mClip->StartBeat()) {
                delta += align;
            }
        }
        return delta;
    }
    return 0.0f;
}

void CharClipDriver::SetBeatOffset(float offset, TaskUnits units, Symbol sym) {
    if (offset == 0.0f)
        return;
    if (!mClip)
        return;
    mBeat = mClip->StartBeat();
    if (!sym.Null()) {
        unsigned int i = 0;
        for (; i < mClip->mBeatEvents.size(); i++) {
            if (mClip->mBeatEvents[i].event == sym) {
                mBeat = mClip->mBeatEvents[i].beat;
                break;
            }
        }
        if (i == mClip->mBeatEvents.size()) {
            MILO_WARN("%s could not find event %s", PathName(mClip), sym);
        }
    }
    if (units != kTaskBeats) {
        offset = mClip->DeltaSecondsToDeltaBeat(offset, mBeat);
    }
    mBeat += offset;
}

void CharClipDriver::PlayEvents(float oldBeat) {
    if (mNextEvent == -1) {
        RndAnimatable *syncAnim = mClip->SyncAnim();
        if (syncAnim) {
            syncAnim->StartAnim();
        }
        ExecuteEvent(enter);
        mNextEvent = 0;
    }
    static DataNode &instant(DataVariable("clip.instant"));
    while ((unsigned int)mNextEvent < mClip->mBeatEvents.size()) {
        CharClip::BeatEvent &ev = mClip->mBeatEvents[mNextEvent];
        if (ev.beat > mBeat)
            return;
        instant = DataNode(oldBeat < ev.beat ? 1 : 0);
        ExecuteEvent(ev.event);
        mNextEvent++;
    }
}

CharClipDriver *CharClipDriver::PreEvaluate(float beat, float deltaBeat, float deltaSeconds) {
    MILO_ASSERT(mBlendFrac >= 0, 0xa0);
    if (mBlendWidth < 0.0f) {
        MILO_WARN("CharClipDriver: blend width < 0 with clip %s", (char *)mClip->Name());
        mBlendWidth = 0.0f;
    }
    if (mNext) {
        mNext = mNext->PreEvaluate(beat, deltaBeat, deltaSeconds);
    }
    int flags = mPlayFlags;
    bool useRealTime = flags & CharClip::kPlayRealTime;
    bool useUserTime = flags & CharClip::kPlayUserTime;

    float advance;
    if (mPlayMultipleClips) {
        advance = deltaBeat;
        if (useRealTime) {
            advance = deltaSeconds;
        }
    } else if (mNext) {
        advance = mNext->mAdvanceBeat;
    } else {
        advance = deltaBeat;
        if (useRealTime) {
            advance = deltaSeconds;
        }
    }

    if (mRampIn > 0.0f || advance > 0.0f) {
        mRampIn -= advance;
    }

    if (mRampIn >= 0.0f) {
        mDBeat = 0.0f;
        mAdvanceBeat = 0.0f;
    } else {
        float oldBeat = mBeat;
        if (!useUserTime) {
            if (flags & 0x80) {
                mDBeat = 0.0f;
                mPlayFlags = flags & ~0x80;
            } else {
                if (useRealTime) {
                    deltaBeat = mClip->DeltaSecondsToDeltaBeat(deltaSeconds, mBeat);
                }
                mDBeat = mTimeScale * deltaBeat;
            }
        }
        mBeat = mDBeat + mBeat;
        float align = AlignToBeat(beat);
        mBeat += align;
        mAdvanceBeat = mDBeat + align;
        PlayEvents(oldBeat);
        RndAnimatable *syncAnim = mClip->SyncAnim();
        if (syncAnim) {
            float frame = mClip->BeatToFrame(mBeat);
            syncAnim->SetFrame(frame, 1.0f);
        }
        if (mBlendFrac < 1.0f) {
            if (mBlendWidth > 0.0f) {
                float inc;
                if (useUserTime) {
                    inc = deltaBeat;
                } else {
                    inc = mDBeat;
                }
                inc = inc / mBlendWidth;
                if (inc > 0.0f) {
                    mBlendFrac += inc;
                }
            } else {
                mBlendFrac = 1.0f;
            }
            float clamped;
            if (mBlendFrac >= 1.0f) {
                clamped = 1.0f;
            } else {
                clamped = mBlendFrac;
            }
            mBlendFrac = clamped;
        }
    }

    if (!mPlayMultipleClips) {
        if (mNext) {
            if (mBlendFrac == 1.0f) {
                mNext = mNext->Exit(true);
            }
        }
    } else {
        if (mBeat > mClip->EndBeat()) {
            return Exit(false);
        }
    }
    return this;
}

float CharClipDriver::Evaluate(float beat, float deltaBeat, float deltaSeconds) {
    float nextResult = 0.0f;
    if (mNext) {
        nextResult = mNext->Evaluate(beat, deltaBeat, deltaSeconds);
    }
    if ((mPlayFlags & 0xF0) == CharClip::kPlayLoop) {
        if (mBeat > mClip->EndBeat()) {
            float lengthBeats = mClip->LengthBeats();
            if (lengthBeats > 0.0f) {
                float startBeat = mClip->StartBeat();
                float dist = std::fmod(mClip->EndBeat() - mBeat, lengthBeats);
                mBeat = dist + startBeat;
            } else {
                mBeat = mClip->StartBeat();
            }
            float align = AlignToBeat(beat);
            mBeat += align;
            mNextEvent = 0;
        } else if (mBeat < mClip->StartBeat()) {
            float startBeat = mClip->StartBeat();
            float lengthBeats = mClip->LengthBeats();
            if (lengthBeats > 0.0f) {
                float endBeat = mClip->EndBeat();
                float dist = std::fmod(startBeat - mBeat, lengthBeats);
                mBeat = endBeat - dist;
            } else {
                mBeat = mClip->StartBeat();
            }
            float align = AlignToBeat(beat);
            mBeat += align;
            mNextEvent = (int)mClip->mBeatEvents.size();
        }
    }
    float sigmoid = Sigmoid(mBlendFrac);
    return (1.0f - sigmoid) * nextResult + sigmoid;
}