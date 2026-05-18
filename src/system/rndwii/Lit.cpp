#include "Lit.h"
#include "math/Color.h"
#include "math/Rot.h"
#include "revolution/gx/GXLight.h"
#include "revolution/gx/GXTypes.h"
#include "rndwii/Cam.h"
#include "rndwii/Rnd.h"
#include "decomp.h"
#include <cmath>

void WiiLight::Update(GXLightID lit) {
    Hmx::Color src_col = mColorOwner->mColor;
    src_col.alpha = 0;

    if (!mOnlyProjection) {
#ifdef MATCHING
        // Inline PSQ color packing (qr6 = u8 quantize format, set by InitGQR)
        register __vec2x32float__ rg_pair;
        register __vec2x32float__ ba_pair;
        register const Hmx::Color *_c = &src_col;
        GXColor gxc;
        register GXColor *_dst = &gxc;
        ASM_BLOCK(
            psq_l rg_pair, 0x0(_c), 0, 0
            psq_l ba_pair, 0x8(_c), 0, 0
            psq_st rg_pair, 0x0(_dst), 0, 6
            psq_st ba_pair, 0x2(_dst), 0, 6
        )
        GXInitLightColor(&mLight, gxc);
#else
        int c = MakeU32Color(src_col);
        GXInitLightColor(&mLight, *(GXColor *)&c);
#endif
    }

    if (unk_0x15C && !mOnlyProjection) {
        if (mType != kFakeSpot) {
            GXInitLightAttn(&mLight, Intensity(), 0, 0, 1, 1 / mRange, 0);
        } else {
            GXInitLightDistAttn(&mLight, mRange / 2, 0.3, GX_DA_STEEP);
            GXInitLightSpot(&mLight, GetLightFieldOfView() / 2, GX_SP_FLAT);
        }
        unk_0x15C = 0;
    }
    UpdatePosition();
    if (!mOnlyProjection) {
        GXLoadLightObjImm(&mLight, lit);
    }
}

void WiiLight::UpdatePosition() {
    Transform &t = WorldXfm();
    WiiCam *cam = (WiiCam *)RndCam::sCurrent;
    if (mType == kPoint) {
        Vector3 p;
        Multiply(t.v, cam->mWiiViewXfm, p);
        GXInitLightPos(&mLight, p.x, p.y, p.z);
    } else if (mType == kDirectional) {
        Vector3 d;
        Multiply(t.m.y, cam->mWiiViewXfm.m, d);
        float k = -1e18f;
        GXInitLightPos(&mLight, k * d.x, k * d.y, k * d.z);
    } else if (mType == kFakeSpot) {
        Vector3 dir;
        Vector3 pos = CalcAdjustedPos();
        Multiply(t.m.y, cam->mWiiViewXfm.m, dir);
        Multiply(pos, cam->mWiiViewXfm, pos);
        if (!mOnlyProjection) {
            GXInitLightPos(&mLight, pos.x, pos.y, pos.z);
            GXInitLightDir(&mLight, dir.x, dir.y, dir.z);
        }
    }
}

float WiiLight::GetLightFieldOfView() {
    Transform t = WorldXfm();

    // this nightmare spaghetti removes 90% of the regswaps. end mii
    float magic_bs = std::atan2(
        mBotRadius, mRange + mTopRadius / ((mBotRadius - mTopRadius) / mRange)
    );
    return (magic_bs * 2) * 180 / float(PI);
}

Vector3 WiiLight::CalcAdjustedPos() {
    Transform t = WorldXfm();
    float f = (mBotRadius - mTopRadius);
    Vector3 v(0, -(mTopRadius / (f / mRange)), 0);
    Multiply(v, t.m, v);
    Add(v, t.v, v);
    return v;
}
