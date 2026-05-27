/**
 * NdevExi2A/exi2.c
 * Wii debugger EXI2 hardware access module
 */
#include <revolution/os/OSInterrupt.h>

#define EXI2_BASE    0xCD000000u
/* 0x6828: channel control register (clock/CS) */
/* 0x6834: EXI transfer control + busy bit */
/* 0x6838: EXI data register */

BOOL __EXI2Imm(void* buf, s32 len, s32 isWrite) {
    u32 data = 0;
    u8* ptr = (u8*)buf;
    s32 pos = 0;

    if (isWrite) {
        if (len > 0) {
            s32 rem = len - 8;
            if (len > 8) {
                s32 canLoop = 0;
                if ((len >= 0) && (len <= 0x7FFFFFFE)) {
                    canLoop = 1;
                }
                if (canLoop) {
                    u8* src = ptr;
                    u32 nChunks = (u32)(rem + 7) >> 3;
                    if (rem > 0) {
                        do {
                            data |= (u32)src[0] << ((3 - pos) * 8);
                            data |= (u32)src[1] << ((3 - (pos + 1)) * 8);
                            data |= (u32)src[2] << ((3 - (pos + 2)) * 8);
                            data |= (u32)src[3] << (pos * -8);
                            data |= (u32)src[4] << ((3 - (pos + 4)) * 8);
                            data |= (u32)src[5] << ((3 - (pos + 5)) * 8);
                            data |= (u32)src[6] << ((3 - (pos + 6)) * 8);
                            data |= (u32)src[7] << ((3 - (pos + 7)) * 8);
                            pos += 8;
                            src += 8;
                            nChunks--;
                        } while (nChunks != 0);
                    }
                }
            }
            u8* rsrc = ptr + pos;
            s32 cnt = len - pos;
            if (pos < len) {
                do {
                    data |= (u32)(*rsrc) << ((3 - pos) * 8);
                    rsrc++;
                    pos++;
                    cnt--;
                } while (cnt != 0);
            }
        }
        *(volatile u32*)0xCD006838 = data;
    }

    *(volatile s32*)0xCD006834 = (isWrite * 4) | 1 | ((len - 1) * 0x10);
    while (*(volatile s32*)0xCD006834 & 1) {}

    if (isWrite == 0) {
        data = *(volatile u32*)0xCD006838;
        s32 rpos = 0;
        if (len > 0) {
            s32 rem = len - 8;
            if (len > 8) {
                s32 canLoop = 0;
                if ((len >= 0) && (len <= 0x7FFFFFFE)) {
                    canLoop = 1;
                }
                if (canLoop) {
                    u32 nChunks = (u32)(rem + 7) >> 3;
                    if (rem > 0) {
                        do {
                            ptr[0] = (u8)(data >> ((3 - rpos) * 8));
                            ptr[1] = (u8)(data >> ((3 - (rpos + 1)) * 8));
                            ptr[2] = (u8)(data >> ((3 - (rpos + 2)) * 8));
                            ptr[3] = (u8)(data >> (rpos * -8));
                            ptr[4] = (u8)(data >> ((3 - (rpos + 4)) * 8));
                            ptr[5] = (u8)(data >> ((3 - (rpos + 5)) * 8));
                            ptr[6] = (u8)(data >> ((3 - (rpos + 6)) * 8));
                            ptr[7] = (u8)(data >> ((3 - (rpos + 7)) * 8));
                            rpos += 8;
                            ptr += 8;
                            nChunks--;
                        } while (nChunks != 0);
                    }
                }
            }
            s32 cnt = len - rpos;
            if (rpos < len) {
                do {
                    s32 shift = 3 - rpos;
                    rpos++;
                    *ptr = (u8)(data >> (shift * 8));
                    ptr++;
                    cnt--;
                } while (cnt != 0);
            }
        }
    }

    return 1;
}

void __DBEXIInit(void) {
    s32 spC;
    s32 sp8;

    __OSMaskInterrupts(0x18000);

    while (((u32)(*(volatile s32*)0xCD006834 & 1)) == 1u) {}

    *(volatile s32*)0xCD006828 = 0;

    spC = (s32)0xB4000000;
    sp8 = (s32)0xD4000000;

    *(volatile s32*)0xCD006828 = (*(volatile s32*)0xCD006828 & 0x405) | 0xC0;
    __EXI2Imm(&spC, 4, 1);

    while (*(volatile s32*)0xCD006834 & 1) {}

    __EXI2Imm(&sp8, 4, 1);

    while (*(volatile s32*)0xCD006834 & 1) {}

    *(volatile s32*)0xCD006828 = *(volatile s32*)0xCD006828 & 0x405;
}

