// Dxt1Compress: DXT1/DXT5 texture-block compression helpers.
//
// Adapted from Mesa's texcompress_s3tc_tmp.h (libtxc_dxtn 1.0,
// Copyright (C) 2004 Roland Scheidegger), with Rock Band 3's Wii GX
// tiled-texture pixel addressing (PixelOffset).
//
// All members are static; the Dxt1Compress class is a namespace-style
// holder for the texture compression helpers.

#include <stdlib.h>

class Dxt1Compress {
public:
    static int PixelOffset(int i, int j, int width, int height, int bpp_in_other_dim, int bpp);
    static void extractsrccolors(
        int width,
        int height,
        int blockX,
        int blockY,
        unsigned char srcpixels[4][4][4],
        const unsigned char *srcaddr,
        int bppOther,
        int bpp,
        int numxpixels,
        int numypixels
    );
    static void fancybasecolorsearch(
        unsigned char *blkaddr,
        unsigned char srccolors[4][4][4],
        unsigned char *bestcolor[2],
        int numxpixels,
        int numypixels,
        int type,
        int haveAlpha
    );
    static void storedxtencodedblock(
        unsigned char *blkaddr,
        unsigned char srccolors[4][4][4],
        unsigned char *bestcolor[2],
        int numxpixels,
        int numypixels,
        unsigned int type,
        int haveAlpha
    );
    static void encodedxtcolorblockfaster(
        unsigned char *blkaddr,
        unsigned char srccolors[4][4][4],
        int numxpixels,
        int numypixels,
        unsigned int type
    );
    static void writedxt5encodedalphablock(
        unsigned char *blkaddr,
        unsigned char alphabase1,
        unsigned char alphabase2,
        unsigned char *alphaenc
    );
    static void encodedxt5alpha(
        unsigned char *blkaddr,
        unsigned char srccolors[4][4][4],
        int numxpixels,
        int numypixels
    );
    static void tx_compress_dxtn(
        int width,
        int height,
        const unsigned char *srcPixData,
        int destFormat,
        unsigned char *dest,
        int srcBpp,
        int dstBpp,
        int dstRowStride
    );
    static void CompressImage(
        const unsigned char *src,
        int width,
        int height,
        void *dst,
        int srcBpp,
        int dstBpp,
        int dstRowStride
    );
};

// ---------------------------------------------------------------------------
// Weights used for the error function. Mesa comment: weights (unsquared
// 2/4/1) according to rgb->luminance conversion.
// ---------------------------------------------------------------------------
#define REDWEIGHT 4
#define GREENWEIGHT 16
#define BLUEWEIGHT 1
#define ALPHACUT 127

// ---------------------------------------------------------------------------
// PixelOffset: Wii GX tiled-texture byte offset for pixel (i, j).
//
// The texture is laid out as tiles; tile width is (bpp == 4 ? 8 : 4) pixels
// wide and (bpp < 16 ? 8 : 4) pixels tall. Each tile is bpp/8 * tileW * tileH
// bytes, with the byte address within the tile being (i % tileW * bpp) / 8
// + (i / tileW) * tileBytes plus equivalent for j.
// ---------------------------------------------------------------------------
int Dxt1Compress::PixelOffset(int i, int j, int width, int height, int bpp_in_other_dim, int bpp) {
    int tileH, tileW, shift;
    int rowTiles, yTile, iTile, tileSize, tileIdx, tileBytes;
    int xInTile, yInTile, linear, divisor, row, rem;
    (void)height;
    tileH = 4;
    if (bpp == 4) tileH = 8;
    tileW = 4;
    if (bpp < 16) tileW = 8;
    shift = ((bpp - 32) == 0 ? 1 : 0) + 1;
    rowTiles = width / tileW;
    yTile = j / tileH;
    iTile = i / tileW;
    tileSize = tileH * tileW;
    tileIdx = iTile + rowTiles * yTile;
    tileBytes = tileSize * shift;
    xInTile = i - iTile * tileW;
    yInTile = j - yTile * tileH;
    linear = tileW * yInTile + tileIdx * tileBytes + xInTile;
    divisor = width * shift;
    row = (unsigned)linear / (unsigned)divisor;
    rem = linear - row * divisor;
    return row * bpp_in_other_dim + ((rem * bpp) >> (shift + 2));
}

// ---------------------------------------------------------------------------
// extractsrccolors: read a 4x4 RGBA block from a tiled Wii GX texture into
// srcpixels[row][col][component]. RB3's version reads (R, G, B, A) from
// byte offsets (+1, +0x20, +0x21, 0) — this corresponds to the Wii GX
// in-memory layout of the source surface.
// ---------------------------------------------------------------------------
void Dxt1Compress::extractsrccolors(
    int width,
    int height,
    int blockX,
    int blockY,
    unsigned char srcpixels[4][4][4],
    const unsigned char *srcaddr,
    int bppOther,
    int bpp,
    int numxpixels,
    int numypixels
) {
    unsigned char *row;
    int j;
    int x, y;
    int i;
    for (y = 0; y < numypixels; y++) {
        row = &srcpixels[y][0][0];
        j = blockY + y;
        for (x = 0; x < numxpixels; x++) {
            i = blockX + x;
            const unsigned char *p = srcaddr + PixelOffset(i, j, width, height, bppOther, bpp);
            row[0] = p[1];
            row[1] = p[0x20];
            row[2] = p[0x21];
            row[3] = p[0];
            row += 4;
        }
    }
}

