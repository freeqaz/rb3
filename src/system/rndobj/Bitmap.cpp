#include "Bitmap.h"
#include "decomp.h"
#include "os/Debug.h"
#include "os/Endian.h"
#include "utl/BinStream.h"
#include "utl/BufStream.h"
#include "utl/ChunkStream.h"
#include "utl/FileStream.h"
#include "utl/MakeString.h"
#include "utl/MemMgr.h"
#include "utl/Symbols.h"
#include <string.h>

unsigned char BITMAP_REV = 1;

BinStream &RndBitmap::LoadHeader(BinStream &bs, u8 &numMips) {
    u8 rev, h;
    u8 pad[0x13];
    bs >> rev;
    bs >> mBpp;
    if (rev != 0)
        bs >> mOrder;
    else {
        bs >> h;
        mOrder = h;
    }
    bs >> numMips;
    bs >> mWidth;
    bs >> mHeight;
    bs >> mRowBytes;
    numMips = 0;
    bs.Read(pad, rev != 0 ? 0x13 : 6);
    return bs;
}

BinStream &RndBitmap::SaveHeader(BinStream &bs) const {
    static u8 pad[0x13];
    bs << BITMAP_REV << mBpp << (unsigned int)mOrder << (unsigned char)NumMips() << mWidth
       << mHeight;
    bs << mRowBytes;
    bs.Write(pad, 0x13);
    return bs;
}

BinStream &operator>>(BinStream &bs, tagBITMAPFILEHEADER &bmfh) {
    bs >> bmfh.bfSize;
    bs >> bmfh.bfReserved1;
    bs >> bmfh.bfReserved2;
    bs >> bmfh.bfOffBits;
    return bs;
}

BinStream &operator<<(BinStream &bs, const tagBITMAPFILEHEADER &bmfh) {
    bs << bmfh.bfSize << bmfh.bfReserved1 << bmfh.bfReserved2 << bmfh.bfOffBits;
    return bs;
}

BinStream &operator>>(BinStream &bs, tagBITMAPINFOHEADER &bmih) {
    bs >> bmih.biSize;
    bs >> bmih.biWidth;
    bs >> bmih.biHeight;
    bs >> bmih.biPlanes;
    bs >> bmih.biBitCount;
    bs >> bmih.biCompression;
    bs >> bmih.biSizeImage;
    bs >> bmih.biXPelsPerMeter;
    bs >> bmih.biYPelsPerMeter;
    bs >> bmih.biClrUsed;
    bs >> bmih.biClrImportant;
    return bs;
}

BinStream &operator<<(BinStream &bs, const tagBITMAPINFOHEADER &bmih) {
    bs << bmih.biSize << bmih.biWidth << bmih.biHeight << bmih.biPlanes << bmih.biBitCount
       << bmih.biCompression << bmih.biSizeImage << bmih.biXPelsPerMeter
       << bmih.biYPelsPerMeter << bmih.biClrUsed << bmih.biClrImportant;
    return bs;
}

int RndBitmap::NumMips() const {
    const RndBitmap *x = this;
    int i;
    for (i = 0; x->mMip; i++)
        x = x->mMip;
    return i;
}

DECOMP_FORCEFUNC(Bitmap, RndBitmap, PixelBytes())

#pragma push
#pragma force_active on
inline int RndBitmap::PixelBytes() const { return mRowBytes * mHeight; }
#pragma pop

int RndBitmap::PaletteBytes() const {
    if (mBpp <= 8) {
        if ((mOrder & 0x38) == 0 && (mOrder & 0x80) == 0) {
            return (1 << mBpp) * 4;
        }
    }
    return 0;
}

unsigned char RndBitmap::NearestColor(
    unsigned char r, unsigned char g, unsigned char b, unsigned char a
) const {
    int paletteColorIdx = -1;
    int minDiff = 0x40000;
    unsigned char pr, pg, pb, pa;
    for (int i = (1 << mBpp) - 1; i >= 0; i--) {
        PaletteColor(i, pr, pg, pb, pa);
        int dr = pr - r;
        int dg = pg - g;
        int db = pb - b;
        int da = pa - a;
        int diff = dr * dr + dg * dg + db * db + da * da;
        if (diff < minDiff) {
            minDiff = diff;
            paletteColorIdx = i;
        }
    }
    return paletteColorIdx;
}

void RndBitmap::ConvertColor(
    const unsigned char *uc,
    unsigned char &r,
    unsigned char &g,
    unsigned char &b,
    unsigned char &a
) const {
    if (mBpp == 0x20 || mPalette) {
        if (mOrder & 1) {
            a = uc[3];
            b = uc[2];
            g = uc[1];
            r = uc[0];
        } else if (mOrder & 0x40) {
            a = uc[0];
            r = uc[1];
            g = uc[0x20];
            b = uc[0x21];
        } else {
            a = uc[3];
            r = uc[2];
            g = uc[1];
            b = uc[0];
        }

        if (mOrder & 2) {
            a = ((a * 256) - a) >> 7;
        }
    } else if (mBpp == 0x10) {
        unsigned short swapped = SwapBytes(*(unsigned short *)uc);
        if (mOrder & 1) {
            a = -(swapped >> 0xF & 1);
            b = swapped >> 7 & 0xF8;
            g = swapped >> 2 & 0xF8;
            r = swapped << 3;
        } else {
            a = -(swapped >> 0xF & 1);
            r = swapped >> 7 & 0xF8;
            g = swapped >> 2 & 0xF8;
            b = swapped << 3;
        }
    } else if ((mOrder & 0x80) && (mOrder & 0x40)) {
        r = 255;
        g = 255;
        b = 255;
        a = *uc;
    } else {
        a = 255;
        if (mOrder & 1) {
            b = uc[2];
            g = uc[1];
            r = uc[0];
        } else {
            r = uc[2];
            g = uc[1];
            b = uc[0];
        }
    }
}

