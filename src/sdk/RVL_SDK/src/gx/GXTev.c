#include <revolution/GX.h>
#include <revolution/gx/GXTypes.h>

/* Per-stage TEV combiner registers are stored in gxdt at:
 *   colorCombiner[stage] = *(u32*)(gxdt + 0x180 + stage*4)   (BP 0xC0..0xCF)
 *   alphaCombiner[stage] = *(u32*)(gxdt + 0x1C0 + stage*4)   (BP 0xC1..0xCF)
 *   tevOrder[stage]      = *(u32*)(gxdt + 0x150 + stage*4)   (BP 0x28..0x2F, shared pair)
 *   tevKSel[pair]        = *(u32*)(gxdt + 0x200 + pair*4)    (BP 0xF6..0xFD)
 * All of these live in the UNK regions of GXData.
 */

/* Lookup tables for GXSetTevOp — must be declared in this order so
 * MWCC places them consecutively: ST0(0x00), ST1(0x14), AST0(0x28), AST1(0x3C).
 * Code loads TEVCOpTableST0 as a base and uses +0x14/+0x28/+0x3C offsets.
 */
static u32 TEVCOpTableST0[5] = {
    0xC008F8AF,
    0xC008A89F,
    0xC008AC8F,
    0xC008FFF8,
    0xC008FFFA,
};

static u32 TEVCOpTableST1[5] = {
    0xC008F80F,
    0xC008089F,
    0xC0080C8F,
    0xC008FFF8,
    0xC008FFF0,
};

static u32 TEVAOpTableST0[5] = {
    0xC108F2F0,
    0xC108FFD0,
    0xC108F2F0,
    0xC108FFC0,
    0xC108FFD0,
};

static u32 TEVAOpTableST1[5] = {
    0xC108F070,
    0xC108FF80,
    0xC108F070,
    0xC108FFC0,
    0xC108FF80,
};

void GXSetTevOp(GXTevStageID stage, GXTevMode mode) {
    u32 *colorEntry;
    u32 *alphaEntry;
    /* Force one lis/addi for TEVCOpTableST0 before the branch by declaring tableBase here */
    u32 *tableBase = TEVCOpTableST0;

    if (stage == GX_TEVSTAGE0) {
        u32 modeOff = (u32)mode * 4;
        u32 *colorBase = TEVCOpTableST0;
        u32 *alphaBase = TEVAOpTableST0;
        colorEntry = (u32 *)((u8 *)colorBase + modeOff);
        alphaEntry = (u32 *)((u8 *)alphaBase + modeOff);
    } else {
        u32 modeOff = (u32)mode * 4;
        u32 *colorBase = TEVCOpTableST1;
        u32 *alphaBase = TEVAOpTableST1;
        colorEntry = (u32 *)((u8 *)colorBase + modeOff);
        alphaEntry = (u32 *)((u8 *)alphaBase + modeOff);
    }

    GXData *gx = gxdt;
    u32 stageOff = (u32)stage * 4;
    u32 *colorReg = (u32 *)((u8 *)gx + 0x180 + stageOff);
    u32 *alphaReg = (u32 *)((u8 *)gx + 0x1C0 + stageOff);

    {
        u32 val = *colorReg;
        val = __rlwimi(val, *colorEntry, 0, 8, 31);
        WGPIPE.c = GX_FIFO_CMD_LOAD_BP_REG;
        WGPIPE.i = val;
        *colorReg = val;
    }
    {
        u32 val = *alphaReg;
        val = __rlwimi(val, 0, 0, 28, 7);
        val = __rlwimi(val, *alphaEntry, 0, 8, 27);
        WGPIPE.c = GX_FIFO_CMD_LOAD_BP_REG;
        WGPIPE.i = val;
        *alphaReg = val;
    }
    gx->lastWriteWasXF = 0;
}

