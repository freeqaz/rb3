#ifndef HX_NATIVE
// This file supplies its own paired-single Matrix3 Multiply (below) whose asm
// (implicit cr0 compare, no register locals) matches the target's codegen for
// this TU; CHARHAIR_LOCAL_MULTIPLY suppresses the generic Mtx.h inline (which
// uses cr1 + register locals) so callers here resolve to the local one. On
// native (clang, no PPC intrinsics) leave the suppression off so the standard
// out-of-line Multiply declared in Mtx.h (defined in Rot.cpp) is used instead.
#define CHARHAIR_LOCAL_MULTIPLY
#endif
#include "char/CharHair.h"
#include <cstdlib>
#ifdef HX_NATIVE
#include <cstdio>
#endif
#include "char/CharCollide.h"
#include "decomp.h"
#include "math/Mtx.h"
#include "obj/ObjMacros.h"
#include "obj/Object.h"
#include "os/Timer.h"
#include "rndobj/Trans.h"
#include "rndobj/Wind.h"
#include "rndobj/PostProc.h"
#include "char/Character.h"
#include "math/Utl.h"
#include "math/Rot.h"
#ifdef HX_NATIVE
// STLport functor-base internals; the host libstdc++ provides these via
// <functional> (CharHair only uses std::list::sort with a comparator).
#include <functional>
#else
#include "stl/_function_base.h"
#endif
#include "world/Dir.h"
#include <cmath>
#include "utl/Symbols.h"

INIT_REVS(CharHair)
CharHair *gHair;
CharHair::Strand *gStrand;

#pragma push
#pragma dont_inline on
// fn_804D49F8
void CharHair::Strand::SetRoot(RndTransformable *trans) {
    mRoot = trans;
    if (!mRoot)
        mPoints.resize(0);
    else {
        float len = mPoints.size() != 0 ? mPoints.back().length : 0;
        mBaseMat = mRoot->LocalXfm().m;
        SetAngle(mAngle);

        int depth = 0;
        for (RndTransformable *it = mRoot;; it = it->TransChildren().front()) {
            depth++;
            if (it->TransChildren().empty())
                break;
        }

        mPoints.resize(depth);
        depth = 0;
        for (RndTransformable *it = mRoot;; it = it->TransChildren().front(), depth++) {
            mPoints[depth].bone = it;
            if (it->TransChildren().empty())
                break;
        }

        Point *pt = 0;
        for (int i = 1; i < mPoints.size(); i++) {
            pt = &mPoints[i - 1];
            RndTransformable *bone = mPoints[i].bone;
            pt->length = bone->LocalXfm().v.y;
            pt->pos = bone->WorldXfm().v;
        }
        Point *backpt = &mPoints.back();
        len = len ? len : (pt ? pt->length : 5.0f);
        backpt->length = len;
        ScaleAdd(
            backpt->bone->WorldXfm().v,
            backpt->bone->WorldXfm().m.y,
            backpt->length,
            backpt->pos
        );
    }
}
#pragma pop

void CharHair::SetCloth(bool b) {
    for (int i = 0; i < mStrands.size(); i++) {
        int next = i + 1;
        Strand &strand = mStrands[i];
        Strand &modidx = mStrands[Mod(next, mStrands.size())];
        for (int j = 0; j < strand.mPoints.size(); j++) {
            Point &point = strand.mPoints[j];
            bool b1 = false;
            if (b && j < modidx.mPoints.size())
                b1 = true;
            point.sideLength = b1 ? Distance(point.pos, modidx.mPoints[j].pos) : -1.0f;
        }
    }
}

