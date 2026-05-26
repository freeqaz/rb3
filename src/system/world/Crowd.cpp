#include "world/Crowd.h"
#include "CameraShot.h"
#include "char/Character.h"
#include "decomp.h"
#include "math/Color.h"
#include "math/Mtx.h"
#include "math/Rand.h"
#include "math/Utl.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/ObjMacros.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "os/Timer.h"
#include "rndobj/Cam.h"
#include "rndobj/Draw.h"
#include "rndobj/Env.h"
#include "rndobj/Mesh.h"
#include "rndobj/MultiMesh.h"
#include "rndobj/Poll.h"
#include "rndobj/Trans.h"
#include "rndwii/Mat.h"
#include "rndwii/Mesh.h"
#include "rndwii/Rnd.h"
#include "stl/_pair.h"
#include "utl/Loader.h"
#include "utl/Symbols.h"
#include "world/ColorPalette.h"

RndCam *gImpostorCamera;
RndMat *gImpostorMat;
int gNumCrowd;
WorldCrowd *gParent;

INIT_REVS(WorldCrowd)

namespace {
    void GetMeshShaderFlags(RndMat *mat, std::list<unsigned int> &flags) {
        std::vector<ObjRef *>::const_reverse_iterator it = mat->Refs().rbegin();
        std::vector<ObjRef *>::const_reverse_iterator itEnd = mat->Refs().rend();
        for (; it != itEnd; ++it) {
            RndMesh *mesh = dynamic_cast<RndMesh *>((*it)->RefOwner());
            if (mesh) {
                unsigned int flag = 0;
                flag |= mesh->IsSkinned();
                flag |= -(mesh->HasAOCalc()) & 2;
                flags.push_back(flag);
            }
        }
        flags.sort();
        flags.unique();
    }
}

DECOMP_FORCEACTIVE(Crowd, "WorldCrowd[%s] does not have a placement mesh.")

WorldCrowd::WorldCrowd()
    : mPlacementMesh(this), mCharacters(this), mNum(0), mRotate(kCrowdRotateNone),
      mForce3DCrowd(0), mShow3DOnly(0), mCharFullness(1.0f), mFlatFullness(1.0f), mLod(0),
      mEnviron(this), mEnviron3D(this), mFocus(this), mModifyStamp(0) {
    if (gNumCrowd++ == 0) {
#ifdef MILO_DEBUG
        GetGfxMode();
#endif
        gImpostorMat = Hmx::Object::New<RndMat>();
        gImpostorMat->SetUseEnv(true);
        gImpostorMat->SetPreLit(false);
        gImpostorMat->SetBlend(RndMat::kBlendSrc);
        gImpostorMat->SetZMode(RndMat::kZModeNormal);
        gImpostorMat->SetAlphaCut(true);
        gImpostorMat->SetAlphaThreshold(0x80);
        gImpostorMat->SetTexWrap(kTexWrapClamp);
        gImpostorMat->SetPerPixelLit(false);
        gImpostorMat->SetPointLights(true);
        gImpostorCamera = Hmx::Object::New<RndCam>();
        SetMatAndCameraLod();
    }
}

void WorldCrowd::SetMatAndCameraLod() {
    RndTex *tex = TheWiiRnd.GetSharedTex((WiiRnd::SharedTexType)5, true);
    gImpostorCamera->SetTargetTex(tex);
    gImpostorMat->SetDiffuseTex(tex);
}

WorldCrowd::~WorldCrowd() {
    FOREACH (it, mCharacters) {
        if (it->mMMesh) {
            delete it->mMMesh->GetMesh();
            RELEASE(it->mMMesh);
        }
    }
    gNumCrowd--;
    if (gNumCrowd == 0) {
        RELEASE(gImpostorCamera);
        RELEASE(gImpostorMat);
    }
}

void WorldCrowd::CreateMeshes() {
    mCharFullness = 1.0f;
    mFlatFullness = 1.0f;
    mLod = 0;
    FOREACH (it, mCharacters) {
        if (it->mMMesh) {
            delete it->mMMesh->GetMesh();
            RELEASE(it->mMMesh);
        }
        it->mBackup.clear();
        if (it->mDef.mChar) {
            RndMesh *built = BuildBillboard(it->mDef.mChar, it->mDef.mHeight);
            it->mMMesh = Hmx::Object::New<RndMultiMesh>();
            it->mMMesh->SetMesh(built);
        }
    }
}

DataNode WorldCrowd::OnRebuild(DataArray *da) { return 0; }

BEGIN_COPYS(WorldCrowd)
    COPY_SUPERCLASS(RndDrawable)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY(WorldCrowd)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mPlacementMesh)
        COPY_MEMBER(mNum)
        COPY_MEMBER(mCenter)
        COPY_MEMBER(mCharFullness)
        COPY_MEMBER(mFlatFullness)
        COPY_MEMBER(mLod)
        COPY_MEMBER(mEnviron)
        COPY_MEMBER(mEnviron3D)
        COPY_MEMBER(mForce3DCrowd)
        COPY_MEMBER(mShow3DOnly)
        COPY_MEMBER(mFocus)
        mCharacters.clear();
        mCharacters.resize(c->mCharacters.size());
        ObjList<CharData>::const_iterator j = c->mCharacters.begin();
        ObjList<CharData>::iterator i = mCharacters.begin();
        for (; i != mCharacters.end(); ++i, ++j) {
            i->mDef = j->mDef;
            i->mBackup = j->mBackup;
            i->m3DChars = j->m3DChars;
            i->m3DCharsCreated = j->m3DCharsCreated;
        }
        CreateMeshes();
        j = c->mCharacters.begin();
        for (ObjList<CharData>::iterator i = mCharacters.begin(); i != mCharacters.end();
             ++i, ++j) {
            if (i->mMMesh) {
                MILO_ASSERT(j->mMMesh, 0x1EB);
                i->mMMesh->mInstances = j->mMMesh->mInstances;
            }
        }
    END_COPYING_MEMBERS
