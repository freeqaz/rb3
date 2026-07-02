#include "char/CharIKFingers.h"
#include <cstdlib>
#include "math/Color.h"
#include "math/Mtx.h"
#include "math/Rot.h"
#include "rndobj/Rnd.h"
#include "rndobj/Trans.h"
#include "rndobj/Utl.h"
#include "utl/Symbols.h"

INIT_REVS(CharIKFingers)

CharIKFingers::CharIKFingers()
    : mHand(0), mForeArm(0), mUpperArm(0), mBlendInFrames(0), mBlendOutFrames(0),
      mResetHandDest(1), mResetCurHandTrans(1), mFingerCurledLength(0.85f),
      mHandMoveForward(1.0f), mHandPinkyRotation(-0.06f), mHandThumbRotation(0.23f),
      mHandDestOffset(-0.4f), mIsRightHand(1), mMoveHand(0), mIsSetup(0),
      mOutputTrans(this), mKeyboardRefBone(this) {
    mFingers.resize(5, FingerDesc());
    mCurHandTrans.Zero();
    mDestHandTrans.Zero();
    mHandKeyboardOffset.Set(0.3f, -6.0f, 0.4f);
    mtx = Hmx::Matrix3();
}

CharIKFingers::~CharIKFingers() {}

void CharIKFingers::SetFinger(Vector3 v1, Vector3 v2, CharIKFingers::FingerNum fingerNum) {
    MILO_ASSERT(fingerNum >= 0 && fingerNum < kFingerNone, 0x37);
    FingerDesc &finger = mFingers[fingerNum];
    finger.mDestPos = v1;
    finger.mAltDestPos = v2;
    finger.mIsControlled = true;
    finger.mDestChanged = true;
    Transform tf48;
    Multiply(finger.mBone1->LocalXfm(), mCurHandTrans, tf48);
    if (Distance(tf48.v, v1) > finger.mLength * mFingerCurledLength) {
        mMoveHand = true;
    }
    mBlendInFrames = 5;
    finger.mFBlendInFrames = 5;
    finger.mFBlendOutFrames = 0;
}

void CharIKFingers::ReleaseFinger(FingerNum finger) {
    MILO_ASSERT(finger >= 0 && finger < kFingerNone, 0x57);
    mFingers[finger].mIsControlled = false;
    mFingers[finger].mDestChanged = true;
    mFingers[finger].mFBlendOutFrames = 0;
    mFingers[finger].mFBlendInFrames = 5;
}