#ifdef CHARHAIR_LOCAL_MULTIPLY
inline void Multiply(const Hmx::Matrix3 &a, const Hmx::Matrix3 &b, Hmx::Matrix3 &out) {
    typedef __vec2x32float__ psq;
    register const Hmx::Matrix3 *_a = &a;
    register const Hmx::Matrix3 *_b = &b;
    register Hmx::Matrix3 *_out = &out;
    register float *_row2, *_row1, *_row0;
    float row2[3], row1[3], row0[3];
    register psq _f0, _f1, _f2, _f3, _f4, _f5, _f6, _f7, _f8, _f9, _f10, _f11, _f12;
    asm { cmplw _b, _out }
    asm volatile {
        beq alias_path

        psq_l _f4, 0x4(_a), 0, 0
        psq_l _f3, 0x18(_b), 0, 0
        psq_l _f2, 0x20(_b), 1, 0
        ps_muls1 _f1, _f3, _f4
        psq_l _f3, 0xc(_b), 0, 0
        ps_muls1 _f0, _f2, _f4
        psq_l _f2, 0x14(_b), 1, 0
        psq_l _f5, 0x0(_a), 0, 0
        ps_madds0 _f1, _f3, _f4, _f1
        ps_madds0 _f0, _f2, _f4, _f0
        psq_l _f3, 0x0(_b), 0, 0
        psq_l _f2, 0x8(_b), 1, 0
        ps_madds0 _f1, _f3, _f5, _f1
        psq_l _f4, 0x10(_a), 0, 0
        ps_madds0 _f0, _f2, _f5, _f0
        psq_st _f1, 0x0(_out), 0, 0
        psq_l _f5, 0xc(_a), 0, 0
        psq_st _f0, 0x8(_out), 1, 0

        psq_l _f6, 0x1c(_a), 0, 0
        psq_l _f3, 0x18(_b), 0, 0
        psq_l _f2, 0x20(_b), 1, 0
        ps_muls1 _f1, _f3, _f4
        psq_l _f3, 0xc(_b), 0, 0
        ps_muls1 _f0, _f2, _f4
        psq_l _f2, 0x14(_b), 1, 0
        psq_l _f7, 0x18(_a), 0, 0
        ps_madds0 _f1, _f3, _f4, _f1
        ps_madds0 _f0, _f2, _f4, _f0
        psq_l _f3, 0x0(_b), 0, 0
        psq_l _f2, 0x8(_b), 1, 0
        ps_madds0 _f1, _f3, _f5, _f1
        ps_madds0 _f0, _f2, _f5, _f0
        psq_st _f1, 0xc(_out), 0, 0
        psq_st _f0, 0x14(_out), 1, 0

        psq_l _f3, 0x18(_b), 0, 0
        psq_l _f2, 0x20(_b), 1, 0
        ps_muls1 _f1, _f3, _f6
        psq_l _f3, 0xc(_b), 0, 0
        ps_muls1 _f0, _f2, _f6
        psq_l _f2, 0x14(_b), 1, 0
        ps_madds0 _f1, _f3, _f6, _f1
        psq_l _f3, 0x0(_b), 0, 0
        ps_madds0 _f0, _f2, _f6, _f0
        psq_l _f2, 0x8(_b), 1, 0
        ps_madds0 _f1, _f3, _f7, _f1
        ps_madds0 _f0, _f2, _f7, _f0
        psq_st _f1, 0x18(_out), 0, 0
        psq_st _f0, 0x20(_out), 1, 0
        b mult_end

    alias_path:
        psq_l _f4, 0x4(_a), 0, 0
        la _row2, row2
        psq_l _f3, 0x18(_b), 0, 0
        la _row1, row1
        psq_l _f2, 0x20(_b), 1, 0
        la _row0, row0
        ps_muls1 _f1, _f3, _f4
        psq_l _f3, 0xc(_b), 0, 0
        ps_muls1 _f0, _f2, _f4
        psq_l _f2, 0x14(_b), 1, 0
        psq_l _f9, 0x10(_a), 0, 0
        psq_l _f8, 0x18(_b), 0, 0
        psq_l _f7, 0x20(_b), 1, 0
        ps_madds0 _f1, _f3, _f4, _f1
        ps_muls1 _f6, _f8, _f9
        psq_l _f12, 0x1c(_a), 0, 0
        ps_mr _f8, _f3
        psq_l _f3, 0x18(_b), 0, 0
        ps_muls1 _f5, _f7, _f9
        ps_muls1 _f11, _f3, _f12
        ps_mr _f7, _f2
        psq_l _f3, 0x0(_b), 0, 0
        ps_madds0 _f0, _f2, _f4, _f0
        psq_l _f2, 0x20(_b), 1, 0
        psq_l _f4, 0x0(_a), 0, 0
        ps_muls1 _f10, _f2, _f12
        psq_l _f2, 0x8(_b), 1, 0
        ps_madds0 _f1, _f3, _f4, _f1
        ps_madds0 _f6, _f8, _f9, _f6
        ps_madds0 _f0, _f2, _f4, _f0
        psq_l _f4, 0x18(_a), 0, 0
        ps_madds0 _f5, _f7, _f9, _f5
        psq_l _f9, 0xc(_a), 0, 0
        ps_madds0 _f11, _f8, _f12, _f11
        ps_madds0 _f10, _f7, _f12, _f10
        psq_st _f1, 0x0(_row2), 0, 0
        ps_madds0 _f6, _f3, _f9, _f6
        ps_madds0 _f5, _f2, _f9, _f5
        ps_madds0 _f11, _f3, _f4, _f11
        lfs _f8, row2[0]
        ps_madds0 _f10, _f2, _f4, _f10
        psq_st _f6, 0x0(_row1), 0, 0
        lfs _f7, row2[1]
        psq_st _f11, 0x0(_row0), 0, 0
        lfs _f4, row1[1]
        psq_st _f5, 0x8(_row1), 1, 0
        lfs _f5, row1[0]
        psq_st _f0, 0x8(_row2), 1, 0
        lfs _f3, row1[2]
        psq_st _f10, 0x8(_row0), 1, 0
        lfs _f6, row2[2]
        lfs _f2, row0[0]
        lfs _f1, row0[1]
        lfs _f0, row0[2]
        stfs _f8, 0x0(_out)
        stfs _f7, 0x4(_out)
        stfs _f6, 0x8(_out)
        stfs _f5, 0xc(_out)
        stfs _f4, 0x10(_out)
        stfs _f3, 0x14(_out)
        stfs _f2, 0x18(_out)
        stfs _f1, 0x1c(_out)
        stfs _f0, 0x20(_out)
    mult_end:
    }
}
#endif