END_COPYS

void WorldCrowd::CollideList(const Segment &seg, std::list<Collision> &colls) {
    if (LOADMGR_EDITMODE && CollideSphere(seg)) {
        FOREACH (it, mCharacters) {
            RndMultiMesh *curMM = it->mMMesh;
            if (curMM) {
                curMM->CollideList(seg, colls);
            }
        }
    }
}

void WorldCrowd::Reset3DCrowd() {
    SetFullness(1.0f, mCharFullness);
    FOREACH (it, mCharacters) {
        RndMultiMesh *multiMesh = it->mMMesh;
        if (multiMesh) {
            std::list<RndMultiMesh::Instance>::iterator instIt =
                multiMesh->mInstances.begin();
            int i6 = 0;
            for (unsigned int i = 0; i != it->m3DCharsCreated.size(); i++) {
                int cap = it->m3DCharsCreated[i].unk30;
                for (; i6 != cap; i6++)
                    ++instIt;
                CharData::Char3D &curChar3D = it->m3DCharsCreated[i];
                RndMultiMesh::Instance inst(curChar3D.unk0);
                instIt = multiMesh->mInstances.insert(instIt, inst);
            }
        }
        it->m3DCharsCreated.clear();
        it->m3DChars.clear();
    }
}

void WorldCrowd::Sort3DCharList() {
    FOREACH (it, mCharacters) {
        std::sort(it->m3DChars.begin(), it->m3DChars.end(), Sort3DChars());
        it->m3DCharsCreated = it->m3DChars;
    }
}

// matches in retail
void WorldCrowd::Set3DCharAll() {
    START_AUTO_TIMER("crowd_set3d");
    float fvar1 = mFlatFullness;
    Reset3DCrowd();
    FOREACH (it, mCharacters) {
        RndMultiMesh *multiMesh = it->mMMesh;
        if (multiMesh) {
            std::list<RndMultiMesh::Instance>::iterator instIt =
                multiMesh->mInstances.begin();
            int idx = 0;
            for (; instIt != multiMesh->mInstances.end(); ++instIt, ++idx) {
                CharData::Char3D char3D(instIt->mXfm, idx);
                it->m3DChars.push_back(char3D);
            }
            multiMesh->mInstances.clear();
            multiMesh->InvalidateProxies();
        }
    }
    Sort3DCharList();
    SetFullness(fvar1, mCharFullness);
    AssignRandomColors();
}

void WorldCrowd::Set3DCharList(
    const std::vector<std::pair<int, int> > &pairVec, Hmx::Object *obj
) {
    START_AUTO_TIMER("crowd_set3d");
    if (IsForced3DCrowd()) {
        return;
    }
    float oldFullness = mFlatFullness;
    Reset3DCrowd();
    std::vector<std::pair<RndMultiMesh *, std::list<RndMultiMesh::Instance>::iterator> >
        grosserPairs;
    grosserPairs.reserve(pairVec.size());
    for (unsigned int i = 0; i != pairVec.size(); i++) {
        int cap1 = pairVec[i].first;
#ifdef MILO_DEBUG
        if (cap1 >= mCharacters.size()) {
            MILO_WARN(
                "%s setting bad mesh %d, only has %d",
                PathName(obj),
                cap1,
                mCharacters.size()
            );
        } else {
#endif
            ObjList<CharData>::iterator charIt = mCharacters.begin();
            for (int n = 0; n < cap1; ++n, ++charIt)
                ;
            RndMultiMesh *curMMesh = charIt->mMMesh;
            if (curMMesh) {
                int cap2 = pairVec[i].second;
#ifdef MILO_DEBUG
                if (cap2 >= curMMesh->mInstances.size()) {
                    MILO_WARN(
                        "%s setting bad 3d char %d on mmesh %s, only has %d chars",
                        PathName(this),
                        cap2,
                        curMMesh->Name(),
                        curMMesh->mInstances.size()
                    );
                } else {
#endif
                    std::list<RndMultiMesh::Instance>::iterator instIt =
                        curMMesh->mInstances.begin();
                    for (int n = 0; n < cap2; ++instIt, ++n)
                        ;
                    charIt->m3DChars.push_back(CharData::Char3D(instIt->mXfm, cap2));
                    grosserPairs.push_back(std::make_pair(charIt->mMMesh, instIt));
#ifdef MILO_DEBUG
                }
#endif
            }
#ifdef MILO_DEBUG
        }
#endif
    }
    for (unsigned int i = 0; i != grosserPairs.size(); i++) {
        grosserPairs[i].first->mInstances.erase(grosserPairs[i].second);
        grosserPairs[i].first->InvalidateProxies();
    }
    Sort3DCharList();
    SetFullness(oldFullness, mCharFullness);
    AssignRandomColors();
}

