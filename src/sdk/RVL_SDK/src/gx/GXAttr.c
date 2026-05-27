#define GXATTR_MATCH_HACK
#include <revolution/GX.h>
#include "revolution/gx/GXTypes.h"

void GXSetVtxDesc(GXAttr name, GXAttrType type) {
    switch (name) {
    case GX_VA_PNMTXIDX:
        GX_CP_SET_VCD_LO_POSMATIDX(gxdt->vcdLoReg, type);
        break;
    case GX_VA_TEX0MTXIDX:
        GX_CP_SET_VCD_LO_TEX0MATIDX(gxdt->vcdLoReg, type);
        break;
    case GX_VA_TEX1MTXIDX:
        GX_CP_SET_VCD_LO_TEX1MATIDX(gxdt->vcdLoReg, type);
        break;
    case GX_VA_TEX2MTXIDX:
        GX_CP_SET_VCD_LO_TEX2MATIDX(gxdt->vcdLoReg, type);
        break;
    case GX_VA_TEX3MTXIDX:
        GX_CP_SET_VCD_LO_TEX3MATIDX(gxdt->vcdLoReg, type);
        break;
    case GX_VA_TEX4MTXIDX:
        GX_CP_SET_VCD_LO_TEX4MATIDX(gxdt->vcdLoReg, type);
        break;
    case GX_VA_TEX5MTXIDX:
        GX_CP_SET_VCD_LO_TEX5MATIDX(gxdt->vcdLoReg, type);
        break;
    case GX_VA_TEX6MTXIDX:
        GX_CP_SET_VCD_LO_TEX6MATIDX(gxdt->vcdLoReg, type);
        break;
    case GX_VA_TEX7MTXIDX:
        GX_CP_SET_VCD_LO_TEX7MATIDX(gxdt->vcdLoReg, type);
        break;
    case GX_VA_POS:
        GX_CP_SET_VCD_LO_POSITION(gxdt->vcdLoReg, type);
        break;
    case GX_VA_NRM:
        if (type != GX_NONE) {
            gxdt->normal = TRUE;
            gxdt->binormal = FALSE;
            gxdt->normalType = type;
        } else {
            gxdt->normal = FALSE;
        }
        break;
    case GX_VA_NBT:
        if (type != GX_NONE) {
            gxdt->binormal = TRUE;
            gxdt->normal = FALSE;
            gxdt->normalType = type;
        } else {
            gxdt->binormal = FALSE;
        }
        break;
    case GX_VA_CLR0:
        GX_CP_SET_VCD_LO_COLORDIFFUSED(gxdt->vcdLoReg, type);
        break;
    case GX_VA_CLR1:
        GX_CP_SET_VCD_LO_COLORSPECULAR(gxdt->vcdLoReg, type);
        break;
    case GX_VA_TEX0:
        GX_CP_SET_VCD_HI_TEX0COORD(gxdt->vcdHiReg, type);
        break;
    case GX_VA_TEX1:
        GX_CP_SET_VCD_HI_TEX1COORD(gxdt->vcdHiReg, type);
        break;
    case GX_VA_TEX2:
        GX_CP_SET_VCD_HI_TEX2COORD(gxdt->vcdHiReg, type);
        break;
    case GX_VA_TEX3:
        GX_CP_SET_VCD_HI_TEX3COORD(gxdt->vcdHiReg, type);
        break;
    case GX_VA_TEX4:
        GX_CP_SET_VCD_HI_TEX4COORD(gxdt->vcdHiReg, type);
        break;
    case GX_VA_TEX5:
        GX_CP_SET_VCD_HI_TEX5COORD(gxdt->vcdHiReg, type);
        break;
    case GX_VA_TEX6:
        GX_CP_SET_VCD_HI_TEX6COORD(gxdt->vcdHiReg, type);
        break;
    case GX_VA_TEX7:
        GX_CP_SET_VCD_HI_TEX7COORD(gxdt->vcdHiReg, type);
        break;
    default:
        break;
    }

    if (gxdt->normal || gxdt->binormal) {
        GX_CP_SET_VCD_LO_NORMAL(gxdt->vcdLoReg, gxdt->normalType);
    } else {
        gxdt->vcdLoReg = gxdt->vcdLoReg & ~GX_CP_VCD_LO_NORMAL_MASK;
    }

    gxdt->gxDirtyFlags |= GX_DIRTY_VCD;
}