#ifdef HX_NATIVE
// StrandMultiply is a hand-tuned MWCC paired-singles matrix multiply computing
// out = a * b (the alias_path handles &b == &out via row scratch). On clang LP64
// there are no Gekko paired-single ops; use the standard out-of-line matrix
// multiply (math/Mtx.h Multiply(Matrix3,Matrix3,Matrix3), which already handles
// the aliasing case). Same substitution CharForeTwist uses for its local
// Multiply (see CharForeTwist.cpp CHARHAIR_LOCAL_MULTIPLY gate).
inline void StrandMultiply(const Hmx::Matrix3 &a, const Hmx::Matrix3 &b, Hmx::Matrix3 &out) {
    Multiply(a, b, out);
}
#else
inline void StrandMultiply(const Hmx::Matrix3 &a, const Hmx::Matrix3 &b, Hmx::Matrix3 &out) {
    register const Hmx::Matrix3 *_a = &a;
    register const Hmx::Matrix3 *_b = &b;
    register Hmx::Matrix3 *_out = &out;
    // NOLINT - _row0/_row1/_row2 used in asm la instructions
    register float *_row0, *_row1, *_row2; // NOLINT(clang-diagnostic-unused-variable)
    float row0[3], row1[3], row2[3];
    asm { cmplw _b, _out }
    asm volatile {
        beq alias_path

        psq_l f0, 0x4(_a), 0, 0
        psq_l f9, 0x18(_b), 0, 0
        psq_l f10, 0x20(_b), 1, 0
        ps_muls1 f11, f9, f0
        psq_l f9, 0xc(_b), 0, 0
        ps_muls1 f12, f10, f0
        psq_l f10, 0x14(_b), 1, 0
        psq_l f3, 0x10(_a), 0, 0
        psq_l f5, 0x18(_b), 0, 0
        psq_l f6, 0x20(_b), 1, 0
        ps_madds0 f11, f9, f0, f11
        ps_muls1 f7, f5, f3
        psq_l f1, 0x1c(_a), 0, 0
        ps_mr f5, f9
        psq_l f2, 0x20(_b), 1, 0
        ps_muls1 f8, f6, f3
        ps_mr f6, f10
        ps_madds0 f12, f10, f0, f12
        psq_l f0, 0x18(_b), 0, 0
        ps_muls1 f2, f2, f1
        psq_l f9, 0x0(_b), 0, 0
        ps_madds0 f7, f5, f3, f7
        ps_madds0 f8, f6, f3, f8
        ps_muls1 f3, f0, f1
        psq_l f0, 0x0(_a), 0, 0
        psq_l f10, 0x8(_b), 1, 0
        ps_madds0 f2, f6, f1, f2
        psq_l f4, 0xc(_a), 0, 0
        ps_madds0 f11, f9, f0, f11
        ps_madds0 f12, f10, f0, f12
        psq_l f0, 0x18(_a), 0, 0
        ps_madds0 f3, f5, f1, f3
        psq_st f11, 0x0(_out), 0, 0
        ps_madds0 f7, f9, f4, f7
        ps_madds0 f8, f10, f4, f8
        ps_madds0 f3, f9, f0, f3
        psq_st f12, 0x8(_out), 1, 0
        ps_madds0 f2, f10, f0, f2
        psq_st f7, 0xc(_out), 0, 0
        psq_st f8, 0x14(_out), 1, 0
        psq_st f3, 0x18(_out), 0, 0
        psq_st f2, 0x20(_out), 1, 0
        b mult_end

    alias_path:
        psq_l f0, 0x4(_a), 0, 0
        la r8, row0
        psq_l f8, 0x18(_b), 0, 0
        la r7, row1
        psq_l f10, 0x20(_b), 1, 0
        la r6, row2
        ps_muls1 f11, f8, f0
        psq_l f8, 0xc(_b), 0, 0
        ps_muls1 f12, f10, f0
        psq_l f10, 0x14(_b), 1, 0
        psq_l f3, 0x10(_a), 0, 0
        psq_l f5, 0x18(_b), 0, 0
        psq_l f6, 0x20(_b), 1, 0
        ps_madds0 f11, f8, f0, f11
        ps_muls1 f7, f5, f3
        psq_l f1, 0x1c(_a), 0, 0
        ps_mr f5, f8
        psq_l f2, 0x20(_b), 1, 0
        ps_muls1 f9, f6, f3
        ps_mr f6, f10
        ps_madds0 f12, f10, f0, f12
        psq_l f0, 0x18(_b), 0, 0
        ps_muls1 f2, f2, f1
        psq_l f8, 0x0(_b), 0, 0
        ps_madds0 f7, f5, f3, f7
        ps_madds0 f9, f6, f3, f9
        ps_muls1 f3, f0, f1
        psq_l f0, 0x0(_a), 0, 0
        psq_l f10, 0x8(_b), 1, 0
        ps_madds0 f2, f6, f1, f2
        psq_l f4, 0xc(_a), 0, 0
        ps_madds0 f11, f8, f0, f11
        ps_madds0 f12, f10, f0, f12
        psq_l f0, 0x18(_a), 0, 0
        ps_madds0 f3, f5, f1, f3
        psq_st f11, 0x0(r8), 0, 0
        ps_madds0 f7, f8, f4, f7
        ps_madds0 f9, f10, f4, f9
        psq_st f7, 0x0(r7), 0, 0
        ps_madds0 f3, f8, f0, f3
        ps_madds0 f2, f10, f0, f2
        lfs f8, row0[0]
        psq_st f12, 0x8(r8), 1, 0
        lfs f7, row0[1]
        psq_st f3, 0x0(r6), 0, 0
        lfs f6, row0[2]
        psq_st f2, 0x8(r6), 1, 0
        lfs f5, row1[0]
        psq_st f9, 0x8(r7), 1, 0
        lfs f4, row1[1]
        lfs f3, row1[2]
        lfs f2, row2[0]
        lfs f1, row2[1]
        lfs f0, row2[2]
        stfs f8, 0x0(_out)
        stfs f7, 0x4(_out)
        stfs f6, 0x8(_out)
        stfs f5, 0xc(_out)
        stfs f4, 0x10(_out)
        stfs f3, 0x14(_out)
        stfs f2, 0x18(_out)
        stfs f1, 0x1c(_out)
        stfs f0, 0x20(_out)
    mult_end:
    }
}
#endif

