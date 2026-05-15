#include "Mesh.h"
#include "decomp.h"
#include "obj/Data.h"
#include "obj/DataFunc.h"
#include "obj/ObjMacros.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "revolution/GX.h"
#include "revolution/mtx/mtx.h"
#include "revolution/os/OSCache.h"
#include "rndobj/Cam.h"
#include "rndwii/Cam.h"
#include "rndwii/Rnd.h"
#include "utl/MemMgr.h"
#include "utl/Std.h"
#include <cstddef>
#include <cstring>

extern "C" {
    void PSVECSubtract(const Vec *a, const Vec *b, Vec *out);
    void C_VECReflect(const Vec *in, const Vec *norm, Vec *out);
}

#define kTempSize 0x40000

int NUM_BUFFERS = 4;
bool gToggleAO = true;
void *DisplayList::sTemp;
void *DisplayList::sCurr;
void *gBoneTransformCache;

static DataNode OnToggleAO(DataArray *) {
    gToggleAO = !gToggleAO;
    TheDebug << MakeString("Ambient Occlusion is now %s!\n", gToggleAO ? "ON" : "OFF");
    return 0;
}

DisplayList::DisplayList() : mData(NULL), mSize(0), unk_0x8(0) {}

DisplayList::~DisplayList() { Clear(); }

void DisplayList::Init() {
    if (sTemp == NULL) {
        static int _x = MemFindHeap("main");
        MemPushHeap(_x);
        sTemp = _MemAlloc(0x40000, 0x20);
        MemPopHeap();
    }
}

void DisplayList::Clear() {
    WiiRnd::SyncFree(mData);
    mData = NULL;
    mSize = 0;
    unk_0x8 = 0;
}

void DisplayList::Copy(const DisplayList &d) {
    Clear();
    mSize = d.mSize;
    unk_0x8 = d.unk_0x8;

    static int _x = MemFindHeap("main");
    MemPushHeap(_x);
    mData = _MemAlloc(mSize, 0x20);
    MemPopHeap();
    DCZeroRange(mData, mSize);
    memcpy(mData, d.mData, mSize);
    DCStoreRange(mData, mSize);
}

void DisplayList::Begin(unsigned short us) {
    MILO_ASSERT(!sCurr, 147);
    sCurr = sTemp;
    Clear();
    unk_0x8 = us;
}

void DisplayList::Begin(
    _GXPrimitive prim, _GXVtxFmt f, unsigned short us1, unsigned short us2
) {
    Begin(us2);
    Start(prim, f, us1);
}

void DisplayList::Start(_GXPrimitive pr, _GXVtxFmt f, unsigned short us) {
    *(u8 *)sCurr = (pr | f);
    sCurr = (void *)((u8 *)sCurr + 1);
    *(u16 *)sCurr = us;
    sCurr = (void *)((u8 *)sCurr + 2);
}

void DisplayList::End() {
    MILO_ASSERT(sCurr, 174);
    MILO_ASSERT(!mSize && !mData, 175);
    static int _x = MemFindHeap("main");
    MemPushHeap(_x);
    u32 tmp_a = ((u32)sTemp - (u32)sCurr);
    u32 tmp_b = (tmp_a + 31) & 0xFFFFFFE0;
    mSize = tmp_b;
    mData = _MemAlloc(tmp_b, 0x20);
    DCZeroRange(mData, mSize);
    memcpy(mData, sTemp, tmp_a);
    memset((void *)((u32)mData + tmp_a), 0, mSize - tmp_a);
    DCStoreRange(mData, mSize);
    sCurr = NULL; // memleak? mem is alloc'd but not freed
    MemPopHeap();
}

