#include "Mat.h"
#include "decomp.h"
#include "math/Color.h"
#include "math/Mtx.h"
#include "math/Rot.h"
#include "obj/ObjMacros.h"
#include "os/Debug.h"
#include "os/Timer.h"
#include "revolution/gx/GXAttr.h"
#include "revolution/gx/GXBump.h"
#include "revolution/gx/GXGeometry.h"
#include "revolution/gx/GXLight.h"
#include "revolution/gx/GXPixel.h"
#include "revolution/gx/GXTev.h"
#include "revolution/gx/GXTransform.h"
#include "revolution/gx/GXTypes.h"
#include "revolution/mtx/mtx.h"
#include "revolution/os/OSError.h"
#include "rndobj/Cam.h"
#include "rndobj/Env.h"
#include "rndobj/Stats_NG.h"
#include "rndobj/Utl.h"
#include "rndobj/Mat.h"
#include "rndwii/Cam.h"
#include "rndwii/Env.h"
#include "rndwii/Rnd.h"
#include "utl/Loader.h"

int DbgGetFrameID();

bool WiiMat::sOverrideAlphaWrite = 0;
bool WiiMat::sCurrentZCompLoc = 1;
bool bDoMatLightHackBS = 1;

WiiMat *WiiMat::sCurrent = nullptr;
Transform *WiiMat::sCurrentModelXfm = nullptr;
RndTex *WiiMat::sGradientTex = nullptr;

int StageId(int i) {
    MILO_ASSERT(0 <= i && i < 16, 45);
    GXSetTevDirect((GXTevStageID)i);
    return i;
}

int TexCoordId(int i) {
    MILO_ASSERT(0 <= i && i < 8, 58);
    return i;
}

int TexMapId(int i) {
    MILO_ASSERT(0 <= i && i < 8, 64);
    return i;
}

int TexMtx(int i) {
    static const int op[8] = { 0x24, 0x27, 0x2a, 0x2d, 0x30, 0x33, 0x36, 0x39 };
    MILO_ASSERT(0 <= i && i < 8, 70);
    return op[i];
}

int PTTexMtx(int i) {
    MILO_ASSERT(0 <= i && i < 20, 89);
    return i * 3 + 64;
}

void WiiMat::SetCurrentModelTransform(const Transform *t) {
    sCurrentModelXfm = const_cast<Transform *>(t);
}

void WiiMat::PreInit() {
    Register();
    delete sGradientTex;
    sGradientTex = nullptr;
    sGradientTex = Hmx::Object::New<RndTex>();
    sGradientTex->SetBitmap(
        FilePath(MakeString("%s/world/gradient64x64_keep.bmp", FileSystemRoot()))
    );
}

void WiiMat::Init() {}

void WiiMat::SelectParticles() {
    if (this == sCurrent && !mDirty) {
        return;
    }
    START_AUTO_TIMER("mat_select_part");
}

