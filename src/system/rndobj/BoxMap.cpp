#include "rndobj/BoxMap.h"
#include "decomp.h"
#include "math/Utl.h"
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
                paramsPoint->mLightPos = light->WorldXfm().v;
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
    float beamLen = spot.mRange;
    if (beamLen > 0) {
        float topR = spot.mRadiusTop;
        float botR = spot.mRadiusBottom;
        if (botR >= topR
            && (spot.mColor.red > 0.003921569f || spot.mColor.green > 0.003921569f
                || spot.mColor.blue > 0.003921569f)) {
            float halfLenRecip = 1.0f / (beamLen * 2.0f);
            float tipOff = (topR * beamLen) / (botR - topR);
            float ratio = botR / (beamLen + tipOff);
            ratio *= ratio;
            spot.mOneOverRange2x = halfLenRecip;
            float oneMinusRatio = 1.0f - ratio;
            float vz = spot.mDirection.z * tipOff;
            float onePlusRatio = 1.0f + ratio;
            float vx = spot.mDirection.x * tipOff;
            float vy = spot.mDirection.y * tipOff;
            spot.mTipPosition.x = spot.mLightPos.x - vx;
            float cosTheta = oneMinusRatio / onePlusRatio;
            spot.mCosTheta = cosTheta;
            float oneSubCos = 1.0f - cosTheta;
            spot.mTipPosition.z = spot.mLightPos.z - vz;
            spot.mTipPosition.y = spot.mLightPos.y - vy;
            spot.mOneOverOneSubCos = 1.0f / oneSubCos;
            spot.mTipOverRange2x = tipOff * halfLenRecip;
            return true;
        }
    }
    mQueued_Spot.RemoveEntry();
    return false;
}

// In the Bank 8 target all three ApplyLight overloads share one hand-written
// paired-single asm kernel (psq_l/ps_merge/ps_sel/ps_madds0|1 over lane pairs
// {dot(+axis,d), dot(-axis,d)}, two faces per step, alpha lanes used as
// scratch), inlined into each body. Bank 5 DWARF shows the original source
// switched at runtime (g_testApplyLightWiiAsm) between a scalar C path
// (fWeight_XP_Sqr..fRes_ZN_B locals) and the Wii asm path; Bank 8 kept only
// the asm. mwcc has no paired-single intrinsics that could express the
// merge10/11, sel, madds0/1, or qr1 forms from C++, so these bodies cannot
// match without transcribing the asm; we keep the portable scalar algorithm.
void BoxMapLighting::ApplyLight(Hmx::Color *color, const LightParams_Directional &light)
    const {
    for (int i = 0; i < 6; i++) {
        float d = Max(0.0f, Dot(sAxisDir[i], light.mDirection));
        d *= d;
        color[i].red += d * light.mColor.red;
        color[i].green += d * light.mColor.green;
        color[i].blue += d * light.mColor.blue;
    }
}

void BoxMapLighting::ApplyLight(
    Hmx::Color *color, const LightParams_Point &light, const Vector3 &viewPos
) const {
    if (light.mRange > light.mFalloffStart) {
        Vector3 d;
        d.y = light.mLightPos.y - viewPos.y;
        d.x = light.mLightPos.x - viewPos.x;
        d.z = light.mLightPos.z - viewPos.z;
        float distSq = d.y * d.y + d.x * d.x + d.z * d.z;
        if (distSq > 0.0f) {
            float dist = std::sqrt(distSq);
            float invDist = 1.0f / dist;
            float fade = Max(0.0f, dist - light.mFalloffStart);
            float atten = Max(0.0f, 1.0f - fade / (light.mRange - light.mFalloffStart));
            LightParams_Directional dl;
            dl.mDirection.x = d.x * invDist;
            dl.mDirection.y = d.y * invDist;
            dl.mDirection.z = d.z * invDist;
            dl.mColor.red = light.mColor.red * atten;
            dl.mColor.green = light.mColor.green * atten;
            dl.mColor.blue = light.mColor.blue * atten;
            dl.mColor.alpha = 1.0f;
            ApplyLight(color, dl);
        }
    }
}

void BoxMapLighting::ApplyLight(
    Hmx::Color *color, const BoxLightArray<LightParams_Spot, 50> &arr,
    const Vector3 &viewPos
) const {
    // Target pool constant @F_295c8f3e = 0x3e8f5c29 = 0.28f: the spot loop's
    // skip threshold really is 0.28, unlike CacheData's per-channel 1/255
    // (@F_8180803b = 0x3b808081) which stays 0.003921569f above.
    static const float kColorEpsilon = 0.28f;
    unsigned int count = arr.NumElements();
    const LightParams_Spot *light = arr.mArray;
    for (unsigned int i = 0; i < count; i++, light++) {
        if (light->mColor.red + light->mColor.green + light->mColor.blue
            < kColorEpsilon)
            continue;
        float dx = viewPos.x - light->mTipPosition.x;
        float dy = viewPos.y - light->mTipPosition.y;
        float dz = viewPos.z - light->mTipPosition.z;
        float distSq = dx * dx + dy * dy + dz * dz;
        float invDist = 1.0f / std::sqrt(distSq);
        float nx = dx * invDist;
        float ny = dy * invDist;
        float nz = dz * invDist;
        float distNorm =
            distSq * invDist * light->mOneOverRange2x - light->mTipOverRange2x;
        float coneDot = nx * light->mDirection.x + ny * light->mDirection.y
            + nz * light->mDirection.z;
        float distClamped = Min(1.0f, distNorm);
        float coneClamped = Min(1.0f, coneDot) - light->mCosTheta;
        float distAtten = Max(0.0f, 1.0f - distClamped);
        float coneAtten = Max(0.0f, coneClamped);
        float atten = distAtten * coneAtten * light->mOneOverOneSubCos;
        LightParams_Directional dl;
        dl.mDirection.x = -nx;
        dl.mDirection.y = -ny;
        dl.mDirection.z = -nz;
        dl.mColor.red = atten * light->mColor.red;
        dl.mColor.green = atten * light->mColor.green;
        dl.mColor.blue = atten * light->mColor.blue;
        ApplyLight(color, dl);
    }
}