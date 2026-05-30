#include "types.h"

void *__sdCardStatus;
void *__sdReq;
void *__sdCmdBuffer;
void *__sdResp;
void *__sdResp2;
void *__sdReg;
void *__sdVect;

static void __sdCb(s32 result, void *ctx) {
    (void)result; (void)ctx;
}

s32 ISD_GetHCRegister(s32 reg, u32 *val) {
    (void)reg; (void)val;
    return 0;
}

s32 ISD_GetDeviceStatus(u32 *status) {
    (void)status;
    return 0;
}

static s32 sduCommandv(void *cmd, s32 argc, void **argv) {
    (void)cmd; (void)argc; (void)argv;
    return 0;
}

s32 sduCommand(void *cmd, ...) {
    (void)cmd;
    return 0;
}

s32 ISD_ResetDevice(void) {
    return 0;
}

s32 ISD_ReadBlock(u32 addr, void *buf) {
    (void)addr; (void)buf;
    return 0;
}

s32 ISD_WriteBlock(u32 addr, const void *buf) {
    (void)addr; (void)buf;
    return 0;
}

s32 ISD_ReadMultiBlock(u32 addr, u32 count, void *buf) {
    (void)addr; (void)count; (void)buf;
    return 0;
}

s32 ISD_WriteMultiBlock(u32 addr, u32 count, const void *buf) {
    (void)addr; (void)count; (void)buf;
    return 0;
}

s32 ISD_ReadMultiBlockAsync(u32 addr, u32 count, void *buf, void (*cb)(s32, void*), void *ctx) {
    (void)addr; (void)count; (void)buf; (void)cb; (void)ctx;
    return 0;
}

s32 ISD_WriteMultiBlockAsync(u32 addr, u32 count, const void *buf, void (*cb)(s32, void*), void *ctx) {
    (void)addr; (void)count; (void)buf; (void)cb; (void)ctx;
    return 0;
}

s32 ISD_MountCard(void) {
    return 0;
}

s32 ISD_UnmountCard(void) {
    return 0;
}

s32 ISD_InitCard(void) {
    return 0;
}

s32 ISD_ReadCardRegister(s32 reg, void *buf, s32 size) {
    (void)reg; (void)buf; (void)size;
    return 0;
}

static s32 sduDatabuswidth(s32 width) {
    (void)width;
    return 0;
}

s32 ISD_GetCardSize(u32 *size) {
    (void)size;
    return 0;
}

s32 ISD_RegisterDeviceIntrHandler(void (*cb)(u32, void*), void *ctx) {
    (void)cb; (void)ctx;
    return 0;
}

s32 ISD_UnregisterDeviceIntrHandler(void) {
    return 0;
}
