#include <revolution/GX.h>
#include <revolution/gx/GXTypes.h>

extern unsigned long __cvt_fp2unsigned(double d);
extern void __GetImageTileCount(GXTexFmt fmt, u16 w, u16 h, int *numBlocksX, int *numBlocksY, int *pitch);

void GXSetDispCopySrc(u16 x, u16 y, u16 w, u16 h) {
    u32 xy = 0;
    xy = GX_BITSET(xy, 22, 10, (u32)x);
    u32 wh = 0;
    wh = GX_BITSET(wh, 22, 10, (u32)(w - 1));
    xy = GX_BITSET(xy, 12, 10, (u32)y);
    xy = GX_BITSET(xy, 0, 8, GX_BP_REG_TEXCOPYSRCXY);
    wh = GX_BITSET(wh, 12, 10, (u32)(h - 1));
    gxdt->dispCopySrcXY = xy;
    wh = GX_BITSET(wh, 0, 8, GX_BP_REG_TEXCOPYSRCWH);
    gxdt->dispCopySrcWH = wh;
}

void GXSetTexCopySrc(u16 x, u16 y, u16 w, u16 h) {
    u32 xy = 0;
    xy = GX_BITSET(xy, 22, 10, (u32)x);
    u32 wh = 0;
    wh = GX_BITSET(wh, 22, 10, (u32)(w - 1));
    xy = GX_BITSET(xy, 12, 10, (u32)y);
    xy = GX_BITSET(xy, 0, 8, GX_BP_REG_TEXCOPYSRCXY);
    wh = GX_BITSET(wh, 12, 10, (u32)(h - 1));
    gxdt->texCopySrcXY = xy;
    wh = GX_BITSET(wh, 0, 8, GX_BP_REG_TEXCOPYSRCWH);
    gxdt->texCopySrcWH = wh;
}

void GXSetDispCopyDst(u16 width) {
    s32 stride = (s32)((width & 0x7fff) << 1) >> 5;
    u32 cmd = 0;
    cmd = GX_BITSET_TRUNC(cmd, 22, 10, stride);
    u32 opcode = GX_BP_REG_DISPCOPYSTRIDE;
    cmd = GX_BITSET(cmd, 0, 8, opcode);
    gxdt->dispCopyDst = cmd;
}

void GXSetTexCopyDst(u16 width, u16 height, GXTexFmt fmt, GXBool mipmap) {
    u32 hwFmt = fmt & 0xf;
    if (fmt == GX_TF_Z24X8) {
        hwFmt = 0xb;
    }

    gxdt->texCopyMipmap = 0;

    if (fmt <= GX_TF_RGBA8 || fmt == GX_TF_Z24X8) {
        gxdt->texCopyCtrl = (gxdt->texCopyCtrl & 0xfffe7fff) | 0x18000;
    } else {
        gxdt->texCopyCtrl = (gxdt->texCopyCtrl & 0xfffe7fff) | 0x10000;
    }

    gxdt->texCopyMipmap = (fmt >> 4) & 1;
    gxdt->texCopyCtrl = (gxdt->texCopyCtrl & 0xfffffff7) | (hwFmt & 8);

    int numBlocksX, numBlocksY, rowPitch;
    __GetImageTileCount(fmt, width, height, &numBlocksX, &numBlocksY, &rowPitch);

    gxdt->texCopyDst = 0;
    GX_BP_SET_OPCODE(gxdt->texCopyDst, GX_BP_REG_TEXCOPYDST);
    gxdt->texCopyDst |= ((u32)(numBlocksX * rowPitch)) & 0x3ff;

    gxdt->texCopyCtrl = (gxdt->texCopyCtrl & 0xfffffd8f) | ((hwFmt & 7) << 4) | ((mipmap & 1) << 9);
}

void GXSetDispCopyFrame2Field(u32 mode) {
    u32 disp = gxdt->dispCopyCtrl;
    disp = GX_BITSET(disp, 18, 2, mode);
    gxdt->dispCopyCtrl = disp;
    u32 tex = gxdt->texCopyCtrl;
    tex = tex & ~(3 << 12);
    gxdt->texCopyCtrl = tex;
}

void GXSetCopyClamp(GXCopyClamp clamp) {
    u32 disp = gxdt->dispCopyCtrl;
    disp = GX_BITSET_TRUNC(disp, 31, 1, (u32)clamp);
    disp = GX_BITSET_TRUNC(disp, 30, 1, (u32)clamp);
    gxdt->dispCopyCtrl = disp;
    u32 tex = gxdt->texCopyCtrl;
    tex = GX_BITSET_TRUNC(tex, 31, 1, (u32)clamp);
    tex = GX_BITSET_TRUNC(tex, 30, 1, (u32)clamp);
    gxdt->texCopyCtrl = tex;
}

