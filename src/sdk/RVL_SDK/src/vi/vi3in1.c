#include "revolution/vi/vi.h"
#include "revolution/vi/vi3in1.h"
#include "revolution/os/OS.h"
#include "types.h"

// Forward declarations of functions in i2c.c
void WaitMicroTime(u32 microseconds);
void __VISendI2CData(u32 addr, u8 *data, u32 size);

// Declared in vi.c
u32 VIGetDTVStatus(void);

// gammaSet: 7 entries of 0x22 (34) bytes each = 0x41E bytes + 2 bytes padding
// Each entry is 34 bytes (13 u16 values + remaining bytes)
typedef struct {
    u16 val[11]; // indices 0-10, 2 bytes each = 22 bytes
    u8  bval[7]; // indices 11-17, single bytes
    u16 end[3];  // 14,15,16 as u16
    u8  ebits[3]; // extra bits for 14,15,16
} GammaEntry;

// gammaSet is a 7-element array indexed by __gamma value
// 0x22 = 34 bytes per entry, 7 entries
extern u8 gammaSet[7 * 0x22];

// ACP data tables (0x1A bytes each = 26 bytes)
extern u8 VINtscACPType1[0x1A];
extern u8 VINtscACPType2[0x1A];
extern u8 VINtscACPType3[0x1A];
extern u8 VIPalACPType1[0x1A];
extern u8 VIPalACPType2[0x1A];
extern u8 VIPalACPType3[0x1A];
extern u8 VIEurgb60ACPType1[0x1A];
extern u8 VIEurgb60ACPType2[0x1A];
extern u8 VIEurgb60ACPType3[0x1A];
extern u8 VIMpalACPType1[0x1A];
extern u8 VIMpalACPType2[0x1A];
extern u8 VIMpalACPType3[0x1A];
extern u8 VIProgressiveACPType[0x1A];
extern u8 VIZeroACPType[0x1A];

// sdata variables (initialized)
s32 __tvType = 0xFF;
u8 __wd0 = 0xFF;
u8 __wd1 = 0xFF;
u8 __wd2 = 0xFF;
u8 __gp1 = 0xFF;
u8 __gp2 = 0xFF;
u8 __gp3 = 0xFF;
u8 __gp4 = 0xFF;
u8 __cc1 = 0xFF;
u8 __cc2 = 0xFF;
u8 __cc3 = 0xFF;
u8 __cc4 = 0xFF;
u8 __filter = 0xFF;

// sbss variables (zero-initialized)
s32 Vdac_Flag_Changed;
s32 __current_3in1_video_mode;
s32 __level;
s32 __gamma;
s32 __type;
s32 Vdac_Flag_Region;

void __VISetYUVSEL(u32 yuv) {
    s32 region;
    u32 tvFormat;
    u8 buf[2];

    tvFormat = *(u32 *)0x800000CC;
    if (tvFormat == 1) {
        region = 2;
    } else if (tvFormat == 5) {
        region = 2;
    } else if (tvFormat == 2) {
        region = 1;
    } else if (tvFormat == 0) {
        region = 0;
    } else {
        region = 0;
    }
    Vdac_Flag_Region = region;
    buf[0] = 1;
    buf[1] = (u8)(((yuv & 0xFF) << 5) | (u8)region);
    __VISendI2CData(0xe0, buf, 2);
    WaitMicroTime(2);
}

void __VISetFilter4EURGB60(u8 val) {
    u8 buf[2];
    buf[0] = 0x6e;
    buf[1] = val;
    __VISendI2CData(0xe0, buf, 2);
    WaitMicroTime(2);
}

void __VISetCGMS(void) {
    u8 buf[3];
    u8 v = (u8)(__wd0 & 0x3);
    buf[0] = 5;
    buf[1] = (u8)(((__wd1 & 0xF) << 2) | v);
    buf[2] = __wd2;
    __VISendI2CData(0xe0, buf, 3);
    WaitMicroTime(2);
}

