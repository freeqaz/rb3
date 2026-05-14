#include "rndobj/HiResScreen.h"
#include "rndobj/Rnd.h"
#include "rndobj/Bitmap.h"
#include "rndobj/Tex.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "os/File.h"
#include "utl/FileStream.h"

HiResScreen gHiResScreen;
HiResScreen &TheHiResScreen = gHiResScreen;

HiResScreen::BmpCache::BmpCache(unsigned int ui1, unsigned int ui2) {
    mRowsPerCacheLine = ui2 + 1;
    mPixelsPerRow = ui1;
    mTotalRows = ui2;
    mDirtyStart = 0;
    mDirtyEnd = 0;
    mByteSize = mTotalRows % mRowsPerCacheLine;
    MILO_ASSERT(mTotalRows % mRowsPerCacheLine == 0, 0x3B);
    mTotalNumCacheLines = mTotalRows / mRowsPerCacheLine;
    mFileNames = new String[mTotalNumCacheLines];
    for (uint i = 0; i < mTotalNumCacheLines; i++) {
        mFileNames[i] = MakeString("_hires_cache_%.2d.dat", i);
    }
    mBuffer = (unsigned char *)_MemAlloc(mByteSize, 0);
    mCurrLoadedIndex = ui2;
    DeleteCache();
}

HiResScreen::BmpCache::~BmpCache() {
    DeleteCache();
    delete[] mFileNames;
    mFileNames = 0;
    delete mBuffer;
    mBuffer = 0;
}

void HiResScreen::BmpCache::DeleteCache() {
    for (unsigned int i = 0; i < mTotalNumCacheLines; i++) {
        FileDelete(mFileNames[i].c_str());
    }
}

void HiResScreen::BmpCache::GetLoadedRange(uint &ui1, uint &ui2) const {
    ui1 = mCurrLoadedIndex * mRowsPerCacheLine;
    ui2 = ui1 + mRowsPerCacheLine - 1;
}

void HiResScreen::BmpCache::FlushCache() {
    MILO_ASSERT(mCurrLoadedIndex < mTotalNumCacheLines, 0x9C);
    if (mDirtyEnd > mDirtyStart) {
        File *cacheFile = NewFile(mFileNames[mCurrLoadedIndex].c_str(), 1);
        MILO_ASSERT(cacheFile, 0xA2);
        cacheFile->Seek(mDirtyStart, 0);
        unsigned int nStart = mDirtyStart;
        unsigned int nEnd = mDirtyEnd;
        unsigned int nBuffRange = nEnd - nStart;
        MILO_ASSERT(nBuffRange <= mByteSize, 0xAA);
        unsigned int numWritten = cacheFile->Write(mBuffer + nStart, nBuffRange);
        MILO_ASSERT(numWritten == nBuffRange, 0xAE);
        cacheFile->Flush();
        delete cacheFile;
        mDirtyStart = 0;
        mDirtyEnd = 0;
    }
}

void HiResScreen::BmpCache::LoadCache(unsigned int y) {
    unsigned int nLoadedStart, nLoadedEnd;
    GetLoadedRange(nLoadedStart, nLoadedEnd);
    if (y >= nLoadedStart && y <= nLoadedEnd) {
        return;
    }
    if (mCurrLoadedIndex < mTotalNumCacheLines) {
        FlushCache();
    }
    unsigned int newIndex = y / mRowsPerCacheLine;
    File *cacheFile = NewFile(mFileNames[newIndex].c_str(), 2);
    if (cacheFile == 0) {
        memset(mBuffer, 0, mByteSize);
        cacheFile = NewFile(mFileNames[newIndex].c_str(), 0x204);
        MILO_ASSERT(cacheFile, 0x80);
        mDirtyStart = 0;
        mDirtyEnd = mByteSize;
    } else {
        unsigned int numRead = cacheFile->Read(mBuffer, mByteSize);
        MILO_ASSERT(numRead == mByteSize, 0x8A);
        mDirtyStart = 0;
        mDirtyEnd = 0;
    }
    delete cacheFile;
    mCurrLoadedIndex = newIndex;
}