void SetMatColorFlags(
    ObjPtrList<RndMat> &matList,
    RndMat::ColorModFlags flags,
    std::vector<Hmx::Color> *modulate
) {
    FOREACH (it, matList) {
        (*it)->SetColorModFlags(flags);
        if (modulate) {
            MILO_ASSERT(RndMat::kColorModNum == modulate->size(), 0x2CB);
            for (int i = 0; i < modulate->size(); i++) {
                (*it)->SetColorMod(modulate->at(i), i);
            }
        }
    }
}

bool WorldCrowd::Crowd3DExists() {
    FOREACH (it, mCharacters) {
        if ((*it).mDef.mChar && (*it).mMMesh && !(*it).m3DChars.empty()) {
            return true;
        }
    }
    return false;
}

void WorldCrowd::Draw3DChars() {
    if (!Crowd3DExists()) return;
    ObjPtr<RndEnviron> *envPtr = mEnviron3D.mPtr ? &mEnviron3D : &mEnviron;
    RndEnviron *env = envPtr->mPtr;
    bool savedApprox = true;
    if (env) {
        savedApprox = env->UsesApproxGlobal();
        env->SetUseApproxGlobal(false);
    }
    RndEnvironTracker tracker(env, nullptr);
    FOREACH (charIt, mCharacters) {
        Character *curChar = charIt->mDef.mChar;
        if (curChar && charIt->mMMesh) {
            for (unsigned int i = 0; i != charIt->m3DChars.size(); i++) {
                Transform spXfm;
                {
                    const Transform &charXfm = charIt->m3DChars[i].unk0;
                    spXfm.v.x = charXfm.v.x;
                    spXfm.v.y = charXfm.v.y;
                    spXfm.v.z = charXfm.v.z;
                }
                spXfm.v.z = -(charIt->mDef.mHeight * 0.5f - spXfm.v.z);
                if (mRotate != 0 || mFocus) {
                    Transform &placeXfm = mPlacementMesh->WorldXfm();
                    spXfm.m.z.x = placeXfm.m.z.x;
                    spXfm.m.z.y = placeXfm.m.z.y;
                    spXfm.m.z.z = placeXfm.m.z.z;
                    if (mRotate == 1) {
                        Transform &camXfm = RndCam::sCurrent->WorldXfm();
                        float cyx = camXfm.m.y.x;
                        float xT7 = spXfm.m.z.y * cyx;
                        float xT6 = spXfm.m.z.z * cyx;
                        float cyy = camXfm.m.y.y;
                        float xT0 = spXfm.m.z.z * cyy;
                        float xT2 = spXfm.m.z.x * cyy;
                        float xT1 = spXfm.m.z.x * camXfm.m.y.z;
                        spXfm.m.x.x = spXfm.m.z.y * camXfm.m.y.z - xT0;
                        spXfm.m.x.y = xT6 - xT1;
                        spXfm.m.x.z = xT2 - xT7;
                    } else if (mRotate == 2) {
                        Transform &camXfm = RndCam::sCurrent->WorldXfm();
                        float czx_b = spXfm.m.z.x;
                        float xT7 = camXfm.m.y.y * czx_b;
                        float xT6 = camXfm.m.y.z * czx_b;
                        float cyx_b = camXfm.m.y.x;
                        float xT0 = camXfm.m.y.z * spXfm.m.z.y;
                        float xT2 = cyx_b * spXfm.m.z.y;
                        float xT1 = cyx_b * spXfm.m.z.z;
                        spXfm.m.x.x = camXfm.m.y.y * spXfm.m.z.z - xT0;
                        spXfm.m.x.y = xT6 - xT1;
                        spXfm.m.x.z = xT2 - xT7;
                    } else {
                        Transform &focusXfm = mFocus->WorldXfm();
                        Vector3 fwd2d;
                        fwd2d.x = focusXfm.v.x - spXfm.v.x;
                        fwd2d.y = focusXfm.v.y - spXfm.v.y;
                        fwd2d.z = 0.0f;
                        spXfm.m.x.x = fwd2d.y * spXfm.m.z.z - fwd2d.z * spXfm.m.z.y;
                        spXfm.m.x.z = fwd2d.x * spXfm.m.z.y - fwd2d.y * spXfm.m.z.x;
                        spXfm.m.x.y = fwd2d.z * spXfm.m.z.x - fwd2d.x * spXfm.m.z.z;
                    }
                    Normalize(spXfm.m.x, spXfm.m.x);
                    spXfm.m.y.x = spXfm.m.z.y * spXfm.m.x.z - spXfm.m.z.z * spXfm.m.x.y;
                    spXfm.m.y.y = spXfm.m.z.z * spXfm.m.x.x - spXfm.m.z.x * spXfm.m.x.z;
                    spXfm.m.y.z = spXfm.m.z.x * spXfm.m.x.y - spXfm.m.z.y * spXfm.m.x.x;
                } else {
                    Transform &placeXfm = mPlacementMesh->WorldXfm();
                    spXfm.m = placeXfm.m;
                }
                if (charIt->mDef.mUseRandomColor) {
                    SetMatColorFlags(charIt->mDef.mMats, RndMat::kColorModModulate, &charIt->m3DChars[i].mRandomColors);
                }
                bool savedSelfShadow = curChar->mSelfShadow;
                bool savedFloorShadow = curChar->mFloorShadow;
                bool savedSpotCutout = curChar->mSpotCutout;
                if (TheRnd->InGame()) {
                    curChar->mSelfShadow = false;
                    curChar->mFloorShadow = false;
                    curChar->mSpotCutout = false;
                }
                curChar->SetWorldXfm(spXfm);
                RndDrawable &drawable = *curChar;
                drawable.DrawShowing();
                curChar->mSelfShadow = savedSelfShadow;
                curChar->mFloorShadow = savedFloorShadow;
                curChar->mSpotCutout = savedSpotCutout;
            }
        }
    }
    if (env) {
        env->SetUseApproxGlobal(savedApprox);
    }
}