void RndBitmap::ConvertColor(
    unsigned char r, unsigned char g, unsigned char b, unsigned char a, unsigned char *uc
) const {
    if (mBpp == 0x20 || mPalette) {
        if (mOrder & 2) {
            a = (a + 1) >> 1;
        }
        if (mOrder & 1) {
            uc[3] = a;
            uc[2] = b;
            uc[1] = g;
            uc[0] = r;
        } else if (mOrder & 0x40) {
            uc[0] = a;
            uc[1] = r;
            uc[0x20] = g;
            uc[0x21] = b;

        } else {
            uc[3] = a;
            uc[2] = r;
            uc[1] = g;
            uc[0] = b;
        }
        return;
    }
    if (mBpp == 0x10) {
        unsigned short *twobytes = (unsigned short *)uc;
        if (mOrder & 1) {
            *twobytes = (a & 0x80) << 8 | (b & 0xF8) << 7 | (g & 0xF8) << 2 | r >> 3;
        } else {
            *twobytes = (a & 0x80) << 8 | (r & 0xF8) << 7 | (g & 0xF8) << 2 | b >> 3;
        }
        *twobytes = EndianSwap(*twobytes);
        return;
    }
    if ((mOrder & 0x80) && (mOrder & 0x40)) {
        uc[0] = a;
        return;
    }
    if (mOrder & 1) {
        uc[2] = b;
        uc[1] = g;
        uc[0] = r;
    } else {
        uc[2] = r;
        uc[1] = g;
        uc[0] = b;
    }
}

void RndBitmap::Reset() {
    mRowBytes = 0;
    mHeight = 0;
    mWidth = 0;
    mBpp = 32;
    mOrder = 1;
    mPalette = NULL;
    mPixels = NULL;
    if (mBuffer) {
        _MemFree(mBuffer);
        mBuffer = NULL;
    }
    RELEASE(mMip);
}

void RndBitmap::Create(const RndBitmap &bm, int bpp, int order, void *palette) {
    Create(bm.Width(), bm.Height(), 0, bpp, order, palette, NULL, NULL);
    if (mPalette && !palette) {
        MILO_ASSERT(bm.Palette(), 392);
        for (int i = 0; i < bm.NumPaletteColors(); i++) {
            unsigned char r, g, b, a;
            bm.PaletteColor(i, r, g, b, a);
            SetPaletteColor(i, r, g, b, a);
        }
    }
    Blt(bm, 0, 0, 0, 0, mWidth, mHeight);
    if (bm.nextMip()) {
        mMip = new RndBitmap();
        mMip->Create(*bm.nextMip(), bpp, order, mPalette);
    }
}

DECOMP_FORCEFUNC(Bitmap, RndBitmap, NumPaletteColors())

#pragma push
#pragma force_active on
inline int RndBitmap::NumPaletteColors() const {
    if (mPalette)
        return 1 << mBpp;
    else
        return 0;
}
#pragma pop

void RndBitmap::AllocateBuffer() {
    int paletteBytes;
    if (mPalette)
        paletteBytes = 0;
    else
        paletteBytes = PaletteBytes();
    int sum = paletteBytes + PixelBytes();
    if (sum)
        mBuffer = (u8 *)_MemAlloc(sum, 32);
    if (paletteBytes)
        mPalette = mBuffer;
    mPixels = mBuffer + paletteBytes;
    MILO_ASSERT(!paletteBytes, 439);
    MILO_ASSERT(!((int)mPixels & 31), 441);
}

void RndBitmap::Create(
    int width,
    int height,
    int rowlen,
    int bpp,
    int order,
    void *palette,
    void *pixels,
    void *buf
) {
    MILO_ASSERT(width >= 0 && height >= 0, 454);
    MILO_ASSERT(bpp == 4 || bpp == 8 || bpp == 16 || bpp == 24 || bpp == 32, 455);
    mWidth = width;
    mHeight = height;
    mRowBytes = rowlen;
    mBpp = bpp;
    mOrder = order;
    mPixels = (u8 *)pixels;
    MILO_ASSERT(!((int)mPixels & 31), 465);
    mPalette = (u8 *)palette;
    RELEASE(mMip);
    if (mRowBytes == 0) {
        if (mBpp == 4 && mWidth & 1) {
            mRowBytes = (mWidth + 1) * mBpp >> 3;
        } else
            mRowBytes = mWidth * mBpp >> 3;
    }
    if (mOrder & 4) {
        unsigned char theBpp = mBpp;
        if ((theBpp == 8 && (mWidth < 16 || mHeight < 16))
            || (theBpp == 4 && (mWidth < 32 || mHeight < 16)) || theBpp > 8) {
            mOrder &= ~0x4;
        }
    }
    if (mBuffer) {
        _MemFree(mBuffer);
        mBuffer = nullptr;
    }
    if (buf)
        mBuffer = (u8 *)buf;
    else if (!pixels)
        AllocateBuffer();
}

void RndBitmap::Create(void *buffer) {
    if (!buffer)
        MILO_WARN("Load buffer is empty");
    else {
        BufStream bs(buffer, 32, true);
        unsigned char buf;
        LoadHeader(bs, buf);
        if (mBuffer) {
            _MemFree(mBuffer);
            mBuffer = nullptr;
        }
        mBuffer = (u8 *)buffer;
        u8 *i5 = mBuffer + bs.Tell();

        int pbytes = PaletteBytes();
        mPalette = pbytes ? i5 : 0;

        int pixbytes = PixelBytes();
        mPixels = i5 + pbytes;
        MILO_ASSERT(!((int)mPixels & 31), 0x21A);
        u8 *pixels = i5 + pbytes + pixbytes;
        RELEASE(mMip);
        int width = mWidth;
        int height = mHeight;
        RndBitmap *cur = this;
        while (buf-- != 0) {
            cur->mMip = new RndBitmap();
            cur = cur->mMip;
            width >>= 1;
            height >>= 1;
            pixbytes >>= 2;
            cur->Create(width, height, 0, mBpp, mOrder, mPalette, pixels, 0);
            pixels += pixbytes;
        }
    }
}

