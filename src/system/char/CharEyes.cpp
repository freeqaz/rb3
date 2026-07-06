#include "char/CharEyes.h"
#include "char/CharFaceServo.h"
#include "char/CharPollable.h"
#include "char/CharWeightSetter.h"
#include "char/CharLookAt.h"
#include "char/CharInterest.h"
#include "char/CharWeightable.h"
#include "char/CharEyeDartRuleset.h"
#include "decomp.h"
#include "math/Rand.h"
#include "math/Utl.h"
#include "math/Vec.h"
#include "obj/DataUtl.h"
#include "obj/ObjMacros.h"
#include "obj/Object.h"
#include "obj/Task.h"
#include "rndobj/Cam.h"
#include "rndobj/Graph.h"
#include "rndobj/Rnd.h"
#include "rndobj/Trans.h"
#include "ui/PanelDir.h"
#include "utl/Symbols.h"
#include "world/Dir.h"
#include <cmath>
#include <cstring>

#ifdef HX_NATIVE
// W0.3d (part a): under a frozen deterministic replay clock, the eye/face
// look-at path's RNG draws (dart offsets, procedural blink timing, and
// CharLookAt jitter) are the sole source of nondeterminism in the draw-log
// gate -- the *order* in which look-at instances consume the global RNG
// varies with heap/async-load/iteration order even with a fixed seed. Freeze
// all three under RB3FixedClockActive() so eye-mesh world transforms become a
// pure function of the frozen sim state. HX_NATIVE-only; Wii/MWCC build never
// sees this include or any of the guarded code below.
#include "rb3_replay.h"
#endif

bool CharEyes::sDisableEyeDart;
bool CharEyes::sDisableEyeJitter;
bool CharEyes::sDisableInterestObjects;
bool CharEyes::sDisableProceduralBlink;
bool CharEyes::sDisableEyeClamping;

INIT_REVS(CharEyes)

CharEyes::EyeDesc::EyeDesc(Hmx::Object *o)
    : mEye(o), mUpperLid(o), mLowerLid(o), mLowerLidBlink(o), mUpperLidBlink(o) {}

CharEyes::EyeDesc::EyeDesc(const CharEyes::EyeDesc &desc)
    : mEye(desc.mEye), mUpperLid(desc.mUpperLid), mLowerLid(desc.mLowerLid),
      mLowerLidBlink(desc.mLowerLidBlink), mUpperLidBlink(desc.mUpperLidBlink) {}

CharEyes::EyeDesc &CharEyes::EyeDesc::operator=(const CharEyes::EyeDesc &desc) {
    mEye = desc.mEye;
    mUpperLid = desc.mUpperLid;
    mLowerLid = desc.mLowerLid;
    mUpperLidBlink = desc.mUpperLidBlink;
    mLowerLidBlink = desc.mLowerLidBlink;
    return *this;
}

void CharEyes::CharInterestState::ResetState() { mLastTimeStamp = -1.0f; }

CharEyes::CharInterestState::CharInterestState(Hmx::Object *o) : mInterest(o) {
    ResetState();
}

CharEyes::CharInterestState::CharInterestState(const CharEyes::CharInterestState &state)
    : mInterest(state.mInterest) {
    ResetState();
}

CharEyes::CharInterestState &
CharEyes::CharInterestState::operator=(const CharEyes::CharInterestState &state) {
    mInterest = state.mInterest;
    return *this;
}

void CharEyes::CharInterestState::BeginRefractoryPeriod() {
    mLastTimeStamp = TheTaskMgr.Seconds(TaskMgr::kRealTime);
}

bool CharEyes::CharInterestState::IsInRefractoryPeriod() {
    if (!mInterest || mLastTimeStamp < 0.0)
        return false;
    else {
        float secs = TheTaskMgr.Seconds(TaskMgr::kRealTime) - mLastTimeStamp;
        if (secs < mInterest->mRefractoryPeriod)
            return true;
        else
            return false;
    }
}

float CharEyes::CharInterestState::RefractoryTimeRemaining() {
    if (!mInterest || mLastTimeStamp < 0.0)
        return 0.0f;
    else {
        float secs = TheTaskMgr.Seconds(TaskMgr::kRealTime) - mLastTimeStamp;
        if (secs < mInterest->mRefractoryPeriod)
            return mInterest->mRefractoryPeriod - secs;
        else
            return 0.0f;
    }
}

CharEyes::CharEyes()
    : mEyes(this), mInterests(this), mFaceServo(this), mCamWeight(this), mTarget(0, 0, 0),
      mDefaultFilterFlags(0), mViewDirection(this), mHeadLookAt(this),
      mMaxExtrapolation(19.5f), mMinTargetDist(35.0f), mUpperLidTrackUp(1.0f),
      mUpperLidTrackDown(1.0f), mLowerLidTrackUp(0.75f), mLowerLidTrackDown(0.75f),
      mLowerLidTrackRotate(0), mInterestFilterFlags(0), mLastFacing(0, 0, 0), mLastLook(0),
      mLastBlinkWeight(0), mBlinkDetect(0), mTargetTooClose(0), mCurInterest(this), mFocusInterest(this), mCurFocusPriorityClass(-1), mNewFocusInterest(0),
#ifdef MILO_DEBUG
      mLastExtrapolatedDir(0, 1.0f, 0),
#endif
      mLastHeadIKWeight(0), mDarting(0), mDartNextEventTime(-1.0f), mDartsRemaining(-1), mProceduralBlink(0), mBlinkTimestamp(-1.0f), mBlinkWindowCount(0),
      mBlinkWindowSecsRemaining(-1.0f), mLastBlinkDetectTime(-1.0f), mInterestFiltersChanged(0), mBlinksEnabled(1) {
    mMaxEyeCang = std::cos(0.52359879f);
    mOverlay = RndOverlay::Find("eye_status", false);
}

CharEyes::~CharEyes() {}

void CharEyes::Enter() {
    mLastFacing.Zero();
    mLastLook = 0;
    mAvDelta = 0;
    mLastCang = 1.0f;
    mLastBlinkWeight = -1.0f;
    mBlinkDetect = 0;
    mDarting = 0;
    mDartNextEventTime = -1.0f;
    mDartsRemaining = -1;
    mProceduralBlink = 0;
    mBlinkTimestamp = -1.0f;
    mBlinkWindowCount = 0;
    mBlinkWindowSecsRemaining = -1.0f;
    mLastBlinkDetectTime = -1.0f;
    mTargetTooClose = 0;
    mInterestFilterFlags = mDefaultFilterFlags;
    mInterestFiltersChanged = 0;
    mNewFocusInterest = 0;
    mLastHeadIKWeight = 0;
    RndTransformable *head = GetHead();
    if (head) {
        mLastFacing = head->WorldXfm().m.y;
        Normalize(mLastFacing, mLastFacing);
    }
    for (ObjVector<EyeDesc>::iterator it = mEyes.begin(); it != mEyes.end(); ++it) {
        it->mEye->Enter();
    }
    for (ObjVector<CharInterestState>::iterator it = mInterests.begin();
         it != mInterests.end();
         ++it) {
        it->ResetState();
    }
    RndPollable::Enter();
}