RndMat *WiiMat::Select(bool hasAOCalc) {
    if (this == sCurrent && !mDirty)
        return nullptr;
    bool fadeResult = false;
    TheNgStats->mMats++;
    START_AUTO_TIMER("mat_select");
    Reset();
    RndEnviron *env = RndEnviron::sCurrent;
    RndCam *cam = RndCam::sCurrent;
    GXColor zeroCol = { 0, 0, 0, 0 };
    int numLightChannels = 0;
#ifdef VERSION_SZBE69_B8
    if (TheLoadMgr.EditMode()) {
        GXSetCullMode(GX_CULL_NONE);
    } else {
        GXSetCullMode((GXCullMode)mCull);
    }
#else
    GXSetCullMode((GXCullMode)mCull);
#endif
    Hmx::Color ambCol = env != nullptr ? env->AmbientColor() : Hmx::Color(0, 0, 0);
    Hmx::Color diffuseCol = mColor;
    if (mBlend == kPreMultAlpha) {
        PreMultiplyAlpha(diffuseCol);
    }
    if (mUseEnviron) {
        diffuseCol.alpha *= RndEnviron::sCurrent->AmbientColor().alpha;
    }
#ifdef MATCHING
    GXColor matGxc;
    GXColor ambGxc;
    {
        register __vec2x32float__ ba_pair;
        register __vec2x32float__ rg_pair;
        register const Hmx::Color *_c = &diffuseCol;
        register GXColor *_dst = &matGxc;
        ASM_BLOCK(
            psq_l ba_pair, 0x8(_c), 0, 0
            psq_l rg_pair, 0x0(_c), 0, 0
            psq_st rg_pair, 0x0(_dst), 0, 6
            psq_st ba_pair, 0x2(_dst), 0, 6
        )
    }
    {
        register __vec2x32float__ ba_pair;
        register __vec2x32float__ rg_pair;
        register const Hmx::Color *_c = &ambCol;
        register GXColor *_dst = &ambGxc;
        ASM_BLOCK(
            psq_l ba_pair, 0x8(_c), 0, 0
            psq_l rg_pair, 0x0(_c), 0, 0
            psq_st rg_pair, 0x0(_dst), 0, 6
            psq_st ba_pair, 0x2(_dst), 0, 6
        )
    }
    GXSetChanMatColor(GX_COLOR0A0, matGxc);
    GXSetChanMatColor(GX_COLOR1A1, matGxc);
    GXSetChanAmbColor(GX_COLOR0A0, ambGxc);
    GXSetChanAmbColor(GX_COLOR1A1, ambGxc);
#else
    int matPacked = MakeU32Color(diffuseCol);
    GXSetChanMatColor(GX_COLOR0A0, *(GXColor *)&matPacked);
    GXSetChanMatColor(GX_COLOR1A1, *(GXColor *)&matPacked);
    int ambPacked = MakeU32Color(ambCol);
    GXSetChanAmbColor(GX_COLOR0A0, *(GXColor *)&ambPacked);
    GXSetChanAmbColor(GX_COLOR1A1, *(GXColor *)&ambPacked);
#endif
    if (!mUseEnviron && !mPreLit) {
        numLightChannels = 1;
        GXSetNumChans(1);
        GXSetChanCtrl(GX_COLOR0A0, 0, GX_SRC_REG, GX_SRC_REG, GX_LIGHT_NULL, GX_DF_NONE, GX_AF_NONE);
        GXSetChanCtrl(GX_COLOR1A1, 0, GX_SRC_REG, GX_SRC_VTX, GX_LIGHT_NULL, GX_DF_NONE, GX_AF_NONE);
    } else if (env != NULL) {
        WiiEnviron *wiiEnv = (WiiEnviron *)env;
        bool allDirectional = wiiEnv->unk_0x19C == 0;
        int lightIds = wiiEnv->unk_0x19E;
        if (mPreLit) {
            numLightChannels = 2;
            GXSetNumChans(2);
            GXSetChanCtrl(GX_COLOR0, (GXBool)mUseEnviron, GX_SRC_REG, GX_SRC_VTX, GX_LIGHT_NULL, GX_DF_NONE, GX_AF_NONE);
            GXSetChanAmbColor(GX_COLOR1A1, zeroCol);
            GXSetChanCtrl(GX_COLOR1, 0, GX_SRC_REG, GX_SRC_REG, GX_LIGHT_NULL, GX_DF_NONE, GX_AF_NONE);
            GXSetChanCtrl(GX_ALPHA0, 0, GX_SRC_REG, GX_SRC_VTX, GX_LIGHT_NULL, GX_DF_NONE, GX_AF_NONE);
            GXSetChanCtrl(GX_ALPHA1, 0, GX_SRC_REG, GX_SRC_REG, GX_LIGHT_NULL, GX_DF_NONE, GX_AF_NONE);
        } else {
            if (gRecoveringThisFrame) {
                lightIds = 0;
            }
            MILO_ASSERT(((unsigned int)lightIds) < ((unsigned int)GX_MAX_LIGHT), 0x1A1);
            if (lightIds == 0xFF && bDoMatLightHackBS) {
                lightIds = 0x7F;
            }
            if (hasAOCalc) {
                numLightChannels = 2;
                GXSetNumChans(2);
                GXSetChanCtrl(GX_COLOR0A0, 0, GX_SRC_REG, GX_SRC_VTX, GX_LIGHT_NULL, GX_DF_NONE, GX_AF_NONE);
                if (mUseEnviron) {
                    GXSetChanCtrl(GX_COLOR1, 1, GX_SRC_REG, GX_SRC_REG, (GXLightID)lightIds, GX_DF_CLAMP, allDirectional ? GX_AF_SPOT : GX_AF_NONE);
                } else {
                    GXSetChanCtrl(GX_COLOR1, 0, GX_SRC_REG, GX_SRC_REG, GX_LIGHT_NULL, GX_DF_CLAMP, GX_AF_NONE);
                }
                GXSetChanCtrl(GX_ALPHA1, 0, GX_SRC_REG, GX_SRC_REG, GX_LIGHT_NULL, GX_DF_NONE, GX_AF_NONE);
            } else {
                numLightChannels = 1;
                GXSetNumChans(1);
                GXSetChanCtrl(GX_COLOR0, 1, GX_SRC_REG, GX_SRC_REG, (GXLightID)lightIds, GX_DF_CLAMP, allDirectional ? GX_AF_SPOT : GX_AF_NONE);
                GXSetChanCtrl(GX_ALPHA0, 0, GX_SRC_REG, GX_SRC_REG, GX_LIGHT_NULL, GX_DF_NONE, GX_AF_NONE);
                GXSetChanCtrl(GX_COLOR1A1, 0, GX_SRC_REG, GX_SRC_VTX, GX_LIGHT_NULL, GX_DF_NONE, GX_AF_NONE);
            }
        }
    }
    SetAlphaCutout(mAlphaCut, mAlphaThresh);
    SetZBufferMode(mZMode);
    Blend b = mBlend;
    SetFrameBlend(b);
    bool doFog = false;
    if (mFog && ((unsigned int)b > (unsigned int)kBlendSubtract || !((1 << b) & 0x35))) {
        doFog = true;
    }
    SetFog(doFog, env, cam);
    int tex = 0;
    int tev = 0;
    int stage = 0;
    SetStageState(tex, tev, stage, mIntensify, numLightChannels);
    if (!TheRnd->DisablePP()) {
        if (mBlend != kBlendSrc && mBlend != kBlendAdd) {
            fadeResult = SetFade(tex, tev, stage, env, cam);
        }
        SetColorXfm(tev, env);
    }
    GXSetDstAlpha(0, 0);
    if (sOverrideAlphaWrite) {
        GXSetAlphaUpdate(1);
    } else {
        GXSetAlphaUpdate((GXBool)mAlphaWrite);
    }
    GXSetNumTexGens((u8)tex);
    GXSetNumTevStages((u8)tev);
    sCurrent = this;
    if (fadeResult && Refs().size() > 1) {
        mDirty = 2;
    } else {
        mDirty = 0;
    }
    if (gbDbgRequestForcedHang) {
        static int firstFrame = -1;
        int curFrame = DbgGetFrameID();
        if (firstFrame < 0) {
            firstFrame = curFrame;
            OSReport("GPHangDebug: forcing gp hang.\n");
        }
        if (curFrame - firstFrame < 1) {
            GXSetNumChans(2);
            GXSetChanCtrl(GX_COLOR0A0, 1, GX_SRC_REG, GX_SRC_REG, (GXLightID)0xFF, GX_DF_CLAMP, GX_AF_SPOT);
            GXSetChanCtrl(GX_COLOR1A1, 1, GX_SRC_REG, GX_SRC_REG, (GXLightID)0xFF, GX_DF_CLAMP, GX_AF_SPOT);
        } else {
            firstFrame = -1;
            gbDbgRequestForcedHang = false;
        }
    }
    return mNextPass;
}