void __GXSetVCD(void) {
    u32 normalCount;
    u32 invSpec;

    GX_CP_LOAD_REG(GX_CP_REG_VCD_LO, gxdt->vcdLoReg);
    GX_CP_LOAD_REG(GX_CP_REG_VCD_HI, gxdt->vcdHiReg);

    if (gxdt->binormal) {
        normalCount = 2;
    } else {
        u8 nm = gxdt->normal;
        normalCount = (u32)((-(s32)nm) | nm) >> 31;
    }

    {
        u32 vcdLo = gxdt->vcdLoReg;
        u32 vcdHi = gxdt->vcdHiReg;
        u32 clrCount = (u32)(0x21 - __cntlzw((vcdLo >> 13) & 0xF)) >> 1;
        u32 texCount = (u32)(0x21 - __cntlzw((u16)vcdHi));

        invSpec = clrCount | (normalCount << 2) | (texCount >> 1);
        GX_XF_LOAD_REG(GX_XF_REG_INVERTEXSPEC, invSpec);
    }

    gxdt->lastWriteWasXF = TRUE;
}

void __GXCalculateVLim(void) {
    static const u8 tbl1[] = {0x00, 0x04, 0x01, 0x02};
    static const u8 tbl2[4] = {0x00, 0x08, 0x01, 0x02};
    static const u8 tbl3[4] = {0x00, 0x0C, 0x01, 0x02};
    u32 vcdLo;
    u32 vcdHi;
    u32 normalMult;
    u32 sum;

    if (gxdt->SHORT_0x4 == 0) {
        return;
    }

    vcdLo = gxdt->vcdLoReg;
    vcdHi = gxdt->vcdHiReg;

    sum = (vcdLo & 1)
        + (vcdLo >> 1 & 1)
        + (vcdLo >> 2 & 1)
        + (vcdLo >> 3 & 1)
        + (vcdLo >> 4 & 1)
        + (vcdLo >> 5 & 1)
        + (vcdLo >> 6 & 1)
        + (vcdLo >> 7 & 1)
        + (vcdLo >> 8 & 1)
        + tbl3[vcdLo >> 9 & 3];

    normalMult = 1;
    if ((gxdt->vatA[0] >> 9 & 1) == 1) {
        normalMult = 3;
    }
    sum += tbl3[vcdLo >> 11 & 3] * normalMult;

    sum += tbl1[vcdLo >> 13 & 3]
        + tbl1[vcdLo >> 15 & 3]
        + tbl2[vcdHi & 3]
        + tbl2[vcdHi >> 2 & 3]
        + tbl2[vcdHi >> 4 & 3]
        + tbl2[vcdHi >> 6 & 3]
        + tbl2[vcdHi >> 8 & 3]
        + tbl2[vcdHi >> 10 & 3]
        + tbl2[vcdHi >> 12 & 3]
        + tbl2[vcdHi >> 14 & 3];

    gxdt->vlim = (u16)sum;
}

void GXClearVtxDesc(void) {
    u32 reg = 0;
    GX_CP_SET_VCD_LO_POSITION(reg, GX_DIRECT);
    gxdt->vcdLoReg = reg;
    gxdt->vcdHiReg = 0;
    gxdt->normal = FALSE;
    gxdt->binormal = FALSE;
    gxdt->gxDirtyFlags |= GX_DIRTY_VCD;
}