void GXSetTevColorIn(GXTevStageID stage, GXTevColorArg a, GXTevColorArg b, GXTevColorArg c,
                     GXTevColorArg d) {
    GXData *gx = gxdt;
    u32 stageOff = (u32)stage * 4;
    u32 *colorReg = (u32 *)((u8 *)gx + 0x180 + stageOff);
    u32 val = *colorReg;

    val = __rlwimi(val, (u32)a, 12, 16, 19);
    val = __rlwimi(val, (u32)b, 8, 20, 23);
    WGPIPE.c = GX_FIFO_CMD_LOAD_BP_REG;
    val = __rlwimi(val, (u32)c, 4, 24, 27);
    val = __rlwimi(val, (u32)d, 0, 28, 31);
    WGPIPE.i = val;
    *colorReg = val;
    gx->lastWriteWasXF = 0;
}

void GXSetTevAlphaIn(GXTevStageID stage, GXTevAlphaArg a, GXTevAlphaArg b, GXTevAlphaArg c,
                     GXTevAlphaArg d) {
    GXData *gx = gxdt;
    u32 stageOff = (u32)stage * 4;
    u32 *alphaReg = (u32 *)((u8 *)gx + 0x1C0 + stageOff);
    u32 val = *alphaReg;

    val = __rlwimi(val, (u32)a, 13, 16, 18);
    val = __rlwimi(val, (u32)b, 10, 19, 21);
    WGPIPE.c = GX_FIFO_CMD_LOAD_BP_REG;
    val = __rlwimi(val, (u32)c, 7, 22, 24);
    val = __rlwimi(val, (u32)d, 4, 25, 27);
    WGPIPE.i = val;
    *alphaReg = val;
    gx->lastWriteWasXF = 0;
}

void GXSetTevColorOp(GXTevStageID stage, GXTevOp op, GXTevBias bias, GXTevScale scale,
                     u8 clamp, GXTevRegID outReg) {
    GXData *gx = gxdt;
    u32 stageOff = (u32)stage * 4;
    u32 *colorReg = (u32 *)((u8 *)gx + 0x180 + stageOff);
    u32 val = *colorReg;

    val = __rlwimi(val, (u32)op, 18, 13, 13);
    if (op <= GX_TEV_SUB) {
        val = __rlwimi(val, (u32)scale, 20, 10, 11);
        val = __rlwimi(val, (u32)bias, 16, 14, 15);
    } else {
        val = __rlwimi(val, (u32)op, 19, 10, 11);
        val |= (u32)0x3 << 16;
    }

    WGPIPE.c = GX_FIFO_CMD_LOAD_BP_REG;
    val = __rlwimi(val, (u32)clamp, 19, 12, 12);
    val = __rlwimi(val, (u32)outReg, 22, 8, 9);
    WGPIPE.i = val;
    *colorReg = val;
    gx->lastWriteWasXF = 0;
}

void GXSetTevAlphaOp(GXTevStageID stage, GXTevOp op, GXTevBias bias, GXTevScale scale,
                     u8 clamp, GXTevRegID outReg) {
    GXData *gx = gxdt;
    u32 stageOff = (u32)stage * 4;
    u32 *alphaReg = (u32 *)((u8 *)gx + 0x1C0 + stageOff);
    u32 val = *alphaReg;

    val = __rlwimi(val, (u32)op, 18, 13, 13);
    if (op <= GX_TEV_SUB) {
        val = __rlwimi(val, (u32)scale, 20, 10, 11);
        val = __rlwimi(val, (u32)bias, 16, 14, 15);
    } else {
        val = __rlwimi(val, (u32)op, 19, 10, 11);
        val |= (u32)0x3 << 16;
    }

    WGPIPE.c = GX_FIFO_CMD_LOAD_BP_REG;
    val = __rlwimi(val, (u32)clamp, 19, 12, 12);
    val = __rlwimi(val, (u32)outReg, 22, 8, 9);
    WGPIPE.i = val;
    *alphaReg = val;
    gx->lastWriteWasXF = 0;
}

