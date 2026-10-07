#include "world/SpotlightDrawer.h"
#include "char/Character.h"
#include "decomp.h"
#include "math/Color.h"
#include "math/Sphere.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "os/System.h"
#include "rndobj/BoxMap.h"
#include "rndobj/Cam.h"
#include "rndobj/Draw.h"
#include "rndobj/Env.h"
#include "rndobj/Flare.h"
#include "rndobj/Mesh.h"
#include "rndobj/MultiMesh.h"
#include "rndobj/Rnd.h"
#include "rndobj/Stats_NG.h"
#include "utl/Std.h"
#include "utl/Symbols.h"
#include "world/Spotlight.h"

RndEnviron *SpotlightDrawer::sEnviron;
RndMat *SpotlightDrawer::sEditorMat;
SpotlightDrawer *SpotlightDrawer::sCurrent;
SpotlightDrawer *SpotlightDrawer::sDefault;
std::vector<SpotlightDrawer::SpotlightEntry> SpotlightDrawer::sLights;
std::vector<SpotlightDrawer::SpotMeshEntry> SpotlightDrawer::sCans;
std::vector<class Spotlight *> SpotlightDrawer::sShadowSpots;
int SpotlightDrawer::sNeedBoxMap = -1;
bool SpotlightDrawer::sNeedDraw;
bool SpotlightDrawer::sHaveAdditionals;
bool SpotlightDrawer::sHaveLenses;
bool SpotlightDrawer::sHaveFlares;
bool SpotlightDrawer::sNoBeams;

DECOMP_FORCEACTIVE(SpotlightDrawer, "%s drawn after SpotlightEnder")

SpotlightDrawer::SpotlightDrawer() : mParams(this) { SetOrder(-100000.f); }

SpotlightDrawer::~SpotlightDrawer() {
    if (sCurrent == this) {
        DeSelect();
        ClearAndShrink(sLights);
        ClearAndShrink(sShadowSpots);
        ClearAndShrink(sCans);
    }
}

void SpotlightDrawer::RemoveFromLists(Spotlight *spot) {
    for (std::vector<SpotlightDrawer::SpotlightEntry>::iterator it = sLights.begin();
         it != sLights.end();) {
        if (it->unk4 == spot) {
            it = sLights.erase(it);
        } else
            ++it;
    }
    for (std::vector<SpotlightDrawer::SpotMeshEntry>::iterator it = sCans.begin();
         it != sCans.end();) {
        if (it->unk8 == spot) {
            it = sCans.erase(it);
        } else
            ++it;
    }
    for (std::vector<class Spotlight *>::iterator it = sShadowSpots.begin();
         it != sShadowSpots.end();) {
        if (*it == spot) {
            it = sShadowSpots.erase(it);
        } else
            ++it;
    }
}

void SpotlightDrawer::ListDrawChildren(std::list<RndDrawable *> &draws) {
    draws.push_back(mParams.mProxy);
}

void SpotlightDrawer::SetAmbientColor(const Hmx::Color &c) {
    sEnviron->SetAmbientColor(c);
    sEnviron->Select(nullptr);
}

