#pragma once
#include "obj/ObjMacros.h"
#include "rndobj/Mat.h"
#include "utl/MemMgr.h"

class RndEnviron;
class RndCam;

class WiiMat : public RndMat {
public:
    WiiMat() { unk_0xAD_7 = false; }
    virtual ~WiiMat();
    OBJ_CLASSNAME(WiiMat)
    OBJ_SET_TYPE(WiiMat)

    void Reset();
    void SelectParticles();
    RndMat *Select(bool);
    void SetModelviewTexGen();
    void SetTexGen(GXTexCoordID, GXTexMtx);
    void SetStageState(int &, int &, int &, bool, int);
    bool SetFade(int &, int &, int &, RndEnviron *, RndCam *);
    void SetColorXfm(int &, RndEnviron *);

    static void SetOverrideAlphaWrite(bool set) { sOverrideAlphaWrite = set; }
    static WiiMat *sCurrent;
    static bool sOverrideAlphaWrite;
    static bool sCurrentZCompLoc;
    static RndTex *sGradientTex;
    static Transform *sCurrentModelXfm;

    static void SetAlphaCutout(bool, int);
    static void SetZBufferMode(ZMode);
    static void SetFrameBlend(Blend);
    static void SetFog(bool, RndEnviron *, RndCam *);
    static void SetCurrentModelTransform(const Transform *);
    static void PreInit();
    static void Init();

    NEW_OBJ(WiiMat)
    REGISTER_OBJ_FACTORY_FUNC(WiiMat)

    NEW_OVERLOAD
    DELETE_OVERLOAD
};