int RndBitmap::PixelOffset(int x, int y, bool &nibble) const {
    static const char bytes02[64] = {
        0x0,  0x4,  0x8,  0xC,  0x10, 0x14, 0x18, 0x1c, 0x2,  0x6,  0xa,  0xe,  0x12,
        0x16, 0x1a, 0x1e, 0x20, 0x24, 0x28, 0x2c, 0x30, 0x34, 0x38, 0x3c, 0x22, 0x26,
        0x2a, 0x2e, 0x32, 0x36, 0x3a, 0x3e, 0x11, 0x15, 0x19, 0x1d, 0x1,  0x5,  0x9,
        0xd,  0x13, 0x17, 0x1b, 0x1f, 0x3,  0x7,  0xb,  0xf,  0x31, 0x35, 0x39, 0x3d,
        0x21, 0x25, 0x29, 0x2d, 0x33, 0x37, 0x3b, 0x3f, 0x23, 0x27, 0x2b, 0x2f
    };
    static const char bytes13[64] = {
        0x10, 0x14, 0x18, 0x1c, 0x0,  0x4,  0x8,  0xc,  0x12, 0x16, 0x1a, 0x1e, 0x2,
        0x6,  0xa,  0xe,  0x30, 0x34, 0x38, 0x3c, 0x20, 0x24, 0x28, 0x2c, 0x32, 0x36,
        0x3a, 0x3e, 0x22, 0x26, 0x2a, 0x2e, 0x1,  0x5,  0x9,  0xd,  0x11, 0x15, 0x19,
        0x1d, 0x3,  0x7,  0xb,  0xf,  0x13, 0x17, 0x1b, 0x1f, 0x21, 0x25, 0x29, 0x2d,
        0x31, 0x35, 0x39, 0x3d, 0x23, 0x27, 0x2b, 0x2f, 0x33, 0x37, 0x3b, 0x3f
    };
    static const char hbytes02[128] = {
        0x0,  0x8,  0x10, 0x18, 0x20, 0x28, 0x30, 0x38, 0x2,  0xa,  0x12, 0x1a, 0x22,
        0x2a, 0x32, 0x3a, 0x4,  0xc,  0x14, 0x1c, 0x24, 0x2c, 0x34, 0x3c, 0x6,  0xe,
        0x16, 0x1e, 0x26, 0x2e, 0x36, 0x3e, 0x40, 0x48, 0x50, 0x58, 0x60, 0x68, 0x70,
        0x78, 0x42, 0x4a, 0x52, 0x5a, 0x62, 0x6a, 0x72, 0x7a, 0x44, 0x4c, 0x54, 0x5c,
        0x64, 0x6c, 0x74, 0x7c, 0x46, 0x4e, 0x56, 0x5e, 0x66, 0x6e, 0x76, 0x7e, 0x21,
        0x29, 0x31, 0x39, 0x1,  0x9,  0x11, 0x19, 0x23, 0x2b, 0x33, 0x3b, 0x3,  0xb,
        0x13, 0x1b, 0x25, 0x2d, 0x35, 0x3d, 0x5,  0xd,  0x15, 0x1d, 0x27, 0x2f, 0x37,
        0x3f, 0x7,  0xf,  0x17, 0x1f, 0x61, 0x69, 0x71, 0x79, 0x41, 0x49, 0x51, 0x59,
        0x63, 0x6b, 0x73, 0x7b, 0x43, 0x4b, 0x53, 0x5b, 0x65, 0x6d, 0x75, 0x7d, 0x45,
        0x4d, 0x55, 0x5d, 0x67, 0x6f, 0x77, 0x7f, 0x47, 0x4f, 0x57, 0x5f
    };
    static const char hbytes13[128] = {
        0x20, 0x28, 0x30, 0x38, 0x0,  0x8,  0x10, 0x18, 0x22, 0x2a, 0x32, 0x3a, 0x2,
        0xa,  0x12, 0x1a, 0x24, 0x2c, 0x34, 0x3c, 0x4,  0xc,  0x14, 0x1c, 0x26, 0x2e,
        0x36, 0x3e, 0x6,  0xe,  0x16, 0x1e, 0x60, 0x68, 0x70, 0x78, 0x40, 0x48, 0x50,
        0x58, 0x62, 0x6a, 0x72, 0x7a, 0x42, 0x4a, 0x52, 0x5a, 0x64, 0x6c, 0x74, 0x7c,
        0x44, 0x4c, 0x54, 0x5c, 0x66, 0x6e, 0x76, 0x7e, 0x46, 0x4e, 0x56, 0x5e, 0x1,
        0x9,  0x11, 0x19, 0x21, 0x29, 0x31, 0x39, 0x3,  0xb,  0x13, 0x1b, 0x23, 0x2b,
        0x33, 0x3b, 0x5,  0xd,  0x15, 0x1d, 0x25, 0x2d, 0x35, 0x3d, 0x7,  0xf,  0x17,
        0x1f, 0x27, 0x2f, 0x37, 0x3f, 0x41, 0x49, 0x51, 0x59, 0x61, 0x69, 0x71, 0x79,
        0x43, 0x4b, 0x53, 0x5b, 0x63, 0x6b, 0x73, 0x7b, 0x45, 0x4d, 0x55, 0x5d, 0x65,
        0x6d, 0x75, 0x7d, 0x47, 0x4f, 0x57, 0x5f, 0x67, 0x6f, 0x77, 0x7f
    };

    if (mOrder & 4) {
        if (mBpp == 8) {
            int temp_r0 = (int)mRowBytes * 2;
            unsigned char var_r11_3;
            if (((y >> 2) % 4) & 1) {
                var_r11_3 = bytes13[(y % 4) * 0x10 + (x % 16)];
            } else {
                var_r11_3 = bytes02[(y % 4) * 0x10 + (x % 16)];
            }
            if ((int)var_r11_3 > 0x1F) {
                var_r11_3 = (var_r11_3 + temp_r0) - 0x20;
            }
            return ((unsigned char)var_r11_3) + (((y >> 1) & 0xFFFFFFFE) * temp_r0) + ((x << 1) & 0xFFFFFFE0);
        }
        int temp_r10_2 = (y >> 2) % 4;
        int var_r3, var_r8, var_r11_2;
        if ((mWidth > 0x80U) && (mHeight > 0x80U)) {
            var_r3 = (((int)(y % 128) >> 1) & 0xFFFFFFF8) + ((x >> 1) & 0xFFFFFFC0);
            var_r8 = (((int)(x % 128) >> 2) & 0xFFFFFFF8) + ((y >> 2) & 0xFFFFFFE0) + (temp_r10_2 * 2);
            var_r11_2 = (((mHeight % 128) & 0xFFFFFFF0) + (mWidth & 0xFFFFFF80)) * 2;
        } else {
            var_r3 = (y >> 1) & 0xFFFFFFF8;
            var_r8 = ((x >> 2) & 0xFFFFFFF8) + (temp_r10_2 * 2);
            var_r11_2 = (int)mHeight * 2;
        }
        unsigned char temp_r10_3;
        if (temp_r10_2 & 1) {
            temp_r10_3 = hbytes13[((y % 4) << 5) + (x % 32)];
        } else {
            temp_r10_3 = hbytes02[((y % 4) << 5) + (x % 32)];
        }
        int var_r10_2 = (int)temp_r10_3 >> 1;
        nibble = temp_r10_3 & 1;
        if (var_r10_2 > 0x1F) {
            var_r10_2 = (var_r10_2 + var_r11_2) - 0x20;
        }
        return var_r10_2 + ((var_r11_2 * var_r8) + (var_r3 * 4));
    }
    if (mOrder & 0x40) {
        unsigned char temp_r11_3 = mBpp;
        int var_r10_3 = 4;
        if (temp_r11_3 == 4) {
            var_r10_3 = 8;
        }
        unsigned short temp_r31 = mWidth;
        int temp_r11_5 = 4;
        if (temp_r11_3 < 0x10) {
            temp_r11_5 = 8;
        }
        nibble = x & 1;
        int temp_r9_2 = (((temp_r11_3 - 0x20) == 0) & 1) + 1;
        int temp_r7_2 = x % temp_r11_5;
        int temp_r8_2 = ((((((int)temp_r31 / temp_r11_5) * (y / var_r10_3)) + (x / temp_r11_5)) * temp_r9_2 * var_r10_3) + (y % var_r10_3)) * temp_r11_5;
        unsigned int temp_r27 = temp_r31 * temp_r9_2;
        int var_r11 = (int)(mBpp * ((temp_r8_2 + temp_r7_2) % temp_r27)) >> (temp_r9_2 + 2);
        int var_r10 = mRowBytes * ((unsigned int)(temp_r8_2 + temp_r7_2) / temp_r27);
        return var_r11 + var_r10;
    }
    nibble = x & 1;
    int var_r11 = (int)(mBpp * x) >> 3;
    int var_r10 = mRowBytes * y;
    return var_r11 + var_r10;
}