void SpotlightDrawer::DrawLight(Spotlight *sl) {
    if (!sl)
        return;

    const Hmx::Color& color = sl->mColorOwner->mColor;
    float intensity = sl->mColorOwner->mIntensity;

    float scaledR = intensity * color.red;
    float scaledG = intensity * color.green;
    float scaledB = color.blue * intensity;
    float scaledA = 1.0f;

    unsigned int packedColor =
        (((int)(scaledB * 255.0f) & 0xFF) << 16)
        | (((int)(scaledG * 255.0f) & 0xFF) << 8)
        | ((int)(scaledR * 255.0f) & 0xFF);

    unsigned char byteR = (unsigned char)packedColor;
    unsigned char byteB = (unsigned char)(packedColor >> 16);
    unsigned char byteG = (unsigned char)(packedColor >> 8);
    bool shouldProcess = (byteR > 5u) || (byteG > 3u) || (byteB > 7u);

    if (shouldProcess && sl->unk286 && sl->Showing()) {
        if (GetGfxMode() == kOldGfx && sl->GetTarget() && sl->GetCastShadow()) {
            sShadowSpots.push_back(sl);
        }

        SpotlightEntry entry;
        entry.unk0 = packedColor;
        entry.unk4 = sl;
        sLights.push_back(entry);

        bool haveAdd = true;
        if (!sHaveAdditionals && sl->mAdditionalObjects.mSize <= 0) {
            haveAdd = false;
        }
        sHaveAdditionals = haveAdd;

        bool haveFlares = true;
        if (!sHaveFlares) {
            bool hasFlare = sl->mFlareEnabled && sl->mFlare;
            if (!hasFlare) {
                haveFlares = false;
            }
        }
        sHaveFlares = haveFlares;

        bool haveLenses = true;
        if (!sHaveLenses && !sl->LensMesh()) {
            haveLenses = false;
        }
        sHaveLenses = haveLenses;

        if ((unsigned int)sNeedBoxMap == (unsigned int)TheRnd->GetFrameID()) {
            MILO_NOTIFY_ONCE("%s drawn after SpotlightEnder", PathName(sl));
        }

        sNeedDraw = true;
    }

    RndMesh *mesh = sl->mLightCanMesh.mPtr;
    if (mesh && !sl->mLightCanSort) {
        bool visible;
        if (!mesh->Showing()) {
            visible = false;
        } else {
            MILO_ASSERT(mesh, 0xB9);
            Sphere s = mesh->GetSphere();
            if (s.GetRadius() > 0.0f) {
                Multiply(s, sl->mLightCanXfm, s);
                visible = !(s > RndCam::sCurrent->mWorldFrustum);
            } else {
                visible = true;
            }
        }
        if (visible) {
            SpotMeshEntry meshEntry;
            meshEntry.unk0 = mesh;
            meshEntry.unk4 = (RndMesh *)RndEnviron::sCurrent;
            meshEntry.unk8 = sl;
            meshEntry.unkc = 0;
            meshEntry.unk10 = sl->mLightCanXfm;
            sCans.push_back(meshEntry);
            sNeedDraw = true;
        }
    }
}

void SpotlightDrawer::UpdateBoxMap() {
    if ((uint)sNeedBoxMap != TheRnd->GetFrameID()) {
        RndEnviron::sGlobalLighting.Clear();
        float lightingInf = mParams.mLightingInfluence;
        if (lightingInf > 0) {
            ApplyLightingApprox(RndEnviron::sGlobalLighting, lightingInf);
        }
        sNeedBoxMap = TheRnd->GetFrameID();
    }
}

void SpotlightDrawer::EndWorld() {
    if (TheRnd->ProcCmds() & kProcessChar) {
        UpdateBoxMap();
        if (sNeedDraw) {
            DrawWorld();
            ClearPostDraw();
        }
        if (TheRnd->DisablePP()) {
            ClearLights();
        }
        MILO_ASSERT(!sNeedDraw, 0x172);
    }
}

void SpotlightDrawer::OnGPHangRecover() {}

void SpotlightDrawer::Select() {
    if (sCurrent != this) {
        if (sCurrent) {
            TheRnd->UnregisterPostProcessor(sCurrent);
        }
        sCurrent = this;
        TheRnd->RegisterPostProcessor(this);
    }
    sNeedBoxMap = -1;
}

void SpotlightDrawer::DeSelect() {
    if (sCurrent != this)
        return;
    if (sDefault == this)
        return;
    sDefault->Select();
}

void SpotlightDrawer::Init() {
    sEnviron = Hmx::Object::New<RndEnviron>();
    sEnviron->SetUseApproxes(false);
    Register();
    sDefault = Hmx::Object::New<SpotlightDrawer>();
    sDefault->mParams.mLightingInfluence = 0;
    sDefault->Select();
}

