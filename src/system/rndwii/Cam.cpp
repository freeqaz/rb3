#include "Cam.h"

Transform WiiCam::sViewToWiiViewXfm(
    Hmx::Matrix3(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f), Vector3(0.0f, 0.0f, 0.0f));
Transform WiiCam::sWiiViewToViewXfm(
    Hmx::Matrix3(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f), Vector3(0.0f, 0.0f, 0.0f));

WiiCam::WiiCam() {}

void WiiCam::Select() {}

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
