#include "rndobj/ShadowMap.h"
#include "math/Mtx.h"
#include "math/Rot.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "os/System.h"
#include "rndobj/Cam.h"
#include "rndobj/Lit.h"
#include "rndobj/Rnd.h"
#include "rndobj/Tex.h"
#include <cmath>

RndCam *RndShadowMap::sLightCam;
RndTex *RndShadowMap::sShadowTex;

void RndShadowMap::EndShadow() {
    TheRnd->SetShadowMap(0, 0, 0);
}

bool RndShadowMap::PrepShadow(RndDrawable *draw, RndEnviron *env) {
    if (GetGfxMode() != kNewGfx) return false;
    if (sLightCam == NULL || sShadowTex == NULL) return false;

    RndEnviron *e = env != NULL ? env : RndEnviron::sCurrent;

    RndLight *light = NULL;
    ObjPtrList<RndLight>::iterator it;
    for (it = e->mLightsReal.begin(); it != e->mLightsReal.end(); ++it) {
        if ((*it)->GetType() == RndLight::kFloorSpot) {
            light = *it;
            break;
        }
    }
    if (light)
        goto found;

    for (it = e->mLightsReal.begin(); it != e->mLightsReal.end(); ++it) {
        if ((*it)->GetType() == RndLight::kDirectional || (*it)->GetType() == RndLight::kPoint) {
            light = *it;
            goto found;
        }
    }

    return false;

found:
    Sphere sphere;
    RndCam *curCam = RndCam::sCurrent;
    if (!draw->MakeWorldSphere(sphere, false)) {
        MILO_NOTIFY_ONCE(
            "Can't self-shadow %s; MakeWorldSphere failed.", PathName(draw)
        );
        return false;
    }

    float yFov = PI / 4.0f;
    Transform lightXfm;
    lightXfm.m = light->WorldXfm().m;
    lightXfm.v = sphere.center;

    if (light->GetType() == RndLight::kPoint) {
        const Transform &lw = light->WorldXfm();
        lightXfm.m.y.x = sphere.center.x - lw.v.x;
        lightXfm.m.y.z = sphere.center.z - lw.v.z;
        lightXfm.m.y.y = sphere.center.y - lw.v.y;
        Normalize(lightXfm.m, lightXfm.m);
    }

    float radius = sphere.radius;
    float dist = radius / (float)std::tan(0.5f * yFov);

    Vector3 offset;
    Multiply(Vector3(0.0f, -dist, 0.0f), lightXfm.m, offset);
    Add(lightXfm.v, offset, lightXfm.v);

    sLightCam->SetWorldXfm(lightXfm);
    sLightCam->SetFrustum(dist - radius, dist + radius, yFov, 1.0f);
    sLightCam->Select();

    Mode oldMode = TheRnd->DrawMode();
    TheRnd->SetDrawMode(kDrawExtrude);
    draw->DrawShowing();
    TheRnd->SetDrawMode(oldMode);

    curCam->Select();

    static Hmx::Color defaultColor(0.0f, 0.0f, 0.0f, 0.0f);
    const Hmx::Color *shadowColor = &defaultColor;
    if (light->GetType() == RndLight::kFloorSpot) {
        shadowColor = &light->GetColor();
    }

    Hmx::Color invertedColor = *shadowColor;
    invertedColor.red = 1.0f - invertedColor.red;
    invertedColor.green = 1.0f - invertedColor.green;
    invertedColor.blue = 1.0f - invertedColor.blue;

    TheRnd->SetShadowMap(sShadowTex, sLightCam, &invertedColor);
    return true;
}
