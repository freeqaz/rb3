/*
 * AX Voice Parameter Block (VPB) management
 * RVL_SDK AX audio subsystem
 *
 * Memory layout for N=96 voices:
 *   __s_AXPB  [N * 0x140] — hardware-visible parameter blocks (DMA'd to DSP)
 *   __s_AXITD [N * 0x40]  — ITD (inter-aural time delay) buffers
 *   __s_AXVPB [N * 0x168] — software VPB structs (includes embedded PB copy)
 *
 * AXVPB offsets (from vpb base, verified against target asm):
 *   +0x00  next / +0x04 prev / +0x08 nextCb
 *   +0x0C  priority / +0x10 callback / +0x14 userContext
 *   +0x18  index / +0x1C sync / +0x20 depop / +0x24 itdBuffer
 *   +0x28  embedded PB (0x140 bytes):
 *     pb+0x00 = vpb+0x28  nextHi, nextLo
 *     pb+0x04 = vpb+0x2C  currHi, currLo
 *     pb+0x08 = vpb+0x30  srcSelect, coefSelect
 *     pb+0x0C = vpb+0x34  mixerCtrl
 *     pb+0x10 = vpb+0x38  state
 *     pb+0x12 = vpb+0x3A  type
 *     pb+0x14 = vpb+0x3C  mix[0..23]  (0x30 bytes)
 *     pb+0x44 = vpb+0x6C  itd[0..6]   (0x0E bytes: flag,bufH,bufL,shL,shR,tgtShL,tgtShR)
 *     pb+0x52 = vpb+0x7A  dpop[0..11] (0x18 bytes)
 *     pb+0x6A = vpb+0x92  ve[0..1]    (0x04 bytes: currentVolume, currentDelta)
 *     pb+0x6E = vpb+0x96  addr[0..7]  (0x10 bytes)
 *     pb+0x7E = vpb+0xA6  adpcm[0..9] (0x28 bytes, stored as 10 u32s)
 *     pb+0xA6 = vpb+0xCE  src[0..6]   (0x0E bytes)
 *     pb+0xB4 = vpb+0xDC  adpcmLoop[0..2] (0x06 bytes)
 *     pb+0xBA = vpb+0xE2  lpf[0..3]   (0x08 bytes: on,yn1,a0,b0)
 *     pb+0xC2 = vpb+0xEA  biquad[0..9](0x14 bytes: on,xn1,xn2,yn1,yn2,b0,b1,b2,a1,a2)
 *     pb+0xD6 = vpb+0xFE  remote
 *     pb+0xD8 = vpb+0x100 rmtMixerCtrl
 *     pb+0xDA = vpb+0x102 rmtMix[0..15]  (0x20 bytes)
 *     pb+0xFA = vpb+0x122 rmtDpop[0..7]  (0x10 bytes)
 *     pb+0x10A= vpb+0x132 rmtSrc[0..4]   (0x0A bytes)
 *     pb+0x114= vpb+0x13C rmtIIR[0..13]  (0x1C bytes)
 *     pb+0x130= vpb+0x158 pad[4]          (0x08 bytes)
 */

#include <revolution/AX.h>
#include <revolution/OS.h>

/* ------------------------------------------------------------------ */
/* AXVPB struct (exact layout matching target binary)                  */
/* ------------------------------------------------------------------ */

typedef struct AXVPB_s AXVPB;
struct AXVPB_s {
    AXVPB   *next;          /* +0x00 */
    AXVPB   *prev;          /* +0x04 */
    AXVPB   *nextCb;        /* +0x08 */
    u32      priority;      /* +0x0C */
    void   (*callback)(AXVPB *); /* +0x10 */
    u32      userContext;   /* +0x14 */
    u32      index;         /* +0x18 */
    u32      sync;          /* +0x1C */
    u32      depop;         /* +0x20 */
    void    *itdBuffer;     /* +0x24 */
    /* --- embedded PB (0x140 bytes, pb+0x00 = vpb+0x28) --- */
    u16      pbNextHi;      /* +0x28 = pb+0x00 */
    u16      pbNextLo;      /* +0x2A = pb+0x02 */
    u16      pbCurrHi;      /* +0x2C = pb+0x04 */
    u16      pbCurrLo;      /* +0x2E = pb+0x06 */
    u16      pbSrcSel;      /* +0x30 = pb+0x08 */
    u16      pbCoefSel;     /* +0x32 = pb+0x0A */
    u32      pbMixerCtrl;   /* +0x34 = pb+0x0C */
    u16      pbState;       /* +0x38 = pb+0x10 */
    u16      pbType;        /* +0x3A = pb+0x12 */
    u16      pbMix[24];     /* +0x3C = pb+0x14  (0x30 bytes) */
    u16      pbItd[7];      /* +0x6C = pb+0x44  (0x0E bytes) */
    u16      pbDpop[12];    /* +0x7A = pb+0x52  (0x18 bytes) */
    u16      pbVe[2];       /* +0x92 = pb+0x6A  (0x04 bytes) */
    u16      pbAddr[8];     /* +0x96 = pb+0x6E  (0x10 bytes) */
    u16      pbAdpcm[20];   /* +0xA6 = pb+0x7E  (0x28 bytes, as u16 to avoid 4B alignment pad) */
    u16      pbSrc[7];      /* +0xCE = pb+0xA6  (0x0E bytes) */
    u16      pbAdpcmLoop[3];/* +0xDC = pb+0xB4  (0x06 bytes) */
    u16      pbLpf[4];      /* +0xE2 = pb+0xBA  (0x08 bytes) — lpf.on at [0] */
    u16      pbBiquad[10];  /* +0xEA = pb+0xC2  (0x14 bytes) — biquad.on at [0] */
    u16      pbRemote;      /* +0xFE = pb+0xD6 */
    u16      pbRmtMixCtrl;  /* +0x100= pb+0xD8 */
    u16      pbRmtMix[16];  /* +0x102= pb+0xDA  (0x20 bytes) */
    u16      pbRmtDpop[8];  /* +0x122= pb+0xFA  (0x10 bytes) */
    u16      pbRmtSrc[5];   /* +0x132= pb+0x10A (0x0A bytes) */
    u16      pbRmtIIR[14];  /* +0x13C= pb+0x114 (0x1C bytes) */
    u16      pbPad[4];      /* +0x158= pb+0x130 (0x08 bytes) */
    /* total = 0x160, but we need 0x168 = 0x28 + 0x140 */
    /* 0x158 + 0x08 = 0x160... need 8 more bytes */
    /* Actually: 14*2=0x1C for pbRmtIIR ends at 0x13C+0x1C=0x158 */
    /* pad from 0x158 to 0x160 = 4 more u16s (done above) */
    /* Total so far = 0x160, need 0x168 — 8 more bytes = 4 u16s */
    u16      pbPad2[4];     /* +0x160 */
};

