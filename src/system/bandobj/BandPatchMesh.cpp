#include "bandobj/BandPatchMesh.h"
#include "bandobj/BandCharDesc.h"
#include "math/Rot.h"
#include "utl/Symbols.h"
#include <algorithm>
#include <cmath>

INIT_REVS(BandPatchMesh);

void BandPatchMesh::MeshVert::SetVert(
    const BandPatchMesh::MeshVert *mvert, const RndMesh::Vert *vert
) {
    mVert = vert;
    unk4 = mvert->unk4;
    unk10 = mvert->unk10;
    unk1c = mvert->unk1c;
    unk26 = mvert->unk26;
}

void BandPatchMesh::MeshVert::SetVert(const RndMesh::Vert *vert) {
    mVert = vert;
    ZeroOut();
}

void BandPatchMesh::MeshVert::ZeroOut() {
    unk1c.Zero();
    unk4.Zero();
    unk10.Zero();
}

int BandPatchMesh::MeshVert::AddUV(
    const BandPatchMesh::MeshVert *mv, const Vector2 &vr, const Vector2 *vp
) {
    MILO_ASSERT(this != mv, 0x55);
    MILO_ASSERT(mv->mVert, 0x57);
    Vector3 v48;
    Subtract(mVert->pos, mv->mVert->pos, v48);
    float lensq = LengthSquared(v48);
    float dot = Dot(v48, mv->mVert->norm);
    ScaleAddEq(v48, mv->mVert->norm, -dot);
    float v50x = mv->unk1c.x;
    float v50y = mv->unk1c.y;
    float newlensq = LengthSquared(v48);
    if (newlensq > 0) {
        float ratio = newlensq / lensq;
        float r = 1.0f / std::sqrt(ratio);
        float recipsq = 0.5f * r * (3.0f - ratio * r * r);
        float dot4 = Dot(v48, mv->unk10);
        float vry = vr.y;
        float dot5 = Dot(v48, mv->unk4);
        v50x += recipsq * vr.x * dot5;
        v50y += recipsq * vry * dot4;
    } else if (lensq > 0)
        return 0;
    if (vp) {
        float dx = vp->x - v50x;
        float dy = vp->y - v50y;
        if (dx * dx + dy * dy > 0.25f)
            return 0;
    }
    unk1c.x += v50x;
    unk1c.y += v50y;
    unk4 += mv->unk4;
    unk10 += mv->unk10;
    return 1;
}

void BandPatchMesh::MeshVert::Normalize(int count) {
    MILO_ASSERT(count > 0, 0x7E);
    unk1c /= count;
    Vector3 v40;
    Cross(unk4, unk10, v40);
    Hmx::Quat q50;
    MakeRotQuat(v40, mVert->norm, q50);
    Hmx::Matrix3 m34;
    MakeRotMatrix(q50, m34);
    Multiply(unk4, m34, unk4);
    Multiply(unk10, m34, unk10);
    ::Normalize(unk4, unk4);
    ::Normalize(unk10, unk10);
    Vector3 v5c;
    ::Add(unk4, unk10, v5c);
    ::Normalize(v5c, v5c);
    Vector3 v68;
    Cross(mVert->norm, v5c, v68);
    ::Normalize(v68, v68);
    ::Add(v5c, v68, unk10);
    Subtract(v5c, v68, unk4);
    ::Normalize(unk4, unk4);
    ::Normalize(unk10, unk10);
    unk26 = 0;
    if (unk1c.x < 0)
        unk26 |= 1;
    else if (unk1c.x > 1.0f)
        unk26 |= 2;
    if (unk1c.y < 0)
        unk26 |= 4;
    else if (unk1c.y > 1.0f)
        unk26 |= 8;
}

struct SortByZ {
    bool operator()(RndMesh::Vert *v1, RndMesh::Vert *v2) {
        if (v1->pos.z != v2->pos.z)
            return v1->pos.z < v2->pos.z;
        else if (v1->pos.y != v2->pos.y)
            return v1->pos.y < v2->pos.y;
        else
            return v1->pos.x < v2->pos.x;
    }
};

struct SortByWorkVertZ {
    bool operator()(BandPatchMesh::MeshVert *v1, BandPatchMesh::MeshVert *v2) {
        return v1->mVert->pos.z < v2->mVert->pos.z;
    }
};