#pragma push
#pragma dont_inline on
void CharIKFingers::SetName(const char *name, ObjectDir *dir) {
    Hmx::Object::SetName(name, dir);
    if (dir) {
        for (int i = 0; i < 5; i++) {
            mFingers[i] = FingerDesc();
        }
        if (mIsRightHand) {
            mHand = dir->Find<RndTransformable>("bone_R-hand.mesh", false);
            mForeArm = dir->Find<RndTransformable>("bone_R-foreArm.mesh", false);
            mUpperArm = dir->Find<RndTransformable>("bone_R-upperArm.mesh", false);
            mFingers[kFingerThumb].mBone1 =
                dir->Find<RndTransformable>("bone_R-thumb01.mesh", false);
            mFingers[kFingerThumb].mBone2 =
                dir->Find<RndTransformable>("bone_R-thumb02.mesh", false);
            mFingers[kFingerThumb].mBone3 =
                dir->Find<RndTransformable>("bone_R-thumb03.mesh", false);
            mFingers[kFingerThumb].mBoneTip =
                dir->Find<RndTransformable>("spot_R-thumb_tip.mesh", false);
            mFingers[kFingerIndex].mBone1 =
                dir->Find<RndTransformable>("bone_R-index01.mesh", false);
            mFingers[kFingerIndex].mBone2 =
                dir->Find<RndTransformable>("bone_R-index02.mesh", false);
            mFingers[kFingerIndex].mBone3 =
                dir->Find<RndTransformable>("bone_R-index03.mesh", false);
            mFingers[kFingerIndex].mBoneTip =
                dir->Find<RndTransformable>("spot_R-index_tip.mesh", false);
            mFingers[kFingerMiddle].mBone1 =
                dir->Find<RndTransformable>("bone_R-middlefinger01.mesh", false);
            mFingers[kFingerMiddle].mBone2 =
                dir->Find<RndTransformable>("bone_R-middlefinger02.mesh", false);
            mFingers[kFingerMiddle].mBone3 =
                dir->Find<RndTransformable>("bone_R-middlefinger03.mesh", false);
            mFingers[kFingerMiddle].mBoneTip =
                dir->Find<RndTransformable>("spot_R-middlefinger_tip.mesh", false);
            mFingers[kFingerRing].mBone1 =
                dir->Find<RndTransformable>("bone_R-ringfinger01.mesh", false);
            mFingers[kFingerRing].mBone2 =
                dir->Find<RndTransformable>("bone_R-ringfinger02.mesh", false);
            mFingers[kFingerRing].mBone3 =
                dir->Find<RndTransformable>("bone_R-ringfinger03.mesh", false);
            mFingers[kFingerRing].mBoneTip =
                dir->Find<RndTransformable>("spot_R-ringfinger_tip.mesh", false);
            mFingers[kFingerPinky].mBone1 =
                dir->Find<RndTransformable>("bone_R-pinky01.mesh", false);
            mFingers[kFingerPinky].mBone2 =
                dir->Find<RndTransformable>("bone_R-pinky02.mesh", false);
            mFingers[kFingerPinky].mBone3 =
                dir->Find<RndTransformable>("bone_R-pinky03.mesh", false);
            mFingers[kFingerPinky].mBoneTip =
                dir->Find<RndTransformable>("spot_R-pinky_tip.mesh", false);
            mtx = Hmx::Matrix3(
                -0.023f,
                0.97899997f,
                0.201f,
                -0.228f,
                0.191f,
                -0.95499998f,
                -0.972,
                -0.068f,
                0.21799999f
            );
            Normalize(mtx, mtx);
            mIsSetup = true;
        } else {
            mHand = dir->Find<RndTransformable>("bone_L-hand.mesh", false);
            mForeArm = dir->Find<RndTransformable>("bone_L-foreArm.mesh", false);
            mUpperArm = dir->Find<RndTransformable>("bone_L-upperArm.mesh", false);
            mFingers[kFingerThumb].mBone1 =
                dir->Find<RndTransformable>("bone_L-thumb01.mesh", false);
            mFingers[kFingerThumb].mBone2 =
                dir->Find<RndTransformable>("bone_L-thumb02.mesh", false);
            mFingers[kFingerThumb].mBone3 =
                dir->Find<RndTransformable>("bone_L-thumb03.mesh", false);
            mFingers[kFingerThumb].mBoneTip =
                dir->Find<RndTransformable>("spot_L-thumb_tip.mesh", false);
            mFingers[kFingerIndex].mBone1 =
                dir->Find<RndTransformable>("bone_L-index01.mesh", false);
            mFingers[kFingerIndex].mBone2 =
                dir->Find<RndTransformable>("bone_L-index02.mesh", false);
            mFingers[kFingerIndex].mBone3 =
                dir->Find<RndTransformable>("bone_L-index03.mesh", false);
            mFingers[kFingerIndex].mBoneTip =
                dir->Find<RndTransformable>("spot_L-index_tip.mesh", false);
            mFingers[kFingerMiddle].mBone1 =
                dir->Find<RndTransformable>("bone_L-middlefinger01.mesh", false);
            mFingers[kFingerMiddle].mBone2 =
                dir->Find<RndTransformable>("bone_L-middlefinger02.mesh", false);
            mFingers[kFingerMiddle].mBone3 =
                dir->Find<RndTransformable>("bone_L-middlefinger03.mesh", false);
            mFingers[kFingerMiddle].mBoneTip =
                dir->Find<RndTransformable>("spot_L-middlefinger_tip.mesh", false);
            mFingers[kFingerRing].mBone1 =
                dir->Find<RndTransformable>("bone_L-ringfinger01.mesh", false);
            mFingers[kFingerRing].mBone2 =
                dir->Find<RndTransformable>("bone_L-ringfinger02.mesh", false);
            mFingers[kFingerRing].mBone3 =
                dir->Find<RndTransformable>("bone_L-ringfinger03.mesh", false);
            mFingers[kFingerRing].mBoneTip =
                dir->Find<RndTransformable>("spot_L-ringfinger_tip.mesh", false);
            mFingers[kFingerPinky].mBone1 =
                dir->Find<RndTransformable>("bone_L-pinky01.mesh", false);
            mFingers[kFingerPinky].mBone2 =
                dir->Find<RndTransformable>("bone_L-pinky02.mesh", false);
            mFingers[kFingerPinky].mBone3 =
                dir->Find<RndTransformable>("bone_L-pinky03.mesh", false);
            mFingers[kFingerPinky].mBoneTip =
                dir->Find<RndTransformable>("spot_L-pinky_tip.mesh", false);
            mtx = Hmx::Matrix3(
                -0.067f,
                0.985f,
                0.156f,
                0.224f,
                0.167f,
                -0.95999998f,
                -0.972f,
                -0.028999999f,
                -0.23199999f
            );
            Normalize(mtx, mtx);
            mIsSetup = true;
        }
        for (int i = 0; i < 5; i++) {
            FingerDesc cur = mFingers[i];
            if (!cur.mBone1 || !cur.mBone2 || !cur.mBone3 || !cur.mBoneTip) {
                mIsSetup = false;
                break;
            }
        }
    }
    MeasureLengths();
}
#pragma pop