/* ------------------------------------------------------------------ */
/* SDA globals (.sbss)                                                  */
/* ------------------------------------------------------------------ */

void  *__AXPB;          /* pointer to hardware PBs          */
void  *__AXITD;         /* pointer to ITD buffers           */
AXVPB *__AXVPB;         /* pointer to software VPBs         */
u32    __AXMaxVoices;   /* maximum voice count              */
u32    __AXNumVoices;   /* voices serviced this frame       */
u32    __AXRecDspCycles;/* recorded DSP cycle total         */
u32    __AXMaxDspCycles;/* DSP cycle budget                 */

/* ------------------------------------------------------------------ */
/* Static backing arrays (.bss)                                         */
/* ------------------------------------------------------------------ */

static u8 __s_AXPB [0x60 * 0x140]; /* 96 * 320 = 0x7800 bytes */
static u8 __s_AXITD[0x60 * 0x40];  /* 96 * 64  = 0x1800 bytes */
static u8 __s_AXVPB[0x60 * 0x168]; /* 96 * 360 = 0x8700 bytes */

/* ------------------------------------------------------------------ */
/* DSP cycle lookup tables (.data)                                      */
/* ------------------------------------------------------------------ */

u32 __AXMixCycles[32] = {
    0x00000002, 0x00000198, 0x00000198, 0x0000032A,
    0x0000057C, 0x0000057C, 0x0000057C, 0x0000057C,
    0x00000198, 0x00000330, 0x00000330, 0x000004C2,
    0x00000714, 0x00000714, 0x00000714, 0x00000714,
    0x000002C3, 0x0000045B, 0x0000045B, 0x000005ED,
    0x0000083F, 0x0000083F, 0x0000083F, 0x0000083F,
    0x000002C3, 0x0000045B, 0x0000045B, 0x000005ED,
    0x0000083F, 0x0000083F, 0x0000083F, 0x0000083F,
};

u32 __AXRmtMixCycles[4] = {
    0x00000004, 0x00000056, 0x00000097, 0x00000097,
};

/* ------------------------------------------------------------------ */
/* External symbols                                                    */
/* ------------------------------------------------------------------ */

extern AXVPB *__AXGetStackHead(int priority);
extern void   __AXPushFreeStack(AXVPB *vpb);
extern void   __AXPushCallbackStack(AXVPB *vpb);
extern void   __AXDepopVoice(void *pb);
extern u32    __AXGetCommandListCycles(void);

/* sdata2 float constant @3514 */
static const float kSrcRatioScale = 65536.0f;

/* ------------------------------------------------------------------ */
/* Simple getters                                                       */
/* ------------------------------------------------------------------ */

u32 __AXGetNumVoices(void) {
    return __AXNumVoices;
}

void *__AXGetPBs(void) {
    return __AXPB;
}

u32 AXGetMaxVoices(void) {
    return __AXMaxVoices;
}

/* ------------------------------------------------------------------ */
/* __AXSetPBDefault                                                     */
/*                                                                      */
/* Resets volatile PB fields. Target asm (0x40 bytes):                 */
/*   lis r4, 0x18a8; li r5, 0; addi r0, r4, 0x24                     */
/*   sth r5, 0x38(r3)   <- state                                      */
/*   sth r5, 0x6c(r3)   <- itd.flag                                   */
/*   stw r0, 0x1c(r3)   <- sync = 0x18A80024                         */
/*   sth r5, 0xe2(r3)   <- lpf.on                                     */
/*   sth r5, 0xea(r3)   <- biquad.on                                  */
/*   sth r5, 0xfe(r3)   <- remote                                     */
/*   sth r5, 0x13c(r3)  <- rmtIIR.lpf.on                             */
/*   sth r5, 0x132(r3)  <- rmtSrc[0]                                 */
/*   sth r5, 0x134(r3)                                                */
/*   sth r5, 0x136(r3)                                                */
/*   sth r5, 0x138(r3)                                                */
/*   sth r5, 0x13a(r3)  <- rmtSrc[4]                                 */
/* ------------------------------------------------------------------ */

