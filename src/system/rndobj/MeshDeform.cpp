#include "rndobj/MeshDeform.h"
#include "obj/ObjMacros.h"
#include "obj/Object.h"
#include "obj/PropSync_p.h"
#include "os/Debug.h"
#include "rndobj/Mesh.h"
#include "utl/MakeString.h"
#include "utl/MemMgr.h"
#include "utl/Symbols.h"
#include "math/Mtx.h"
#include "math/Vec.h"

INIT_REVS(RndMeshDeform)

RndMeshDeform::VertArray::VertArray(RndMeshDeform *md)
    : mSize(0), mData(0), mParent(md) {}

RndMeshDeform::VertArray::~VertArray() { _MemFree(mData); }

void RndMeshDeform::VertArray::Clear() { SetSize(0); }

int RndMeshDeform::VertArray::NumVerts() {
    void *end = (void *)((int)mData + mSize);
    int i = 0;
    u8 *buf = (u8 *)mData;
    for (; buf < end;) {
        i++;
        buf = (*buf << 1) + buf;
        buf++;
    }
    return i;
}

void RndMeshDeform::VertArray::SetSize(int i) {
    if (mSize != i) {
        mSize = i;
        _MemFree(mData);
        mData = _MemAlloc(mSize, 0);
    }
}

void RndMeshDeform::VertArray::Copy(const RndMeshDeform::VertArray &other) {
    SetSize(other.mSize);
    memcpy(mData, other.mData, mSize);
}

void RndMeshDeform::VertArray::Load(BinStream &bs) {
    int siz;
    bs >> siz;
    SetSize(siz);
    bs.Read(mData, mSize);
}

RndMeshDeform::RndMeshDeform()
    : mMesh(this, 0), mBones(this), mVerts(this), mSkipInverse(0), mDeformed(0) {}

RndMeshDeform::~RndMeshDeform() {}

void RndMeshDeform::SetKeepMeshData() {
    if (mMesh) {
        mMesh->SetKeepMeshData(true);
    }
}

void RndMeshDeform::SetMesh(RndMesh *mesh) {
    mMesh = mesh;
    mVerts.Clear();
}

BinStream &operator>>(BinStream &bs, RndMeshDeform::BoneDesc &) { return bs; }

SAVE_OBJ(RndMeshDeform, 532)

BEGIN_COPYS(RndMeshDeform)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY(RndMeshDeform)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mMesh)
        mMeshInverse = c->mMeshInverse;
        COPY_MEMBER(mBones)
        COPY_MEMBER(mSkipInverse)
        mVerts.Copy(c->mVerts);
    END_COPYING_MEMBERS
END_COPYS

void RndMeshDeform::PreSave(BinStream &) { SetKeepMeshData(); }

void RndMeshDeform::Print() {}

BEGIN_PROPSYNCS(RndMeshDeform)
    SYNC_PROP(mesh, mMesh)
    SYNC_PROP_SET(num_verts, mVerts.NumVerts(), )
    SYNC_PROP_SET(num_bones, (int)mBones.size(), )
END_PROPSYNCS

BEGIN_LOADS(RndMeshDeform)
    LOAD_REVS(bs)
    ASSERT_REVS(1, 0)
    LOAD_SUPERCLASS(Hmx::Object)
    bs >> mMesh;
    int what = 0;
    if (gRev < 1) {
        bs >> what;
    }
    int bones;
    bs >> bones;
    if (gRev < 1) {
        mVerts.Clear();
        float weights[64];
        int boneIndices[64];
        for (int i = 0; i < what; i++) {
            int numWeights = 0;
            for (int j = 0; j < bones; j++) {
                float w;
                bs >> w;
                if (w != 0) {
                    boneIndices[numWeights] = j;
                    weights[numWeights] = w;
                    numWeights++;
                }
            }
            mVerts.AppendWeights(numWeights, boneIndices, weights);
        }
    }
    SetNumBones(bones);
    for (int i = 0; i < bones; i++) {
        bs >> mBones[i];
    }
    if (gRev != 0)
        mVerts.Load(bs);
    bs >> mMeshInverse;
    // how NOT to check against the identity matrix
    mSkipInverse =
        (mMeshInverse.v.x == 0 && mMeshInverse.v.y == 0 && mMeshInverse.v.z == 0
         && mMeshInverse.m.x.x == 1 && mMeshInverse.m.x.y == 0 && mMeshInverse.m.x.z == 0
         && mMeshInverse.m.y.x == 0 && mMeshInverse.m.y.y == 1 && mMeshInverse.m.y.z == 0
         && mMeshInverse.m.z.x == 0 && mMeshInverse.m.z.y == 0
         && mMeshInverse.m.z.z == 1);
END_LOADS