void __VISetWSS(void) {
    u8 buf[3];
    u8 v1 = (u8)(__gp1 & 0xF);
    u8 v2 = (u8)(__gp3 & 0x7);
    buf[0] = 8;
    buf[1] = (u8)(((__gp2 & 0xF) << 4) | v1);
    buf[2] = (u8)(((__gp4 & 0x7) << 3) | v2);
    __VISendI2CData(0xe0, buf, 3);
    WaitMicroTime(2);
}

void __VISetClosedCaption(void) {
    u8 buf[5];
    buf[0] = 0x7a;
    buf[1] = (u8)(__cc1 & 0x7F);
    buf[2] = (u8)(__cc2 & 0x7F);
    buf[3] = (u8)(__cc3 & 0x7F);
    buf[4] = (u8)(__cc4 & 0x7F);
    __VISendI2CData(0xe0, buf, 5);
    WaitMicroTime(2);
}

void __VISetMacrovision(void) {
    u8 *acpData;
    u8 buf[0x1C];
    int i;

    if (__type == 2) {
        switch (__tvType) {
        case 0: // NTSC
            acpData = VINtscACPType2;
            break;
        case 1: // PAL
            acpData = VIPalACPType2;
            break;
        case 2: // MPAL
            acpData = VIMpalACPType2;
            break;
        case 5: // EURGB60
            acpData = VIEurgb60ACPType2;
            break;
        default:
            return;
        }
    } else if (__type == 3) {
        switch (__tvType) {
        case 0:
            acpData = VINtscACPType3;
            break;
        case 1:
            acpData = VIPalACPType3;
            break;
        case 2:
            acpData = VIMpalACPType3;
            break;
        case 5:
            acpData = VIEurgb60ACPType3;
            break;
        default:
            return;
        }
    } else if (__type == 4) {
        switch (__tvType) {
        case 0:
            acpData = VINtscACPType1;
            break;
        case 1:
            acpData = VIPalACPType1;
            break;
        case 2:
            acpData = VIMpalACPType1;
            break;
        case 5:
            acpData = VIEurgb60ACPType1;
            break;
        default:
            return;
        }
    } else if (__type == 1) {
        acpData = VIProgressiveACPType;
    } else {
        return;
    }

    // Load 0x1B bytes starting with 0x40 header
    buf[0] = 0x40;
    for (i = 0; i < 0x1A; i++) {
        buf[i + 1] = acpData[i];
    }
    __VISendI2CData(0xe0, buf, 0x1b);
    WaitMicroTime(2);
}

void __VISetGammaImm(u8 *data) {
    u8 buf[0x22];
    u32 tmp;

    buf[0] = 0x10;

    // 6 u16 values (indices 0-5): store hi then lo
    tmp = ((u16 *)data)[0];
    buf[2] = (u8)tmp;
    buf[1] = (u8)(tmp >> 8);
    tmp = ((u16 *)data)[1];
    buf[4] = (u8)tmp;
    buf[3] = (u8)(tmp >> 8);
    tmp = ((u16 *)data)[2];
    buf[6] = (u8)tmp;
    buf[5] = (u8)(tmp >> 8);
    tmp = ((u16 *)data)[3];
    buf[8] = (u8)tmp;
    buf[7] = (u8)(tmp >> 8);
    tmp = ((u16 *)data)[4];
    buf[10] = (u8)tmp;
    buf[9] = (u8)(tmp >> 8);
    tmp = ((u16 *)data)[5];
    buf[12] = (u8)tmp;
    buf[11] = (u8)(tmp >> 8);

    // 7 single bytes (offset 0xC from data)
    buf[13] = data[0xC];
    buf[14] = data[0xD];
    buf[15] = data[0xE];
    buf[16] = data[0xF];
    buf[17] = data[0x10];
    buf[18] = data[0x11];
    buf[19] = data[0x12];

    // 7 more u16 values with masking (offset 0x14 from data)
    // Each: hi=full byte, lo=masked (&0xC0)
    tmp = ((u16 *)(data + 0x14))[0];
    buf[20] = (u8)(tmp >> 8);
    buf[21] = (u8)(tmp & 0xC0);
    tmp = ((u16 *)(data + 0x14))[1];
    buf[22] = (u8)(tmp >> 8);
    buf[23] = (u8)(tmp & 0xC0);
    tmp = ((u16 *)(data + 0x14))[2];
    buf[24] = (u8)(tmp >> 8);
    buf[25] = (u8)(tmp & 0xC0);
    tmp = ((u16 *)(data + 0x14))[3];
    buf[26] = (u8)(tmp >> 8);
    buf[27] = (u8)(tmp & 0xC0);
    tmp = ((u16 *)(data + 0x14))[4];
    buf[28] = (u8)(tmp >> 8);
    buf[29] = (u8)(tmp & 0xC0);
    tmp = ((u16 *)(data + 0x14))[5];
    buf[30] = (u8)(tmp >> 8);
    buf[31] = (u8)(tmp & 0xC0);
    tmp = ((u16 *)(data + 0x14))[6];
    buf[32] = (u8)(tmp >> 8);
    buf[33] = (u8)(tmp & 0xC0);

    __VISendI2CData(0xe0, buf, 0x22);
    WaitMicroTime(2);
}