void CharEyes::Exit() {
    mFocusInterest = 0;
    mCurFocusPriorityClass = -1;
    mInterests.clear();
    for (ObjVector<EyeDesc>::iterator it = mEyes.begin(); it != mEyes.end(); ++it) {
        it->mEye->Exit();
    }
    RndPollable::Exit();
}

void CharEyes::Highlight() {
#ifdef MILO_DEBUG
    if (GetHead()) {
        RndGraph *oneframe = RndGraph::GetOneFrame();
        RndTransformable *trans = 0;
        for (ObjVector<EyeDesc>::iterator it = mEyes.begin(); it != mEyes.end(); ++it) {
            trans = it->mEye->GetSource();
            if (trans) {
                const Transform &tf1 = trans->WorldXfm();
                const Transform &tf2 = trans->WorldXfm();
                Vector3 v100;
                ScaleAdd(tf2.v, tf1.m.y, 3, v100);
                if (it->mEye->unkb1)
                    oneframe->AddLine(
                        trans->WorldXfm().v, v100, Hmx::Color(1.0f, 0.0f, 0.0f), true
                    );
                else
                    oneframe->AddLine(
                        trans->WorldXfm().v, v100, Hmx::Color(0.0f, 1.0f, 0.0f), true
                    );
            }
        }
        Vector3 v10c(GetHead()->WorldXfm().v);
        if (trans) {
            bool fcmp = mLastCang >= (mCurInterest ? mCurInterest->mMaxViewAngleCos : mMaxEyeCang);
            if (mDarting) {
                oneframe->AddSphere(
                    mTarget, mEyeDartRulesetData.mMaxRadius, Hmx::Color(0.9f, 0.9f, 0.9f)
                );
                Vector3 v118;
                Add(mTarget, mCurDartOffset, v118);
                EnforceMinimumTargetDistance(v10c, v118, v118);
                oneframe->AddSphere(v118, 0.5f, Hmx::Color(0.0f, 0.0f, 1.0f));
                oneframe->AddLine(
                    trans->WorldXfm().v,
                    v118,
                    fcmp ? Hmx::Color(0.2f, 0.2f, 1.0f) : Hmx::Color(1, 0, 0),
                    true
                );
            } else {
                oneframe->AddLine(
                    trans->WorldXfm().v,
                    mTarget,
                    fcmp ? Hmx::Color(1, 1, 1) : Hmx::Color(1, 0, 0),
                    true
                );
            }
            if (mProceduralBlink) {
                oneframe->AddString3D(
                    "p blink!", trans->WorldXfm().v, Hmx::Color(1, 1, 1)
                );
            }
        }

        if (mFocusInterest) {
            if (mCurInterest != mFocusInterest) {
                const char *nametouse = mCurInterest ? mCurInterest->Name() : "GENERATED";
                oneframe->AddString3D(
                    MakeString("focus = '%s' (looking at %s)", mFocusInterest->Name(), nametouse),
                    v10c,
                    Hmx::Color(1, 0, 0)
                );
            } else {
                oneframe->AddString3D(
                    MakeString("focus = '%s'", mFocusInterest->Name()), v10c, Hmx::Color(0, 1, 0)
                );
            }
        } else {
            if (mCurInterest) {
                oneframe->AddString3D(
                    MakeString("interest = '%s'", mCurInterest->Name()),
                    v10c,
                    Hmx::Color(0, 1, 0)
                );
            }
        }

        if (mInterests.size() != 0) {
            const Transform &headXfm = GetHead()->WorldXfm();
            Vector3 headMY = headXfm.m.y;
            Normalize(headMY, headMY);
            Vector3 va0 = headXfm.v;
            for (ObjVector<CharInterestState>::iterator it = mInterests.begin();
                 it != mInterests.end();
                 ++it) {
                bool b7 = it->mInterest->IsMatchingFilterFlags(mInterestFilterFlags)
                    || ((mInterestFilterFlags == mDefaultFilterFlags)
                        && !it->mInterest->CategoryFlags());
                if (mCurInterest == it->mInterest) {
                    oneframe->AddSphere(
                        it->mInterest->WorldXfm().v, 2, Hmx::Color(0, 1, 0)
                    );
                    Vector2 v2;
                    if (RndCam::Current()->WorldToScreen(it->mInterest->WorldXfm().v, v2)
                        > 0) {
                        v2.x *= TheRnd->Width();
                        v2.y *= TheRnd->Height();
                        v2.y += 15.0;
                        v2.x -= 30.0;
                        oneframe->AddString(
                            MakeString("%s", it->mInterest->Name()),
                            v2,
                            Hmx::Color(1, 1, 1)
                        );
                    }
                } else {
                    if (it->mInterest->IsWithinViewCone(va0, mLastExtrapolatedDir)
                        && it->mInterest->IsWithinViewCone(va0, headMY)) {
                        oneframe->AddSphere(
                            it->mInterest->WorldXfm().v,
                            2,
                            b7 ? Hmx::Color(1, 1, 0) : Hmx::Color(1, 0.64705884f, 0)
                        );
                    } else {
                        oneframe->AddSphere(
                            it->mInterest->WorldXfm().v,
                            2,
                            b7 ? Hmx::Color(1, 0, 0)
                               : Hmx::Color(0.6901961f, 0.1882353f, 0.3764706f)
                        );
                    }
                }
                if (it->IsInRefractoryPeriod()) {
                    oneframe->AddString3D(
                        MakeString("r=%f", it->RefractoryTimeRemaining()),
                        it->mInterest->WorldXfm().v,
                        Hmx::Color(1, 1, 1)
                    );
                }
            }
        }
    }
#endif
}

DECOMP_FORCEACTIVE(CharEyes, "%s", "r=%f")