void SpotlightDrawer::SortLights() {
    if (sLights.size() > 2) {
        std::sort(sLights.begin(), sLights.end(), ByColor());
    }
    if (sCans.size() > 2) {
        std::sort(sCans.begin(), sCans.end(), ByEnvMesh());
    }
}

template <class T>
void DrawAccessories(
    SpotlightDrawer::SpotlightEntry *const &, SpotlightDrawer::SpotlightEntry *const &
);

struct LensExtract {};

template <>
void DrawAccessories<LensExtract>(
    SpotlightDrawer::SpotlightEntry *const &spotBegin,
    SpotlightDrawer::SpotlightEntry *const &spotEnd
) {
    SpotlightDrawer::SpotlightEntry *it = spotBegin;
    RndMat *curMat = NULL;
    RndMesh *curDisk = NULL;
    RndMultiMesh *multiMesh = NULL;
    for (; it != spotEnd; ++it) {
        if (it->unk4->LensMesh() != NULL) {
            RndMesh *mesh = Spotlight::sDiskMesh;
            RndMultiMesh *nextMesh;
            if (mesh != curDisk) {
                nextMesh = mesh->CreateMultiMesh();
            } else {
                nextMesh = multiMesh;
            }
            const Transform &lensXfm = it->unk4->mLensXfm;
            bool visible;
            if (!mesh->Showing()) {
                visible = false;
            } else {
                MILO_ASSERT(mesh, 0xB9);
                Sphere sphere = mesh->GetSphere();
                if (sphere.radius > 0.0f) {
                    Multiply(sphere, lensXfm, sphere);
                    visible = !(sphere > RndCam::Current()->mWorldFrustum);
                } else {
                    visible = true;
                }
            }
            if (visible) {
                bool diskChanged = (curDisk != mesh);
                RndMat *lensMat = it->unk4->LensMesh();
                bool matChanged = (curMat != lensMat);
                if ((diskChanged || matChanged) && multiMesh != NULL
                    && !multiMesh->mInstances.empty()) {
                    multiMesh->DrawShowing();
                    multiMesh->mInstances.resize(0, RndMultiMesh::Instance());
                }
                if (diskChanged) {
                    curDisk = mesh;
                    nextMesh = mesh->CreateMultiMesh();
                }
                if (matChanged || diskChanged) {
                    curMat = lensMat;
                    curDisk->SetMat(lensMat);
                }
                RndMultiMesh::Instance inst(lensXfm);
                nextMesh->mInstances.insert(nextMesh->mInstances.end(), inst);
                multiMesh = nextMesh;
            }
        }
    }
    if (multiMesh != NULL && !multiMesh->mInstances.empty()) {
        multiMesh->DrawShowing();
        multiMesh->mInstances.resize(0, RndMultiMesh::Instance());
    }
}

#ifdef HX_NATIVE
#include "platform/SpotBeamHook.h"
#include "world/Dir.h"
#include <cstdio>
#include <cstdlib>

bool SpotlightDrawer::DrawNGSpotlights() {
    return (GetGfxMode() == kNewGfx || GetNativeSpotBeamRenderer())
        && TheLoadMgr.GetPlatform() != kPlatformPC;
}

