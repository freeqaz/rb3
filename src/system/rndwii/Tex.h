#pragma once

#include "obj/ObjMacros.h"
#include "revolution/gx/GXTypes.h"
#include "rndobj/Tex.h"
#include "utl/PoolAlloc.h"
#include <set>

/**
 * @brief Platform implementation of RndTex.
 */
class WiiTex : public RndTex {
public:
    WiiTex();
    virtual ~WiiTex();
    OBJ_CLASSNAME(WiiTex)
    OBJ_SET_TYPE(WiiTex)
    NEW_OBJ(WiiTex)
    virtual void unk(bool);

    void PresyncBitmap();
    void DeleteSurface();
    void MovieSwapFrames();
    void CopyFromFB(int, int, int, int, bool, bool);
    void CreateScreenShot();
    void DisableFiltering(bool);
    void Select(_GXTexMapID);
    void *ImgData() { return mImageData; }
    void *GetMovieLoadingFramePtr();

    u8 pad[32];
    void *mImageData; // 0x84
    GXTexFmt mFormat; // 0x88
    u8 pad2[32]; // 0x8C
    int unkAC; // 0xAC
    int unkB0; // 0xB0
    int unkB4; // 0xB4
    int unkB8; // 0xB8
    u8 pad3[12]; // 0xBC
    int unkC8; // 0xC8
    int unkCC; // 0xCC

    static bool bComposingOutfitTexture;

    NEW_POOL_OVERLOAD(WiiTex)
    DELETE_POOL_OVERLOAD(WiiTex)
    REGISTER_OBJ_FACTORY_FUNC(WiiTex)
};

extern std::set<WiiTex *> gRenderTextureSet;