void CharEyes::UpdateOverlay() {
    if (mOverlay && mOverlay->Showing()) {
        *mOverlay << Dir()->Name() << ": ";
        if (mCurInterest) {
            if (mFocusInterest) {
                if (streq(mFocusInterest->Name(), mCurInterest->Name())) {
                    *mOverlay << "Look(FOC) ";
                    goto lol;
                }
            }
            *mOverlay << "Look(" << mCurInterest->Name() << ") ";
        } else
            *mOverlay << "Look(GEN) ";
    lol:
        if (mFocusInterest) {
            Transform &headxfm = GetHead()->WorldXfm();
            Vector3 v34(headxfm.m.y);
            Normalize(v34, v34);
            const char *str = mFocusInterest->IsWithinViewCone(headxfm.v, v34) ? "t" : "f";
            *mOverlay << "Foc(" << mFocusInterest->Name() << " p(" << mCurFocusPriorityClass << ") v(" << str << ")) ";
        } else
            *mOverlay << "Foc(NA) ";
        *mOverlay << "t(" << mLastLook << ") ";
        Vector3 v40(GetHead()->WorldXfm().v);
        Vector3 v4c;
        Vector3 v58(mTarget);
        RndTransformable *target = GetTarget();
        if (target)
            v58 = target->WorldXfm().v;
        Subtract(v58, v40, v4c);
        float len = Length(v4c);
        *mOverlay << "Dist(" << len << ") ";
        if (mProceduralBlink)
            *mOverlay << "P Blink! ";
        if (mDarting)
            *mOverlay << "Dart! ";
        if (mTargetTooClose)
            *mOverlay << "Close! ";
        *mOverlay << "\n";
    }
}

DECOMP_FORCEACTIVE(
    CharEyes,
    "no_lids",
    "eyes.disable_clamping",
    "eyes.debug_clamping",
    "eyes.disable_llidnorm",
    "cheat.disable_eye_darts",
    "cheat.disable_procedural_blinks",
    "cheat.disable_interest_objects",
    "ObjPtr_p.h",
    "f.Owner()",
    ""
)

bool CharEyes::EitherEyeClamped() {
    for (ObjVector<EyeDesc>::iterator it = mEyes.begin(); it != mEyes.end(); ++it) {
        if (it->mEye && it->mEye->unkb1)
            return true;
    }
    return false;
}

void CharEyes::ClearAllInterestObjects() { mInterests.clear(); }

void CharEyes::AddInterestObject(CharInterest *interest) {
    if (interest) {
        CharInterestState state(this);
        state.mInterest = interest;
        mInterests.push_back(state);
    }
}

bool CharEyes::SetFocusInterest(CharInterest *interest, int i) {
    if (mFocusInterest.mPtr && mCurFocusPriorityClass > i)
        return false;
    bool temp = interest != mFocusInterest.mPtr;
    mFocusInterest = interest;
    mCurFocusPriorityClass = i;
    if (temp)
        mNewFocusInterest = true;
    if (!mFocusInterest)
        mCurFocusPriorityClass = -1;
    return true;
}

bool CharEyes::EyesOnTarget(float f) {
    for (ObjVector<EyeDesc>::iterator it = mEyes.begin(); it != mEyes.end(); ++it) {
        RndTransformable *src = it->mEye->GetSource();
        if (src) {
            const Transform &xfm = src->WorldXfm();
            float diffy = mTarget.y - xfm.v.y;
            float diffx = mTarget.x - xfm.v.x;
            Vector3 v8c(src->WorldXfm().m.y);
            v8c.z = 0;
            float dot = v8c.x * diffx + v8c.y * diffy;
            Vector3 v80(diffx, diffy, 0);
            if (std::acos(Clamp<float>(-1, 1, dot / (Length(v8c) * Length(v80))))
                    * 57.295776f
                > f) {
                return false;
            }
        }
    }
    return true;
}

void CharEyes::EnforceMinimumTargetDistance(
    const Vector3 &v1, const Vector3 &v2, Vector3 &vout
) {
    Vector3 v2c;
    Subtract(v2, v1, v2c);
    float vlen = Length(v2c);
    bool b1 = false;
    mTargetTooClose = false;
    if (mCurInterest && mCurInterest->ShouldOverrideMinTargetDistance())
        b1 = true;
    float f4;
    if (b1)
        f4 = mCurInterest->MinTargetDistanceOverride();
    else
        f4 = mMinTargetDist;
    if (vlen < f4) {
        Vector3 v38;
        if (!(std::abs(v2c.x) < 0.0001f) || !(std::abs(v2c.y) < 0.0001f) || !(std::abs(v2c.z) < 0.0001f))
            Scale(v2c, f4 / Length(v2c), v38);
        else
            v38.Set(0, 0, 0);
        Add(v1, v38, vout);
        mTargetTooClose = true;
    }
}

bool CharEyes::IsHeadIKWeightIncreasing() {
    if (mHeadLookAt) {
        float weight = mHeadLookAt->Weight();
        return (weight > 0 && weight - mLastHeadIKWeight > 0);
    } else
        return false;
}

RndTransformable *CharEyes::GetTarget() {
    if (mEyes.empty() || !mEyes[0].mEye)
        return nullptr;
    else {
        return mEyes[0].mEye->GetDest();
    }
}

Vector3 CharEyes::GenerateDartOffset() {
    Vector3 vout;
    float start = mEyeDartRulesetData.mMinRadius;
    float end = mEyeDartRulesetData.mMaxRadius;
    if (mEyeDartRulesetData.mScaleWithDistance
        && mEyeDartRulesetData.mReferenceDistance > 0.1f) {
        Vector3 v48;
        Subtract(mTarget, GetHead()->WorldXfm().v, v48);
        float len = Length(v48);
        start *= len / mEyeDartRulesetData.mReferenceDistance;
        end *= len / mEyeDartRulesetData.mReferenceDistance;
    }
    float mult = RandomFloat(0, 1) > 0.5f ? 1.0f : -1.0f;
    vout[0] = RandomFloat(start, end) * mult;
    mult = RandomFloat(0, 1) > 0.5f ? 1.0f : -1.0f;
    vout[1] = RandomFloat(start, end) * mult;
    mult = RandomFloat(0, 1) > 0.5f ? 1.0f : -1.0f;
    vout[2] = RandomFloat(start, end) * mult;
    return vout;
}

void CharEyes::DartUpdate() {
    static DataNode &dartCheat = DataVariable("cheat.disable_eye_darts");
    if (sDisableEyeDart || dartCheat.Int(nullptr) != 0)
        return;
    mDartNextEventTime -= TheTaskMgr.DeltaSeconds();
    if (mDarting) {
        if (mDartNextEventTime < 0) {
            mDartsRemaining--;
            if (mDartsRemaining < 0) {
                mDarting = false;
                mDartNextEventTime = RandomFloat(
                    mEyeDartRulesetData.mMinSecsBetweenSequences,
                    mEyeDartRulesetData.mMaxSecsBetweenSequences
                );
            } else {
                mDartNextEventTime = RandomFloat(
                    mEyeDartRulesetData.mMinSecsBetweenDarts,
                    mEyeDartRulesetData.mMaxSecsBetweenDarts
                );
                mCurDartOffset = GenerateDartOffset();
            }
        }
    } else if (mDartNextEventTime < 0 && EyesOnTarget(mEyeDartRulesetData.mOnTargetAngleThresh)
               && !mProceduralBlink) {
        mDarting = true;
        mDartsRemaining = RandomInt(
            mEyeDartRulesetData.mMinDartsPerSequence,
            mEyeDartRulesetData.mMaxDartsPerSequence
        );
        mDartNextEventTime = RandomFloat(
            mEyeDartRulesetData.mMinSecsBetweenDarts,
            mEyeDartRulesetData.mMaxSecsBetweenDarts
        );
        mCurDartOffset = GenerateDartOffset();
    }
}

