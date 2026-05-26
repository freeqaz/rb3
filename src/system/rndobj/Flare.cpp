#include "rndobj/Flare.h"
#include "math/Rot.h"
#include "obj/ObjMacros.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "rndobj/Cam.h"
#include "rndobj/Draw.h"
#include "rndobj/Env.h"
#include "rndobj/HiResScreen.h"
#include "rndobj/Mat.h"
#include "rndobj/Rnd.h"
#include "rndobj/Trans.h"
#include "rndobj/Utl.h"
#include "utl/Symbols.h"
#include "utl/Symbols4.h"
#include "utl/TextStream.h"

INIT_REVS(RndFlare)

BEGIN_COPYS(RndFlare)
    CREATE_COPY_AS(RndFlare, f)
    MILO_ASSERT(f, 25);
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(RndTransformable)
    COPY_SUPERCLASS(RndDrawable)
    COPY_MEMBER_FROM(f, mSizes)
    COPY_MEMBER_FROM(f, mMat)
    COPY_MEMBER_FROM(f, mVisible)
    COPY_MEMBER_FROM(f, mRange)
    COPY_MEMBER_FROM(f, mOffset)
    COPY_MEMBER_FROM(f, mSteps)
    COPY_MEMBER_FROM(f, mPointTest)
    mLastDone = false;
    mTestDone = mLastDone;
END_COPYS

void RndFlare::Print() {
    TextStream &ts = TheDebug;
    ts << "   mat: " << mMat << "\n";
    ts << "   sizes: " << mSizes << "\n";
    ts << "   range: " << mRange << "\n";
    ts << "   offset:" << mOffset << "\n";
    ts << "   steps: " << mSteps << "\n";
    ts << "   point test: " << mPointTest << "\n";
}

SAVE_OBJ(RndFlare, 60)

BEGIN_LOADS(RndFlare)
    LOAD_REVS(bs)
    ASSERT_REVS(7, 0)
    if (gRev > 3)
        LOAD_SUPERCLASS(Hmx::Object)
    LOAD_SUPERCLASS(RndTransformable)
    LOAD_SUPERCLASS(RndDrawable)
    if (gRev != 0) {
        bs >> mMat;
    }
    if (gRev > 2)
        bs >> mSizes;
    else {
        bs >> mSizes.x;
        mSizes.y = mSizes.x;
    }
    if (gRev > 1) {
        bs >> mRange >> mSteps;
    }
    if (gRev > 4) {
        bs >> mPointTest;
    }
    if (gRev > 6)
        bs >> mOffset;
    mLastDone = 0;
    mTestDone = mLastDone;
    CalcScale();
END_LOADS

RndFlare::RndFlare()
    : mPointTest(1), mAreaTest(1), mVisible(0), mSizes(0.1f, 0.1f), mMat(this),
      mRange(0, 0), mOffset(0), mSteps(1), mStep(0), unkec(0), unk114(1, 1) {
    mTestDone = 0;
    mLastDone = 0;
    mMatrix.Identity();
}

RndFlare::~RndFlare() {
    if (!gSuppressPointTest)
        MILO_FAIL("Async point tests not disabled while destroying flares!\n");
    TheRnd->RemovePointTest(this);
}

void RndFlare::CalcScale() {
    if (mMatrix != WorldXfm().m) {
        Vector3 v28;
        mMatrix = WorldXfm().m;
        float len = Length(mMatrix.z);
        Cross(mMatrix.x, mMatrix.y, v28);
        unk114.Set(Length(mMatrix.x), Dot(v28, mMatrix.z) > 0.0f ? len : -len);
    }
}

void RndFlare::SetPointTest(bool b) {
    if (!b && mPointTest)
        TheRnd->RemovePointTest(this);
    mPointTest = b;
}