void HiResScreen::BmpCache::GetPixelColor(
    int x, int y, unsigned char &r, unsigned char &g, unsigned char &b, unsigned char &a
) const {
    MILO_ASSERT(x >= 0 && x < mPixelsPerRow, 0xBC);
    unsigned int nLoadedStart = mCurrLoadedIndex * mRowsPerCacheLine;
    unsigned int nLoadedEnd = nLoadedStart + mRowsPerCacheLine - 1;
    MILO_ASSERT(y >= nLoadedStart && y <= nLoadedEnd, 0xC1);
    unsigned int yOffset = nLoadedEnd - y;
    unsigned int offset = (yOffset * mPixelsPerRow + x) * 4;
    unsigned char *ptr = mBuffer + offset;
    a = ptr[3];
    r = ptr[2];
    g = ptr[1];
    b = ptr[0];
}

void HiResScreen::BmpCache::SetPixelColor(
    int x, int y, unsigned char r, unsigned char g, unsigned char b, unsigned char a
) {
    MILO_ASSERT(x >= 0 && x < mPixelsPerRow, 0xD0);
    unsigned int nLoadedStart = mCurrLoadedIndex * mRowsPerCacheLine;
    unsigned int nLoadedEnd = nLoadedStart + mRowsPerCacheLine - 1;
    MILO_ASSERT(y >= nLoadedStart && y <= nLoadedEnd, 0xD5);
    unsigned int yOffset = nLoadedEnd - y;
    unsigned int offset = (yOffset * mPixelsPerRow + x) * 4;
    unsigned int newPixel = (a << 24) | (r << 16) | (g << 8) | b;
    unsigned char *bufPtr = mBuffer + offset;
    unsigned int oldPixel = *(unsigned int *)bufPtr;
    if (newPixel != oldPixel) {
        *(unsigned int *)bufPtr = newPixel;
        unsigned int minDirty = mDirtyStart;
        if (minDirty > offset) {
            minDirty = offset;
        }
        mDirtyStart = minDirty;
        unsigned int maxDirty = offset + 4;
        unsigned int curEnd = mDirtyEnd;
        if (maxDirty >= curEnd) {
            curEnd = maxDirty;
        }
        mDirtyEnd = curEnd;
    }
}

HiResScreen::HiResScreen()
    : mActive(0), mTiling(3), mFileBase("urhigh"), mAccumWidth(0), mAccumHeight(0),
      mCurrTile(0), mEvenOddDisabled(0), mShrinkToSafe(1), mConsoleShowing(0),
      mCache(NULL) {}

HiResScreen::~HiResScreen() {}

void HiResScreen::TakeShot(const char *c, int i) {
#ifdef VERSION_SZBE69_B8
    mFileBase = c;
    mTiling = i;
    mActive = 1;
    int x = i * 270;
    mCurrTile = 0;
    mAccumWidth = i * (TheRnd->mWidth - 480);
    mAccumHeight = TheRnd->mHeight * i;
    mAccumHeight -= x;
    mCache = new BmpCache(mAccumWidth, mAccumHeight);

    mEvenOddDisabled = TheRnd->GetEvenOddDisabled();
    mShrinkToSafe = TheRnd->ShrinkToSafeArea();
    mConsoleShowing = TheRnd->ConsoleShowing();
    TheRnd->SetEvenOddDisabled(true);
    TheRnd->SetShrinkToSafeArea(false);
    TheRnd->ShowConsole(false);
#endif
}

int HiResScreen::GetPaddingX() const { return 480; }
int HiResScreen::GetPaddingY() const { return 270; }

void HiResScreen::GetBorderForTile(
    int x, int y, int &left, int &right, int &top, int &bottom
) const {
    left = 0;
    top = 0;
    right = 0;
    bottom = 0;
    if (x < mTiling - 1) {
        top = 480;
    } else if (x > 0) {
        left = 480;
    }
    if (y < mTiling - 1) {
        bottom = 270;
        return;
    }
    if (y > 0) {
        right = 270;
    }
}

void HiResScreen::CurrentTileRect(
    const Hmx::Rect &inRect, Hmx::Rect &outTileRect, Hmx::Rect &outAccumRect
) const {
    int tiling = mTiling;
    int tile = mCurrTile;
    int tileY = tile / tiling;
    int tileX = tile % tiling;
    float invTiling = 1.0f / (float)tiling;
    float tileXStart = (float)tileX * invTiling;
    float tileYStart = (float)tileY * invTiling;
    float xPlusW = inRect.x + inRect.w;
    float yPlusH = inRect.y + inRect.h;
    float tileXWidth = (float)tileX * invTiling + invTiling - tileXStart;
    float tileYHeight = (float)tileY * invTiling + invTiling - tileYStart;
    float x0 = (inRect.x - tileXStart) / tileXWidth;
    x0 = Clamp(0.0f, 1.0f, x0);
    float y0 = (inRect.y - tileYStart) / tileYHeight;
    y0 = Clamp(0.0f, 1.0f, y0);
    float x1 = (xPlusW - tileXStart) / tileXWidth;
    x1 = Clamp(0.0f, 1.0f, x1);
    float y1 = (yPlusH - tileYStart) / tileYHeight;
    y1 = Clamp(0.0f, 1.0f, y1);
    outTileRect.x = x0;
    outTileRect.y = y0;
    outTileRect.w = x1;
    outTileRect.h = y1;
    outAccumRect.x = x0 * invTiling + tileXStart;
    outAccumRect.y = y0 * invTiling + tileYStart;
    outAccumRect.w = (x1 * invTiling + tileXStart) - outAccumRect.x;
    outAccumRect.h = (y1 * invTiling + tileYStart) - outAccumRect.y;
}

