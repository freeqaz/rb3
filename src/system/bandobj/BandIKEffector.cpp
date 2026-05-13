#define CHARHAIR_LOCAL_MULTIPLY
#include "bandobj/BandIKEffector.h"
#include "math/Mtx.h"
#include "math/Rot.h"
#include "utl/Symbols.h"

#ifdef __MWERKS__
inline void Multiply(const Hmx::Matrix3 &a, const Hmx::Matrix3 &b, Hmx::Matrix3 &out) {
    typedef __vec2x32float__ psq;
    register const Hmx::Matrix3 *_a = &a;
    register const Hmx::Matrix3 *_b = &b;
    register Hmx::Matrix3 *_out = &out;
    float row0[3], row1[3], row2[3];
    register psq _f0, _f1, _f2, _f3, _f4, _f5, _f6, _f7, _f8, _f9, _f10, _f11, _f12;
    asm { cmplw _b, _out }
    asm volatile {
        beq alias_path
        // non-alias path
        psq_l  _f4, 0x4(_a),  0, 0
        psq_l  _f3, 0x18(_b), 0, 0
        psq_l  _f2, 0x20(_b), 1, 0
        ps_muls1 _f1, _f3, _f4
        psq_l  _f3, 0xc(_b),  0, 0
        ps_muls1 _f0, _f2, _f4
        psq_l  _f2, 0x14(_b), 1, 0
        psq_l  _f9, 0x10(_a), 0, 0
        psq_l  _f8, 0x18(_b), 0, 0
        psq_l  _f7, 0x20(_b), 1, 0
        ps_madds0 _f1, _f3, _f4, _f1
        ps_madds0 _f0, _f2, _f4, _f0
        psq_l  _f4, 0x0(_a),  0, 0
        ps_muls1 _f6, _f8, _f9
        psq_l  _f3, 0x0(_b),  0, 0
        ps_muls1 _f5, _f7, _f9
        ps_madds0 _f1, _f3, _f4, _f1
        psq_l  _f2, 0x8(_b),  1, 0
        psq_l  _f8, 0xc(_b),  0, 0
        ps_madds0 _f0, _f2, _f4, _f0
        psq_l  _f7, 0x14(_b), 1, 0
        ps_madds0 _f6, _f8, _f9, _f6
        psq_l  _f2, 0xc(_a),  0, 0
        ps_madds0 _f5, _f7, _f9, _f5
        psq_l  _f4, 0x1c(_a), 0, 0
        psq_l  _f7, 0x1c(_b), 0, 0
        psq_l  _f3, 0x18(_b), 0, 0
        ps_madds0 _f6, _f1, _f2, _f6
        ps_madds0 _f5, _f0, _f2, _f5
        psq_l  _f8, 0x20(_b), 1, 0
        ps_muls1 _f3, _f3, _f7
        psq_l  _f9, 0x18(_a), 0, 0
        ps_muls1 _f2, _f8, _f7
        psq_st _f1, 0x0(_out), 0, 0
        ps_madds0 _f6, _f3, _f9, _f6
        ps_madds0 _f5, _f2, _f9, _f5
        psq_st _f0, 0x8(_out), 1, 0
        ps_madds0 _f3, _f1, _f4, _f3
        psq_st _f6, 0xc(_out), 0, 0
        ps_madds0 _f2, _f0, _f4, _f2
        psq_st _f5, 0x14(_out), 1, 0
        psq_st _f3, 0x18(_out), 0, 0
        psq_st _f2, 0x20(_out), 1, 0
        b mult_end
    alias_path:
        psq_l  _f4, 0x4(_a),  0, 0
        la r7, row2
        psq_l  _f3, 0x18(_out), 0, 0
        la r6, row1
        psq_l  _f2, 0x20(_out), 1, 0
        la r5, row0
        ps_muls1 _f1, _f3, _f4
        psq_l  _f3, 0xc(_out), 0, 0
        ps_muls1 _f0, _f2, _f4
        psq_l  _f2, 0x14(_out), 1, 0
        psq_l  _f9, 0x10(_a),  0, 0
        psq_l  _f8, 0x18(_out), 0, 0
        psq_l  _f7, 0x20(_out), 1, 0
        ps_madds0 _f1, _f3, _f4, _f1
        ps_muls1 _f6, _f8, _f9
        psq_l  _f12, 0x1c(_a), 0, 0
        ps_mr  _f8, _f3
        psq_l  _f3, 0x18(_out), 0, 0
        ps_muls1 _f5, _f7, _f9
        ps_muls1 _f11, _f3, _f12
        ps_mr  _f7, _f2
        psq_l  _f3, 0x0(_out), 0, 0
        ps_madds0 _f0, _f2, _f4, _f0
        psq_l  _f2, 0x20(_out), 1, 0
        psq_l  _f4, 0x0(_a),  0, 0
        ps_muls1 _f10, _f2, _f12
        psq_l  _f2, 0x8(_out), 1, 0
        ps_madds0 _f1, _f3, _f4, _f1
        ps_madds0 _f6, _f8, _f9, _f6
        ps_madds0 _f0, _f2, _f4, _f0
        psq_l  _f4, 0x18(_a), 0, 0
        ps_madds0 _f5, _f7, _f9, _f5
        psq_l  _f9, 0xc(_a),  0, 0
        ps_madds0 _f11, _f8, _f12, _f11
        ps_madds0 _f10, _f7, _f12, _f10
        psq_st _f1, 0x0(r7), 0, 0
        ps_madds0 _f6, _f3, _f9, _f6
        ps_madds0 _f5, _f2, _f9, _f5
        ps_madds0 _f11, _f3, _f4, _f11
        lfs    _f8, 0x0(r7)
        ps_madds0 _f10, _f2, _f4, _f10
        psq_st _f6, 0x0(r6), 0, 0
        lfs    _f7, 0x4(r7)
        psq_st _f11, 0x0(r5), 0, 0
        lfs    _f4, 0x4(r6)
        psq_st _f5, 0x8(r6), 1, 0
        lfs    _f5, 0x0(r6)
        psq_st _f0, 0x8(r7), 1, 0
        lfs    _f3, 0x8(r6)
        psq_st _f10, 0x8(r5), 1, 0
        lfs    _f6, 0x8(r7)
        lfs    _f2, 0x0(r5)
        lfs    _f1, 0x4(r5)
        lfs    _f0, 0x8(r5)
        stfs   _f8, 0x0(_out)
        stfs   _f7, 0x4(_out)
        stfs   _f6, 0x8(_out)
        stfs   _f5, 0xc(_out)
        stfs   _f4, 0x10(_out)
        stfs   _f3, 0x14(_out)
        stfs   _f2, 0x18(_out)
        stfs   _f1, 0x1c(_out)
        stfs   _f0, 0x20(_out)
    mult_end:
    }
}
#endif