void RndFlare::DrawShowing() {
    if (TheRnd->DrawMode() != 0)
        return;
    RndCam *cam = RndCam::sCurrent;

    Vector2 screenPos;
    float depth = cam->WorldToScreen(WorldXfm().v, screenPos);

    float scale;
    Hmx::Rect localRect = CalcRect(screenPos, scale);

    if (RectOffscreen(localRect) || depth <= 0.0f) {
        mStep = 0;
        unkec = 0.0f;
        mTestDone = false;
        mLastDone = mTestDone;
        return;
    }

    bool useOcc = false;
    if (mPointTest && !TheHiResScreen.IsActive()) {
        useOcc = !mLastDone && mTestDone;
        mLastDone = mTestDone;
        mTestDone = false;

        const Transform &camXfm = cam->WorldXfm();
        const Transform &flareXfm = WorldXfm();
        Vector3 dir;
        Subtract(camXfm.v, flareXfm.v, dir);
        Normalize(dir, dir);

        const Transform &flareXfm2 = WorldXfm();
        float offset = mOffset;
        dir.x = dir.x * offset + flareXfm2.v.x;
        dir.y = dir.y * offset + flareXfm2.v.y;
        dir.z = dir.z * offset + flareXfm2.v.z;
        TheRnd->TestPoint(dir, this);
    } else {
        unkec = scale;
        mVisible = true;
    }

    if (useOcc) {
        mStep = mVisible ? mSteps : 0;
    } else {
        int steps = mSteps;
        int newStep = (mStep + mVisible * 2) - 1;
        if (newStep <= steps) {
            steps = newStep & ~(newStep >> 31);
        }
        mStep = steps;
    }

    float ratio = (float)mStep / (float)mSteps;
    if (mAreaTest) {
        ratio *= unkec / (localRect.w * localRect.h);
    }

    if (ratio > 0.0f) {
        if (mMat) {
            float t;
            if (mRange.x != mRange.y) {
                t = (depth - mRange.y) / (mRange.x - mRange.y);
            } else {
                t = 1.0f;
            }
            float alpha = Clamp(0.0f, 1.0f, t * ratio);

            Hmx::Color col;
            col.red = alpha * 0.6f;
            col.green = alpha * 0.6f;
            col.blue = alpha * 0.6f;
            col.alpha = 1.0f;
            RndMat *mat = mMat;
            if (mat->mUseEnviron) {
                RndEnviron *env = RndEnviron::sCurrent;
                if (env) {
                    const Hmx::Color &ambColor = env->AmbientColor();
                    col.red *= ambColor.red;
                    col.green *= ambColor.green;
                    col.blue *= ambColor.blue;
                    col.alpha *= ambColor.alpha;
                }
            }
            mat->mColor.red = col.red;
            mat->mColor.green = col.green;
            mat->mColor.blue = col.blue;
            mat->mDirty |= 1;
        }

        if (mMat && mMat->mTexGen == kTexGenXfm) {
            Transform texMat;
            texMat = mMat->TexXfm();
            float angle = screenPos.x - 0.5f;
            float c = Cosine(angle);
            float s = Sine(angle);
            texMat.m.x.x = c;
            texMat.m.x.y = s;
            texMat.m.x.z = 0.0f;
            texMat.m.y.x = -s;
            texMat.m.y.y = c;
            texMat.m.y.z = 0.0f;
            texMat.m.z.x = 0.0f;
            texMat.m.z.y = 0.0f;
            texMat.m.z.z = 1.0f;
            RndMat *m = mMat;
            m->mTexXfm = texMat;
            m->mDirty |= 2;
        }

        Hmx::Rect *drawRect;
        if (mAreaTest) {
            drawRect = &mArea;
        } else {
            drawRect = &CalcRect(screenPos, scale);
        }
        Hmx::Rect rect = *drawRect;
        Hmx::Color white(1.0f, 1.0f, 1.0f, 1.0f);
        TheRnd->DrawRect(rect, white, mMat, NULL, NULL);
    }
}

Hmx::Rect &RndFlare::CalcRect(Vector2 &vref, float &fref) {
    float flareSize = mSizes.x;
    if (flareSize != mSizes.y) {
        RndCam *cam = RndCam::sCurrent;
        float dot = Dot(cam->WorldXfm().m.y, WorldXfm().m.y);
        float blend = Max(0.0f, -dot);
        flareSize = mSizes.x + blend * (mSizes.y - mSizes.x);
    }
    int width = TheRnd->Width();
    int height = TheRnd->Height();
    if (TheHiResScreen.IsActive()) {
        int tiling = TheHiResScreen.mTiling;
        width *= tiling;
        height *= tiling;
        int paddingX = TheHiResScreen.GetPaddingX();
        width -= tiling * paddingX;
        tiling = TheHiResScreen.mTiling;
        int paddingY = TheHiResScreen.GetPaddingY();
        height -= tiling * paddingY;
        Hmx::Rect screenRect = TheHiResScreen.ScreenRect();
        vref.x -= screenRect.x;
        vref.y -= screenRect.y;
    }
    CalcScale();
    flareSize *= width;
    mArea.w = flareSize * unk114.x;
    mArea.h = (height * (flareSize * unk114.y)) / (width * TheRnd->YRatio());
    mArea.x = vref.x * width - mArea.w * 0.5f;
    mArea.y = vref.y * height - mArea.h * 0.5f;
    float f1 = Min<float>(width, mArea.x + mArea.w) - Max(0.0f, mArea.x);
    float f2 = Min<float>(height, mArea.y + mArea.h) - Max(0.0f, mArea.y);
    fref = f1 * f2;
    return mArea;
}

bool RndFlare::RectOffscreen(const Hmx::Rect &r) const {
    if (r.x + r.w < 0)
        return true;
    else if (r.y + r.h < 0)
        return true;
    else if (r.x > TheRnd->Width())
        return true;
    else if (r.y > TheRnd->Height())
        return true;
    else
        return false;
}

void RndFlare::Mats(std::list<RndMat *> &list, bool) {
    if (mMat) {
        mMat->mShaderOptions = GetDefaultMatShaderOpts(this, mMat);
        list.push_back(mMat);
    }
}

void RndFlare::SetMat(RndMat *m) { mMat = m; }

void RndFlare::SetSteps(int i1) {
    int max = Max(1, i1);
    if (mStep == mSteps) {
        mStep = max;
    } else {
        float maxFloat = (float)max;
        float stepsFloat = (float)mSteps;
        mStep = (int)(maxFloat / stepsFloat) * mStep;
    }
    mSteps = max;
}

BEGIN_HANDLERS(RndFlare)
    HANDLE_ACTION(set_steps, SetSteps(_msg->Int(2)))
    HANDLE_ACTION(set_point_test, SetPointTest(_msg->Int(2)))
    HANDLE_SUPERCLASS(RndTransformable)
    HANDLE_SUPERCLASS(RndDrawable)
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(373)
END_HANDLERS

BEGIN_PROPSYNCS(RndFlare)
    SYNC_PROP(mat, mMat)
    SYNC_PROP(sizes, mSizes)
    SYNC_PROP(steps, mSteps)
    SYNC_PROP(range, mRange)
    SYNC_PROP(offset, mOffset)
    SYNC_PROP_MODIFY(point_test, mPointTest, TheRnd->RemovePointTest(this))
    SYNC_SUPERCLASS(RndTransformable)
    SYNC_SUPERCLASS(RndDrawable)
END_PROPSYNCS