void HiResScreen::Accumulate() {
    if (mCurrTile == 0) {
        mCurrTile = 1;
        return;
    }
    int prevTile = mCurrTile - 1;
    if (prevTile >= mTiling * mTiling) {
        return;
    }
    RndTex *tex = Hmx::Object::New<RndTex>();
    RndBitmap bm;
    tex->SetBitmap(0, 0, 0, RndTex::kFrontBuffer, false, 0);
    tex->LockBitmap(bm, true);
    delete tex;
    int tileX = prevTile % mTiling;
    int tileY = prevTile / mTiling;
    int left, top, right, bottom;
    GetBorderForTile(tileX, tileY, left, right, top, bottom);
    int xOff = (TheRnd->mWidth - 480) * tileX;
    int yOff = (TheRnd->mHeight - 270) * tileY;
    Merge(bm, xOff, yOff, left, right, bm.Width(), bm.Height(), top, bottom);
    TheRnd->ResetProcCounter();
    mCurrTile++;
}

void HiResScreen::Finish() {
    int fileNum = 0;
    String filename;
    File *existFile = 0;
    FileStream *fs = 0;
    do {
        filename = MakeString("%s_%d.bmp", mFileBase, ++fileNum);
        delete existFile;
        existFile = NewFile(filename.c_str(), 4);
    } while (existFile);
    mCache->FlushCache();
    fs = new FileStream(filename.c_str(), FileStream::kWrite, true);
    void *tmpBuf = _MemAlloc(0x40, 0);
    RndBitmap bm;
    bm.Create(mAccumWidth, mAccumHeight, 0, 0x20, 0, 0, tmpBuf, 0);
    bm.SaveBmpHeader(fs);
    operator delete(tmpBuf);
    for (int i = mCache->mTotalNumCacheLines - 1; i >= 0; i--) {
        mCache->LoadCache(i * mCache->mRowsPerCacheLine);
        fs->Write(mCache->mBuffer, mCache->mByteSize);
    }
    delete fs;
    FileMkDir("lo_res");
    filename = MakeString("lo_res/%s_%d.bmp", mFileBase, fileNum);
    File *loResFile = NewFile(filename.c_str(), 0x204);
    if (loResFile != 0) {
        delete loResFile;
        RndBitmap loResBm;
        DownSample(loResBm);
        loResBm.SaveBmp(filename.c_str());
    }
    mActive = false;
    TheRnd->SetEvenOddDisabled(mEvenOddDisabled);
    TheRnd->SetShrinkToSafeArea(mShrinkToSafe);
    TheRnd->ShowConsole(mConsoleShowing);
    delete mCache;
}