namespace {
// The beams NgSpotlightDrawer::RenderBeams would draw this frame, for the
// engine's beam pass (platform/SpotBeamHook.h). Retail draws them in
// NgSpotlightDrawer::DoPost from sLights; the old-gfx drawer this build runs
// clears sLights at world end, before any post, so they are taken here.
std::vector<NativeSpotBeam> gNativeBeams;

void AddNativeBeam(Spotlight *sl) {
    Spotlight::BeamDef &def = sl->mBeam;
    RndMesh *mesh = def.mBeam;
    if (!mesh || !mesh->Showing())
        return;
    NativeSpotBeam b;
    memset(&b, 0, sizeof(b));
    b.mesh = mesh;
    b.xsection = def.mXSection.Ptr();
    const Transform &mx = mesh->WorldXfm();
    const Vector3 *rows[4] = { &mx.m.x, &mx.m.y, &mx.m.z, &mx.v };
    for (int r = 0; r < 4; r++) {
        b.meshXfm[r * 3 + 0] = rows[r]->x;
        b.meshXfm[r * 3 + 1] = rows[r]->y;
        b.meshXfm[r * 3 + 2] = rows[r]->z;
    }
    b.shape = def.mShape;
    // GetLightPosition: the spotlight's position plus the beam's local offset
    // in the spotlight's frame.
    const Transform &sx = sl->WorldXfm();
    Vector3 off;
    Multiply(mesh->LocalXfm().v, sx.m, off);
    b.lightPos[0] = sx.v.x + off.x;
    b.lightPos[1] = sx.v.y + off.y;
    b.lightPos[2] = sx.v.z + off.z;
    b.axis[0] = sx.m.y.x;
    b.axis[1] = sx.m.y.y;
    b.axis[2] = sx.m.y.z;
    b.sheetDir[0] = sx.m.z.x;
    b.sheetDir[1] = sx.m.z.y;
    b.sheetDir[2] = sx.m.z.z;
    Vector2 radii = def.NGRadii();
    b.ngRadii[0] = radii.x;
    b.ngRadii[1] = radii.y;
    b.topRadius = def.mTopRadius;
    b.length = def.mLength;
    b.brighten = def.mBrighten;
    // The colour owner's colour (packed here; floats on retail).
    const unsigned int packed = sl->Color().color;
    b.color[0] = (packed & 255) / 255.0f;
    b.color[1] = ((packed >> 8) & 255) / 255.0f;
    b.color[2] = ((packed >> 16) & 255) / 255.0f;
    b.color[3] = ((packed >> 24) & 255) / 255.0f;
    b.intensity = sl->Intensity();
    b.matColor[0] = b.matColor[1] = b.matColor[2] = b.matColor[3] = 1.0f;
    RndMat *mat = def.mMat;
    if (!sl->AnimateColorFromPreset() && mat) {
        const Hmx::Color &mc = mat->GetColor();
        b.matColor[0] = mc.red;
        b.matColor[1] = mc.green;
        b.matColor[2] = mc.blue;
        b.matColor[3] = mc.alpha;
    }
    gNativeBeams.push_back(b);
    if (getenv("RB3_SPOT_BEAM_LOG") && gNativeBeams.size() < 13)
        printf("[SpotBeam]  %s cone %d\n", sl->Name(), def.mIsCone);
}

// RB3_SPOT_BEAM_LOG=1: print each frame's beams and drawer parameters.
void LogNativeBeams(const NativeSpotBeamFrame &f, SpotlightDrawer *d) {
    static const bool sLog = getenv("RB3_SPOT_BEAM_LOG") != nullptr;
    static int sFrames = 0;
    if (!sLog || sFrames >= 3)
        return;
    sFrames++;
    printf(
        "[SpotBeam] drawer %s: intensity %g base %g smoke %g half %g texture %s proxy %s, %d beams\n",
        d->Name(), f.intensity, f.baseIntensity, f.smokeIntensity,
        d->mParams.mHalfDistance,
        d->mParams.mTexture ? d->mParams.mTexture->Name() : "-",
        d->mParams.mProxy ? d->mParams.mProxy->Name() : "-", (int)gNativeBeams.size()
    );
    RndCam *cam = (RndCam *)f.camera;
    for (int i = 0; i < (int)gNativeBeams.size(); i++) {
        const NativeSpotBeam &b = gNativeBeams[i];
        // Where the beam's two ends land on screen (0..1), and their depths.
        Vector3 top(b.lightPos[0], b.lightPos[1], b.lightPos[2]);
        Vector3 end(
            top.x + b.axis[0] * b.length, top.y + b.axis[1] * b.length,
            top.z + b.axis[2] * b.length
        );
        Vector2 st(-1, -1), se(-1, -1);
        float dt = cam ? cam->WorldToScreen(top, st) : 0;
        float de = cam ? cam->WorldToScreen(end, se) : 0;
        printf(
            "[SpotBeam]  screen top %.3f %.3f (depth %g) end %.3f %.3f (depth %g)\n", st.x,
            st.y, dt, se.x, se.y, de
        );
        printf(
            "[SpotBeam]  %s shape %d len %g radii %g %g top %g brighten %g color %g %g %g "
            "x %g mat %g %g %g pos %g %g %g axis %g %g %g mesh y %g %g %g xsection %s\n",
            ((RndMesh *)b.mesh)->Name(), b.shape, b.length, b.ngRadii[0], b.ngRadii[1],
            b.topRadius, b.brighten, b.color[0], b.color[1], b.color[2], b.intensity,
            b.matColor[0], b.matColor[1], b.matColor[2], b.lightPos[0], b.lightPos[1],
            b.lightPos[2], b.axis[0], b.axis[1], b.axis[2], b.meshXfm[3], b.meshXfm[4],
            b.meshXfm[5], b.xsection ? ((RndTex *)b.xsection)->Name() : "-"
        );
    }
}
}
#endif