void GXSetVtxAttrFmt(
    GXVtxFmt fmt, GXAttr attr, GXCompCnt compCnt, GXCompType compType, u8 shift
) {
    struct _GXData *vbase = (struct _GXData *)((char *)gxdt + (u32)fmt * 4);

    switch (attr) {
    case GX_VA_POS:
        GX_CP_SET_VAT_GROUP0_POS_CNT(vbase->vatA[0], compCnt);
        GX_CP_SET_VAT_GROUP0_POS_TYPE(vbase->vatA[0], compType);
        GX_CP_SET_VAT_GROUP0_POS_SHIFT(vbase->vatA[0], shift);
        break;
    case GX_VA_NRM:
    case GX_VA_NBT:
        GX_CP_SET_VAT_GROUP0_NRM_TYPE(vbase->vatA[0], compType);
        if (compCnt == 2) {
            /* 3 normals (NBT) */
            vbase->vatA[0] |= 0x200;
            vbase->vatA[0] |= 0x80000000;
        } else {
            GX_CP_SET_VAT_GROUP0_NRM_CNT(vbase->vatA[0], compCnt);
            vbase->vatA[0] &= ~0x80000000;
        }
        break;
    case GX_VA_CLR0:
        GX_CP_SET_VAT_GROUP0_COLORDIFF_CNT(vbase->vatA[0], compCnt);
        GX_CP_SET_VAT_GROUP0_COLORDIFF_TYPE(vbase->vatA[0], compType);
        break;
    case GX_VA_CLR1:
        GX_CP_SET_VAT_GROUP0_COLORSPEC_CNT(vbase->vatA[0], compCnt);
        GX_CP_SET_VAT_GROUP0_COLORSPEC_TYPE(vbase->vatA[0], compType);
        break;
    case GX_VA_TEX0:
        GX_CP_SET_VAT_GROUP0_TXC0_CNT(vbase->vatA[0], compCnt);
        GX_CP_SET_VAT_GROUP0_TXC0_TYPE(vbase->vatA[0], compType);
        GX_CP_SET_VAT_GROUP0_TXC0_SHIFT(vbase->vatA[0], shift);
        break;
    case GX_VA_TEX1:
        GX_CP_SET_VAT_GROUP1_TXC1_CNT(vbase->vatB[0], compCnt);
        GX_CP_SET_VAT_GROUP1_TXC1_TYPE(vbase->vatB[0], compType);
        GX_CP_SET_VAT_GROUP1_TXC1_SHIFT(vbase->vatB[0], shift);
        break;
    case GX_VA_TEX2:
        GX_CP_SET_VAT_GROUP1_TXC2_CNT(vbase->vatB[0], compCnt);
        GX_CP_SET_VAT_GROUP1_TXC2_TYPE(vbase->vatB[0], compType);
        GX_CP_SET_VAT_GROUP1_TXC2_SHIFT(vbase->vatB[0], shift);
        break;
    case GX_VA_TEX3:
        GX_CP_SET_VAT_GROUP1_TXC3_CNT(vbase->vatB[0], compCnt);
        GX_CP_SET_VAT_GROUP1_TXC3_TYPE(vbase->vatB[0], compType);
        GX_CP_SET_VAT_GROUP1_TXC3_SHIFT(vbase->vatB[0], shift);
        break;
    case GX_VA_TEX4:
        GX_CP_SET_VAT_GROUP1_TXC4_CNT(vbase->vatB[0], compCnt);
        GX_CP_SET_VAT_GROUP1_TXC4_TYPE(vbase->vatB[0], compType);
        GX_CP_SET_VAT_GROUP2_TXC4_SHIFT(vbase->vatC[0], shift);
        break;
    case GX_VA_TEX5:
        GX_CP_SET_VAT_GROUP2_TXC5_CNT(vbase->vatC[0], compCnt);
        GX_CP_SET_VAT_GROUP2_TXC5_TYPE(vbase->vatC[0], compType);
        GX_CP_SET_VAT_GROUP2_TXC5_SHIFT(vbase->vatC[0], shift);
        break;
    case GX_VA_TEX6:
        GX_CP_SET_VAT_GROUP2_TXC6_CNT(vbase->vatC[0], compCnt);
        GX_CP_SET_VAT_GROUP2_TXC6_TYPE(vbase->vatC[0], compType);
        GX_CP_SET_VAT_GROUP2_TXC6_SHIFT(vbase->vatC[0], shift);
        break;
    case GX_VA_TEX7:
        GX_CP_SET_VAT_GROUP2_TXC7_CNT(vbase->vatC[0], compCnt);
        GX_CP_SET_VAT_GROUP2_TXC7_TYPE(vbase->vatC[0], compType);
        GX_CP_SET_VAT_GROUP2_TXC7_SHIFT(vbase->vatC[0], shift);
        break;
    default:
        break;
    }

    gxdt->gxDirtyFlags |= GX_DIRTY_VAT;
    gxdt->vatDirtyFlags |= (u8)(1 << (u8)fmt);
}

