#include "bandobj/BandPatchMesh.h"
#include "bandobj/BandCharDesc.h"
#include "math/Rot.h"
#include "utl/Symbols.h"
#include <algorithm>
#include <cmath>
#ifdef HX_NATIVE
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#endif

// MeshVert per-vert arena layout.
//
// Each WorkVerts::mMeshVerts[i] points into the `unkc` byte arena at a slot
// laid out as: [ MeshVert header ][ unsigned short faceList[valence] ].
// The face-incidence list lives immediately after the header (at the byte
// offset where the header ends), and the per-slot stride leaves room for the
// rounded-up valence.
//
// On the Wii (MWCC, 4-byte pointer) the header is exactly 0x32 bytes, the twin
// flag (MeshVert::unk27) sits at 0x27, and the slot stride base is 0x38. Those
// literals are baked all over this TU. On a native LP64 build the leading
// `const RndMesh::Vert *mVert` is 8 bytes, which shifts every field after it
// by +4 (unk27 -> 0x2b, the header end -> 0x36, struct size -> 0x38). Using the
// Wii literals on the host writes the trailing face-index list into the high
// halfword of MeshVert::unk2c (offset 0x30..0x33 on host), scribbling the twin
// cursor with a face index over the intact 0xFFFF low-half of its -1 sentinel
// -> later twin-list walk (SetMeshVertAndTwins:476) reads e.g. 0x0752FFFF and
// subscripts mMeshVerts[] out of bounds -> heap corruption / abort.
//
// The Wii path keeps the exact original literals (byte-identical match); only
// the HX_NATIVE path uses layout-derived offsets so the arena is laid out
// correctly for the host ABI.
#ifdef HX_NATIVE
// End of the MeshVert header == start of the trailing face-index array.
static const size_t kMVFaceList =
    offsetof(BandPatchMesh::MeshVert, unk30) + sizeof(unsigned short);
