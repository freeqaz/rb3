#include "world/FreeCamera.h"
#include "obj/Data.h"
#include "obj/Task.h"
#include "rndobj/Cam.h"
#include "rndobj/DOFProc.h"
#include "rndobj/Trans.h"
#include "math/Rot.h"
#include "math/Trig.h"
#include "world/Dir.h"
#include "os/Joypad.h"
#include "utl/Symbols.h"
#include <math.h>

FreeCamera::FreeCamera(WorldDir *dir, float f1, float f2, int i)
    : mParent(0), mFrozen(0), mPadNum(i), mRotateRate(f1), mSlewRate(f2),
      mUseParentRotateX(1), mUseParentRotateY(1), mUseParentRotateZ(1), mWorld(dir) {
    UpdateFromCamera();
}

FreeCamera::~FreeCamera() {}

void FreeCamera::UpdateFromCamera() {
    RndCam *cam = mWorld->GetCam();
    mFov = cam->YFov();
    mXfm = cam->WorldXfm();
    MakeEuler(mXfm.m, mRot);
    mParent = 0;
    mFocalPlane = TheDOFProc->FocalPlane();
}

void FreeCamera::SetParentDof(bool b1, bool b2, bool b3) {
    mUseParentRotateX = b1;
    mUseParentRotateY = b2;
    mUseParentRotateZ = b3;
}