void GXSetTevColor(GXTevRegID reg, GXColor color) {
    u32 packed = *(u32 *)&color;
    u32 idx = (u32)reg * 2;
    u32 loReg = (idx + 0xE0) << 24;
    u32 hiReg;

    WGPIPE.c = GX_FIFO_CMD_LOAD_BP_REG;
    loReg = __rlwimi(loReg, packed, 8, 24, 31);  /* alpha in [31:24] */
    loReg = __rlwimi(loReg, packed, 12, 12, 19); /* red in [19:12] */
    WGPIPE.i = loReg;

    hiReg = (idx + 0xE1) << 24;
    hiReg = __rlwimi(hiReg, packed, 24, 24, 31); /* blue in [31:24] */
    hiReg = __rlwimi(hiReg, packed, 28, 12, 19); /* green in [19:12] */
    GXData *gx = gxdt;
    WGPIPE.c = GX_FIFO_CMD_LOAD_BP_REG;
    WGPIPE.i = hiReg;
    WGPIPE.c = GX_FIFO_CMD_LOAD_BP_REG;
    WGPIPE.i = hiReg;
    WGPIPE.c = GX_FIFO_CMD_LOAD_BP_REG;
    WGPIPE.i = hiReg;
    gx->lastWriteWasXF = 0;
}

void GXSetTevColorS10(GXTevRegID reg, GXColorS10 color) {
    u32 loVal = *(u32 *)&color;        /* r16=color.r, lo16=color.g  -- wait, GXColorS10 is s16 r,g,b,a */
    u32 hiVal = *((u32 *)&color + 1);  /* r16=color.b, lo16=color.a */
    u32 idx = (u32)reg * 2;
    u32 loReg = (idx + 0xE0) << 24;
    u32 hiReg;

    WGPIPE.c = GX_FIFO_CMD_LOAD_BP_REG;
    loReg = __rlwimi(loReg, loVal, 16, 21, 31);  /* r[10:0] in bits [31:21] */
    loReg = __rlwimi(loReg, hiVal, 12, 9, 19);   /* b[10:0] in bits [19:9] */
    WGPIPE.i = loReg;

    hiReg = (idx + 0xE1) << 24;
    hiReg = __rlwimi(hiReg, hiVal, 16, 21, 31);  /* b[10:0] in bits [31:21] -- wait */
    hiReg = __rlwimi(hiReg, loVal, 12, 9, 19);   /* g[10:0] in bits [19:9] */
    GXData *gx = gxdt;
    WGPIPE.c = GX_FIFO_CMD_LOAD_BP_REG;
    WGPIPE.i = hiReg;
    WGPIPE.c = GX_FIFO_CMD_LOAD_BP_REG;
    WGPIPE.i = hiReg;
    WGPIPE.c = GX_FIFO_CMD_LOAD_BP_REG;
    WGPIPE.i = hiReg;
    gx->lastWriteWasXF = 0;
}

void GXSetTevKColor(GXTevKColorID reg, GXColor color) {
    u32 packed = *(u32 *)&color;
    u32 idx = (u32)reg * 2;
    u32 loReg = (idx + 0xE0) << 24;
    u32 hiReg;
    u32 ksel = 0x8;

    WGPIPE.c = GX_FIFO_CMD_LOAD_BP_REG;
    loReg = __rlwimi(loReg, packed, 8, 24, 31);
    loReg = __rlwimi(loReg, packed, 12, 12, 19);
    loReg = __rlwimi(loReg, ksel, 20, 8, 11);
    WGPIPE.i = loReg;

    hiReg = (idx + 0xE1) << 24;
    GXData *gx = gxdt;
    hiReg = __rlwimi(hiReg, packed, 24, 24, 31);
    hiReg = __rlwimi(hiReg, packed, 28, 12, 19);
    hiReg = __rlwimi(hiReg, ksel, 20, 8, 11);
    WGPIPE.c = GX_FIFO_CMD_LOAD_BP_REG;
    WGPIPE.i = hiReg;
    gx->lastWriteWasXF = 0;
}

