#pragma once
#include "rndobj/Draw.h"
#include "rndobj/Env.h"

class RndCam;
class RndTex;

class RndShadowMap {
public:
    static bool PrepShadow(RndDrawable *, RndEnviron *);
    static void EndShadow();

protected:
    static RndCam *sLightCam;
    static RndTex *sShadowTex;
};