void __AXSetPBDefault(AXVPB *vpb) {
    vpb->pbState      = 0;
    vpb->pbItd[0]     = 0;   /* itd.flag at vpb+0x6C */
    vpb->sync         = 0x18A80024u;
    vpb->pbLpf[0]     = 0;   /* lpf.on at vpb+0xE2 */
    vpb->pbBiquad[0]  = 0;   /* biquad.on at vpb+0xEA */
    vpb->pbRemote     = 0;
    vpb->pbRmtIIR[0]  = 0;   /* rmtIIR.lpf.on at vpb+0x13C */
    vpb->pbRmtSrc[0]  = 0;
    vpb->pbRmtSrc[1]  = 0;
    vpb->pbRmtSrc[2]  = 0;
    vpb->pbRmtSrc[3]  = 0;
    vpb->pbRmtSrc[4]  = 0;
}

/* ------------------------------------------------------------------ */
/* __AXServiceVPB                                                       */
/* ------------------------------------------------------------------ */

void __AXServiceVPB(AXVPB *vpb) {
    u8  *pb;
    u8  *src;
    u32  sync;

    __AXNumVoices++;

    pb   = (u8 *)__AXPB + vpb->index * 0x140;
    src  = (u8 *)vpb + 0x28;
    sync = vpb->sync;

    if (sync == 0) {
        *(u16 *)(src + 0x10) = *(u16 *)(pb + 0x10);
        *(u16 *)(src + 0x6a) = *(u16 *)(pb + 0x6a);
        *(u16 *)(src + 0x7a) = *(u16 *)(pb + 0x7a);
        *(u16 *)(src + 0x7c) = *(u16 *)(pb + 0x7c);
        return;
    }

    if (sync & 0x80000000u) {
        memcpy(pb, src, 0x140);
        return;
    }

    if (sync & 0x40000000u) {
        *(u16 *)(pb + 0x08) = *(u16 *)(src + 0x08);
        *(u16 *)(pb + 0x0a) = *(u16 *)(src + 0x0a);
    }

    if (sync & 0x20000000u) {
        *(u32 *)(pb + 0x0c) = *(u32 *)(src + 0x0c);
    }

    if (sync & 0x10000000u) {
        *(u16 *)(pb + 0x10) = *(u16 *)(src + 0x10);
    } else {
        *(u16 *)(src + 0x10) = *(u16 *)(pb + 0x10);
    }

    if (sync & 0x08000000u) {
        *(u16 *)(pb + 0x12) = *(u16 *)(src + 0x12);
    }

    /* Bits 25-26: mix block */
    if (sync & 0x02000000u) {
        *(u16 *)(pb + 0x4e) = *(u16 *)(src + 0x4e);
        *(u16 *)(pb + 0x50) = *(u16 *)(src + 0x50);
    } else if (sync & 0x04000000u) {
        memcpy(pb + 0x14, src + 0x14, 0x30);
        /* after bit26 path, also zero ITD buffer pointer */
        *(u32 *)((u8 *)vpb + 0x24)  = 0;  /* clear itdBuffer pointer */
        {
            u8 *pbITD = (u8 *)vpb + 0x24;
            u32 zero  = 0;
            *(u32 *)pbITD = zero;
            *(u32 *)(pbITD + 0x04) = zero;
            *(u32 *)(pbITD + 0x08) = zero;
            *(u32 *)(pbITD + 0x0c) = zero;
            *(u32 *)(pbITD + 0x10) = zero;
            *(u32 *)(pbITD + 0x14) = zero;
            *(u32 *)(pbITD + 0x18) = zero;
            *(u32 *)(pbITD + 0x1c) = zero;
            *(u32 *)(pbITD + 0x20) = zero;
            *(u32 *)(pbITD + 0x24) = zero;
            *(u32 *)(pbITD + 0x28) = zero;
            *(u32 *)(pbITD + 0x2c) = zero;
            *(u32 *)(pbITD + 0x30) = zero;
            *(u32 *)(pbITD + 0x34) = zero;
            *(u32 *)(pbITD + 0x38) = zero;
            *(u32 *)(pbITD + 0x3c) = zero;
        }
    }

    if (sync & 0x01000000u) {
        memcpy(pb + 0x52, src + 0x52, 0x18);
    }

    if (sync & 0x00400000u) {
        *(u16 *)(pb + 0x6a) = *(u16 *)(src + 0x6a);
        *(s16 *)(pb + 0x6c) = *(s16 *)(src + 0x6c);
    } else if (sync & 0x00800000u) {
        *(u16 *)(pb + 0x6a) = *(u16 *)(src + 0x6a);
        *(s16 *)(pb + 0x6c) = *(s16 *)(src + 0x6c);
    }

    if (sync & 0x001E0000u) {
        if (sync & 0x00100000u) {
            *(u16 *)(pb + 0x6e) = *(u16 *)(src + 0x6e);
        }
        if (sync & 0x00080000u) {
            *(u32 *)(pb + 0x72) = *(u32 *)(src + 0x72);
        }
        if (sync & 0x00040000u) {
            *(u32 *)(pb + 0x76) = *(u32 *)(src + 0x76);
        }
        if (sync & 0x00020000u) {
            *(u32 *)(pb + 0x7a) = *(u32 *)(src + 0x7a);
        } else {
            *(u32 *)(src + 0x7a) = *(u32 *)(pb + 0x7a);
        }
    } else if (sync & 0x00200000u) {
        *(u32 *)(pb + 0x6e) = *(u32 *)(src + 0x6e);
        *(u32 *)(pb + 0x72) = *(u32 *)(src + 0x72);
        *(u32 *)(pb + 0x76) = *(u32 *)(src + 0x76);
        *(u32 *)(pb + 0x7a) = *(u32 *)(src + 0x7a);
    } else {
        *(u16 *)(src + 0x7a) = *(u16 *)(pb + 0x7a);
        *(u16 *)(src + 0x7c) = *(u16 *)(pb + 0x7c);
    }

    if (sync & 0x00010000u) {
        *(u32 *)(pb + 0x7e) = *(u32 *)(src + 0x7e);
        *(u32 *)(pb + 0x82) = *(u32 *)(src + 0x82);
        *(u32 *)(pb + 0x86) = *(u32 *)(src + 0x86);
        *(u32 *)(pb + 0x8a) = *(u32 *)(src + 0x8a);
        *(u32 *)(pb + 0x8e) = *(u32 *)(src + 0x8e);
        *(u32 *)(pb + 0x92) = *(u32 *)(src + 0x92);
        *(u32 *)(pb + 0x96) = *(u32 *)(src + 0x96);
        *(u32 *)(pb + 0x9a) = *(u32 *)(src + 0x9a);
        *(u32 *)(pb + 0x9e) = *(u32 *)(src + 0x9e);
        *(u32 *)(pb + 0xa2) = *(u32 *)(src + 0xa2);
    }

    if (sync & 0x00004000u) {
        *(u16 *)(pb + 0xa6) = *(u16 *)(src + 0xa6);
        *(u16 *)(pb + 0xa8) = *(u16 *)(src + 0xa8);
    } else if (sync & 0x00008000u) {
        *(u16 *)(pb + 0xa6) = *(u16 *)(src + 0xa6);
        *(u16 *)(pb + 0xa8) = *(u16 *)(src + 0xa8);
        *(u16 *)(pb + 0xaa) = *(u16 *)(src + 0xaa);
        *(u16 *)(pb + 0xac) = *(u16 *)(src + 0xac);
        *(u16 *)(pb + 0xae) = *(u16 *)(src + 0xae);
        *(u16 *)(pb + 0xb0) = *(u16 *)(src + 0xb0);
        *(u16 *)(pb + 0xb2) = *(u16 *)(src + 0xb2);
    }

    if (sync & 0x00002000u) {
        *(u16 *)(pb + 0xb4) = *(u16 *)(src + 0xb4);
        *(u16 *)(pb + 0xb6) = *(u16 *)(src + 0xb6);
        *(u16 *)(pb + 0xb8) = *(u16 *)(src + 0xb8);
    }

    if (sync & 0x00000800u) {
        *(u16 *)(pb + 0xbe) = *(u16 *)(src + 0xbe);
        *(u16 *)(pb + 0xc0) = *(u16 *)(src + 0xc0);
    } else if (sync & 0x00001000u) {
        *(u16 *)(pb + 0xba) = *(u16 *)(src + 0xba);
        *(u16 *)(pb + 0xbc) = *(u16 *)(src + 0xbc);
        *(u16 *)(pb + 0xbe) = *(u16 *)(src + 0xbe);
        *(u16 *)(pb + 0xc0) = *(u16 *)(src + 0xc0);
    }

    if (sync & 0x00000200u) {
        *(u16 *)(pb + 0xcc) = *(u16 *)(src + 0xcc);
        *(u16 *)(pb + 0xce) = *(u16 *)(src + 0xce);
        *(u16 *)(pb + 0xd0) = *(u16 *)(src + 0xd0);
        *(u16 *)(pb + 0xd2) = *(u16 *)(src + 0xd2);
        *(u16 *)(pb + 0xd4) = *(u16 *)(src + 0xd4);
    } else if (sync & 0x00000400u) {
        *(u16 *)(pb + 0xc2) = *(u16 *)(src + 0xc2);
        *(u16 *)(pb + 0xc4) = *(u16 *)(src + 0xc4);
        *(u16 *)(pb + 0xc6) = *(u16 *)(src + 0xc6);
        *(u16 *)(pb + 0xc8) = *(u16 *)(src + 0xc8);
        *(u16 *)(pb + 0xca) = *(u16 *)(src + 0xca);
        *(u16 *)(pb + 0xcc) = *(u16 *)(src + 0xcc);
        *(u16 *)(pb + 0xce) = *(u16 *)(src + 0xce);
        *(u16 *)(pb + 0xd0) = *(u16 *)(src + 0xd0);
        *(u16 *)(pb + 0xd2) = *(u16 *)(src + 0xd2);
        *(u16 *)(pb + 0xd4) = *(u16 *)(src + 0xd4);
    }

    if (sync & 0x00000100u) {
        *(u16 *)(pb + 0xd6) = *(u16 *)(src + 0xd6);
    }

    if (sync & 0x00000080u) {
        *(u16 *)(pb + 0xd8) = *(u16 *)(src + 0xd8);
    }

    if (sync & 0x00000040u) {
        memcpy(pb + 0xda, src + 0xda, 0x20);
    }

    if (sync & 0x00000020u) {
        memcpy(pb + 0xfa, src + 0xfa, 0x10);
    }

    if (sync & 0x00000010u) {
        memcpy(pb + 0x10a, src + 0x10a, 0x0a);
    }

    if (sync & 0x00000004u) {
        *(u16 *)(pb + 0x118) = *(u16 *)(src + 0x118);
        *(u16 *)(pb + 0x11a) = *(u16 *)(src + 0x11a);
    } else if (sync & 0x00000002u) {
        *(u16 *)(pb + 0x11e) = *(u16 *)(src + 0x11e);
        *(u16 *)(pb + 0x120) = *(u16 *)(src + 0x120);
        *(u16 *)(pb + 0x122) = *(u16 *)(src + 0x122);
        *(u16 *)(pb + 0x124) = *(u16 *)(src + 0x124);
        *(u16 *)(pb + 0x126) = *(u16 *)(src + 0x126);
    } else if (sync & 0x00000008u) {
        *(u16 *)(pb + 0x114) = *(u16 *)(src + 0x114);
        *(u16 *)(pb + 0x116) = *(u16 *)(src + 0x116);
        *(u16 *)(pb + 0x118) = *(u16 *)(src + 0x118);
        *(u16 *)(pb + 0x11a) = *(u16 *)(src + 0x11a);
        *(u16 *)(pb + 0x11c) = *(u16 *)(src + 0x11c);
        *(u16 *)(pb + 0x11e) = *(u16 *)(src + 0x11e);
        *(u16 *)(pb + 0x120) = *(u16 *)(src + 0x120);
        *(u16 *)(pb + 0x122) = *(u16 *)(src + 0x122);
        *(u16 *)(pb + 0x124) = *(u16 *)(src + 0x124);
        *(u16 *)(pb + 0x126) = *(u16 *)(src + 0x126);
    }
}