// MeshVert::unk27 twin flag byte offset.
static const size_t kMVTwinFlag = offsetof(BandPatchMesh::MeshVert, unk27);
// Per-slot stride base (faceList start + 6, matching the Wii 0x32+6=0x38).
static const size_t kMVSlotBase = kMVFaceList + 6;
#else
static const size_t kMVFaceList = 0x32;
static const size_t kMVTwinFlag = 0x27;
static const size_t kMVSlotBase = 0x38;
#endif

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
    float dot = Dot(mv->mVert->norm, v48);
    ScaleAddEq(v48, mv->mVert->norm, -dot);
    float v50y = mv->unk1c.y;
    float v50x = mv->unk1c.x;
    float v48x = v48.x;
    float v48y = v48.y;
    float v48z = v48.z;
    float newlensq = v48z * v48z + v48x * v48x + v48y * v48y;
    if (newlensq > 0) {
        float ratio = newlensq / lensq;
        float r = 1.0f / std::sqrt(ratio);
        float recipsq = 0.5f * r * (3.0f - ratio * r * r);
        float dot4 = v48x * mv->unk10.x + v48y * mv->unk10.y + v48z * mv->unk10.z;
        float vry = vr.y;
        float dot5 = v48x * mv->unk4.x + v48y * mv->unk4.y + v48z * mv->unk4.z;
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

struct SortByPointer {
    bool operator()(BandPatchMesh::MeshVert *v1, BandPatchMesh::MeshVert *v2) {
        return v1->mVert < v2->mVert;
    }
};

// Explicit specializations to avoid bool materialization in comparison loops,
// so CW uses blt/bge directly after fcmpo instead of mfcr/srwi./bne.
#ifndef HX_NATIVE
// V20 (salvage V33 re-apply): the explicit stlpmtx_std specializations are
// asm-match-only — clang's libstdc++ has no `__unguarded_partition` /
// `__introsort_loop` symbols, so these `template <>` definitions fail with
// "no function template matches". Same pattern as GameGemList.cpp. The
// std::sort calls below fall through to the host libstdc++.
namespace stlpmtx_std {

// --- SortByWorkVertZ specializations ---

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
void __adjust_heap<BandPatchMesh::MeshVert **, long, BandPatchMesh::MeshVert *, SortByWorkVertZ>(
    BandPatchMesh::MeshVert **__first,
    long __holeIndex,
    long __len,
    BandPatchMesh::MeshVert *__val,
    SortByWorkVertZ
) {
    long __topIndex = __holeIndex;
    long __secondChild = 2 * __holeIndex + 2;
    while (__secondChild < __len) {
        if ((*(__first + __secondChild))->mVert->pos.z
            < (*(__first + (__secondChild - 1)))->mVert->pos.z)
            __secondChild--;
        *(__first + __holeIndex) = *(__first + __secondChild);
        __holeIndex = __secondChild;
        __secondChild = 2 * (__secondChild + 1);
    }
    if (__secondChild == __len) {
        *(__first + __holeIndex) = *(__first + (__secondChild - 1));
        __holeIndex = __secondChild - 1;
    }
    // inline __push_heap with SortByWorkVertZ comparator
    long __parent = (__holeIndex - 1) / 2;
    while (
        __holeIndex > __topIndex
        && (*(__first + __parent))->mVert->pos.z < __val->mVert->pos.z
    ) {
        *(__first + __holeIndex) = *(__first + __parent);
        __holeIndex = __parent;
        __parent = (__holeIndex - 1) / 2;
    }
    *(__first + __holeIndex) = __val;
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
#endif // !HX_NATIVE

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
#ifdef HX_NATIVE
    if (getenv("RB3_PP_PROBE")) {
        int maxIdx = -1;
        for (int i = 0; i < (int)mMesh->Faces().size(); i++) {
            RndMesh::Face &f = mMesh->Faces()[i];
            for (int j = 0; j < 3; j++)
                if ((int)f[j] > maxIdx)
                    maxIdx = (int)f[j];
        }
        fprintf(
            stderr,
            "[SMV-ENTRY] mesh=%s verts=%d faces=%d maxFaceIdx=%d oob=%d\n",
            mMesh->Name(), (int)mMesh->Verts().size(),
            (int)mMesh->Faces().size(), maxIdx,
            maxIdx >= (int)mMesh->Verts().size() ? 1 : 0
        );
    }
#endif
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
#ifdef HX_NATIVE
            // Defensive backstop (native only): if a face references a vertex
            // index past the mesh's vert count (a Faces()/Verts() desync from a
            // bad decode/merge), the original `mMeshVerts[curface[j]]` write
            // would corrupt the heap. Skip the out-of-range index instead. A
            // no-op on a well-formed mesh; HX_NATIVE-only so the Wii match is
            // byte-identical (the #else keeps the exact original expression).
            if ((unsigned)(int)curface[j] >= (unsigned)mMeshVerts.size()) {
                MILO_WARN(
                    "BandPatchMesh::SetMeshVerts: face %d vert idx %d >= "
                    "Verts()=%d on %s — skipping",
                    i, (int)curface[j], (int)mMeshVerts.size(), mMesh->Name()
                );
                continue;
            }
#endif
            ((int &)mMeshVerts[curface[j]])++;
        }
    }
    int count = 0;
    for (int i = 0; i < mMeshVerts.size(); i++) {
        int c = (int)mMeshVerts[i];
        mMeshVerts[i] = (MeshVert *)count;
        count += (((c + 1) & ~1) - 2) * 2 + kMVSlotBase;
    }
    unkc = new char[count];
    for (int i = 0; i < mMeshVerts.size(); i++) {
        mMeshVerts[i] = (MeshVert *)((char *)unkc + (int)mMeshVerts[i]);
        MeshVert *v = mMeshVerts[i];
        *((unsigned char *)v + kMVTwinFlag) = 0;
        v->unk28 = -1;
        v->unk2c = -1;
        v->unk30 = 0;
        v->mVert = 0;
        v->unk24 = 0;
    }
    for (int i = 0; i < mMesh->Faces().size(); i++) {
        RndMesh::Face &curface = mMesh->Faces()[i];
        for (int j = 0; j < 3; j++) {
#ifdef HX_NATIVE
            // Backstop, same rationale as the valence loop above: skip any
            // out-of-range face vertex index so the face-list write can never
            // overrun the arena. HX_NATIVE-only; Wii match byte-identical.
            if ((unsigned)(int)curface[j] >= (unsigned)mMeshVerts.size())
                continue;
#endif
            MeshVert *mv = mMeshVerts[curface[j]];
            int n = mv->unk30;
            ((unsigned short *)((char *)mv + kMVFaceList))[n] = i;
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
                *((unsigned char *)mMeshVerts[vi2] + kMVTwinFlag) = 1;
                mMeshVerts[prev]->unk2c = vi2;
                prev = vi2;
            }
            if (prev != vi) {
                *((unsigned char *)mMeshVerts[vi] + kMVTwinFlag) = 1;
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
    int mv1Idx = mv1->unk28;
    for (int idx = mv0->unk28; idx != -1; idx = mMeshVerts[idx]->unk2c) {
        MeshVert *mv = mMeshVerts[idx];
        unsigned short *faceidxptr = (unsigned short *)((char *)mv + kMVFaceList);
        for (int i = 0; i < mv->unk30; i++) {
            int faceidx = faceidxptr[i];
            if (unk28[faceidx].mFlags == -1) {
                RndMesh *mesh = mMesh;
                RndMesh::Face &face = mesh->Faces()[faceidx];
                for (int b = 0; b < 3; b++) {
                    if (idx == face[b]
                        && mv1Idx == mMeshVerts[face[(b + 1) % 3]]->unk28) {
                        TryAddFace(faceidx, b);
                        break;
                    }
                }
            }
        }
    }
}

// TU-local enum so MILO_ASSERT stringifies as "vf != MeshFace::kFinished" etc.
// without editing the header. The original BandPatchMesh::MeshFace class has
// no enum members, but the target binary's string pool references named
// constants — using a #define alias lets us preserve the runtime behavior
// (b != 4 etc.) while emitting the longer pool strings the target binary has.
namespace {
    struct MeshFace_local {
        enum { kUnAdded = -1, kDontTestMonotonicity = 3, kFinished = 4 };
    };
}

int BandPatchMesh::WorkVerts::TryAddFace(int faceidx, int b) {
    unk28[faceidx].mFlags = b;
    unk20.push_back(faceidx);
#define vf b
#define MeshFace MeshFace_local
    MILO_ASSERT(vf != MeshFace::kFinished, 0x2A1);
    MILO_ASSERT(vf != MeshFace::kUnAdded, 0x2A2);
#undef MeshFace
#undef vf
    int prevVertCount = unk10.size();
    RndMesh::Face &face = mMesh->Faces()[faceidx];
    MeshVert *verts[3];
    int allOut = 0xf;
    for (int i = 0.0f; i < 3; i++) {
        int fi = face[i];
        verts[i] = mMeshVerts[fi];
        if (mMeshVerts[fi]->mVert == 0) {
#define vf b
#define MeshFace MeshFace_local
            MILO_ASSERT(vf != MeshFace::kDontTestMonotonicity, 0x2B1);
#undef MeshFace
#undef vf
            AddMeshVertAndTwins(fi, mMeshVerts[face[b]]);
        }
        allOut &= verts[i]->unk26;
    }
    int reject = (allOut != 0) ? 1 : 0;
    if (reject == 0) {
        MeshVert temp;
        temp.SetVert(verts[0]->mVert);
        reject = 0;
        Vector2 v(temp.unk1c);
        int _tmp0 = temp.AddUV(verts[1], unk34, &v);
        if (_tmp0 == 0
            || temp.AddUV(verts[2], unk34, &v) == 0)
            reject = 1;
    }
    if (reject == 0 && b != 3) {
        int prev = (b == 0) ? 2 : b - 1;
        int next = (b == 2) ? 0 : b + 1;
        MeshVert *vn = verts[next];
        MeshVert *vp = verts[prev];
        float ey = vn->unk1c.y - verts[b]->unk1c.y;
        float py = vp->unk1c.y - verts[b]->unk1c.y;
        float ex = vn->unk1c.x - verts[b]->unk1c.x;
        float px = vp->unk1c.x - verts[b]->unk1c.x;
        float t = (ex * px + ey * py) / (ey * ey + ex * ex);
        if (t > 1.0f)
            t = 1.0f;
        else if (t < 0)
            t = 0;
        float projy = verts[b]->unk1c.y + t * ey;
        float projx = verts[b]->unk1c.x + t * ex;
        float dot = (vp->unk1c.y - projy) * (vp->unk1c.y - 0.5f)
            + (vp->unk1c.x - projx) * (vp->unk1c.x - 0.5f);
        reject = (dot < 0) ? 1 : 0;
    }
    if (reject != 0) {
        int added = unk10.size() - prevVertCount;
        for (; added != 0; added--) {
            unk10[unk10.size() - 1]->mVert = 0;
            unk10.pop_back();
        }
        unk20.pop_back();
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
        RndMesh::Face &curface = mMesh->Faces()[idxptr[kMVFaceList / 2]];
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

// SortByPointer specializations in a second namespace block so that the struct
// is defined at its original source location, preserving IPA register decisions
// for the already-100% SortByWorkVertZ functions above.
#ifndef HX_NATIVE
namespace stlpmtx_std {

template <>
void __unguarded_linear_insert<BandPatchMesh::MeshVert **, BandPatchMesh::MeshVert *, SortByPointer>(
    BandPatchMesh::MeshVert **__last,
    BandPatchMesh::MeshVert *__val,
    SortByPointer
) {
    BandPatchMesh::MeshVert **__next = __last;
    --__next;
    while (__val->mVert < (*__next)->mVert) {
        BandPatchMesh::MeshVert *__tmp = *__next;
        *__last = __tmp;
        __last = __next;
        --__next;
    }
    *__last = __val;
}

template <>
void __adjust_heap<BandPatchMesh::MeshVert **, long, BandPatchMesh::MeshVert *, SortByPointer>(
    BandPatchMesh::MeshVert **__first,
    long __holeIndex,
    long __len,
    BandPatchMesh::MeshVert *__val,
    SortByPointer
) {
    long __topIndex = __holeIndex;
    long __secondChild = 2 * __holeIndex + 2;
    while (__secondChild < __len) {
        if ((*(__first + __secondChild))->mVert < (*(__first + (__secondChild - 1)))->mVert)
            __secondChild--;
        *(__first + __holeIndex) = *(__first + __secondChild);
        __holeIndex = __secondChild;
        __secondChild = 2 * (__secondChild + 1);
    }
    if (__secondChild == __len) {
        *(__first + __holeIndex) = *(__first + (__secondChild - 1));
        __holeIndex = __secondChild - 1;
    }
    // inline __push_heap with SortByPointer comparator
    long __parent = (__holeIndex - 1) / 2;
    while (
        __holeIndex > __topIndex
        && (*(__first + __parent))->mVert < __val->mVert
    ) {
        *(__first + __holeIndex) = *(__first + __parent);
        __holeIndex = __parent;
        __parent = (__holeIndex - 1) / 2;
    }
    *(__first + __holeIndex) = __val;
}

} // namespace stlpmtx_std
#endif // !HX_NATIVE

void BandPatchMesh::WorkVerts::SetVertsAndFaces(RndMesh *mesh, bool b) {
    std::sort(unk10.begin(), unk10.end(), SortByPointer());
    for (int i = 0.0f; i < unk10.size(); i++) {
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
            ExtendTwin(cur, v40, v48);
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

void BandPatchMesh::WorkVerts::ExtendTwin(
    const BandPatchMesh::MeshVert *mv, Vector2 &outDir, Vector2 &outUv
) {
    if (mv->unk27 == 0)
        return;
    float accumX = 0.0f;
    float accumY = 0.0f;
    const MeshVert *anchor = mv;
    const MeshVert *prevTwin = mv;
    const MeshVert *prevOther = mv;
    unsigned short *facePtr = (unsigned short *)((char *)mv + kMVFaceList);
    for (int i = 0; i < mv->unk30; i++) {
        unsigned short faceIdx = facePtr[i];
        if (unk28[faceIdx].mFlags == 4) {
            const RndMesh::Face &face = mMesh->Faces()[faceIdx];
            MeshVert *v0 = mMeshVerts[face.v2];
            MeshVert *next = mMeshVerts[face.v3];
            unsigned short *vptr = (unsigned short *)&face;
            for (int j = 0; j < 3; j++) {
                MeshVert *curr = mMeshVerts[vptr[j]];
                if (next == mv) {
                    if (curr->unk27 != 0) {
                        float dx = (curr->mVert->uv.x - next->mVert->uv.x) * unk44.x;
                        float dy = (curr->mVert->uv.y - next->mVert->uv.y) * unk44.y;
                        float len = std::sqrt(dx * dx + dy * dy);
                        float inv = 1.0f / len;
                        accumY = dy;
                        accumX = dx;
                        outDir.x += dx * inv;
                        outDir.y += dy * inv;
                        prevTwin = next;
                        prevOther = v0;
                        anchor = curr;
                    }
                } else if (curr == mv) {
                    if (next->unk27 != 0) {
                        float dx = (curr->mVert->uv.x - next->mVert->uv.x) * unk44.x;
                        float dy = (curr->mVert->uv.y - next->mVert->uv.y) * unk44.y;
                        float len = std::sqrt(dx * dx + dy * dy);
                        float inv = 1.0f / len;
                        accumY = dy;
                        accumX = dx;
                        outDir.x += dx * inv;
                        outDir.y += dy * inv;
                        prevTwin = next;
                        prevOther = v0;
                        anchor = next;
                    }
                }
                v0 = next;
                next = curr;
            }
        }
    }
    if (prevTwin == prevOther)
        return;
    float dyOther = prevTwin->mVert->uv.y - prevOther->mVert->uv.y;
    float cross = accumX * dyOther - accumY * (prevTwin->mVert->uv.x - prevOther->mVert->uv.x);
    float sign;
    if (cross >= 0.0f)
        sign = 1.0f;
    else
        sign = -1.0f;
    float ox = outDir.x;
    float oy = outDir.y;
    float invLen = sign * (1.0f / std::sqrt(ox * ox + oy * oy));
    float newY = ox * invLen * unk4c.y;
    float newX = -oy * invLen * unk4c.x;
    outDir.y = newY;
    outDir.x = newX;
    float ax = mv->mVert->uv.y - prevOther->mVert->uv.y;
    float bx = mv->mVert->uv.y - anchor->mVert->uv.y;
    float ay = mv->mVert->uv.x - prevOther->mVert->uv.x;
    float by = mv->mVert->uv.x - anchor->mVert->uv.x;
    float det = ay * bx - ax * by;
    float m00;
    float m01;
    float m10;
    float m11;
    int ok;
    float absDet = fabs(det);
    if (absDet < 1e-15f) {
        ok = 0;
    } else {
        float invDet = 1.0f / det;
        m00 = ay * invDet;
        m10 = -by * invDet;
        m11 = -ax * invDet;
        m01 = bx * invDet;
        ok = 1;
    }
    if (ok) {
        float p = outDir.x * m11 + outDir.y * m00;
        float q = outDir.x * m01 + outDir.y * m10;
        float av = mv->unk1c.y - anchor->unk1c.y;
        float tv = mv->unk1c.y - prevOther->unk1c.y;
        float au = mv->unk1c.x - anchor->unk1c.x;
        float tu = mv->unk1c.x - prevOther->unk1c.x;
        outUv.y = p * av + q * tv;
        outUv.x = p * au + q * tu;
    }
}

bool BandPatchMesh::WorkVerts::SetSameVerts(BandPatchMesh::WorkVerts *other) {
    int start = 0;
    int end = 0;
    for (int i = 0; i < other->unk10.size(); i++) {
        MeshVert *mv = other->unk10[i];
        int otherIdx = mv->mVert - &other->mMesh->Verts(0);
        if (mv->unk28 == otherIdx) {
            float mvz = mv->mVert->pos.z;
            float lo = mvz - 0.1f;
            float hi = mvz + 0.1f;
            int size = unk18.size();
            while ((unsigned int)start < size && unk18[start]->pos.z < lo)
                start++;
            if (end < start)
                end = start;
            while ((unsigned int)end < size && unk18[end]->pos.z < hi)
                end++;
            for (int k = start; k < end; k++) {
                RndMesh::Vert *v = unk18[k];
                float dx = mv->mVert->pos.x - v->pos.x;
                float dy = mv->mVert->pos.y - v->pos.y;
                float dz = mvz - v->pos.z;
                if (dx * dx + dy * dy + dz * dz < 0.01f) {
                    mv->unk27 = 1;
                    if (mMeshVerts.empty()) {
                        SetMeshVerts();
                    }
                    int idx = unk18[k] - &mMesh->Verts(0);
                    SetMeshVertAndTwins(idx, mv);
                    mMeshVerts[idx]->unk27 = 1;
                    break;
                }
            }
        }
    }
    int n10 = unk10.size();
    for (int i = 0; i < n10; i++) {
        MeshVert *mv = unk10[i];
        int vIdx = mv->mVert - &mMesh->Verts(0);
        MeshVert *faceIter = mv;
        for (int j = 0; j < mv->unk30; j++, faceIter = (MeshVert *)((char *)faceIter + 2)) {
            unsigned short faceIdx = *(unsigned short *)((char *)faceIter + kMVFaceList);
            RndMesh::Face &face = mMesh->Faces()[faceIdx];
            unsigned short prev = face.v3;
            for (int z = 0; z <= 2; z++) {
                if ((int)face[z] == vIdx) {
                    MeshVert *partner = mMeshVerts[prev];
                    if (partner->mVert) {
                        AddEdge(partner, mv);
                    }
                    break;
                }
                prev = face[z];
            }
        }
    }
                return !(unk10.empty());
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

void BandPatchMesh::ProjectPatches(const Transform &xfm, RndTex *tex, bool perm) {
    Vector2 scale(1.0f / Length(xfm.m.x), -0.5f / Length(xfm.m.y));
    Segment seg;
    seg.start = xfm.v;
    seg.end.x = xfm.m.z.x * -100.0f + xfm.v.x;
    seg.end.y = xfm.m.z.y * -100.0f + xfm.v.y;
    seg.end.z = xfm.m.z.z * -100.0f + xfm.v.z;
    MILO_ASSERT(64 > mMeshes.size(), 0x60A);
    unsigned int meshCount = mMeshes.size();
    int meshIndices[64];
    {
        int *p = meshIndices;
        int n = 0;
        for (unsigned int k = meshCount; (unsigned long)(int)k >= 1; k--) {
            *p++ = n++;
        }
    }
    RndMesh::sRawCollide = true;
    int hitMeshIdx = -1;
    int hitFaceIdx = 0;
    for (int i = 0; i < mMeshes.size(); i++) {
        RndMesh *mesh = mMeshes[i].mesh;
        if (mesh && !mesh->GetKeepMeshData()) {
            MILO_WARN(
                "%s patch trying to collide against mesh with no keep_mesh_data",
                PathName(mesh)
            );
        }
        if (mesh) {
            float t;
            Plane plane;
            if (mesh->CollideShowing(seg, t, plane)) {
                hitMeshIdx = i;
                hitFaceIdx = RndMesh::sLastCollide;
                if (t == 0.0f) {
                    seg.end = seg.start;
                } else if (t != 1.0f) {
                    seg.end.x = t * (seg.end.x - seg.start.x) + seg.start.x;
                    seg.end.y = t * (seg.end.y - seg.start.y) + seg.start.y;
                    seg.end.z = t * (seg.end.z - seg.start.z) + seg.start.z;
                }
            }
        }
    }
    RndMesh::sRawCollide = false;
    if (hitMeshIdx == -1)
        return;
    meshCount--;
    int tmpIdx = meshIndices[meshCount];
    meshIndices[meshCount] = meshIndices[hitMeshIdx];
    meshIndices[hitMeshIdx] = tmpIdx;
    MeshPair *hitPair = &mMeshes[hitMeshIdx];
    WorkVerts *firstWV = new WorkVerts(hitPair->mesh, scale);
    firstWV->SetMeshVerts();
    RndMesh::Vert seedVert;
    MeshVert seedMV;
    seedVert.pos = seg.end;
    seedVert.norm.x = xfm.m.z.x;
    seedVert.norm.y = xfm.m.z.y;
    seedVert.norm.z = xfm.m.z.z;
    seedVert.uv.Set(0.5f, 0.5f);
    seedMV.SetVert(&seedVert);
    seedMV.unk4 = xfm.m.x;
    seedMV.unk10 = xfm.m.y;
    seedMV.unk1c.Set(0.5f, 0.5f);
    seedMV.Normalize(1);
    firstWV->AddFace(hitFaceIdx, &seedMV);
    firstWV->Project();
    firstWV->SortWorkVertsByZ();
    MeshPair *meshPairs[64];
    WorkVerts *workVerts[64];
    meshPairs[0] = hitPair;
    workVerts[0] = firstWV;
    int j = 0;
    int wvCount = 1;
    while (meshCount > j) {
        MeshPair *cur = &mMeshes[meshIndices[j]];
        if (cur->mesh != NULL) {
            WorkVerts *wv = new WorkVerts(cur->mesh, scale);
            int k = 0;
            while (k < wvCount) {
                if (wv->SetSameVerts(workVerts[k])) {
                    wv->Project();
                    wv->SortWorkVertsByZ();
                    meshCount--;
                    workVerts[wvCount] = wv;
                    wvCount++;
                    meshPairs[wvCount - 1] = cur;
                    int swap = meshIndices[meshCount];
                    meshIndices[j] = swap;
                    j--;
                    break;
                }
                k++;
            }
            if (wv->mMeshVerts.empty()) {
                delete wv;
            }
        }
        j++;
    }
    for (int i = 0; i < wvCount; i++) {
        Construct(*meshPairs[i], tex, false, perm, workVerts[i]);
        delete workVerts[i];
    }
}

bool BandPatchMesh::FindXfm(RndMesh *mesh, const Vector2 &uv, Transform &xfm) {
    if (mesh->Verts().size() == 0 || mesh->Faces().size() == 0) {
        TheDebug.Notify(
            FormatString("Patches can't project onto %s, has no verts or faces!").Str()
        );
        return false;
    }
    unsigned short *foundFace = (unsigned short *)&mesh->Faces()[0] + mesh->Faces().size() * 3;
    unsigned short *faceIter = (unsigned short *)&mesh->Faces()[0];
    {
        float zero = 0.0f;
        while (faceIter != (unsigned short *)&mesh->Faces()[0] + mesh->Faces().size() * 3) {
            unsigned short lastIdx = faceIter[2];
            RndMesh::Vert *v0 = &mesh->Verts(lastIdx);
            int matched = 0;
            float firstSign = 0.0f;
            unsigned short *iter = faceIter;
            do {
                unsigned short curIdx = iter[0];
                RndMesh::Vert *v1 = &mesh->Verts(curIdx);
                float dx = uv.x - v0->uv.x;
                float dy = uv.y - v0->uv.y;
                float ex = v1->uv.x - v0->uv.x;
                float ey = v1->uv.y - v0->uv.y;
                float cross = dx * ey - dy * ex;
                if (zero == firstSign) {
                    firstSign = cross;
                }
                if (cross * firstSign < zero)
                    break;
                matched++;
                v0 = v1;
                iter++;
            } while (matched < 3);
            if (matched == 3) {
                foundFace = faceIter;
                break;
            }
            faceIter += 3;
        }
    }
    if (foundFace == (unsigned short *)&mesh->Faces()[0] + mesh->Faces().size() * 3) {
        float minDistSq = 1e30f;
        unsigned short *facePtr = (unsigned short *)&mesh->Faces()[0];
        while (facePtr != (unsigned short *)&mesh->Faces()[0] + mesh->Faces().size() * 3) {
            unsigned short lastIdx = facePtr[2];
            RndMesh::Vert *v0 = &mesh->Verts(lastIdx);
            unsigned short *iter = facePtr;
            int j = 0;
            do {
                unsigned short curIdx = iter[0];
                RndMesh::Vert *v1 = &mesh->Verts(curIdx);
                float ex = v1->uv.x - v0->uv.x;
                float ey = v1->uv.y - v0->uv.y;
                float px = uv.x - v0->uv.x;
                float py = uv.y - v0->uv.y;
                float t = (px * ex + py * ey) / (ex * ex + ey * ey);
                if (t < 0.0f)
                    t = 0.0f;
                else if (t > 1.0f)
                    t = 1.0f;
                float closestX = v0->uv.x + ex * t;
                float closestY = v0->uv.y + ey * t;
                float ddx = closestX - uv.x;
                float ddy = closestY - uv.y;
                float distSq = ddx * ddx + ddy * ddy;
                int better;
                if (distSq < minDistSq) {
                    minDistSq = distSq;
                    better = 1;
                } else {
                    better = 0;
                }
                if (better) {
                    foundFace = facePtr;
                }
                v0 = v1;
                iter++;
                j++;
            } while (j < 3);
            facePtr += 3;
        }
    }
    RndMesh::Vert *triVerts[3];
    for (int i = 0; i < 3; i++) {
        triVerts[i] = &mesh->Verts(foundFace[i]);
    }
    Hmx::Matrix3 uvMat;
    uvMat.x.y = triVerts[0]->uv.y;
    uvMat.x.x = triVerts[0]->uv.x;
    uvMat.x.z = 1.0f;
    uvMat.y.x = triVerts[1]->uv.x;
    uvMat.y.y = triVerts[1]->uv.y;
    uvMat.y.z = 1.0f;
    uvMat.z.x = triVerts[2]->uv.x;
    uvMat.z.y = triVerts[2]->uv.y;
    uvMat.z.z = 1.0f;
    Hmx::Matrix3 posMat;
    posMat.x = triVerts[0]->pos;
    posMat.y = triVerts[1]->pos;
    posMat.z = triVerts[2]->pos;
    Hmx::Matrix3 normMat;
    normMat.x = triVerts[0]->norm;
    normMat.y = triVerts[1]->norm;
    normMat.z = triVerts[2]->norm;
    Invert(uvMat, uvMat);
    Hmx::Matrix3 posOut;
    Multiply(uvMat, posMat, posOut);
    Hmx::Matrix3 normOut;
    Multiply(uvMat, normMat, normOut);
    float invRowX = 1.0f / Length((posOut.x));
    float invRowY = 1.0f / Length((posOut.y));
    RndMesh::Vert centerVert;
    centerVert.pos.Set(uv.x, uv.y, 1.0f);
    centerVert.norm.Set(0, 0, 0);
    Multiply(posMat, centerVert.pos, centerVert.pos);
    Multiply(normMat, centerVert.pos, centerVert.norm);
    MeshVert centerMV;
    centerMV.SetVert(&centerVert);
    centerMV.unk4 = posOut.x;
    centerMV.unk10 = posOut.y;
    centerMV.unk4.x *= -1.0f;
    centerMV.unk4.y *= -1.0f;
    centerMV.unk4.z *= -1.0f;
    centerMV.Normalize(1);
    float scaleX = 0.5f * invRowX;
    float scaleY = 0.5f * invRowY;
    xfm.m.x.x = centerMV.unk4.x * scaleX;
    xfm.m.x.y = centerMV.unk4.y * scaleX;
    xfm.m.x.z = centerMV.unk4.z * scaleX;
    xfm.m.y.x = centerMV.unk10.x * scaleY;
    xfm.m.y.y = centerMV.unk10.y * scaleY;
    xfm.m.y.z = centerMV.unk10.z * scaleY;
    return true;
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
            float y = (i < 2) ? 1.0f : 0.0f;
            float x = (i == 1 || i == 2) ? 1.0f : 0.0f;
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