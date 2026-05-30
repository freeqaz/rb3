#include "types.h"

s32 IsInitialized;
s32 vsync_timing_err_cnt;
s32 vsync_timing_test_flag;
s32 __VIDimming_All_Clear;
s32 THD_TIME_TO_DIMMING;
s32 NEW_TIME_TO_DIMMING;
s32 THD_TIME_TO_DVD_STOP;
s32 _gIdleCount_dimming;
s32 _gIdleCount_dvd;
s32 __VIDimmingState;
void *PositionCallback;
s16 displayOffsetH;
s16 displayOffsetV;
s32 changeMode;
u8 changed[8];
s32 shdwChangeMode;
u8 shdwChanged[8];
s32 FBSet;
s32 timingExtra;
s32 CurrBufAddr;
s32 NextBufAddr;
s32 CurrTvMode;
s32 CurrTiming;
s32 encoderType;
void *PostCB;
void *PreCB;
u8 retraceQueue[8];
s32 __VIDimmingFlag_SI_IDLE;
s32 __VIDimmingFlag_RF_IDLE;
s32 g_current_time_to_dim;
s32 __VIDVDStopFlag_Enable;
s32 __VIDimmingFlag_Enable;
s32 flushFlag3in1;
s32 flushFlag;
s32 retraceCount;
u8 shdwRegs[0x76];
u8 regs[0x76];
u8 HorVer[0x58];
u8 __VIDimmingFlag_DEV_IDLE[0x28];

static s32 OnShutdown(s32 type, u32 code) {
    (void)type; (void)code;
    return 0;
}

void __VIRetraceHandler(s16 type, void *context) {
    (void)type; (void)context;
}

void *VISetPreRetraceCallback(void *cb) {
    (void)cb;
    return 0;
}

void *VISetPostRetraceCallback(void *cb) {
    (void)cb;
    return 0;
}

static s32 getTiming(s32 mode) {
    (void)mode;
    return 0;
}

void __VIInit(void) {
}

void VIInit(void) {
}

void VIWaitForRetrace(void) {
}

static void setFbbRegs(void) {
}

static void setHorizontalRegs(void) {
}

static void setVerticalRegs(void) {
}

void VIConfigure(void *timing) {
    (void)timing;
}

void VIConfigurePan(u16 hbe, u16 hbs, u16 vbe, u16 vbs) {
    (void)hbe; (void)hbs; (void)vbe; (void)vbs;
}

void VIFlush(void) {
}

void VISetNextFrameBuffer(void *fb) {
    (void)fb;
}

void *VIGetCurrentFrameBuffer(void) {
    return 0;
}

void VISetBlack(s32 black) {
    (void)black;
}

u32 VIGetRetraceCount(void) {
    return 0;
}

u32 VIGetCurrentLine(void) {
    return 0;
}

s32 VIGetTvFormat(void) {
    return 0;
}

s32 VIGetScanMode(void) {
    return 0;
}

s32 VIGetDTVStatus(void) {
    return 0;
}

void __VIDisplayPositionToXY(s32 displayPos, u16 *x, u16 *y) {
    (void)displayPos; (void)x; (void)y;
}

void VIEnableDimming(s32 enable) {
    (void)enable;
}

void VISetTimeToDimming(u32 time) {
    (void)time;
}

void VIResetDimmingCount(void) {
}

void __VIResetRFIdle(void) {
}