inline float EaseInExp(float t) {
    MILO_ASSERT(t >= 0 && t <= 1, 0x1F3);
    return std::pow(t, 3.76f);
}

void CharEyes::NextLook() {
    Vector3 oldTarget = mTarget;

    RndTransformable *head = GetHead();
    const Transform &headXfm = head->WorldXfm();

    Vector3 facingDir(headXfm.m.y);
    Normalize(facingDir, facingDir);

    if (mFocusInterest && (mFocusInterest->IsWithinViewCone(headXfm.v, facingDir) || IsHeadIKWeightIncreasing())) {
        mTarget = mFocusInterest->WorldXfm().v;
        mCurInterest = mFocusInterest;
        const CharEyeDartRuleset *dartOverride = mCurInterest->GetDartRulesetOverride();
        if (dartOverride) {
            mEyeDartRulesetData = dartOverride->mData;
        } else {
            mEyeDartRulesetData.ClearToDefaults();
        }
    } else {
        const Vector3 &lastFacing = mLastFacing;
        Vector3 extrap(
            (facingDir.x - lastFacing.x) * 45.0f,
            (facingDir.y - lastFacing.y) * 45.0f,
            (facingDir.z - lastFacing.z) * 45.0f
        );
        float extrapMag = Length(extrap);
        float maxExtrap = std::tan(mMaxExtrapolation * 0.017453292f);
        if (extrapMag > maxExtrap) {
            float scale = maxExtrap / extrapMag;
            extrap.x = scale * extrap.x;
            extrap.y = extrap.y * scale;
            extrap.z = extrap.z * scale;
        }
        Vector3 newFacing(
            facingDir.x + extrap.x,
            extrap.y + facingDir.y,
            extrap.z + facingDir.z
        );
        float dist = RandomFloat(100.0f, 20.0f);
        dist *= 12.0f;
        Vector3 proj(dist * newFacing.x, newFacing.y * dist, newFacing.z * dist);
        mTarget.x = headXfm.v.x + proj.x;
        mTarget.y = proj.y + headXfm.v.y;
        mTarget.z = headXfm.v.z + proj.z;
        RndTransformable *dirTrans = dynamic_cast<RndTransformable *>(Dir());
        if (dirTrans) {
            const Vector3 &dirPos = dirTrans->WorldXfm().v;
            if (dirPos.z > mTarget.z) {
                float scale = (dirPos.z - headXfm.v.z) / (mTarget.z - headXfm.v.z);
                float sx = proj.x * scale;
                float sy = proj.y * scale;
                float sz = proj.z * scale;
                mTarget.x = headXfm.v.x + sx;
                mTarget.y = sy + headXfm.v.y;
                mTarget.z = headXfm.v.z + sz;
            }
        }

        static const DataNode &interestCheat = DataVariable("cheat.disable_interest_objects");

        if (mInterests.size() > 0 && !sDisableInterestObjects) {
            if (interestCheat.Int(0) == 0) {
                float maxDistSq = -1.0f;
                float bestScore = maxDistSq;
                for (ObjVector<CharInterestState>::iterator it = mInterests.begin();
                     it != mInterests.end();
                     ++it) {
                    const Vector3 &intPos = it->mInterest->WorldXfm().v;
                    float fx = intPos.x - headXfm.v.x;
                    float fy = intPos.y - headXfm.v.y;
                    float fz = intPos.z - headXfm.v.z;
                    float distSq = (fz * fz + (fx * fx + fy * fy));
                    if (distSq > maxDistSq)
                        maxDistSq = distSq;
                }

                if (maxDistSq > 0.0f) {
                    CharInterestState *bestState = 0;
                    Vector3 targetDir;
                    Subtract(mTarget, headXfm.v, targetDir);
                    Normalize(targetDir, targetDir);

                    float inverseDist = 1.0f / maxDistSq;

                    for (ObjVector<CharInterestState>::iterator it = mInterests.begin();
                         it != mInterests.end();
                         ++it) {
                        if (it->mInterest != mCurInterest) {
                            bool _cond = !it->IsInRefractoryPeriod();
                            if (_cond) {
                                float score = it->mInterest->ComputeScore(
                                    headXfm.m.y,
                                    headXfm.v,
                                    targetDir,
                                    inverseDist,
                                    mInterestFilterFlags,
                                    mDefaultFilterFlags == mInterestFilterFlags
                                );
                                if (score >= 0.0f && score > bestScore) {
                                    bestScore = score;
                                    bestState = &*it;
                                }
                            }
                        }
                    }

                    if (bestState) {
                        mTarget = bestState->mInterest->WorldXfm().v;
                        mCurInterest = bestState->mInterest;
                        const CharEyeDartRuleset *dartOverride =
                            mCurInterest->GetDartRulesetOverride();
                        if (dartOverride) {
                            mEyeDartRulesetData = dartOverride->mData;
                        } else {
                            mEyeDartRulesetData.ClearToDefaults();
                        }
                        bestState->BeginRefractoryPeriod();
                    } else {
                        mCurInterest = 0;
                        mEyeDartRulesetData.ClearToDefaults();
                    }

                    mCurDartOffset = targetDir;
                    goto stateReset;
                }
            }
        }

        mCurInterest = 0;
        mEyeDartRulesetData.ClearToDefaults();
    }

stateReset:
    mLastLook = 0.0f;
    mAvDelta = 0.0f;
    mInterestFiltersChanged = false;
    mNewFocusInterest = false;
    mLastCang = 1e30f;
    mDarting = false;
    mDartNextEventTime = 0.2f;
    mDartsRemaining = -1;

    static const DataNode &blinkCheat = DataVariable("cheat.disable_procedural_blinks");

    if (!sDisableProceduralBlink && !blinkCheat.NotNull() && !mProceduralBlink && mFaceServo
        && mBlinkWindowCount < 9
        && TheTaskMgr.Seconds(TaskMgr::kRealTime) - mLastBlinkDetectTime > 0.6f
        && mLastBlinkWeight < 0.5f) {
        Vector3 oldDir(
            oldTarget.x - headXfm.v.x,
            oldTarget.y - headXfm.v.y,
            oldTarget.z - headXfm.v.z
        );
        Normalize(oldDir, oldDir);

        Vector3 newDir(
            mTarget.x - headXfm.v.x,
            mTarget.y - headXfm.v.y,
            mTarget.z - headXfm.v.z
        );
        Normalize(newDir, newDir);

        float dotProd = Dot(newDir, oldDir);
        if (dotProd < 0.984808f) {
            ForceBlink();
            mDelayedTarget = mTarget;
            mTarget = oldTarget;
        }
    }
}