void GXSetVtxAttrFmtv(s16 fmt, const GXVtxAttrFmtList *list) {
    const GXVtxAttrFmtList *p;
    for (p = list; p->attr != GX_VA_NULL; p++) {
        GXSetVtxAttrFmt((GXVtxFmt)fmt, p->attr, p->compCnt, p->compType, p->shift);
    }
}

void __GXSetVAT(void) {
    u8 dirtyFlags = gxdt->vatDirtyFlags;
    u32 i = 0;
    u32 *p = gxdt->vatA;
    while (dirtyFlags != 0) {
        if (dirtyFlags & 1) {
            GX_CP_LOAD_REG((u8)(GX_CP_REG_VAT_GROUP0 | (u8)i), p[0x00]);
            GX_CP_LOAD_REG((u8)(GX_CP_REG_VAT_GROUP1 | (u8)i), p[0x08]);
            GX_CP_LOAD_REG((u8)(GX_CP_REG_VAT_GROUP2 | (u8)i), p[0x10]);
        }
        dirtyFlags >>= 1;
        i++;
        p++;
    }
    WGPIPE.c = 0;
    gxdt->vatDirtyFlags = 0;
}

void GXSetArray(GXAttr attr, const void *base, u8 stride) {
    u32 idx;
    if (attr == GX_VA_NBT) {
        attr = GX_VA_NRM;
    }
    idx = attr - GX_VA_POS;
    GX_CP_LOAD_REG((u8)(GX_CP_REG_ARRAYBASE | idx), (u32)base & 0x3FFFFFFF);
    GX_CP_LOAD_REG((u8)(GX_CP_REG_ARRAYSTRIDE | idx), stride);
}

void GXInvalidateVtxCache(void) {
    WGPIPE.c = GX_FIFO_CMD_INVAL_VTX;
}

