#include <revolution/GX.h>

void GXSetFog(
    GXFogType type,
    GXColor color,
    float start,
    float end,
    float near,
    float far
) {
    u32 proj = type >> 3;
    u32 fsel = type & 7;
    float stfs_a, stfs_b;
    u32 cmd;

    if (fsel == 0) {
        stfs_a = 0.0f;
        stfs_b = 0.0f;
    } else {
        if (!proj) {
            if (end == start || far == near) {
                stfs_a = 0.0f;
                stfs_b = 0.0f;
            } else {
                float f_minus_s = end - start;
                stfs_a = (end - start) / f_minus_s;
                stfs_b = (start - near) / f_minus_s;
            }
        } else {
            float A, B;
            float half = 0.5f;
            int n;

            if (end == start || far == near) {
                A = 0.0f;
                B = 0.5f;
            } else {
                float a = end - start;
                float b = far - near;
                float c = far * near;
                float d = far / a;
                b = d / b;
                A = near / b;
                B = far / b;
            }

            n = 0;
            while (A > 1.0) {
                A *= half;
                n++;
            }
            while (A > 0.0f && A < half) {
                A *= 2.0f;
                n--;
            }

            stfs_a = A;
            stfs_b = B;
        }
    }

    cmd = 0;
    GX_BP_SET_FOGPARAM0_A_MANT(cmd, (u32)(stfs_a * 8388638.0f));
    GX_BP_SET_OPCODE(cmd, GX_BP_REG_FOGPARAM0);
    GX_BP_LOAD_REG(cmd);

    cmd = 0;
    GX_BP_SET_FOGPARAM1_B_MAG(cmd, (u32)(stfs_b * 8388638.0f));
    GX_BP_SET_OPCODE(cmd, GX_BP_REG_FOGPARAM1);
    GX_BP_LOAD_REG(cmd);

    cmd = 0;
    GX_BP_SET_FOGPARAM3_PROJ(cmd, proj);
    GX_BP_SET_FOGPARAM3_FSEL(cmd, fsel);
    GX_BP_SET_OPCODE(cmd, GX_BP_REG_FOGPARAM3);
    GX_BP_LOAD_REG(cmd);

    cmd = 0;
    GX_BP_SET_FOGCOLOR_RGB(cmd, (color.r << 16) | (color.g << 8) | color.b);
    GX_BP_SET_OPCODE(cmd, GX_BP_REG_FOGCOLOR);
    GX_BP_LOAD_REG(cmd);

    gxdt->lastWriteWasXF = FALSE;
}

void GXSetFogRangeAdj(GXBool enable, u16 center, const GXFogAdjTable* table) {
    if (enable) {
        u32 cmd;

        cmd = 0;
        GX_BP_SET_FOGRANGEK_HI(cmd, table->r[0]);
        GX_BP_SET_FOGRANGEK_LO(cmd, table->r[1]);
        GX_BP_SET_OPCODE(cmd, GX_BP_REG_FOGRANGEK0);
        GX_BP_LOAD_REG(cmd);

        cmd = 0;
        GX_BP_SET_FOGRANGEK_HI(cmd, table->r[2]);
        GX_BP_SET_FOGRANGEK_LO(cmd, table->r[3]);
        GX_BP_SET_OPCODE(cmd, GX_BP_REG_FOGRANGEK1);
        GX_BP_LOAD_REG(cmd);

        cmd = 0;
        GX_BP_SET_FOGRANGEK_HI(cmd, table->r[4]);
        GX_BP_SET_FOGRANGEK_LO(cmd, table->r[5]);
        GX_BP_SET_OPCODE(cmd, GX_BP_REG_FOGRANGEK2);
        GX_BP_LOAD_REG(cmd);

        cmd = 0;
        GX_BP_SET_FOGRANGEK_HI(cmd, table->r[6]);
        GX_BP_SET_FOGRANGEK_LO(cmd, table->r[7]);
        GX_BP_SET_OPCODE(cmd, GX_BP_REG_FOGRANGEK3);
        GX_BP_LOAD_REG(cmd);

        cmd = 0;
        GX_BP_SET_FOGRANGEK_HI(cmd, table->r[8]);
        GX_BP_SET_FOGRANGEK_LO(cmd, table->r[9]);
        GX_BP_SET_OPCODE(cmd, GX_BP_REG_FOGRANGEK4);
        GX_BP_LOAD_REG(cmd);
    }

    {
        u32 cmd = 0;
        GX_BP_SET_FOGRANGE_CENTER(cmd, center + 0x156);
        GX_BP_SET_FOGRANGE_ENABLED(cmd, enable);
        GX_BP_SET_OPCODE(cmd, GX_BP_REG_FOGRANGE);
        GX_BP_LOAD_REG(cmd);
    }

    gxdt->lastWriteWasXF = FALSE;
}

void GXSetBlendMode(GXBlendMode mode, GXBlendFactor src, GXBlendFactor dst, GXLogicOp op) {
    u32 subtract = mode - GX_BM_SUBTRACT;
    u32 logic_en = mode - GX_BM_LOGIC;
    u32 blendMode = gxdt->blendMode;
    subtract = __cntlzw(subtract);
    logic_en = __cntlzw(logic_en);

    blendMode = __rlwimi(blendMode, subtract, 6, 20, 20);
    GX_BP_SET_BLENDMODE_BLEND_ENABLE(blendMode, (u32)mode);
    blendMode = __rlwimi(blendMode, logic_en, 28, 30, 30);
    GX_BP_SET_BLENDMODE_LOGIC_MODE(blendMode, op);
    GX_BP_SET_BLENDMODE_SRC_FACTOR(blendMode, src);
    GX_BP_SET_BLENDMODE_DST_FACTOR(blendMode, dst);
    GX_BP_LOAD_REG(blendMode);
    gxdt->blendMode = blendMode;
    gxdt->lastWriteWasXF = FALSE;
}