void CharIKFingers::Poll() {
#ifdef HX_NATIVE
    { static int g=-1; if(g<0)g=getenv("RB3_NO_IK")?1:0; if(g)return; }
#endif
    if (!mHand || !mIsSetup)
        return;
    else {
        Hmx::Matrix3 mtx58;
        Hmx::Matrix3 mtx7c;
        Invert(mKeyboardRefBone->WorldXfm().m, mtx58);
        Multiply(mHand->WorldXfm().m, mtx58, mtx7c);
        Vector3 v88;
        float weight = Weight();
        if ((int)weight < 1.0) {
            if (mOutputTrans) {
                mOutputTrans->SetWorldXfm(mHand->WorldXfm());
            }
        } else {
            if (mResetCurHandTrans) {
                mCurHandTrans.Set(mHand->WorldXfm().m, mHand->WorldXfm().v);
                mDestHandTrans.Set(mHand->WorldXfm().m, mHand->WorldXfm().v);
                mResetCurHandTrans = false;
            }
            int i3 = 0;
            float f8 = 1.0f;
            int i1 = -1;
            for (int i = 0; i < 5; i++) {
                if (mFingers[i].mIsControlled) {
                    if (i1 == -1)
                        i1 = i;
                    i3++;
                }
            }
            CalculateHandDest(i3, i1);
            if (mBlendInFrames > 0) {
                f8 = 1.0f - mBlendInFrames / 5.0f;
            } else if (mBlendOutFrames > 0) {
                f8 = 1.0f - mBlendOutFrames / 5.0f;
            }
            Interp(mCurHandTrans.v, mDestHandTrans.v, f8, mCurHandTrans.v);
            Interp(mCurHandTrans.m, mDestHandTrans.m, f8, mCurHandTrans.m);
            if (mOutputTrans) {
                mOutputTrans->SetWorldXfm(mCurHandTrans);
            }
            for (int i = 0; i < 5; i++) {
                CalculateFingerDest((FingerNum)i);
                MoveFinger((FingerNum)i);
            }
            if (i3 > 0) {
                for (int i = 2; i <= 4; i++) {
                    const FingerDesc &prevFinger = mFingers[i - 1];
                    const FingerDesc &curFinger = mFingers[i];
                    if (!curFinger.mIsControlled) {
                        if (i == 4) {
                            FixSingleFinger(
                                prevFinger.mBone1, curFinger.mBone1, nullptr
                            );
                        } else {
                            FixSingleFinger(
                                prevFinger.mBone1,
                                curFinger.mBone1,
                                mFingers[i + 1].mBone1
                            );
                        }
                    }
                }
            }
            if (mBlendInFrames > 0)
                mBlendInFrames--;
            if (mBlendOutFrames > 0)
                mBlendOutFrames--;
        }
    }
}