unsigned char RndBitmap::PixelIndex(int i1, int i2) const {
    bool bb;
    int offset = PixelOffset(i1, i2, bb);
    u8 *pixels = mPixels;
    unsigned char ret;
    if (mBpp == 8) {
        ret = *(pixels + offset);
    } else if (bb) {
        ret = (*(pixels + offset) >> 4) & 0xF;
    } else {
        ret = *(pixels + offset) & 0xF;
    }
    return ret;
}

void RndBitmap::SetPixelIndex(int i1, int i2, unsigned char uc) {
    bool bb;
    int offset = PixelOffset(i1, i2, bb);
    u8 *pixels = mPixels;
    if (mBpp == 8) {
        *(pixels + offset) = uc;
    } else if (bb) {
        *(pixels + offset) = uc << 4 | *(pixels + offset) & 0xF;
    } else {
        unsigned char cur = *(pixels + offset) & 0xF0;
        *(pixels + offset) = cur | uc;
    }
}

DECOMP_FORCEACTIVE(
    Bitmap,
    "mBpp == 4",
    "alpha pair doesn't match size or palettization",
    "alpha combination has too many colors"
)

void RndBitmap::ConvertToAlpha() {
    if (mBpp == 24) {
        RndBitmap bmap;
        bmap.Create(*this, 32, mOrder, 0);
        if (mBuffer) {
            _MemFree(mBuffer);
            mBuffer = nullptr;
        }
        mPalette = bmap.mPalette;
        mPixels = bmap.mPixels;
        MILO_ASSERT(!((int)mPixels & 31), 0x342);
        mBuffer = bmap.mBuffer;
        mBpp = bmap.mBpp;
        mRowBytes = bmap.mRowBytes;
        bmap.mBuffer = nullptr;
    }
}

void RndBitmap::SetAlpha(AlphaFlag flag) {
    ConvertToAlpha();
    if (mBpp <= 8) {
        int max = 255;
        int min = 0;
        for (int i = NumPaletteColors() - 1; i >= 0; i--) {
            unsigned char r, g, b, a;
            PaletteColor(i, r, g, b, a);
            switch (flag) {
            case kTransparentBlack:
                if (b == 0 && r == 0 && g == 0) {
                    a = min;
                }
                break;
            case kGrayscaleWhite:
                a = r;
                b = max;
                g = max;
                r = max;
                break;
            case kGrayscaleAlpha:
                a = r;
                break;
            default:
                break;
            }
            SetPaletteColor(i, r, g, b, a);
        }
    } else {
        for (int i = 0; i < mHeight; i++) {
            for (int j = 0; j < mWidth; j++) {
                unsigned char r, g, b, a;
                PixelColor(j, i, r, g, b, a);
                switch (flag) {
                case kTransparentBlack:
                    if (b == 0 && r == 0 && g == 0) {
                        a = 0;
                    }
                    break;
                case kGrayscaleWhite:
                    a = r;
                    b = 255;
                    g = 255;
                    r = 255;
                    break;
                case kGrayscaleAlpha:
                    a = r;
                    break;
                default:
                    break;
                }
                SetPixelColor(j, i, r, g, b, a);
            }
        }
    }
}

void PreMultiplyAlpha(u8 &r, u8 &g, u8 &b, u8 a) {
    float new_alpha = (float)a / 255.0f;
    r *= new_alpha;
    g *= new_alpha;
    b *= new_alpha;
}

void RndBitmap::SetPreMultipliedAlpha() {
    ConvertToAlpha();
    if (mBpp <= 8) {
        int max = 255;
        int min = 0;
        for (int i = NumPaletteColors() - 1; i >= 0; i--) {
            unsigned char r, g, b, a;
            PaletteColor(i, r, g, b, a);
            if (a != 255) {
                PreMultiplyAlpha(r, g, b, a);
                SetPaletteColor(i, r, g, b, a);
            }
        }
    } else {
        for (int i = 0; i < mHeight; i++) {
            for (int j = 0; j < mWidth; j++) {
                unsigned char r, g, b, a;
                PixelColor(j, i, r, g, b, a);
                if (a != 255) {
                    PreMultiplyAlpha(r, g, b, a);
                    SetPixelColor(j, i, r, g, b, a);
                }
            }
        }
    }
}