void GXSetColorUpdate(GXBool enable) {
    u32 blendMode = gxdt->blendMode;
    GX_BP_SET_BLENDMODE_COLOR_UPDATE(blendMode, enable);
    GX_BP_LOAD_REG(blendMode);
    gxdt->blendMode = blendMode;
    gxdt->lastWriteWasXF = FALSE;
}

void GXSetAlphaUpdate(GXBool enable) {
    u32 blendMode = gxdt->blendMode;
    GX_BP_SET_BLENDMODE_ALPHA_UPDATE(blendMode, enable);
    GX_BP_LOAD_REG(blendMode);
    gxdt->blendMode = blendMode;
    gxdt->lastWriteWasXF = FALSE;
}

void GXSetZMode(GXBool enableTest, GXCompare func, GXBool enableUpdate) {
    u32 zMode = gxdt->zMode;
    GX_BP_SET_ZMODE_TEST_ENABLE(zMode, enableTest);
    GX_BP_SET_ZMODE_COMPARE(zMode, func);
    GX_BP_SET_ZMODE_UPDATE_ENABLE(zMode, enableUpdate);
    GX_BP_LOAD_REG(zMode);
    gxdt->zMode = zMode;
    gxdt->lastWriteWasXF = FALSE;
}

void GXSetZCompLoc(GXBool beforeTex) {
    u32 zControl = gxdt->zControl;
    GX_BP_SET_ZCONTROL_BEFORE_TEX(zControl, beforeTex);
    gxdt->zControl = zControl;
    GX_BP_LOAD_REG(gxdt->zControl);
    gxdt->lastWriteWasXF = FALSE;
}

void GXSetPixelFmt(GXPixelFmt pixelFmt, GXZFmt16 zFmt) {
    static u32 p2f[] = {
        0, 1, 2, 3, 4, 4, 4, 5
    };
    u32 oldZControl;
    u32 newZControl;

    oldZControl = gxdt->zControl;
    newZControl = oldZControl;
    GX_BP_SET_ZCONTROL_PIXEL_FMT(newZControl, p2f[pixelFmt]);
    GX_BP_SET_ZCONTROL_Z_FMT(newZControl, zFmt);
    gxdt->zControl = newZControl;

    if (oldZControl != newZControl) {
        u32 genMode;
        u32 aa_en;
        GX_BP_LOAD_REG(gxdt->zControl);

        aa_en = __cntlzw(pixelFmt - GX_PF_RGBA565_Z16);
        genMode = gxdt->genMode;
        genMode = __rlwimi(genMode, aa_en, 4, 22, 22);
        gxdt->genMode = genMode;
        gxdt->gxDirtyFlags |= GX_DIRTY_GEN_MODE;
    }

    if (p2f[pixelFmt] == 4) {
        u32 dstAlpha = gxdt->dstAlpha;
        u32 aaMode = pixelFmt - GX_PF_Y8;
        GX_BP_SET_DSTALPHA_YUV_FMT(dstAlpha, aaMode);
        GX_BP_SET_OPCODE(dstAlpha, GX_BP_REG_DSTALPHA);
        gxdt->dstAlpha = dstAlpha;
        GX_BP_LOAD_REG(gxdt->dstAlpha);
    }

    gxdt->lastWriteWasXF = FALSE;
}

void GXSetDither(GXBool enable) {
    u32 blendMode = gxdt->blendMode;
    GX_BP_SET_BLENDMODE_DITHER(blendMode, enable);
    GX_BP_LOAD_REG(blendMode);
    gxdt->blendMode = blendMode;
    gxdt->lastWriteWasXF = FALSE;
}

void GXSetDstAlpha(GXBool enable, u8 alpha) {
    u32 dstAlpha = gxdt->dstAlpha;
    GX_BP_SET_DSTALPHA_ALPHA(dstAlpha, alpha);
    GX_BP_SET_DSTALPHA_ENABLE(dstAlpha, enable);
    GX_BP_LOAD_REG(dstAlpha);
    gxdt->dstAlpha = dstAlpha;
    gxdt->lastWriteWasXF = FALSE;
}

void GXSetFieldMask(GXBool enableEven, GXBool enableOdd) {
    u32 cmd = 0;
    GX_BP_SET_FIELDMASK_ODD(cmd, enableOdd);
    GX_BP_SET_FIELDMASK_EVEN(cmd, enableEven);
    GX_BP_SET_OPCODE(cmd, GX_BP_REG_FIELDMASK);
    GX_BP_LOAD_REG(cmd);
    gxdt->lastWriteWasXF = FALSE;
}

void GXSetFieldMode(GXBool texLOD, GXBool adjustAR) {
    u32 linePtWidth = gxdt->linePtWidth;
    GX_BP_SET_LINEPTWIDTH_ADJUST_AR(linePtWidth, adjustAR);
    gxdt->linePtWidth = linePtWidth;
    GX_BP_LOAD_REG(gxdt->linePtWidth);
    __GXFlushTextureState();

    GX_BP_LOAD_REG((GX_BP_REG_FIELDMODE << 24) | (u32)texLOD);
    __GXFlushTextureState();
}