void WorldCrowd::DrawShowing() {
    START_AUTO_TIMER("crowd_draw");
    if (!mPlacementMesh) return;
    Draw3DChars();
    if (TheRnd->DrawMode() == kDrawOcclusion) return;
    MILO_ASSERT(!gImpostorMat->NextPass(), 0x34A);
    std::vector<Hmx::Rect> rects;
    rects.reserve(12);
    FOREACH (charIt, mCharacters) {
        Character *curChar = charIt->mDef.mChar;
        RndMultiMesh *mmesh = charIt->mMMesh;
        if (curChar && mmesh && !mShow3DOnly && TheRnd->DrawMode() != kDrawOcclusion) {
            int numInstances = 0;
            for (std::list<RndMultiMesh::Instance>::iterator instIt = mmesh->mInstances.begin();
                 instIt != mmesh->mInstances.end(); ++instIt) {
                numInstances++;
            }
            if (numInstances == 0) continue;
            {
                SetMatAndCameraLod();
                RndCam *curCam = RndCam::Current();
                Transform camXfmCopy;
                camXfmCopy.m = curCam->WorldXfm().m;

                float halfHeight = charIt->mDef.mHeight * 0.5f;

                const Transform &curCamXfm = curCam->WorldXfm();
                const Transform &placementXfm = mPlacementMesh->WorldXfm();
                camXfmCopy.v.y = curCamXfm.v.y - placementXfm.v.y;
                camXfmCopy.v.x = curCamXfm.v.x - placementXfm.v.x;
                camXfmCopy.v.z = curCamXfm.v.z - placementXfm.v.z - halfHeight;
                float dist = Length(camXfmCopy.v);
                float minDist = curCam->NearPlane() + halfHeight;
                dist = Max(dist, minDist);
                Vector3 delta(0.0f, -dist, 0.0f);
                Multiply(delta, camXfmCopy.m, camXfmCopy.v);
                camXfmCopy.v.z += halfHeight;
                gImpostorCamera->SetLocalXfm(camXfmCopy);
                float yFov = (float)std::atan((double)(halfHeight / dist)) * 2.0f;
                float nearP = curCam->NearPlane();
                gImpostorCamera->SetFrustum(
                    nearP, curCam->FarPlane(), yFov, 1.0f
                );

                Transform charXfm;
                if (mRotate == kCrowdRotateNone) {
                    const Transform &meshXfm = mPlacementMesh->WorldXfm();
                    charXfm.m = meshXfm.m;
                } else {
                    const Transform &meshXfm2 = mPlacementMesh->WorldXfm();
                    charXfm.m.z.x = meshXfm2.m.z.x;
                    charXfm.m.z.y = meshXfm2.m.z.y;
                    charXfm.m.z.z = meshXfm2.m.z.z;

                    if (mRotate == kCrowdRotateFace) {
                        const Transform &camWXfm = curCam->WorldXfm();
                        float cyx = camWXfm.m.y.x;
                        float xT7 = charXfm.m.z.y * cyx;
                        float xT6 = charXfm.m.z.z * cyx;
                        float cyy = camWXfm.m.y.y;
                        float xT0 = charXfm.m.z.z * cyy;
                        float xT2 = charXfm.m.z.x * cyy;
                        float xT1 = charXfm.m.z.x * camWXfm.m.y.z;
                        charXfm.m.x.x = charXfm.m.z.y * camWXfm.m.y.z - xT0;
                        charXfm.m.x.y = xT6 - xT1;
                        charXfm.m.x.z = xT2 - xT7;
                    } else {
                        const Transform &camWXfm = curCam->WorldXfm();
                        float czx_b = charXfm.m.z.x;
                        float xT7 = camWXfm.m.y.y * czx_b;
                        float xT6 = camWXfm.m.y.z * czx_b;
                        float cyx_b = camWXfm.m.y.x;
                        float xT0 = camWXfm.m.y.z * charXfm.m.z.y;
                        float xT2 = cyx_b * charXfm.m.z.y;
                        float xT1 = cyx_b * charXfm.m.z.z;
                        charXfm.m.x.x = camWXfm.m.y.y * charXfm.m.z.z - xT0;
                        charXfm.m.x.y = xT6 - xT1;
                        charXfm.m.x.z = xT2 - xT7;
                    }

                    Normalize(charXfm.m.x, charXfm.m.x);

                    float cxx = charXfm.m.x.x;
                    float yT7 = charXfm.m.z.y * cxx;
                    float yT6 = charXfm.m.z.z * cxx;
                    float cxy = charXfm.m.x.y;
                    float czx = charXfm.m.z.x;
                    float yT0 = charXfm.m.z.z * cxy;
                    float yT2 = czx * cxy;
                    float yT1 = czx * charXfm.m.x.z;
                    charXfm.m.y.x = charXfm.m.z.y * charXfm.m.x.z - yT0;
                    charXfm.m.y.y = yT6 - yT1;
                    charXfm.m.y.z = yT2 - yT7;
                }
                charXfm.v.x = 0;
                charXfm.v.y = 0;
                charXfm.v.z = 0;
                curChar->SetWorldXfm(charXfm);

                rects.erase(rects.begin(), rects.end());

                TheWiiRnd.PrepareRenderAlley();
                if (TheRnd->DrawMode() == kDrawNormal) {
                    if (!mEnviron) {
                        MILO_NOTIFY_ONCE(
                            "%s: Rendering 2D crowd character texture without an environment, set the environ property on the WorldCrowd object.",
                            PathName(this)
                        );
                    }
                    RndEnviron *env = mEnviron;
                    bool savedApprox = true;
                    if (env) {
                        savedApprox = env->UsesApproxGlobal();
                        env->SetUseApproxGlobal(false);
                    }
                    {
                        const Transform &charWorldXfm = curChar->WorldXfm();
                        const RndEnvironTracker tracker(mEnviron, &charWorldXfm.v);
                        gImpostorCamera->Select();
                        WiiMat::SetOverrideAlphaWrite(true);
                        curChar->SetShowing(true);
                        curChar->DrawShowing();
                        WiiMat::SetOverrideAlphaWrite(false);
                        if (mEnviron) {
                            env->SetUseApproxGlobal(savedApprox);
                        }
                        curCam->Select();
                    }
                }
                TheWiiRnd.RestoreRenderAlley();

                {
                    RndEnviron *curEnv = RndEnviron::sCurrent;
                    bool savedApprox = true;
                    if (curEnv) {
                        savedApprox = curEnv->UsesApproxGlobal();
                        curEnv->SetUseApproxGlobal(false);
                    }
                    {
                        RndEnvironTracker tracker(curEnv, nullptr);
                        mmesh->DrawShowing();
                        if (curEnv) {
                            curEnv->SetUseApproxGlobal(savedApprox);
                        }
                    }
                }
            }
        }
    }
}