void CharIKFingers::CalculateHandDest(int i1, int i2) {
    Transform tf110(mHand->WorldXfm()); // auStack_110
    if (mMoveHand) {
        if (i1 > 0) {
            Vector3 v188(0, 0, 0);
            FingerDesc desc(mFingers[i2]);
            Vector3 v194;
            v194.Zero();
            bool b1 = false;
            Vector3 v1a0(mHandDestOffset, 0, 0);
            if (!mIsRightHand) {
                Scale(v1a0, -1.0f, v1a0);
            }
            Multiply(v1a0, mKeyboardRefBone->WorldXfm().m, v1a0);
            Hmx::Matrix3 m134;
            Multiply(mtx, mKeyboardRefBone->WorldXfm().m, m134);
            Normalize(m134, mDestHandTrans.m);
            for (int i = 0; (unsigned int)i < 5; i++) {
                const FingerDesc &curDesc = mFingers[i];
                if (curDesc.mIsControlled) {
                    ::Add(curDesc.mDestPos, v194, v194);
                    Vector3 v1ac;
                    Scale(v1a0, i - 2.0f, v1ac);
                    ::Add(v1ac, v194, v194);
                    if (i == 0) {
                        Hmx::Matrix3 m158;
                        float f5 = mHandThumbRotation;
                        if (!mIsRightHand)
                            f5 *= -1.0f;
                        m158.RotateAboutY(f5);
                        Multiply(m158, m134, mDestHandTrans.m);
                        b1 = true;
                    } else if (i == 4) {
                        Hmx::Matrix3 m17c;
                        float f5 = mHandPinkyRotation;
                        if (!mIsRightHand)
                            f5 *= -1.0f;
                        m17c.RotateAboutY(f5);
                        Multiply(m17c, m134, mDestHandTrans.m);
                        b1 = true;
                    }
                }
            }
            Scale(v194, 1.0f / (float)i1, v194);
            if (b1)
                v188.y += mHandMoveForward;
            ::Add(mHandKeyboardOffset, v188, v188);
            Multiply(v188, mKeyboardRefBone->WorldXfm().m, v188);
            Vector3 v1b8;
            ::Add(v194, v188, v1b8);
            mDestHandTrans.v.Set(v1b8.x, v1b8.y, v1b8.z);
        }
        mMoveHand = false;
    }
}