BOOL __DBEXIReadReg(s32 cmd, void* buf, s32 size) {
    u32 spC = 0;
    s32 sp8 = cmd;
    s32 fail1;
    s32 failTotal;
    volatile s32* csr = (volatile s32*)0xCD006828;

    *csr = (*csr & 0x405) | 0xC0;
    fail1 = (__EXI2Imm(&sp8, 4, 1) == 0);

    while (*(volatile s32*)0xCD006834 & 1) {}

    failTotal = fail1 | (__EXI2Imm(&spC, 4, 0) == 0);

    while (*(volatile s32*)0xCD006834 & 1) {}

    *csr = *csr & 0x405;

    if (size == 2) {
        *((s16*)buf) = (s16)(((spC >> 8) & 0xFF00) | (spC >> 24));
        goto done;
    } else if (size >= 2) {
        goto do4byte;
    } else if (size >= 1) {
        *((s8*)buf) = (s8)(spC >> 24);
        goto done;
    }
do4byte:
    *((s32*)buf) = (s32)(((spC << 8) & 0xFF0000) | (spC << 24) | ((spC >> 8) & 0xFF00) | (spC >> 24));
done: ;

    return (failTotal == 0);
}

BOOL __DBEXIWriteReg(s32 cmd, const void* buf, s32 size) {
    u32 spC;
    s32 sp8 = cmd;
    s32 fail1;
    s32 failTotal;

    if (size == 2) {
        u16 val = *((const u16*)buf);
        spC = ((val << 8) & 0xFF0000) | (val << 24);
        goto dowrite;
    } else if (size >= 2) {
        goto do4byte;
    } else if (size >= 1) {
        spC = *((const u8*)buf) << 24;
        goto dowrite;
    }
do4byte:
    {
        u32 val = *((const u32*)buf);
        spC = ((val << 8) & 0xFF0000) | (val << 24) | ((val >> 8) & 0xFF00) | (val >> 24);
    }
dowrite: ;

    volatile s32* csr = (volatile s32*)0xCD006828;
    *csr = (*csr & 0x405) | 0xC0;
    fail1 = (__EXI2Imm(&sp8, 4, 1) == 0);

    while (*(volatile s32*)0xCD006834 & 1) {}

    failTotal = fail1 | (__EXI2Imm(&spC, 4, 1) == 0);

    while (*(volatile s32*)0xCD006834 & 1) {}

    *csr = *csr & 0x405;

    return (failTotal == 0);
}

BOOL __DBEXIReadRam(s32 addr, s32* buf, s32 size) {
    volatile s32* csr = (volatile s32*)0xCD006828;
    s32 spC;
    s32 sp8 = addr;
    s32 fail;
    s32* dst = buf;
    s32 rem = size;

    *csr = (*csr & 0x405) | 0xC0;
    fail = (__EXI2Imm(&sp8, 4, 1) == 0);

    while (*(volatile s32*)0xCD006834 & 1) {}

    while (rem > 0) {
        fail |= (__EXI2Imm(&spC, 4, 0) == 0);

        while (*(volatile s32*)0xCD006834 & 1) {}

        rem -= 4;
        *dst = spC;
        dst++;
    }

    *csr = *csr & 0x405;

    return (fail == 0);
}

BOOL __DBEXIWriteRam(s32 addr, s32* buf, s32 size) {
    volatile s32* csr = (volatile s32*)0xCD006828;
    s32 spC;
    s32 sp8 = addr;
    s32 fail;
    s32* src = buf;
    s32 rem = size;

    *csr = (*csr & 0x405) | 0xC0;
    fail = (__EXI2Imm(&sp8, 4, 1) == 0);

    while (*(volatile s32*)0xCD006834 & 1) {}

    while (rem > 0) {
        spC = *src;
        src++;
        fail |= (__EXI2Imm(&spC, 4, 1) == 0);

        while (*(volatile s32*)0xCD006834 & 1) {}

        rem -= 4;
    }

    *csr = *csr & 0x405;

    return (fail == 0);
}