void SpotlightDrawer::DrawWorld() {
    int numLights = sLights.size();
    if (numLights < TheNgStats->mMotionBlurs) {
        numLights = TheNgStats->mMotionBlurs;
    }
    TheNgStats->mMotionBlurs = numLights;
    if ((!sLights.empty() || !sCans.empty()) && Showing()) {
        SortLights();
        DrawMeshVec(sCans);
        sCans.resize(0);
        if (!sLights.empty()) {
            RndEnviron *cur = RndEnviron::sCurrent;
            Vector3 *pos = RndEnviron::CurrentPos();
            MILO_ASSERT(sEnviron->GetUseApprox() == false, 0x1EE);
            sEnviron->Select(nullptr);
            if (GetGfxMode() == kOldGfx) {
                DrawShadow();
            }
            SpotlightEntry *it = &sLights[0];
            SpotlightEntry *itEnd = it + sLights.size();
#ifdef HX_NATIVE
            NativeSpotBeamRenderer *nativeBeams = GetNativeSpotBeamRenderer();
            gNativeBeams.resize(0);
#endif
            while (it != itEnd) {
                SpotlightEntry *const e1 = it;
                Spotlight *spot = it->unk4;
                int packed = spot->Color().color;
                float r = (packed & 255) / 255.0f;
                float g = ((packed >> 8) & 255) / 255.0f;
                float b = ((packed >> 0x10) & 255) / 255.0f;
                float intensity = spot->Intensity();
                Hmx::Color sp10;
                sp10.red = r * intensity;
                sp10.alpha = 1.0f;
                sp10.green = g * intensity;
                sp10.blue = b * intensity;
                SpotlightEntry *e2 = it + 1;
                for (; e2 != itEnd && e2->unk0 == it->unk0; ++e2) {
                }
                SetAmbientColor(sp10);
                if (sHaveAdditionals) {
                    DrawAdditional(it, e2);
                }
                if (sHaveLenses) {
                    DrawAccessories<LensExtract>(e1, e2);
                }
                bool drawNG = false;
                if (GetGfxMode() == kNewGfx && TheLoadMgr.GetPlatform() != kPlatformPC) {
                    drawNG = true;
                }
#ifdef HX_NATIVE
                // The backend draws the beams (retail's NG post); see above.
                if (nativeBeams) {
                    drawNG = true;
                    if (!sNoBeams && TheRnd->DrawMode() != 4) {
                        for (SpotlightEntry *b = it; b != e2; ++b)
                            AddNativeBeam(b->unk4);
                    }
                }
#endif
                if (!drawNG && !sNoBeams && TheRnd->DrawMode() != 4) {
                    DrawBeams(it, e2);
                }
                if (sHaveFlares) {
                    DrawFlares(it, e2);
                }
                it = e2;
            }
#ifdef HX_NATIVE
            if (nativeBeams && !gNativeBeams.empty()) {
                // CheckCam: TheWorld's camera, else the current one.
                NativeSpotBeamFrame f;
                f.camera = TheWorld && TheWorld->GetCam() ? TheWorld->GetCam()
                                                          : RndCam::Current();
                f.intensity = mParams.mIntensity;
                f.baseIntensity = mParams.mBaseIntensity;
                f.smokeIntensity = mParams.mSmokeIntensity;
                f.fogTexture = mParams.mTexture.Ptr();
                f.hasProxy = mParams.mProxy.Ptr() != nullptr;
                LogNativeBeams(f, this);
                nativeBeams->SubmitSpotBeams(f, &gNativeBeams[0], (int)gNativeBeams.size());
            }
#endif
            if (cur) {
                cur->Select(pos);
            }
        }
    }
}