// ---------------------------------------------------------------------------
// fancybasecolorsearch: refine the two endpoint colors of a DXT1 block via
// a luminance-weighted distance metric, then snap them to 5/6/5 bit packed
// order. See Mesa for the original.
// ---------------------------------------------------------------------------
void Dxt1Compress::fancybasecolorsearch(
    unsigned char * /*blkaddr*/,
    unsigned char srccolors[4][4][4],
    unsigned char *bestcolor[2],
    int numxpixels,
    int numypixels,
    int /*type*/,
    int /*haveAlpha*/
) {
    int i, j, colors, z;
    unsigned int pixerror, pixerrorbest;
    int pixerrorred, pixerrorgreen, pixerrorblue;
    int colordist, blockerrlin[2][3];
    unsigned char nrcolor[2];
    int pixerrorcolorbest[3];
    unsigned char enc = 0;
    unsigned char cv[4][4];
    unsigned char testcolor[2][3];

    pixerrorcolorbest[0] = 0;
    pixerrorcolorbest[1] = 0;
    pixerrorcolorbest[2] = 0;

    if (((bestcolor[0][0] & 0xf8) << 8 | (bestcolor[0][1] & 0xfc) << 3 | bestcolor[0][2] >> 3) <
        ((bestcolor[1][0] & 0xf8) << 8 | (bestcolor[1][1] & 0xfc) << 3 | bestcolor[1][2] >> 3)) {
        testcolor[0][0] = bestcolor[0][0];
        testcolor[0][1] = bestcolor[0][1];
        testcolor[0][2] = bestcolor[0][2];
        testcolor[1][0] = bestcolor[1][0];
        testcolor[1][1] = bestcolor[1][1];
        testcolor[1][2] = bestcolor[1][2];
    } else {
        testcolor[1][0] = bestcolor[0][0];
        testcolor[1][1] = bestcolor[0][1];
        testcolor[1][2] = bestcolor[0][2];
        testcolor[0][0] = bestcolor[1][0];
        testcolor[0][1] = bestcolor[1][1];
        testcolor[0][2] = bestcolor[1][2];
    }

    for (i = 0; i < 3; i++) {
        cv[0][i] = testcolor[0][i];
        cv[1][i] = testcolor[1][i];
        cv[2][i] = (testcolor[0][i] * 2 + testcolor[1][i]) / 3;
        cv[3][i] = (testcolor[0][i] + testcolor[1][i] * 2) / 3;
    }

    blockerrlin[0][0] = 0;
    blockerrlin[0][1] = 0;
    blockerrlin[0][2] = 0;
    blockerrlin[1][0] = 0;
    blockerrlin[1][1] = 0;
    blockerrlin[1][2] = 0;
    nrcolor[0] = 0;
    nrcolor[1] = 0;

    for (j = 0; j < numypixels; j++) {
        for (i = 0; i < numxpixels; i++) {
            pixerrorbest = 0xffffffff;
            for (colors = 0; colors < 4; colors++) {
                colordist = srccolors[j][i][0] - cv[colors][0];
                pixerror = colordist * colordist * REDWEIGHT;
                pixerrorred = colordist;
                colordist = srccolors[j][i][1] - cv[colors][1];
                pixerror += colordist * colordist * GREENWEIGHT;
                pixerrorgreen = colordist;
                colordist = srccolors[j][i][2] - cv[colors][2];
                pixerror += colordist * colordist * BLUEWEIGHT;
                pixerrorblue = colordist;
                if (pixerror < pixerrorbest) {
                    enc = (unsigned char)colors;
                    pixerrorbest = pixerror;
                    pixerrorcolorbest[0] = pixerrorred;
                    pixerrorcolorbest[1] = pixerrorgreen;
                    pixerrorcolorbest[2] = pixerrorblue;
                }
            }
            if (enc == 0) {
                for (z = 0; z < 3; z++) {
                    blockerrlin[0][z] += 3 * pixerrorcolorbest[z];
                }
                nrcolor[0] += 3;
            } else if (enc == 2) {
                for (z = 0; z < 3; z++) {
                    blockerrlin[0][z] += 2 * pixerrorcolorbest[z];
                }
                nrcolor[0] += 2;
                for (z = 0; z < 3; z++) {
                    blockerrlin[1][z] += 1 * pixerrorcolorbest[z];
                }
                nrcolor[1] += 1;
            } else if (enc == 3) {
                for (z = 0; z < 3; z++) {
                    blockerrlin[0][z] += 1 * pixerrorcolorbest[z];
                }
                nrcolor[0] += 1;
                for (z = 0; z < 3; z++) {
                    blockerrlin[1][z] += 2 * pixerrorcolorbest[z];
                }
                nrcolor[1] += 2;
            } else if (enc == 1) {
                for (z = 0; z < 3; z++) {
                    blockerrlin[1][z] += 3 * pixerrorcolorbest[z];
                }
                nrcolor[1] += 3;
            }
        }
    }
    if (nrcolor[0] == 0) nrcolor[0] = 1;
    if (nrcolor[1] == 0) nrcolor[1] = 1;
    for (j = 0; j < 2; j++) {
        for (i = 0; i < 3; i++) {
            int newvalue = testcolor[j][i] + blockerrlin[j][i] / nrcolor[j];
            if (newvalue <= 0)
                testcolor[j][i] = 0;
            else if (newvalue >= 255)
                testcolor[j][i] = 255;
            else
                testcolor[j][i] = (unsigned char)newvalue;
        }
    }

    if ((abs(testcolor[0][0] - testcolor[1][0]) < 8) &&
        (abs(testcolor[0][1] - testcolor[1][1]) < 4) &&
        (abs(testcolor[0][2] - testcolor[1][2]) < 8)) {
        unsigned char coldiffred, coldiffgreen, coldiffblue, coldiffmax, factor;
        unsigned char ind0, ind1;

        coldiffred = (unsigned char)abs(testcolor[0][0] - testcolor[1][0]);
        coldiffgreen = (unsigned char)(2 * abs(testcolor[0][1] - testcolor[1][1]));
        coldiffblue = (unsigned char)abs(testcolor[0][2] - testcolor[1][2]);
        coldiffmax = coldiffred;
        if (coldiffmax < coldiffgreen) coldiffmax = coldiffgreen;
        if (coldiffmax < coldiffblue) coldiffmax = coldiffblue;
        if (coldiffmax > 0) {
            if (coldiffmax > 4) factor = 2;
            else if (coldiffmax > 2) factor = 3;
            else factor = 4;
            if (testcolor[1][1] >= testcolor[0][1]) {
                ind1 = 1;
                ind0 = 0;
            } else {
                ind1 = 0;
                ind0 = 1;
            }
            if ((testcolor[ind1][1] + factor * coldiffgreen) <= 255)
                testcolor[ind1][1] += factor * coldiffgreen;
            else
                testcolor[ind1][1] = 255;
            if ((testcolor[ind1][0] - testcolor[ind0][1]) > 0) {
                if ((testcolor[ind1][0] + factor * coldiffred) <= 255)
                    testcolor[ind1][0] += factor * coldiffred;
                else
                    testcolor[ind1][0] = 255;
            } else {
                if ((testcolor[ind0][0] + factor * coldiffred) <= 255)
                    testcolor[ind0][0] += factor * coldiffred;
                else
                    testcolor[ind0][0] = 255;
            }
            if ((testcolor[ind1][2] - testcolor[ind0][2]) > 0) {
                if ((testcolor[ind1][2] + factor * coldiffblue) <= 255)
                    testcolor[ind1][2] += factor * coldiffblue;
                else
                    testcolor[ind1][2] = 255;
            } else {
                if ((testcolor[ind0][2] + factor * coldiffblue) <= 255)
                    testcolor[ind0][2] += factor * coldiffblue;
                else
                    testcolor[ind0][2] = 255;
            }
        }
    }

    if (((testcolor[0][0] & 0xf8) << 8 | (testcolor[0][1] & 0xfc) << 3 | testcolor[0][2] >> 3) <
        ((testcolor[1][0] & 0xf8) << 8 | (testcolor[1][1] & 0xfc) << 3 | testcolor[1][2]) >> 3) {
        for (i = 0; i < 3; i++) {
            bestcolor[0][i] = testcolor[0][i];
            bestcolor[1][i] = testcolor[1][i];
        }
    } else {
        for (i = 0; i < 3; i++) {
            bestcolor[0][i] = testcolor[1][i];
            bestcolor[1][i] = testcolor[0][i];
        }
    }
}