void WiiMat::SetAlphaCutout(bool b, int i) {
    if (b) {
        GXSetAlphaCompare(GX_GREATER, i, GX_AOP_OR, GX_NEVER, 0);
    } else {
        GXSetAlphaCompare(GX_ALWAYS, 0, GX_AOP_OR, GX_ALWAYS, 0);
    }
    if (b != sCurrentZCompLoc) {
        if (b != 0) {
            GXSetZCompLoc(false);
        } else {
            GXSetZCompLoc(true);
        }
        sCurrentZCompLoc = b;
    }
}

void WiiMat::SetZBufferMode(ZMode z) {
    switch (z) {
    case kZModeDisable:
        GXSetZMode(0, GX_NEVER, 0);
        return;
    case kZModeNormal:
        GXSetZMode(1, GX_LESS, 1);
        return;
    case kZModeTransparent:
        GXSetZMode(1, GX_LEQUAL, 0);
        return;
    case kZModeForce:
        GXSetZMode(1, GX_ALWAYS, 1);
        return;
    case kZModeDecal:
        GXSetZMode(1, GX_LEQUAL, 1);
        return;
    default:
        return;
    }
}

void WiiMat::SetFrameBlend(Blend b) {
    switch (b) {
    case RndMat::kBlendDest:
        GXSetBlendMode(GX_BM_BLEND, GX_BL_ZERO, GX_BL_ONE, GX_LO_NOOP);
        break;
    case RndMat::kBlendSrc:
        GXSetBlendMode(GX_BM_BLEND, GX_BL_ONE, GX_BL_ZERO, GX_LO_NOOP);
        break;
    case RndMat::kBlendAdd:
        GXSetBlendMode(GX_BM_BLEND, GX_BL_ONE, GX_BL_ONE, GX_LO_NOOP);
        break;
    case RndMat::kBlendSrcAlpha:
        GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_NOOP);
        break;
    case RndMat::kBlendSrcAlphaAdd:
        GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_ONE, GX_LO_NOOP);
        break;
    case RndMat::kBlendSubtract:
        GXSetBlendMode(GX_BM_SUBTRACT, GX_BL_ONE, GX_BL_ONE, GX_LO_NOOP);
        break;
    case RndMat::kBlendMultiply:
        GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCCLR, GX_BL_ZERO, GX_LO_NOOP);
        break;
    case RndMat::kPreMultAlpha:
        GXSetBlendMode(GX_BM_BLEND, GX_BL_ONE, GX_BL_INVSRCALPHA, GX_LO_NOOP);
        break;
    default:
        MILO_WARN("Invalid frame blend mode: %d\n", b);
        GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_NOOP);
    }
}
#ifdef VERSION_SZBE69_B8
#pragma push
#pragma dont_inline on
#endif
void WiiMat::SetTexGen(GXTexCoordID tcid, GXTexMtx mtx) {
    Transform t1, t2;
    t1.Zero();
    Vector3 v;
    Mtx m;
    if (!unk_0xAD_5) {
        switch (mTexGen) {
        case kTexGenXfm:
        case kTexGenXfmOrigin:
            if (unk_0xAD_7) {
                Transform t;
                t.Reset();
                MakeWiiMtxTex(t, mTexGen == kTexGenXfm, m);
                GXLoadTexMtxImm(m, mtx, GX_MTX_2x4);
                break;
            } else {
                MakeWiiMtxTex(mTexXfm, mTexGen == kTexGenXfm, m);
                GXLoadTexMtxImm(m, mtx, GX_MTX_2x4);
                break;
            }
            break;
        case kTexGenProjected:
            FastInvert(mTexXfm, t1);
            if (sCurrentModelXfm != nullptr) {
                Multiply(*sCurrentModelXfm, t1, t1);
            }
            t2.v.Zero();
            t2.m.Set(1, 0, 0, 0, 0, 1, 0, -1, 0);
            Multiply(t1, t2, t1);
            MakeWiiMtx(t1, m);
            GXLoadTexMtxImm(m, mtx, GX_MTX_2x4);
            break;
        case kTexGenEnviron:
            Transpose(mTexXfm.m, t1.m);
            Multiply(RndCam::Current()->WorldXfm().m, t1.m, t1.m);
            Hmx::Matrix3 m2(1, 0, 0, 0, 0, 1, 0, 1, 0);
            Multiply(m2, t1.m, t1.m);
            m2.x.x = 0.5;
            m2.z.y = -0.5;
            Multiply(t1.m, m2, t1.m);
            t1.v.Set(0.5, 0.5, 0);
            MakeWiiMtx(t1, m);
            GXLoadTexMtxImm(m, mtx, GX_MTX_3x4);
            break;
        case kTexGenSphere:
            Transpose(mTexXfm.m, t1.m);
            Multiply(RndCam::Current()->WorldXfm().m, t1.m, t1.m);
            // Vector3 v;
            MakeEuler(t1.m, v);
            v.x = InterpAng(0, v.x, 0.5);
            v.z = InterpAng(0, v.z, 0.5);
            MakeRotMatrix(v, t1.m, true);
            t2.v.Zero();
            Transpose(RndCam::Current()->WorldXfm().m, t2.m);
            Multiply(t2.m, t1.m, t1.m);
            t2.m.Set(0.5, 0, 0, 0, 0, 1, 0, -0.5, 0);
            Multiply(t1.m, t2.m, t1.m);
            t1.v.Set(0.5, 0.5, 0);
            MakeWiiMtx(t1, m);
            GXLoadTexMtxImm(m, mtx, GX_MTX_2x4);
            break;
        default:
            break;
        }
        unk_0xAD_5 = true;
    }
    switch (mTexGen) {
    case kTexGenXfm:
    case kTexGenXfmOrigin:
        GXSetTexCoordGen(tcid, GX_TG_MTX2x4, GX_TG_TEX0, mtx);
        break;
    case kTexGenProjected:
        GXSetTexCoordGen(tcid, GX_TG_MTX2x4, GX_TG_POS, mtx);
        break;
    case kTexGenEnviron:
        GXSetTexCoordGen(tcid, GX_TG_MTX2x4, GX_TG_BINRM, mtx);
        break;
    case kTexGenSphere:
        GXSetTexCoordGen(tcid, GX_TG_MTX2x4, GX_TG_NRM, mtx);
        break;
    case kTexGenNone:
        GXSetTexCoordGen(tcid, GX_TG_MTX2x4, GX_TG_TEX0, GX_TEXMTX_IDENT);
        break;
    default:;
    }
}
#ifdef VERSION_SZBE69_B8
#pragma pop
#endif

