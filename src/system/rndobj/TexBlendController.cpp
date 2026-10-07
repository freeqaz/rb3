#include "rndobj/TexBlendController.h"
#include "rndobj/Mesh.h"
#include "rndobj/Tex.h"
#include "rndobj/Trans.h"
#include "math/Utl.h"
#include "obj/PropSync_p.h"
#include "utl/Symbols.h"

unsigned short RndTexBlendController::gRev = 0;

RndTexBlendController::RndTexBlendController()
    : mMesh(this, 0), mObject1(this, 0), mObject2(this, 0), mReferenceDistance(0.0f),
      mMinDistance(0.0f), mMaxDistance(0.0f), mTex(this, 0) {}

RndTexBlendController::~RndTexBlendController() {}

// fn_806413C4
bool RndTexBlendController::GetCurrentDistance(float &dist) const {
    if (mObject1 && mObject2) {
#ifdef MILO_DEBUG
        dist = Distance(mObject1->WorldXfm().v, mObject2->WorldXfm().v);
#else
        Transform &t = mObject1->WorldXfm();
        dist = Distance(t.v, mObject2->WorldXfm().v);
#endif
        return true;
    } else {
        dist = 0;
        return false;
    }
}

#ifdef HX_NATIVE
// Retail Xbox RndTexBlendController::IsValid and GetBlendState (rb3-xenon
// rndobj/TexBlendController.cpp, matched against the TU5 binary).
bool RndTexBlendController::IsValid() const {
    if (!mMesh)
        return false;
    if (!mTex) {
        float refDist = mReferenceDistance;
        bool distValid = !(mMinDistance > refDist) || !(mMaxDistance < refDist);
        return mObject1 && mObject2 && refDist > 0 && distValid;
    }
    return true;
}

RndTexBlendController::BlendState
RndTexBlendController::GetBlendState(float &blend, float influence) const {
    BlendState state = kBlendNone;
    blend = 0.0f;
    if (IsValid()) {
        if (mTex) {
            blend = 1.0f;
            state = kBlendCustom;
        } else {
            float dist;
            if (GetCurrentDistance(dist) && mReferenceDistance > 0.0f) {
                if (dist < mReferenceDistance) {
                    float denom = mReferenceDistance - mMinDistance;
                    if (denom > 0.0f) {
                        state = kBlendNear;
                        blend = (mReferenceDistance - Max(dist, mMinDistance)) / denom;
                    }
                } else if (dist > mReferenceDistance) {
                    float denom = mMaxDistance - mReferenceDistance;
                    if (denom > 0.0f) {
                        state = kBlendFar;
                        blend = (Min(dist, mMaxDistance) - mReferenceDistance) / denom;
                    }
                }
            }
            // Smoothstep.
            float t2 = blend * blend;
            float t3 = blend * t2;
            blend = t3 * -2.0f + t2 * 3.0f;
        }
    }
    blend *= influence;
    blend = Clamp(0.0f, 1.0f, blend);
    // Quantised to the 8-bit alpha the draw writes.
    blend = (unsigned char)(blend * 255.0f) * (1.0f / 255.0f);
    if (blend < 1.0f / 255.0f)
        state = kBlendNone;
    return state;
}
#endif

void RndTexBlendController::UpdateReferenceDistance() {
    GetCurrentDistance(mReferenceDistance);
    mMinDistance = Min(mMinDistance, mReferenceDistance);
    mMaxDistance = Max(mMaxDistance, mReferenceDistance);
}

void RndTexBlendController::UpdateMinDistance() {
    GetCurrentDistance(mMinDistance);
    mMinDistance = Min(mMinDistance, mReferenceDistance);
}

void RndTexBlendController::UpdateMaxDistance() {
    GetCurrentDistance(mMaxDistance);
    mMaxDistance = Max(mMaxDistance, mReferenceDistance);
}

void RndTexBlendController::UpdateAllDistances() {
    UpdateReferenceDistance();
    mMinDistance = mReferenceDistance * 0.5f;
    mMaxDistance = mReferenceDistance * 1.5f;
}

BEGIN_COPYS(RndTexBlendController)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY(RndTexBlendController)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mMesh)
        COPY_MEMBER(mObject1)
        COPY_MEMBER(mObject2)
        COPY_MEMBER(mReferenceDistance)
        COPY_MEMBER(mMinDistance)
        COPY_MEMBER(mMaxDistance)
        COPY_MEMBER(mTex)
    END_COPYING_MEMBERS
END_COPYS

SAVE_OBJ(RndTexBlendController, 0xF5)

void RndTexBlendController::Load(BinStream &bs) {
    int rev;
    bs >> rev;
#ifdef MILO_DEBUG
    if (rev > 2) {
        MILO_FAIL(
            "%s can't load new %s version %d > %d",
            PathName(this),
            ClassName(),
            rev,
            (unsigned short)2
        );
    }
#endif
#ifdef HX_NATIVE
    // The Wii fork never stores the revision, so `gRev > 1` below was always
    // false and a rev 2 controller left its override map (mTex) unread.
    // Retail Xbox stores it (rb3-xenon: gRev = getHmxRev(rev)); the head's
    // norm_*.texblendctl controllers are rev 2.
    if (getenv("RB3_TEXBLEND_PROBE"))
        fprintf(stderr, "[TEXBLEND] load ctl '%s' rev=%d\n", Name() ? Name() : "?", rev);
    gRev = rev;
#endif
    Hmx::Object::Load(bs);
    bs >> mMesh;
    bs >> mObject1;
    bs >> mObject2;
#ifdef VERSION_SZBE69_B8
    bs >> mReferenceDistance >> mMinDistance >> mMaxDistance;
#else
    bs >> mReferenceDistance;
    bs >> mMinDistance;
    bs >> mMaxDistance;
#endif
    if (gRev > 1)
        bs >> mTex;
}

BEGIN_HANDLERS(RndTexBlendController)
    HANDLE_ACTION(set_min_distance, UpdateMinDistance())
    HANDLE_ACTION(set_max_distance, UpdateMaxDistance())
    HANDLE_ACTION(set_base_distance, UpdateReferenceDistance())
    HANDLE_ACTION(set_all_distances, UpdateAllDistances())
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0x129)
END_HANDLERS

BEGIN_PROPSYNCS(RndTexBlendController)
    SYNC_PROP_MODIFY_ALT(reference_object_1, mObject1, UpdateAllDistances())
    SYNC_PROP_MODIFY_ALT(reference_object_2, mObject2, UpdateAllDistances())
    SYNC_PROP(mesh, mMesh)
    SYNC_PROP(base_distance, mReferenceDistance)
    SYNC_PROP(min_distance, mMinDistance)
    SYNC_PROP(max_distance, mMaxDistance)
    SYNC_PROP(override_map, mTex)
END_PROPSYNCS