void __VISetGamma1_0(void) {
    __VISetGammaImm(gammaSet + 0x154);
}

void __VISetGamma(void) {
    __VISetGammaImm(gammaSet + __gamma * 0x22);
}

void __VISetTrapFilter(void) {
    u8 buf[2];
    buf[0] = 3;
    if (__filter == 1) {
        buf[1] = 0;
    } else {
        buf[1] = 1;
    }
    __VISendI2CData(0xe0, buf, 2);
    WaitMicroTime(2);
}

void __VISetRGBOverDrive(void) {
    u8 buf[2];
    if (Vdac_Flag_Region == 3) {
        buf[0] = 0xa;
        buf[1] = (u8)((__level << 1) | 1);
        __VISendI2CData(0xe0, buf, 2);
        WaitMicroTime(2);
    } else {
        buf[0] = 0xa;
        buf[1] = 0;
        __VISendI2CData(0xe0, buf, 2);
        WaitMicroTime(2);
    }
}

void VISetRGBModeImm(void) {
    Vdac_Flag_Changed = Vdac_Flag_Changed | 0x80;
}

void __VISetRGBModeImm(void) {
    Vdac_Flag_Region = 3;
    u8 buf[2];
    buf[0] = 1;
    buf[1] = 3;
    __VISendI2CData(0xe0, buf, 2);
    WaitMicroTime(2);
}

