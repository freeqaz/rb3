#include "char/CharBoneTwist.h"
#include "utl/Symbols.h"

INIT_REVS(CharBoneTwist);

CharBoneTwist::CharBoneTwist() : mBone(this), mTargets(this) {}

// fn_804B42D0 - poll
void CharBoneTwist::Poll() {
    if (!mBone || mTargets.size() == 0)
        return;
    Vector3 v58;
    v58.Zero();
    for (ObjPtrList<RndTransformable, ObjectDir>::iterator it = mTargets.begin();
         it != mTargets.end();
         ++it) {
        Vector3 v64((*it)->WorldXfm().v);
        Add(v64, v58, v58);
    }
    Scale(v58, 1.0f / mTargets.size(), v58);
    Transform tf48(mBone->WorldXfm());
    Vector3 v70;
    Subtract(v58, mBone->WorldXfm().v, v70);
    Vector3 v7c;
    Scale(tf48.m.x, Dot(tf48.m.x, v70), v7c);
    Subtract(v70, v7c, v7c);
    Normalize(v7c, v7c);

    Interp(tf48.m.y, v7c, Weight(), tf48.m.y);
    Normalize(tf48.m.y, tf48.m.y);
    // Inlined Cross(tf48.m.x, tf48.m.y, tf48.m.z) — must compute all six
    // products before any subtractions to prevent the compiler from emitting
    // fused fmsubs in place of the separate fmuls/fsubs the target uses.
    {
        float y1 = tf48.m.x.y;
        float z2 = tf48.m.y.z;
        float x2 = tf48.m.y.x;
        float z1 = tf48.m.x.z;
        float p_yz = y1 * z2;
        float p_yx = y1 * x2;
        float y2 = tf48.m.y.y;
        float p_zx = z1 * x2;
        float x1 = tf48.m.x.x;
        float p_zy = z1 * y2;
        float p_xy = x1 * y2;
        float p_xz = x1 * z2;
        tf48.m.z.x = p_yz - p_zy;
        tf48.m.z.y = p_zx - p_xz;
        tf48.m.z.z = p_xy - p_yx;
    }
    Normalize(tf48.m.z, tf48.m.z);
    Scale(tf48.m.z, Length(tf48.m.x), tf48.m.z);
    mBone->SetWorldXfm(tf48);
}

void CharBoneTwist::PollDeps(
    std::list<Hmx::Object *> &changedBy, std::list<Hmx::Object *> &change
) {
    change.push_back(mBone);
    for (ObjPtrList<RndTransformable, class ObjectDir>::iterator it = mTargets.begin();
         it != mTargets.end();
         ++it) {
        changedBy.push_back(*it);
    }
}

SAVE_OBJ(CharBoneTwist, 0x59)

void CharBoneTwist::Load(BinStream &bs) {
    LOAD_REVS(bs);
    ASSERT_REVS(0, 0);
    Hmx::Object::Load(bs);
    CharWeightable::Load(bs);
    bs >> mBone;
    bs >> mTargets;
}

BEGIN_COPYS(CharBoneTwist)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(CharWeightable)
    CREATE_COPY(CharBoneTwist)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mBone)
        COPY_MEMBER(mTargets)
    END_COPYING_MEMBERS
END_COPYS

BEGIN_HANDLERS(CharBoneTwist)
    HANDLE_SUPERCLASS(CharWeightable)
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0x7A)
END_HANDLERS

BEGIN_PROPSYNCS(CharBoneTwist)
    SYNC_PROP(bone, mBone)
    SYNC_PROP(targets, mTargets)
    SYNC_SUPERCLASS(CharWeightable)
END_PROPSYNCS