void HiResScreen::Merge(
    const RndBitmap &bm,
    int srcX,
    int srcY,
    int srcW,
    int srcH,
    int dstX,
    int dstY,
    int padX,
    int padY
) {
    if ((unsigned int)srcH >= srcW) {
        return;
    }
    int xStart = dstX;
    int xEnd = srcH;
    int xRange = xEnd - srcX;
    for (; xStart < mAccumHeight && xStart >= 0; xStart++, xRange++) {
        mCache->LoadCache(xStart);
        if (xStart + xRange >= srcH) {
            break;
        }
        int yStart = srcY;
        int yOff = srcX - padX;
        int yRange = srcY - padY;
        for (; yStart < mAccumWidth && yStart >= 0; yStart++, yOff++, yRange++) {
            if (yStart + yRange >= srcW) {
                break;
            }
            int bmY = yRange + yStart;
            int bmX = xRange + xStart;
            unsigned char r, g, b, a;
            bm.PixelColor(bmY, bmX, r, g, b, a);
            unsigned char cr, cg, cb, ca;
            mCache->GetPixelColor(yStart, xStart, cr, cg, cb, ca);
            float blendX = 0.0f;
            if (bmY > padX) {
                blendX = (float)yOff / (float)padX;
            }
            float blendY = 0.0f;
            if (bmX > srcX) {
                blendY = (float)xRange / (float)srcX;
            }
            float blend;
            if (blendX > 0.0f || blendY > 0.0f) {
                blend = sqrtf(blendX * blendX + blendY * blendY);
                blend = blend - 0.5f;
                blend = blend + blend;
                blend = Min(blend, 1.0f);
                if (blend < 0.0f) blend = 0.0f;
            } else {
                blend = 0.0f;
            }
            float invBlend = (1.0f - blend) * 255.0f;
            unsigned char newA = (unsigned char)invBlend;
            if (ca != 0) {
                float t = ca / 255.0f;
                int dr = cr - r;
                int dg = cg - g;
                int db = cb - b;
                r += (unsigned char)(dr * t + 0.5f);
                g += (unsigned char)(dg * t + 0.5f);
                b += (unsigned char)(db * t + 0.5f);
                if (newA < ca) {
                    newA = ca;
                }
            }
            mCache->SetPixelColor(yStart, xStart, r, g, b, newA);
        }
    }
}

void HiResScreen::DownSample(RndBitmap &outBm) {
    int tiling = mTiling;
    int accum_h = mAccumHeight;
    int accum_w = mAccumWidth;
    int newWidth = (tiling * 480 + accum_w) / tiling;
    int newHeight = (tiling * 270 + accum_h) / tiling;
    float scaleX = (float)accum_w / (float)newWidth;
    float scaleY = (float)accum_h / (float)newHeight;
    outBm.Create(newWidth, newHeight, 0, 0x20, 0, 0, 0, 0);
    memset(outBm.Pixels(), 0, outBm.PixelBytes());
    for (int y = 0; y < newHeight; y++) {
        int srcY = (int)(y * scaleY);
        mCache->LoadCache(srcY);
        for (int x = 0; x < newWidth; x++) {
            int srcX = (int)(x * scaleX);
            unsigned char r, g, b, a;
            mCache->GetPixelColor(srcX, srcY, r, g, b, a);
            outBm.SetPixelColor(x, y, r, g, b, a);
        }
    }
}

Hmx::Rect HiResScreen::ScreenRect() const {
    Hmx::Rect r = RndCam::sCurrent->mScreenRect;
    return ScreenRect(RndCam::sCurrent, r);
}

Hmx::Rect HiResScreen::ScreenRect(const RndCam *cam, const Hmx::Rect &r) const {
    Hmx::Rect inRect = r;
    Hmx::Rect ret = r;
    if ((cam->mTargetTex == NULL || mOverride) && mActive) {
        int tiling = mTiling;
        int tile = mCurrTile;
        if (tile < tiling * tiling) {
            float invTiling = 1.0f / (float)tiling;
            Hmx::Rect tileRect;
            CurrentTileRect(inRect, tileRect, ret);
            int right, left, bottom, top;
            GetBorderForTile(tile % tiling, tile / tiling, left, right, top, bottom);
            float screenH = (float)TheRnd->mHeight;
            float screenW = (float)TheRnd->mWidth;
            float leftF = (float)left;
            float rightF = (float)right;
            float topF = (float)top;
            float bottomF = (float)bottom;
            float xScale = screenH / (screenH - leftF);
            float yScale = screenW / (screenW - topF);
            float xShift = screenH / (screenH - rightF);
            float yShift = screenW / (screenW - bottomF);
            float xOffset = (xScale - invTiling) - invTiling;
            xShift = (xShift - invTiling) - invTiling;
            float yOffset = yScale - invTiling;
            yShift = yShift - invTiling;
            ret.x = ret.x - xOffset;
            ret.w = ret.w + (xOffset + xShift);
            ret.y = ret.y - yOffset;
            ret.h = ret.h + yOffset + yShift;
            return ret;
        }
    }
    return ret;
}

Hmx::Rect HiResScreen::InvScreenRect() const {
    Hmx::Rect r = ScreenRect();
    Hmx::Rect ret;
    float negX = -r.x;
    float negY = -r.y;
    float w = r.w;
    float h = r.h;
    ret.x = negX / w;
    ret.w = 1.0f / w;
    ret.y = negY / h;
    ret.h = 1.0f / h;
    return ret;
}