void GXSetTevKColorSel(GXTevStageID stage, GXTevKColorSel sel) {
    u32 odd = (u32)stage & 1;
    GXData *gx = gxdt;
    u32 pairOff = ((u32)stage >> 1) << 2;  /* extlwi r0, r3, 30, 1 = (stage/2)*4 */
    u32 *kselReg = (u32 *)((u8 *)gx + 0x200 + pairOff);
    u32 val;

    if (odd) {
        val = *kselReg;
        val = __rlwimi(val, (u32)sel, 14, 13, 17);
        *kselReg = val;
    } else {
        val = *kselReg;
        val = __rlwimi(val, (u32)sel, 4, 23, 27);
        *kselReg = val;
    }

    WGPIPE.c = GX_FIFO_CMD_LOAD_BP_REG;
    WGPIPE.i = *kselReg;
    gx->lastWriteWasXF = 0;
}

void GXSetTevKAlphaSel(GXTevStageID stage, GXTevKAlphaSel sel) {
    u32 odd = (u32)stage & 1;
    GXData *gx = gxdt;
    u32 pairOff = ((u32)stage >> 1) << 2;  /* extlwi r0, r3, 30, 1 = (stage/2)*4 */
    u32 *kselReg = (u32 *)((u8 *)gx + 0x200 + pairOff);
    u32 val;

    if (odd) {
        val = *kselReg;
        val = __rlwimi(val, (u32)sel, 19, 8, 12);
        *kselReg = val;
    } else {
        val = *kselReg;
        val = __rlwimi(val, (u32)sel, 9, 18, 22);
        *kselReg = val;
    }

    WGPIPE.c = GX_FIFO_CMD_LOAD_BP_REG;
    WGPIPE.i = *kselReg;
    gx->lastWriteWasXF = 0;
}

void GXSetTevSwapMode(GXTevStageID stage, GXTevSwapSel rasTable, GXTevSwapSel texTable) {
    GXData *gx = gxdt;
    u32 stageOff = (u32)stage * 4;
    u32 *alphaReg = (u32 *)((u8 *)gx + 0x1C0 + stageOff);
    u32 val = *alphaReg;

    val = __rlwimi(val, (u32)rasTable, 0, 30, 31);
    val = __rlwimi(val, (u32)texTable, 2, 28, 29);
    *alphaReg = val;

    WGPIPE.c = GX_FIFO_CMD_LOAD_BP_REG;
    WGPIPE.i = *alphaReg;
    gx->lastWriteWasXF = 0;
}

void GXSetTevSwapModeTable(GXTevSwapSel swapSel, GXTevColorChan r, GXTevColorChan g,
                           GXTevColorChan b, GXTevColorChan a) {
    u32 evenOff = (u32)swapSel * 8;   /* slwi r11, r3, 3 */
    u32 tmp = (u32)swapSel * 2;       /* slwi r12, r3, 1 */
    u32 oddIdx = tmp + 1;              /* addi r3, r12, 1 */
    u32 val0;
    u32 oddOff;
    /* Load gxdt; assign kselBase first so it gets r31 (longer-lived than gx which is r30) */
    GXData *gx = gxdt;
    u32 *kselBase = (u32 *)((u8 *)gxdt + 0x200);  /* use gxdt directly so kselBase is independent */
    oddOff = oddIdx * 4;              /* slwi r4, r3, 2 */

    val0 = *(u32 *)((u8 *)kselBase + evenOff);
    val0 = __rlwimi(val0, (u32)r, 0, 30, 31);
    val0 = __rlwimi(val0, (u32)g, 2, 28, 29);
    *(u32 *)((u8 *)kselBase + evenOff) = val0;

    WGPIPE.c = GX_FIFO_CMD_LOAD_BP_REG;
    WGPIPE.i = *(u32 *)((u8 *)kselBase + evenOff);

    val0 = *(u32 *)((u8 *)kselBase + oddOff);
    val0 = __rlwimi(val0, (u32)b, 0, 30, 31);
    val0 = __rlwimi(val0, (u32)a, 2, 28, 29);
    *(u32 *)((u8 *)kselBase + oddOff) = val0;

    WGPIPE.c = GX_FIFO_CMD_LOAD_BP_REG;
    WGPIPE.i = *(u32 *)((u8 *)kselBase + oddOff);
    gx->lastWriteWasXF = 0;
}