void __VISetRevolutionModeSimple(void) {
    u8 buf1[2];
    u8 buf2[2];
    u8 buf3[2];
    u8 buf4[2];
    u8 buf5[2];
    u8 buf6[3];
    u8 buf7[3];
    u8 buf8[5];
    u8 buf9[0x1B];
    s32 region;
    u8 *acpPtr;
    u32 tvf;
    int i;

    // Step 1: Send register 0x6A=1
    buf1[0] = 0x6a;
    buf1[1] = 1;
    __VISendI2CData(0xe0, buf1, 2);
    WaitMicroTime(2);

    // Step 2: Send register 0x65=1
    buf2[0] = 0x65;
    buf2[1] = 1;
    __VISendI2CData(0xe0, buf2, 2);
    WaitMicroTime(2);

    // Step 3: Get DTV status to determine region
    VIGetDTVStatus();
    tvf = *(u32 *)0x800000CC;
    if (tvf == 1) {
        region = 2;
    } else if (tvf == 5) {
        region = 2;
    } else if (tvf == 2) {
        region = 1;
    } else if (tvf == 0) {
        region = 0;
    } else {
        region = 0;
    }
    Vdac_Flag_Region = region;

    // Step 4: Set YUVSELL register 0x1 with region
    buf3[0] = 1;
    buf3[1] = (u8)(((u8)(tvf & 0xFF) << 5) | (u8)region);
    __VISendI2CData(0xe0, buf3, 2);
    WaitMicroTime(2);

    // Step 5: Clear registers 0x0/0x0
    buf4[0] = 0;
    buf4[1] = 0;
    __VISendI2CData(0xe0, buf4, 2);
    WaitMicroTime(2);

    // Step 6: Set 0x71 with 3 bytes 0x8E
    buf5[0] = 0x71;
    buf5[1] = 0x8e;
    // buf5 is 2 bytes but we need 3... use buf6
    buf6[0] = 0x71;
    buf6[1] = 0x8e;
    buf6[2] = 0x8e;
    __VISendI2CData(0xe0, buf6, 3);
    WaitMicroTime(2);

    // Step 7: Send register 0x02, 0x07
    buf5[0] = 2;
    buf5[1] = 7;
    __VISendI2CData(0xe0, buf5, 2);
    WaitMicroTime(2);

    // Step 8: Clear wd0/wd1/wd2 and send 3-byte command 0x05
    __wd0 = 0;
    __wd1 = 0;
    __wd2 = 0;
    buf7[0] = 5;
    buf7[1] = 0;
    buf7[2] = 0;
    __VISendI2CData(0xe0, buf7, 3);
    WaitMicroTime(2);

    // Step 9: Check gp1/gp2/gp3/gp4 - if any nonzero, clear and update flag
    if (__gp1 != 0 || __gp2 != 0 || __gp3 != 0 || __gp4 != 0) {
        __gp1 = 0;
        Vdac_Flag_Changed = Vdac_Flag_Changed | 2;
        __gp2 = 0;
        __gp3 = 0;
        __gp4 = 0;
        Vdac_Flag_Changed = Vdac_Flag_Changed | 2;
    }

    // Step 10: Send WSS register 0x08
    {
        u8 v1 = (u8)(__gp1 & 0xF);
        u8 v2 = (u8)(__gp3 & 0x7);
        buf7[0] = 8;
        buf7[1] = (u8)(((__gp2 & 0xF) << 4) | v1);
        buf7[2] = (u8)(((__gp4 & 0x7) << 3) | v2);
        __VISendI2CData(0xe0, buf7, 3);
        WaitMicroTime(2);
    }

    // Step 11: Check cc1/cc2/cc3/cc4 - if any nonzero, clear and update flag
    if (__cc1 != 0 || __cc2 != 0 || __cc3 != 0 || __cc4 != 0) {
        __cc1 = 0;
        Vdac_Flag_Changed = Vdac_Flag_Changed | 4;
        __cc2 = 0;
        __cc3 = 0;
        __cc4 = 0;
        Vdac_Flag_Changed = Vdac_Flag_Changed | 4;
    }

    // Step 12: Send CC register 0x7A
    buf8[0] = 0x7a;
    buf8[1] = (u8)(__cc1 & 0x7F);
    buf8[2] = (u8)(__cc2 & 0x7F);
    buf8[3] = (u8)(__cc3 & 0x7F);
    buf8[4] = (u8)(__cc4 & 0x7F);
    __VISendI2CData(0xe0, buf8, 5);
    WaitMicroTime(2);

    // Step 13: Send VIZeroACPType data (0x1B bytes)
    acpPtr = VIZeroACPType;
    buf9[0] = 0x40;
    for (i = 0; i < 0x1A; i++) {
        buf9[i + 1] = acpPtr[i];
    }
    __VISendI2CData(0xe0, buf9, 0x1b);
    WaitMicroTime(2);

    // Step 14: Handle level for RGB mode
    if (__level != 0) {
        __level = 0;
        Vdac_Flag_Changed = Vdac_Flag_Changed | 0x40;
        Vdac_Flag_Changed = Vdac_Flag_Changed | 0x40;
    }

    // Step 15: Send RGB overdrive
    if (Vdac_Flag_Region == 3) {
        buf1[0] = 0xa;
        buf1[1] = (u8)((__level << 1) | 1);
        __VISendI2CData(0xe0, buf1, 2);
        WaitMicroTime(2);
    } else {
        buf1[0] = 0xa;
        buf1[1] = 0;
        __VISendI2CData(0xe0, buf1, 2);
        WaitMicroTime(2);
    }

    // Step 16: Set filter register 0x03
    buf2[0] = 3;
    buf2[1] = 1;
    __VISendI2CData(0xe0, buf2, 2);
    WaitMicroTime(2);

    // Step 17: Set gamma (1.0)
    __VISetGammaImm(gammaSet + 0x154);

    // Step 18: Reset current 3in1 video mode
    __current_3in1_video_mode = 0;
}

