#include "rndobj/BoxMap.h"
#include "decomp.h"
#include "os/Timer.h"
#include "rndobj/Lit.h"

Vector3 BoxMapLighting::sAxisDir[6] = { Vector3(1, 0, 0), Vector3(-1, 0, 0),
                                        Vector3(0, 1, 0), Vector3(0, -1, 0),
                                        Vector3(0, 0, 1), Vector3(0, 0, -1) };

BoxMapLighting::BoxMapLighting() { Clear(); }

void BoxMapLighting::Clear() {
    mQueued_Directional.Clear();
    mQueued_Point.Clear();
    mQueued_Spot.Clear();
}

bool BoxMapLighting::QueueLight(RndLight *light, float colorScale) {
    if (light->Showing()) {
        Hmx::Color lightColor(light->GetColor());
        lightColor.red *= colorScale;
        lightColor.green *= colorScale;
        lightColor.blue *= colorScale;
        switch (light->GetType()) {
        case RndLight::kDirectional:
        case RndLight::kFakeSpot:
            LightParams_Directional *paramsDirectional;
            if (ParamsAt(paramsDirectional)) {
                paramsDirectional->mColor = lightColor;
                Negate(light->WorldXfm().m.y, paramsDirectional->mDirection);
                return true;
            }
            break;
        case RndLight::kPoint:
            LightParams_Point *paramsPoint;
            if (ParamsAt(paramsPoint)) {
                paramsPoint->unk0 = light->WorldXfm().v;
                paramsPoint->mColor = lightColor;
                paramsPoint->mRange = light->Range();
                paramsPoint->mFalloffStart = light->FalloffStart();
                return true;
            }
            break;
        default:
            break;
        }
    }
    return false;
}

UNPOOL_DATA
void BoxMapLighting::ApplyQueuedLights(Hmx::Color *color, const Vector3 *v3) const {
    START_AUTO_TIMER("light_approx_poll");
    {
        START_AUTO_TIMER("light_approx_dir");
        const LightParams_Directional *pd = mQueued_Directional.mArray;
        for (unsigned int i = 0; i < mQueued_Directional.NumElements(); i++, pd++) {
            ApplyLight(color, *pd);
        }
    }
    if (v3) {
        {
            START_AUTO_TIMER("light_approx_point");
            const LightParams_Point *pp = mQueued_Point.mArray;
            for (unsigned int i = 0; i < mQueued_Point.NumElements(); i++, pp++) {
                ApplyLight(color, *pp, *v3);
            }
        }
        {
            START_AUTO_TIMER("light_approx_spot");
            if (mQueued_Spot.NumElements() != 0) {
                ApplyLight(color, mQueued_Spot, *v3);
            }
        }
    }
}
END_UNPOOL_DATA

bool BoxMapLighting::CacheData(BoxMapLighting::LightParams_Spot &spot) {
    float beamLen = spot.unk44;
    if (beamLen > 0) {
        float topR = spot.unk48;
        float botR = spot.unk4c;
        if (botR >= topR
            && (spot.mColor.red > 0.003921569f || spot.mColor.green > 0.003921569f
                || spot.mColor.blue > 0.003921569f)) {
            float f1 = (topR * beamLen) / (botR - topR);
            float vy = spot.unk0.y * f1;
            float vz = spot.unk0.z * f1;
            float f2 = botR / (beamLen + f1);
            f2 *= f2;
            float vx = spot.unk0.x * f1;
            float f3 = 1.0f / (beamLen * 2.0f);
            f2 = (1.0f - f2) / (f2 + 1.0f);
            spot.unk30 = f3;
            spot.unk1c.x = spot.unk38.x - vx;
            spot.unk28 = f2;
            spot.unk1c.z = spot.unk38.z - vz;
            spot.unk1c.y = spot.unk38.y - vy;
            spot.unk2c = 1.0f / (1.0f - f2);
            spot.unk34 = f1 * f3;
            return true;
        }
    }
    mQueued_Spot.RemoveEntry();
    return 0;
}