// ---------------------------------------------------------------------------
// storedxtencodedblock: pick the 4-color encoding (always) and additionally
// the 3-color encoding when haveAlpha is set, then write back colors+bits.
// ---------------------------------------------------------------------------
void Dxt1Compress::storedxtencodedblock(
    unsigned char *blkaddr,
    unsigned char srccolors[4][4][4],
    unsigned char *bestcolor[2],
    int numxpixels,
    int numypixels,
    unsigned int /*type*/,
    int haveAlpha
) {
    int i, j, colors;
    unsigned int testerror, testerror2, pixerror, pixerrorbest;
    int colordist;
    unsigned short color0, color1, tempcolor;
    unsigned int bits = 0, bits2 = 0;
    unsigned char *colorptr;
    unsigned char enc = 0;
    unsigned char cv[4][4];

    bestcolor[0][0] = bestcolor[0][0] & 0xf8;
    bestcolor[0][1] = bestcolor[0][1] & 0xfc;
    bestcolor[0][2] = bestcolor[0][2] & 0xf8;
    bestcolor[1][0] = bestcolor[1][0] & 0xf8;
    bestcolor[1][1] = bestcolor[1][1] & 0xfc;
    bestcolor[1][2] = bestcolor[1][2] & 0xf8;

    color0 = (unsigned short)(bestcolor[0][0] << 8 | bestcolor[0][1] << 3 | bestcolor[0][2] >> 3);
    color1 = (unsigned short)(bestcolor[1][0] << 8 | bestcolor[1][1] << 3 | bestcolor[1][2] >> 3);
    if (color0 < color1) {
        tempcolor = color0; color0 = color1; color1 = tempcolor;
        colorptr = bestcolor[0]; bestcolor[0] = bestcolor[1]; bestcolor[1] = colorptr;
    }

    for (i = 0; i < 3; i++) {
        cv[0][i] = bestcolor[0][i];
        cv[1][i] = bestcolor[1][i];
        cv[2][i] = (bestcolor[0][i] * 2 + bestcolor[1][i]) / 3;
        cv[3][i] = (bestcolor[0][i] + bestcolor[1][i] * 2) / 3;
    }

    testerror = 0;
    for (j = 0; j < numypixels; j++) {
        for (i = 0; i < numxpixels; i++) {
            pixerrorbest = 0xffffffff;
            for (colors = 0; colors < 4; colors++) {
                colordist = srccolors[j][i][0] - cv[colors][0];
                pixerror = colordist * colordist * REDWEIGHT;
                colordist = srccolors[j][i][1] - cv[colors][1];
                pixerror += colordist * colordist * GREENWEIGHT;
                colordist = srccolors[j][i][2] - cv[colors][2];
                pixerror += colordist * colordist * BLUEWEIGHT;
                if (pixerror < pixerrorbest) {
                    pixerrorbest = pixerror;
                    enc = (unsigned char)colors;
                }
            }
            testerror += pixerrorbest;
            bits |= (unsigned int)enc << (2 * (j * 4 + i));
        }
    }

    for (i = 0; i < 3; i++) {
        cv[2][i] = (bestcolor[0][i] + bestcolor[1][i]) / 2;
        cv[3][i] = 0;
    }
    testerror2 = 0;
    for (j = 0; j < numypixels; j++) {
        for (i = 0; i < numxpixels; i++) {
            pixerrorbest = 0xffffffff;
            if (srccolors[j][i][3] <= ALPHACUT) {
                enc = 3;
                pixerrorbest = 0;
            } else {
                for (colors = 0; colors < 3; colors++) {
                    colordist = srccolors[j][i][0] - cv[colors][0];
                    pixerror = colordist * colordist * REDWEIGHT;
                    colordist = srccolors[j][i][1] - cv[colors][1];
                    pixerror += colordist * colordist * GREENWEIGHT;
                    colordist = srccolors[j][i][2] - cv[colors][2];
                    pixerror += colordist * colordist * BLUEWEIGHT;
                    if (pixerror < pixerrorbest) {
                        pixerrorbest = pixerror;
                        if (colors > 1)
                            enc = (unsigned char)colors;
                        else
                            enc = (unsigned char)(colors ^ 1);
                    }
                }
            }
            testerror2 += pixerrorbest;
            bits2 |= (unsigned int)enc << (2 * (j * 4 + i));
        }
    }

    if ((testerror > testerror2) || haveAlpha) {
        *blkaddr++ = (unsigned char)(color1 & 0xff);
        *blkaddr++ = (unsigned char)(color1 >> 8);
        *blkaddr++ = (unsigned char)(color0 & 0xff);
        *blkaddr++ = (unsigned char)(color0 >> 8);
        *blkaddr++ = (unsigned char)(bits2 & 0xff);
        *blkaddr++ = (unsigned char)((bits2 >> 8) & 0xff);
        *blkaddr++ = (unsigned char)((bits2 >> 16) & 0xff);
        *blkaddr = (unsigned char)(bits2 >> 24);
    } else {
        *blkaddr++ = (unsigned char)(color0 & 0xff);
        *blkaddr++ = (unsigned char)(color0 >> 8);
        *blkaddr++ = (unsigned char)(color1 & 0xff);
        *blkaddr++ = (unsigned char)(color1 >> 8);
        *blkaddr++ = (unsigned char)(bits & 0xff);
        *blkaddr++ = (unsigned char)((bits >> 8) & 0xff);
        *blkaddr++ = (unsigned char)((bits >> 16) & 0xff);
        *blkaddr = (unsigned char)(bits >> 24);
    }
}