void GXSetAlphaCompare(GXCompare comp0, u8 ref0, GXAlphaOp op, GXCompare comp1, u8 ref1) {
    WGPIPE.c = GX_FIFO_CMD_LOAD_BP_REG;
    u32 val = (u32)0xF3 << 24;
    val = __rlwimi(val, (u32)ref0, 0, 24, 31);
    GXData *gx = gxdt;
    val = __rlwimi(val, (u32)ref1, 8, 16, 23);
    val = __rlwimi(val, (u32)comp0, 16, 13, 15);
    val = __rlwimi(val, (u32)comp1, 19, 10, 12);
    val = __rlwimi(val, (u32)op, 22, 8, 9);
    WGPIPE.i = val;
    gx->lastWriteWasXF = 0;
}

void GXSetZTexture(GXZTexOp op, GXTexFmt fmt, u8 bias) {
    u32 fmtCode;
    u32 reg0 = 0;
    u32 reg0opcode = 0xF4;

    reg0 = __rlwimi(reg0, (u32)bias, 0, 8, 31);
    reg0 = __rlwimi(reg0, reg0opcode, 24, 0, 7);

    /* Map GXTexFmt to 3-bit ztex format code.
     * Target asm: cmpwi Z8; beq; cmpwi Z16; beq; cmpwi Z24; beq; b; then 4 li labels
     * Use switch with fmtCode pre-set to 2 so no default label needed,
     * forcing MWCC to emit separate cmpwi for each case including Z24.
     */
    fmtCode = 2;
    switch (fmt) {
    case GX_TF_Z8:
        fmtCode = 0;
        break;
    case GX_TF_Z16:
        fmtCode = 1;
        break;
    case GX_TF_Z24X8:
        fmtCode = 2;
        break;
    }

    WGPIPE.c = GX_FIFO_CMD_LOAD_BP_REG;
    u32 reg1 = 0;
    reg1 = __rlwimi(reg1, (u32)fmtCode, 0, 30, 31);
    u32 reg1opcode = 0xF5;
    WGPIPE.i = reg0;
    reg1 = __rlwimi(reg1, (u32)op, 2, 28, 29);
    GXData *gx = gxdt;
    reg1 = __rlwimi(reg1, reg1opcode, 24, 0, 7);
    WGPIPE.c = GX_FIFO_CMD_LOAD_BP_REG;
    WGPIPE.i = reg1;
    gx->lastWriteWasXF = 0;
}

/* Local table for texmap->c2r mapping used in GXSetTevOrder */
static u32 c2r[9] = {
    0, 1, 0, 1, 0, 1, 7, 5, 6,
};

