#include "rndobj/MeshDeform.h"
#include "MSL_Common/extras.h"
#include "obj/ObjMacros.h"
#include "obj/Object.h"
#include "obj/PropSync_p.h"
#include "os/Debug.h"
#include "rndobj/Mesh.h"
#include "utl/MakeString.h"
#include "utl/MemMgr.h"
#include "utl/Symbols.h"
#include "math/Mtx.h"
#include "math/Rot.h"
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

extern void *
MemResizeElem(void *&, int &, void *, int, int, const char *);

void *RndMeshDeform::VertArray::FindVert(int vert) {
    u8 *buf = (u8 *)mData;
    while (vert != 0) {
        buf = (*buf * 2) + buf + 1;
        vert--;
    }
    MILO_ASSERT(buf <= (u8 *)mData + mSize, 0x37);
    return buf;
}

void RndMeshDeform::VertArray::CopyVert(int to, int from, RndMeshDeform::VertArray &fromArr) {
    int num = fromArr.NumVerts();
    MILO_ASSERT(from >= 0 && from < num, 0x41);
    u8 buf[VertArray::kMaxWeights * 2 + 1];
    u8 *src = (u8 *)fromArr.FindVert(from);
    memcpy(buf, src, *src * 2 + 1);
    if (to > NumVerts()) {
        MILO_FAIL("can't copy vert past end");
        return;
    }
    u8 *dst = (u8 *)FindVert(to);
    int insertLength = *buf * 2 + 1;
    int cutLength = (dst == (u8 *)mData + mSize) ? 0 : *dst * 2 + 1;
    void *out = MemResizeElem(
        mData, mSize, dst, cutLength, insertLength, "RndMeshDeform");
    memcpy(out, buf, *buf * 2 + 1);
}

void RndMeshDeform::VertArray::Load(BinStream &bs) {
    int siz;
    bs >> siz;
    SetSize(siz);
    bs.Read(mData, mSize);
}

int RndMeshDeform::VertArray::AppendWeights(int num, int *boneIndices, float *weights) {
    MILO_ASSERT(num < VertArray::kMaxWeights, 0x5F);
    int numVerts = NumVerts();
    RndMeshDeform * &_ref0 = mParent;
    float sum = 0.0f;
    for (int i = 0; i < num; i++) {
        for (int j = i + 1; j < num; j++) {
            if (boneIndices[j] == boneIndices[i]) {
                weights[i] += weights[j];
                num--;
                boneIndices[j] = boneIndices[num];
                weights[j] = weights[num];
                j--;
            }
        }
        float w = weights[i];
        if (w <= 0.0f) {
            TheDebug.Notify(MakeString(
                "%s vert %d has negative weight %g on bone, won't export",
                PathName(_ref0), numVerts, w));
            weights[i] = 0.0f;
        }
        sum += weights[i];
    }
    if (sum > 0.0f) {
        for (int i = 0; i < num; i++) {
            weights[i] /= sum;
        }
    } else {
        TheDebug.Notify(MakeString(
            "%s vert %d weights sum to %g, check the skinning",
            PathName(_ref0), numVerts, sum));
    }
    u8 *elem = (u8 *)MemResizeElem(
        mData, mSize, (char *)mData + mSize, 0, num * 2 + 1, "RndMeshDeform");
    *elem = num;
    for (int i = 0; i < num; i++) {
        elem[i * 2 + 1] = boneIndices[i];
        float w = weights[i];
        if (w > 1.0f)
            w = 1.0f;
        else if (w < 0.0f)
            w = 0.0f;
        elem[i * 2 + 2] = 255.0f * w + 0.5f;
    }
    return numVerts;
}

RndMeshDeform::RndMeshDeform()
    : mMesh(this, 0), mBones(this), mVerts(this), mSkipInverse(0), mDeformed(0) {}

RndMeshDeform::~RndMeshDeform() {}

void RndMeshDeform::SetKeepMeshData() {
    if (mMesh) {
        mMesh->SetKeepMeshData(true);
    }
}

void RndMeshDeform::CopyWeights(int to, int from, RndMeshDeform *fromMd) {
    mVerts.CopyVert(to, from, fromMd ? fromMd->mVerts : mVerts);
}

RndMeshDeform *RndMeshDeform::FindDeform(RndMesh *m) {
    std::vector<ObjRef *>::const_reverse_iterator rit = m->Refs().rbegin();
    std::vector<ObjRef *>::const_reverse_iterator ritEnd = m->Refs().rend();
    for (; rit != ritEnd; ++rit) {
        RndMeshDeform *md = dynamic_cast<RndMeshDeform *>((*rit)->RefOwner());
        if (md) {
            MILO_ASSERT(md->Mesh() == m, 0x125);
            return md;
        }
    }
    return NULL;
}

bool RndMeshDeform::IsExoBone(RndTransformable *t) {
    if (!t) return false;
    return strnicmp("exo_", ((const char **)((void **)(*(void ***)t))[0])[3], 4) == 0;
}

void RndMeshDeform::BoneDesc::ExportWorldXfm(Transform &xfm) {
    xfm.Reset();
    RndTransformable *t = mBone;
    while (RndMeshDeform::IsExoBone(t)) {
        Multiply(xfm, t->LocalXfm(), xfm);
        t = t->TransParent();
    }
    Multiply(xfm, unk54, xfm);
}

void RndMeshDeform::SetMesh(RndMesh *mesh) {
    mMesh = mesh;
    mVerts.Clear();
}

void RndMeshDeform::SetNumBones(int n) { mBones.resize(n); }

void operator>>(BinStream &bs, RndMeshDeform::BoneDesc &desc) {
    bs >> desc.mBone;
    bs >> desc.unk14 >> desc.unk54;
}

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

void RndMeshDeform::Print() {
    TheDebug << "num_verts " << mVerts.NumVerts() << "\n";
    TheDebug << "mesh_inverse " << mMeshInverse << "\n";
    TheDebug << "skip_inverse " << mSkipInverse << "\n";
    TheDebug << "mesh " << mMesh.Ptr() << "\n";
    for (unsigned int i = 0; i < mBones.size(); i++) {
        BoneDesc &cur = mBones[i];
        TheDebug << "bone" << (int)i << ":\n";
        TheDebug << "   " << cur.mBone.Ptr() << "\n";
        TheDebug << "   " << cur.unk14 << "\n";
        TheDebug << "   " << cur.unk54 << "\n";
    }
    int idx = 0;
    u8 *cData = (u8 *)mVerts.mData;
    while (cData < (u8 *)mVerts.mData + mVerts.mSize) {
        TheDebug << "weights" << idx << ": ";
        u8 *p = cData + 1;
        for (int j = 0; j < (int)*cData; j++) {
            TheDebug << "(" << p[0] << " "
                     << (float)p[1] * 0.003921568859368563f << ") ";
            p += 2;
        }
        TheDebug << "\n";
        idx++;
        cData += (*cData * 2) + 1;
    }
}

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