// Data tables
u8 gammaSet[7 * 0x22] = {
    // gamma = 0 (identity/off - 0x22 bytes of zeros)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00,
    // gamma = 1
    0x00, 0x30, 0x03, 0x97, 0x3B, 0x49, 0x10, 0x1D,
    0x36, 0x58, 0x82, 0xB3, 0xEB, 0x00, 0x10, 0x00,
    0x10, 0x00, 0x10, 0x00, 0x10, 0x80, 0x1B, 0x80,
    0xEB, 0x00, 0x00, 0x00, 0x00, 0x28, 0x00, 0x5A,
    0x02, 0xDB,
    // gamma = 2
    0x0D, 0x8D, 0x30, 0x49, 0x10, 0x1D, 0x36, 0x58,
    0x82, 0xB3, 0xEB, 0x00, 0x10, 0x00, 0x10, 0x00,
    0x10, 0x40, 0x11, 0x00, 0x18, 0x80, 0x42, 0x00,
    0xEB, 0x00, 0x00, 0x00, 0x00, 0x7A, 0x02, 0x3C,
    0x07, 0x6D,
    // gamma = 3
    0x12, 0x9C, 0x27, 0x24, 0x10, 0x1D, 0x36, 0x58,
    0x82, 0xB3, 0xEB, 0x00, 0x10, 0x00, 0x10, 0x00,
    0x10, 0xC0, 0x15, 0x80, 0x29, 0x00, 0x62, 0x00,
    0xEB, 0x00, 0x00, 0x4E, 0x01, 0x99, 0x05, 0x2D,
    0x0B, 0x24,
    // gamma = 4
    0x14, 0x29, 0x20, 0xA4, 0x10, 0x1D, 0x36, 0x58,
    0x82, 0xB3, 0xEB, 0x00, 0x10, 0x00, 0x10, 0x40,
    0x12, 0xC0, 0x1D, 0xC0, 0x3B, 0x00, 0x78, 0xC0,
    0xEB, 0x00, 0x00, 0xEC, 0x03, 0xD7, 0x08, 0x00,
    0x0D, 0x9E,
    // gamma = 5
    0x14, 0x3E, 0x1B, 0xDB, 0x10, 0x1D, 0x36, 0x58,
    0x82, 0xB3, 0xEB, 0x00, 0x10, 0x00, 0x10, 0xC0,
    0x16, 0xC0, 0x27, 0xC0, 0x4B, 0x80, 0x89, 0x80,
    0xEB, 0x00, 0x02, 0x76, 0x06, 0x66, 0x0A, 0x96,
    0x0E, 0xF3,
    // gamma = 6 (index 0x154 / 0x22 = 6 - this is the "1.0" gamma)
    0x13, 0xAC, 0x18, 0x49, 0x10, 0x1D, 0x36, 0x58,
    0x82, 0xB3, 0xEB, 0x00, 0x10, 0x00, 0x12, 0x00,
    0x1C, 0x00, 0x32, 0x80, 0x59, 0xC0, 0x96, 0x00,
    0xEB, 0x00, 0x04, 0xEC, 0x08, 0xF5, 0x0C, 0x96,
    0x0F, 0xCF,
};

u8 VINtscACPType1[0x1A] = {
    0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x1B, 0x1B, 0x24, 0x07, 0xF8, 0x00, 0x00,
    0x0F, 0x0F, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00,
};

u8 VINtscACPType2[0x1A] = {
    0x3E, 0x1D, 0x11, 0x25, 0x11, 0x01, 0x07, 0x00,
    0x1B, 0x1B, 0x24, 0x07, 0xF8, 0x00, 0x00, 0x0F,
    0x0F, 0x60, 0x01, 0x0A, 0x00, 0x05, 0x04, 0x03,
    0xFF, 0x00,
};