RndMesh *WorldCrowd::BuildBillboard(Character *c, float f) {
    c->mSphere.GetRadius();
    RndMesh *mesh = Hmx::Object::New<RndMesh>();
    RndMesh::VertVector &verts = mesh->Verts();
    std::vector<RndMesh::Face> &faces = mesh->Faces();
    float f1 = f * 0.5f;
    float f2 = f1 * 0.5f;
    verts.resize(4, true);
    float f2neg = -f2;
    verts[0].pos.Set(f2neg, 0, f1);
    float f1neg = -f1;
    verts[1].pos.Set(f2neg, 0, f1neg);
    verts[2].pos.Set(f2, 0, f1);
    verts[3].pos.Set(f2, 0, f1neg);

    verts[0].uv.Set(0, 0);
    verts[1].uv.Set(0, 1);
    verts[2].uv.Set(1, 0);
    verts[3].uv.Set(1, 1);

    faces.resize(2);
    faces[0].Set(0, 1, 2);
    faces[1].Set(1, 3, 2);
    mesh->Sync(0x3F);
    mesh->SetMat(gImpostorMat);
    mesh->SetTransConstraint(RndTransformable::kFastBillboardXYZ, gImpostorCamera, false);
    return mesh;
}

void WorldCrowd::SetLod(int lod) { mLod = Clamp(0, 2, lod); }

void WorldCrowd::SetFullness(float flatFullness, float charFullness) {
    START_AUTO_TIMER("crowd_set");
    mFlatFullness = flatFullness;
    mCharFullness = charFullness;
    FOREACH (it, mCharacters) {
        RndMultiMesh *multiMesh = it->mMMesh;
        if (multiMesh) {
            std::list<RndMultiMesh::Instance> &instances = multiMesh->mInstances;
            int instanceCount = (int)instances.size();
            std::list<RndMultiMesh::Instance> &backup = it->mBackup;
            int backupCount = (int)backup.size();
            int targetInstances = (int)((float)(instanceCount + backupCount) * mFlatFullness);
            if (instanceCount < targetInstances) {
                int toMove = targetInstances - instanceCount;
                std::list<RndMultiMesh::Instance>::iterator backIt = backup.begin();
                std::list<RndMultiMesh::Instance>::iterator backBegin = backup.begin();
                for (int i = 0; i < toMove; i++) {
                    ++backIt;
                }
                instances.splice(instances.end(), backup, backBegin, backIt);
            } else if (targetInstances < instanceCount) {
                int toRemove = instanceCount - targetInstances;
                std::list<RndMultiMesh::Instance>::iterator instIt = instances.begin();
                std::list<RndMultiMesh::Instance>::iterator instBegin = instances.begin();
                for (int i = 0; i < toRemove; i++) {
                    ++instIt;
                }
                backup.splice(backup.end(), instances, instBegin, instIt);
                multiMesh->InvalidateProxies();
            }
            unsigned short totalChars3D = it->m3DCharsCreated.size();
            int targetChars3D = (int)((float)totalChars3D * charFullness);
            if (targetChars3D >= (int)totalChars3D) {
                targetChars3D = (int)totalChars3D;
            }
            int currentChars3D = (int)it->m3DChars.size();
            if (currentChars3D < targetChars3D) {
                int toAdd = targetChars3D - currentChars3D;
                for (int i = 0; i < toAdd; i++) {
                    it->m3DChars.push_back(it->m3DCharsCreated[(int)it->m3DChars.size()]);
                }
            } else if (targetChars3D < currentChars3D) {
                int toRemove = currentChars3D - targetChars3D;
                for (int i = 0; i < toRemove; i++) {
                    it->m3DChars.pop_back();
                }
            }
        }
    }
}