// Explicit specializations to avoid bool materialization in comparison loops,
// so CW uses blt/bge directly after fcmpo instead of mfcr/srwi./bne.
namespace stlpmtx_std {

template <>
BandPatchMesh::MeshVert **
__unguarded_partition<BandPatchMesh::MeshVert **, BandPatchMesh::MeshVert *, SortByWorkVertZ>(
    BandPatchMesh::MeshVert **__first,
    BandPatchMesh::MeshVert **__last,
    BandPatchMesh::MeshVert *__pivot,
    SortByWorkVertZ
) {
    for (;;) {
        while ((*__first)->mVert->pos.z < __pivot->mVert->pos.z)
            ++__first;
        --__last;
        while (__pivot->mVert->pos.z < (*__last)->mVert->pos.z)
            --__last;
        if (!(__first < __last))
            return __first;
        iter_swap(__first, __last);
        ++__first;
    }
}

template <>
void __unguarded_linear_insert<BandPatchMesh::MeshVert **, BandPatchMesh::MeshVert *, SortByWorkVertZ>(
    BandPatchMesh::MeshVert **__last,
    BandPatchMesh::MeshVert *__val,
    SortByWorkVertZ
) {
    BandPatchMesh::MeshVert **__next = __last;
    --__next;
    while (__val->mVert->pos.z < (*__next)->mVert->pos.z) {
        *__last = *__next;
        __last = __next;
        --__next;
    }
    *__last = __val;
}

template <>
void __introsort_loop<BandPatchMesh::MeshVert **, BandPatchMesh::MeshVert *, long, SortByWorkVertZ>(
    BandPatchMesh::MeshVert **__first,
    BandPatchMesh::MeshVert **__last,
    BandPatchMesh::MeshVert **,
    long __depth_limit,
    SortByWorkVertZ __comp
) {
    while (__last - __first > 16) {
        if (__depth_limit == 0) {
            partial_sort(__first, __last, __last, __comp);
            return;
        }
        ptrdiff_t __len = __last - __first;
        BandPatchMesh::MeshVert *__a = *__first;
        --__depth_limit;
        BandPatchMesh::MeshVert **__mid = __first + __len / 2;
        BandPatchMesh::MeshVert *__b = *__mid;
        BandPatchMesh::MeshVert **__pivot_ptr;
        float a_z = __a->mVert->pos.z;
        float b_z = __b->mVert->pos.z;
        if (a_z < b_z) {
            float c_z = (*(__last - 1))->mVert->pos.z;
            if (b_z < c_z)
                __pivot_ptr = __mid;
            else if (a_z < c_z)
                __pivot_ptr = __last - 1;
            else
                __pivot_ptr = __first;
        } else {
            float c_z = (*(__last - 1))->mVert->pos.z;
            if (a_z < c_z)
                __pivot_ptr = __first;
            else if (b_z < c_z)
                __pivot_ptr = __last - 1;
            else
                __pivot_ptr = __mid;
        }
        BandPatchMesh::MeshVert **__cut =
            __unguarded_partition(__first, __last, *__pivot_ptr, __comp);
        __introsort_loop(__cut, __last, (BandPatchMesh::MeshVert **)0, __depth_limit, __comp);
        __last = __cut;
    }
}

} // namespace stlpmtx_std

BandPatchMesh::WorkVerts::WorkVerts(RndMesh *mesh, const Vector2 &v2)
    : unkc(0), mMesh(mesh), unk34(v2), unk3c((1.0f / v2.x), (1.0f / v2.y)) {
    unk0 = 0;
    MemDoTempAllocations m(true, false);
    unk18.resize(mMesh->Verts().size());
    for (int i = 0; i < unk18.size(); i++) {
        unk18[i] = &mMesh->Verts(i);
    }
    std::sort(unk18.begin(), unk18.end(), SortByZ());
}

BandPatchMesh::WorkVerts::~WorkVerts() { delete[] unkc; }

void BandPatchMesh::WorkVerts::SortWorkVertsByZ() {
    std::sort(unk10.begin(), unk10.end(), SortByWorkVertZ());
}