INIT_REVS(BandIKEffector);
CharClip *BandIKEffector::sDeformClip;

BandIKEffector::Constraint::Constraint(Hmx::Object *o)
    : mTarget(o, 0), mFinger(o, 0), mWeight(1.0f) {}

BandIKEffector::Constraint::Constraint(const BandIKEffector::Constraint &c)
    : mTarget(c.mTarget), mFinger(c.mFinger), mWeight(c.mWeight) {}

BandIKEffector::Constraint &
BandIKEffector::Constraint::operator=(const BandIKEffector::Constraint &c) {
    mTarget = c.mTarget;
    mFinger = c.mFinger;
    mWeight = c.mWeight;
    return *this;
}

BandIKEffector::BandIKEffector()
    : mEffector(this, 0), mGround(this, 0), mMore(this, 0), mElbow(this, 0),
      mConstraints(this), unk64(this, 0) {}

BandIKEffector::~BandIKEffector() {}

void BandIKEffector::SetName(const char *cc, ObjectDir *dir) {
    Hmx::Object::SetName(cc, dir);
    unk64 = dynamic_cast<BandCharacter *>(dir);
}

void BandIKEffector::SetDeformClip(Hmx::Object *o) {
    static Symbol bc("BandCharacter");
    if (o->ClassName() == bc) {
        sDeformClip =
            BandCharDesc::GetDeformClip(dynamic_cast<BandCharacter *>(o)->mGender);
    } else
        sDeformClip = 0;
}

void BandIKEffector::NeutralWorldXfm(RndTransformable *trans, Transform &tf) {
    RndTransformable *parent = trans->TransParent();
    if (!parent) {
        SetDeformClip(trans);
        NeutralLocalXfm(trans, tf);
    } else {
        Transform tf38;
        NeutralWorldXfm(parent, tf);
        NeutralLocalXfm(trans, tf38);
        Multiply(tf38, tf, tf);
    }
}

void BandIKEffector::Highlight() {}

BinStream &operator>>(BinStream &bs, BandIKEffector::Constraint &c) {
    bs >> c.mTarget;
    bs >> c.mFinger;
    if (BandIKEffector::gRev > 2)
        bs >> c.mWeight;
    return bs;
}

SAVE_OBJ(BandIKEffector, 0x354);

BEGIN_LOADS(BandIKEffector)
    LOAD_REVS(bs)
    ASSERT_REVS(4, 0)
    LOAD_SUPERCLASS(Hmx::Object)
    LOAD_SUPERCLASS(CharWeightable)
    bs >> mEffector;
    bs >> mMore;
    if (gRev > 1)
        bs >> mElbow;
    if (gRev < 1) {
        int i;
        bs >> i;
    }
    bs >> mConstraints;
    if (gRev > 3)
        bs >> mGround;