void CharEyes::Poll() {
    if (mEyes.empty())
        return;

    RndTransformable *head = GetHead();
    if (!head)
        return;

#ifdef HX_NATIVE
    // W0.3d (part a): freeze dart/blink RNG for the whole Poll() under a
    // frozen replay clock. Flag-OFF (no RB3_FIXED_CLOCK): RB3FixedClockActive()
    // returns false, statics keep their existing DTA/cheat-driven values.
    bool rb3FixedClockLookAt = RB3FixedClockActive();
    if (rb3FixedClockLookAt) {
        sDisableEyeDart = true;
        sDisableProceduralBlink = true;
    }
#endif

    float camWeight = TheTaskMgr.DeltaSeconds();
    if (camWeight < 0.0f) {
        Enter();
        return;
    }

    camWeight = 0.0f;
    if (mCamWeight) {
        camWeight = mCamWeight->Weight();
    }

    mLastLook += TheTaskMgr.DeltaSeconds();

    float blinkWeight;
    if (mFaceServo) {
        blinkWeight = mFaceServo->BlinkWeightLeft();
    } else {
        blinkWeight = 0.0f;
    }

    bool blinkDetected = false;
    if (blinkWeight < 0.3f) {
        mBlinkDetect = true;
    } else {
        if (mBlinkDetect && mLastBlinkWeight > 0.8f && blinkWeight < mLastBlinkWeight) {
            mBlinkDetect = false;
            mBlinkWindowCount++;
            blinkDetected = true;
            mLastBlinkDetectTime = TheTaskMgr.Seconds(TaskMgr::kRealTime);
        }
    }
    mLastBlinkWeight = blinkWeight;

    Transform &headXfm = head->WorldXfm();

    Vector3 targetDir;
    Subtract(mTarget, headXfm.v, targetDir);
    Normalize(targetDir, targetDir);

    Vector3 facingDir(headXfm.m.y);
    Normalize(facingDir, facingDir);

    float cang = Dot(targetDir, facingDir);
    cang = Clamp<float>(-1.0f, 1.0f, cang);

    if (mLastCang != 1e30f) {
        TheTaskMgr.Seconds(TaskMgr::kRealTime);
        CharInterest *interest = mCurInterest;
        mAvDelta = (cang - mLastCang - mAvDelta) * 0.1f + mAvDelta;

        float minLookTime = interest ? interest->mMinLookTime : 1.0f;
        float maxLookTime = interest ? interest->mMaxLookTime : 3.0f;
        float viewAngleCos = interest ? interest->mMaxViewAngleCos : mMaxEyeCang;

        bool canSeeTarget = cang >= viewAngleCos;

        if (maxLookTime >= mLastLook && !mNewFocusInterest
            && (!mFocusInterest || interest == (CharInterest *)mFocusInterest
                || ((mLastLook <= 0.4f
                     || !mFocusInterest->IsWithinViewCone(headXfm.v, facingDir))
                    && !IsHeadIKWeightIncreasing()))
            && (!mInterestFiltersChanged || mLastLook <= 0.25f)) {
            if (mLastLook <= minLookTime)
                goto storeState;
            if (!blinkDetected) {
                if (canSeeTarget) {
                    if (!EitherEyeClamped())
                        goto storeState;
                }
                if (mAvDelta >= 0.0f)
                    goto storeState;
            }
        }

        if (camWeight == 0.0f) {
            NextLook();
        }
    }

storeState:
    mLastCang = cang;
    mLastFacing = facingDir;

    if (mHeadLookAt) {
        mLastHeadIKWeight = mHeadLookAt->Weight();
    } else {
        mLastHeadIKWeight = 0.0f;
    }

    DartUpdate();

    if (mCurInterest) {
        if (!mProceduralBlink) {
            mTarget = mCurInterest->WorldXfm().v;
        }
        EnforceMinimumTargetDistance(headXfm.v, mTarget, mTarget);
    }

    RndTransformable *eyeTarget = GetTarget();

    if (eyeTarget) {
        float weight = Weight();
        Transform camXfm;
        Transform xfm;
        if (camWeight > 0.0f) {
            RndCam *cam;
            do {
                if (TheWorld && (cam = TheWorld->GetCam())) break;
                if ((cam = RndCam::Current())) break;
                cam = TheRnd->DefaultCam();
            } while(false);
            if (cam) {
                camXfm = eyeTarget->WorldXfm();
                Interp(camXfm.v, cam->WorldXfm().v, camWeight, camXfm.v);
                eyeTarget->SetWorldXfm(camXfm);
            }
        } else {
            Vector3 localTarget = mTarget;
            if (mDarting) {
                localTarget.x += mCurDartOffset.x;
                localTarget.y += mCurDartOffset.y;
                localTarget.z += mCurDartOffset.z;
                EnforceMinimumTargetDistance(headXfm.v, localTarget, localTarget);
            }
            xfm = eyeTarget->WorldXfm();
            Interp(xfm.v, localTarget, weight, xfm.v);
            eyeTarget->SetWorldXfm(xfm);
        }
    }
    ProceduralBlinkUpdate();

#ifdef HX_NATIVE
    CharLookAt::sDisableJitter = sDisableEyeJitter || rb3FixedClockLookAt;
#else
    CharLookAt::sDisableJitter = sDisableEyeJitter;
#endif
    for (ObjVector<EyeDesc>::iterator it = mEyes.begin(); it != mEyes.end(); ++it) {
        it->mEye->Poll();
        LidTrackAndClampingUpdate(*it, blinkWeight);
    }
#ifdef HX_NATIVE
    // Under a frozen clock, hold jitter disabled for the rest of the frame
    // (not just this bracket) -- other CharLookAt instances (e.g. head IK)
    // may poll outside this loop at an order-dependent point in the frame,
    // and re-enabling here would reintroduce the same order-dependent RNG
    // nondeterminism this freeze exists to remove.
    CharLookAt::sDisableJitter = rb3FixedClockLookAt;
#else
    CharLookAt::sDisableJitter = false;
#endif

    UpdateOverlay();
}