// ---------------------------------------------------------------------------
// encodedxtcolorblockfaster: pick low/high luminance endpoints from the
// source pixels, copy them so the originals aren't touched, then run the
// search and store. Pixels with alpha <= ALPHACUT are excluded.
// ---------------------------------------------------------------------------
void Dxt1Compress::encodedxtcolorblockfaster(
    unsigned char *blkaddr,
    unsigned char srccolors[4][4][4],
    int numxpixels,
    int numypixels,
    unsigned int type
) {
    unsigned char *bestcolor[2];
    unsigned char basecolors[2][3];
    unsigned char i, j;
    unsigned int lowcv, highcv, testcv;
    int haveAlpha = 0;

    lowcv = highcv = srccolors[0][0][0] * srccolors[0][0][0] * REDWEIGHT +
                     srccolors[0][0][1] * srccolors[0][0][1] * GREENWEIGHT +
                     srccolors[0][0][2] * srccolors[0][0][2] * BLUEWEIGHT;
    bestcolor[0] = bestcolor[1] = srccolors[0][0];
    for (j = 0; j < numypixels; j++) {
        for (i = 0; i < numxpixels; i++) {
            if ((type != 0) || (srccolors[j][i][3] > ALPHACUT)) {
                testcv = srccolors[j][i][0] * srccolors[j][i][0] * REDWEIGHT +
                         srccolors[j][i][1] * srccolors[j][i][1] * GREENWEIGHT +
                         srccolors[j][i][2] * srccolors[j][i][2] * BLUEWEIGHT;
                if (testcv > highcv) {
                    highcv = testcv;
                    bestcolor[1] = srccolors[j][i];
                } else if (testcv < lowcv) {
                    lowcv = testcv;
                    bestcolor[0] = srccolors[j][i];
                }
            } else {
                haveAlpha = 1;
            }
        }
    }
    for (j = 0; j < 2; j++) {
        for (i = 0; i < 3; i++) {
            basecolors[j][i] = bestcolor[j][i];
        }
    }
    bestcolor[0] = basecolors[0];
    bestcolor[1] = basecolors[1];

    fancybasecolorsearch(blkaddr, srccolors, bestcolor, numxpixels, numypixels, (int)type, haveAlpha);
    storedxtencodedblock(blkaddr, srccolors, bestcolor, numxpixels, numypixels, type, haveAlpha);
}