void BandPatchMesh::WorkVerts::SetMeshVerts() {
    MILO_ASSERT(mMeshVerts.empty(), 0x10C);
    MemDoTempAllocations m(true, false);
    unk10.reserve(mMesh->Verts().size());
    unk20.reserve(mMesh->Faces().size());
    unk28.resize(mMesh->Faces().size());
    for (int i = 0; i < unk28.size(); i++) {
        unk28[i].mFlags = -1;
    }
    mMeshVerts.resize(mMesh->Verts().size());
    for (int i = 0; i < mMeshVerts.size(); i++) {
        mMeshVerts[i] = 0;
    }
    for (int i = 0; i < mMesh->Faces().size(); i++) {
        RndMesh::Face &curface = mMesh->Faces()[i];
        for (int j = 0; j < 3; j++) {
            ((int &)mMeshVerts[curface[j]])++;
        }
    }
    int count = 0;
    for (int i = 0; i < mMeshVerts.size(); i++) {
        int c = (int)mMeshVerts[i];
        mMeshVerts[i] = (MeshVert *)count;
        count += (((c + 1) & ~1) - 2) * 2 + 0x38;
    }
    unkc = new char[count];
    for (int i = 0; i < mMeshVerts.size(); i++) {
        mMeshVerts[i] = (MeshVert *)((char *)unkc + (int)mMeshVerts[i]);
        MeshVert *v = mMeshVerts[i];
        *((unsigned char *)v + 0x27) = 0;
        v->unk28 = -1;
        v->unk2c = -1;
        v->unk30 = 0;
        v->mVert = 0;
        v->unk24 = 0;
    }
    for (int i = 0; i < mMesh->Faces().size(); i++) {
        RndMesh::Face &curface = mMesh->Faces()[i];
        for (int j = 0; j < 3; j++) {
            MeshVert *mv = mMeshVerts[curface[j]];
            int n = mv->unk30;
            ((unsigned short *)((char *)mv + 0x32))[n] = i;
            mv->unk30 = n + 1;
        }
    }
    RndMesh::Vert *base = &mMesh->Verts()[0];
    for (int i = 0; i < unk18.size(); i++) {
        RndMesh::Vert *v1 = unk18[i];
        int vi = v1 - base;
        MeshVert *mv = mMeshVerts[vi];
        if (mv->unk28 == -1) {
            mv->unk28 = vi;
            int prev = vi;
            for (int j = i + 1; j < unk18.size(); j++) {
                RndMesh::Vert *v2 = unk18[j];
                bool diff = v1->pos.x != v2->pos.x || v1->pos.y != v2->pos.y
                    || v1->pos.z != v2->pos.z;
                if (diff)
                    break;
                int vi2 = v2 - base;
                mMeshVerts[vi2]->unk28 = vi;
                *((unsigned char *)mMeshVerts[vi2] + 0x27) = 1;
                mMeshVerts[prev]->unk2c = vi2;
                prev = vi2;
            }
            if (prev != vi) {
                *((unsigned char *)mMeshVerts[vi] + 0x27) = 1;
            }
        }
    }
}

void BandPatchMesh::WorkVerts::AddFace(int i, MeshVert *mv) {
    RndMesh::Face &curface = mMesh->Faces()[i];
    for (int n = 0; n < 3; n++) {
        SetMeshVertAndTwins(curface[n], mv);
    }
    TryAddFace(i, 3);
}

void BandPatchMesh::WorkVerts::AddEdge(
    BandPatchMesh::MeshVert *mv0, BandPatchMesh::MeshVert *mv1
) {
    int target = mv1->unk28;
    for (int idx = mv0->unk28; idx != -1; idx = mMeshVerts[idx]->unk2c) {
        MeshVert *mv = mMeshVerts[idx];
        unsigned short *faceidxptr = (unsigned short *)((char *)mv + 0x32);
        for (int i = 0; i < mv->unk30; i++) {
            int faceidx = faceidxptr[i];
            if (unk28[faceidx].mFlags == -1) {
                RndMesh *mesh = mMesh;
                RndMesh::Face &face = mesh->Faces()[faceidx];
                if (idx == face[0]) {
                    if (mMeshVerts[face[1]]->unk28 == target)
                        TryAddFace(faceidx, 0);
                } else if (idx == face[1]) {
                    if (mMeshVerts[face[2]]->unk28 == target)
                        TryAddFace(faceidx, 1);
                } else if (idx == face[2]) {
                    if (mMeshVerts[face[0]]->unk28 == target)
                        TryAddFace(faceidx, 2);
                }
            }
        }
    }
}