void CharIKFingers::MoveFinger(FingerNum num) {
    FingerDesc &finger = mFingers[num];
    if (finger.mIsControlled || finger.mFBlendInFrames > 0 || finger.mFBlendOutFrames > 0) {
        Transform tf60;
        Transform tf90(mDestHandTrans);
        if (finger.mBone1->TransParent() != mHand) {
            Multiply(finger.mBone1->TransParent()->mLocalXfm, mDestHandTrans, tf90);
        }
        Multiply(finger.mBone1->mLocalXfm, tf90, tf60);

        float f5 = 1.0f;
        if (finger.mFBlendInFrames > 0 || finger.mFBlendOutFrames > 0) {
            if (finger.mFBlendInFrames > 0) {
                f5 = 1.0f - finger.mFBlendInFrames / 5.0f;
            } else if (finger.mFBlendOutFrames > 0) {
                f5 = 1.0f - finger.mFBlendOutFrames / 5.0f;
            }
        }

        Interp(finger.mBone2CurAngle, finger.mBone2DestAngle, f5, finger.mBone2CurAngle);
        Interp(finger.mBone3CurAngle, finger.mBone3DestAngle, f5, finger.mBone3CurAngle);
        Hmx::Matrix3 mb8;
        Vector3 v128(0, 0, finger.mBone2CurAngle);
        MakeRotMatrix(v128, mb8, true);
        finger.mBone2->SetLocalRot(mb8);
        v128.Set(0, 0, finger.mBone3CurAngle);
        MakeRotMatrix(v128, mb8, true);
        finger.mBone3->SetLocalRot(mb8);
        Interp(finger.mCurForwardVector, finger.mDestForwardVector, f5, finger.mCurForwardVector);
        Hmx::Quat q138;
        MakeRotQuat(tf60.m.x, finger.mCurForwardVector, q138);
        Transform tfe8;
        Multiply(tf60.m.x, q138, tfe8.m.x);
        Multiply(tf60.m.y, q138, tfe8.m.y);
        Multiply(tf60.m.z, q138, tfe8.m.z);
        Normalize(tfe8.m, tfe8.m);
        tfe8.v = tf60.v;
        Transform tf118;
        Invert(tf90, tf118);
        Multiply(tfe8, tf118, finger.mBone1->DirtyLocalXfm());
        if (finger.mFBlendOutFrames > 0)
            finger.mFBlendOutFrames--;
        if (finger.mFBlendInFrames > 0)
            finger.mFBlendInFrames--;
    }
}

void CharIKFingers::CalculateFingerDest(FingerNum num) {
    ObjPtr<RndTransformable> &_ref0 = mOutputTrans;
    if (_ref0) {
        FingerDesc &finger = mFingers[num];
        if (finger.mResetFingerRots) {
            Transform tf78;
            Multiply(finger.mBone1->mLocalXfm, _ref0->WorldXfm(), tf78);
            finger.mCurForwardVector = tf78.m.x;
            Vector3 v1cc;
            MakeEuler(finger.mBone2->mLocalXfm.m, v1cc);
            Vector3 v1d8;
            MakeEuler(finger.mBone3->mLocalXfm.m, v1d8);
            finger.mBone2CurAngle = v1cc.z;
            finger.mBone3CurAngle = v1d8.z;
            finger.mResetFingerRots = false;
        }
        if (finger.mDestChanged) {
            if (finger.mIsControlled) {
                Transform tfa8;
                Transform tfd8;
                Transform tf108;
                Transform tf138;
                if (finger.mBone1->TransParent() != mHand) {
                    Transform tf168;
                    Multiply(
                        finger.mBone1->TransParent()->mLocalXfm, mDestHandTrans, tf168
                    );
                    Multiply(finger.mBone1->mLocalXfm, tf168, tfa8);
                } else {
                    Multiply(finger.mBone1->mLocalXfm, mDestHandTrans, tfa8);
                }

                Multiply(finger.mBone2->mLocalXfm, tfa8, tfd8);
                Multiply(finger.mBone3->mLocalXfm, tfd8, tf108);
                Multiply(finger.mBoneTip->mLocalXfm, tf108, tf138);
                Vector3 v1e4 = (Distance(tf138.v, finger.mDestPos) < Distance(tf138.v, finger.mAltDestPos))
                        ? finger.mDestPos
                        : finger.mAltDestPos;

                Vector3 v220;
                Subtract(v1e4, tfa8.v, v220);
                Vector3 v1fc = tfa8.m.x;
                Vector3 v208 = tfa8.m.z;

                float len5 = Length(finger.mBone2->mLocalXfm.v);
                float len6 = Length(finger.mBoneTip->mLocalXfm.v);
                float len8 = Length(v220);
                float len7 = Length(finger.mBone3->mLocalXfm.v);
                float f9 = std::acos(
                    -((len8 - len7) * (len8 - len7) - (len5 * len5 + len6 * len6))
                    / (len5 * 2.0f * len6)
                );
                if (f9 < 0.87f)
                    f9 = 0.87f;
                float f5 = f9 * 0.5f + 1.5707964f;
                if (IsNaN(f5)) {
                    f5 = 2.96f;
                }
                finger.mBone2DestAngle = PI - f5;
                finger.mBone3DestAngle = PI - f5;
                Hmx::Quat q230;
                q230.Set(v208, (2.0f * PI - 2.0f * f5) * 0.5f);
                Multiply(v1fc, q230, v1fc);
                Hmx::Quat q240;
                MakeRotQuat(v1fc, v220, q240);
                Multiply(tfa8.m.x, q240, finger.mDestForwardVector);
                finger.mDestChanged = false;
            } else {
                Transform tf1c0;
                Multiply(finger.mBone1->mLocalXfm, _ref0->WorldXfm(), tf1c0);
                finger.mDestForwardVector = tf1c0.m.x;
                Vector3 v24c;
                MakeEuler(finger.mBone2->mLocalXfm.m, v24c);
                Vector3 v258;
                MakeEuler(finger.mBone3->mLocalXfm.m, v258);
                finger.mBone2DestAngle = v24c.z;
                finger.mBone3DestAngle = v258.z;
                finger.mDestChanged = false;
            }
        }
    }
}

