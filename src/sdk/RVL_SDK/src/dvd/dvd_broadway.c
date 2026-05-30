#include "types.h"

u8 requestInProgress;
u8 callbackInProgress;
void *freeCommandBuf;
void *freeDvdContext;
u8 dvdContextsInited;
u8 DVDLowInitCalled;
s32 spinUpValue;
s32 readLength;
void *pathBuf;
void *diCommand;
u8 breakRequested;

static void doTransactionCallback(s32 result, void *ctx) {
    (void)result; (void)ctx;
}

static void doPrepareCoverRegisterCallback(s32 result, void *ctx) {
    (void)result; (void)ctx;
}

void DVDLowFinalize(void) {
}

void DVDLowInit(void) {
}

s32 DVDLowReadDiskID(void *diskID, void (*cb)(s32, void*), void *ctx) {
    (void)diskID; (void)cb; (void)ctx;
    return 0;
}

s32 DVDLowOpenPartition(u32 partOffset, void (*cb)(s32, void*), void *ctx) {
    (void)partOffset; (void)cb; (void)ctx;
    return 0;
}

s32 DVDLowOpenPartitionWithTmdAndTicketView(u32 partOffset, void *tmd, void *ticket, void (*cb)(s32, void*), void *ctx) {
    (void)partOffset; (void)tmd; (void)ticket; (void)cb; (void)ctx;
    return 0;
}

s32 DVDLowGetNoDiscBufferSizes(void *sizes) {
    (void)sizes;
    return 0;
}

s32 DVDLowGetNoDiscOpenPartitionParams(void *params) {
    (void)params;
    return 0;
}

s32 DVDLowClosePartition(void (*cb)(s32, void*), void *ctx) {
    (void)cb; (void)ctx;
    return 0;
}

s32 DVDLowUnencryptedRead(void *buf, u32 len, u32 offset, void (*cb)(s32, void*), void *ctx) {
    (void)buf; (void)len; (void)offset; (void)cb; (void)ctx;
    return 0;
}

s32 DVDLowStopMotor(void (*cb)(s32, void*), void *ctx) {
    (void)cb; (void)ctx;
    return 0;
}

s32 DVDLowInquiry(void *info, void (*cb)(s32, void*), void *ctx) {
    (void)info; (void)cb; (void)ctx;
    return 0;
}

s32 DVDLowRequestError(void (*cb)(s32, void*), void *ctx) {
    (void)cb; (void)ctx;
    return 0;
}

void DVDLowSetSpinupFlag(s32 flag) {
    (void)flag;
}

s32 DVDLowReset(void (*cb)(s32, void*), void *ctx) {
    (void)cb; (void)ctx;
    return 0;
}

s32 DVDLowAudioBufferConfig(s32 enable, s32 size, void (*cb)(s32, void*), void *ctx) {
    (void)enable; (void)size; (void)cb; (void)ctx;
    return 0;
}

s32 DVDLowReportKey(u32 subCmdId, u32 agid, void *data, void (*cb)(s32, void*), void *ctx) {
    (void)subCmdId; (void)agid; (void)data; (void)cb; (void)ctx;
    return 0;
}

s32 DVDLowSetMaximumRotation(s32 speed, void (*cb)(s32, void*), void *ctx) {
    (void)speed; (void)cb; (void)ctx;
    return 0;
}

s32 DVDLowRead(void *buf, u32 len, u32 offset, void (*cb)(s32, void*), void *ctx) {
    (void)buf; (void)len; (void)offset; (void)cb; (void)ctx;
    return 0;
}

s32 DVDLowSeek(u32 offset, void (*cb)(s32, void*), void *ctx) {
    (void)offset; (void)cb; (void)ctx;
    return 0;
}

u32 DVDLowGetCoverRegister(void) {
    return 0;
}

u32 DVDLowGetStatusRegister(void) {
    return 0;
}

u32 DVDLowGetControlRegister(void) {
    return 0;
}

s32 DVDLowPrepareCoverRegister(void (*cb)(s32, void*), void *ctx) {
    (void)cb; (void)ctx;
    return 0;
}

s32 DVDLowPrepareStatusRegister(void (*cb)(s32, void*), void *ctx) {
    (void)cb; (void)ctx;
    return 0;
}

s32 DVDLowPrepareControlRegister(void (*cb)(s32, void*), void *ctx) {
    (void)cb; (void)ctx;
    return 0;
}

u32 DVDLowGetImmBufferReg(void) {
    return 0;
}

void DVDLowUnmaskStatusInterrupts(void) {
}

void DVDLowMaskCoverInterrupt(void) {
}

s32 DVDLowClearCoverInterrupt(void (*cb)(s32, void*), void *ctx) {
    (void)cb; (void)ctx;
    return 0;
}

s32 __DVDLowTestAlarm(void *alarm) {
    (void)alarm;
    return 0;
}
