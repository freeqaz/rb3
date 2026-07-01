#include "char/CharUpperTwist.h"
#include <cstdlib>
#include "math/Rot.h"
#include "obj/ObjMacros.h"
#include "obj/Object.h"
#include "utl/Symbols.h"

INIT_REVS(CharUpperTwist)

CharUpperTwist::CharUpperTwist() : mUpperArm(this), mTwist1(this), mTwist2(this) {}

CharUpperTwist::~CharUpperTwist() {}

// fn_804FAB0C - poll
void CharUpperTwist::Poll() {
#ifdef HX_NATIVE
    { static int g=-1; if(g<0)g=getenv("RB3_NO_IK")?1:0; if(g)return; }
#endif
    if (!mTwist2 || !mTwist1 || !mUpperArm)
        return;
    Transform &twist2parentworld = mTwist2->TransParent()->WorldXfm();
    Hmx::Quat q;
    Transform &twist2world = mTwist2->WorldXfm();
    MakeRotQuat(twist2parentworld.m.x, twist2world.m.x, q);
    Vector3 v68;
    Multiply(twist2parentworld.m.y, q, v68);
    Transform tf48;
    tf48.m.x = twist2world.m.x;
    tf48.v = mUpperArm->WorldXfm().v;
    Interp(v68, twist2world.m.y, 0.333f, tf48.m.y);
    {
        Hmx::Matrix3 &m = tf48.m;
        float xy = m.x.x * m.y.y;
        float yx = m.y.x * m.x.y;
        float yz = m.x.y * m.y.z;
        float zy = m.y.y * m.x.z;
        float zx = m.x.z * m.y.x;
        float xz = m.x.x * m.y.z;
        m.z.Set(yz - zy, zx - xz, xy - yx);
        Normalize(m.z, m.z);
        float xy2 = m.z.x * m.x.y;
        float yx2 = m.z.y * m.x.x;
        float yz2 = m.z.y * m.x.z;
        float zy2 = m.z.z * m.x.y;
        float zx2 = m.z.z * m.x.x;
        float xz2 = m.z.x * m.x.z;
        m.y.Set(yz2 - zy2, zx2 - xz2, xy2 - yx2);
    }
    mUpperArm->SetWorldXfm(tf48);
    tf48.v = mTwist1->WorldXfm().v;
    Interp(v68, twist2world.m.y, 0.666f, tf48.m.y);
    {
        Hmx::Matrix3 &m = tf48.m;
        float xy = m.x.x * m.y.y;
        float yx = m.x.y * m.y.x;
        float yz = m.x.y * m.y.z;
        float zy = m.x.z * m.y.y;
        float zx = m.x.z * m.y.x;
        float xz = m.x.x * m.y.z;
        m.z.Set(yz - zy, zx - xz, xy - yx);
        Normalize(m.z, m.z);
        Cross(m.z, m.x, m.y);
    }
    mTwist1->SetWorldXfm(tf48);
}

void CharUpperTwist::PollDeps(
    std::list<Hmx::Object *> &changedBy, std::list<Hmx::Object *> &change
) {
    changedBy.push_back(mTwist2);
    change.push_back(mUpperArm);
    change.push_back(mTwist1);
}

SAVE_OBJ(CharUpperTwist, 0x5D)

BEGIN_LOADS(CharUpperTwist)
    LOAD_REVS(bs);
    ASSERT_REVS(1, 0);
    LOAD_SUPERCLASS(Hmx::Object)
    bs >> mTwist2;
    bs >> mUpperArm;
    bs >> mTwist1;
END_LOADS

BEGIN_COPYS(CharUpperTwist)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY(CharUpperTwist)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mTwist2)
        COPY_MEMBER(mUpperArm)
        COPY_MEMBER(mTwist1)
    END_COPYING_MEMBERS
END_COPYS

BEGIN_HANDLERS(CharUpperTwist)
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0x7E)
END_HANDLERS

// don't ask me why it's this way, it's what matches
BEGIN_PROPSYNCS(CharUpperTwist)
    SYNC_PROP(upper_arm, mTwist2)
    SYNC_PROP(twist1, mUpperArm)
    SYNC_PROP(twist2, mTwist1)
END_PROPSYNCS