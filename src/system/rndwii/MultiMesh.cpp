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
    WiiMesh *m2 = (WiiMesh *)(RndMesh *)mesh->mGeomOwner;

    bool fadeOut = false;
    if (RndEnviron::sCurrent->mFadeOut && RndEnviron::sCurrent->mFadeEnd != RndEnviron::sCurrent->mFadeStart) {
        fadeOut = true;
    }
    RndCam *curCam = RndCam::sCurrent;

    MILO_ASSERT(mesh->NumBones() == 0, 5);
#ifdef MILO_DEBUG
    if (m2->NumFaces() == 0) {
        return;
    }
#endif
    TIMER_ACTION("faces", m2->SetVertexDesc(););
    TIMER_ACTION("faces", m2->SetVertexBuffers(nullptr););
    {
        START_AUTO_TIMER("selmat");
        if (mat == nullptr)
            mat = (WiiMat *)TheRnd->mDefaultMat;
        mat->Select(false);
    }

    int count = 0;

    {
        START_AUTO_TIMER("xfms");

        // Load the camera's precomputed Wii view matrix into camMtx
        Mtx camMtx;
        {
            START_AUTO_TIMER("xfms");
            const Transform &src = ((WiiCam *)curCam)->mWiiViewXfm;
            camMtx[0][0] = src.m.x.x; camMtx[0][1] = src.m.y.x; camMtx[0][2] = src.m.z.x; camMtx[0][3] = src.v.x;
            camMtx[1][0] = src.m.x.y; camMtx[1][1] = src.m.y.y; camMtx[1][2] = src.m.z.y; camMtx[1][3] = src.v.y;
            camMtx[2][0] = src.m.x.z; camMtx[2][1] = src.m.y.z; camMtx[2][2] = src.m.z.z; camMtx[2][3] = src.v.z;
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

        int constraint = (int)(unsigned short)mesh->mConstraint;

        if (constraint == RndTransformable::kFastBillboardXYZ) {
            START_AUTO_TIMER("xfms");

            {
                START_AUTO_TIMER("xfms");
                if (curCam->Dirty()) {
                    curCam->WorldXfm_Force();
                }
                const Transform &camWorld = curCam->mWorldXfm;
                instMtx[0][0] = camWorld.m.x.x; instMtx[0][1] = camWorld.m.y.x; instMtx[0][2] = camWorld.m.z.x; instMtx[0][3] = camWorld.v.x;
                instMtx[1][0] = camWorld.m.x.y; instMtx[1][1] = camWorld.m.y.y; instMtx[1][2] = camWorld.m.z.y; instMtx[1][3] = camWorld.v.y;
                instMtx[2][0] = camWorld.m.x.z; instMtx[2][1] = camWorld.m.y.z; instMtx[2][2] = camWorld.m.z.z; instMtx[2][3] = camWorld.v.z;
            }

            int idx = 0;
            while (idx + 9 < count) {
                TIMER_ACTION("xfms",
                    int slot = 0;
                    for (int i = 0; i < 10; i++) {
                        instMtx[0][3] = it->mXfm.v.x;
                        instMtx[1][3] = it->mXfm.v.y;
                        instMtx[2][3] = it->mXfm.v.z;
                        PSMTXConcat(camMtx, instMtx, resultMtx);
                        GXLoadPosMtxImm(resultMtx, slot);
                        GXLoadNrmMtxImm(resultMtx, slot);
                        ++it;
                        slot += 3;
                    }
                );
                TIMER_ACTION("faces",
                    GXSetCurrentMtx(0); m2->DrawFaces();
                    GXSetCurrentMtx(3); m2->DrawFaces();
                    GXSetCurrentMtx(6); m2->DrawFaces();
                    GXSetCurrentMtx(9); m2->DrawFaces();
                    GXSetCurrentMtx(12); m2->DrawFaces();
                    GXSetCurrentMtx(15); m2->DrawFaces();
                    GXSetCurrentMtx(18); m2->DrawFaces();
                    GXSetCurrentMtx(21); m2->DrawFaces();
                    GXSetCurrentMtx(24); m2->DrawFaces();
                    GXSetCurrentMtx(27); m2->DrawFaces();
                );
                idx += 10;
            }
            GXSetCurrentMtx(0);
            while (idx < count) {
                instMtx[0][3] = it->mXfm.v.x;
                instMtx[1][3] = it->mXfm.v.y;
                instMtx[2][3] = it->mXfm.v.z;
                PSMTXConcat(camMtx, instMtx, resultMtx);
                GXLoadPosMtxImm(resultMtx, 0);
                GXLoadNrmMtxImm(resultMtx, 0);
                ++it;
                TIMER_ACTION("faces", m2->DrawFaces(););
                idx++;
            }
        } else if (constraint == RndTransformable::kBillboardXYZ) {
            START_AUTO_TIMER("xfms");

            {
                START_AUTO_TIMER("xfms");
                if (curCam->Dirty()) {
                    curCam->WorldXfm_Force();
                }
                const Transform &camWorld = curCam->mWorldXfm;
                instMtx[0][0] = camWorld.m.x.x; instMtx[0][1] = camWorld.m.y.x; instMtx[0][2] = camWorld.m.z.x; instMtx[0][3] = camWorld.v.x;
                instMtx[1][0] = camWorld.m.x.y; instMtx[1][1] = camWorld.m.y.y; instMtx[1][2] = camWorld.m.z.y; instMtx[1][3] = camWorld.v.y;
                instMtx[2][0] = camWorld.m.x.z; instMtx[2][1] = camWorld.m.y.z; instMtx[2][2] = camWorld.m.z.z; instMtx[2][3] = camWorld.v.z;
            }

            float baseDiag0 = instMtx[0][0];
            float baseDiag1 = instMtx[1][1];
            float baseDiag2 = instMtx[2][2];

            int idx = 0;
            while (idx + 9 < count) {
                TIMER_ACTION("xfms",
                    int slot = 0;
                    for (int i = 0; i < 10; i++) {
                        instMtx[0][3] = it->mXfm.v.x;
                        instMtx[1][3] = it->mXfm.v.y;
                        instMtx[2][3] = it->mXfm.v.z;
                        float sx = instMtx[0][0];
                        float sy = instMtx[1][1];
                        float sz = instMtx[2][2];
                        instMtx[0][0] = sx * it->mXfm.m.x.x;
                        instMtx[1][1] = sy * it->mXfm.m.y.y;
                        instMtx[2][2] = sz * it->mXfm.m.z.z;
                        PSMTXConcat(camMtx, instMtx, resultMtx);
                        GXLoadPosMtxImm(resultMtx, slot);
                        GXLoadNrmMtxImm(resultMtx, slot);
                        instMtx[0][0] = baseDiag0;
                        instMtx[1][1] = baseDiag1;
                        instMtx[2][2] = baseDiag2;
                        ++it;
                        slot += 3;
                    }
                );
                TIMER_ACTION("faces",
                    GXSetCurrentMtx(0); m2->DrawFaces();
                    GXSetCurrentMtx(3); m2->DrawFaces();
                    GXSetCurrentMtx(6); m2->DrawFaces();
                    GXSetCurrentMtx(9); m2->DrawFaces();
                    GXSetCurrentMtx(12); m2->DrawFaces();
                    GXSetCurrentMtx(15); m2->DrawFaces();
                    GXSetCurrentMtx(18); m2->DrawFaces();
                    GXSetCurrentMtx(21); m2->DrawFaces();
                    GXSetCurrentMtx(24); m2->DrawFaces();
                    GXSetCurrentMtx(27); m2->DrawFaces();
                );
                idx += 10;
            }
            GXSetCurrentMtx(0);
            while (idx < count) {
                instMtx[0][3] = it->mXfm.v.x;
                instMtx[1][3] = it->mXfm.v.y;
                instMtx[2][3] = it->mXfm.v.z;
                float sx = instMtx[0][0];
                float sy = instMtx[1][1];
                float sz = instMtx[2][2];
                instMtx[0][0] = sx * it->mXfm.m.x.x;
                instMtx[1][1] = sy * it->mXfm.m.y.y;
                instMtx[2][2] = sz * it->mXfm.m.z.z;
                PSMTXConcat(camMtx, instMtx, resultMtx);
                GXLoadPosMtxImm(resultMtx, 0);
                GXLoadNrmMtxImm(resultMtx, 0);
                instMtx[0][0] = baseDiag0;
                instMtx[1][1] = baseDiag1;
                instMtx[2][2] = baseDiag2;
                ++it;
                TIMER_ACTION("faces", m2->DrawFaces(););
                idx++;
            }
        } else {
            while (it != mInstances.end()) {
                TIMER_ACTION("xfms",
                    const Transform &src = it->mXfm;
                    instMtx[0][0] = src.m.x.x; instMtx[0][1] = src.m.y.x; instMtx[0][2] = src.m.z.x; instMtx[0][3] = src.v.x;
                    instMtx[1][0] = src.m.x.y; instMtx[1][1] = src.m.y.y; instMtx[1][2] = src.m.z.y; instMtx[1][3] = src.v.y;
                    instMtx[2][0] = src.m.x.z; instMtx[2][1] = src.m.y.z; instMtx[2][2] = src.m.z.z; instMtx[2][3] = src.v.z;
                    if (unk34) {
                        Mtx scaleMtx;
                        PSMTXScale(scaleMtx, m2->mLocalXfm.m.x.x, m2->mLocalXfm.m.y.y, m2->mLocalXfm.m.z.z);
                        PSMTXConcat(instMtx, scaleMtx, instMtx);
                    }
                    PSMTXConcat(camMtx, instMtx, resultMtx);
                    if (fadeOut) {
                        GXLoadTexMtxImm(resultMtx, GX_TEXMTX1, GX_MTX_3x4);
                    }
                    GXLoadPosMtxImm(resultMtx, 0);
                    GXLoadNrmMtxImm(resultMtx, 0);
                    ++it;
                );
                TIMER_ACTION("faces", m2->DrawFaces(););
            }
        }
    }

    unk34 = false;
    TheNgStats->mMultiMeshInsts += count;
    TheNgStats->mFaces += count * m2->NumFaces();
}