int BandPatchMesh::WorkVerts::TryAddFace(int faceidx, int b) {
    unk28[faceidx].mFlags = b;
    unk20.push_back(faceidx);
    MILO_ASSERT(b != 4, 0x2A1);
    MILO_ASSERT(b != -1, 0x2A2);
    RndMesh::Face &face = mMesh->Faces()[faceidx];
    int prevVertCount = unk10.size();
    MeshVert *verts[3];
    int allOut = 0xf;
    for (int i = 0; i < 3; i++) {
        MeshVert *mv = mMeshVerts[face[i]];
        verts[i] = mv;
        if (mv->mVert == 0) {
            MILO_ASSERT(b != 3, 0x2B1);
            AddMeshVertAndTwins(face[i], mMeshVerts[face[b]]);
        }
        allOut &= verts[i]->unk26;
    }
    int reject;
    if (allOut != 0) {
        reject = 1;
    } else {
        MeshVert temp;
        temp.SetVert(verts[0]->mVert);
        reject = 0;
        Vector2 v(temp.unk1c);
        if (temp.AddUV(verts[1], unk34, &v) != 0
            && temp.AddUV(verts[2], unk34, &v) != 0) {
            reject = 0;
        } else {
            reject = 1;
        }
    }
    if (reject == 0 && b != 3) {
        int prev = (b == 0) ? 2 : b - 1;
        int next = (b + 1) % 3;
        MeshVert *vb = verts[b];
        MeshVert *vn = verts[next];
        MeshVert *vp = verts[prev];
        float ey = vn->unk1c.y - vb->unk1c.y;
        float py = vp->unk1c.y - vb->unk1c.y;
        float ex = vn->unk1c.x - vb->unk1c.x;
        float px = vp->unk1c.x - vb->unk1c.x;
        float t = (ex * px + ey * py) / (ex * ex + ey * ey);
        if (t > 1.0f)
            t = 1.0f;
        else if (t < 0)
            t = 0;
        float projx = vb->unk1c.x + t * (vn->unk1c.x - vb->unk1c.x);
        float projy = vb->unk1c.y + t * (vn->unk1c.y - vb->unk1c.y);
        float dot = (vp->unk1c.x - projx) * (vp->unk1c.x - 0.5f)
            + (vp->unk1c.y - projy) * (vp->unk1c.y - 0.5f);
        reject = (dot < 0) ? 1 : 0;
    }
    if (reject != 0) {
        int added = unk10.size() - prevVertCount;
        for (int i = 0; i < added; i++) {
            unk10[unk10.size() - 1]->mVert = 0;
            unk10.resize(unk10.size() - 1);
        }
        unk20.resize(unk20.size() - 1);
        if (allOut == 0) {
            unk28[faceidx].mFlags = -1;
        }
        return 0;
    } else {
        unk28[faceidx].mFlags = 4;
        return 1;
    }
}

void BandPatchMesh::WorkVerts::SpreadEdges(int i) {
    MeshVert *meshverts[3];
    RndMesh::Face &curface = mMesh->Faces()[unk20[i]];
    for (int n = 0; n < 3; n++) {
        meshverts[n] = mMeshVerts[curface[n]];
    }
    AddEdge(meshverts[1], meshverts[0]);
    AddEdge(meshverts[2], meshverts[1]);
    AddEdge(meshverts[0], meshverts[2]);
}

int BandPatchMesh::WorkVerts::AddUvs(
    BandPatchMesh::MeshVert *mv1, BandPatchMesh::MeshVert *mv2, const Vector2 *v2
) {
    unsigned short *idxptr = (unsigned short *)mv2;
    int ret = 0;
    for (int i = 0; i < mv2->unk30; i++) {
        RndMesh::Face &curface = mMesh->Faces()[idxptr[0x32 / 2]];
        for (int j = 0; j < 3; j++) {
            MeshVert *curmv = mMeshVerts[curface[j]];
            if (curmv != mv2 && curmv->mVert != 0 && curmv->unk24 != unk0) {
                curmv->unk24 = unk0;
                ret += mv1->AddUV(curmv, unk34, v2);
            }
        }
        idxptr++;
    }
    return ret;
}

void BandPatchMesh::WorkVerts::SetMeshVertAndTwins(
    int idx, BandPatchMesh::MeshVert *first
) {
    MeshVert *cur = mMeshVerts[idx];
    MILO_ASSERT(!cur->mVert, 0x3BA);
    cur->SetVert(&mMesh->Verts(idx));
    unk10.push_back(cur);
    MILO_ASSERT(cur->mVert, 0x3C7);
    MILO_ASSERT(first->mVert, 0x3C8);
    cur->AddUV(first, unk34, 0);
    cur->Normalize(1);
    MILO_ASSERT(cur->mVert, 0x3D3);
    for (int num = cur->unk28; num != -1; num = mMeshVerts[num]->unk2c) {
        MeshVert *mt = mMeshVerts[num];
        if (mt != cur) {
            MILO_ASSERT(!mt->mVert, 0x3DB);
            unk10.push_back(mt);
            mt->SetVert(cur, &mMesh->Verts(num));
        }
    }
}

