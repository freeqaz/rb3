#include "char/CharInterest.h"
#include "math/Utl.h"
#include "math/Rand.h"
#include "obj/Data.h"
#include "obj/ObjMacros.h"
#include "obj/DataUtl.h"
#include "obj/PropSync_p.h"
#include "rndobj/Cam.h"
#include "rndobj/Trans.h"
#include "rndobj/Graph.h"
#include "utl/Symbols.h"

INIT_REVS(CharInterest)

CharInterest::CharInterest()
    : mMaxViewAngle(20.0f), mPriority(1.0f), mMinLookTime(1.0f), mMaxLookTime(3.0f),
      mRefractoryPeriod(6.1f), mDartOverride(this), mCategoryFlags(0),
      mOverrideMinTargetDistance(0), mMinTargetDistanceOverride(35.0f) {
    SyncMaxViewAngle();
}

CharInterest::~CharInterest() {}

void CharInterest::SyncMaxViewAngle() {
    mMaxViewAngleCos = std::cos(mMaxViewAngle * 0.017453292f);
}

// https://decomp.me/scratch/ekyoO retail scratch
void CharInterest::Highlight() {
    RndGraph *oneframe = RndGraph::GetOneFrame();
    oneframe->AddSphere(WorldXfm().v, 1.0f, Hmx::Color(1.0f, 0.0f, 0.0f));
    Vector2 vec2;
    float wts = RndCam::sCurrent->WorldToScreen(WorldXfm().v, vec2);
    if (wts > 0.0f) {
        vec2.x = vec2.x * TheRnd->Width();
        vec2.y = vec2.y * TheRnd->Height();
        vec2.y += 15.0;
        vec2.x -= 30.0;
        oneframe->AddString(MakeString("%s", Name()), vec2, Hmx::Color(1.0f, 1.0f, 1.0f));
    }
    if (mDartOverride) {
        const DataNode *minrad = mDartOverride->Property("min_radius", false);
        const DataNode *maxrad = mDartOverride->Property("max_radius", false);
        if (minrad && maxrad) {
            oneframe->AddSphere(
                WorldXfm().v, minrad->Float(), Hmx::Color(0.7f, 0.7f, 0.7f)
            );
            oneframe->AddSphere(
                WorldXfm().v, maxrad->Float(), Hmx::Color(1.0f, 1.0f, 1.0f)
            );
        }
    }
}

SAVE_OBJ(CharInterest, 0x52);

BEGIN_LOADS(CharInterest)
    LOAD_REVS(bs)
    ASSERT_REVS(6, 0)
    LOAD_SUPERCLASS(Hmx::Object)
    LOAD_SUPERCLASS(RndTransformable)
    bs >> mMaxViewAngle;
    bs >> mPriority;
    bs >> mMinLookTime;
    bs >> mMaxLookTime;
    bs >> mRefractoryPeriod;
    if (gRev >= 2 && gRev <= 5) {
        ObjPtr<Hmx::Object> obj(this);
        bs >> obj;
    } else if (gRev > 5) {
        bs >> mDartOverride;
    }
    if (gRev > 2) {
        bs >> mCategoryFlags;
        if (gRev == 3) {
            u8 x;
            bs >> x;
        }
    }
    if (gRev > 4) {
        bs >> mOverrideMinTargetDistance;
        bs >> mMinTargetDistanceOverride;
    }
    SyncMaxViewAngle();
END_LOADS

BEGIN_COPYS(CharInterest)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(RndTransformable)
    CREATE_COPY(CharInterest)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mMaxViewAngle)
        COPY_MEMBER(mPriority)
        COPY_MEMBER(mMinLookTime)
        COPY_MEMBER(mMaxLookTime)
        COPY_MEMBER(mRefractoryPeriod)
        COPY_MEMBER(mDartOverride)
        COPY_MEMBER(mCategoryFlags)
        COPY_MEMBER(mOverrideMinTargetDistance)
        COPY_MEMBER(mMinTargetDistanceOverride)
        SyncMaxViewAngle();
    END_COPYING_MEMBERS
END_COPYS

CharEyeDartRuleset *CharInterest::GetDartRulesetOverride() const { return mDartOverride; }

bool CharInterest::IsWithinViewCone(const Vector3 &v1, const Vector3 &v2) {
    Vector3 v1c;
    v1c = WorldXfm().v;
    Vector3 v28;
    Subtract(v1c, v1, v28);
    Normalize(v28, v28);
    if (Dot(v2, v28) >= mMaxViewAngleCos)
        return true;
    else
        return false;
}

bool CharInterest::IsMatchingFilterFlags(int mask) {
    return (mCategoryFlags & mask) == mCategoryFlags && mCategoryFlags != 0;
}

// retail scratch: https://decomp.me/scratch/JpD0G
float CharInterest::ComputeScore(
    const Vector3 &v1, const Vector3 &v2, const Vector3 &v3, float f, int i, bool b
) {
    float score = -0.99f;
    int r = 1;
    int catFlags = mCategoryFlags;
    int m = 0;
    if ((catFlags & i) == catFlags && catFlags != 0)
        m = 1;
    if (m != 0)
        goto done;
    {
        int bc = 0;
        if (b && catFlags == 0)
            bc = 1;
        if (bc == 0)
            r = 0;
    }
done:
    if (r == 0)
        return -1.0f;
    Vector3 v7c(WorldXfm().v);
    Vector3 v88;
    Subtract(v7c, v2, v88);
    float xx = v88.x * v88.x;
    float yy = v88.y * v88.y;
    float zz = v88.z * v88.z;
    float lensq = xx + yy + zz;
    Normalize(v88, v88);

    float f1score = 0.0f;
    if (Dot(v1, v88) >= mMaxViewAngleCos)
        f1score = 1.0f;

    float f2score = 0.0f;
    if (Dot(v3, v88) >= mMaxViewAngleCos)
        f2score = 1.0f;

    float f7 = -(lensq * f - 1.0f);
    if (f7 != f7) {
        f7 = 0.2f;
    }
    if (!(f7 >= -0.0001f)) {
        MILO_FAIL("error scoring interest object: bad normalize factor gave score %f", f7);
    }

    score += f2score + (f7 + f1score);
    if (score >= 0.0f) {
        score += RandomFloat(-0.25f, 0.25f);
    }
    return score * mPriority;
}

BEGIN_PROPSYNCS(CharInterest)
    SYNC_PROP_MODIFY(max_view_angle, mMaxViewAngle, SyncMaxViewAngle())
    SYNC_PROP(priority, mPriority)
    SYNC_PROP(min_look_time, mMinLookTime)
    SYNC_PROP(max_look_time, mMaxLookTime)
    SYNC_PROP(refractory_period, mRefractoryPeriod)
    SYNC_PROP(dart_ruleset_override, mDartOverride)
    SYNC_PROP_BITFIELD_STATIC(category_flags, mCategoryFlags, 0x138)
    SYNC_PROP(overrides_min_target_dist, mOverrideMinTargetDistance)
    SYNC_PROP(min_target_dist_override, mMinTargetDistanceOverride)
    SYNC_SUPERCLASS(RndTransformable)
END_PROPSYNCS

BEGIN_HANDLERS(CharInterest)
    HANDLE_SUPERCLASS(RndTransformable)
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0x141)
END_HANDLERS
