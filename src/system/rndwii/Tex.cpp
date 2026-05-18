#include "Tex.h"
#include "os/Debug.h"
#include "revolution/gx/GXFrameBuf.h"
#include "revolution/gx/GXPixel.h"
#include "rndobj/Rnd.h"
#include "rndobj/Tex.h"
#include "rndwii/Rnd.h"
#include "utl/MemMgr.h"
#include <set>
#include <stdlib.h>

std::set<WiiTex *> gRenderTextureSet;

bool WiiTex::bComposingOutfitTexture = false;

WiiTex::WiiTex() : mImageData(NULL), mFormat() {
    unkAC = 0;
    unkB0 = 0;
    unkC8 = 0;
    unkCC = 0;
    unkB8 = 0;
    unkB4 = 0;
}

WiiTex::~WiiTex() { DeleteSurface(); }

void WiiTex::PresyncBitmap() { DeleteSurface(); }

void WiiTex::DeleteSurface() {}

u32 OrderFromFormat(unsigned int ui) {
    switch (ui) {
    case 4:
        return 0;
    case 14:
        return 72;
    case 6:
        return 64;
    case 1:
        return 192;
    default:
        MILO_FAIL("Currently unsupported format %d for OrderFromFormat.\n", ui);
        return 0;
    }
}

extern "C" void GXInitTexObjData(GXTexObj *obj, void *imageData);

void WiiTex::MovieSwapFrames() {
    MILO_ASSERT_FMT(!(unkB0 & 2), "mImageData 0x%08x being leaked!\n", (int)mImageData);
    mImageData = (void *)((&unkB4)[(unkB0 & 0x100) ? 1 : 0]);
    MILO_ASSERT(!((int)mImageData & 31), 0x25C);
    unkB0 ^= 0x100;
    typedef u32 (*PFN_GXGetTexBufferSize)(u16, u16, GXTexFmt, u32, u32);
    u32 sz = ((PFN_GXGetTexBufferSize)&GXGetTexBufferSize)(mWidth, mHeight, mFormat, 1, 2);
    DCStoreRange(mImageData, sz);
    GXInitTexObjData((GXTexObj *)((char *)this + 0x64), mImageData);
}

void WiiTex::CopyFromFB(
    int src_x, int src_y, int src_w, int src_h, bool copy_bool, bool is_mip
) {
    MILO_ASSERT(mImageData, 711);
    MILO_ASSERT(mType & kRendered, 712);
    if (copy_bool)
        GXSetZMode(TRUE, GX_ALWAYS, TRUE);
    GXSetAlphaUpdate(TRUE);
    // TODO add PSVEC copy
    GXSetCopyClear(*(GXColor *)&TheRnd->mClearColor, 0x00FFFFFF);
    GXSetTexCopySrc(src_x, src_y, src_w, src_h);
    GXSetTexCopyDst(mWidth, mHeight, mFormat, is_mip);
    GXGetTexBufferSize(mWidth, mHeight, mFormat, 0, 0);

    GXSetCopyClamp(GX_CLAMP_ALL);
    GXCopyTex(mImageData, u8(copy_bool));
    if (bComposingOutfitTexture || !TheRnd->mInGame)
        RndGxDrawDone();
}

struct YUV422;
struct YUV444;
struct RGB;
void YUV422To444(YUV422 *, YUV444 *, int, int);
void YUV444ToRGB(YUV444 *, RGB *, int, int);
void RGBToBMP(RGB *, void *, unsigned long, unsigned long);
bool ConvertAndStoreYUV2BMP(void *, int, int, void *);

void WiiTex::CreateScreenShot() {
    DeleteSurface();
    mWidth = TheWiiRnd.unk_0x170;
    mHeight = *(unsigned short *)((char *)&TheWiiRnd + 0x174);
    mBpp = 24;
    mImageData = _MemAlloc((mBpp >> 3) * (mWidth * mHeight), 0x20);
    *(int *)((char *)this + 0xb0) = (*(int *)((char *)this + 0xb0) & ~0x2) | 0x2;
    MILO_ASSERT(!((int)mImageData & 31), 0x3D6);
    if (!ConvertAndStoreYUV2BMP(WiiRnd::GetCurrXFB(), mWidth, mHeight, mImageData)) {
        MILO_WARN("[WiiTex::CreateScreenShot] Failed to covert XFB to BMP!\n"); // BUG:
                                                                                // covert
        DeleteSurface();
    }
}

bool ConvertAndStoreYUV2BMP(void *src, int w, int h, void *dst) {
    int n = w * h;
    YUV444 *yuv444 = (YUV444 *)calloc(n, 3);
    if (yuv444 == NULL) return false;
    YUV422To444((YUV422 *)src, yuv444, w, h);
    RGB *rgb = (RGB *)calloc(n, 3);
    if (rgb == NULL) return false;
    YUV444ToRGB(yuv444, rgb, w, h);
    free(yuv444);
    RGBToBMP(rgb, dst, w, h);
    free(rgb);
    return true;
}