/* ------------------------------------------------------------------ */
/* __AXSyncPBs — called each audio frame to service all active voices  */
/* ------------------------------------------------------------------ */

void __AXSyncPBs(void *param) {
    AXVPB *vpb;
    u32    cycles;
    int    prio;

    __AXNumVoices = 0;
    DCInvalidateRange(__AXPB, __AXMaxVoices * 0x140);
    DCInvalidateRange(__AXITD, __AXMaxVoices << 6);

    cycles = (u32)param + __AXMaxVoices * 0x258 + __AXGetCommandListCycles() + 0x20;

    prio = 0x1f;
    do {
        vpb = __AXGetStackHead(prio);
        while (vpb != NULL) {

            if (vpb->pbItd[0] == 1) {
                cycles += 0x81;
            }

            if (vpb->depop != 0) {
                __AXDepopVoice((u8 *)__AXPB + vpb->index * 0x140);
            }

            if (vpb->pbState == 1) {
                u32 mixCtrl = vpb->pbMixerCtrl;
                u32 mixIdx0, mixIdx1, mixIdx2, mixIdx3;
                u32 srcCycles;
                u32 ratio;

                cycles += 0x183;

                if (vpb->pbLpf[0] != 0) {
                    cycles += 0x135;
                }
                if (vpb->pbBiquad[0] != 0) {
                    cycles += 0x400;
                }
                if (vpb->pbItd[0] == 1) {
                    cycles += 0x1b;
                }

                ratio = ((u32)vpb->pbSrc[0] << 16) | vpb->pbSrc[1];

                if (vpb->pbSrcSel == 0) {
                    u32 tmp = (ratio << 9) + 0x10000 - 0x8000;
                    srcCycles = (tmp >> 16) + 0x619;
                } else if (vpb->pbSrcSel == 1) {
                    srcCycles = 0x25d;
                } else {
                    u32 tmp = (ratio << 9) + 0x10000 - 0x8000;
                    srcCycles = (tmp >> 16) + 0x5ba;
                }

                /* Extract 4 mix channel indices from mixerCtrl and look up cycle costs */
                mixIdx0 = ((mixCtrl >> 13) & 0x1f) << 2;   /* bits 15..19 */
                mixIdx1 = ((mixCtrl >>  3) & 0x1f) << 2;   /* bits 5..1: clrlslwi MB=27,b=2 */
                mixIdx2 = ((mixCtrl >>  8) & 0x1f) << 2;   /* bits 10..14 */
                mixIdx3 = ((mixCtrl >> 18) & 0x1f) << 2;   /* bits 20..24 */

                cycles = cycles + __AXMixCycles[mixIdx1>>2] + srcCycles;
                cycles += __AXMixCycles[mixIdx0>>2] + __AXMixCycles[mixIdx2>>2] + __AXMixCycles[mixIdx3>>2];

                if (vpb->pbRemote == 1) {
                    u16 rmtCtrl = vpb->pbRmtMixCtrl;
                    u32 ri0, ri1, ri2, ri3, ri4, ri5, ri6, ri7;
                    u16 iir;

                    cycles += 0x265;

                    iir = vpb->pbRmtIIR[0];
                    if (iir == 1) {
                        cycles += 0x76;
                    } else if (iir == 2) {
                        cycles += 0x342;
                    }

                    ri0 = ((rmtCtrl >> 14) & 3);
                    ri1 = ((rmtCtrl >>  0) & 3);
                    ri2 = ((rmtCtrl >>  2) & 3);
                    ri3 = ((rmtCtrl >>  4) & 3);
                    ri4 = ((rmtCtrl >>  6) & 3);
                    ri5 = ((rmtCtrl >>  8) & 3);
                    ri6 = ((rmtCtrl >> 10) & 3);
                    ri7 = ((rmtCtrl >> 12) & 3);

                    cycles += __AXRmtMixCycles[ri0] + __AXRmtMixCycles[ri1]
                            + __AXRmtMixCycles[ri2] + __AXRmtMixCycles[ri3]
                            + __AXRmtMixCycles[ri4] + __AXRmtMixCycles[ri5]
                            + __AXRmtMixCycles[ri6] + __AXRmtMixCycles[ri7];
                }

                if (__AXMaxDspCycles > cycles) {
                    __AXServiceVPB(vpb);
                } else {
                    /* Budget exceeded: voice done */
                    u8 *pb = (u8 *)__AXPB + vpb->index * 0x140;
                    if (*(u16 *)(pb + 0x10) == 1) {
                        __AXDepopVoice(pb);
                    }
                    vpb->pbState  = 0;
                    *(u16 *)(pb + 0x10) = 0;
                    __AXPushCallbackStack(vpb);
                }
            } else {
                __AXServiceVPB(vpb);
            }

            vpb->sync  = 0;
            vpb->depop = 0;
            vpb = vpb->next;
        }
    } while (--prio != 0);

    __AXRecDspCycles = cycles;

    vpb = __AXGetStackHead(0);
    while (vpb != NULL) {
        if (vpb->depop != 0) {
            __AXDepopVoice((u8 *)__AXPB + vpb->index * 0x140);
        }
        vpb->depop = 0;
        {
            u8 *pb = (u8 *)__AXPB + vpb->index * 0x140;
            *(u16 *)(pb + 0x10) = 0;
        }
        vpb = vpb->next;
    }

    DCFlushRange(__AXPB, __AXMaxVoices * 0x140);
    DCFlushRange(__AXITD, __AXMaxVoices << 6);
}