// ---------------------------------------------------------------------------
// writedxt5encodedalphablock: pack 16 3-bit alpha indices into 6 bytes.
// ---------------------------------------------------------------------------
void Dxt1Compress::writedxt5encodedalphablock(
    unsigned char *blkaddr,
    unsigned char alphabase1,
    unsigned char alphabase2,
    unsigned char *alphaenc
) {
    *blkaddr++ = alphabase1;
    *blkaddr++ = alphabase2;
    *blkaddr++ = (unsigned char)(alphaenc[0] | (alphaenc[1] << 3) | ((alphaenc[2] & 3) << 6));
    *blkaddr++ = (unsigned char)((alphaenc[2] >> 2) | (alphaenc[3] << 1) | (alphaenc[4] << 4) | ((alphaenc[5] & 1) << 7));
    *blkaddr++ = (unsigned char)((alphaenc[5] >> 1) | (alphaenc[6] << 2) | (alphaenc[7] << 5));
    *blkaddr++ = (unsigned char)(alphaenc[8] | (alphaenc[9] << 3) | ((alphaenc[10] & 3) << 6));
    *blkaddr++ = (unsigned char)((alphaenc[10] >> 2) | (alphaenc[11] << 1) | (alphaenc[12] << 4) | ((alphaenc[13] & 1) << 7));
    *blkaddr++ = (unsigned char)((alphaenc[13] >> 1) | (alphaenc[14] << 2) | (alphaenc[15] << 5));
}