void GXSetTexCoordGen2(
    GXTexCoordID id,
    GXTexGenType type,
    GXTexGenSrc src,
    u32 texMtxIdx,
    GXBool normalize,
    u32 dualTexMtxIdx
) {
    u32 texReg;
    u32 dualTexReg;
    int srcRow;
    int inputForm;
    u32 t;

    texReg = 0;
    inputForm = 0;
    srcRow = 5;

    switch (src) {
    case GX_TG_POS:
        srcRow = 0;
        inputForm = 1;
        break;
    case GX_TG_NRM:
        srcRow = 1;
        inputForm = 1;
        break;
    case GX_TG_BINRM:
        srcRow = 3;
        inputForm = 1;
        break;
    case GX_TG_TANGENT:
        srcRow = 4;
        inputForm = 1;
        break;
    case GX_TG_TEX0:
        srcRow = 5;
        break;
    case GX_TG_TEX1:
        srcRow = 6;
        break;
    case GX_TG_TEX2:
        srcRow = 7;
        break;
    case GX_TG_TEX3:
        srcRow = 8;
        break;
    case GX_TG_TEX4:
        srcRow = 9;
        break;
    case GX_TG_TEX5:
        srcRow = 10;
        break;
    case GX_TG_TEX6:
        srcRow = 11;
        break;
    case GX_TG_TEX7:
        srcRow = 12;
        break;
    case GX_TG_COLOR0:
        srcRow = 2;
        break;
    case GX_TG_COLOR1:
        srcRow = 2;
        break;
    default:
        break;
    }

    t = (u32)type - 2;
    if (t > 7) {
        if (type == GX_TG_MTX2x4) {
            texReg = 0;
            texReg = GX_BITSET(texReg, 29, 1, inputForm);
            texReg = GX_BITSET(texReg, 20, 5, srcRow);
        } else if (type == GX_TG_MTX3x4) {
            texReg = 0;
            texReg = GX_BITSET(texReg, 30, 1, 1);
            texReg = GX_BITSET(texReg, 29, 1, inputForm);
            texReg = GX_BITSET(texReg, 20, 5, srcRow);
        } else if (type == GX_TG_SRTG) {
            texReg = 0;
            texReg = GX_BITSET(texReg, 29, 1, inputForm);
            if (src == GX_TG_COLOR0) {
                texReg = GX_BITSET(texReg, 25, 3, 2);
            } else {
                texReg = GX_BITSET(texReg, 25, 3, 3);
            }
            texReg = GX_BITSET(texReg, 20, 5, 2);
        }
    } else {
        texReg = 0;
        texReg = GX_BITSET(texReg, 29, 1, inputForm);
        texReg = GX_BITSET(texReg, 25, 3, 1);
        texReg = GX_BITSET(texReg, 20, 5, srcRow);
        texReg = GX_BITSET(texReg, 17, 3, (u32)src - 0xC);
        texReg = GX_BITSET(texReg, 14, 3, t);
    }

    gxdt->texRegs[id] = texReg;
    gxdt->gxDirtyFlags |= GX_DIRTY_TEX0 << id;

    dualTexReg = 0;
    GX_XF_SET_DUALTEX_NORMALIZE(dualTexReg, normalize);
    GX_XF_SET_DUALTEX_BASEROW(dualTexReg, (u32)dualTexMtxIdx - 0x40);
    gxdt->dualTexRegs[id] = dualTexReg;

    switch (id) {
    case GX_TEXCOORD0:
        GX_XF_SET_MATRIXINDEX0_TEX0(gxdt->matrixIndex0, texMtxIdx);
        break;
    case GX_TEXCOORD1:
        GX_XF_SET_MATRIXINDEX0_TEX1(gxdt->matrixIndex0, texMtxIdx);
        break;
    case GX_TEXCOORD2:
        GX_XF_SET_MATRIXINDEX0_TEX2(gxdt->matrixIndex0, texMtxIdx);
        break;
    case GX_TEXCOORD3:
        GX_XF_SET_MATRIXINDEX0_TEX3(gxdt->matrixIndex0, texMtxIdx);
        break;
    case GX_TEXCOORD4:
        GX_XF_SET_MATRIXINDEX1_TEX4(gxdt->matrixIndex1, texMtxIdx);
        break;
    case GX_TEXCOORD5:
        GX_XF_SET_MATRIXINDEX1_TEX5(gxdt->matrixIndex1, texMtxIdx);
        break;
    case GX_TEXCOORD6:
        GX_XF_SET_MATRIXINDEX1_TEX6(gxdt->matrixIndex1, texMtxIdx);
        break;
    default:
        GX_XF_SET_MATRIXINDEX1_TEX7(gxdt->matrixIndex1, texMtxIdx);
        break;
    }

    gxdt->gxDirtyFlags |= 0x04000000;
}

void GXSetNumTexGens(u8 num) {
    GX_BP_SET_GENMODE_NUMTEX(gxdt->genMode, num);
    gxdt->gxDirtyFlags |= GX_DIRTY_NUM_TEX;
    gxdt->gxDirtyFlags |= GX_DIRTY_GEN_MODE;
}