SAVE_OBJ(WorldCrowd, 0x4BF)

DECOMP_FORCEACTIVE(WorldCrowd, "ObjPtr_p.h", "f.Owner()", "")

// matches in retail
BEGIN_LOADS(WorldCrowd)
    LOAD_REVS(bs)
    ASSERT_REVS(0xE, 0)
    LOAD_SUPERCLASS(RndDrawable)
    bs >> mPlacementMesh;
    if (gRev < 3) {
        int i;
        bs >> i;
    }
    bs >> mNum;
    if (gRev < 8) {
        bool b;
        bs >> b;
    }
    bs >> mCharacters;
    if (gRev > 6)
        bs >> mEnviron;
    if (gRev > 9)
        bs >> mEnviron3D;
    else
        mEnviron3D = mEnviron;
    if (gRev > 1) {
        CreateMeshes();
        FOREACH (it, mCharacters) {
            if (gRev < 0xE) {
                std::list<Transform> xfmList;
                std::list<RndMultiMesh::Instance> instancesList;
                std::list<OldMMInst> oldmmiList;
                if (it->mMMesh) {
                    if (gRev < 9) {
                        bs >> xfmList;
                        it->mMMesh->mInstances.clear();
                        FOREACH (transIt, xfmList) {
                            it->mMMesh->mInstances.push_back(RndMultiMesh::Instance(*transIt));
                        }
                    } else if (gRev < 0xB) {
                        bs >> oldmmiList;
                        FOREACH (mmiIt, oldmmiList) {
                            const OldMMInst &old = *mmiIt;
                            std::list<RndMultiMesh::Instance> &instances =
                                it->mMMesh->mInstances;
                            instances.push_back(RndMultiMesh::Instance(old.mOldXfm));
                        }
                    } else {
                        std::list<RndMultiMesh::Instance> &instances =
                            it->mMMesh->mInstances;
                        unsigned int count;
                        bs >> count;
                        if (count > 10000000) {
                            MILO_FAIL(
                                "Crowd tried to allocate %d mesh instances.  That's a little excessive, no?",
                                count
                            );
                        }
                        instances.resize(count);
                        FOREACH_POST (instIt, instances) {
                            instIt->LoadRev(bs, 3);
                        }
                    }
                } else if (gRev > 3) {
                    if (gRev < 9)
                        bs >> xfmList;
                    else if (gRev < 0xB)
                        bs >> oldmmiList;
                    else
                        bs >> instancesList;
                }
            } else {
                std::list<Transform> xfms;
                bs >> xfms;
                it->mMMesh->mInstances.clear();
                std::list<Transform>::iterator xfmItEnd = xfms.end();
                std::list<Transform>::iterator xfmIt = xfms.begin();
                for (; xfmIt != xfmItEnd; ++xfmIt) {
                    std::list<RndMultiMesh::Instance> &instances = it->mMMesh->mInstances;
                    instances.push_back(RndMultiMesh::Instance(*xfmIt));
                }
            }
            AssignRandomColors();
        }
    } else
        OnRebuild(0);
    if (gRev > 4)
        bs >> mModifyStamp;
    if (gRev > 0xC) {
        bool force = false;
        bs >> force;
        Force3DCrowd(force);
    }
    if (gRev > 5)
        bs >> mShow3DOnly;
    if (gRev > 0xB)
        bs >> mFocus;
    if (gRev != 0)
        LOAD_SUPERCLASS(RndHighlightable);
END_LOADS

void WorldCrowd::AssignRandomColors() {
    FOREACH (it, mCharacters) {
        if (it->mDef.mChar && it->mMMesh && !it->m3DChars.empty()) {
            bool b1 = false;
            std::vector<ColorPalette *> colorPaletteList;
            it->mDef.mUseRandomColor = false;
            for (int i = 0; i < 3; i++) {
                ColorPalette *randPal = it->mDef.mChar->Find<ColorPalette>(
                    MakeString("random%d.pal", i + 1), false
                );
                if (randPal) {
                    colorPaletteList.push_back(randPal);
                    b1 = true;
                }
            }
            if (b1) {
                for (int i = 0; i != it->m3DChars.size(); i++) {
                    CharData::Char3D &curChar3D = it->m3DChars[i];
                    curChar3D.mRandomColors.clear();
                    MILO_ASSERT(!colorPaletteList.empty(), 0x5B8);
                    it->mDef.mUseRandomColor = true;
                    while (curChar3D.mRandomColors.size() < 3) {
                        ColorPalette *randPal =
                            colorPaletteList[RandomInt(0, colorPaletteList.size())];
                        Hmx::Color randColor =
                            randPal->GetColor(RandomInt(0, randPal->NumColors()));
                        curChar3D.mRandomColors.push_back(randColor);
                    }
                }
            }
        }
    }
}