void CharHair::Strand::SetAngle(float angle) {
    register float angle_rad = angle * DEG2RAD;
    mAngle = angle;
    register float cos_val = Sine(1.5707964f + angle_rad);
    Hmx::Matrix3 m38;
    register float sin_val = Sine(angle_rad);
    m38.Set(1.0f, 0.0f, 0.0f, 0.0f, cos_val, sin_val, 0.0f, -sin_val, cos_val);
    StrandMultiply(m38, mBaseMat, mRootMat);
}

CharHair::CharHair()
    : mStiffness(0.04f), mTorsion(0.1f), mInertia(0.7f), mGravity(1.0f), mWeight(0.5f),
      mFriction(0.3f), mMinSlack(0.0f), mMaxSlack(0.0f), mStrands(this), mReset(1),
      mSimulate(1), mUsePostProc(1), mMe(this), mWind(this), mCollide(this),
      mManagedHookup(0) {}

CharHair::~CharHair() {}

void CharHair::Enter() {
    mReset = 1;
    RndPollable::Enter();
    Hookup();
}

// matches in retail
void CharHair::FreezePoseRaw() {
    for (int i = 0; i < mStrands.size(); i++) {
        Strand &strand = mStrands[i];
        if (strand.Root() && strand.Root()->TransParent()) {
            ObjVector<Point> &pts = strand.mPoints;
            Transform tf48(strand.Root()->TransParent()->WorldXfm());
            Invert(tf48, tf48);
            for (int j = 0; j < pts.size(); j++) {
                pts[j];
                Multiply(pts[j].pos, tf48, pts[j].unk5c);
            }
        }
    }
}

void CharHair::FreezePose() {
    bool tmpsim = mSimulate;
    Hookup();
    SimulateLoops(200, 60.0f);
    mSimulate = tmpsim;
    FreezePoseRaw();
}

// https://decomp.me/scratch/zTOLT (retail scratch)
void CharHair::DoReset(int reset) {
    for (int i = 0; i < mStrands.size(); i++) {
        Strand &strand = mStrands[i];
        if (strand.Root() && strand.Root()->TransParent()) {
            ObjVector<Point> &pts = strand.mPoints;
            Transform tf70(strand.Root()->TransParent()->WorldXfm());
            Vector3 v80(strand.Root()->WorldXfm().v);
            Vector3 v8c(strand.Root()->WorldXfm().m.x);
            for (int j = 0; j < pts.size(); j++) {
                Point &pt = pts[j];
                Multiply(pt.unk5c, tf70, pt.pos);
                Vector3 v98;
                Subtract(pt.pos, v80, v98);
                v80 = pt.pos;
                Cross(v8c, v98, pt.lastZ);
                Normalize(pt.lastZ, pt.lastZ);
                Cross(v98, pt.lastZ, v8c);
                pt.force.Zero();
                pt.lastFriction.Zero();
            }
        }
    }
    bool tmpsim = mSimulate;
    float tmpinert = mInertia;
    float tmpfric = mFriction;
    mSimulate = true;
    mInertia = 0;
    mFriction = 0;
    SimulateLoops(reset, GetFPS());
    mSimulate = tmpsim;
    mFriction = tmpfric;
    mInertia = tmpinert;
    mReset = 0;
}

void CharHair::SetName(const char *cc, ObjectDir *dir) {
    Hmx::Object::SetName(cc, dir);
    mMe = dynamic_cast<Character *>(dir);
    mUsePostProc = mMe || dynamic_cast<WorldDir *>(dir);
}

void CharHair::Poll() {
#ifdef HX_NATIVE
    { static int g=-1; if(g<0)g=getenv("RB3_NO_FACE")?1:0; if(g)return; }
#endif
    if (mMe) {
        if (mMe->GetPollState() == Character::kCharSyncObject)
            Hookup();
        if (mMe->Teleported())
            mReset = 1;
        if (mMe->MinLod() > 0) {
            DoReset(0);
            return;
        }
    }
    if (mReset > 0)
        DoReset(mReset);
    if (TheTaskMgr.DeltaSeconds() != 0.0f) {
        SimulateLoops(1, GetFPS());
    } else
        SimulateZeroTime();
}

