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
    while (true) {
        mRowsPerCacheLine--;
        if (mTotalRows % mRowsPerCacheLine != 0) continue;
        mByteSize = mPixelsPerRow * 4 * mRowsPerCacheLine;
        if (mByteSize <= 0xC5C100) break;
    }
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
    const unsigned int &_ref0 = mCurrLoadedIndex;
    MILO_ASSERT(_ref0 < mTotalNumCacheLines, 0x9C);
    if (mDirtyEnd > mDirtyStart) {
        File *cacheFile = NewFile(mFileNames[_ref0].c_str(), 4);
        MILO_ASSERT(cacheFile, 0xA2);
        cacheFile->Seek(mDirtyStart, 0);
        unsigned int nStart = mDirtyStart;
        unsigned int nEnd = mDirtyEnd;
        unsigned char *pBuff = mBuffer + nStart;
        unsigned int nBuffRange = nEnd - nStart;
        MILO_ASSERT(nBuffRange <= mByteSize, 0xAA);
        unsigned int numWritten = cacheFile->Write(pBuff, nBuffRange);
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
    unsigned int nLoadedStart, nLoadedEnd;
    GetLoadedRange(nLoadedStart, nLoadedEnd);
    MILO_ASSERT(y >= nLoadedStart && y <= nLoadedEnd, 0xBF);
    unsigned char *ptr = mBuffer + (nLoadedEnd - y) * mPixelsPerRow * 4 + x * 4;
    a = ptr[3];
    r = ptr[2];
    g = ptr[1];
    b = ptr[0];
}

void HiResScreen::BmpCache::SetPixelColor(
    int x, int y, unsigned char r, unsigned char g, unsigned char b, unsigned char a
) {
    unsigned int nLoadedStart;
    unsigned int nLoadedEnd;
    GetLoadedRange(nLoadedStart, nLoadedEnd);
    MILO_ASSERT(y >= nLoadedStart && y <= nLoadedEnd, 0xD1);
    unsigned char newBuf[4];
    unsigned int offset = ((nLoadedEnd - y) * mPixelsPerRow + x) * 4;
    newBuf[3] = a;
    newBuf[2] = r;
    newBuf[1] = g;
    newBuf[0] = b;
    unsigned char *bufPtr = mBuffer + offset;
    if ((*(unsigned int *)newBuf) != (*(unsigned int *)bufPtr)) {
        *(unsigned int *)bufPtr = (*(unsigned int *)newBuf);
        unsigned int dirtyStart = mDirtyStart;
        if (offset < dirtyStart) {
            dirtyStart = offset;
        }
        unsigned int dirtyEnd = mDirtyEnd;
        unsigned int endOff = offset + 4;
        mDirtyStart = dirtyStart;
        if (dirtyEnd < endOff) {
            dirtyEnd = endOff;
        }
        mDirtyEnd = dirtyEnd;
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
    int tileX = mCurrTile % mTiling;
    int tileY = mCurrTile / mTiling;
    float invTiling = 1.0f / (float)mTiling;
    float tileXStart = (float)tileX * invTiling;
    float tileYStart = (float)tileY * invTiling;
    float xPlusW = inRect.x + inRect.w;
    float tileYHeight = (float)tileY * invTiling + invTiling - tileYStart;
    float yPlusH = inRect.y + inRect.h;
    float tileXWidth = (float)tileX * invTiling + invTiling - tileXStart;
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
    int prevTile = mCurrTile;
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
    int xOff = tileX * (TheRnd->mWidth - 480);
    int yOff = tileY * (TheRnd->mHeight - 270);
    auto _tmp0 = bm.Width();
    Merge(bm, xOff, yOff, left, right, _tmp0, bm.Height(), top, bottom);
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
    const RndBitmap &bm, int srcX, int srcY, int srcW, int srcH, int dstX, int dstY, int padX, int padY
) {
    int yIter;
    int bmY = srcH;
    int blendThreshX = dstX - padX;
    int blendThreshY = dstY - padY;
    for (int i = 0; bmY < dstY; i++, bmY++) {
        yIter = srcY + i;
        mCache->LoadCache(yIter);
        for (int j = 0, bmX = srcW; bmX < dstX; j++, bmX++) {
            unsigned char r, g, b, a;
            bm.PixelColor(bmX, bmY, r, g, b, a);
            int xOff = srcX + j;
            unsigned char cr, cg, cb, ca;
            mCache->GetPixelColor(xOff, yIter, cr, cg, cb, ca);
            float blend = 0.0f;
            float blendY = 0.0f;
            float blendX = 0.0f;
            if (bmX > blendThreshX) {
                blendX = (float)(bmX - blendThreshX) / (float)padX;
            }
            if (bmY > blendThreshY) {
                blendY = (float)(bmY - blendThreshY) / (float)padY;
            }
            if (blendX > 0.0f || blendY > 0.0f) {
                blend = 2.0f * (sqrtf(blendX * blendX + blendY * blendY) - 0.5f);
                if (blend < 0.0f) blend = 0.0f;
                blend = Min(blend, 1.0f);
            }
            a = (unsigned char)((1.0f - blend) * 255.0f);
            if (ca != 0) {
                float t = (float)ca / 255.0f;
                r += (unsigned char)((int)(cr - r) * t + 0.5f);
                g += (unsigned char)((int)(cg - g) * t + 0.5f);
                b += (unsigned char)((int)(cb - b) * t + 0.5f);
                a = Max(a, ca);
            }
            mCache->SetPixelColor(xOff, yIter, r, g, b, a);
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
    if ((cam->mTargetTex == NULL || mOverride) && mActive) {
        int tiling = mTiling;
        int tile = mCurrTile;
        if (tile < tiling * tiling) {
            float invTiling = 1.0f / (float)tiling;
            Hmx::Rect ret;
            Hmx::Rect tileRect;
            CurrentTileRect(inRect, tileRect, ret);
            int left, top, right, bottom;
            GetBorderForTile(tile % tiling, tile / tiling, left, right, top, bottom);
            float fLeft = (float)left;
            float fTop = (float)top;
            float fRight = (float)right;
            float fBottom = (float)bottom;
            float screenW = (float)TheRnd->mWidth;
            float screenH = (float)TheRnd->mHeight;
            float xOffset = (screenW * invTiling) / (screenW - fLeft) - invTiling;
            float xShift = (screenW * invTiling) / (screenW - fTop) - invTiling;
            float yOffset = (screenH * invTiling) / (screenH - fRight) - invTiling;
            float yShift = (screenH * invTiling) / (screenH - fBottom) - invTiling;
            ret.y -= yOffset;
            ret.x -= xOffset;
            ret.w += (xOffset + xShift);
            ret.h += (yOffset + yShift);
            return ret;
        }
    }
    return inRect;
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