void BandPatchMesh::WorkVerts::AddMeshVertAndTwins(
    int idx, BandPatchMesh::MeshVert *first
) {
    MeshVert *cur = mMeshVerts[idx];
    MILO_ASSERT(!cur->mVert, 0x3EA);
    cur->SetVert(&mMesh->Verts(idx));
    unk10.push_back(cur);
    unk0++;
    MILO_ASSERT(cur->mVert, 0x3F9);
    MILO_ASSERT(first->mVert, 0x3FA);
    cur->AddUV(first, unk34, 0);
    cur->unk24 = unk0;
    first->unk24 = unk0;
    Vector2 v18(cur->unk1c);
    int count = 1;
    for (int num = cur->unk28; num != -1; num = mMeshVerts[num]->unk2c) {
        MeshVert *mt = mMeshVerts[num];
        count += AddUvs(cur, mt, &v18);
    }
    cur->Normalize(count);
    MILO_ASSERT(cur->mVert, 0x412);
    for (int num = cur->unk28; num != -1; num = mMeshVerts[num]->unk2c) {
        MeshVert *mt = mMeshVerts[num];
        if (mt != cur) {
            MILO_ASSERT(!mt->mVert, 0x41A);
            unk10.push_back(mt);
            mt->SetVert(cur, &mMesh->Verts(num));
        }
    }
}

void BandPatchMesh::WorkVerts::Project() {
    for (int i = 0; i < unk20.size(); i++)
        SpreadEdges(i);
}

struct SortByPointer {
    bool operator()(BandPatchMesh::MeshVert *v1, BandPatchMesh::MeshVert *v2) {
        return v1->mVert < v2->mVert;
    }
};

void BandPatchMesh::WorkVerts::SetVertsAndFaces(RndMesh *mesh, bool b) {
    std::sort(unk10.begin(), unk10.end(), SortByPointer());
    for (int i = 0; i < unk10.size(); i++) {
        unk10[i]->unk24 = i;
    }
    std::sort(unk20.begin(), unk20.end());
    mesh->Verts().resize(unk10.size(), true);
    mesh->Faces().resize(unk20.size());
    if (b) {
        MILO_ASSERT(mMesh->Mat(), 0x475);
        RndTex *dest = mMesh->Mat()->GetDiffuseTex();
        MILO_ASSERT(dest, 0x477);
        unk44.Set(dest->Width(), dest->Height());
        unk44 *= 0.707f;
        unk4c.Set(1.0f / unk44.x, 1.0f / unk44.y);
        unk54.Set(std::fabs(unk3c.x), std::fabs(unk3c.y));
        unk5c.Set(1.0f / unk54.x, 1.0f / unk54.y);
        for (int i = 0; i < mesh->Verts().size(); i++) {
            MeshVert *cur = unk10[i];
            Vector2 v40(0, 0);
            Vector2 v48(0, 0);
            ExtendTwin((const MeshVert *)this, v40, v48);
            v40 += cur->mVert->uv;
            v48 += cur->unk1c;
            SetRenderToVert(mesh->Verts(i), v40, v48);
        }
    } else {
        for (int i = 0; i < mesh->Verts().size(); i++) {
            MeshVert *cur = unk10[i];
            mesh->Verts(i) = *cur->mVert;
            mesh->Verts(i).uv = cur->unk1c;
        }
    }
    for (int i = 0; i < mesh->Faces().size(); i++) {
        RndMesh::Face &myface = mMesh->Faces()[unk20[i]];
        for (int j = 0; j < 3; j++) {
            mesh->Faces()[i][j] = mMeshVerts[myface[j]]->unk24;
        }
    }
}

void BandPatchMesh::WorkVerts::CopyDeformWeights(RndMeshDeform *m1, RndMeshDeform *md) {
    MILO_ASSERT(mMesh == md->Mesh(), 0x49E);
    for (int i = 0; i < unk10.size(); i++) {
        m1->CopyWeights(i, (unk10[i]->mVert - &mMesh->Verts(0)), md);
    }
}

RndTex *BandPatchMesh::MeshPair::OutputTex() const {
    if (mesh && mesh->Mat())
        return mesh->Mat()->GetDiffuseTex();
    else
        return 0;
}

void BandPatchMesh::MeshPair::AddMappingPatch(RndMesh *themesh) {
    patches.push_back();
    patches.back().mPatch = themesh;
}