void WorldCrowd::ListDrawChildren(std::list<RndDrawable *> &draws) {
    FOREACH (it, mCharacters) {
        Character *curChar = it->mDef.mChar;
        if (curChar)
            draws.push_back(curChar);
    }
}

void WorldCrowd::UpdateSphere() {
    Sphere s;
    MakeWorldSphere(s, true);
    SetSphere(s);
}

float WorldCrowd::GetDistanceToPlane(const Plane &p, Vector3 &vout) {
    if (mCharacters.empty())
        return 0;
    else {
        float dist = 0;
        bool b1 = true;
        FOREACH (it, mCharacters) {
            RndMultiMesh *multimesh = it->mMMesh;
            if (multimesh) {
                Vector3 v4c;
                float f5 = multimesh->GetDistanceToPlane(p, v4c);
                if (b1 || (std::fabs(f5) < std::fabs(dist))) {
                    b1 = false;
                    vout = v4c;
                    dist = f5;
                }
            }
        }
        return dist;
    }
}

bool WorldCrowd::MakeWorldSphere(Sphere &s, bool b) {
    if (b) {
        s.Zero();
        FOREACH (it, mCharacters) {
            RndMultiMesh *multimesh = it->mMMesh;
            if (multimesh) {
                Sphere local;
                multimesh->MakeWorldSphere(local, true);
                s.GrowToContain(local);
            }
        }
        return true;
    } else if (mSphere.GetRadius()) {
        s = mSphere;
        return true;
    } else
        return false;
}

void WorldCrowd::ListPollChildren(std::list<RndPollable *> &polls) const {
    FOREACH (it, mCharacters) {
        Character *curChar = it->mDef.mChar;
        if (curChar)
            polls.push_back(curChar);
    }
}

void WorldCrowd::Poll() {
    if (Showing()) {
        FOREACH (it, mCharacters) {
            Character *curChar = it->mDef.mChar;
            if (curChar && curChar->GetPollState() != 3) {
                curChar->Poll();
            }
        }
    }
}

void WorldCrowd::Enter() {
    RndPollable::Enter();
    FOREACH (it, mCharacters) {
        CharDef &def = it->mDef;
        Character *curChar = def.mChar;
        if (curChar) {
            if (curChar->GetPollState() != 2)
                curChar->Enter();
            for (int i = 0; i < 3; i++) {
                ColorPalette *randPal =
                    curChar->Find<ColorPalette>(MakeString("random%d.pal", i + 1), false);
                if (!randPal || randPal->NumColors() == 0)
                    break;
                if (i == 0) {
                    for (ObjDirItr<RndMat> objIt(curChar, true); objIt; ++objIt) {
                        def.mMats.push_back(objIt);
                    }
                }
            }
        }
    }
}

void WorldCrowd::Exit() {
    RndPollable::Exit();
    FOREACH (it, mCharacters) {
        Character *curChar = it->mDef.mChar;
        if (curChar)
            curChar->Exit();
    }
}

void WorldCrowd::Mats(std::list<RndMat *> &mats, bool additive) {
    if (additive) {
        MatShaderOptions opts;
        unsigned short shaderTypes[2] = {0xd, 0x13};

        opts.pack |= 0x20;

        for (unsigned int p = 0; p < 2; p++) {
            opts.SetLast5(shaderTypes[p]);
            for (int envIdx = 0; envIdx < 2; envIdx++) {
                bool useEnv = (envIdx != 0);
                for (int aoIdx = 0; aoIdx < 2; aoIdx++) {
                    RndMat *mat = Hmx::Object::New<RndMat>();
                    mat->Copy(gImpostorMat, Hmx::Object::kCopyDeep);
                    mat->SetUseEnv(useEnv);
                    opts.mTempMat = true;
                    opts.shader_struct.mHasAOCalc = 0;
                    opts.shader_struct.mHasAOCalc = aoIdx;
                    mat->SetShaderOpts(opts);
                    mats.insert(mats.end(), mat);
                }
            }
        }

        int i = 0;
        std::vector<Hmx::Color> colors;
        Hmx::Color white(1.0f, 1.0f, 1.0f, 1.0f);
        for (; i < 3; i++) {
            colors.push_back(white);
        }

        for (int colorIdx = 0; colorIdx <= 3; colorIdx++) {
            if (colorIdx == 2) continue;

            for (std::list<CharData>::iterator charIt = mCharacters.begin();
                 charIt != mCharacters.end(); ++charIt) {
                if (charIt->mDef.mUseRandomColor) {
                    SetMatColorFlags(charIt->mDef.mMats, (RndMat::ColorModFlags)colorIdx, &colors);
                }

                for (ObjPtrList<RndMat>::iterator matIt = charIt->mDef.mMats.begin();
                     matIt != charIt->mDef.mMats.end(); ++matIt) {
                    std::list<unsigned int> flags;
                    GetMeshShaderFlags(*matIt, flags);
                    for (std::list<unsigned int>::iterator flagIt = flags.begin();
                         flagIt != flags.end(); ++flagIt) {
                        unsigned int flag = *flagIt;
                        opts.pack = 0x12;
                        opts.SetHasBones(flag & 1);
                        opts.SetHasAOCalc((flag >> 1) & 1);
                        RndMat *newMat = Hmx::Object::New<RndMat>();
                        newMat->Copy(*matIt, Hmx::Object::kCopyDeep);
                        opts.mTempMat = true;
                        newMat->SetShaderOpts(opts);
                        mats.insert(mats.end(), newMat);
                    }
                }
            }
        }
    }
}