void CharIKFingers::FixSingleFinger(
    RndTransformable *t1, RndTransformable *t2, RndTransformable *t3
) {
    Hmx::Matrix3 m38;
    Vector3 va8;
    if (t3) {
        ::Add(t1->WorldXfm().m.x, t3->WorldXfm().m.x, va8);
        Scale(va8, 0.5f, va8);
    } else {
        va8 = t1->WorldXfm().m.x;
    }

    Hmx::Quat qb8;
    MakeRotQuat(t2->WorldXfm().m.x, va8, qb8);

    Transform tf68;
    Multiply(t2->WorldXfm().m.x, qb8, tf68.m.x);
    Multiply(t2->WorldXfm().m.y, qb8, tf68.m.y);
    Multiply(t2->WorldXfm().m.z, qb8, tf68.m.z);
    tf68.v = t2->WorldXfm().v;

    Transform tf98;
    Invert(t2->TransParent()->WorldXfm(), tf98);
    Multiply(tf68, tf98, t2->DirtyLocalXfm());
}

void CharIKFingers::MeasureLengths() {
    for (int i = 0; i < 5; i++) {
        RndTransformable *t1 = mFingers[i].mBone2;
        RndTransformable *t2 = mFingers[i].mBone3;
        RndTransformable *t3 = mFingers[i].mBoneTip;
        if (t1 && t2 && t3) {
            float &len = mFingers[i].mLength;
            len = Length(t3->mLocalXfm.v) + Length(t2->mLocalXfm.v)
                + Length(t1->mLocalXfm.v);
        }
    }

    if (mHand && mHand->TransParent() && mHand->TransParent()->TransParent()) {
        mInv2ab = 2.0f;
        mAAPlusBB = 0;
        float handLen = Length(mHand->mLocalXfm.v);
        mAAPlusBB += handLen * handLen;
        float handParentLen = Length(mHand->TransParent()->mLocalXfm.v);
        mAAPlusBB += handParentLen * handParentLen;
        mInv2ab *= handLen;
        mInv2ab = 1.0f / (mInv2ab * handParentLen);
    }
}