void RndBitmap::SelfMip() {
    int rowOffset = mRowBytes / 2;
    int pixelBytes = PixelBytes();
    mWidth /= 2;
    int dim = Min<unsigned short>(Height(), Width());

    int i3 = 0;
    int i4 = 0;
    while (dim > 1) {
        if (dim & 1) {
            i3 = 1;
        }
        dim >>= 1;
        i4++;
    }
    int count = i4 + i3 - 3;
    RndBitmap *cur = this;
    RELEASE(mMip);
    for (int i = 0; i < count; i++) {
        cur->mMip = new RndBitmap();
        cur->mMip->Create(
            cur->mWidth >> 1,
            cur->mHeight >> 1,
            mRowBytes,
            mBpp,
            mOrder,
            mPalette,
            mPixels + rowOffset,
            0
        );
        pixelBytes >>= 1;
        cur = cur->mMip;
        rowOffset += pixelBytes;
    }
}

void RndBitmap::GenerateMips() {
    int dim = Min(mWidth, mHeight);
    if (dim > 16U) {
        RELEASE(mMip);
        mMip = new RndBitmap();
        mMip->Create(mWidth >> 1, mHeight >> 1, 0, mBpp, mOrder, mPalette, 0, 0);
        int i18 = 0;
        for (int i = 0; i < mMip->mHeight; i++) {
            int i181 = i18 + 1;
            int i17 = 0;
            for (int j = 0; j < mMip->mWidth; j++) {
                int i171 = i17 + 1;
                unsigned char r, g, b, a;
                PixelColor(i17, i18, r, g, b, a);
                unsigned short rsum = r;
                unsigned short gsum = g;
                unsigned short bsum = b;
                unsigned short asum = a;
                PixelColor(i171, i18, r, g, b, a);
                rsum += r;
                gsum += g;
                bsum += b;
                asum += a;
                PixelColor(i17, i181, r, g, b, a);
                rsum += r;
                gsum += g;
                bsum += b;
                asum += a;
                PixelColor(i171, i181, r, g, b, a);
                rsum += r;
                gsum += g;
                bsum += b;
                asum += a;
                mMip->SetPixelColor(j, i, rsum >> 2, gsum >> 2, bsum >> 2, asum >> 2);
                i17 += 2;
            }
            i18 += 2;
        }
        mMip->GenerateMips();
    }
}

RndBitmap *RndBitmap::DetachMip() {
    RndBitmap *m = mMip;
    mMip = NULL;
    return m;
}

void RndBitmap::SetMip(RndBitmap *bm) {
    RndBitmap *mip = mMip;
    delete mip;
    mMip = 0;
    if (bm) {
        MILO_ASSERT(mWidth / 2 == bm->Width(), 1101);
        MILO_ASSERT(mHeight / 2 == bm->Height(), 1102);
        MILO_ASSERT(mOrder == bm->Order(), 1103);
        MILO_ASSERT(mBpp == bm->Bpp(), 1104);
        mMip = bm;
    }
}

bool RndBitmap::LoadBmp(BinStream *bs) {
    unsigned short us;
    *bs >> us;
    if (us != 0x4D42) {
        MILO_WARN("%s not BMP format", bs->Name());
        return false;
    } else {
        tagBITMAPFILEHEADER header;
        *bs >> header;
        return LoadDIB(bs, header.bfOffBits);
    }
}

bool RndBitmap::LoadDIB(BinStream *bs, unsigned int offbits) {
    tagBITMAPINFOHEADER infoheader;
    *bs >> infoheader;
    if (infoheader.biBitCount < 4) {
        TheDebug.Notify(
            MakeString("%s: Unsupported bit depth %d", bs->Name(), infoheader.biBitCount)
        );
        return false;
    }
    if (infoheader.biCompression != 0) {
        TheDebug.Notify(
            MakeString(
                "%s: Unsupported compression %d", bs->Name(), infoheader.biCompression
            )
        );
        return false;
    }
    unsigned int paletteBytes = 0;
    if (infoheader.biBitCount <= 8) {
        paletteBytes = (1 << infoheader.biBitCount) * 4;
    }
    int rowBytes;
    if (infoheader.biBitCount == 4 && (infoheader.biWidth & 1)) {
        rowBytes = ((infoheader.biWidth + 1) * infoheader.biBitCount) >> 3;
    } else {
        rowBytes = (infoheader.biWidth * infoheader.biBitCount) >> 3;
    }
    rowBytes = (rowBytes + 3) & ~3;
    int pixelBytes = rowBytes * infoheader.biHeight;
    void *buf = _MemAlloc(paletteBytes + pixelBytes, 0);
    void *palette = nullptr;
    if (paletteBytes != 0) {
        palette = buf;
        if ((infoheader.biClrUsed == 0) ||
            (infoheader.biClrUsed > (unsigned int)(1 << infoheader.biBitCount))) {
            bs->Read(buf, paletteBytes);
        } else {
            memset(buf, 0, paletteBytes);
            bs->Read(buf, infoheader.biClrUsed * 4);
        }
    }
    void *pixels = (void *)((char *)buf + paletteBytes);
    bs->Seek(offbits, BinStream::kSeekBegin);
    if (infoheader.biHeight < 0) {
        bs->Read(pixels, pixelBytes);
    } else {
        for (int i = infoheader.biHeight - 1; i >= 0; i--) {
            bs->Read((void *)((char *)pixels + i * rowBytes), rowBytes);
        }
    }
    if (infoheader.biBitCount == 4) {
        unsigned char *p = (unsigned char *)pixels;
        unsigned char *end = p + pixelBytes;
        for (; p != end; p++) {
            *p = ((*p >> 4) & 0x0F) | ((*p << 4) & 0xF0);
        }
    }
    if ((int)infoheader.biXPelsPerMeter != 0xB11) {
        unsigned int palCount = (unsigned int)paletteBytes >> 2;
        if ((paletteBytes -= 4) >= 0) {
            unsigned int blocks = palCount >> 3;
            int t;
            if (blocks != 0) {
                do {
                    t = paletteBytes - 4; ((unsigned char *)palette + paletteBytes)[3] = 0xFF;
                    paletteBytes = t - 4; ((unsigned char *)palette + t)[3] = 0xFF;
                    t = paletteBytes - 4; ((unsigned char *)palette + paletteBytes)[3] = 0xFF;
                    paletteBytes = t - 4; ((unsigned char *)palette + t)[3] = 0xFF;
                    t = paletteBytes - 4; ((unsigned char *)palette + paletteBytes)[3] = 0xFF;
                    paletteBytes = t - 4; ((unsigned char *)palette + t)[3] = 0xFF;
                    t = paletteBytes - 4; ((unsigned char *)palette + paletteBytes)[3] = 0xFF;
                    paletteBytes = t - 4; ((unsigned char *)palette + t)[3] = 0xFF;
                } while (--blocks != 0);
                palCount &= 7;
                if (palCount == 0) goto palette_done;
            }
            do {
                *(((unsigned char *)palette + (paletteBytes + 3))) = 0xFF;
                paletteBytes -= 4;
                palCount--;
            } while (palCount != 0);
        }
palette_done:
        if (infoheader.biBitCount == 16) {
            unsigned int pixCount = (unsigned int)pixelBytes >> 1;
            if ((pixelBytes -= 2) >= 0) {
                unsigned int blocks = pixCount >> 3;
                if (blocks != 0) {
                    unsigned char *pixPtr = (unsigned char *)pixels + pixelBytes;
                    do {
                        blocks--;
                        pixelBytes -= 16;
                        pixPtr[1] |= 0x80;
                        pixPtr[-1] |= 0x80;
                        pixPtr[-3] |= 0x80;
                        pixPtr[-5] |= 0x80;
                        pixPtr[-7] |= 0x80;
                        pixPtr[-9] |= 0x80;
                        pixPtr[-11] |= 0x80;
                        pixPtr[-13] |= 0x80;
                        pixPtr -= 16;
                    } while (blocks != 0);
                    pixCount &= 7;
                    if (pixCount == 0) goto pixels_done;
                }
                do {
                    *((unsigned char *)pixels + pixelBytes + 1) |= 0x80;
                    pixelBytes -= 2;
                    pixCount--;
                } while (pixCount != 0);
            }
pixels_done:;
        }
    }
    Create(
        infoheader.biWidth,
        infoheader.biHeight,
        rowBytes,
        infoheader.biBitCount,
        0,
        palette,
        pixels,
        buf
    );
    return true;
}

