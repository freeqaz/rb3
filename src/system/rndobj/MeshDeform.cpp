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
    RndMesh *mesh = &*mMesh;
    if (!cb->HasMesh(mesh) && !force && mDeformed) return;
    mDeformed = 1;
    int numBones = mBones.size();
    MemDoTempAllocations mem(1, 0);
    std::vector<Transform> xfms(numBones);
    for (int i = 0; i < numBones; i++) {
        if (mBones[i].mBone) {
            mBones[i].ExportWorldXfm(xfms[i]);
        } else {
            xfms[i].Reset();
            TheDebug.Notify(MakeString("%s null bone %d\n", PathName(this), i));
        }
    }
    int meshVerts = mesh->NumVerts();
    int myVerts = mVerts.NumVerts();
    if (myVerts != meshVerts) {
        TheDebug.Notify(MakeString(
            "%s cannot reskin %s, the vert counts differ mesh:%d me:%d",
            PathName(this), PathName(mesh), meshVerts, myVerts));
        return;
    }
    cb->SyncMesh(mesh, 0x1f);
}

BEGIN_HANDLERS(RndMeshDeform)
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0x2A1)
END_HANDLERS