void CharEyes::LidTrackAndClampingUpdate(EyeDesc &desc, float blinkWeight) {
    auto _tmp0 = DataVariable("no_lids").Int(0);
    if (_tmp0)
        return;
    if (!mFaceServo)
        return;
    if (!mFaceServo->mClips)
        return;
    if (!mFaceServo->mBaseClip)
        return;

    RndTransformable *source = desc.mEye->GetSource();
    if (!source)
        return;

    RndTransformable *lowerLid = desc.mLowerLid;
    RndTransformable *upperLid = desc.mUpperLid;

    float dist = -1.0f;
    if (lowerLid) {
        Vector3 srcPos = source->WorldXfm().v;
        Vector3 lidPos = lowerLid->WorldXfm().v;
        Vector3 diff(lidPos.x - srcPos.x, lidPos.y - srcPos.y, lidPos.z - srcPos.z);
        dist = Length(diff);
    }

    float eyeRot = (1.0f - blinkWeight) * source->LocalXfm().m.y.x;

    if (upperLid) {
        float angle;
        if (eyeRot >= 0.0f) angle = -eyeRot * mUpperLidTrackUp;
        else angle = -eyeRot * mUpperLidTrackDown;
        Transform &xfm = upperLid->DirtyLocalXfm();
        RotateAboutZ(xfm.m, angle, xfm.m);
    }

    if (lowerLid) {
        if (mLowerLidTrackRotate) {
            float angle;
            if (eyeRot >= 0.0f) angle = -eyeRot * mLowerLidTrackUp;
            else angle = -eyeRot * mLowerLidTrackDown;
            Transform &xfm = lowerLid->DirtyLocalXfm();
            RotateAboutZ(xfm.m, angle, xfm.m);
        } else {
            float offset;
            if (eyeRot >= 0.0f) offset = eyeRot * mLowerLidTrackUp;
            else offset = eyeRot * mLowerLidTrackDown;
            lowerLid->DirtyLocalXfm().v.x += offset;
        }
    }

    RndTransformable *lowerBlink = desc.mLowerLidBlink;
    RndTransformable *upperBlink = desc.mUpperLidBlink;
    if (lowerBlink && upperBlink) {
        Vector3 sourcePos = source->WorldXfm().v;
        Vector3 upperBlinkPos = upperBlink->WorldXfm().v;
        Vector3 lowerBlinkPos = lowerBlink->WorldXfm().v;

        Vector3 upperDir(
            upperBlinkPos.x - sourcePos.x,
            upperBlinkPos.y - sourcePos.y,
            upperBlinkPos.z - sourcePos.z
        );
        Normalize(upperDir, upperDir);

        Vector3 lowerDir(
            lowerBlinkPos.x - sourcePos.x,
            lowerBlinkPos.y - sourcePos.y,
            lowerBlinkPos.z - sourcePos.z
        );
        Normalize(lowerDir, lowerDir);

        Vector3 cross;
        Cross(lowerDir, upperDir, cross);

        const Transform &srcXfm = source->WorldXfm();
        bool notLidsOK =
            cross.x * srcXfm.m.x.x + cross.y * srcXfm.m.x.y + cross.z * srcXfm.m.x.z
            > 0.0f;

        if (!sDisableEyeClamping && !DataVariable("disable_clamping").Int(0) && notLidsOK) {
            Vector3 mid;
            mid.x = (upperBlinkPos.x - lowerBlinkPos.x) * 0.5f + lowerBlinkPos.x;
            mid.y = (upperBlinkPos.y - lowerBlinkPos.y) * 0.5f + lowerBlinkPos.y;
            mid.z = (upperBlinkPos.z - lowerBlinkPos.z) * 0.5f + lowerBlinkPos.z;

            Vector3 clampOff;
            clampOff.x = mid.x - lowerBlinkPos.x;
            clampOff.y = mid.y - lowerBlinkPos.y;
            clampOff.z = mid.z - lowerBlinkPos.z;

            Vector3 newLowerPos = lowerLid->WorldXfm().v;
            newLowerPos.x += clampOff.x;
            newLowerPos.y += clampOff.y;
            newLowerPos.z += clampOff.z;
            lowerLid->SetWorldPos(newLowerPos);

            const Vector3 &ulidPos = upperLid->WorldXfm().v;
            Vector3 origDir(
                upperBlinkPos.x - ulidPos.x,
                upperBlinkPos.y - ulidPos.y,
                upperBlinkPos.z - ulidPos.z
            );
            Normalize(origDir, origDir);

            const Vector3 &ulidPos2 = upperLid->WorldXfm().v;
            Vector3 newDir(
                mid.x - ulidPos2.x,
                mid.y - ulidPos2.y,
                mid.z - ulidPos2.z
            );
            Normalize(newDir, newDir);

            float dot = Dot(origDir, newDir);
            float clamped = Clamp<float>(-1.0f, 1.0f, dot);
            float angle = std::acos(clamped);

            Transform &xfm = upperLid->DirtyLocalXfm();
            RotateAboutZ(xfm.m, -angle, xfm.m);
        }

        if (DataVariable("debug_clamping").Int(0)) {
            RndGraph *graph = RndGraph::GetOneFrame();

            graph->AddSphere(
                upperBlinkPos, 0.05f,
                notLidsOK ? Hmx::Color(1.0f, 0.0f, 0.0f, 1.0f)
                          : Hmx::Color(0.0f, 0.0f, 1.0f, 1.0f)
            );
            graph->AddSphere(
                lowerBlinkPos, 0.05f,
                notLidsOK ? Hmx::Color(1.0f, 0.0f, 0.0f, 1.0f)
                          : Hmx::Color(0.0f, 0.0f, 1.0f, 1.0f)
            );
            graph->AddSphere(sourcePos, 0.05f, Hmx::Color(0.0f, 0.0f, 1.0f, 1.0f));

            Hmx::Color cyanColor(0.0f, 1.0f, 1.0f, 1.0f);
            graph->AddLine(sourcePos, upperBlinkPos, cyanColor, false);
            graph->AddLine(sourcePos, lowerBlinkPos, cyanColor, false);

            Normalize(cross, cross);
            Vector3 normalEnd(
                cross.x + sourcePos.x, cross.y + sourcePos.y, cross.z + sourcePos.z
            );
            graph->AddLine(
                sourcePos, normalEnd,
                notLidsOK ? Hmx::Color(1.0f, 0.0f, 0.0f, 1.0f)
                          : Hmx::Color(0.0f, 1.0f, 0.0f, 1.0f),
                false
            );

            const Transform &srcXfm2 = source->WorldXfm();
            Vector3 facingEnd(
                srcXfm2.m.x.x + sourcePos.x,
                srcXfm2.m.x.y + sourcePos.y,
                srcXfm2.m.x.z + sourcePos.z
            );
            graph->AddLine(
                sourcePos, facingEnd, Hmx::Color(1.0f, 1.0f, 0.0f, 1.0f), false
            );

            if (notLidsOK) {
                Vector3 mid2(
                    (upperBlinkPos.x - lowerBlinkPos.x) * 0.5f + lowerBlinkPos.x,
                    (upperBlinkPos.y - lowerBlinkPos.y) * 0.5f + lowerBlinkPos.y,
                    (upperBlinkPos.z - lowerBlinkPos.z) * 0.5f + lowerBlinkPos.z
                );
                graph->AddSphere(
                    mid2, 0.03f, Hmx::Color(1.0f, 0.0f, 1.0f, 1.0f)
                );
            }
        }
    }

    if (!DataVariable("disable_llidnorm").Int(0) && !mLowerLidTrackRotate && dist > 0.0f) {
        Vector3 srcPos = source->WorldXfm().v;
        Vector3 lidPos = lowerLid->WorldXfm().v;
        Vector3 dir(
            lidPos.x - srcPos.x, lidPos.y - srcPos.y, lidPos.z - srcPos.z
        );
        Normalize(dir, dir);
        Vector3 clampedPos(
            dir.x * dist + srcPos.x, dir.y * dist + srcPos.y, dir.z * dist + srcPos.z
        );
        lowerLid->SetWorldPos(clampedPos);
    }
}