bool RndBitmap::LoadBmp(const char *filename, bool wantMips, bool noAlpha) {
    FileStream *stream = new FileStream(filename, FileStream::kRead, true);
    if (stream->Fail()) {
        delete stream;
        return false;
    } else {
        if (!LoadBmp(stream)) {
            delete stream;
            return false;
        } else {
            delete stream;
            if (!noAlpha) {
                ProcessFlags(filename, wantMips);
            }
            return true;
        }
    }
}

static inline bool FileContains(const char *filename, const char *key) {
    return strstr(filename, MakeString("%s.", key))
        || strstr(filename, MakeString("%s_", key));
}

bool RndBitmap::ProcessFlags(const char *filename, bool wantMips) {
    if (FileContains(filename, "_tb")) {
        SetAlpha(kTransparentBlack);
    } else if (FileContains(filename, "_gw")) {
        SetAlpha(kGrayscaleWhite);
    } else if (FileContains(filename, "_ga")) {
        SetAlpha(kGrayscaleAlpha);
    }

    if (FileContains(filename, "_pma")) {
        SetPreMultipliedAlpha();
    }
    if (FileContains(filename, "_selfmip")) {
        SelfMip();
    } else if (wantMips) {
        if (!FileContains(filename, "_nomip")) {
            GenerateMips();
        }
    } else if (FileContains(filename, "_mip")) {
        GenerateMips();
    }
    return true;
}

bool RndBitmap::SaveBmp(const char *) const {
    MILO_ASSERT(0, 0x610);
    return false;
}

bool RndBitmap::SaveBmp(BinStream *file) const {
    MILO_ASSERT(file, 0x626);
    if (file->Fail()) {
        return false;
    } else if (mOrder & 1) {
        MILO_WARN("Order isn't kARGB");
        return false;
    } else {
        SaveBmpHeader(file);
        SaveBmpPixels(file);
        return true;
    }
}

void RndBitmap::SaveBmpHeader(BinStream *file) const {
    tagBITMAPFILEHEADER fileheader;
    tagBITMAPINFOHEADER infoheader;

    MILO_ASSERT(file, 0x63F);
    unsigned short us = 0x4D42; // "BM" in ASCII, used to identify that this is a bmp file
    *file << us;
    fileheader.bfOffBits = PaletteBytes() + 54;
    fileheader.bfSize = fileheader.bfOffBits + PixelBytes();
    fileheader.bfReserved1 = 0;
    fileheader.bfReserved2 = 0;
    *file << fileheader;

    infoheader.biSize = 40;
    infoheader.biWidth = mWidth;
    infoheader.biHeight = mHeight;
    infoheader.biPlanes = 1;
    infoheader.biBitCount = mBpp;
    infoheader.biCompression = 0;
    infoheader.biSizeImage = 0;
    infoheader.biXPelsPerMeter = 0xb11;
    infoheader.biYPelsPerMeter = 0xb11;
    infoheader.biClrUsed = 0;
    infoheader.biClrImportant = 0;
    *file << infoheader;
    if (mPalette) {
        file->Write(mPalette, (1 << mBpp) << 2);
    }
}

void RndBitmap::SaveBmpPixels(BinStream *file) const {
    for (int i = mHeight - 1; i >= 0; i--) {
        u8 *pixels = mPixels + mRowBytes * i;
        if (mBpp == 4) {
            u8 *pixelIt = pixels;
            for (; pixelIt != pixels + mRowBytes; pixelIt++) {
                unsigned char pix = ((*pixelIt & 0xF0) >> 4) | ((*pixelIt & 0x0F) << 4);
                *file << pix;
            }
        } else {
            file->Write(pixels, mRowBytes);
        }
    }
}

bool RndBitmap::IsTranslucent() const {
    if (mBpp == 24)
        return false;
    for (int i = 0; i < mHeight; i++) {
        for (int j = 0; j < mWidth; j++) {
            unsigned char r, g, b, a;
            PixelColor(j, i, r, g, b, a);
            if (a < 253)
                return true;
        }
    }
    return false;
}

struct ColorRgba {
    u8 r, g, b, a;
};

bool RndBitmap::SamePaletteColors(const RndBitmap &bmap) const {
    if (mPalette == bmap.Palette())
        return true;
    else {
        for (int i = (1 << mBpp) - 1; i >= 0; i--) {
            uint myColors;
            uint otherColors;
            ColorRgba *myC = (ColorRgba *)&myColors;
            ColorRgba *otherC = (ColorRgba *)&otherColors;

            PaletteColor(i, myC->r, myC->g, myC->b, myC->a);
            bmap.PaletteColor(i, otherC->r, otherC->g, otherC->b, otherC->a);
            if (myColors != otherColors)
                return false;
        }
        return true;
    }
}

