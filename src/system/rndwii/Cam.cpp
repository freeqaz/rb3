#include "Cam.h"
#include "rndwii/Rnd.h"
#include "rndobj/Stats_NG.h"
#include "revolution/gx/GXTransform.h"

Transform WiiCam::sViewToWiiViewXfm(
    Hmx::Matrix3(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f), Vector3(0.0f, 0.0f, 0.0f));
Transform WiiCam::sWiiViewToViewXfm(
    Hmx::Matrix3(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f), Vector3(0.0f, 0.0f, 0.0f));

WiiCam::WiiCam() {}

void WiiCam::Select() {
    TheNgStats->mCams++;
    RndCam::Select();
    int ox = 0;
    int height;
    int width;
    int oy = 0;
    if (mTargetTex) {
        width = mTargetTex->mWidth;
        height = mTargetTex->mHeight;
        if (width <= 0x20) {
            ox = 0x260;
        }
    } else {
        if (TheWiiRnd.unk_0x2B0) {
            width = 0x260;
        } else {
            width = TheWiiRnd.mWidth;
        }
        height = TheWiiRnd.mHeight;
        if (TheWiiRnd.mAspect == Rnd::kLetterbox && !TheWiiRnd.unk_0x2B0) {
            float scaled = (9.0f * ((float)width * 0.0625f));
            float bar = (float)height - scaled;
            unsigned int barI;
            if (bar > 0.0f) {
                barI = (unsigned int)(0.5f * bar);
            } else {
                barI = 0;
            }
            oy = barI;
            height -= barI * 2;
        }
    }
    Multiply(mInvWorldXfm, sViewToWiiViewXfm, mWiiViewXfm);
    Mtx44 proj;
    memset(proj, 0, sizeof(proj));
    float var_f2;
    if (mYFov == 0.0f) {
        proj[3][3] = 1.0f;
        var_f2 = 1.0f / (mFarPlane - mNearPlane);
        proj[2][2] = var_f2;
    } else {
        proj[3][2] = -1.0f;
        var_f2 = mFarPlane / (mFarPlane - mNearPlane);
        proj[2][2] = 1.0f - var_f2;
    }
    proj[0][0] = mLocalProjectXfm.m.x.x * mScreenRect.w;
    proj[1][1] = -mLocalProjectXfm.m.z.y * mScreenRect.h;
    proj[2][3] = -var_f2 * mNearPlane;
    proj[0][3] = mLocalProjectXfm.v.x;
    proj[1][3] = -mLocalProjectXfm.v.y;
    GXSetProjection(
        proj, (mYFov == 0.0f) ? GX_ORTHOGRAPHIC : GX_PERSPECTIVE
    );
    float vw = mScreenRect.w * (float)width;
    float vh = mScreenRect.h * (float)height;
    float vx = mScreenRect.x * (float)width + (float)ox;
    float vy = mScreenRect.y * (float)height + (float)oy;
    GXSetViewport(vx, vy, vw, vh, mZRange.x, mZRange.y);
    GXSetScissor((u32)vx, (u32)vy, (u32)vw, (u32)vh);
}

u32 WiiCam::ProjectZ(float f1) {
    float near = mNearPlane;
    float far = mFarPlane;
    float zx = mZRange.x;
    float f3 = near - far;
    float zy = mZRange.y;
    float f5 = near / f3;
    float f2 = zy - zx;
    f3 = f1 * f5 - f5 * far;
    f3 = f3 / f1;
    f1 = f3 * f2 + zx;
    return 16777215 * f1;
}