float CharHair::GetFPS() {
    if (mUsePostProc && RndPostProc::Current()
        && RndPostProc::Current()->EmulateFPS() > 0) {
        float ret = RndPostProc::Current()->EmulateFPS();
        if (ret == 60.0f)
            return ret;
        return 60.0f - ret;
    } else
        return 60.0f;
}

void CharHair::SimulateLoops(int count, float f) {
    if (!mSimulate || mStrands.size() == 0)
        return;
    START_AUTO_TIMER("char_hair");
    for (ObjPtrList<CharCollide>::iterator it = mCollide.begin(); it != mCollide.end();
         ++it) {
        (*it)->SyncWorldState();
    }
    for (int n = 0; n < count; n++) {
        SimulateInternal(f);
    }
}

DECOMP_FORCEACTIVE(CharHair, "ObjPtr_p.h", "f.Owner()", "")

#pragma push
#pragma dont_inline on
// fn_804D6590
void CharHair::SimulateInternal(float f) {
    float sixtyover = 60.0f / f;
    float f19 = (1.0f / f) * sixtyover;
    float gravTerm = -3.85826778f * (mGravity * f19);
    float powed = std::pow(1.0f - mStiffness, sixtyover * sixtyover);
    float stiffFriction = 1.0f - powed;
    float halfWeight = 0.5f * -mWeight;
    Vector3 vec134(0, 0, 0);
    if (mWind) {
        if (mStrands[0].Root()) {
            mWind->GetWind(
                mStrands[0].Root()->WorldXfm().v,
                TheTaskMgr.Seconds(TaskMgr::kRealTime),
                vec134
            );
            vec134 *= f19 * 0.5f;
        }
    }
    vec134.z = vec134.z + gravTerm;

    for (int i = 0; i < mStrands.size(); i++) {
        Strand &modStrand = mStrands[Mod(i + 1, mStrands.size())];
        Strand &thisStrand = mStrands[i];
        if (thisStrand.Root() && thisStrand.Root()->TransParent()) {
            Transform t100;
            t100.v = thisStrand.Root()->WorldXfm().v;
            Multiply(
                thisStrand.RootMat(),
                thisStrand.Root()->TransParent()->WorldXfm().m,
                t100.m
            );
            ObjVector<Point> &points = thisStrand.Points();
            for (int j = 0; j < points.size(); j++) {
                Point &thisPoint = points[j];
                Vector3 v140(thisPoint.pos);
                thisPoint.pos += thisPoint.force;
                thisPoint.pos.x += vec134.x;
                thisPoint.pos.y += vec134.y;
                thisPoint.pos.z += vec134.z;
                if (thisPoint.sideLength >= 0.0f) {
                    Vector3 vRes;
                    Point &modPoint = modStrand.Points()[j];
                    Subtract(thisPoint.pos, modPoint.pos, vRes);
                    float lensq = LengthSquared(vRes);
                    float sidelensq = thisPoint.sideLength - mMinSlack;
                    sidelensq *= sidelensq;
                    if (lensq < sidelensq) {
                        vRes *= (sidelensq / (sidelensq + lensq) - 0.5f);
                        thisPoint.pos += vRes;
                        modPoint.force -= vRes;
                    } else {
                        float maxslacklensq = thisPoint.sideLength + mMaxSlack;
                        maxslacklensq *= maxslacklensq;
                        if (lensq > maxslacklensq) {
                            vRes *= (maxslacklensq / (maxslacklensq + lensq) - 0.5f);
                            thisPoint.pos += vRes;
                            modPoint.force -= vRes;
                        }
                    }
                }
                Hmx::Matrix3 m128;
                Subtract(thisPoint.pos, t100.v, m128.y);
                float rsa = RecipSqrtAccurate(LengthSquared(m128.y));
                float rsalen = thisPoint.length * rsa - 1.0f;
                if (j > 0) {
                    ScaleAddEq(points[j - 1].force, m128.y, halfWeight * rsalen);
                }
                ScaleAddEq(thisPoint.pos, m128.y, rsalen);
                Vector3 v158;
                ScaleAdd(t100.v, t100.m.y, thisPoint.length, v158);
                Interp(thisPoint.lastZ, t100.m.z, mTorsion, m128.z);
                if (thisPoint.collides.size() != 0) {
                    float diffRad = thisPoint.outerRadius - thisPoint.radius;
                    float maxRad = Max(thisPoint.radius, thisPoint.outerRadius);
                    for (ObjPtrList<CharCollide>::iterator it =
                             thisPoint.collides.begin();
                         it != thisPoint.collides.end();
                         ++it) {
                        CharCollide *thisCollide = *it;
                        Vector3 v164;
                        float collideRad = thisCollide->GetRadius(thisPoint.pos, v164);
                        switch (thisCollide->GetShape()) {
                        case CharCollide::kPlane: // 0
                            if (maxRad > collideRad) {
                                ScaleAddEq(
                                    thisPoint.pos,
                                    thisCollide->Axis(),
                                    maxRad - collideRad
                                );
                            }
                            break;
                        case CharCollide::kCigar: // 3
                        case CharCollide::kSphere: { // 1
                            float v164sq = LengthSquared(v164);
                            float sumRad = collideRad + maxRad;
                            if (v164sq < sumRad * sumRad) {
                                if (diffRad > 0.0f) {
                                    float v164sqrecip = RecipSqrtAccurate(v164sq);
                                    float cluster = v164sq * v164sqrecip;
                                    float othersumRad = collideRad + thisPoint.radius;
                                    v164 *= -v164sqrecip;
                                    if (cluster < othersumRad) {
                                        m128.z = v164;
                                        ScaleAddEq(
                                            thisPoint.pos, v164, cluster - othersumRad
                                        );
                                    } else
                                        Interp(
                                            m128.z,
                                            v164,
                                            (sumRad - cluster) / diffRad,
                                            m128.z
                                        );
                                } else
                                    ScaleAddEq(
                                        thisPoint.pos,
                                        v164,
                                        sumRad * RecipSqrtAccurate(v164sq) - 1.0f
                                    );
                            }
                            break;
                        }
                        case CharCollide::kInsideCigar: // 4
                        case CharCollide::kInsideSphere: { // 2
                            float v164sq42 = LengthSquared(v164);
                            float minusRad = collideRad - maxRad;
                            if (v164sq42 > minusRad * minusRad) {
                                if (diffRad > 0.0f) {
                                    float v164sqrecip = RecipSqrtAccurate(v164sq42);
                                    float cluster = v164sq42 * v164sqrecip;
                                    float othersumRad = collideRad - thisPoint.radius;
                                    v164 *= -v164sqrecip;
                                    if (cluster > othersumRad) {
                                        m128.z = v164;
                                        ScaleAddEq(
                                            thisPoint.pos, v164, cluster - othersumRad
                                        );
                                    } else
                                        Interp(
                                            m128.z,
                                            v164,
                                            (cluster - minusRad) / diffRad,
                                            m128.z
                                        );
                                } else
                                    ScaleAddEq(
                                        thisPoint.pos,
                                        v164,
                                        minusRad * RecipSqrtAccurate(v164sq42) - 1.0f
                                    );
                            }
                            break;
                        }
                        default:
                            break;
                        }
                    }
                }

                Scale(m128.y, rsa, t100.m.y);
                Cross(t100.m.y, m128.z, t100.m.x);
                t100.m.x *= RecipSqrtAccurate(LengthSquared(t100.m.x));
                Normalize(t100.m.x, t100.m.x);
                Cross(t100.m.x, t100.m.y, t100.m.z);
                thisPoint.lastZ = t100.m.z;
                if (thisPoint.bone)
                    thisPoint.bone->SetWorldXfm(t100);
                Subtract(v158, thisPoint.pos, thisPoint.force);
                Vector3 v170;
                Subtract(thisPoint.lastFriction, thisPoint.force, v170);
                thisPoint.lastFriction = thisPoint.force;
                thisPoint.force *= stiffFriction;
                ScaleAddEq(thisPoint.force, v170, -mFriction);
                Vector3 v17c;
                Subtract(thisPoint.pos, v140, v17c);
                ScaleAddEq(thisPoint.force, v17c, mInertia);
                t100.v = thisPoint.pos;
            }
        }
    }
}
#pragma pop