// ---------------------------------------------------------------------------
// encodedxt5alpha: choose the best of three DXT5 alpha encodings.
// ---------------------------------------------------------------------------
void Dxt1Compress::encodedxt5alpha(
    unsigned char *blkaddr,
    unsigned char srccolors[4][4][4],
    int numxpixels,
    int numypixels
) {
    short alphatest[2];
    short alphadist;
    unsigned char alphabase[2], alphause[2];
    unsigned int alphablockerror1, alphablockerror2, alphablockerror3;
    unsigned char i, j;
    unsigned char aindex, acutValues[7];
    unsigned char alphaenc1[16], alphaenc2[16], alphaenc3[16];
    int alphaabsmin = 0;
    int alphaabsmax = 0;

    alphatest[0] = 0;
    alphatest[1] = 0;

    alphabase[0] = 0xff;
    alphabase[1] = 0x0;
    for (j = 0; j < numypixels; j++) {
        for (i = 0; i < numxpixels; i++) {
            if (srccolors[j][i][3] == 0)
                alphaabsmin = 1;
            else if (srccolors[j][i][3] == 255)
                alphaabsmax = 1;
            else {
                if (srccolors[j][i][3] > alphabase[1])
                    alphabase[1] = srccolors[j][i][3];
                if (srccolors[j][i][3] < alphabase[0])
                    alphabase[0] = srccolors[j][i][3];
            }
        }
    }

    if ((alphabase[0] > alphabase[1]) && !(alphaabsmin && alphaabsmax)) {
        *blkaddr++ = srccolors[0][0][3];
        blkaddr++;
        *blkaddr++ = 0;
        *blkaddr++ = 0;
        *blkaddr++ = 0;
        *blkaddr++ = 0;
        *blkaddr++ = 0;
        *blkaddr++ = 0;
        return;
    }

    alphablockerror1 = 0x0;
    alphablockerror2 = 0xffffffff;
    alphablockerror3 = 0xffffffff;
    if (alphaabsmin) alphause[0] = 0;
    else alphause[0] = alphabase[0];
    if (alphaabsmax) alphause[1] = 255;
    else alphause[1] = alphabase[1];
    for (aindex = 0; aindex < 7; aindex++) {
        acutValues[aindex] = (unsigned char)((alphause[0] * (2 * aindex + 1) + alphause[1] * (14 - (2 * aindex + 1))) / 14);
    }

    for (j = 0; j < numypixels; j++) {
        for (i = 0; i < numxpixels; i++) {
            if (srccolors[j][i][3] > acutValues[0]) {
                alphaenc1[4 * j + i] = 0;
                alphadist = (short)(srccolors[j][i][3] - alphause[1]);
            } else if (srccolors[j][i][3] > acutValues[1]) {
                alphaenc1[4 * j + i] = 2;
                alphadist = (short)(srccolors[j][i][3] - (alphause[1] * 6 + alphause[0] * 1) / 7);
            } else if (srccolors[j][i][3] > acutValues[2]) {
                alphaenc1[4 * j + i] = 3;
                alphadist = (short)(srccolors[j][i][3] - (alphause[1] * 5 + alphause[0] * 2) / 7);
            } else if (srccolors[j][i][3] > acutValues[3]) {
                alphaenc1[4 * j + i] = 4;
                alphadist = (short)(srccolors[j][i][3] - (alphause[1] * 4 + alphause[0] * 3) / 7);
            } else if (srccolors[j][i][3] > acutValues[4]) {
                alphaenc1[4 * j + i] = 5;
                alphadist = (short)(srccolors[j][i][3] - (alphause[1] * 3 + alphause[0] * 4) / 7);
            } else if (srccolors[j][i][3] > acutValues[5]) {
                alphaenc1[4 * j + i] = 6;
                alphadist = (short)(srccolors[j][i][3] - (alphause[1] * 2 + alphause[0] * 5) / 7);
            } else if (srccolors[j][i][3] > acutValues[6]) {
                alphaenc1[4 * j + i] = 7;
                alphadist = (short)(srccolors[j][i][3] - (alphause[1] * 1 + alphause[0] * 6) / 7);
            } else {
                alphaenc1[4 * j + i] = 1;
                alphadist = (short)(srccolors[j][i][3] - alphause[0]);
            }
            alphablockerror1 += alphadist * alphadist;
        }
    }

    if (alphablockerror1 >= 32) {
        alphablockerror2 = 0;
        for (aindex = 0; aindex < 5; aindex++) {
            acutValues[aindex] = (unsigned char)((alphabase[0] * (10 - (2 * aindex + 1)) + alphabase[1] * (2 * aindex + 1)) / 10);
        }
        for (j = 0; j < numypixels; j++) {
            for (i = 0; i < numxpixels; i++) {
                if (srccolors[j][i][3] == 0) {
                    alphaenc2[4 * j + i] = 6;
                    alphadist = 0;
                } else if (srccolors[j][i][3] == 255) {
                    alphaenc2[4 * j + i] = 7;
                    alphadist = 0;
                } else if (srccolors[j][i][3] <= acutValues[0]) {
                    alphaenc2[4 * j + i] = 0;
                    alphadist = (short)(srccolors[j][i][3] - alphabase[0]);
                } else if (srccolors[j][i][3] <= acutValues[1]) {
                    alphaenc2[4 * j + i] = 2;
                    alphadist = (short)(srccolors[j][i][3] - (alphabase[0] * 4 + alphabase[1] * 1) / 5);
                } else if (srccolors[j][i][3] <= acutValues[2]) {
                    alphaenc2[4 * j + i] = 3;
                    alphadist = (short)(srccolors[j][i][3] - (alphabase[0] * 3 + alphabase[1] * 2) / 5);
                } else if (srccolors[j][i][3] <= acutValues[3]) {
                    alphaenc2[4 * j + i] = 4;
                    alphadist = (short)(srccolors[j][i][3] - (alphabase[0] * 2 + alphabase[1] * 3) / 5);
                } else if (srccolors[j][i][3] <= acutValues[4]) {
                    alphaenc2[4 * j + i] = 5;
                    alphadist = (short)(srccolors[j][i][3] - (alphabase[0] * 1 + alphabase[1] * 4) / 5);
                } else {
                    alphaenc2[4 * j + i] = 1;
                    alphadist = (short)(srccolors[j][i][3] - alphabase[1]);
                }
                alphablockerror2 += alphadist * alphadist;
            }
        }

        if ((alphablockerror2 > 96) && (alphablockerror1 > 96)) {
            short blockerrlin1 = 0;
            short blockerrlin2 = 0;
            unsigned char nralphainrangelow = 0;
            unsigned char nralphainrangehigh = 0;
            alphatest[0] = 0xff;
            alphatest[1] = 0x0;
            for (j = 0; j < numypixels; j++) {
                for (i = 0; i < numxpixels; i++) {
                    if ((srccolors[j][i][3] > alphatest[1]) && (srccolors[j][i][3] < (255 - (alphabase[1] - alphabase[0]) / 28)))
                        alphatest[1] = srccolors[j][i][3];
                    if ((srccolors[j][i][3] < alphatest[0]) && (srccolors[j][i][3] > (alphabase[1] - alphabase[0]) / 28))
                        alphatest[0] = srccolors[j][i][3];
                }
            }
            if (alphatest[1] <= alphatest[0]) {
                alphatest[0] = 1;
                alphatest[1] = 254;
            }
            for (aindex = 0; aindex < 5; aindex++) {
                acutValues[aindex] = (unsigned char)((alphatest[0] * (10 - (2 * aindex + 1)) + alphatest[1] * (2 * aindex + 1)) / 10);
            }

            for (j = 0; j < numypixels; j++) {
                for (i = 0; i < numxpixels; i++) {
                    if (srccolors[j][i][3] <= alphatest[0] / 2) {
                    } else if (srccolors[j][i][3] > ((255 + alphatest[1]) / 2)) {
                    } else if (srccolors[j][i][3] <= acutValues[0]) {
                        blockerrlin1 += (short)(srccolors[j][i][3] - alphatest[0]);
                        nralphainrangelow += 1;
                    } else if (srccolors[j][i][3] <= acutValues[1]) {
                        blockerrlin1 += (short)(srccolors[j][i][3] - (alphatest[0] * 4 + alphatest[1] * 1) / 5);
                        blockerrlin2 += (short)(srccolors[j][i][3] - (alphatest[0] * 4 + alphatest[1] * 1) / 5);
                        nralphainrangelow += 1;
                        nralphainrangehigh += 1;
                    } else if (srccolors[j][i][3] <= acutValues[2]) {
                        blockerrlin1 += (short)(srccolors[j][i][3] - (alphatest[0] * 3 + alphatest[1] * 2) / 5);
                        blockerrlin2 += (short)(srccolors[j][i][3] - (alphatest[0] * 3 + alphatest[1] * 2) / 5);
                        nralphainrangelow += 1;
                        nralphainrangehigh += 1;
                    } else if (srccolors[j][i][3] <= acutValues[3]) {
                        blockerrlin1 += (short)(srccolors[j][i][3] - (alphatest[0] * 2 + alphatest[1] * 3) / 5);
                        blockerrlin2 += (short)(srccolors[j][i][3] - (alphatest[0] * 2 + alphatest[1] * 3) / 5);
                        nralphainrangelow += 1;
                        nralphainrangehigh += 1;
                    } else if (srccolors[j][i][3] <= acutValues[4]) {
                        blockerrlin1 += (short)(srccolors[j][i][3] - (alphatest[0] * 1 + alphatest[1] * 4) / 5);
                        blockerrlin2 += (short)(srccolors[j][i][3] - (alphatest[0] * 1 + alphatest[1] * 4) / 5);
                        nralphainrangelow += 1;
                        nralphainrangehigh += 1;
                    } else {
                        blockerrlin2 += (short)(srccolors[j][i][3] - alphatest[1]);
                        nralphainrangehigh += 1;
                    }
                }
            }
            if (nralphainrangelow == 0) nralphainrangelow = 1;
            if (nralphainrangehigh == 0) nralphainrangehigh = 1;
            alphatest[0] = (short)(alphatest[0] + (blockerrlin1 / nralphainrangelow));
            if (alphatest[0] < 0) {
                alphatest[0] = 0;
            }
            alphatest[1] = (short)(alphatest[1] + (blockerrlin2 / nralphainrangehigh));
            if (alphatest[1] > 255) {
                alphatest[1] = 255;
            }

            alphablockerror3 = 0;
            for (aindex = 0; aindex < 5; aindex++) {
                acutValues[aindex] = (unsigned char)((alphatest[0] * (10 - (2 * aindex + 1)) + alphatest[1] * (2 * aindex + 1)) / 10);
            }
            for (j = 0; j < numypixels; j++) {
                for (i = 0; i < numxpixels; i++) {
                    if (srccolors[j][i][3] <= alphatest[0] / 2) {
                        alphaenc3[4 * j + i] = 6;
                        alphadist = srccolors[j][i][3];
                    } else if (srccolors[j][i][3] > ((255 + alphatest[1]) / 2)) {
                        alphaenc3[4 * j + i] = 7;
                        alphadist = (short)(255 - srccolors[j][i][3]);
                    } else if (srccolors[j][i][3] <= acutValues[0]) {
                        alphaenc3[4 * j + i] = 0;
                        alphadist = (short)(srccolors[j][i][3] - alphatest[0]);
                    } else if (srccolors[j][i][3] <= acutValues[1]) {
                        alphaenc3[4 * j + i] = 2;
                        alphadist = (short)(srccolors[j][i][3] - (alphatest[0] * 4 + alphatest[1] * 1) / 5);
                    } else if (srccolors[j][i][3] <= acutValues[2]) {
                        alphaenc3[4 * j + i] = 3;
                        alphadist = (short)(srccolors[j][i][3] - (alphatest[0] * 3 + alphatest[1] * 2) / 5);
                    } else if (srccolors[j][i][3] <= acutValues[3]) {
                        alphaenc3[4 * j + i] = 4;
                        alphadist = (short)(srccolors[j][i][3] - (alphatest[0] * 2 + alphatest[1] * 3) / 5);
                    } else if (srccolors[j][i][3] <= acutValues[4]) {
                        alphaenc3[4 * j + i] = 5;
                        alphadist = (short)(srccolors[j][i][3] - (alphatest[0] * 1 + alphatest[1] * 4) / 5);
                    } else {
                        alphaenc3[4 * j + i] = 1;
                        alphadist = (short)(srccolors[j][i][3] - alphatest[1]);
                    }
                    alphablockerror3 += alphadist * alphadist;
                }
            }
        }
    }

    if ((alphablockerror1 <= alphablockerror2) && (alphablockerror1 <= alphablockerror3)) {
        writedxt5encodedalphablock(blkaddr, alphause[1], alphause[0], alphaenc1);
    } else if (alphablockerror2 <= alphablockerror3) {
        writedxt5encodedalphablock(blkaddr, alphabase[0], alphabase[1], alphaenc2);
    } else {
        writedxt5encodedalphablock(blkaddr, (unsigned char)alphatest[0], (unsigned char)alphatest[1], alphaenc3);
    }
}