void CharIKFingers::PollDeps(
    std::list<Hmx::Object *> &changedBy, std::list<Hmx::Object *> &change
) {
    change.push_back(mHand);
    changedBy.push_back(mHand);
    for (int i = 0; i < 5; i++) {
        FingerDesc desc(mFingers[i]);
        if (desc.mBone1) {
            changedBy.push_back(desc.mBone1);
        }
        if (desc.mBone2) {
            changedBy.push_back(desc.mBone2);
        }
        if (desc.mBone3) {
            changedBy.push_back(desc.mBone3);
        }
        if (desc.mBoneTip) {
            changedBy.push_back(desc.mBoneTip);
        }
    }
    if (mForeArm) {
        change.push_back(mForeArm);
        changedBy.push_back(mForeArm);
    }
    if (mUpperArm) {
        change.push_back(mUpperArm);
        changedBy.push_back(mUpperArm);
    }
}

void CharIKFingers::Highlight() {
#ifdef MILO_DEBUG
    for (int i = 0; i < 5; i++) {
        FingerDesc desc(mFingers[i]);
        if (desc.mIsControlled) {
            UtilDrawSphere(desc.mDestPos, 0.2f, Hmx::Color(1, 0, 0));
            UtilDrawSphere(desc.mAltDestPos, 0.2f, Hmx::Color(0, 1, 0));
            UtilDrawAxes(desc.mBone1->WorldXfm(), 1.0f, Hmx::Color(1, 1, 1));
            TheRnd->DrawLine(
                desc.mBone1->WorldXfm().v,
                desc.mBone2->WorldXfm().v,
                Hmx::Color(1, 1, 1),
                false
            );
            TheRnd->DrawLine(
                desc.mBone2->WorldXfm().v,
                desc.mBone3->WorldXfm().v,
                Hmx::Color(1, 1, 1),
                false
            );
            TheRnd->DrawLine(
                desc.mBone3->WorldXfm().v,
                desc.mBoneTip->WorldXfm().v,
                Hmx::Color(1, 1, 1),
                false
            );
        }
    }
#endif
}

SAVE_OBJ(CharIKFingers, 0x36A)

BEGIN_LOADS(CharIKFingers)
    LOAD_REVS(bs)
    ASSERT_REVS(5, 0)
    LOAD_SUPERCLASS(Hmx::Object)
    LOAD_SUPERCLASS(CharWeightable)
    if (gRev > 1)
        bs >> mIsRightHand;
    if (gRev > 2)
        bs >> mOutputTrans;
    if (gRev > 3)
        bs >> mKeyboardRefBone;
    if (gRev > 4) {
        bs >> mHandKeyboardOffset;
        bs >> mHandThumbRotation;
        bs >> mHandPinkyRotation;
        bs >> mHandMoveForward;
        bs >> mHandDestOffset;
    }
END_LOADS

BEGIN_COPYS(CharIKFingers)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(CharWeightable)
    CREATE_COPY(CharIKFingers)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mIsRightHand)
        COPY_MEMBER(mOutputTrans)
        COPY_MEMBER(mKeyboardRefBone)
        COPY_MEMBER(mHandKeyboardOffset)
        COPY_MEMBER(mHandThumbRotation)
        COPY_MEMBER(mHandPinkyRotation)
        COPY_MEMBER(mHandMoveForward)
        COPY_MEMBER(mHandDestOffset)
    END_COPYING_MEMBERS
END_COPYS

BEGIN_HANDLERS(CharIKFingers)
    HANDLE_SUPERCLASS(CharWeightable)
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0x3AB)
END_HANDLERS

BEGIN_PROPSYNCS(CharIKFingers)
    SYNC_PROP(is_right_hand, mIsRightHand)
    SYNC_PROP(output_trans, mOutputTrans)
    SYNC_PROP(keyboard_ref_bone, mKeyboardRefBone)
    SYNC_PROP(hand_keyboard_offset, mHandKeyboardOffset)
    SYNC_PROP(hand_thumb_rotation, mHandThumbRotation)
    SYNC_PROP(hand_pinky_rotation, mHandPinkyRotation)
    SYNC_PROP(hand_move_forward, mHandMoveForward)
    SYNC_PROP(hand_dest_offset, mHandDestOffset)
    SYNC_SUPERCLASS(CharWeightable)
END_PROPSYNCS