bool RndBitmap::SamePixelFormat(const RndBitmap &bm) const {
    if (mBpp != bm.Bpp() || mOrder != bm.Order())
        return false;
    if (mPalette && bm.Palette()) {
        return SamePaletteColors(bm);
    } else
        return true;
}

void RndBitmap::Blt(
    const RndBitmap &bm, int dX, int dY, int sX, int sY, int width, int height
) {
    MILO_ASSERT(dX + width <= mWidth, 1728);
    MILO_ASSERT(dY + height <= mHeight, 1729);
    MILO_ASSERT(sX + width <= bm.Width(), 1730);
    MILO_ASSERT(sY + height <= bm.Height(), 1731);
    if (SamePixelFormat(bm)) {
        if (mOrder & 0x38) {
            MILO_ASSERT(!((dX | dY | sX | sY | width | height) & 0x3), 0x6CC);
        }
        int count = width * mBpp >> 3;
        for (; height > 0; height--, dY++, sY++) {
            void *dst = mPixels + (dY * mRowBytes) + (dX * mBpp >> 3);
            void *src = bm.mPixels + (sY * bm.RowBytes()) + (sX * bm.Bpp() >> 3);
            memcpy(dst, src, count);
        }
    } else {
        if (mOrder & 0x38) {
            MILO_WARN(
                "RndBitmap::Blt: Can't blt to DXT formatted textures, changing to rgba."
            );
            Create(mWidth, mHeight, 0, 0x20, 0, 0, 0, 0);
        }
        if (mPalette && bm.Palette()) {
            unsigned char colorBuffer[256];
            for (int i = bm.NumPaletteColors() - 1; i >= 0; i--) {
                unsigned char r, g, b, a;
                bm.PaletteColor(i, r, g, b, a);
                colorBuffer[i] = NearestColor(r, g, b, a);
            }
            int origDX = dX, origSX = sX, origWidth = width;
            for (; height > 0; height--, dY++, sY++) {
                for (dX = origDX, sX = origSX, width = origWidth; width > 0; width--, dX++, sX++) {
                    SetPixelIndex(dX, dY, colorBuffer[bm.PixelIndex(sX, sY)]);
                }
            }
        } else {
            int origDX = dX, origSX = sX, origWidth = width;
            for (; height > 0; height--, dY++, sY++) {
                for (dX = origDX, sX = origSX, width = origWidth; width > 0; width--, dX++, sX++) {
                    unsigned char r, g, b, a;
                    bm.PixelColor(sX, sY, r, g, b, a);
                    SetPixelColor(dX, dY, r, g, b, a);
                }
            }
        }
    }
}

void RndBitmap::PixelColor(
    int x, int y, unsigned char &r, unsigned char &g, unsigned char &b, unsigned char &a
) const {
    if (mPalette) {
        PaletteColor(PixelIndex(x, y), r, g, b, a);
    } else if (mOrder & 0x38) {
        DxtColor(x, y, r, g, b, a);
    } else {
        bool boolbool;
        const unsigned char *p = mPixels + PixelOffset(x, y, boolbool);
        ConvertColor(p, r, g, b, a);
    }
}

void RndBitmap::SetPixelColor(
    int x, int y, unsigned char r, unsigned char g, unsigned char b, unsigned char a
) {
    if (mPalette) {
        SetPixelIndex(x, y, NearestColor(r, g, b, a));
    } else {
        bool boolbool;
        unsigned char *p = mPixels + PixelOffset(x, y, boolbool);
        ConvertColor(r, g, b, a, p);
    }
}

void DecodeDxtColor(
    unsigned char *blockData,
    int pixelX,
    int pixelY,
    bool hasDxt1Alpha,
    unsigned char &r,
    unsigned char &g,
    unsigned char &b,
    unsigned char &a
) {
    unsigned short color0 = *(unsigned short *)blockData;
    unsigned short color1 = *((unsigned short *)blockData + 1);

    unsigned char *rowPtr = blockData + pixelY;
    int colorIdx = ((int)rowPtr[4] >> (pixelX << 1)) & 3;

    unsigned char r0 = (color0 >> 8) & 0xF8;
    a = 0xFF;
    unsigned char r1 = (color1 >> 8) & 0xF8;
    unsigned char g0 = (color0 >> 3) & 0xFC;
    unsigned char g1 = (color1 >> 3) & 0xFC;
    unsigned char b0 = (color0 << 3) & 0xF8;
    unsigned char b1 = (color1 << 3) & 0xF8;

    if (colorIdx == 0) {
        r = r0;
        g = g0;
        b = b0;
        return;
    }

    if (colorIdx == 1) {
        r = r1;
        g = g1;
        b = b1;
        return;
    }

    if (color0 <= color1 && hasDxt1Alpha) {
        r = ((int)r0 + (int)r1) / 2;
        g = ((int)g0 + (int)g1) / 2;
        b = ((int)b0 + (int)b1) / 2;
        if (colorIdx == 3) {
            a = 0;
        }
    } else {
        int w0 = 4 - colorIdx;
        int w1 = colorIdx - 1;
        r = (unsigned int)((w0 * r0) + (w1 * r1)) / 3U;
        g = (unsigned int)((w0 * g0) + (w1 * g1)) / 3U;
        b = (unsigned int)((w0 * b0) + (w1 * b1)) / 3U;
    }
}

void DecodeDxt3Alpha(unsigned char *uc, int i, int j, unsigned char &alpha) {
    unsigned short *bytepair = (unsigned short *)uc;
    int i1 = bytepair[j] >> (i << 2);
    alpha = ((i1 << 4) & 0xF0) | (i1 & 0xF);
}