END_LOADS

BEGIN_COPYS(BandIKEffector)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(CharWeightable)
    CREATE_COPY(BandIKEffector)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mEffector)
        COPY_MEMBER(mMore)
        COPY_MEMBER(mElbow)
        COPY_MEMBER(mConstraints)
        COPY_MEMBER(mGround)
    END_COPYING_MEMBERS
END_COPYS

BEGIN_HANDLERS(BandIKEffector)
    HANDLE_SUPERCLASS(CharWeightable)
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0x388)
END_HANDLERS

BEGIN_CUSTOM_PROPSYNC(BandIKEffector::Constraint)
    SYNC_PROP(target, o.mTarget)
    SYNC_PROP(finger, o.mFinger)
    SYNC_PROP(weight, o.mWeight)
END_CUSTOM_PROPSYNC

void BandIKEffector::DoFancyElbow(QuatXfm &hand, float handWeight) {
    Transform neutralElbow;
    Transform worldShoulder;
    Transform handLocalElbow;
    Hmx::Matrix3 m;
    Transform elbowOut;
    Transform handOut;
    QuatXfm accum;
    RndTransformable *elbow;
    RndTransformable *shoulder;
    float aaPlusbb;
    float inv2ab;
    float aPlusb;
    if (!MeasureLengths(elbow, shoulder, inv2ab, aaPlusbb, aPlusb))
        return;

    NeutralWorldXfm(elbow, neutralElbow);

    Vector3 elbowDest;
    elbowDest.x = 0.0f;
    elbowDest.y = 0.0f;
    elbowDest.z = 0.0f;
    float elbowWeight = mElbow->ApplyPosConstraints(elbowDest, neutralElbow.v, this);
    float totalWeight = elbowWeight + handWeight;
    if (totalWeight == 0.0f)
        return;

    float naturalWeight = 0.0f;
    accum.v.x = 0.0f;
    accum.v.y = 0.0f;
    accum.v.z = 0.0f;
    accum.q.x = 0.0f;
    accum.q.y = 0.0f;
    accum.q.z = 0.0f;
    accum.q.w = 0.0f;
    if (totalWeight < 1.0f) {
        naturalWeight = 1.0f - totalWeight;
        if (accum.q.w < 0.0f) {
            accum.q.w -= naturalWeight;
        } else {
            accum.q.w += naturalWeight;
        }
        totalWeight += naturalWeight;
    }

    worldShoulder = shoulder->WorldXfm();

    if (elbowWeight > 0.0f) {
        elbowDest.x /= elbowWeight;
        elbowDest.y /= elbowWeight;
        elbowDest.z /= elbowWeight;
        QuatXfm shoulderXfm;
        ComputeElbowPullAndQuat(shoulderXfm, worldShoulder, elbowDest);

        float absW = (float)fabs(elbowWeight);
        Hmx::Quat scaled;
        scaled.x = shoulderXfm.q.x * absW;
        scaled.y = shoulderXfm.q.y * absW;
        scaled.z = shoulderXfm.q.z * absW;
        scaled.w = shoulderXfm.q.w * elbowWeight;

        accum.v.x += shoulderXfm.v.x * elbowWeight;
        accum.v.y += shoulderXfm.v.y * elbowWeight;
        accum.v.z += shoulderXfm.v.z * elbowWeight;

        float dot = scaled.x * accum.q.x + scaled.y * accum.q.y
            + scaled.z * accum.q.z + scaled.w * accum.q.w;
        if (dot < 0.0f) {
            accum.q.x -= scaled.x;
            accum.q.y -= scaled.y;
            accum.q.z -= scaled.z;
            accum.q.w -= scaled.w;
        } else {
            accum.q.x += scaled.x;
            accum.q.y += scaled.y;
            accum.q.z += scaled.z;
            accum.q.w += scaled.w;
        }
    }

    if (handWeight > 0.0f) {
        float invW = 1.0f / handWeight;
        float hz = hand.v.z;
        float hy = hand.v.y;
        float hx = hand.v.x;
        Vector3 handTarget;
        handTarget.z = hz * invW;
        handTarget.x = hx * invW;
        handTarget.y = hy * invW;
        QuatXfm handPull;
        ComputeHandPullAndQuat(
            handPull, handLocalElbow, worldShoulder, handTarget, inv2ab, aaPlusbb, aPlusb
        );

        float absW = (float)fabs(handWeight);
        Hmx::Quat scaled;
        scaled.x = handPull.q.x * absW;
        scaled.y = handPull.q.y * absW;
        scaled.z = handPull.q.z * absW;
        scaled.w = handPull.q.w * handWeight;

        accum.v.x += handPull.v.x * handWeight;
        accum.v.y += handPull.v.y * handWeight;
        accum.v.z += handPull.v.z * handWeight;

        float dot = scaled.x * accum.q.x + scaled.y * accum.q.y
            + scaled.z * accum.q.z + scaled.w * accum.q.w;
        if (dot < 0.0f) {
            accum.q.x -= scaled.x;
            accum.q.y -= scaled.y;
            accum.q.z -= scaled.z;
            accum.q.w -= scaled.w;
        } else {
            accum.q.x += scaled.x;
            accum.q.y += scaled.y;
            accum.q.z += scaled.z;
            accum.q.w += scaled.w;
        }
    }

    Normalize(accum.q, accum.q);
    accum.v.x /= totalWeight;
    accum.v.y /= totalWeight;
    accum.v.z /= totalWeight;
    MakeRotMatrix(accum.q, m);
    Multiply(m, worldShoulder.m, worldShoulder.m);
    worldShoulder.v.x += accum.v.x;
    worldShoulder.v.y += accum.v.y;
    worldShoulder.v.z += accum.v.z;
    shoulder->SetWorldXfm(worldShoulder);

    if (handWeight > 0.0f) {
        Hmx::Quat elbowQuat;
        elbowQuat.Set(elbow->LocalXfm().m);
        float elbowScale = naturalWeight + elbowWeight;
        elbowQuat.x *= elbowScale;
        elbowQuat.y *= elbowScale;
        elbowQuat.z *= elbowScale;
        elbowQuat.w *= elbowScale;

        Hmx::Quat handPullQ;
        handPullQ.Set(handLocalElbow.m);

        float absW = (float)fabs(handWeight);
        Hmx::Quat scaledHand;
        scaledHand.x = handPullQ.x * absW;
        scaledHand.y = handPullQ.y * absW;
        scaledHand.z = handPullQ.z * absW;
        scaledHand.w = handPullQ.w * handWeight;

        float dot = scaledHand.x * elbowQuat.x + scaledHand.y * elbowQuat.y
            + scaledHand.z * elbowQuat.z + scaledHand.w * elbowQuat.w;
        if (dot < 0.0f) {
            elbowQuat.x -= scaledHand.x;
            elbowQuat.y -= scaledHand.y;
            elbowQuat.z -= scaledHand.z;
            elbowQuat.w -= scaledHand.w;
        } else {
            elbowQuat.x += scaledHand.x;
            elbowQuat.y += scaledHand.y;
            elbowQuat.z += scaledHand.z;
            elbowQuat.w += scaledHand.w;
        }
        Normalize(elbowQuat, elbowQuat);
        MakeRotMatrix(elbowQuat, m);

        elbowOut.v = elbow->WorldXfm().v;
        Multiply(m, worldShoulder.m, elbowOut.m);
        elbow->SetWorldXfm(elbowOut);

        Transform &handWorld = mEffector->WorldXfm();
        handOut.v = handWorld.v;
        Hmx::Quat finalHandQ;
        finalHandQ.Set(handWorld.m);
        float handScale = naturalWeight + elbowWeight;
        float absH = (float)fabs(handScale);
        Hmx::Quat scaledFinal;
        scaledFinal.x = finalHandQ.x * absH;
        scaledFinal.y = finalHandQ.y * absH;
        scaledFinal.z = finalHandQ.z * absH;
        scaledFinal.w = finalHandQ.w * handScale;
        float fdot = scaledFinal.x * hand.q.x + scaledFinal.y * hand.q.y
            + scaledFinal.z * hand.q.z + scaledFinal.w * hand.q.w;
        if (fdot < 0.0f) {
            hand.q.x -= scaledFinal.x;
            hand.q.y -= scaledFinal.y;
            hand.q.z -= scaledFinal.z;
            hand.q.w -= scaledFinal.w;
        } else {
            hand.q.x += scaledFinal.x;
            hand.q.y += scaledFinal.y;
            hand.q.z += scaledFinal.z;
            hand.q.w += scaledFinal.w;
        }
        Normalize(hand.q, hand.q);
        MakeRotMatrix(hand.q, handOut.m);
        mEffector->SetWorldXfm(handOut);
    }
}

BEGIN_PROPSYNCS(BandIKEffector)
    SYNC_PROP(effector, mEffector)
    SYNC_PROP(ground, mGround)
    SYNC_PROP(more, mMore)
    SYNC_PROP(elbow, mElbow)
    SYNC_PROP(constraints, mConstraints)
    SYNC_SUPERCLASS(CharWeightable)
END_PROPSYNCS