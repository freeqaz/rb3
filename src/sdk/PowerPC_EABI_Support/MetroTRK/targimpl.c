#include "types.h"

u8 gTRKMemMap[0x10];
u8 gTRKExceptionStatus[0x10];
s16 TRK_saved_exceptionID;
u8 gTRKRestoreFlags[9];
u8 gTRKStepStatus[0x18];
u8 gTRKSaveState[0x94];
u8 TRKvalue128_temp[0x10];
u8 gTRKState[0xA4];
u8 gTRKCPUState[0x430];

s32 __TRK_get_MSR(void) {
    return 0;
}

void __TRK_set_MSR(s32 msr) {
    (void)msr;
}

s32 TRKValidMemory32(u32 addr) {
    (void)addr;
    return 1;
}

s32 TRK_ppc_memcpy(void *dst, const void *src, s32 n, s32 validate) {
    (void)dst; (void)src; (void)n; (void)validate;
    return 0;
}

s32 TRKTargetAccessMemory(void *data, void *proc, u32 addr, u32 size, s32 write) {
    (void)data; (void)proc; (void)addr; (void)size; (void)write;
    return 0;
}

s32 TRKTargetAccessDefault(void *data, void *proc, u32 addr, u32 size, s32 write) {
    (void)data; (void)proc; (void)addr; (void)size; (void)write;
    return 0;
}

s32 TRKTargetAccessFP(void *data, void *proc, u32 addr, u32 size, s32 write) {
    (void)data; (void)proc; (void)addr; (void)size; (void)write;
    return 0;
}

s32 TRKTargetAccessExtended1(void *data, void *proc, u32 addr, u32 size, s32 write) {
    (void)data; (void)proc; (void)addr; (void)size; (void)write;
    return 0;
}

s32 TRKTargetAccessExtended2(void *data, void *proc, u32 addr, u32 size, s32 write) {
    (void)data; (void)proc; (void)addr; (void)size; (void)write;
    return 0;
}

void TRK_InterruptHandler(void) {
}

void TRKExceptionHandler(void) {
}

s32 TRKPostInterruptEvent(void *event) {
    (void)event;
    return 0;
}

s32 TRKSwapAndGo(void) {
    return 0;
}

void TRKInterruptHandlerEnableInterrupts(void) {
}

s32 TRKTargetInterrupt(void *event) {
    (void)event;
    return 0;
}

s32 TRKTargetAddStopInfo(void *msg) {
    (void)msg;
    return 0;
}

s32 TRKTargetAddExceptionInfo(void *msg) {
    (void)msg;
    return 0;
}

s32 TRKTargetCheckStep(void) {
    return 0;
}

s32 TRKTargetSingleStep(void) {
    return 0;
}

s32 TRKTargetStepOutOfRange(void) {
    return 0;
}

u32 TRKTargetGetPC(void) {
    return 0;
}

s32 TRKTargetSupportRequest(void) {
    return 0;
}

s32 TRKTargetStopped(void) {
    return 0;
}

void TRKTargetSetStopped(s32 stopped) {
    (void)stopped;
}

s32 TRKTargetStop(void) {
    return 0;
}

s32 TRKPPCAccessSPR(void *data, u32 spr, s32 write) {
    (void)data; (void)spr; (void)write;
    return 0;
}

s32 TRKPPCAccessPairedSingleRegister(void *data, u32 reg, s32 write) {
    (void)data; (void)reg; (void)write;
    return 0;
}

void ReadFPSCR(void *dst) {
    (void)dst;
}

void WriteFPSCR(const void *src) {
    (void)src;
}

s32 TRKPPCAccessFPRegister(void *data, u32 reg, s32 write) {
    (void)data; (void)reg; (void)write;
    return 0;
}

s32 TRKPPCAccessSpecialReg(void *data, u32 reg, s32 write) {
    (void)data; (void)reg; (void)write;
    return 0;
}

void TRKTargetSetInputPendingPtr(void *ptr) {
    (void)ptr;
}

u32 ConvertAddress(u32 addr) {
    return addr;
}

s32 GetThreadInfo(void *info) {
    (void)info;
    return 0;
}
