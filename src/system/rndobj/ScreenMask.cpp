#include "ScreenMask.h"
#include "os/Debug.h"
#include "utl/Symbols.h"
#include "obj/PropSync_p.h"
#include "rndobj/Cam.h"
#include "rndobj/HiResScreen.h"
#include "rndobj/Rnd.h"
#include "rndobj/Tex.h"
#include <list>

int SCREENMASK_REV = 2;

RndScreenMask::RndScreenMask()
    : mMat(this, 0), mColor(1.0f, 1.0f, 1.0f, 1.0f), mRect(0.0f, 0.0f, 1.0f, 1.0f) {
    mUseCurrentRect = 0;
}

BEGIN_COPYS(RndScreenMask)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(RndDrawable)
    CREATE_COPY(RndScreenMask)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mMat)
        COPY_MEMBER(mColor)
        COPY_MEMBER(mRect)
        COPY_MEMBER(mUseCurrentRect)
    END_COPYING_MEMBERS
END_COPYS

SAVE_OBJ(RndScreenMask, 0x38)

void RndScreenMask::Load(BinStream &bs) {
    int rev;
    bs >> rev;
    if (rev > SCREENMASK_REV) {
        MILO_FAIL(
            "%s can't load new %s version %d > %d",
            PathName(this),
            ClassName(),
            rev,
            SCREENMASK_REV
        );
    }
    Hmx::Object::Load(bs);
    RndDrawable::Load(bs);
    bs >> mMat >> mColor;
    if (rev > 0) {
        bs >> mRect;
    }
    if (rev > 1) {
        bool userect_loaded;
        bs >> userect_loaded;
        mUseCurrentRect = userect_loaded;
    }
}

void RndScreenMask::DrawShowing() {
    if (TheRnd->DrawMode() != kDrawNormal)
        return;

    float width = (float)TheRnd->Width();
    float height = (float)TheRnd->Height();
    RndTex *targetTex = RndCam::sCurrent->TargetTex();
    if ((int)targetTex) {
        width = (float)targetTex->Width();
        height = (float)targetTex->Height();
    }

    if (!mUseCurrentRect && (int)targetTex) {
        RndCam *cur = RndCam::sCurrent;
        int isDefaultRect = 0;
        if (cur->mScreenRect.x == 0.0f && cur->mScreenRect.y == 0.0f &&
            cur->mScreenRect.w == 1.0f && cur->mScreenRect.h == 1.0f) {
            isDefaultRect = 1;
        }
        if (!isDefaultRect) {
            MILO_NOTIFY_ONCE(
                "%s: Overriding camera screen_rect not supported with render texture",
                Name()
            );
        }
    }

    if (!mUseCurrentRect && !RndCam::sCurrent->TargetTex()) {
        RndCam *cam = RndCam::sCurrent;
        TheRnd->DefaultCam()->Select();
        Hmx::Rect hiRes = TheHiResScreen.InvScreenRect();
        Hmx::Rect drawRect;
        float rhH = hiRes.h * mRect.h;
        float rhW = hiRes.w * mRect.w;
        float rhY = mRect.y * hiRes.h + hiRes.y;
        float rhX = mRect.x * hiRes.w + hiRes.x;
        drawRect.h = height * rhH;
        drawRect.w = width * rhW;
        drawRect.y = height * rhY;
        drawRect.x = width * rhX;
        TheRnd->DrawRect(drawRect, mColor, mMat, NULL, NULL);
        cam->Select();
    } else {
        Hmx::Rect hiRes = TheHiResScreen.InvScreenRect();
        Hmx::Rect drawRect;
        float rhH = hiRes.h * mRect.h;
        float rhW = hiRes.w * mRect.w;
        float rhY = mRect.y * hiRes.h + hiRes.y;
        float rhX = mRect.x * hiRes.w + hiRes.x;
        drawRect.h = height * rhH;
        drawRect.w = width * rhW;
        drawRect.y = height * rhY;
        drawRect.x = width * rhX;
        TheRnd->DrawRect(drawRect, mColor, mMat, NULL, NULL);
    }
}

BEGIN_HANDLERS(RndScreenMask)
    HANDLE_SUPERCLASS(RndDrawable)
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0xB0)
END_HANDLERS

BEGIN_PROPSYNCS(RndScreenMask)
    SYNC_PROP(mat, mMat)
    SYNC_PROP(color, mColor)
    SYNC_PROP(alpha, mColor.alpha)
    SYNC_PROP(screen_rect, mRect)
    static Symbol _s("use_cam_rect");
    if (sym == _s) {
        if (_op == kPropSet) {
            mUseCurrentRect = _val.Int() != 0;
        } else {
            _val = DataNode(mUseCurrentRect);
        }
        return true;
    }
    SYNC_SUPERCLASS(RndDrawable)
END_PROPSYNCS
