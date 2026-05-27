#include <revolution/sc/scapi_prdinfo.h>
#include <stdio.h>
#include <string.h>

/* ProductArea table: {area_id, name[4]} entries, terminated by {0xFF, ...} */
static const SCArea ProductAreaAndStringTbl[] = {
    { SC_AREA_JPN, "JPN" },
    { SC_AREA_USA, "USA" },
    { SC_AREA_EUR, "EUR" },
    { SC_AREA_AUS, "AUS" },
    { SC_AREA_BRA, "BRA" },
    { SC_AREA_TWN, "TWN" },
    { SC_AREA_TWN, "ROC" },
    { SC_AREA_KOR, "KOR" },
    { SC_AREA_HKG, "HKG" },
    { SC_AREA_ASI, "ASI" },
    { SC_AREA_LTN, "LTN" },
    { SC_AREA_SAF, "SAF" },
    { SC_AREA_CHN, "CHN" },
    { (SCProductArea)-1, "" }
};

/* ProductGameRegion table: {region_id, name[3]} entries, terminated by {0xFF, ...} */
static const SCRegion ProductGameRegionAndStringTbl[] = {
    { SC_PRD_GAME_REG_JP,  "JP" },
    { SC_PRD_GAME_REG_US,  "US" },
    { SC_PRD_GAME_REG_EU,  "EU" },
    { SC_PRD_GAME_REG_KR,  "KR" },
    { SC_PRD_GAME_REG_CN,  "CN" },
    { (SCProductGameRegion)-1, "" }
};

/* Static buffer for product code */
static char SCGetProductCode_buf[6];

BOOL __SCF1(const char *type, char *buf, u32 sz) {
    u32 lfsr = (u32)(0x73b6 << 16) - 0x2406;
    u32 i = 0;
    u32 pos = 0;
    u32 found = 0;
    u32 count = 0;
    u32 ctr;

    for (ctr = 0x80; ctr > 0; ctr--) {
        /* First iteration */
        {
            u8 key = *(volatile u8 *)(0x80000000 + 0x3800 + i);
            if (key != 0) {
                u8 c = (u8)(s8)type[pos];
                u8 decrypted = (u8)(key ^ lfsr);
                if ((s8)c == 0) {
                    if (decrypted == '=') {
                        found = 1;
                        goto search_done;
                    }
                } else {
                    u32 next = pos + 1;
                    u32 xored = (u8)((u8)c ^ decrypted) & 0xdf;
                    u32 eq = (u32)((u32)(xored == 0));
                    u32 mask = (u32)(-(s32)eq);
                    pos = next & mask;
                }
            }
        }
        i++;
        lfsr = (lfsr >> 31) | (lfsr << 1);

        /* Second iteration */
        {
            u8 key = *(volatile u8 *)(0x80000000 + 0x3800 + i);
            if (key != 0) {
                u8 c = (u8)(s8)type[pos];
                u8 decrypted = (u8)(key ^ lfsr);
                if ((s8)c == 0) {
                    if (decrypted == '=') {
                        found = 1;
                        goto search_done;
                    }
                } else {
                    u32 next = pos + 1;
                    u32 xored = (u8)((u8)c ^ decrypted) & 0xdf;
                    u32 eq = (u32)((u32)(xored == 0));
                    u32 mask = (u32)(-(s32)eq);
                    pos = next & mask;
                }
            }
        }
        lfsr = (lfsr >> 31) | (lfsr << 1);
        i++;
    }

search_done:
    if (!found) {
        return FALSE;
    }

    i++;

    for (; i < 0x100 && count < sz; i++) {
        u8 key = *(volatile u8 *)(0x80000000 + 0x3800 + i);
        lfsr = (lfsr >> 31) | (lfsr << 1);
        if (key == 0) {
            break;
        }
        u8 decrypted = (u8)(key ^ lfsr);
        if (decrypted == '\r' || decrypted == '\n') {
            decrypted = 0;
        }
        *buf = decrypted;
        count++;
        buf++;
        if (decrypted == 0) {
            return TRUE;
        }
    }

    return FALSE;
}

BOOL SCGetProductAreaString(char *buf, u32 sz) {
    return __SCF1("AREA", buf, sz);
}

SCProductArea SCGetProductArea(void) {
    char str[4];
    const SCArea *p = ProductAreaAndStringTbl;
    if (__SCF1("AREA", str, 4)) {
        while ((s8)p->area != -1) {
            if (strcmp(p->name, str) == 0) {
                return p->area;
            }
            p++;
        }
    }
    return (SCProductArea)-1;
}

const char *SCGetProductCode(void) {
    if (!__SCF1("CODE", SCGetProductCode_buf, 6)) {
        return NULL;
    }
    return SCGetProductCode_buf;
}

BOOL SCGetProductSNString(char *buf, u32 sz) {
    return __SCF1("SERNO", buf, sz);
}

BOOL SCGetProductSN(u32 *sn) {
    char str[11];
    if (__SCF1("SERNO", str, 11)) {
        if (sscanf(str, "%u", sn) == 1) {
            return TRUE;
        }
    }
    return FALSE;
}

SCProductGameRegion SCGetProductGameRegion(void) {
    char str[3];
    const SCRegion *p = ProductGameRegionAndStringTbl;
    if (__SCF1("GAME", str, 3)) {
        while ((s8)p->region != -1) {
            if (strcmp(p->name, str) == 0) {
                return p->region;
            }
            p++;
        }
    }
    return SC_PRD_GAME_REG_NULL;
}