void WiiMat::SetModelviewTexGen() {
    if (!unk_0xAD_6) {
        unk_0xAD_6 = true;
        Mtx mtxView;
        Mtx mtxModel;
        Mtx mtxResult;
#ifdef MATCHING
        // Inline MakeWiiMtx: convert Transform (3x3 + translation) to GX Mtx (3x4),
        // transposing the rotation portion. Uses paired-single quantized loads:
        //   W=0: load 2 floats; W=1: load 1 float (qr0 is identity quantizer).
        {
            register WiiCam *cam = static_cast<WiiCam *>(RndCam::sCurrent);
            register Mtx *dst = &mtxView;
            ASM_BLOCK(
                psq_l       fp6,  0x278(cam), 0, 0
                psq_l       fp8,  0x284(cam), 0, 0
                psq_l       fp7,  0x280(cam), 1, 0
                psq_l       fp9,  0x28c(cam), 1, 0
                ps_merge00  fp0,  fp6,  fp8
                psq_l       fp10, 0x290(cam), 0, 0
                ps_merge11  fp2,  fp6,  fp8
                psq_l       fp12, 0x29c(cam), 0, 0
                ps_merge00  fp4,  fp7,  fp9
                psq_l       fp11, 0x298(cam), 1, 0
                psq_l       fp13, 0x2a4(cam), 1, 0
                ps_merge00  fp1, fp10, fp12
                ps_merge11  fp3, fp10, fp12
                ps_merge00  fp5, fp11, fp13
                psq_st      fp0,  0x0(dst),  0, 0
                psq_st      fp1,  0x8(dst),  0, 0
                psq_st      fp2,  0x10(dst), 0, 0
                psq_st      fp3,  0x18(dst), 0, 0
                psq_st      fp4,  0x20(dst), 0, 0
                psq_st      fp5,  0x28(dst), 0, 0
            )
        }
#else
        {
            const Transform &v = static_cast<WiiCam *>(RndCam::sCurrent)->mWiiViewXfm;
            mtxView[0][0] = v.m.x.x; mtxView[0][1] = v.m.y.x; mtxView[0][2] = v.m.z.x; mtxView[0][3] = v.v.x;
            mtxView[1][0] = v.m.x.y; mtxView[1][1] = v.m.y.y; mtxView[1][2] = v.m.z.y; mtxView[1][3] = v.v.y;
            mtxView[2][0] = v.m.x.z; mtxView[2][1] = v.m.y.z; mtxView[2][2] = v.m.z.z; mtxView[2][3] = v.v.z;
        }
#endif
        if (sCurrentModelXfm != nullptr) {
#ifdef MATCHING
            {
                register Transform *src = sCurrentModelXfm;
                register Mtx *dst = &mtxModel;
                ASM_BLOCK(
                    psq_l       fp6,  0x0(src),  0, 0
                    psq_l       fp8,  0xc(src),  0, 0
                    psq_l       fp7,  0x8(src),  1, 0
                    psq_l       fp9,  0x14(src), 1, 0
                    ps_merge00  fp0,  fp6,  fp8
                    psq_l       fp10, 0x18(src), 0, 0
                    ps_merge11  fp2,  fp6,  fp8
                    psq_l       fp12, 0x24(src), 0, 0
                    ps_merge00  fp4,  fp7,  fp9
                    psq_l       fp11, 0x20(src), 1, 0
                    psq_l       fp13, 0x2c(src), 1, 0
                    ps_merge00  fp1, fp10, fp12
                    ps_merge11  fp3, fp10, fp12
                    ps_merge00  fp5, fp11, fp13
                    psq_st      fp0,  0x0(dst),  0, 0
                    psq_st      fp1,  0x8(dst),  0, 0
                    psq_st      fp2,  0x10(dst), 0, 0
                    psq_st      fp3,  0x18(dst), 0, 0
                    psq_st      fp4,  0x20(dst), 0, 0
                    psq_st      fp5,  0x28(dst), 0, 0
                )
            }
#else
            {
                const Transform &m = *sCurrentModelXfm;
                mtxModel[0][0] = m.m.x.x; mtxModel[0][1] = m.m.y.x; mtxModel[0][2] = m.m.z.x; mtxModel[0][3] = m.v.x;
                mtxModel[1][0] = m.m.x.y; mtxModel[1][1] = m.m.y.y; mtxModel[1][2] = m.m.z.y; mtxModel[1][3] = m.v.y;
                mtxModel[2][0] = m.m.x.z; mtxModel[2][1] = m.m.y.z; mtxModel[2][2] = m.m.z.z; mtxModel[2][3] = m.v.z;
            }
#endif
            PSMTXConcat(mtxView, mtxModel, mtxResult);
        } else {
            PSMTXCopy(mtxView, mtxResult);
        }
        GXLoadTexMtxImm(mtxResult, 0x21, GX_MTX_3x4);
    }
}

void WiiMat::Reset() {
    unk_0xAD_5 = 0;
    unk_0xAD_6 = 0;
    GXSetTevDirect(GX_TEVSTAGE0);
    GXSetTevDirect(GX_TEVSTAGE1);
    GXSetTevDirect(GX_TEVSTAGE2);
    GXSetNumIndStages(0);
}

WiiMat::~WiiMat() {
    if (sCurrent == this)
        sCurrent = nullptr;
}
