#include "char/CharGuitarString.h"
#include "rndobj/Trans.h"
#include "utl/Symbols.h"
#include "obj/PropSync_p.h"

INIT_REVS(CharGuitarString)

CharGuitarString::CharGuitarString()
    : mOpen(0), mNut(this, 0), mBridge(this, 0), mBend(this, 0), mTarget(this, 0) {}

CharGuitarString::~CharGuitarString() {}

// fn_80507700 - poll
// checks out in retail: https://decomp.me/scratch/Bxu4k
void CharGuitarString::Poll() {
    if (!mNut || !mBridge || !mBend || !mTarget)
        return;
    Transform tf50(mBend->WorldXfm());
    const Vector3 &nutvec = mNut->WorldXfm().v;
    const Vector3 &bridgevec = mBridge->WorldXfm().v;
    const Vector3 &targetvec = mTarget->WorldXfm().v;
    float ny = nutvec.y;
    float by = bridgevec.y;
    float ty = targetvec.y;
    float dy = by - ny;
    float nx = nutvec.x;
    float ey = ty - ny;
    float bx = bridgevec.x;
    float tx = targetvec.x;
    float dx = bx - nx;
    float dy_sq = dy * dy;
    float nz = nutvec.z;
    float ey_dy = ey * dy;
    float bz = bridgevec.z;
    float ex = tx - nx;
    float tz = targetvec.z;
    float dz = bz - nz;
    float ez = tz - nz;
    float clamped = Clamp(0.0f, 1.0f, (ez*dz + (ex*dx + ey_dy)) / (dz*dz + (dx*dx + dy_sq)));
    if (mOpen)
        clamped = 0.0f;
    if (clamped == 0.0f) {
        tf50.v = nutvec;
    } else if (clamped == 1.0f) {
        tf50.v = bridgevec;
    } else {
        tf50.v.Set(
            clamped * (bx - nx) + nx,
            clamped * (by - ny) + ny,
            clamped * (bz - nz) + nz
        );
    }
    mBend->SetWorldXfm(tf50);
}

void CharGuitarString::PollDeps(
    std::list<Hmx::Object *> &changedBy, std::list<Hmx::Object *> &change
) {
    changedBy.push_back(mNut);
    changedBy.push_back(mBridge);
    changedBy.push_back(mTarget);
    change.push_back(mBend);
}

SAVE_OBJ(CharGuitarString, 0x47)

void CharGuitarString::Load(BinStream &bs) {
    LOAD_REVS(bs);
    ASSERT_REVS(0, 0);
    Hmx::Object::Load(bs);
    bs >> mNut;
    bs >> mBridge;
    bs >> mBend;
    bs >> mTarget;
}

BEGIN_COPYS(CharGuitarString)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY(CharGuitarString)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mTarget)
        COPY_MEMBER(mNut)
        COPY_MEMBER(mBridge)
        COPY_MEMBER(mBend)
    END_COPYING_MEMBERS
END_COPYS

BEGIN_HANDLERS(CharGuitarString)
    HANDLE_ACTION(set_open, mOpen = _msg->Int(2) != 0)
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0x70)
END_HANDLERS

BEGIN_PROPSYNCS(CharGuitarString)
    SYNC_PROP(nut, mNut)
    SYNC_PROP(bridge, mBridge)
    SYNC_PROP(bend, mBend)
    SYNC_PROP(target, mTarget)
END_PROPSYNCS