// ---------------------------------------------------------------------------
// tx_compress_dxtn: dispatch on destFormat (0=DXT1, 1=DXT3, 2=DXT5) and
// loop over the source in 4x4 blocks.
// ---------------------------------------------------------------------------
void Dxt1Compress::tx_compress_dxtn(
    int width,
    int height,
    const unsigned char *srcPixData,
    int destFormat,
    unsigned char *dest,
    int srcBpp,
    int dstBpp,
    int dstRowStride
) {
    unsigned char *blkaddr = dest;
    unsigned char srcpixels[4][4][4];
    int j, dstRowDiff, numypixels, i, numxpixels;

    switch (destFormat) {
    case 0:
        dstRowDiff = dstRowStride - ((width / 4) * 8);
        for (j = 0; j < height; j += 4) {
            if (height > j + 3) numypixels = 4;
            else numypixels = height - j;
            for (i = 0; i < width; i += 4) {
                if (width > i + 3) numxpixels = 4;
                else numxpixels = width - i;
                extractsrccolors(width, height, i, j, srcpixels, srcPixData, srcBpp, dstBpp, numxpixels, numypixels);
                encodedxtcolorblockfaster(blkaddr, srcpixels, numxpixels, numypixels, (unsigned int)destFormat);
                blkaddr += 8;
            }
            if (dstRowStride > 0) {
                blkaddr += dstRowDiff;
            }
        }
        break;
    case 1:
        dstRowDiff = dstRowStride - ((width / 4) * 16);
        for (j = 0; j < height; j += 4) {
            if (height > j + 3) numypixels = 4;
            else numypixels = height - j;
            for (i = 0; i < width; i += 4) {
                if (width > i + 3) numxpixels = 4;
                else numxpixels = width - i;
                extractsrccolors(width, height, i, j, srcpixels, srcPixData, srcBpp, dstBpp, numxpixels, numypixels);
                *blkaddr++ = (unsigned char)((srcpixels[0][0][3] >> 4) | (srcpixels[0][1][3] & 0xf0));
                *blkaddr++ = (unsigned char)((srcpixels[0][2][3] >> 4) | (srcpixels[0][3][3] & 0xf0));
                *blkaddr++ = (unsigned char)((srcpixels[1][0][3] >> 4) | (srcpixels[1][1][3] & 0xf0));
                *blkaddr++ = (unsigned char)((srcpixels[1][2][3] >> 4) | (srcpixels[1][3][3] & 0xf0));
                *blkaddr++ = (unsigned char)((srcpixels[2][0][3] >> 4) | (srcpixels[2][1][3] & 0xf0));
                *blkaddr++ = (unsigned char)((srcpixels[2][2][3] >> 4) | (srcpixels[2][3][3] & 0xf0));
                *blkaddr++ = (unsigned char)((srcpixels[3][0][3] >> 4) | (srcpixels[3][1][3] & 0xf0));
                *blkaddr++ = (unsigned char)((srcpixels[3][2][3] >> 4) | (srcpixels[3][3][3] & 0xf0));
                encodedxtcolorblockfaster(blkaddr, srcpixels, numxpixels, numypixels, (unsigned int)destFormat);
                blkaddr += 8;
            }
            if (dstRowStride > 0) {
                blkaddr += dstRowDiff;
            }
        }
        break;
    case 2:
        dstRowDiff = dstRowStride - ((width / 4) * 16);
        for (j = 0; j < height; j += 4) {
            if (height > j + 3) numypixels = 4;
            else numypixels = height - j;
            for (i = 0; i < width; i += 4) {
                if (width > i + 3) numxpixels = 4;
                else numxpixels = width - i;
                extractsrccolors(width, height, i, j, srcpixels, srcPixData, srcBpp, dstBpp, numxpixels, numypixels);
                encodedxt5alpha(blkaddr, srcpixels, numxpixels, numypixels);
                encodedxtcolorblockfaster(blkaddr + 8, srcpixels, numxpixels, numypixels, (unsigned int)destFormat);
                blkaddr += 16;
            }
            if (dstRowStride > 0) {
                blkaddr += dstRowDiff;
            }
        }
        break;
    }
}

// ---------------------------------------------------------------------------
// CompressImage: convenience wrapper that fixes destFormat to 0 (DXT1).
// ---------------------------------------------------------------------------
void Dxt1Compress::CompressImage(
    const unsigned char *src,
    int width,
    int height,
    void *dst,
    int srcBpp,
    int dstBpp,
    int dstRowStride
) {
    tx_compress_dxtn(width, height, src, 0, (unsigned char *)dst, srcBpp, dstBpp, dstRowStride);
}