DisplayList &DisplayList::operator<<(unsigned short us) {
    MILO_ASSERT(sCurr, 202);
    if (unk_0x8 < 0x100) {
        u8 *test = (u8 *)((u32)sCurr + 4);
        us &= 0xFF;
        sCurr = test;
#ifdef MILO_DEBUG
        (((u32)sCurr < (u32)sTemp + kTempSize)
         || (TheDebugFailer
                 << (MakeString(kAssertStr, "Mesh.cpp", 210, "sCurr < sTemp + kTempSize")
                    ),
             0));
#endif
        test[3] = us;
        test[2] = us;
        test[1] = us;
        test[0] = us;
    } else {
        u16 *test = (u16 *)((u32)sCurr + 8);
        sCurr = test;
#ifdef MILO_DEBUG
        (((u32)sCurr < (u32)sTemp + kTempSize)
         || (TheDebugFailer
                 << (MakeString(kAssertStr, "Mesh.cpp", 219, "sCurr < sTemp + kTempSize")
                    ),
             0));
#endif
        test[3] = us;
        test[2] = us;
        test[1] = us;
        test[0] = us;
    }
    return *this;
}

void DisplayList::Draw(u32, _GXVtxFmt) const {
    MILO_ASSERT(mData, 237);
    MILO_ASSERT(mSize > 0, 238);
    DCStoreRange(mData, mSize);
    GXCallDisplayList(mData, mSize);
}

WiiMesh::WiiMesh()
    : mCTVtxs(nullptr), mPosNrmVtxs(nullptr), mPosQ(nullptr), mNrmQ(nullptr),
      mBoneWeights(nullptr), mBoneIndices(nullptr), mNumVerts(0), mNumFaces(0),
      unk_0x164(0), bitmask_0(0), bitmask_1(1), bitmask_2(0), unk_0x168(-1) {
    unk_0x150 = nullptr;
}

BEGIN_COPYS(WiiMesh)
    COPY_SUPERCLASS(RndMesh)
    CREATE_COPY(WiiMesh)
    if (c && mGeomOwner == this && ty == kCopyDeep && mMutable == 0) {
        if (mKeepMeshData) return;
        mNumVerts = c->mNumVerts;
        unk_0x164 = c->unk_0x164;
        mNumFaces = c->mNumFaces;
        CreateBuffers();
        if (mNumVerts != 0) {
            int factor;
            if (((WiiMesh *)(RndMesh *)mGeomOwner)->bitmask_2
                && ((WiiMesh *)(RndMesh *)mGeomOwner)->bitmask_1) {
                factor = 9;
            } else {
                factor = 16;
                if (((WiiMesh *)(RndMesh *)mGeomOwner)->bitmask_1)
                    factor = 10;
            }
            memcpy(mCTVtxs, c->mCTVtxs, mNumVerts * 8);
            if (mPosNrmVtxs)
                memcpy(mPosNrmVtxs, c->mPosNrmVtxs, mNumVerts * factor);
            if (mPosQ)
                memcpy(mPosQ, c->mPosQ, mNumVerts * 6);
            if (mNrmQ)
                memcpy(mNrmQ, c->mNrmQ, mNumVerts * 3);
            if (unk_0x164 > 1) {
                memcpy(
                    mBoneWeights,
                    c->mBoneWeights,
                    (mNumVerts * Min((int)unk_0x164, 4)) << 1
                );
            }
            if (unk_0x164 > 4) {
                memcpy(mBoneIndices, c->mBoneIndices, mNumVerts << 2);
            } else if (unk_0x164 > 1) {
                memcpy(mBoneIndices, c->mBoneIndices, unk_0x164);
            }
        }
        if (mNumFaces != 0)
            mDisplays.Copy(c->mDisplays);
    }
END_COPYS

int WiiMesh::GetSomeSizeFactor() {
    int ret;
    WiiMesh *own = (WiiMesh *)GeomOwner();
    if (own->bitmask_2 && own->bitmask_1) {
        ret = 9;
    } else {
        ret = 16;
        if (own->bitmask_1)
            ret = 10;
    }
    return ret;
}