WorldCrowd::CharData::CharData(Hmx::Object *o) : mDef(o), mMMesh(0) {}

void WorldCrowd::CharData::Load(BinStream &bs) { bs >> mDef; }

WorldCrowd::CharDef::CharDef(Hmx::Object *o)
    : mChar(o), mHeight(75.0f), mDensity(1.0f), mRadius(10.0f), mUseRandomColor(0),
      mMats(o) {}

void WorldCrowd::CharDef::Load(BinStream &bs) {
    bs >> mChar;
    bs >> mHeight;
    bs >> mDensity;
    if (WorldCrowd::gRev > 1)
        bs >> mRadius;
    if (WorldCrowd::gRev > 8)
        bs >> mUseRandomColor;
}

BEGIN_HANDLERS(WorldCrowd)
    HANDLE(rebuild, OnRebuild)
    HANDLE_ACTION(assign_random_colors, AssignRandomColors())
    HANDLE(iterate_frac, OnIterateFrac)
    HANDLE_ACTION(set_fullness, SetFullness(_msg->Float(2), _msg->Float(3)))
    HANDLE_SUPERCLASS(RndDrawable)
    HANDLE_SUPERCLASS(RndPollable)
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0x6FF)
END_HANDLERS

void WorldCrowd::Force3DCrowd(bool force) {
    mForce3DCrowd = force;
    if (mForce3DCrowd)
        Set3DCharAll();
    else {
        SetFullness(1, 1);
        Set3DCharList(std::vector<std::pair<int, int> >(), this);
    }
}

DataNode WorldCrowd::OnIterateFrac(DataArray *da) {
    START_AUTO_TIMER("crowd_iter");
    Character *chars[64];
    if (mCharacters.empty()) {
        return DataNode(0);
    }

    // Gather non-null character pointers.
    int count = 0;
    for (std::list<CharData>::iterator it = mCharacters.begin();
         it != mCharacters.end(); ++it) {
        Character *c = it->mDef.mChar.mPtr;
        if (c) {
            chars[count++] = c;
        }
    }

    // Fisher-Yates shuffle.
    for (int i = count - 1; i > 0; i--) {
        int j = RandomInt() % (i + 1);
        Character *tmp = chars[i];
        chars[i] = chars[j];
        chars[j] = tmp;
    }

    // Sum positive fractions from the script's sub-arrays.
    float totalWeight = 0.0f;
    for (int i = 2; i < ((const DataArray *)da)->Size(); i++) {
        const DataArray *sub = ((const DataArray *)da)->Node(i).Array(da);
        float frac = sub->Node(0).Float(sub);
        if (frac > 0.0f) {
            totalWeight += frac;
        }
    }

    // Dispatch each sub-script to a fraction of the shuffled characters.
    float threshold = -0.5f;
    float charsPerWeight = (float)count / totalWeight;
    int charIdx = 0;
    for (int i = 2; i < ((const DataArray *)da)->Size(); i++) {
        const DataArray *sub = ((const DataArray *)da)->Node(i).Array(da);
        threshold += charsPerWeight * sub->Node(0).Float(sub);
        DataArray *script = (DataArray *)sub;
        while ((float)charIdx < threshold) {
            script->ExecuteScript(1, chars[charIdx], 0, 1);
            charIdx++;
        }
    }

    return DataNode(0);
}

void WorldCrowd::CleanUpCrowdFloor() {
    Hmx::Object *miloObj = ObjectDir::Main()->FindObject("milo", false);
    if (!miloObj) {
        WiiMesh *mesh = dynamic_cast<WiiMesh *>(mPlacementMesh.Ptr());
        if (mesh)
            mesh->RemoveVertData();
        else
            MILO_WARN("WorldCrowd[%s] does not have a placement mesh.", PathName(this));
    }
}

BEGIN_CUSTOM_PROPSYNC(WorldCrowd::CharData)
    SYNC_PROP(character, o.mDef.mChar)
    SYNC_PROP(height, o.mDef.mHeight)
    SYNC_PROP(density, o.mDef.mDensity)
    SYNC_PROP(radius, o.mDef.mRadius)
END_CUSTOM_PROPSYNC

BEGIN_PROPSYNCS(WorldCrowd)
    gParent = this;
    SYNC_PROP(num, mNum)
    SYNC_PROP(placement_mesh, mPlacementMesh)
    SYNC_PROP(characters, mCharacters)
    SYNC_PROP(show_3d_only, mShow3DOnly)
    SYNC_PROP_STATIC(environ, mEnviron)
    SYNC_PROP(environ_3d, mEnviron3D)
    SYNC_PROP_SET(lod, mLod, SetLod(_val.Int()))
    SYNC_PROP_SET(force_3D_crowd, mForce3DCrowd, Force3DCrowd(_val.Int()))
    SYNC_PROP(focus, mFocus)
    SYNC_SUPERCLASS(RndDrawable)
END_PROPSYNCS
