#include "types.h"

s32 __AXAuxCpuReadWritePosition;
s32 __AXAuxDspReadPosition;
s32 __AXAuxDspWritePosition;
s32 __AXAuxCDspRead;
s32 __AXAuxCDspWrite;
s32 __AXAuxBDspRead;
s32 __AXAuxBDspWrite;
s32 __AXAuxADspRead;
s32 __AXAuxADspWrite;
void *__AXContextAuxC;
void *__AXContextAuxB;
void *__AXContextAuxA;
void *__AXCallbackAuxC;
void *__AXCallbackAuxB;
void *__AXCallbackAuxA;
u8 __clearAuxC;
u8 __clearAuxB;
u8 __clearAuxA;
s16 __AXBufferAuxA[0x1200 / 2];
s16 __AXBufferAuxB[0x1200 / 2];
s16 __AXBufferAuxC[0xD80 / 2];

void __AXAuxInit(void) {
}

void __AXAuxQuit(void) {
}

void *__AXGetAuxAInput(void) {
    return 0;
}

void *__AXGetAuxAOutput(void) {
    return 0;
}

void *__AXGetAuxAInputDpl2(void) {
    return 0;
}

void *__AXGetAuxAOutputDpl2R(void) {
    return 0;
}

void *__AXGetAuxAOutputDpl2Ls(void) {
    return 0;
}

void *__AXGetAuxAOutputDpl2Rs(void) {
    return 0;
}

void *__AXGetAuxBInput(void) {
    return 0;
}

void *__AXGetAuxBOutput(void) {
    return 0;
}

void *__AXGetAuxBInputDpl2(void) {
    return 0;
}

void *__AXGetAuxBOutputDpl2R(void) {
    return 0;
}

void *__AXGetAuxBOutputDpl2Ls(void) {
    return 0;
}

void *__AXGetAuxBOutputDpl2Rs(void) {
    return 0;
}

void *__AXGetAuxCInput(void) {
    return 0;
}

void *__AXGetAuxCOutput(void) {
    return 0;
}

void __AXProcessAux(void) {
}

void AXRegisterAuxACallback(void *cb, void *ctx) {
    (void)cb; (void)ctx;
}

void AXRegisterAuxBCallback(void *cb, void *ctx) {
    (void)cb; (void)ctx;
}

void AXRegisterAuxCCallback(void *cb, void *ctx) {
    (void)cb; (void)ctx;
}

void *AXGetAuxACallback(void *ctx) {
    (void)ctx;
    return 0;
}
