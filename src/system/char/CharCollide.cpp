#include "char/CharCollide.h"
#include "rndobj/Utl.h"
#include "utl/Symbols.h"

INIT_REVS(CharCollide)

CharCollide::CharCollide() : mShape(kSphere), mFlags(0), mMesh(this, 0), mMeshYBias(0) {
    for (int i = 0; i < 2; i++) {
        mOrigLength[i] = 0;
        mOrigRadius[i] = 0;
    }
    CopyOriginalToCur();
    for (int i = 0; i < 8; i++) {
        unk_structs[i].unk0 = 0;
        unk_structs[i].vec.Zero();
    }
    unk148.Reset();
}

CharCollide::~CharCollide() {}

void CharCollide::Highlight() {
    Hmx::Color black(1.0f, 1.0f, 1.0f);
    Hmx::Color red(1.0f, 0.0f, 0.0f);
    switch (mShape) {
    case kPlane: {
        Plane p(WorldXfm().v, WorldXfm().m.x);
        UtilDrawPlane(p, WorldXfm().v, red, 1, 12.0f);
        break;
    }
    case kSphere:
    case kInsideSphere: {
        UtilDrawSphere(WorldXfm().v, mOrigRadius[0], red);
        UtilDrawSphere(WorldXfm().v, mCurRadius[0], black);
        break;
    }
    case kCigar:
    case kInsideCigar: {
        UtilDrawCigar(WorldXfm(), mOrigRadius, mOrigLength, red, 8);
        UtilDrawCigar(WorldXfm(), mCurRadius, mCurLength, black, 8);
        break;
    }
    default:
        break;
    }
    if (mMesh) {
        int n = NumSpheres() * 4;
        for (int i = 0; i < n; i++) {
            UtilDrawSphere(
                mMesh->Verts(unk_structs[i].unk0).pos, 0.1f, Hmx::Color(0.0f, 0.0f, 1.0f)
            );
        }
    }
}

SAVE_OBJ(CharCollide, 0x58)

BEGIN_LOADS(CharCollide)
    LOAD_REVS(bs)
    ASSERT_REVS(7, 0)
    LOAD_SUPERCLASS(Hmx::Object)
    LOAD_SUPERCLASS(RndTransformable)
    bs >> (int &)mShape;
    bs >> mOrigRadius[0];
    if (gRev > 4)
        bs >> mOrigLength[0];
    if (gRev > 2)
        bs >> mOrigLength[1];
    if (gRev > 1)
        bs >> mFlags;
    else
        mFlags = 0;
    if (gRev > 3)
        bs >> mCurRadius[0];
    else
        mCurRadius[0] = mOrigRadius[0];

    if (gRev > 5) {
        bs >> mOrigRadius[1];
        bs >> mCurRadius[1];
        bs >> mCurLength[0];
        bs >> mCurLength[1];
        bs >> unk148;
        bs >> mMesh;
        for (int i = 0; i < 8; i++) {
            bs >> unk_structs[i].unk0;
            bs >> unk_structs[i].vec;
        }
        bs >> mDigest;
        bs >> mMeshYBias;
        if (gRev < 7)
            CopyOriginalToCur();
    } else {
        mOrigRadius[1] = mOrigRadius[0];
        CopyOriginalToCur();
    }
END_LOADS

BEGIN_COPYS(CharCollide)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(RndTransformable)
    CREATE_COPY(CharCollide)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mShape)
        COPY_MEMBER(mFlags)
        memcpy(mOrigRadius, c->mOrigRadius, 8);
        memcpy(mOrigLength, c->mOrigLength, 8);
        memcpy(mCurRadius, c->mCurRadius, 8);
        memcpy(mCurLength, c->mCurLength, 8);
        COPY_MEMBER(unk148)
        COPY_MEMBER(mMeshYBias)
        COPY_MEMBER(mMesh)
    END_COPYING_MEMBERS
END_COPYS