void CharEyes::ProceduralBlinkUpdate() {
    static DataNode &disable = DataVariable("cheat.disable_procedural_blinks");

    if (sDisableProceduralBlink)
        return;
    if (disable.Int(0))
        return;
    if (!mBlinksEnabled && !mProceduralBlink)
        return;

    mBlinkWindowSecsRemaining -= TheTaskMgr.DeltaSeconds();
    if (mBlinkWindowSecsRemaining < 0.0f) {
        mBlinkWindowCount = 0;
        mBlinkWindowSecsRemaining = 15.0f;
    }

    if (!mFaceServo)
        return;
    if (!mProceduralBlink)
        return;

    float elapsed = TheTaskMgr.Seconds(TaskMgr::kRealTime) - mBlinkTimestamp;
    if (elapsed < 0.115f) {
        float t = Clamp(0.0f, 1.0f, elapsed / 0.115f);
        float blinkWeight = EaseInExp(t);
        mFaceServo->SetProceduralBlinkWeight(blinkWeight);
    } else if (elapsed < 0.3f) {
        float t = Clamp(0.0f, 1.0f, 1.0f - (elapsed - 0.115f) / 0.185f);
        float blinkWeight = Sigmoid(t);
        mFaceServo->SetProceduralBlinkWeight(blinkWeight);
        mTarget = mDelayedTarget;
    } else {
        mFaceServo->SetProceduralBlinkWeight(0.0f);
        mProceduralBlink = false;
        mTarget = mDelayedTarget;
    }
}

void CharEyes::SetEnableBlinks(bool b1, bool b2) {
    mBlinksEnabled = b1;
    if (b2 && !mBlinksEnabled && mProceduralBlink && mFaceServo) {
        mFaceServo->SetProceduralBlinkWeight(0);
        mProceduralBlink = false;
        mTarget = mDelayedTarget;
    }
}

// fn_804CCF70
RndTransformable *CharEyes::GetHead() {
    if (mViewDirection)
        return mViewDirection;
    else if (!mEyes.empty() && mEyes[0].mEye) {
        RndTransformable *src = mEyes[0].mEye->GetSource();
        if (src)
            return src->TransParent();
    }
    return 0;
}

CharInterest *CharEyes::GetCurrentInterest() {
    if (mFocusInterest)
        return mFocusInterest;
    if (mCurInterest)
        return mCurInterest;
    return 0;
}

void CharEyes::Replace(Hmx::Object *from, Hmx::Object *to) {
    Hmx::Object::Replace(from, to);
    CharWeightable::Replace(from, to);
    CharPollable::Replace(from, to);
    for (ObjVector<EyeDesc>::iterator it = mEyes.begin(); it != mEyes.end(); it) {
        if (it->mEye == from) {
            it->mEye = dynamic_cast<CharLookAt *>(to);
        }
        if (!it->mEye)
            it = mEyes.erase(it);
        else
            ++it;
    }
    for (ObjVector<CharInterestState>::iterator it = mInterests.begin();
         it != mInterests.end();
         it) {
        if (it->mInterest == from) {
            it->mInterest = dynamic_cast<CharInterest *>(to);
        }
        if (!it->mInterest)
            it = mInterests.erase(it);
        else
            ++it;
    }
}

void CharEyes::ForceBlink() {
    mProceduralBlink = true;
    mBlinkTimestamp = TheTaskMgr.Seconds(TaskMgr::kRealTime);
    mBlinkWindowCount++;
}

void CharEyes::ListPollChildren(std::list<RndPollable *> &plist) const {
    for (ObjVector<EyeDesc>::const_iterator it = mEyes.begin(); it != mEyes.end(); ++it) {
        plist.push_back((*it).mEye);
    }
}

void CharEyes::PollDeps(
    std::list<Hmx::Object *> &changedBy, std::list<Hmx::Object *> &change
) {
    for (ObjVector<CharInterestState>::iterator it = mInterests.begin();
         it != mInterests.end();
         ++it) {
        ObjectDir *dir = (*it).mInterest->Dir();
        if (dir == Dir()) {
            changedBy.push_back((*it).mInterest);
        }
    }
    if (!mEyes.empty()) {
        changedBy.push_back(GetHead());
        change.push_back(GetTarget());
    }
    if (mHeadLookAt)
        changedBy.push_back(mHeadLookAt);
    if (mFaceServo)
        changedBy.push_back(mFaceServo);
}

SAVE_OBJ(CharEyes, 0x575)

BinStream &operator>>(BinStream &bs, CharEyes::EyeDesc &desc) {
    bs >> desc.mEye;
    bs >> desc.mUpperLid;
    if (CharEyes::gRev > 6)
        bs >> desc.mLowerLid;
    if (CharEyes::gRev > 0xF) {
        bs >> desc.mUpperLidBlink;
        bs >> desc.mLowerLidBlink;
    }
    return bs;
}

BinStream &operator>>(BinStream &bs, CharEyes::CharInterestState &state) {
    bs >> state.mInterest;
    return bs;
}