/* ------------------------------------------------------------------ */
/* __AXVPBInitCommon                                                    */
/* ------------------------------------------------------------------ */

static void __AXVPBInitCommon(void) {
    u32    i;
    u32    maxV;
    u32    total;
    u32   *p;
    u32    n8, rem, ctr;

    __AXRecDspCycles = 0;

    maxV = __AXMaxVoices;

    /* Compute __AXMaxDspCycles from the bus clock frequency */
    {
        u32 busClock = *(volatile u32 *)0x800000F8u;
        u32 magic    = 0x890FFD51u;
        u32 m        = (u32)(((u64)magic * (u64)busClock) >> 32);
        u32 d        = busClock - m;
        u32 half     = (d >> 1) + m;
        __AXMaxDspCycles = half >> 9;
    }

    /* --- zero __AXPB (maxV * 0x50 dwords) --- */
    total = maxV * 0x50;
    p     = (u32 *)__AXPB;
    if (total != 0) {
        n8  = total >> 3;
        ctr = n8;
        if (ctr != 0) {
            do {
                p[0] = 0; p[1] = 0; p[2] = 0; p[3] = 0;
                p[4] = 0; p[5] = 0; p[6] = 0; p[7] = 0;
                p += 8;
            } while (--ctr);
            rem = total & 7;
        } else {
            rem = total;
        }
        if (rem != 0) {
            ctr = rem;
            do { *p++ = 0; } while (--ctr);
        }
    }

    /* --- zero __AXITD (maxV * 0x10 dwords) --- */
    total = maxV << 4;
    p     = (u32 *)__AXITD;
    if (total != 0) {
        n8  = total >> 3;
        ctr = n8;
        if (ctr != 0) {
            do {
                p[0] = 0; p[1] = 0; p[2] = 0; p[3] = 0;
                p[4] = 0; p[5] = 0; p[6] = 0; p[7] = 0;
                p += 8;
            } while (--ctr);
            rem = total & 7;
        } else {
            rem = total;
        }
        if (rem != 0) {
            ctr = rem;
            do { *p++ = 0; } while (--ctr);
        }
    }

    /* --- zero __AXVPB (maxV * 0x5A dwords) --- */
    total = maxV * 0x5A;
    p     = (u32 *)__AXVPB;
    if (total != 0) {
        n8  = total >> 3;
        ctr = n8;
        if (ctr != 0) {
            do {
                p[0] = 0; p[1] = 0; p[2] = 0; p[3] = 0;
                p[4] = 0; p[5] = 0; p[6] = 0; p[7] = 0;
                p += 8;
            } while (--ctr);
            rem = total & 7;
        } else {
            rem = total;
        }
        if (rem != 0) {
            /* target has explicit nop before this loop (for __AXVPB only) */
            ctr = rem;
            do { *p++ = 0; } while (--ctr);
        }
    }

    /* --- initialise each VPB --- */
    {
        u32  pbOff  = 0;
        u32  itdOff = 0;
        u32  vpbOff = 0;
        u32  defSync = 0x18A80024u;
        u32  one  = 1;

        for (i = 0; i < maxV; i++) {
            AXVPB *v    = (AXVPB *)((u8 *)__AXVPB + vpbOff);
            u8    *pb   = (u8    *)__AXPB  + pbOff;
            u8    *itd  = (u8    *)__AXITD + itdOff;
            u8    *vpb8 = (u8 *)v + 0x28;

            v->index     = i;
            v->itdBuffer = itd;

            /* Reset volatile fields (same as __AXSetPBDefault) */
            v->pbState     = 0;
            v->pbItd[0]    = 0;   /* itd.flag */
            v->sync        = defSync;
            v->pbLpf[0]    = 0;   /* lpf.on */
            v->pbBiquad[0] = 0;   /* biquad.on */
            v->pbRemote    = 0;
            v->pbRmtIIR[0] = 0;
            v->pbRmtSrc[0] = 0;
            v->pbRmtSrc[1] = 0;
            v->pbRmtSrc[2] = 0;
            v->pbRmtSrc[3] = 0;
            v->pbRmtSrc[4] = 0;

            /* Set pbNext linked list of HW PBs */
            if (i == maxV - 1) {
                *(u16 *)(vpb8 + 0x02) = 0;
                *(u16 *)(vpb8 + 0x00) = 0;
                *(u16 *)(pb   + 0x02) = 0;
                *(u16 *)(pb   + 0x00) = 0;
            } else {
                u8  *nextPb = pb + 0x140;
                u32  addr   = (u32)nextPb;
                *(u16 *)(vpb8 + 0x00) = (u16)(addr >> 16);
                *(u16 *)(vpb8 + 0x02) = (u16)(addr & 0xFFFF);
                *(u16 *)(pb   + 0x00) = (u16)(addr >> 16);
                *(u16 *)(pb   + 0x02) = (u16)(addr & 0xFFFF);
            }

            /* pbCurr = this HW PB address */
            {
                u32 self = (u32)pb;
                *(u16 *)(vpb8 + 0x04) = (u16)(self >> 16);
                *(u16 *)(vpb8 + 0x06) = (u16)(self & 0xFFFF);
                *(u16 *)(pb   + 0x04) = (u16)(self >> 16);
                *(u16 *)(pb   + 0x06) = (u16)(self & 0xFFFF);
            }

            /* ITD buffer address into pb+0x46/0x48 (vpb+0x6E/0x70) */
            {
                u32 itdAddr = (u32)itd;
                *(u16 *)(vpb8 + 0x46) = (u16)(itdAddr >> 16);
                *(u16 *)(vpb8 + 0x48) = (u16)(itdAddr & 0xFFFF);
                *(u16 *)(pb   + 0x46) = (u16)(itdAddr >> 16);
                *(u16 *)(pb   + 0x48) = (u16)(itdAddr & 0xFFFF);
            }

            v->priority = one;
            __AXPushFreeStack(v);

            pbOff  += 0x140;
            itdOff += 0x40;
            vpbOff += 0x168;
        }
    }

    DCFlushRange(__AXPB, __AXMaxVoices * 0x140);
}