BandPatchMesh::MeshPair::PatchPair &BandPatchMesh::MeshPair::AddPatch(bool permanent) {
    MILO_ASSERT(!permanent || patches.size() == 0, 0x4BE);
    ObjectDir *dir = mesh.Owner()->Dir();
    const char *name = PatchName();
    RndMesh *mesh = 0;
    if (permanent)
        mesh = dir->Find<RndMesh>(name, false);
    if (!mesh) {
        mesh = Hmx::Object::New<RndMesh>();
        if (permanent) {
            mesh->SetName(PatchName(), dir);
            mesh->SetOrder(0.01f);
        }
    }
    AddMappingPatch(mesh);
    return patches.back();
}

const char *BandPatchMesh::MeshPair::PatchName() const {
    if (mesh)
        return MakeString("%s_patch.mesh", FileGetBase(mesh->Name(), 0));
    else
        return "";
}

BandPatchMesh::BandPatchMesh(Hmx::Object *o)
    : mMeshes(o), mRenderTo(true), mSrc(o, 0), mCategory(0) {}

BandPatchMesh::BandPatchMesh(const BandPatchMesh &mesh)
    : mMeshes(mesh.mMeshes), mRenderTo(mesh.mRenderTo), mSrc(mesh.mSrc),
      mCategory(mesh.mCategory) {}

BandPatchMesh &BandPatchMesh::operator=(const BandPatchMesh &mesh) {
    mSrc = mesh.mSrc;
    mMeshes = mesh.mMeshes;
    mRenderTo = mesh.mRenderTo;
    mCategory = mesh.mCategory;
    return *this;
}

bool BandPatchMesh::ReProject() {
    PostRender();
    if (mSrc)
        ProjectPatches(mSrc->LocalXfm(), 0, true);
    PostRender();
    return mRenderTo;
}

void BandPatchMesh::PostRender() {
    for (ObjVector<MeshPair>::iterator mp = mMeshes.begin(); mp != mMeshes.end();
         ++mp) {
        for (ObjVector<MeshPair::PatchPair>::iterator pp = mp->patches.begin();
             pp != mp->patches.end();
             ++pp) {
            RndMesh *patch = pp->mPatch;
            if (patch && !patch->Dir()) {
                delete patch;
            }
        }
        mp->patches.clear();
    }
}

void BandPatchMesh::PreRender(BandCharDesc *desc, int iii) {
    if (mCategory == 0 || (iii & mCategory)) {
        for (ObjVector<MeshPair>::iterator mp = mMeshes.begin(); mp != mMeshes.end();
             ++mp) {
            MILO_ASSERT(mp->patches.empty(), 0x509);
        }
        if (mSrc) {
            for (ObjVector<MeshPair>::iterator mp = mMeshes.begin(); mp != mMeshes.end();
                 ++mp) {
                mp->AddPatch(true);
            }
        }
        ObjectDir *pdir = desc->GetPatchDir();
        if (pdir) {
            for (int i = 0; i < desc->mPatches.size(); i++) {
                BandCharDesc::Patch &patch = desc->mPatches[i];
                if (patch.mCategory & mCategory) {
                    RndMesh *mesh = desc->GetPatchMesh(patch);
                    RndTex *tex = 0;
                    if (patch.mTexture == -1) {
                        if (mesh && mesh->Mat()) {
                            tex = mesh->Mat()->GetDiffuseTex();
                        } else {
                            MILO_WARN(
                                "%s could not find texture from placement mesh, category %d.",
                                PathName(pdir),
                                mCategory
                            );
                        }
                    } else
                        tex = desc->GetPatchTex(patch);
                    if (tex) {
                        if (mesh) {
                            if (patch.mTexture == -1) {
                                if (mMeshes.size() == 1) {
                                    AddMappingPatch(mMeshes[0], mesh);
                                }
                            } else {
                                Transform tf60;
                                if (FindXfm(mesh, patch.mUV, tf60)) {
                                    Hmx::Matrix3 m88;
                                    m88.RotateAboutZ(patch.mRotation);
                                    Multiply(m88, tf60.m, tf60.m);
                                    tf60.m.x *= (patch.mScale.x * 0.5f);
                                    tf60.m.y *= (patch.mScale.y * 0.5f);
                                    ProjectPatches(tf60, tex, false);
                                } else {
                                    MILO_WARN(
                                        "Could not project %s onto %s\n",
                                        tex->Name(),
                                        mesh->Name()
                                    );
                                }
                            }
                        } else {
                            if (patch.mMeshName.empty()) {
                                ConstructQuad(tex);
                            } else {
                                MILO_WARN(
                                    "%s: could not find placement mesh %s",
                                    PathName(pdir),
                                    patch.mMeshName.c_str()
                                );
                            }
                        }
                    }
                }
            }
            desc->AddOverlays(*this);
        }
    }
}