BEGIN_LOADS(CharEyes)
    LOAD_REVS(bs)
    ASSERT_REVS(0x12, 0)
    LOAD_SUPERCLASS(Hmx::Object)
    if (gRev > 5)
        LOAD_SUPERCLASS(CharWeightable)
    if (gRev > 4)
        bs >> mEyes;
    else {
        ObjPtrList<CharLookAt> pList(this, kObjListNoNull);
        bs >> pList;
        mEyes.resize(pList.size());
        int idx = 0;
        for (ObjPtrList<CharLookAt>::iterator it = pList.begin(); it != pList.end();
             ++it) {
            mEyes[idx].mEye = *it;
            mEyes[idx].mUpperLid = 0;
            mEyes[idx].mLowerLid = 0;
            mEyes[idx].mLowerLidBlink = 0;
            mEyes[idx].mUpperLidBlink = 0;
            idx++;
        }
    }
    if (gRev == 3 || gRev == 4) {
        ObjPtr<RndTransformable> tPtr(this);
        bs >> tPtr;
    }
    mInterests.clear();
    if (gRev >= 4 && gRev <= 8) {
        ObjPtr<RndTransformable> tPtr(this);
        int cnt;
        bs >> cnt;
        for (int i = 0; i < cnt; i++) {
            bs >> tPtr;
            int x;
            bs >> x;
        }
    } else if (gRev > 8)
        bs >> mInterests;
    if (gRev > 4)
        bs >> mFaceServo;
    else
        mFaceServo = 0;
    if (gRev > 7)
        bs >> mCamWeight;
    if (gRev > 9)
        bs >> mDefaultFilterFlags;
    if (gRev > 10)
        bs >> mViewDirection;
    if (gRev > 0xB)
        bs >> mHeadLookAt;
    if (gRev > 0xC)
        bs >> mMaxExtrapolation;
    if (gRev > 0xD)
        bs >> mMinTargetDist;
    if (gRev > 0xE) {
        bs >> mUpperLidTrackUp;
        bs >> mUpperLidTrackDown;
        bs >> mLowerLidTrackUp;
        if (gRev < 0x11) {
            int x, y;
            bs >> x;
            bs >> mLowerLidTrackDown;
            bs >> y;
        } else
            bs >> mLowerLidTrackDown;
    }
    if (gRev > 0x11)
        bs >> mLowerLidTrackRotate;
END_LOADS

BEGIN_COPYS(CharEyes)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(CharWeightable)
    CREATE_COPY(CharEyes)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mEyes)
        COPY_MEMBER(mInterests)
        COPY_MEMBER(mFaceServo)
        COPY_MEMBER(mLastFacing)
        COPY_MEMBER(mLastLook)
        COPY_MEMBER(mCamWeight)
        COPY_MEMBER(mDefaultFilterFlags)
        COPY_MEMBER(mViewDirection)
        COPY_MEMBER(mHeadLookAt)
        COPY_MEMBER(mMaxExtrapolation)
        COPY_MEMBER(mMinTargetDist)
        COPY_MEMBER(mUpperLidTrackUp)
        COPY_MEMBER(mUpperLidTrackDown)
        COPY_MEMBER(mLowerLidTrackUp)
        COPY_MEMBER(mLowerLidTrackDown)
        COPY_MEMBER(mLowerLidTrackRotate)
    END_COPYING_MEMBERS
END_COPYS

DataNode CharEyes::OnToggleForceFocus(DataArray *da) {
    if (mFocusInterest)
        SetFocusInterest(0, 0);
    else
        SetFocusInterest(mCurInterest, 0);
    return 0;
}

DataNode CharEyes::OnToggleInterestOverlay(DataArray *da) {
    ToggleInterestsDebugOverlay();
    return 0;
}

void CharEyes::ToggleInterestsDebugOverlay() {
    if (mOverlay)
        mOverlay->SetShowing(!mOverlay->Showing());
}

BEGIN_HANDLERS(CharEyes)
    HANDLE(add_interest, OnAddInterest)
    HANDLE_ACTION(force_blink, ForceBlink())
#ifdef MILO_DEBUG
    HANDLE(toggle_force_focus, OnToggleForceFocus)
    HANDLE(toggle_interest_overlay, OnToggleInterestOverlay)
#endif
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0x660)
END_HANDLERS

DataNode CharEyes::OnAddInterest(DataArray *arr) {
    mInterests.push_back(CharInterestState(arr->Obj<CharInterest>(1)));
    return 0;
}

BEGIN_CUSTOM_PROPSYNC(CharEyes::EyeDesc)
    SYNC_PROP(eye, o.mEye)
    SYNC_PROP(upper_lid, o.mUpperLid)
    SYNC_PROP(lower_lid, o.mLowerLid)
    SYNC_PROP(upper_lid_blink, o.mUpperLidBlink)
    SYNC_PROP(lower_lid_blink, o.mLowerLidBlink)
END_CUSTOM_PROPSYNC

BEGIN_CUSTOM_PROPSYNC(CharEyes::CharInterestState)
    SYNC_PROP(interest, o.mInterest)
END_CUSTOM_PROPSYNC

BEGIN_PROPSYNCS(CharEyes)
    SYNC_PROP(eyes, mEyes)
    SYNC_PROP(view_direction, mViewDirection)
    SYNC_PROP(interests, mInterests)
    SYNC_PROP(face_servo, mFaceServo)
    SYNC_PROP(camera_weight, mCamWeight)
    SYNC_PROP_BITFIELD_STATIC(default_interest_categories, mDefaultFilterFlags, 0x67B)
    SYNC_PROP(head_lookat, mHeadLookAt)
    SYNC_PROP(max_extrapolation, mMaxExtrapolation)
#ifdef MILO_DEBUG
    SYNC_PROP(disable_eye_dart, sDisableEyeDart)
    SYNC_PROP(disable_eye_jitter, sDisableEyeJitter)
    SYNC_PROP(disable_interest_objects, sDisableInterestObjects)
    SYNC_PROP(disable_procedural_blink, sDisableProceduralBlink)
    SYNC_PROP(disable_eye_clamping, sDisableEyeClamping)
    SYNC_PROP_BITFIELD_STATIC(interest_filter_testing, mInterestFilterFlags, 0x684)
#endif
    SYNC_PROP(min_target_dist, mMinTargetDist)
    SYNC_PROP(ulid_track_up, mUpperLidTrackUp)
    SYNC_PROP(ulid_track_down, mUpperLidTrackDown)
    SYNC_PROP(llid_track_up, mLowerLidTrackUp)
    SYNC_PROP(llid_track_down, mLowerLidTrackDown)
    SYNC_PROP(llid_track_rotate, mLowerLidTrackRotate)
    SYNC_SUPERCLASS(CharWeightable)
END_PROPSYNCS