void GXSetTevOrder(GXTevStageID stage, GXTexCoordID coord, GXTexMapID map, GXChannelID color) {
    /* texmapNoDisable: clear bits above byte, specifically the disable bit (bit 8 = 0x100) */
    u32 texmapNoDisable = (u32)map & 0xFFU;  /* rlwinm r11, r5, 0, 24, 22 clears bit 8 */
    GXData *gx = gxdt;
    u32 stageOff = (u32)stage * 4;
    /* Signed-divide-by-2 pair index: (stage + (stage>>31)) >> 1 << 2
     * MWCC emits: srwi r9,r3,31; add r9,r9,r3; extlwi r8,r9,30,1
     */
    u32 signedHalf = (u32)((s32)stage >> 31) + (u32)stage;
    u32 pairOff = (signedHalf >> 1) << 2;
    /* stageBase = gxdt + stage*4 (for stw map) */
    u32 *stageBase = (u32 *)((u8 *)gx + stageOff);
    /* Compare texmapNoDisable to 8 using carry: subfc+addze+subfic+andc
     * (map < 8) ? texmapNoDisable : 0
     * MWCC optimization: li r7,8; subfc r0,r7,r11; addze r0,r7; subfic r0,r0,8; andc r9,r11,r0
     */
    u32 enableMap = (texmapNoDisable < 8u) ? texmapNoDisable : 0u;
    /* orderReg = gxdt + 0x150 + pairOff */
    u32 *orderReg = (u32 *)((u8 *)gx + 0x150 + pairOff);

    /* Store original map to gxdt[0x5A4 + stage*4] */
    *(u32 *)((u8 *)gx + 0x5A4 + stageOff) = (u32)map;

    /* Texmap enable flag: target uses cmpwi COORD (r4) to 8, not map.
     * Disable path also resets coord to 0 (li r4, 0x0 in disable branch).
     */
    if ((u32)coord < 8u) {
        u32 flags = *(u32 *)((u8 *)gx + 0x5E8);
        u32 bit = 1u << (u32)stage;
        flags |= bit;
        *(u32 *)((u8 *)gx + 0x5E8) = flags;
    } else {
        u32 flags = *(u32 *)((u8 *)gx + 0x5E8);
        u32 bit = 1u << (u32)stage;
        coord = (GXTexCoordID)0;   /* li r4, 0x0 in disable path */
        flags &= ~bit;
        *(u32 *)((u8 *)gx + 0x5E8) = flags;
    }

    if ((u32)stage & 1u) {
        /* odd stage */
        u32 val = *orderReg;
        val = __rlwimi(val, enableMap, 12, 17, 19);
        val = __rlwimi(val, (u32)coord, 15, 14, 16);
        *orderReg = val;

        u32 colorVal;
        if (color == GX_COLOR_NULL) {
            colorVal = 7;
        } else {
            colorVal = c2r[(u32)color];
        }
        val = __rlwimi(val, colorVal, 19, 10, 12);
        *orderReg = val;

        /* rlwinm. r0, r5, 0, 23, 23 — test bit 8 from MSB (=bit 8 from LSB = 0x100) of map */
        u32 useTexLOD = 0;
        if ((u32)map != 0xFF) {
            if (!((u32)map & 0x100)) {
                useTexLOD = 1;
            }
        }
        val = __rlwimi(val, useTexLOD, 18, 13, 13);
        *orderReg = val;
    } else {
        /* even stage */
        u32 val = *orderReg;
        val = __rlwimi(val, enableMap, 0, 29, 31);
        val = __rlwimi(val, (u32)coord, 3, 26, 28);
        *orderReg = val;

        u32 colorVal;
        if (color == GX_COLOR_NULL) {
            colorVal = 7;
        } else {
            colorVal = c2r[(u32)color];
        }
        val = __rlwimi(val, colorVal, 7, 22, 24);
        *orderReg = val;

        u32 useTexLOD = 0;
        if ((u32)map != 0xFF) {
            if (!((u32)map & 0x100)) {
                useTexLOD = 1;
            }
        }
        val = __rlwimi(val, useTexLOD, 6, 25, 25);
        *orderReg = val;
    }

    WGPIPE.c = GX_FIFO_CMD_LOAD_BP_REG;
    WGPIPE.i = *orderReg;
    gx->lastWriteWasXF = 0;
    *(u32 *)((u8 *)gx + 0x5FC) |= 1;
    (void)stageBase;
}

void GXSetNumTevStages(u8 num) {
    GXData *gx = gxdt;
    u32 val = gx->genMode;
    val = __rlwimi(val, (u32)(num - 1), 10, 18, 21);
    gx->genMode = val;
    *(u32 *)((u8 *)gx + 0x5FC) |= 4;
}