void BandPatchMesh::Render(RndTex *tex, RndMat *mat) {
    for (int i = 0; i < mMeshes.size(); i++) {
        RndTex *outputtex = mMeshes[i].OutputTex();
        if (outputtex == tex) {
            for (int j = 0; j < mMeshes[i].patches.size(); j++) {
                BandPatchMesh::MeshPair::PatchPair &ppair = mMeshes[i].patches[j];
                RndMesh *patch = ppair.mPatch;
                if (patch) {
                    RndMat *patchmat = patch->Mat();
                    if (patchmat) {
                        mat->SetColor(patchmat->mColor);
                        mat->SetTexWrap(patchmat->GetTexWrap());
                        mat->SetBlend(patchmat->GetBlend());
                        mat->SetDiffuseTex(patchmat->GetDiffuseTex());
                    } else {
                        mat->SetColor(1, 1, 1);
                        mat->SetTexWrap(kTexBorderBlack);
                        mat->SetBlend(RndMat::kPreMultAlpha);
                        mat->SetDiffuseTex(mMeshes[i].patches[j].mTex);
                    }
                    Transform tf88;
                    tf88.Reset();
                    tf88.m.y *= (float)tex->Height() / (float)tex->Width();
                    patch->SetLocalXfm(tf88);
                    patch->SetMat(mat);
                    if (mat->GetDiffuseTex())
                        patch->DrawShowing();
                    patch->SetMat(patchmat);
                    patch->DirtyLocalXfm().Reset();
                }
            }
        }
    }
}

void BandPatchMesh::Compress(BandCharDesc *desc) {
    ObjectDir *pdir = desc->GetPatchDir();
    for (int i = 0; i < mMeshes.size(); i++) {
        for (int j = 0; j < mMeshes[i].patches.size(); j++) {
            RndMesh *patch = mMeshes[i].patches[j].mPatch;
            if (patch) {
                RndTex *tex = mMeshes[i].patches[j].mTex;
                if (tex && pdir && tex->Dir() == pdir) {
                    delete tex;
                }
                if (!patch->Dir())
                    delete patch;
            }
        }
    }
}

void BandPatchMesh::ListDrawChildren(std::list<RndDrawable *> &list) {
    if (mRenderTo) {
        for (int i = 0; i < mMeshes.size(); i++) {
            for (int j = 0; j < mMeshes[i].patches.size(); j++) {
                list.push_back(mMeshes[i].patches[j].mPatch);
            }
        }
    }
}

void BandPatchMesh::AddMappingPatch(BandPatchMesh::MeshPair &pair, RndMesh *mesh) {
    MILO_ASSERT(mRenderTo, 0x761);
    mesh->SetTransParent(0, false);
    mesh->CopyBones(0);
    mesh->SetHasAOCalc(false);
    pair.AddMappingPatch(mesh);
}

void BandPatchMesh::ConstructQuad(RndTex *tex) {
    MILO_ASSERT(mRenderTo, 0x76B);
    if (mMeshes.size() != 1) {
        MILO_WARN(
            "%s: Can't construct quad with %d meshes, must exactly 1",
            PathName(mMeshes.Owner()),
            mMeshes.size()
        );
    } else
        Construct(mMeshes[0], tex, true, false, 0);
}

