#include "char/CharBonesMeshes.h"
#include "char/CharUtl.h"
#include "math/Mtx.h"
#include "math/Rot.h"
#include "utl/Std.h"
#include "utl/Symbols.h"
#ifdef HX_NATIVE
#include <cmath>   // V38: CBM_DBG-gated scale-channel instrumentation
#include <cstdio>
#include <cstdlib>
#include <cstring>
#endif

CharBonesMeshes::CharBonesMeshes()
    : mMeshes(this), mDummyMesh(Hmx::Object::New<RndTransformable>()) {}

CharBonesMeshes::~CharBonesMeshes() {
    mMeshes.clear();
    delete mDummyMesh;
}

void CharBonesMeshes::Replace(Hmx::Object *from, Hmx::Object *to) {
    Hmx::Object::Replace(from, to);
    if (from != mDummyMesh) {
        for (ObjVector<ObjOwnerPtr<RndTransformable, ObjectDir> >::iterator it =
                 mMeshes.begin();
             it != mMeshes.end();
             ++it) {
            if (*it == from) {
                *it = dynamic_cast<RndTransformable *>(to);
                if (!*it)
                    *it = mDummyMesh;
                return;
            }
        }
    }
}

// fn_804B07F4
void CharBonesMeshes::ReallocateInternal() {
    CharBonesAlloc::ReallocateInternal();
    String str;
#ifdef MILO_DEBUG
    {
        ObjVector<ObjOwnerPtr<RndTransformable, ObjectDir> > temp(this);
        mMeshes.swap(temp);
    }
#else
    mMeshes =
        bool(ObjVector<ObjOwnerPtr<RndTransformable, ObjectDir> >(this)); // ClearAndShrink?
#endif
    mMeshes.resize(mBones.size());
    for (int i = 0; mMeshes.size() > i; i++) {
        mMeshes[i] = CharUtlFindBoneTrans(mBones[i].name.mStr, Dir());
        if (!mMeshes[i]) {
            if (strncmp("bone_facing", mBones[i].name.mStr, 0xB)) {
                str += MakeString("%s, ", mBones[i].name);
            }
            mMeshes[i] = mDummyMesh;
        }
    }
    if (mMeshes.empty())
        return;
    else
        AcquirePose();
}

void CharBonesMeshes::AcquirePose() {
    ObjOwnerPtr<RndTransformable> *curMesh = &mMeshes[0];
    Vector3 *end = (Vector3 *)ScaleOffset();
    for (Vector3 *p = (Vector3 *)Start(); p < end; p++, curMesh++) {
        *p = (*curMesh)->mLocalXfm.v;
    }
    Vector3 *vEnd = (Vector3 *)QuatOffset();
    for (Vector3 *p = (Vector3 *)ScaleOffset(); p < vEnd; p++, curMesh++) {
        MakeScale((*curMesh)->mLocalXfm.m, *p);
    }
    Hmx::Quat *qEnd = (Hmx::Quat *)RotXOffset();
    for (Hmx::Quat *p = (Hmx::Quat *)QuatOffset(); p < qEnd; p++, curMesh++) {
        p->Set((*curMesh)->mLocalXfm.m);
    }
    float *rotIt = (float *)RotXOffset();
    float *xEnd = (float *)RotYOffset();
    for (; rotIt < xEnd; rotIt++, curMesh++) {
        *rotIt = GetXAngle((*curMesh)->mLocalXfm.m);
    }
    float *yEnd = (float *)RotZOffset();
    for (; rotIt < yEnd; rotIt++, curMesh++) {
        *rotIt = GetYAngle((*curMesh)->mLocalXfm.m);
    }
    float *zEnd = (float *)EndOffset();
    for (; rotIt < zEnd; rotIt++, curMesh++) {
        *rotIt = GetZAngle((*curMesh)->mLocalXfm.m);
    }
}