float GXGetYScaleFactor(u32 efbHeight, u32 xfbHeight) {
    float efb = (float)efbHeight;
    float xfb = (float)xfbHeight;

    float ratio = xfb / efb;
    u32 yScale = __cvt_fp2unsigned((double)(1.0f / ratio));
    yScale = yScale & 0x1ff;

    u32 srcH = efbHeight - 1;
    u32 num = (srcH << 8) / yScale;
    u32 yscaleOut = num + 1;

    if (yScale > 0x80 && yScale < 0x100) {
        while ((yScale & 1) == 0) {
            yScale >>= 1;
        }
        if (efbHeight == (efbHeight / yScale) * yScale) {
            yscaleOut = num + 2;
        }
    }

    if (yscaleOut > 0x400) {
        yscaleOut = 0x400;
    }

    while (xfbHeight < yscaleOut) {
        xfbHeight--;
        xfb = (float)xfbHeight;
        ratio = xfb / efb;
        yScale = __cvt_fp2unsigned((double)(1.0f / ratio));
        yScale = yScale & 0x1ff;
        num = (srcH << 8) / yScale;
        yscaleOut = num + 1;
        if (yScale > 0x80 && yScale < 0x100) {
            while ((yScale & 1) == 0) {
                yScale >>= 1;
            }
            if (efbHeight == (efbHeight / yScale) * yScale) {
                yscaleOut = num + 2;
            }
        }
        if (yscaleOut > 0x400) {
            yscaleOut = 0x400;
        }
    }

    while (yscaleOut < xfbHeight) {
        xfbHeight++;
        xfb = (float)xfbHeight;
        ratio = xfb / efb;
        yScale = __cvt_fp2unsigned((double)(1.0f / ratio));
        yScale = yScale & 0x1ff;
        num = (srcH << 8) / yScale;
        yscaleOut = num + 1;
        if (yScale > 0x80 && yScale < 0x100) {
            while ((yScale & 1) == 0) {
                yScale >>= 1;
            }
            if (efbHeight == (efbHeight / yScale) * yScale) {
                yscaleOut = num + 2;
            }
        }
        if (yscaleOut > 0x400) {
            yscaleOut = 0x400;
        }
    }

    return ratio;
}

u32 GXSetDispCopyYScale(float yscale) {
    u32 yScaleHW = __cvt_fp2unsigned((double)(1.0f / yscale));

    u32 cmd = 0;
    u32 opcode = GX_BP_REG_DISPCOPYSCALEY;
    u32 yScaleReg = yScaleHW & 0x1ff;
    cmd = GX_BITSET(cmd, 23, 9, yScaleHW);
    cmd = GX_BITSET(cmd, 0, 8, opcode);
    GX_BP_LOAD_REG(cmd);

    gxdt->lastWriteWasXF = FALSE;

    // update fracLnScaleY (integer indicator) in dispCopyCtrl
    u32 intFlag = ((0x100 - yScaleReg) | (yScaleReg - 0x100)) >> 21 & 0x400;
    u32 dispCtrl = gxdt->dispCopyCtrl;
    dispCtrl = GX_BITSET(dispCtrl, 21, 1, intFlag >> 10);
    gxdt->dispCopyCtrl = dispCtrl;

    // compute output XFB height: get (h-1) from dispCopySrcWH bits 12-21
    u32 dispSrcWH = gxdt->dispCopySrcWH;
    u32 srcH = (dispSrcWH >> 10) & 0x3ff;
    u32 efbHeight = srcH + 1;
    u32 num = (srcH << 8) / yScaleReg;
    u32 yscaleOut = num + 1;

    if (yScaleReg > 0x80 && yScaleReg < 0x100) {
        while ((yScaleReg & 1) == 0) {
            yScaleReg >>= 1;
        }
        if (efbHeight == (efbHeight / yScaleReg) * yScaleReg) {
            yscaleOut = num + 2;
        }
    }

    if (yscaleOut > 0x400) {
        yscaleOut = 0x400;
    }

    return yscaleOut;
}