void CharCollide::Deform() {
    int numSpheres = NumSpheres();
    if (mMesh) {
        for (int i = 0; i < 8; i++) {
            if (unk_structs[i].unk0 >= mMesh->Verts().size()) {
                MILO_NOTIFY_ONCE(
                    "%s: can't do vertex based deformation vert %d is greater than the mesh %s vert count %d, please recompute the deformation by re-setting the mesh property",
                    PathName(this),
                    unk_structs[i].unk0,
                    PathName(mMesh),
                    mMesh->Verts().size()
                );
                return;
            }
        }

        Sphere spheres[2];
        for (int i = 0; i < numSpheres; i++) {
            spheres[i].center.Zero();
            CharCollideStruct *s = &unk_structs[i * 4];
            CharCollideStruct *s2 = s;
            for (int j = 0; j < 4; j++) {
                int idx = s2->unk0;
                Vector3 &pos = mMesh->Verts(idx).pos;
                Vector3 vertPos;
                vertPos.x = pos.x + s2->vec.x;
                vertPos.y = pos.y + s2->vec.y;
                vertPos.z = pos.z + s2->vec.z;
                s2++;
                spheres[i].center.x += vertPos.x;
                spheres[i].center.y += vertPos.y;
                spheres[i].center.z += vertPos.z;
            }
            spheres[i].center.x *= 0.25f;
            spheres[i].center.y *= 0.25f;
            spheres[i].center.z *= 0.25f;
            spheres[i].radius = 0.0f;
            for (int j = 0; j < 4; j++) {
                float vecLen = Length(s->vec);
                if (0.0f == vecLen) {
                    vecLen = 0.001f;
                }
                float scale = (vecLen - mOrigRadius[i]) / vecLen;
                int idx = s->unk0;
                Vector3 &pos = mMesh->Verts(idx).pos;
                Vector3 deformed;
                deformed.y = s->vec.y * scale + pos.y;
                deformed.z = s->vec.z * scale + pos.z;
                deformed.x = s->vec.x * scale + pos.x;
                spheres[i].radius += Distance(deformed, spheres[i].center);
                s++;
            }
            spheres[i].radius *= 0.25f;
        }

        Transform xfm;
        xfm.v = spheres[0].center;
        mCurLength[0] = 0.0f;
        for (int i = 0; i < numSpheres; i++) {
            mCurRadius[i] = spheres[i].radius;
        }

        if (numSpheres == 2) {
            xfm.m.x.y = spheres[1].center.y - xfm.v.y;
            xfm.m.x.x = spheres[1].center.x - xfm.v.x;
            xfm.m.x.z = spheres[1].center.z - xfm.v.z;
            mCurLength[1] = Length(xfm.m.x);
            if (0.0f == (float)mCurLength[1]) {
                mCurLength[1] = 0.001f;
            }
            float len = mCurLength[1];
            xfm.m.x.x /= len;
            xfm.m.x.y /= len;
            xfm.m.x.z /= len;
        } else {
            Vector3 xAxis(1.0f, 0.0f, 0.0f);
            xfm.m.x = xAxis;
        }

        const Vector3 *upPtr;
        if (std::fabs(xfm.m.x.x) < std::fabs(xfm.m.x.y)) {
            Vector3 upX(1.0f, 0.0f, 0.0f);
            upPtr = &upX;
        } else {
            Vector3 upY(0.0f, 1.0f, 0.0f);
            upPtr = &upY;
        }

        Cross(*upPtr, xfm.m.x, xfm.m.y);
        Normalize(xfm.m.y, xfm.m.y);
        Cross(xfm.m.x, xfm.m.y, xfm.m.z);

        SetDirty();
        Multiply(xfm, unk148, mLocalXfm);
    }
}

int CharCollide::NumSpheres() {
    if (mShape == kCigar || mShape == kInsideCigar)
        return 2;
    else if (mShape == kSphere || mShape == kInsideSphere)
        return 1;
    else
        return 0;
}

void CharCollide::CopyOriginalToCur() {
    memcpy(mCurRadius, mOrigRadius, 8);
    memcpy(mCurLength, mOrigLength, 8);
}

void CharCollide::SyncShape() {
    float t = mCurLength[1];
    if (mCurLength[0] > t) {
        mCurLength[0] = mCurLength[1];
    }
    CopyOriginalToCur();
}

BEGIN_HANDLERS(CharCollide)
    HANDLE_SUPERCLASS(RndTransformable)
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0x221)
END_HANDLERS

BEGIN_PROPSYNCS(CharCollide)
    SYNC_PROP_MODIFY(shape, (int &)mShape, SyncShape())
    SYNC_PROP(flags, mFlags)
    SYNC_PROP_MODIFY(radius0, mOrigRadius[0], SyncShape())
    SYNC_PROP_MODIFY(radius1, mOrigRadius[1], SyncShape())
    SYNC_PROP_MODIFY(length0, mOrigLength[0], SyncShape())
    SYNC_PROP_MODIFY(length1, mOrigLength[1], SyncShape())
    SYNC_PROP_MODIFY_ALT(mesh, mMesh, SyncShape())
    SYNC_PROP_MODIFY(mesh_y_bias, mMeshYBias, SyncShape())
    SYNC_SUPERCLASS(RndTransformable)
END_PROPSYNCS
