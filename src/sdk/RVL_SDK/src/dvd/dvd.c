#include "types.h"

s32 CommandInfoCounter;
s32 PauseFlag;
s32 PausingFlag;
s32 FatalErrorFlag;
s32 Canceling;
void *ResumeFromHere;
s32 NumInternalRetry;
s32 FirstTimeInBootrom;
s32 Breaking;
s32 WaitingForCoverOpen;
s32 WaitingForCoverClose;
s32 MotorStopped;
s32 ChangedDisc;
s32 PreparingCover;
s32 __DVDLayoutFormat;
s32 DVDInitialized;
s32 __BS2DVDLowIntType;
void *BootGameInfo;
void *PartInfo;
void *GameToc;
s32 __DVDNumTmdBytes;
u64 LastResetEnd;
s32 MotorState;
s32 ResetRequired;
s32 LastError;
s32 CancelLastError;
void *CancelCallback;
void *CurrCommand;
void *bootInfo;
void *IDShouldBe;
s32 executing;
void *LastState;

static void StampCommand(void *cmd, s32 val) {
    (void)cmd; (void)val;
}

static void defaultOptionalCommandChecker(void *cmd) {
    (void)cmd;
}

void DVDInit(void) {
}

static void stateReadingFST(void *cmd) {
    (void)cmd;
}

static void cbForStateReadingFST(s32 result, void *cmd) {
    (void)result; (void)cmd;
}

static void FatalAlarmHandler(void *alarm, void *ctx) {
    (void)alarm; (void)ctx;
}

static void cbForStateError(s32 result, void *cmd) {
    (void)result; (void)cmd;
}

static void cbForStoreErrorCode1(s32 result, void *cmd) {
    (void)result; (void)cmd;
}

static void cbForStoreErrorCode2(s32 result, void *cmd) {
    (void)result; (void)cmd;
}

static s32 CategorizeError(s32 err) {
    (void)err;
    return 0;
}

static void cbForStoreErrorCode3(s32 result, void *cmd) {
    (void)result; (void)cmd;
}

static void cbForStateGettingError(s32 result, void *cmd) {
    (void)result; (void)cmd;
}

static void cbForUnrecoveredError(s32 result, void *cmd) {
    (void)result; (void)cmd;
}

static void cbForUnrecoveredErrorRetry(s32 result, void *cmd) {
    (void)result; (void)cmd;
}

static void cbForStateGoToRetry(s32 result, void *cmd) {
    (void)result; (void)cmd;
}

static void stateCheckID(void *cmd) {
    (void)cmd;
}

static void cbForStateReadingTOC(s32 result, void *cmd) {
    (void)result; (void)cmd;
}

static void cbForStateReadingPartitionInfo(s32 result, void *cmd) {
    (void)result; (void)cmd;
}

static void cbForStateOpenPartition(s32 result, void *cmd) {
    (void)result; (void)cmd;
}

static void cbForStateOpenPartition2(s32 result, void *cmd) {
    (void)result; (void)cmd;
}

static void cbForStateCheckID1(s32 result, void *cmd) {
    (void)result; (void)cmd;
}

static void cbForStateCheckID2(s32 result, void *cmd) {
    (void)result; (void)cmd;
}

static void stateCoverClosed(void *cmd) {
    (void)cmd;
}

static void ResetAlarmHandler(void *alarm, void *ctx) {
    (void)alarm; (void)ctx;
}

static void cbForStateReset(s32 result, void *cmd) {
    (void)result; (void)cmd;
}

static void stateDownRotation(void *cmd) {
    (void)cmd;
}

static void cbForStateDownRotation(s32 result, void *cmd) {
    (void)result; (void)cmd;
}

static void stateCoverClosed_CMD(void *cmd) {
    (void)cmd;
}

static void cbForStateCoverClosed(s32 result, void *cmd) {
    (void)result; (void)cmd;
}

static void cbForPrepareCoverRegister(s32 result, void *cmd) {
    (void)result; (void)cmd;
}

static void CoverAlarmHandler(void *alarm, void *ctx) {
    (void)alarm; (void)ctx;
}

static void stateReady(void *cmd) {
    (void)cmd;
}

static void stateBusy(void *cmd) {
    (void)cmd;
}

static void cbForStateBusy(s32 result, void *cmd) {
    (void)result; (void)cmd;
}

s32 DVDReadAbsAsyncPrio(void *cmd, void *buf, s32 len, s32 offset, void (*cb)(s32, void*), s32 prio) {
    (void)cmd; (void)buf; (void)len; (void)offset; (void)cb; (void)prio;
    return 0;
}

s32 DVDInquiryAsync(void *cmd, void *info, void (*cb)(s32, void*)) {
    (void)cmd; (void)info; (void)cb;
    return 0;
}

s32 DVDGetCommandBlockStatus(void *cmd) {
    (void)cmd;
    return 0;
}

s32 DVDGetDriveStatus(void) {
    return 0;
}

void DVDSetAutoInvalidation(s32 enable) {
    (void)enable;
}

s32 DVDResume(void) {
    return 0;
}

s32 DVDCancelAsync(void *cmd, void (*cb)(s32, void*)) {
    (void)cmd; (void)cb;
    return 0;
}

s32 DVDCancel(void *cmd) {
    (void)cmd;
    return 0;
}

static void cbForCancelSync(s32 result, void *cmd) {
    (void)result; (void)cmd;
}

void *DVDGetCurrentDiskID(void) {
    return 0;
}

void __BS2DVDLowCallback(s32 result, void *cmd) {
    (void)result; (void)cmd;
}

u32 __DVDGetCoverStatus(void) {
    return 0;
}

s32 DVDIsDiskIdentified(void) {
    return 0;
}

s32 __DVDPrepareResetAsync(void (*cb)(s32, void*)) {
    (void)cb;
    return 0;
}

static void Callback(s32 result, void *cmd) {
    (void)result; (void)cmd;
}

s32 __DVDPrepareReset(void) {
    return 0;
}

s32 __DVDTestAlarm(void *alarm) {
    (void)alarm;
    return 0;
}

s32 __DVDStopMotorAsync(void (*cb)(s32, void*)) {
    (void)cb;
    return 0;
}

void __DVDRestartMotor(void) {
}
