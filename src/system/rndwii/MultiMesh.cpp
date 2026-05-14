#include "MultiMesh.h"
#include "os/Timer.h"
#include "rndobj/Cam.h"
#include "rndobj/Env.h"
#include "rndobj/Rnd.h"
#include "rndobj/Stats_NG.h"
#include "rndobj/Trans.h"
#include "rndwii/Cam.h"
#include "rndwii/Mat.h"
#include "rndwii/Mesh.h"
#include "revolution/mtx/mtx.h"
#include "revolution/gx/GXTransform.h"

void WiiMultiMesh::DrawShowing() {
    START_AUTO_TIMER("multimesh");
    if (mInstances.empty() || mMesh == nullptr) {
        return;
    }
    WiiMesh *mesh = (WiiMesh *)(RndMesh *)mMesh;
    WiiMat *mat = (WiiMat *)mesh->Mat();
    RndMesh *m2 = mesh->mGeomOwner;
    MILO_ASSERT(mesh->NumBones() == 0, 5);
#ifdef MILO_DEBUG
    if (m2->NumFaces() == 0) {
        return;
    }
#endif
    {
        TIMER_ACTION("faces", mesh->SetVertexDesc(); mesh->SetVertexBuffers(nullptr););
    }
    {
        START_AUTO_TIMER("selmat");
        if (mat == nullptr)
            mat = (WiiMat *)TheRnd->mDefaultMat;
        mat->Select(false);
    }

    int count = 0;

    {
        START_AUTO_TIMER("xfms");

        // Load the camera's precomputed Wii view matrix
        Mtx camMtx;
        {
            const Transform &src = ((WiiCam *)RndCam::sCurrent)->mWiiViewXfm;
            camMtx[0][0] = src.m.x.x; camMtx[0][1] = src.m.y.x; camMtx[0][2] = src.m.z.x; camMtx[0][3] = src.v.x;
            camMtx[1][0] = src.m.x.y; camMtx[1][1] = src.m.y.y; camMtx[1][2] = src.m.z.y; camMtx[1][3] = src.v.y;
            camMtx[2][0] = src.m.x.z; camMtx[2][1] = src.m.y.z; camMtx[2][2] = src.m.z.z; camMtx[2][3] = src.v.z;
        }

        bool fadeOut = false;
        if (RndEnviron::sCurrent->mFadeOut && RndEnviron::sCurrent->mFadeEnd != RndEnviron::sCurrent->mFadeStart) {
            fadeOut = true;
        }

        // Count instances
        {
            std::list<RndMultiMesh::Instance>::iterator countIt = mInstances.begin();
            for (; countIt != mInstances.end(); ++countIt) {
                count++;
            }
        }

        std::list<RndMultiMesh::Instance>::iterator it = mInstances.begin();
        Mtx instMtx;
        Mtx resultMtx;

        RndTransformable::Constraint constraint = (RndTransformable::Constraint)(int)mesh->mConstraint;

        if (constraint == RndTransformable::kFastBillboardXYZ) {
            START_AUTO_TIMER("xfms");

            // Load camera world transform into instMtx (rotation stays constant)
            if (RndCam::sCurrent->Dirty()) {
                RndCam::sCurrent->WorldXfm_Force();
            }
            const Transform &camWorld = RndCam::sCurrent->mWorldXfm;
            instMtx[0][0] = camWorld.m.x.x; instMtx[0][1] = camWorld.m.y.x; instMtx[0][2] = camWorld.m.z.x; instMtx[0][3] = camWorld.v.x;
            instMtx[1][0] = camWorld.m.x.y; instMtx[1][1] = camWorld.m.y.y; instMtx[1][2] = camWorld.m.z.y; instMtx[1][3] = camWorld.v.y;
            instMtx[2][0] = camWorld.m.x.z; instMtx[2][1] = camWorld.m.y.z; instMtx[2][2] = camWorld.m.z.z; instMtx[2][3] = camWorld.v.z;

            // Batch 10 instances at a time
            int idx = 0;
            int mtxSlot = 0;
            while (idx + 9 < count) {
                {
                    START_AUTO_TIMER("xfms");
                    int slot = 0;
                    for (int i = 0; i < 10; i++) {
                        instMtx[0][3] = it->mXfm.v.x;
                        instMtx[1][3] = it->mXfm.v.y;
                        instMtx[2][3] = it->mXfm.v.z;
                        PSMTXConcat(camMtx, instMtx, resultMtx);
                        GXLoadPosMtxImm(resultMtx, mtxSlot + slot);
                        GXLoadNrmMtxImm(resultMtx, mtxSlot + slot);
                        ++it;
                        slot += 3;
                    }
                }
                {
                    START_AUTO_TIMER("xfms");
                    GXSetCurrentMtx(mtxSlot + 0);
                    mesh->DrawFaces();
                    GXSetCurrentMtx(mtxSlot + 3);
                    mesh->DrawFaces();
                    GXSetCurrentMtx(mtxSlot + 6);
                    mesh->DrawFaces();
                    GXSetCurrentMtx(mtxSlot + 9);
                    mesh->DrawFaces();
                    GXSetCurrentMtx(mtxSlot + 12);
                    mesh->DrawFaces();
                    GXSetCurrentMtx(mtxSlot + 15);
                    mesh->DrawFaces();
                    GXSetCurrentMtx(mtxSlot + 18);
                    mesh->DrawFaces();
                    GXSetCurrentMtx(mtxSlot + 21);
                    mesh->DrawFaces();
                    GXSetCurrentMtx(mtxSlot + 24);
                    mesh->DrawFaces();
                    GXSetCurrentMtx(mtxSlot + 27);
                    mesh->DrawFaces();
                }
                idx += 10;
            }
            // Remaining instances
            GXSetCurrentMtx(0);
            while (idx < count) {
                {
                    START_AUTO_TIMER("xfms");
                    instMtx[0][3] = it->mXfm.v.x;
                    instMtx[1][3] = it->mXfm.v.y;
                    instMtx[2][3] = it->mXfm.v.z;
                    PSMTXConcat(camMtx, instMtx, resultMtx);
                    GXLoadPosMtxImm(resultMtx, 0);
                    GXLoadNrmMtxImm(resultMtx, 0);
                    ++it;
                }
                {
                    START_AUTO_TIMER("xfms");
                    mesh->DrawFaces();
                }
                idx++;
            }
        } else if (constraint == RndTransformable::kBillboardXYZ) {
            START_AUTO_TIMER("xfms");

            if (RndCam::sCurrent->Dirty()) {
                RndCam::sCurrent->WorldXfm_Force();
            }
            const Transform &camWorld = RndCam::sCurrent->mWorldXfm;
            float baseX = camWorld.v.x;
            float baseY = camWorld.v.y;
            float baseZ = camWorld.v.z;
            instMtx[0][0] = camWorld.m.x.x; instMtx[0][1] = camWorld.m.y.x; instMtx[0][2] = camWorld.m.z.x; instMtx[0][3] = baseX;
            instMtx[1][0] = camWorld.m.x.y; instMtx[1][1] = camWorld.m.y.y; instMtx[1][2] = camWorld.m.z.y; instMtx[1][3] = baseY;
            instMtx[2][0] = camWorld.m.x.z; instMtx[2][1] = camWorld.m.y.z; instMtx[2][2] = camWorld.m.z.z; instMtx[2][3] = baseZ;

            int idx = 0;
            int mtxSlot = 0;
            while (idx + 9 < count) {
                {
                    START_AUTO_TIMER("xfms");
                    int slot = 0;
                    for (int i = 0; i < 10; i++) {
                        instMtx[0][3] = it->mXfm.v.x;
                        instMtx[1][3] = it->mXfm.v.y;
                        instMtx[2][3] = it->mXfm.v.z;
                        instMtx[0][0] = camWorld.m.x.x * it->mXfm.m.x.x;
                        instMtx[1][1] = camWorld.m.y.y * it->mXfm.m.y.y;
                        instMtx[2][2] = camWorld.m.z.z * it->mXfm.m.z.z;
                        PSMTXConcat(camMtx, instMtx, resultMtx);
                        GXLoadPosMtxImm(resultMtx, mtxSlot + slot);
                        GXLoadNrmMtxImm(resultMtx, mtxSlot + slot);
                        instMtx[0][0] = camWorld.m.x.x;
                        instMtx[1][1] = camWorld.m.y.y;
                        instMtx[2][2] = camWorld.m.z.z;
                        instMtx[0][3] = baseX;
                        instMtx[1][3] = baseY;
                        instMtx[2][3] = baseZ;
                        ++it;
                        slot += 3;
                    }
                }
                {
                    START_AUTO_TIMER("xfms");
                    GXSetCurrentMtx(mtxSlot + 0);
                    mesh->DrawFaces();
                    GXSetCurrentMtx(mtxSlot + 3);
                    mesh->DrawFaces();
                    GXSetCurrentMtx(mtxSlot + 6);
                    mesh->DrawFaces();
                    GXSetCurrentMtx(mtxSlot + 9);
                    mesh->DrawFaces();
                    GXSetCurrentMtx(mtxSlot + 12);
                    mesh->DrawFaces();
                    GXSetCurrentMtx(mtxSlot + 15);
                    mesh->DrawFaces();
                    GXSetCurrentMtx(mtxSlot + 18);
                    mesh->DrawFaces();
                    GXSetCurrentMtx(mtxSlot + 21);
                    mesh->DrawFaces();
                    GXSetCurrentMtx(mtxSlot + 24);
                    mesh->DrawFaces();
                    GXSetCurrentMtx(mtxSlot + 27);
                    mesh->DrawFaces();
                }
                idx += 10;
            }
            GXSetCurrentMtx(0);
            while (idx < count) {
                {
                    START_AUTO_TIMER("xfms");
                    instMtx[0][3] = it->mXfm.v.x;
                    instMtx[1][3] = it->mXfm.v.y;
                    instMtx[2][3] = it->mXfm.v.z;
                    instMtx[0][0] = camWorld.m.x.x * it->mXfm.m.x.x;
                    instMtx[1][1] = camWorld.m.y.y * it->mXfm.m.y.y;
                    instMtx[2][2] = camWorld.m.z.z * it->mXfm.m.z.z;
                    PSMTXConcat(camMtx, instMtx, resultMtx);
                    GXLoadPosMtxImm(resultMtx, 0);
                    GXLoadNrmMtxImm(resultMtx, 0);
                    instMtx[0][0] = camWorld.m.x.x;
                    instMtx[1][1] = camWorld.m.y.y;
                    instMtx[2][2] = camWorld.m.z.z;
                    instMtx[0][3] = baseX;
                    instMtx[1][3] = baseY;
                    instMtx[2][3] = baseZ;
                    ++it;
                }
                {
                    START_AUTO_TIMER("xfms");
                    mesh->DrawFaces();
                }
                idx++;
            }
        } else {
            while (it != mInstances.end()) {
                {
                    START_AUTO_TIMER("xfms");
                    const Transform &src = it->mXfm;
                    instMtx[0][0] = src.m.x.x; instMtx[0][1] = src.m.y.x; instMtx[0][2] = src.m.z.x; instMtx[0][3] = src.v.x;
                    instMtx[1][0] = src.m.x.y; instMtx[1][1] = src.m.y.y; instMtx[1][2] = src.m.z.y; instMtx[1][3] = src.v.y;
                    instMtx[2][0] = src.m.x.z; instMtx[2][1] = src.m.y.z; instMtx[2][2] = src.m.z.z; instMtx[2][3] = src.v.z;
                    if (unk34) {
                        Mtx scaleMtx;
                        PSMTXScale(scaleMtx, mesh->mLocalXfm.m.x.x, mesh->mLocalXfm.m.y.y, mesh->mLocalXfm.m.z.z);
                        PSMTXConcat(instMtx, scaleMtx, instMtx);
                    }
                    PSMTXConcat(camMtx, instMtx, resultMtx);
                    if (fadeOut) {
                        GXLoadTexMtxImm(resultMtx, GX_TEXMTX1, GX_MTX_3x4);
                    }
                    GXLoadPosMtxImm(resultMtx, 0);
                    GXLoadNrmMtxImm(resultMtx, 0);
                    ++it;
                }
                {
                    START_AUTO_TIMER("xfms");
                    mesh->DrawFaces();
                }
            }
        }
    }

    unk34 = false;
    TheNgStats->mMultiMeshInsts += count;
    TheNgStats->mFaces += count * m2->NumFaces();
}