void SpotlightDrawer::ApplyLightingApprox(BoxMapLighting &boxMap, float f2) const {
    MILO_ASSERT(boxMap.NumQueuedLights() == 0, 0x21D);
    std::vector<SpotlightEntry>::iterator it = sLights.begin();
    std::vector<SpotlightEntry>::iterator itEnd = sLights.end();
    for (; it != itEnd; ++it) {
        Spotlight *curSpotlight = it->unk4;
        Transform &xfm = curSpotlight->WorldXfm();
        int packed = (int)curSpotlight->Color().color;
        float fr = (int)(packed & 0xFF) / 255.0f * f2;
        float fa = (int)((packed >> 24) & 0xFF) / 255.0f * f2;
        float fb = (int)((packed >> 16) & 0xFF) / 255.0f * f2;
        float fg = (int)((packed >> 8) & 0xFF) / 255.0f * f2;
        float intensity = curSpotlight->Intensity();
        float final_a = fa * intensity;
        float final_b = fb * intensity;
        float final_g = fg * intensity;
        float final_r = fr * intensity;
        BoxMapLighting::LightParams_Spot *params;
        if (!boxMap.ParamsAt(params))
            break;
        params->mLightPos = xfm.v;
        params->mDirection = xfm.m.y;
        params->mColor.red = final_r;
        params->mColor.green = final_g;
        params->mColor.blue = final_b;
        params->mColor.alpha = final_a;
        params->mRadiusTop = curSpotlight->mBeam.mTopRadius;
        params->mRadiusBottom = curSpotlight->mBeam.mBottomRadius * 2.0f;
        params->mRange = curSpotlight->mBeam.mLength * 2.0f;
        boxMap.CacheData(*params);
    }
}

void SpotlightDrawer::DrawMeshVec(std::vector<SpotMeshEntry> &entries) {
    if (entries.size() != 0) {
        std::vector<SpotMeshEntry>::iterator it = entries.begin();
        RndMesh *mesh = it->unk0;
        RndMultiMesh *multiMesh = mesh->CreateMultiMesh();
        multiMesh->mInstances.push_back(RndMultiMesh::Instance(it->unk10));
        RndEnviron *currEnv = (RndEnviron *)it->unk4;
        currEnv->Select(NULL);
        std::vector<SpotMeshEntry>::iterator itEnd = entries.end();
        for (++it; it != itEnd; ++it) {
            bool envChanged = ((RndEnviron *)it->unk4 != currEnv);
            bool meshChanged = (it->unk0 != mesh);
            if (envChanged || meshChanged) {
                multiMesh->DrawShowing();
                if (envChanged && currEnv) {
                    currEnv = (RndEnviron *)it->unk4;
                    currEnv->Select(NULL);
                }
                if (meshChanged) {
                    mesh = it->unk0;
                    multiMesh = mesh->CreateMultiMesh();
                }
            }
            multiMesh->mInstances.push_back(RndMultiMesh::Instance(it->unk10));
        }
        multiMesh->DrawShowing();
    }
}