void DecodeDxt5Alpha(unsigned char *uc, int i, int j, unsigned char &alpha) {
    int pixel = i + (j << 2);
    unsigned char byteOffsets[16] = {
        0, 0, 0, 1, 1, 1, 2, 2,
        3, 3, 3, 4, 4, 4, 5, 5,
    };
    unsigned char bitOffsets[16] = {
        0, 3, 6, 1, 4, 7, 2, 5,
        0, 3, 6, 1, 4, 7, 2, 5,
    };
    unsigned char a0 = uc[0];
    unsigned char a1 = uc[1];
    unsigned int bit = bitOffsets[pixel];
    unsigned int byte = byteOffsets[pixel];
    int code;
    if (bit < 6) {
        code = (uc[byte + 2] >> bit) & 7;
    } else {
        unsigned char nextByte = (unsigned char)(byte + 1);
        if (bit == 6) {
            code = ((uc[nextByte + 2] & 1) << 2) | ((uc[byte + 2] >> 6) & 3);
        } else {
            code = ((uc[nextByte + 2] & 3) << 1) | ((uc[byte + 2] >> 7) & 1);
        }
    }
    if (code == 0) {
        alpha = a0;
    } else if (code == 1) {
        alpha = a1;
    } else if (a0 <= a1) {
        if (code == 6) {
            alpha = 0;
        } else if (code == 7) {
            alpha = 0xFF;
        } else {
            alpha = (unsigned char)(((unsigned int)(6 - code) * a0 + (unsigned int)(code - 1) * a1 + 2U) / 5U);
        }
    } else {
        alpha = (unsigned char)(((unsigned int)(8 - code) * a0 + (unsigned int)(code - 1) * a1 + 3U) / 7U);
    }
}

void RndBitmap::DxtColor(
    int x, int y, unsigned char &r, unsigned char &g, unsigned char &b, unsigned char &a
) const {
    int dxt = mOrder & 0x38;
    MILO_ASSERT(dxt != 0, 0x7EE);

    int xQuotient = x / 4;
    int yQuotient = y / 4;
    int i2 = xQuotient + (mWidth >> 2) * yQuotient;
    int xRemainder = x % 4;
    int yRemainder = y % 4;

    if (dxt == 8) {
        DecodeDxtColor(mPixels + i2 * 8, xRemainder, yRemainder, true, r, g, b, a);
    } else {
        u8 *newpixels = mPixels + i2 * 0x10;
        unsigned char throwaway;
        DecodeDxtColor(newpixels + 8, xRemainder, yRemainder, false, r, g, b, throwaway);
        if (dxt == 0x10) {
            DecodeDxt3Alpha(newpixels, xRemainder, yRemainder, a);
        } else {
            DecodeDxt5Alpha(newpixels, xRemainder, yRemainder, a);
        }
    }
}

int RndBitmap::PaletteOffset(int i) const {
    if ((mOrder & 2) && mBpp == 8) {
        if ((i & 0x18) == 8) {
            i += 8;
        } else if ((i & 0x18) == 0x10) {
            i -= 8;
        }
    }
    return i;
}

void RndBitmap::PaletteColor(
    int i, unsigned char &r, unsigned char &g, unsigned char &b, unsigned char &a
) const {
    ConvertColor(mPalette + PaletteOffset(i) * 4, r, g, b, a);
}

void RndBitmap::SetPaletteColor(
    int idx, unsigned char r, unsigned char g, unsigned char b, unsigned char a
) {
    ConvertColor(r, g, b, a, mPalette + PaletteOffset(idx) * 4);
}

unsigned char RndBitmap::RowNonTransparent(int x, int y, int z, int *iptr) {
    for (int i = x; i < y; i++) {
        unsigned char r, g, b, a;
        PixelColor(i, z, r, g, b, a);
        if (a != 0) {
            *iptr = i;
            return a;
        }
    }
    return 0;
}

unsigned char RndBitmap::ColumnNonTransparent(int x, int y, int z, int *iptr) {
    for (int i = y; i < z; i++) {
        unsigned char r, g, b, a;
        PixelColor(x, i, r, g, b, a);
        if (a != 0) {
            *iptr = i;
            return a;
        }
    }
    return 0;
}

void RndBitmap::Save(BinStream &bs) const {
    SaveHeader(bs);
    if (mPalette) {
        bs.Write(mPalette, PaletteBytes());
    }
    const RndBitmap *m = this;
    while (m) {
        WriteChunks(bs, m->Pixels(), m->PixelBytes(), 0x8000);
        m = m->mMip;
    }
}

void RndBitmap::Load(BinStream &bs) {
    u8 mipCt;
    LoadHeader(bs, mipCt);
    if (mBuffer) {
        _MemFree(mBuffer);
        mBuffer = 0;
    }
    mPalette = 0;
    AllocateBuffer();
    if (mPalette)
        bs.Read(mPalette, PaletteBytes());
    ReadChunks(bs, mPixels, PixelBytes(), 0x8000);
    RELEASE(mMip);
    RndBitmap *workingMip = this;
    int working_w = mWidth;
    int working_h = mHeight;
    while (mipCt--) {
        RndBitmap *newMip = new RndBitmap();
        workingMip->mMip = newMip;
        workingMip = newMip;
        working_w = working_w >> 1;
        working_h = working_h >> 1;
        newMip->Create(working_w, working_h, 0, mBpp, mOrder, mPalette, 0, 0);
        ReadChunks(bs, newMip->Pixels(), newMip->PixelBytes(), 0x8000);
    }
}

bool RndBitmap::LoadSafely(BinStream &bs, int w, int h) {
    u8 mips;
    LoadHeader(bs, mips);
    if (mWidth > w || mHeight > h) {
        MILO_WARN(
            "Something is wrong with the bitmap you're loading from %s, w = %d, h = %d",
            bs.Name(),
            mWidth,
            mHeight
        );
        Create(8, 8, 0, 0x20, 0, NULL, NULL, NULL);
        return false;
    } else if (mBpp * mWidth / 8 != mRowBytes) {
        MILO_WARN(
            "Something is wrong with the bitmap you're loading from %s, w = %d, bpp = %d, row bytes: %d",
            bs.Name(),
            mWidth,
            mBpp,
            mRowBytes
        );
        Create(8, 8, 0, 0x20, 0, NULL, NULL, NULL);
        return false;
    }
    if (mBuffer) {
        _MemFree(mBuffer);
        mBuffer = NULL;
    }
    mPalette = NULL;
    AllocateBuffer();
    if (mPalette)
        bs.Read(mPalette, PaletteBytes());
    ReadChunks(bs, mPixels, PixelBytes(), 0x8000);
    RELEASE(mMip);
    RndBitmap *cur = this;
    int lw = mWidth;
    int lh = mHeight;
    while (mips-- != 0) {
        cur->mMip = new RndBitmap();
        cur = cur->mMip;
        lw >>= 1;
        lh >>= 1;
        cur->Create(lw, lh, 0, mBpp, mOrder, mPalette, NULL, NULL);
        ReadChunks(bs, cur->Pixels(), cur->PixelBytes(), 0x8000);
    }
    return true;
}