float RecipSqrtAccurate(float x) {
    float est = 1.0f / (float)sqrt(x);
    return 0.5f * est * (3.0f - est * (x * est));
}

const Vector3 &CharCollide::Axis() const { return unk194; }

void CharHair::SimulateZeroTime() {
    if (mSimulate) {
        for (int i = 0; i < mStrands.size(); i++) {
            Strand &curStrand = mStrands[i];
            RndTransformable *root = curStrand.Root();
            if (root && curStrand.Root()->TransParent()) {
                Transform tf50;
                tf50.v = curStrand.Root()->WorldXfm().v;
                Multiply(
                    curStrand.mRootMat,
                    curStrand.Root()->TransParent()->WorldXfm().m,
                    tf50.m
                );
                ObjVector<Point> &points = curStrand.mPoints;
                for (int j = 0; j < points.size(); j++) {
                    Point &curPoint = points[j];
                    Hmx::Matrix3 m78;
                    Subtract(curPoint.pos, tf50.v, m78.y);
                    m78.z = curPoint.lastZ;
                    Normalize(m78, tf50.m);
                    if (curPoint.bone) {
                        curPoint.bone->SetWorldXfm(tf50);
                    }
                    tf50.v = curPoint.pos;
                }
            }
        }
    }
}

void CharHair::PollDeps(
    std::list<Hmx::Object *> &changedBy, std::list<Hmx::Object *> &change
) {
    for (int i = 0; i < mStrands.size(); i++) {
        changedBy.push_back(mStrands[i].Root());
        change.push_back(mStrands[i].Root());
    }
}