void FreeCamera::Poll() {
    JoypadData *padData = JoypadGetPadData(mPadNum);
    if (!padData)
        return;

    float dt = TheTaskMgr.DeltaUISeconds();
    float deltaMs = dt * 1000.0f;
    if (mFrozen) {
        deltaMs = 0.0f;
    }

    // Apply left stick to rotation, inlining LimitAng as fmod
    float lx = padData->mSticks[0][0];
    float ly = padData->mSticks[0][1];
    float rotSpeed = mRotateRate * deltaMs;

    // LimitAng(mRot.z - rotSpeed * lx * |lx|)
    // rotXDelta is kept in f30 across the first fmod call
    float zDelta = rotSpeed * (-lx) * fabsf(lx);
    float lyPart = rotSpeed * ly;
    float rotXDelta = lyPart * fabsf(ly);
    float limZ = (float)fmod(mRot.z + zDelta + 3.14159265f, 2.0 * 3.14159265f);
    mRot.z = (limZ < 0.0f) ? limZ + 3.14159265f : limZ - 3.14159265f;

    // LimitAng(mRot.x + rotSpeed * ly * |ly|)
    float limX = (float)fmod(mRot.x + rotXDelta + 3.14159265f, 2.0 * 3.14159265f);
    mRot.x = (limX < 0.0f) ? limX + 3.14159265f : limX - 3.14159265f;

    // Rebuild rotation matrix
    MakeRotMatrix(mRot, mXfm.m, true);

    // Compute slew speed
    float slewSpeed = mSlewRate * deltaMs;
    bool isL2 = padData->mButtons & (1 << kPad_L2);
    if (isL2) {
        slewSpeed *= 0.1f;
    }

    float rx = padData->mSticks[1][0];
    float slewHalf = slewSpeed * 0.5f;
    float ry = padData->mSticks[1][1];
    float slewX = fabsf(rx * rx) * rx * slewHalf;
    float slewY = -fabsf(ry * ry) * slewSpeed * ry;

    // Move along X axis (strafe)
    mXfm.v.x += mXfm.m.x.x * slewX;
    mXfm.v.y += mXfm.m.x.y * slewX;
    mXfm.v.z += mXfm.m.x.z * slewX;

    // Move along Y (forward) or Z (up) depending on L1
    bool isL1 = padData->mButtons & (1 << kPad_L1);
    if (isL1) {
        // L1 pressed - move along Z axis (up/down)
        mXfm.v.x += mXfm.m.z.x * slewY;
        mXfm.v.y += mXfm.m.z.y * slewY;
        mXfm.v.z += mXfm.m.z.z * slewY;
    } else {
        // Move along Y axis (forward/back)
        mXfm.v.x += mXfm.m.y.x * slewY;
        mXfm.v.y += mXfm.m.y.y * slewY;
        mXfm.v.z += mXfm.m.y.z * slewY;
    }

    // Load mButtons before GetCam for register scheduling
    unsigned int dpadButtons = padData->mButtons;
    RndCam *cam = mWorld->GetCam();

    // FOV adjustment with D-pad Up/Down (bool materialization)
    bool isDUp = dpadButtons & (1 << kPad_DUp);
    if (isDUp) {
        mFov = mFov + 0.001f;
    } else {
        bool isDDown = dpadButtons & (1 << kPad_DDown);
        if (isDDown) {
            mFov = mFov - 0.001f;
        }
    }

    unsigned int buttons = padData->mButtons;
    bool isX = buttons & (1 << kPad_X);
    if (isX) {
        // X button - roll rotation
        bool isDLeft = buttons & (1 << kPad_DLeft);
        if (isDLeft) {
            mRot.y = deltaMs * 0.001f + mRot.y;
        } else {
            bool isDRight = buttons & (1 << kPad_DRight);
            if (isDRight) {
                mRot.y = -(deltaMs * 0.001f - mRot.y);
            }
        }
    } else {
        // Focal plane adjustment
        bool isDLeft = buttons & (1 << kPad_DLeft);
        if (isDLeft) {
            mFocalPlane = (float)((double)mFocalPlane / pow(2.0, deltaMs * 0.001f));
        } else {
            bool isDRight = buttons & (1 << kPad_DRight);
            if (isDRight) {
                mFocalPlane = (float)((double)mFocalPlane * pow(2.0, deltaMs * 0.001f));
            }
        }
    }

    // Apply parent transform
    Transform resultXfm;
    if (mParent) {
        if (!mUseParentRotateX || !mUseParentRotateY || !mUseParentRotateZ) {
            Hmx::Matrix3 parentRot(mParent->WorldXfm().m);
            Vector3 parentEuler(0.0f, 0.0f, 0.0f);
            MakeEuler(parentRot, parentEuler);
            if (!mUseParentRotateX) {
                parentEuler.x = mRot.x;
            }
            if (!mUseParentRotateY) {
                parentEuler.y = mRot.y;
            }
            if (!mUseParentRotateZ) {
                parentEuler.z = mRot.z;
            }
            Hmx::Matrix3 newRot;
            MakeRotMatrix(parentEuler, newRot, false);
            const Transform &parentWorld = mParent->WorldXfm();
            Transform parentXfm(newRot, parentWorld.v);
            Multiply(mXfm, parentXfm, resultXfm);
        } else {
            Multiply(mXfm, mParent->WorldXfm(), resultXfm);
        }
    } else {
        resultXfm = mXfm;
    }

    cam->SetFrustum(cam->NearPlane(), cam->FarPlane(), mFov, 1.0f);

    // If camera has a parent transform, convert to local space
    RndTransformable *camParent = cam->TransParent();
    if (camParent) {
        Transform invParent;
        Invert(camParent->WorldXfm(), invParent);
        Multiply(resultXfm, invParent, resultXfm);
    }

    cam->SetLocalXfm(resultXfm);

    // Handle DOF
    if (TheDOFProc->Enabled()) {
        TheDOFProc->Set(
            cam, mFocalPlane, TheDOFProc->BlurDepth(), TheDOFProc->MaxBlur(),
            TheDOFProc->MinBlur()
        );
    }
}

BEGIN_HANDLERS(FreeCamera)
    HANDLE_ACTION(set_parent, mParent = _msg->Obj<RndTransformable>(2))
    HANDLE_ACTION(set_pos, mXfm.v.Set(_msg->Float(2), _msg->Float(3), _msg->Float(4)))
    HANDLE_ACTION(
        set_rot,
        (mRot.Set(_msg->Float(2), _msg->Float(3), _msg->Float(4)),
         mRot.x *= DEG2RAD, mRot.y *= DEG2RAD, mRot.z *= DEG2RAD)
    )
    HANDLE_ACTION(set_parent_dof, SetParentDof(_msg->Int(2), _msg->Int(3), _msg->Int(4)))
    HANDLE_ACTION(set_frozen, mFrozen = _msg->Int(2))
    HANDLE_CHECK(0xDC)
END_HANDLERS