void WiiMesh::Init() {
    Register();
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, (GXCompType)4, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_NRM, GX_NRM_XYZ, (GXCompType)1, 6);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, (GXCompType)5, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, (GXCompType)3, 9);
    GXSetVtxAttrFmt(GX_VTXFMT1, GX_VA_POS, GX_POS_XYZ, (GXCompType)4, 0);
    GXSetVtxAttrFmt(GX_VTXFMT1, GX_VA_NBT, GX_NRM_NBT, (GXCompType)4, 0);
    GXSetVtxAttrFmt(GX_VTXFMT1, GX_VA_CLR0, GX_CLR_RGBA, (GXCompType)5, 0);
    GXSetVtxAttrFmt(GX_VTXFMT1, GX_VA_TEX0, GX_TEX_ST, (GXCompType)3, 9);
    GXSetVtxAttrFmt(GX_VTXFMT4, GX_VA_POS, GX_POS_XYZ, (GXCompType)3, 5);
    GXSetVtxAttrFmt(GX_VTXFMT4, GX_VA_NRM, GX_NRM_XYZ, (GXCompType)1, 6);
    GXSetVtxAttrFmt(GX_VTXFMT4, GX_VA_CLR0, GX_CLR_RGBA, (GXCompType)5, 0);
    GXSetVtxAttrFmt(GX_VTXFMT4, GX_VA_TEX0, GX_TEX_ST, (GXCompType)3, 9);

    DisplayList::Init();
    gBoneTransformCache = (void *)0xe0000000;
    MaxBones();
    DataRegisterFunc("toggle_ao", OnToggleAO);
}

void WiiMesh::ReleaseBuffers() {
    WiiRnd::SyncFree(mBoneWeights);
    WiiRnd::SyncFree(mBoneIndices);
    WiiRnd::SyncFree(mCTVtxs);
    WiiRnd::SyncFree(mPosNrmVtxs);
    WiiRnd::SyncFree(mPosQ);
    WiiRnd::SyncFree(mNrmQ);
    WiiRnd::SyncFree(unk_0x150);
    mCTVtxs = NULL;
    mPosNrmVtxs = NULL;
    mPosQ = NULL;
    mNrmQ = NULL;
    mBoneWeights = NULL;
    mBoneIndices = NULL;
    unk_0x150 = NULL;
}

void *SkinAlloc(int size, char *, int align) {
    static int fastHeapNum = MemFindHeap("fast");
    int a, b, c, d;
    MemFreeBlockStats(fastHeapNum, a, b, c, d);
    if (d > size) {
        static int _x = MemFindHeap("fast");
        MemPushHeap(_x);
        void *result = _MemAlloc(size, align);
        MemPopHeap();
        return result;
    }
    static int _x = MemFindHeap("char");
    MemPushHeap(_x);
    void *result = _MemAlloc(size, align);
    MemPopHeap();
    return result;
}

void WiiMesh::CreateBuffers() {
    u32 pos_nrm_vtx_scale = GetSomeSizeFactor();

    MILO_ASSERT(!mCTVtxs, 678);
    MILO_ASSERT(!mPosNrmVtxs, 679);
    MILO_ASSERT(!mPosQ, 680);
    MILO_ASSERT(!mNrmQ, 681);
    mCTVtxs = _MemAlloc(mNumVerts << 3, 0x20);
    if (bitmask_2 && bitmask_1) {
        mPosNrmVtxs = nullptr;
        mPosQ = SkinAlloc(mNumVerts * 6, "mesh VPosNrmQ", 0x20);
        mNrmQ = SkinAlloc(mNumVerts * 3, "mesh VPosNrmQ", 0x20);
    } else {
        mPosNrmVtxs = _MemAlloc(mNumVerts * pos_nrm_vtx_scale, 0x20);
        mPosQ = nullptr;
        mNrmQ = nullptr;
    }
    if (unk_0x164 > 1) {
        MILO_ASSERT(!mBoneWeights && !mBoneIndices, 711);
        mBoneWeights =
            SkinAlloc((Min((int)unk_0x164, 4) * mNumVerts) << 1, "Vertex Weights", 0x20);
    }
    if (unk_0x164 > 4) {
        MILO_ASSERT(!mBoneIndices, 723);
        mBoneIndices = SkinAlloc(mNumVerts << 2, "Vertex Indices", 0x20);
    } else {
        if (unk_0x164 > 1) {
            mBoneIndices = SkinAlloc(unk_0x164, "Vertex Indices", 0x20);
        }
    }
}