CharHair::Strand::Strand(Hmx::Object *o)
    : mShowSpheres(0), mShowCollide(0), mShowPose(0), mRoot(o, 0), mAngle(0.0f),
      mPoints(o), mHookupFlags(0) {
    mBaseMat.Identity();
    mRootMat.Identity();
}

void CharHair::Hookup() {
    if (mManagedHookup)
        return;
    ObjPtrList<CharCollide> colList(this);
    for (ObjDirItr<CharCollide> it(Dir(), true); it; ++it) {
        colList.push_back(it);
    }
    colList.sort(ByRadius());
    Hookup(colList);
}

void CharHair::Hookup(ObjPtrList<CharCollide> &collides) {
    mCollide.clear();
    for (int i = 0; i < mStrands.size(); i++) {
        Strand &curStrand = mStrands[i];
        if (!curStrand.Root())
            continue;

        ObjVector<Point> &pts = curStrand.mPoints;
        for (int j = 0; j < pts.size(); j++) {
            pts[j].collides.clear();
        }

        for (ObjPtrList<CharCollide>::iterator it = collides.begin();
             it != collides.end();
             ++it) {
            CharCollide *col = *it;
            if (!(curStrand.mHookupFlags & col->mFlags))
                continue;

            col->SyncWorldState();

            Vector3 colPos(col->WorldXfm().v);

            int shape = (int)col->GetShape();
            float colAdjust = col->mCurRadius[0];
            if (shape >= 3) {
                Vector3 p1;
                ScaleAdd(
                    col->WorldXfm().v,
                    col->WorldXfm().m.x,
                    col->mCurLength[0] - col->mCurRadius[0],
                    p1
                );
                Vector3 p2;
                ScaleAdd(
                    col->WorldXfm().v,
                    col->WorldXfm().m.x,
                    col->mCurLength[1] + col->mCurRadius[1],
                    p2
                );
                Interp(p1, p2, 0.5f, colPos);
                colAdjust = Distance(p1, p2) * 0.5f;
            } else if (shape == 0) {
                colAdjust = kHugeFloat;
            }

            const Transform &rootXfm = curStrand.Root()->WorldXfm();
            float dist = Distance(colPos, rootXfm.v) - colAdjust;

            for (int j = 0; j < pts.size(); j++) {
                Point &pt = pts[j];
                dist -= pt.length;
                float maxRad = Max(pt.radius, pt.outerRadius);
                if (maxRad > dist) {
                    pt.collides.push_back(col);
                    if (mCollide.find(col) == mCollide.end()) {
                        mCollide.push_back(col);
                    }
                }
            }
        }
    }
#ifdef HX_NATIVE
    // H2 collide-hookup coverage probe (env-gated, silent by default). Reports
    // per-strand how many strand points hooked at least one CharCollide, the
    // strand's hookup flags, and how many CharCollides were reachable at hookup
    // time (both the passed-in `collides` list and an independent Dir() scan).
    // Free-hanging long-hair strands that hook zero collides pass through the
    // skull/shoulders under motion (H2 gap); this quantifies that.
    {
        static int dbg = -1;
        if (dbg < 0)
            dbg = getenv("RB3_HAIR_DBG") ? 1 : 0;
        if (dbg) {
            int dirCollides = 0;
            for (ObjDirItr<CharCollide> it(Dir(), true); it; ++it)
                dirCollides++;
            int totalPts = 0, totalHooked = 0, strandsWithNone = 0;
            for (int i = 0; i < mStrands.size(); i++) {
                Strand &s = mStrands[i];
                if (!s.Root())
                    continue;
                ObjVector<Point> &pts = s.mPoints;
                int hooked = 0;
                for (int j = 0; j < pts.size(); j++)
                    if (pts[j].collides.size() != 0)
                        hooked++;
                totalPts += pts.size();
                totalHooked += hooked;
                if (hooked == 0)
                    strandsWithNone++;
                fprintf(stderr,
                    "[HAIR_DBG] hair='%s' strand=%d pts-with-collides=%d/%d "
                    "hookupFlags=0x%x\n",
                    Name(), i, hooked, pts.size(), s.mHookupFlags);
            }
            fprintf(stderr,
                "[HAIR_DBG] SUMMARY hair='%s' strands=%d strands-hooking-none=%d "
                "pts-hooked=%d/%d dir-collides=%d passed-collides=%d "
                "mCollide=%d\n",
                Name(), mStrands.size(), strandsWithNone, totalHooked, totalPts,
                dirCollides, collides.size(), mCollide.size());
        }
    }
#endif
}