void RndMeshDeform::Reskin(SyncMeshCB *cb, bool force) {
    if (!mMesh) return;
    if (!cb->HasMesh(mMesh) && !force && mDeformed) return;
    cb->SyncMesh(mMesh, 0x1f);
    mDeformed = 1;
    std::vector<Transform> xfms;
    {
        MemDoTempAllocations mem(1, 0);
        xfms.resize(mBones.size());
    }
    for (unsigned int i = 0; i < mBones.size(); i++) {
        if (mBones[i].mBone) {
            Transform tmp;
            mBones[i].ExportWorldXfm(tmp);
            Multiply(mBones[i].unk14, tmp, xfms[i]);
        } else {
            xfms[i].Reset();
            TheDebug << MakeString("%s null bone %d\n", PathName(this), i);
        }
    }
    int meshNumVerts = mMesh->Verts().size();
    int vertIdx = 0;
    u8 *vertData = (u8 *)mVerts.mData;
    while (vertData < (u8 *)mVerts.mData + mVerts.mSize) {
        if (vertIdx == meshNumVerts) {
            TheDebug.Notify(MakeString(
                "%s cannot reskin %s, the vert counts differ mesh:%d me:%d",
                PathName(this), PathName(mMesh.Ptr()), meshNumVerts,
                mVerts.NumVerts()));
            return;
        }
        Transform weighted;
        weighted.m.x.x = 0; weighted.m.x.y = 0; weighted.m.x.z = 0;
        weighted.m.y.x = 0; weighted.m.y.y = 0; weighted.m.y.z = 0;
        weighted.m.z.x = 0; weighted.m.z.y = 0; weighted.m.z.z = 0;
        weighted.v.x = 0; weighted.v.y = 0; weighted.v.z = 0;
        float totalWeight = 0.0f;
        u8 *pair = vertData;
        int n = 0;
        while (n < (int)*vertData) {
            unsigned int boneIdx = pair[1];
            unsigned int weightByte = pair[2];
            pair += 2;
            n++;
            float w = (1.0f / 255.0f) * (float)weightByte;
            totalWeight += w;
            Transform &bx = xfms[boneIdx];
            weighted.m.x.x += bx.m.x.x * w;
            weighted.m.x.y += bx.m.x.y * w;
            weighted.m.x.z += bx.m.x.z * w;
            weighted.m.y.x += bx.m.y.x * w;
            weighted.m.y.y += bx.m.y.y * w;
            weighted.m.y.z += bx.m.y.z * w;
            weighted.m.z.x += bx.m.z.x * w;
            weighted.m.z.y += bx.m.z.y * w;
            weighted.m.z.z += bx.m.z.z * w;
            weighted.v.x += bx.v.x * w;
            weighted.v.y += bx.v.y * w;
            weighted.v.z += bx.v.z * w;
        }
        float inv = 1.0f / totalWeight;
        weighted.m.x.x *= inv; weighted.m.x.y *= inv; weighted.m.x.z *= inv;
        weighted.m.y.x *= inv; weighted.m.y.y *= inv; weighted.m.y.z *= inv;
        weighted.m.z.x *= inv; weighted.m.z.y *= inv; weighted.m.z.z *= inv;
        weighted.v.x *= inv; weighted.v.y *= inv; weighted.v.z *= inv;
        if (!mSkipInverse) {
            Multiply(weighted, mMeshInverse, weighted);
        }
        // transform pos: new = pos * M + v (uses paired-singles)
        Multiply(mMesh->Verts(vertIdx).pos, weighted, mMesh->Verts(vertIdx).pos);
        RndMesh::Vert &v = mMesh->Verts(vertIdx);
        // pick perpendicular axis to v.norm based on smallest abs
        Vector3 axis;
        float anx = std::fabs(v.norm.x);
        float any = std::fabs(v.norm.y);
        float anz = std::fabs(v.norm.z);
        if (anx <= any && anx <= anz) {
            axis.x = v.norm.x * -v.norm.x + 1.0f;
            axis.y = v.norm.y * -v.norm.x;
            axis.z = v.norm.z * -v.norm.x;
        } else if (any < anx && any < anz) {
            axis.x = v.norm.x * -v.norm.y;
            axis.y = v.norm.y * -v.norm.y + 1.0f;
            axis.z = v.norm.z * -v.norm.y;
        } else {
            axis.x = v.norm.x * -v.norm.z;
            axis.y = v.norm.y * -v.norm.z;
            axis.z = v.norm.z * -v.norm.z + 1.0f;
        }
        // cross = v.norm x axis
        Vector3 cross;
        cross.x = v.norm.y * axis.z - v.norm.z * axis.y;
        cross.y = v.norm.z * axis.x - v.norm.x * axis.z;
        cross.z = v.norm.x * axis.y - v.norm.y * axis.x;
        // transform axis and cross (rotation part only)
        Multiply(axis, weighted.m, axis);
        Multiply(cross, weighted.m, cross);
        // norm = axis x cross
        v.norm.x = axis.y * cross.z - axis.z * cross.y;
        v.norm.y = axis.z * cross.x - axis.x * cross.z;
        v.norm.z = axis.x * cross.y - axis.y * cross.x;
        Normalize(v.norm, v.norm);
        vertIdx++;
        vertData += (*vertData * 2) + 1;
    }
}

BEGIN_HANDLERS(RndMeshDeform)
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0x2A1)
END_HANDLERS