WiiMesh::~WiiMesh() { ReleaseBuffers(); }

void WiiMesh::SetVertexDesc() {
    GXClearVtxDesc();
    GXAttrType x = mDisplays.GetIdxType();
    GXSetVtxDesc(GX_VA_POS, x);
    GXSetVtxDesc(GX_VA_NRM, x);
    GXSetVtxDesc(GX_VA_CLR0, x);
    GXSetVtxDesc(GX_VA_TEX0, x);
    GXInvalidateVtxCache();
}

void WiiMesh::SetVertexBuffers(const void *arg) {
    const void *v;
    if (bitmask_1) {
        if (arg != NULL) {
            v = arg;
        } else {
            v = mPosNrmVtxs;
        }
        GXSetArray(GX_VA_POS, v, 0xA);
        GXSetArray(GX_VA_NRM, (const void *)((u32)v + 6), 0xA);
    } else {
        if (arg != NULL) {
            v = arg;
        } else {
            v = mPosNrmVtxs;
        }
        GXSetArray(GX_VA_POS, v, 0x10);
        GXSetArray(GX_VA_NRM, (const void *)((u32)v + 0xC), 0x10);
    }
    const void *c = mCTVtxs;
    GXSetArray(GX_VA_CLR0, c, 8);
    GXSetArray(GX_VA_TEX0, (const void *)((u32)c + 4), 8);
}

void WiiMesh::DrawFaces() {
    mDisplays.Draw((u32)this, bitmask_1 ? GX_VTXFMT4 : GX_VTXFMT0);
}

void WiiMesh::DrawReflection(bool calc) {
    if (mVerts.empty()) {
        MILO_WARN("WiiMesh::DrawReflection: no vertices to render!\n");
        return;
    }
    if (mFaces.size() == 0) {
        MILO_WARN("WiiMesh::DrawReflection: no faces to render!\n");
        return;
    }
    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_INDEX16);
    GXSetVtxDesc(GX_VA_NBT, GX_DIRECT);
    GXSetVtxDesc(GX_VA_CLR0, GX_INDEX16);
    GXSetVtxDesc(GX_VA_TEX0, GX_INDEX16);
    Mtx identity;
    PSMTXIdentity(identity);
    GXLoadNrmMtxImm(identity, 0);

    Vec eyePos;
    Mtx view;
    Mtx world;
    Mtx modelView;
    ((int *)&eyePos)[0] = 0;
    ((int *)&eyePos)[1] = 0;
    ((int *)&eyePos)[2] = 0;

