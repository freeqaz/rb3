#include "rndobj/CamAnim.h"
#include "decomp.h"
#include "obj/PropSync_p.h"
#include "rndobj/Utl.h"

INIT_REVS(RndCamAnim);

BEGIN_COPYS(RndCamAnim)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(RndAnimatable)
    CREATE_COPY(RndCamAnim)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mCam)
        if (ty == kCopyShallow || ty == kCopyFromMax && c->mKeysOwner != c) {
            COPY_MEMBER(mKeysOwner)
        } else {
            mKeysOwner = this;
            mFovKeys = c->mKeysOwner->mFovKeys;
        }
    END_COPYING_MEMBERS
END_COPYS

void RndCamAnim::Print() {
    TextStream &ts = TheDebug;
    ts << "   cam: " << mCam.Ptr() << "\n";
    ts << "   keysOwner: " << mKeysOwner.Ptr() << "\n";
    ts << "   fovKeys: " << mFovKeys << "\n";
}

SAVE_OBJ(RndCamAnim, 0x37);

void RndCamAnim::Load(BinStream &bs) {
    LOAD_REVS(bs);
    ASSERT_REVS(2, 0);
    if (gRev != 0)
        Hmx::Object::Load(bs);
    RndAnimatable::Load(bs);
    bs >> mCam >> mFovKeys >> mKeysOwner;
    if (gRev < 2) {
        for (Keys<float, float>::iterator it = mFovKeys.begin(); it != mFovKeys.end();
             it++) {
            it->value = ConvertFov(it->value, 0.75f);
        }
    }
    if (!mKeysOwner.Ptr())
        mKeysOwner = this;
}

DECOMP_FORCEACTIVE(CamAnim, "o")

void RndCamAnim::Replace(Hmx::Object *from, Hmx::Object *to) {
    Hmx::Object::Replace(from, to);
    if (mKeysOwner == from) {
        if (!to)
            mKeysOwner = this;
        else
            mKeysOwner = dynamic_cast<RndCamAnim *>(to)->mKeysOwner;
    }
}
__declspec(noinline) float _outline_YFov(RndCam* _obj) {
    return _obj->YFov();
}


// fn_805CE7DC
void RndCamAnim::SetFrame(float frame, float blend) {
    RndAnimatable::SetFrame(frame, blend);
    ObjPtr<RndCam> &_ref0 = mCam;
    if (_ref0) {
        if (!FovKeys().empty()) {
            float ref = _outline_YFov(_ref0);
            FovKeys().AtFrame(frame, ref);
            if (blend != 1.0f) {
                Interp(_outline_YFov(_ref0), ref, blend, ref);
            }
            float nearPlane = _ref0->NearPlane();
            float farPlane = _ref0->FarPlane();
            _ref0->SetFrustum(nearPlane, farPlane, ref, 1.0f);
        }
    }
}

float RndCamAnim::EndFrame() { return FovKeys().LastFrame(); }

// fn_805CE930
void RndCamAnim::SetKey(float frame) {
    if (mCam) {
        const float &val = mCam->YFov();
        FovKeys().Add(val, frame, true);
    }
}

RndCamAnim::~RndCamAnim() {}

RndCamAnim::RndCamAnim() : mCam(this, 0), mKeysOwner(this, this) {}

BEGIN_HANDLERS(RndCamAnim)
    HANDLE_SUPERCLASS(RndAnimatable)
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0xC5)
END_HANDLERS

BEGIN_PROPSYNCS(RndCamAnim)
    SYNC_SUPERCLASS(RndAnimatable)
END_PROPSYNCS