#pragma push
#pragma auto_inline on
void SpotlightDrawer::DrawShowing() {
    if (sCurrent && sCurrent != sDefault && sCurrent != this) {
        MILO_NOTIFY_ONCE(
            "Drawing 2 spotlightdrawers in one frame, %s and %s",
            PathName(sCurrent),
            PathName(this)
        );
    } else
        Select();
}
#pragma pop

void SpotlightDrawer::DrawShadow() {
    std::vector<Spotlight *>::iterator it = sShadowSpots.begin();
    std::vector<Spotlight *>::iterator itEnd = sShadowSpots.end();
    for (; it != itEnd; ++it) {
        Spotlight *shadowSpot = *it;
        MILO_ASSERT(shadowSpot->GetTarget() && shadowSpot->GetCastShadow(), 0x29A);
        Character *theChar = dynamic_cast<Character *>(shadowSpot->GetTarget());
        if (theChar) {
            Vector3 v48(theChar->WorldXfm().v);
            v48.z += 1.5f;
            Plane p58;
            p58.Set(0.0f, 0.0f, 1.0f, -v48.z);
            theChar->DrawShadow(shadowSpot->WorldXfm(), p58);
        }
    }
}

void SpotlightDrawer::DrawAdditional(
    SpotlightDrawer::SpotlightEntry *spotIter,
    SpotlightDrawer::SpotlightEntry *const &spotEnd
) {
    MILO_ASSERT(spotIter != spotEnd, 0x2AF);
    for (; spotIter != spotEnd; ++spotIter) {
        Spotlight *sl = spotIter->unk4;
        FOREACH (it, sl->mAdditionalObjects) {
            RndDrawable *add = *it;
            MILO_ASSERT(add != sl, 0x2BA);
            if (add != sl)
                add->Draw();
        }
    }
}

void SpotlightDrawer::DrawLenses(
    SpotlightDrawer::SpotlightEntry *spotIter,
    SpotlightDrawer::SpotlightEntry *const &spotEnd
) {
    MILO_ASSERT(spotIter != spotEnd, 0x2C8);
    for (; spotIter != spotEnd; ++spotIter) {
        Spotlight *sl = spotIter->unk4;
        if (Spotlight::sDiskMesh) {
            MILO_ASSERT(sl->LensMesh(), 0x2D0);
            Spotlight::sDiskMesh->SetMat(sl->LensMesh());
            Spotlight::sDiskMesh->Draw();
        }
    }
}

void SpotlightDrawer::DrawBeams(
    SpotlightDrawer::SpotlightEntry *spotIter,
    SpotlightDrawer::SpotlightEntry *const &spotEnd
) {
    MILO_ASSERT(spotIter != spotEnd, 0x2DE);
    for (; spotIter != spotEnd; ++spotIter) {
        Spotlight *sl = spotIter->unk4;
        Spotlight::BeamDef &def = sl->mBeam;
        if (def.mBeam) {
            float f3 = 1;
            RndMat *mat = def.mBeam->Mat();
            if (mat) {
                f3 = mat->Alpha();
                mat->SetAlpha(f3 * 0.60f);
            }
            MILO_ASSERT(def.mBeam->Showing(), 0x307);
            def.mBeam->DrawShowing();
            if (mat) {
                mat->SetAlpha(f3);
            }
        }
    }
}

void SpotlightDrawer::DrawFlares(
    SpotlightDrawer::SpotlightEntry *spotIter,
    SpotlightDrawer::SpotlightEntry *const &spotEnd
) {
    MILO_ASSERT(spotIter != spotEnd, 0x31E);
    for (; spotIter != spotEnd; ++spotIter) {
        Spotlight *sl = spotIter->unk4;
        if (sl->GetFlare() && sl->GetFlare()->GetMat()) {
            sl->GetFlare()->Draw();
        }
    }
}

void SpotlightDrawer::ClearPostDraw() {
    ClearLights();
    sNeedDraw = false;
}