void GXSetCopyClear(GXColor color, u32 z) {
    u32 clearAR = 0;
    clearAR = GX_BITSET_TRUNC(clearAR, 24, 8, (u32)color.r);
    clearAR = GX_BITSET(clearAR, 16, 8, (u32)color.a);
    clearAR = GX_BITSET(clearAR, 0, 8, GX_BP_REG_COPYCLEARAR);
    GX_BP_LOAD_REG(clearAR);

    u32 clearZ = 0;
    clearZ = GX_BITSET_TRUNC(clearZ, 8, 24, z);

    u32 clearGB = 0;
    clearGB = GX_BITSET_TRUNC(clearGB, 24, 8, (u32)color.b);
    clearZ = GX_BITSET(clearZ, 0, 8, GX_BP_REG_COPYCLEARZ);
    clearGB = GX_BITSET(clearGB, 16, 8, (u32)color.g);
    clearGB = GX_BITSET(clearGB, 0, 8, GX_BP_REG_COPYCLEARGB);
    GX_BP_LOAD_REG(clearGB);
    GX_BP_LOAD_REG(clearZ);

    gxdt->lastWriteWasXF = FALSE;
}

void GXSetCopyFilter(
    GXBool aa, u8 sample_pattern[12][2], GXBool vf, u8 vfilter[7]
) {
    u32 filter0, filter1, filter2, filter3;

    if (aa) {
        u8 *sp = (u8 *)sample_pattern;

        filter0 = 0;
        filter0 |= (sp[0] & 0xf);
        filter0 |= (sp[1] & 0xf) << 4;
        filter0 |= (sp[2] & 0xf) << 8;
        filter0 |= (sp[3] & 0xf) << 12;
        filter0 |= (sp[4] & 0xf) << 16;
        filter0 |= (sp[5] & 0xf) << 20;
        GX_BP_SET_OPCODE(filter0, GX_BP_REG_DISPCOPYFILTER0);

        filter1 = 0;
        filter1 |= (sp[6] & 0xf);
        filter1 |= (sp[7] & 0xf) << 4;
        filter1 |= (sp[8] & 0xf) << 8;
        filter1 |= (sp[9] & 0xf) << 12;
        filter1 |= (sp[10] & 0xf) << 16;
        filter1 |= (sp[11] & 0xf) << 20;
        GX_BP_SET_OPCODE(filter1, GX_BP_REG_DISPCOPYFILTER1);

        filter2 = 0;
        filter2 |= (sp[12] & 0xf);
        filter2 |= (sp[13] & 0xf) << 4;
        filter2 |= (sp[14] & 0xf) << 8;
        filter2 |= (sp[15] & 0xf) << 12;
        filter2 |= (sp[16] & 0xf) << 16;
        filter2 |= (sp[17] & 0xf) << 20;
        GX_BP_SET_OPCODE(filter2, GX_BP_REG_DISPCOPYFILTER2);

        filter3 = 0;
        filter3 |= (sp[18] & 0xf);
        filter3 |= (sp[19] & 0xf) << 4;
        filter3 |= (sp[20] & 0xf) << 8;
        filter3 |= (sp[21] & 0xf) << 12;
        filter3 |= (sp[22] & 0xf) << 16;
        filter3 |= (sp[23] & 0xf) << 20;
        GX_BP_SET_OPCODE(filter3, GX_BP_REG_DISPCOPYFILTER3);
    } else {
        filter0 = 0x01666666;
        filter1 = 0x02666666;
        filter2 = 0x03666666;
        filter3 = 0x04666666;
    }

    GX_BP_LOAD_REG(filter0);
    GX_BP_LOAD_REG(filter1);
    GX_BP_LOAD_REG(filter2);
    GX_BP_LOAD_REG(filter3);

    u32 copyFilter0 = 0;
    u32 copyFilter1 = 0;
    GX_BP_SET_OPCODE(copyFilter0, GX_BP_REG_COPYFILTER0);
    GX_BP_SET_OPCODE(copyFilter1, GX_BP_REG_COPYFILTER1);

    if (vf) {
        copyFilter0 |= (vfilter[0] & 0x3f);
        copyFilter0 |= (vfilter[1] & 0x3f) << 6;
        copyFilter0 |= (vfilter[2] & 0x3f) << 12;
        copyFilter0 |= (vfilter[3] & 0x3f) << 18;
        copyFilter1 |= (vfilter[4] & 0x3f);
        copyFilter1 |= (vfilter[5] & 0x3f) << 6;
        copyFilter1 |= (vfilter[6] & 0x3f) << 12;
    } else {
        copyFilter0 |= 0x15;
        copyFilter0 |= 0x15 << 12;
        copyFilter0 |= 0x16 << 18;
        copyFilter1 |= 0x16;
    }

    GX_BP_LOAD_REG(copyFilter0);
    GX_BP_LOAD_REG(copyFilter1);
    gxdt->lastWriteWasXF = FALSE;
}

void GXSetDispCopyGamma(u32 gamma) {
    u32 ctrl = gxdt->dispCopyCtrl;
    ctrl = GX_BITSET(ctrl, 23, 2, gamma);
    gxdt->dispCopyCtrl = ctrl;
}