// fn_804B0C60 - pose meshes
void CharBonesMeshes::PoseMeshes() {
    ObjOwnerPtr<RndTransformable> *curMesh = &mMeshes[0];
    Vector3 *end = (Vector3 *)ScaleOffset();
    for (Vector3 *p = (Vector3 *)Start(); p < end; p++, curMesh++) {
        (*curMesh)->SetLocalPos(*p);
    }
    if (mQuatCount < mMeshes.size()) {
        curMesh = &mMeshes[mQuatCount];
        Hmx::Quat *qEnd = (Hmx::Quat *)RotXOffset();
        for (Hmx::Quat *p = (Hmx::Quat *)QuatOffset(); p < qEnd; p++, curMesh++) {
#ifdef HX_NATIVE
            // V38 probe: dump quat magnitude (pre-Normalize) + det for a named bone.
            if (getenv("CBM_DBG2")) {
                const char* bn = (*curMesh)->Name() ? (*curMesh)->Name() : "?";
                const char* f = getenv("CBM_DBG2");
                if (strstr(bn, f)) {
                    float mag = std::sqrt(p->x*p->x+p->y*p->y+p->z*p->z+p->w*p->w);
                    fprintf(stderr, "[CBM_DBG2] QUAT bone='%s' quatMag=%.4f\n", bn, mag);
                }
            }
#endif
            Normalize(*p, *p);
            MakeRotMatrix(*p, (*curMesh)->DirtyLocalXfm().m);
        }
        float *rotIt = (float *)RotXOffset();
        float *xEnd = (float *)RotYOffset();
        for (; rotIt < xEnd; rotIt++, curMesh++) {
            (*curMesh)->DirtyLocalXfm().m.RotateAboutX(*rotIt);
        }
        float *yEnd = (float *)RotZOffset();
        for (; rotIt < yEnd; rotIt++, curMesh++) {
            (*curMesh)->DirtyLocalXfm().m.RotateAboutY(*rotIt);
        }
        float *zEnd = (float *)EndOffset();
        for (; rotIt < zEnd; rotIt++, curMesh++) {
            (*curMesh)->DirtyLocalXfm().m.RotateAboutZ(*rotIt);
        }
    }
#ifdef HX_NATIVE
    // V38 probe: after QUAT+ROT (pre-SCALE), dump the final LocalXfm det for a
    // named bone, to localize whether the crowd 0.53-det is born here.
    if (getenv("CBM_DBG2")) {
        const char* f = getenv("CBM_DBG2");
        ObjOwnerPtr<RndTransformable>* cm = &mMeshes[0];
        for (int i = 0; i < (int)mMeshes.size(); i++, cm++) {
            const char* bn = (*cm)->Name() ? (*cm)->Name() : "?";
            if (strstr(bn, f)) {
                const Hmx::Matrix3& m = (*cm)->LocalXfm().m;
                float det = m.x.x*(m.y.y*m.z.z-m.y.z*m.z.y)
                          - m.x.y*(m.y.x*m.z.z-m.y.z*m.z.x)
                          + m.x.z*(m.y.x*m.z.y-m.y.y*m.z.x);
                float lx=std::sqrt(m.x.x*m.x.x+m.x.y*m.x.y+m.x.z*m.x.z);
                float ly=std::sqrt(m.y.x*m.y.x+m.y.y*m.y.y+m.y.z*m.y.z);
                float lz=std::sqrt(m.z.x*m.z.x+m.z.y*m.z.y+m.z.z*m.z.z);
                fprintf(stderr,"[CBM_DBG2] FINAL bone='%s' det=%.3f rowLen=(%.3f,%.3f,%.3f)\n",
                    bn, det, lx, ly, lz);
            }
        }
    }
#endif
    if (mScaleCount < mMeshes.size()) {
        curMesh = &mMeshes[mScaleCount];
        Vector3 *vEnd = (Vector3 *)QuatOffset();
        for (Vector3 *p = (Vector3 *)ScaleOffset(); p < vEnd; p++, curMesh++) {
            Transform &xfm = (*curMesh)->DirtyLocalXfm();
            Vector3 scale;
            MakeScale(xfm.m, scale);
#ifdef HX_NATIVE
            // V38 instrumentation (env-gated, OFF by default). CBM_DBG=1 dumps the
            // decoded SCALE channel `p` and the pre-divide matrix row lengths for
            // each scale-bone, to localize the crowd/extras 0.53-det Y-squash.
            if (getenv("CBM_DBG")) {
                const char* bn = (*curMesh)->Name() ? (*curMesh)->Name() : "?";
                fprintf(stderr, "[CBM_DBG] bone='%s' scaleChan=(%.3f,%.3f,%.3f) "
                    "rowLen=(%.3f,%.3f,%.3f)\n",
                    bn, p->x, p->y, p->z, scale.x, scale.y, scale.z);
            }
#endif
            xfm.m.x *= p->x / scale.x;
            xfm.m.y *= p->y / scale.y;
            xfm.m.z *= p->z / scale.z;
        }
    }
}

void CharBonesMeshes::StuffMeshes(std::list<Hmx::Object *> &oList) {
    for (int i = 0; i < mMeshes.size(); i++) {
        oList.push_back(mMeshes[i]);
    }
}

BEGIN_PROPSYNCS(CharBonesMeshes)
    SYNC_PROP(meshes, mMeshes)
    SYNC_SUPERCLASS(CharBonesObject)
END_PROPSYNCS