void __AXVPBInit(void) {
    __AXMaxVoices = 0x60;
    __AXPB  = (void *)__s_AXPB;
    __AXITD = (void *)__s_AXITD;
    __AXVPB = (AXVPB *)__s_AXVPB;
    __AXVPBInitCommon();
}

void __AXVPBInitSpecifyMem(u32 maxVoices, u32 mem) {
    u32 pbSize;
    u32 itdSize;
    u32 itdBase;

    if (maxVoices > 0x60) return;
    if (mem == 0)         return;
    if ((mem & 0x1F) == 0) {
        pbSize  = maxVoices * 0x140;
        itdSize = maxVoices << 6;
        itdBase = mem + pbSize;

        __AXMaxVoices = maxVoices;
        __AXITD = (void *)itdBase;
        __AXPB  = (void *)mem;
        __AXVPB = (AXVPB *)(itdBase + itdSize);
        __AXVPBInitCommon();
    }
}

void __AXVPBQuit(void) {
    __AXPB        = NULL;
    __AXITD       = NULL;
    __AXVPB       = NULL;
    __AXMaxVoices = 0;
}

/* ------------------------------------------------------------------ */
/* AX voice setter API                                                  */
/* ------------------------------------------------------------------ */

void AXSetVoiceSrcType(AXVPB *vpb, u32 type) {
    BOOL enabled = OSDisableInterrupts();

    switch (type) {
    case 0:
        vpb->pbSrcSel = 2;
        break;
    case 1:
        vpb->pbSrcSel = 1;
        break;
    case 2:
        vpb->pbSrcSel  = 0;
        vpb->pbCoefSel = 0;
        break;
    case 3:
        vpb->pbSrcSel  = 0;
        vpb->pbCoefSel = 1;
        break;
    case 4:
        vpb->pbSrcSel  = 0;
        vpb->pbCoefSel = 2;
        break;
    }

    vpb->sync |= 0x1u;
    OSRestoreInterrupts(enabled);
}