#ifdef MATCHING
    {
        register WiiCam *cam = static_cast<WiiCam *>(RndCam::sCurrent);
        register Mtx *dst = &view;
        ASM_BLOCK(
            psq_l       fp6,  0x278(cam), 0, 0
            psq_l       fp8,  0x284(cam), 0, 0
            psq_l       fp7,  0x280(cam), 1, 0
            psq_l       fp9,  0x28c(cam), 1, 0
            ps_merge00  fp0,  fp6,  fp8
            psq_l       fp10, 0x290(cam), 0, 0
            ps_merge11  fp2,  fp6,  fp8
            psq_l       fp12, 0x29c(cam), 0, 0
            ps_merge00  fp4,  fp7,  fp9
            psq_l       fp11, 0x298(cam), 1, 0
            psq_l       fp13, 0x2a4(cam), 1, 0
            ps_merge00  fp1, fp10, fp12
            ps_merge11  fp3, fp10, fp12
            ps_merge00  fp5, fp11, fp13
            psq_st      fp0,  0x0(dst),  0, 0
            psq_st      fp1,  0x8(dst),  0, 0
            psq_st      fp2,  0x10(dst), 0, 0
            psq_st      fp3,  0x18(dst), 0, 0
            psq_st      fp4,  0x20(dst), 0, 0
            psq_st      fp5,  0x28(dst), 0, 0
        )
    }
    {
        register Transform *src = &WorldXfm();
        register Mtx *dst = &world;
        ASM_BLOCK(
            psq_l       fp6,  0x0(src),  0, 0
            psq_l       fp8,  0xc(src),  0, 0
            psq_l       fp7,  0x8(src),  1, 0
            psq_l       fp9,  0x14(src), 1, 0
            ps_merge00  fp0,  fp6,  fp8
            psq_l       fp10, 0x18(src), 0, 0
            ps_merge11  fp2,  fp6,  fp8
            psq_l       fp12, 0x24(src), 0, 0
            ps_merge00  fp4,  fp7,  fp9
            psq_l       fp11, 0x20(src), 1, 0
            psq_l       fp13, 0x2c(src), 1, 0
            ps_merge00  fp1, fp10, fp12
            ps_merge11  fp3, fp10, fp12
            ps_merge00  fp5, fp11, fp13
            psq_st      fp0,  0x0(dst),  0, 0
            psq_st      fp1,  0x8(dst),  0, 0
            psq_st      fp2,  0x10(dst), 0, 0
            psq_st      fp3,  0x18(dst), 0, 0
            psq_st      fp4,  0x20(dst), 0, 0
            psq_st      fp5,  0x28(dst), 0, 0
        )
    }
#else
    MakeWiiMtx(static_cast<WiiCam *>(RndCam::sCurrent)->mWiiViewXfm, view);
    MakeWiiMtx(WorldXfm(), world);
#endif

    PSMTXConcat(view, world, modelView);

    unsigned short numFaces = mFaces.size();
    RndGXBegin(GX_TRIANGLES, GX_VTXFMT1, numFaces * 3);

    for (int faceIdx = 0; faceIdx < (int)numFaces; ++faceIdx) {
        unsigned short *vertIndices = &mGeomOwner->mFaces[faceIdx].v1;
        for (int i = 0; i < 3; ++i) {
            unsigned short vertIdx = *vertIndices;
            RndMesh::Vert &vert = mGeomOwner->mVerts[vertIdx];
            Vec mvPos;
            Vec mvNorm;
            Vec reflect;
            Vec eyeToVert;
            Vec pos;
            Vec norm;

            pos.x = vert.pos.x;
            pos.y = vert.pos.y;
            pos.z = vert.pos.z;
            PSMTXMultVec(modelView, &pos, &mvPos);

            norm.x = vert.norm.x;
            norm.y = vert.norm.y;
            norm.z = vert.norm.z;
            PSMTXMultVecSR(modelView, &norm, &mvNorm);

            if (calc) {
                PSVECSubtract(&eyePos, &mvPos, &eyeToVert);
                C_VECReflect(&eyeToVert, &mvNorm, &reflect);
            } else {
                reflect.z = 0.0f;
                reflect.y = 0.0f;
                reflect.x = 0.0f;
            }

            *(volatile u16 *)0xCC008000 = *vertIndices;
            *(volatile f32 *)0xCC008000 = mvNorm.x;
            *(volatile f32 *)0xCC008000 = mvNorm.y;
            *(volatile f32 *)0xCC008000 = mvNorm.z;
            *(volatile f32 *)0xCC008000 = reflect.x;
            *(volatile f32 *)0xCC008000 = reflect.y;
            *(volatile f32 *)0xCC008000 = reflect.z;
            *(volatile f32 *)0xCC008000 = 0.0f;
            *(volatile f32 *)0xCC008000 = 0.0f;
            *(volatile f32 *)0xCC008000 = 0.0f;
            *(volatile u16 *)0xCC008000 = *vertIndices;
            *(volatile u16 *)0xCC008000 = *vertIndices;
            ++vertIndices;
        }
    }
    RndGXEnd();
}

void WiiMesh::RemoveVertData() {
    mVerts.resize(0, true);
    ClearAndShrink(mFaces);
    ReleaseBuffers();
}