void BandPatchMesh::Construct(
    BandPatchMesh::MeshPair &meshpair,
    RndTex *tex,
    bool quad,
    bool perm,
    BandPatchMesh::WorkVerts *wv
) {
    MILO_ASSERT(quad || wv, 0x77D);
    BandPatchMesh::MeshPair::PatchPair &patchpair = meshpair.AddPatch(perm);
    patchpair.mTex = tex;
    if (mRenderTo) {
        patchpair.mPatch->SetTransParent(0, false);
        patchpair.mPatch->CopyBones(0);
        patchpair.mPatch->SetHasAOCalc(false);
    } else {
        patchpair.mPatch->SetOrder(0.01f);
        patchpair.mPatch->CopyBones(meshpair.mesh);
        patchpair.mPatch->RndTransformable::Copy(meshpair.mesh, Hmx::Object::kCopyDeep);
        patchpair.mPatch->SetHasAOCalc(meshpair.mesh->HasAOCalc());
    }
    if (quad) {
        if (!mRenderTo)
            MILO_WARN("Generating quad patch for non render to!");
        patchpair.mPatch->Verts().resize(4, true);
        patchpair.mPatch->Faces().resize(2);
        for (int i = 0; i < 4; i++) {
            float x, y;
            if (i < 2)
                y = 1.0f;
            else
                y = 0;
            bool b13 = false;
            if (i == 1 || i == 2)
                b13 = true;
            if (b13)
                x = 1.0f;
            else
                x = 0;
            Vector2 v30(y, x);
            SetRenderToVert(patchpair.mPatch->Verts(i), v30, v30);
        }
        patchpair.mPatch->Faces()[0].Set(0, 1, 2);
        patchpair.mPatch->Faces()[1].Set(0, 2, 3);
    } else
        wv->SetVertsAndFaces(patchpair.mPatch, mRenderTo);
    patchpair.mPatch->Sync(0x13F);
    RndMeshDeform *deform = RndMeshDeform::FindDeform(patchpair.mPatch);
    delete deform;
    if (perm) {
        MakeString("Generated by OutfitConfig patch port to %s", meshpair.mesh->Name());
        if (!quad && !mRenderTo) {
            RndMeshDeform *df = RndMeshDeform::FindDeform(meshpair.mesh);
            if (df) {
                RndMeshDeform *newdef = Hmx::Object::New<RndMeshDeform>();
                RndMesh *patch = patchpair.mPatch;
                const char *deformname =
                    MakeString("%s.deform", FileGetBase(patch->Name(), 0));
                newdef->SetName(deformname, patch->Dir());
                newdef->Copy(df, Hmx::Object::kCopyDeep);
                newdef->SetMesh(patchpair.mPatch);
                wv->CopyDeformWeights(newdef, df);
            }
        }
    }
}

void BandPatchMesh::SetRenderToVert(
    RndMesh::Vert &vert, const Vector2 &pos, const Vector2 &uv
) {
    vert.uv = uv;
    vert.pos.Set((pos.x - 0.5f) * 2.0f, (pos.y - 0.5f) * 2.0f, 0);
    vert.norm.Set(0, 0, -1.0f);
    vert.boneWeights.Set(0, 0, 0, 0);
    vert.color.Clear();
}

BinStream &operator>>(BinStream &bs, BandPatchMesh::MeshPair &mp) {
    bs >> mp.mesh;
    return bs;
}

BinStream &operator>>(BinStream &bs, BandPatchMesh &mesh) {
    int rev;
    bs >> rev;
    BandPatchMesh::gRev = getHmxRev(rev);
    BandPatchMesh::gAltRev = getAltRev(rev);
#ifdef MILO_DEBUG
    if (BandPatchMesh::gRev > 4) {
        MILO_FAIL(
            "%s can't load new %s version %d > %d",
            PathName(mesh.mSrc.Owner()),
            "BandPatchMesh",
            BandPatchMesh::gRev,
            (unsigned short)4
        );
    }
    if (BandPatchMesh::gAltRev != 0) {
        MILO_FAIL(
            "%s can't load new %s alt version %d > %d",
            PathName(mesh.mSrc.Owner()),
            "BandPatchMesh",
            BandPatchMesh::gAltRev,
            (unsigned short)0
        );
    }
#endif
    bs >> mesh.mSrc;
    if (BandPatchMesh::gRev > 3)
        bs >> mesh.mMeshes;
    else {
        mesh.mMeshes.resize(1);
        bs >> mesh.mMeshes[0].mesh;
    }
    if (BandPatchMesh::gRev < 1) {
        Symbol s;
        bs >> s;
    }
    if (BandPatchMesh::gRev < 4) {
        Symbol s;
        bs >> s;
    }
    if (BandPatchMesh::gRev > 1) {
        if (BandPatchMesh::gRev > 2)
            bs >> mesh.mRenderTo;
        else {
            Symbol s;
            bs >> s;
            mesh.mRenderTo = !s.Null();
        }
    }
    if (BandPatchMesh::gRev > 3)
        bs >> mesh.mCategory;
    return bs;
}

BEGIN_CUSTOM_PROPSYNC(BandPatchMesh::MeshPair::PatchPair)
    SYNC_PROP(patch, o.mPatch)
    SYNC_PROP(tex, o.mTex)
END_CUSTOM_PROPSYNC

BEGIN_CUSTOM_PROPSYNC(BandPatchMesh::MeshPair)
    SYNC_PROP(mesh, o.mesh)
    SYNC_PROP(patches, o.patches)
END_CUSTOM_PROPSYNC

BEGIN_CUSTOM_PROPSYNC(BandPatchMesh)
    SYNC_PROP(meshes, o.mMeshes)
    SYNC_PROP(src, o.mSrc)
    SYNC_PROP(render_to, o.mRenderTo)
    SYNC_PROP(category, o.mCategory)
END_CUSTOM_PROPSYNC