void AXSetVoiceState(AXVPB *vpb, u16 state) {
    BOOL enabled = OSDisableInterrupts();

    if (vpb->pbState == state) {
        OSRestoreInterrupts(enabled);
        return;
    }

    vpb->pbState  = state;
    vpb->sync    |= 0x4u;
    if (state == 0) {
        vpb->depop = 1;
    }
    OSRestoreInterrupts(enabled);
}

void AXSetVoiceType(AXVPB *vpb, u16 type) {
    BOOL enabled = OSDisableInterrupts();
    vpb->pbType = type;
    vpb->sync  |= 0x8u;
    OSRestoreInterrupts(enabled);
}

void AXSetVoiceAddr(AXVPB *vpb, void *addr) {
    u32 *a = (u32 *)addr;
    BOOL enabled = OSDisableInterrupts();

    /* Copy 4 dwords from addr struct: loopFlag+fmt, loopAddr, endAddr, curAddr */
    *(u32 *)&vpb->pbAddr[0] = a[0];
    *(u32 *)&vpb->pbAddr[2] = a[1];
    *(u32 *)&vpb->pbAddr[4] = a[2];
    *(u32 *)&vpb->pbAddr[6] = a[3];

    {
        int fmt = *(u16 *)((u8 *)addr + 0x02);
        switch (fmt) {
        case 0x0a: {
            /* PCM16: clear ADPCM coeff, set pcm16 loop step */
            u32 *pa = (u32 *)&vpb->pbAdpcm[0];
            u32 zero = 0;
            pa[0] = zero;
            pa[1] = zero;
            pa[2] = zero;
            pa[3] = zero;
            pa[4] = zero;
            pa[5] = zero;
            pa[6] = zero;
            pa[7] = zero;
            pa[8] = 0x08000000u;
            pa[9] = zero;
            break;
        }
        case 0x19: {
            /* PCM8: clear ADPCM coeff, set pcm8 loop step */
            u32 *pa = (u32 *)&vpb->pbAdpcm[0];
            u32 zero = 0;
            pa[0] = zero;
            pa[1] = zero;
            pa[2] = zero;
            pa[3] = zero;
            pa[4] = zero;
            pa[5] = zero;
            pa[6] = zero;
            pa[7] = zero;
            pa[8] = 0x01000000u;
            pa[9] = zero;
            break;
        }
        }
    }

    vpb->sync = (vpb->sync & ~0x00007800u) | 0x00008400u;
    OSRestoreInterrupts(enabled);
}