void GXCopyDisp(void *dest, GXBool clear) {
    if (clear) {
        GX_BP_LOAD_REG(gxdt->zMode | 0xf);
        GX_BP_LOAD_REG(gxdt->blendMode & ~3);
    }

    int doRestoreZControl = 0;
    u32 *zCtrlPtr;
    if (!clear) {
        u32 zc = gxdt->zControl;
        zCtrlPtr = &gxdt->zControl;
        if ((zc & 7) != 3) {
            goto skip_zctrl;
        }
    }
    {
        u32 zc = gxdt->zControl;
        zCtrlPtr = &gxdt->zControl;
        if ((zc >> 6 & 1) == 1) {
            GX_BP_LOAD_REG(zc & ~(1 << 6));
            doRestoreZControl = 1;
        }
    }
skip_zctrl:;

    u32 copyDstCmd = 0;
    copyDstCmd = GX_BITSET_TRUNC(copyDstCmd, 8, 24, ((u32)dest >> 5));
    copyDstCmd = GX_BITSET(copyDstCmd, 0, 8, GX_BP_REG_TEXCOPYDST);

    GX_BP_LOAD_REG(gxdt->dispCopySrcXY);
    GX_BP_LOAD_REG(gxdt->dispCopySrcWH);
    GX_BP_LOAD_REG(gxdt->dispCopyDst);
    GX_BP_LOAD_REG(copyDstCmd);

    u32 ctrl = gxdt->dispCopyCtrl;
    ctrl = GX_BITSET(ctrl, 20, 1, (u32)clear);
    ctrl |= 0x4000;
    ctrl = GX_BITSET(ctrl, 0, 8, GX_BP_REG_COPYEXECUTE);
    gxdt->dispCopyCtrl = ctrl;
    GX_BP_LOAD_REG(gxdt->dispCopyCtrl);

    if (clear) {
        GX_BP_LOAD_REG(gxdt->zMode);
        GX_BP_LOAD_REG(gxdt->blendMode);
    }

    if (doRestoreZControl) {
        GX_BP_LOAD_REG(*zCtrlPtr);
    }

    gxdt->lastWriteWasXF = FALSE;
}

void GXCopyTex(void *dest, GXBool clear) {
    if (clear) {
        GX_BP_LOAD_REG(gxdt->zMode | 0xf);
        GX_BP_LOAD_REG(gxdt->blendMode & ~3);
    }

    int doRestoreZControl = 0;
    u32 zc = gxdt->zControl;

    if (gxdt->texCopyMipmap) {
        if ((zc & 7) != 3) {
            doRestoreZControl = 1;
            zc = GX_BITSET_TRUNC(zc, 29, 3, 3);
        }
    }

    if (clear || (zc & 7) == 3) {
        if ((zc >> 6 & 1) == 1) {
            doRestoreZControl = 1;
            zc = zc & ~(1 << 6);
        }
    }

    if (doRestoreZControl) {
        GX_BP_LOAD_REG(zc);
    }

    u32 copyDstCmd = 0;
    copyDstCmd = GX_BITSET_TRUNC(copyDstCmd, 8, 24, ((u32)dest >> 5));
    copyDstCmd = GX_BITSET(copyDstCmd, 0, 8, GX_BP_REG_TEXCOPYDST);

    GX_BP_LOAD_REG(gxdt->texCopySrcXY);
    GX_BP_LOAD_REG(gxdt->texCopySrcWH);
    GX_BP_LOAD_REG(gxdt->texCopyDst);
    GX_BP_LOAD_REG(copyDstCmd);

    u32 ctrl = gxdt->texCopyCtrl;
    ctrl = GX_BITSET(ctrl, 20, 1, (u32)clear);
    ctrl = ctrl & ~(3 << 13);
    ctrl = GX_BITSET(ctrl, 0, 8, GX_BP_REG_COPYEXECUTE);
    gxdt->texCopyCtrl = ctrl;
    GX_BP_LOAD_REG(gxdt->texCopyCtrl);

    if (clear) {
        GX_BP_LOAD_REG(gxdt->zMode);
        GX_BP_LOAD_REG(gxdt->blendMode);
    }

    if (doRestoreZControl) {
        GX_BP_LOAD_REG(gxdt->zControl);
    }

    gxdt->lastWriteWasXF = FALSE;
}

void GXClearBoundingBox(void) {
    GX_BP_LOAD_REG(0x55000000 | 0x3ff);
    GX_BP_LOAD_REG(0x56000000 | 0x3ff);
    gxdt->lastWriteWasXF = FALSE;
}