BinStream &operator>>(BinStream &bs, CharHair::Point &pt) {
    bs >> pt.pos;
    bs >> pt.bone;
    bs >> pt.length;
    if (CharHair::gRev < 3) {
        int i;
        char buf[0x100];
        bs >> i;
        bs.ReadString(buf, 0xff);
    } else if (CharHair::gRev == 3) {
        int i;
        bs >> i;
    }
    bs >> pt.radius;
    if (CharHair::gRev > 1)
        bs >> pt.outerRadius;
    else
        pt.outerRadius = 0;
    if (CharHair::gRev >= 6 && CharHair::gRev <= 8) {
        float f;
        bs >> f;
        pt.radius += f;
        pt.outerRadius += f;
    }
    if (CharHair::gRev == 6) {
        char buf[0x100];
        bs.ReadString(buf, 0xff);
    }
    if (CharHair::gRev < 8) {
        pt.sideLength = -1.0f;
        if (CharHair::gRev > 5) {
            int i;
            bs >> i >> i;
        }
    } else {
        bool b = false;
        if (CharHair::gRev < 9)
            bs >> b;
        bs >> pt.sideLength;
        if (CharHair::gRev < 9 && !b) {
            pt.sideLength = -1.0f;
        }
    }
    if (CharHair::gRev > 9) {
        bs >> pt.unk5c;
    }
    pt.collides.clear();
    pt.force.Zero();
    pt.lastFriction.Zero();
    pt.lastZ.Zero();
#ifdef HX_NATIVE
    // Matched-fork omits the trailing `return bs;` (the declared BinStream&
    // return is never used by callers, and MWCC let the value linger); clang
    // LP64 emits ud2 (SIGILL) on the non-void fall-through. Same class as the
    // CharBones::SetStart / ObjDirPtr::operator= missing-return fixes.
    return bs;
#endif
}

void CharHair::Strand::Load(BinStream &bs) {
    bs >> mRoot;
    bs >> mAngle;
    bs >> mPoints;
    bs >> mBaseMat >> mRootMat;
    if (CharHair::gRev > 2) {
        bs >> mHookupFlags;
    } else
        mHookupFlags = 0;
}

BinStream &operator>>(BinStream &bs, CharHair::Strand &strand) {
    strand.Load(bs);
#ifdef HX_NATIVE
    return bs; // matched-fork missing-return (clang ud2 on fall-through)
#endif
}

SAVE_OBJ(CharHair, 0x41B)

void CharHair::Load(BinStream &bs) {
    LOAD_REVS(bs);
    ASSERT_REVS(11, 0);
    Hmx::Object::Load(bs);
    bs >> mStiffness >> mTorsion >> mInertia >> mGravity >> mWeight >> mFriction;
    if (gRev < 8) {
        mMinSlack = 0.0f;
        mMaxSlack = 0.0f;
    } else
        bs >> mMinSlack >> mMaxSlack;
    bs >> mStrands;
    bs >> mSimulate;
    if (gRev > 10)
        bs >> mWind;
}

BEGIN_COPYS(CharHair)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY(CharHair)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mStiffness)
        COPY_MEMBER(mInertia)
        COPY_MEMBER(mGravity)
        COPY_MEMBER(mWeight)
        COPY_MEMBER(mFriction)
        COPY_MEMBER(mTorsion)
        COPY_MEMBER(mStrands)
        COPY_MEMBER(mSimulate)
        COPY_MEMBER(mMinSlack)
        COPY_MEMBER(mMaxSlack)
        COPY_MEMBER(mWind)
    END_COPYING_MEMBERS
END_COPYS

BEGIN_HANDLERS(CharHair)
    HANDLE_ACTION(reset, mReset = _msg->Int(2))
    HANDLE_ACTION(hookup, Hookup())
    HANDLE_ACTION(set_cloth, SetCloth(_msg->Int(2)))
    HANDLE_ACTION(freeze_pose, FreezePose())
    HANDLE_SUPERCLASS(RndPollable)
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0x46F)
END_HANDLERS

BEGIN_CUSTOM_PROPSYNC(CharHair::Point)
    SYNC_PROP(bone, o.bone)
    SYNC_PROP(length, o.length)
    SYNC_PROP(collides, o.collides)
    SYNC_PROP(radius, o.radius)
    SYNC_PROP(outer_radius, o.outerRadius)
    SYNC_PROP(side_length, o.sideLength)
END_CUSTOM_PROPSYNC

BEGIN_CUSTOM_PROPSYNC(CharHair::Strand)
    gStrand = &o;
    SYNC_PROP_SET(root, o.mRoot, o.SetRoot(_val.Obj<RndTransformable>()))
    SYNC_PROP_SET(angle, o.mAngle, o.SetAngle(_val.Float()))
    SYNC_PROP(points, o.mPoints)
    SYNC_PROP(hookup_flags, o.mHookupFlags)
    SYNC_PROP(show_spheres, o.mShowSpheres)
    SYNC_PROP(show_collide, o.mShowCollide)
    SYNC_PROP(show_pose, o.mShowPose)
END_CUSTOM_PROPSYNC

BEGIN_PROPSYNCS(CharHair)
    gHair = this;
    SYNC_PROP(stiffness, mStiffness)
    SYNC_PROP(torsion, mTorsion)
    SYNC_PROP(inertia, mInertia)
    SYNC_PROP(gravity, mGravity)
    SYNC_PROP(weight, mWeight)
    SYNC_PROP(friction, mFriction)
    SYNC_PROP(min_slack, mMinSlack)
    SYNC_PROP(max_slack, mMaxSlack)
    SYNC_PROP(strands, mStrands)
    SYNC_PROP(simulate, mSimulate)
    SYNC_PROP(wind, mWind)
END_PROPSYNCS