void SpotlightDrawer::ClearLights() {
    sLights.resize(0);
    sShadowSpots.resize(0);
    sCans.resize(0);
    sHaveAdditionals = false;
    sHaveLenses = false;
    sHaveFlares = false;
}

SAVE_OBJ(SpotlightDrawer, 0x344)

BEGIN_COPYS(SpotlightDrawer)
    COPY_SUPERCLASS(RndDrawable)
    CREATE_COPY(SpotlightDrawer)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mParams)
    END_COPYING_MEMBERS
END_COPYS

BEGIN_LOADS(SpotlightDrawer)
    int rev;
    bs >> rev;
    if (rev > 5)
        MILO_FAIL("DxSpotlightDrawer: not forward compatable!");
    else {
        if (rev > 0)
            LOAD_SUPERCLASS(RndDrawable)
        else
            LOAD_SUPERCLASS(Hmx::Object)
        SetOrder(-100000.f);
        mParams.Load(bs, rev);
    }
END_LOADS

SpotDrawParams::SpotDrawParams(SpotlightDrawer *owner)
    : mIntensity(1.0f), mColor(1.0f, 1.0f, 1.0f), mBaseIntensity(0.1f),
      mSmokeIntensity(0.5f), mHalfDistance(250.0f), mLightingInfluence(1.0f),
      mTexture(owner, 0), mProxy(owner, 0), mOwner(owner) {
    MILO_ASSERT(owner, 0x37C);
}

SpotDrawParams &SpotDrawParams::operator=(const SpotDrawParams &params) {
    mIntensity = params.mIntensity;
    mBaseIntensity = params.mBaseIntensity;
    mSmokeIntensity = params.mSmokeIntensity;
    mHalfDistance = params.mHalfDistance;
    mLightingInfluence = params.mLightingInfluence;
    mColor = params.mColor;
    mTexture = params.mTexture;
    mProxy = params.mProxy;
    return *this;
}

void SpotDrawParams::Load(BinStream &bs, int rev) {
    if (rev > 5)
        MILO_WARN("Can't load new Params");
    else {
        bs >> mIntensity;
        if (rev > 3) {
            bs >> mBaseIntensity >> mSmokeIntensity >> mHalfDistance;
        } else {
            float i, j, k, l;
            bs >> i >> j >> k >> l;
            if (k < 0.5f) {
                mSmokeIntensity = 0.5f;
                mBaseIntensity = 0.1f;
            } else {
                mBaseIntensity = 0.15f;
                mSmokeIntensity = 1.0f;
            }
        }
        bs >> mColor;
        if (rev < 4) {
            int a, b, c, d, e;
            bs >> a >> b >> c >> d >> e;
        }
        bs >> mTexture;
        bs >> mProxy;
        if (rev < 3) {
            bool b;
            bs >> b;
        }
        if (rev > 4)
            bs >> mLightingInfluence;
    }
}

BEGIN_HANDLERS(SpotlightDrawer)
    HANDLE_SUPERCLASS(RndDrawable)
    HANDLE_SUPERCLASS(Hmx::Object)
#ifdef HX_NATIVE
    HANDLE_ACTION(Symbol("select"), Select()) // `select` collides with POSIX select()
#else
    HANDLE_ACTION(select, Select())
#endif
    HANDLE_ACTION(deselect, DeSelect())
    HANDLE_CHECK(0x3E4)
END_HANDLERS

BEGIN_PROPSYNCS(SpotlightDrawer)
    SYNC_PROP(total, mParams.mIntensity)
    SYNC_PROP(base_intensity, mParams.mBaseIntensity)
    SYNC_PROP(smoke_intensity, mParams.mSmokeIntensity)
    SYNC_PROP(color, mParams.mColor)
    SYNC_PROP(proxy, mParams.mProxy)
    SYNC_PROP(light_influence, mParams.mLightingInfluence)
    SYNC_SUPERCLASS(RndDrawable)
END_PROPSYNCS