u8 VINtscACPType3[0x1A] = {
    0x3E, 0x17, 0x15, 0x21, 0x15, 0x05, 0x05, 0x02,
    0x1B, 0x1B, 0x24, 0x07, 0xF8, 0x00, 0x00, 0x0F,
    0x0F, 0x60, 0x01, 0x0A, 0x00, 0x05, 0x04, 0x03,
    0xFF, 0x00,
};

u8 VIPalACPType1[0x1A] = {
    0x36, 0x1A, 0x22, 0x2A, 0x22, 0x05, 0x02, 0x00,
    0x1C, 0x3D, 0x14, 0x03, 0xFE, 0x01, 0x54, 0xFE,
    0x7E, 0x60, 0x00, 0x08, 0x00, 0x04, 0x07, 0x01,
    0x55, 0x01,
};

u8 VIPalACPType2[0x1A] = {
    0x36, 0x1A, 0x22, 0x2A, 0x22, 0x05, 0x02, 0x00,
    0x1C, 0x3D, 0x14, 0x03, 0xFE, 0x01, 0x54, 0xFE,
    0x7E, 0x60, 0x00, 0x08, 0x00, 0x04, 0x07, 0x01,
    0x55, 0x01,
};

u8 VIPalACPType3[0x1A] = {
    0x36, 0x1A, 0x22, 0x2A, 0x22, 0x05, 0x02, 0x00,
    0x1C, 0x3D, 0x14, 0x03, 0xFE, 0x01, 0x54, 0xFE,
    0x7E, 0x60, 0x00, 0x08, 0x00, 0x04, 0x07, 0x01,
    0x55, 0x01,
};

u8 VIEurgb60ACPType1[0x1A] = {
    0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x1B, 0x1B, 0x24, 0x07, 0xF8, 0x00, 0x00,
    0x1E, 0x1E, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x01,
};

u8 VIEurgb60ACPType2[0x1A] = {
    0x36, 0x1D, 0x11, 0x25, 0x11, 0x01, 0x07, 0x00,
    0x1B, 0x1B, 0x24, 0x07, 0xF8, 0x00, 0x00, 0x1E,
    0x1E, 0x60, 0x01, 0x0A, 0x00, 0x05, 0x04, 0x03,
    0xFF, 0x01,
};

u8 VIEurgb60ACPType3[0x1A] = {
    0x36, 0x17, 0x15, 0x21, 0x15, 0x05, 0x05, 0x02,
    0x1B, 0x1B, 0x24, 0x07, 0xF8, 0x00, 0x00, 0x1E,
    0x1E, 0x60, 0x01, 0x0A, 0x00, 0x05, 0x04, 0x03,
    0xFF, 0x01,
};

u8 VIMpalACPType1[0x1A] = {
    0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x1B, 0x1B, 0x24, 0x07, 0xF8, 0x00, 0x00,
    0x0F, 0x0F, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00,
};

u8 VIMpalACPType2[0x1A] = {
    0x36, 0x1D, 0x11, 0x25, 0x11, 0x01, 0x07, 0x00,
    0x1B, 0x1B, 0x24, 0x07, 0xF8, 0x00, 0x00, 0x0F,
    0x0F, 0x60, 0x01, 0x0A, 0x00, 0x05, 0x04, 0x03,
    0xFF, 0x00,
};

u8 VIMpalACPType3[0x1A] = {
    0x36, 0x17, 0x15, 0x21, 0x15, 0x05, 0x05, 0x02,
    0x1B, 0x1B, 0x24, 0x07, 0xF8, 0x00, 0x00, 0x0F,
    0x0F, 0x60, 0x01, 0x0A, 0x00, 0x05, 0x04, 0x03,
    0xFF, 0x00,
};

u8 VIProgressiveACPType[0x1A] = {
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00,
};

// VIZeroACPType is in .bss (zero-initialized)
u8 VIZeroACPType[0x1A];