void AXSetVoiceAdpcm(AXVPB *vpb, void *adpcm) {
    u32 *a    = (u32 *)adpcm;
    u32 *pa   = (u32 *)&vpb->pbAdpcm[0];
    u32  sync;
    BOOL enabled = OSDisableInterrupts();

    pa[0] = a[0];
    sync  = vpb->sync;
    pa[1] = a[1];
    sync |= 0x00008000u;
    pa[2] = a[2];
    pa[3] = a[3];
    pa[4] = a[4];
    pa[5] = a[5];
    pa[6] = a[6];
    pa[7] = a[7];
    pa[8] = a[8];
    pa[9] = a[9];
    vpb->sync = sync;

    OSRestoreInterrupts(enabled);
}

void AXSetVoiceSrc(AXVPB *vpb, void *src) {
    u16 *s = (u16 *)src;
    u32  sync;
    BOOL enabled = OSDisableInterrupts();

    vpb->pbSrc[0] = s[0];  /* ratioHi */
    sync = vpb->sync;
    vpb->pbSrc[1] = s[1];  /* ratioLo */
    sync = (sync & ~0x00020000u) | 0x00010000u;
    vpb->pbSrc[2] = s[2];  /* currentAddressFrac */
    vpb->pbSrc[3] = s[3];  /* last_samples[0] */
    vpb->pbSrc[4] = s[4];  /* last_samples[1] */
    vpb->pbSrc[5] = s[5];  /* last_samples[2] */
    vpb->pbSrc[6] = s[6];  /* last_samples[3] */
    vpb->sync = sync;

    OSRestoreInterrupts(enabled);
}

void AXSetVoiceSrcRatio(AXVPB *vpb, f32 ratio) {
    extern u32 __cvt_fp2unsigned(f32 f);
    BOOL enabled = OSDisableInterrupts();
    u32  fixed;
    u32  hi;
    u32  sync;

    fixed = __cvt_fp2unsigned(kSrcRatioScale * ratio);
    sync  = vpb->sync;
    hi    = fixed >> 16;

    vpb->pbSrc[1] = (u16)fixed;  /* ratioLo */
    sync |= 0x00020000u;
    vpb->pbSrc[0] = (u16)hi;     /* ratioHi */
    vpb->sync = sync;

    OSRestoreInterrupts(enabled);
}

void AXSetVoiceAdpcmLoop(AXVPB *vpb, void *loop) {
    u16 *l = (u16 *)loop;
    u32  sync;
    BOOL enabled = OSDisableInterrupts();

    vpb->pbAdpcmLoop[0] = l[0];  /* loop_pred_scale */
    sync = vpb->sync;
    vpb->pbAdpcmLoop[1] = l[1];  /* loop_yn1 */
    sync |= 0x00040000u;
    vpb->pbAdpcmLoop[2] = l[2];  /* loop_yn2 */
    vpb->sync = sync;

    OSRestoreInterrupts(enabled);
}
