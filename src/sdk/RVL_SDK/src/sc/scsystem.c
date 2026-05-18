#include <revolution/sc/scsystem.h>
#include <revolution/OS.h>
#include <revolution/NAND.h>
#include "types.h"
#include <string.h>

static SCControl Control;
static u8 ConfBuf[0x4000];
static u8 ConfBufForFlush[0x4000];

static u8 BgJobStatus = FALSE;
static u8 IsDevKit = FALSE;
static s8 DirtyFlag;
static u8 Initialized = FALSE;
const char *__SCVersion =
    "<< RVL_SDK - SC \trelease build: Dec 11 2009 15:59:09 (0x4302_145) >>";

BOOL SCReloadConfFileAsync(u8 *, int, int);
static int ParseConfBuf(u8 *buf, u32 size);

void SCInit(void) {
    BOOL s = OSDisableInterrupts();
    if (Initialized) {
        OSRestoreInterrupts(s);
        return;
    }
    Initialized = TRUE;
    BgJobStatus = TRUE;
    OSRestoreInterrupts(s);
    OSRegisterVersion(__SCVersion);
    OSInitThreadQueue(&Control.threadQueue);
    if (OSGetConsoleType() & OS_CONSOLE_MASK_EMU)
        IsDevKit = TRUE;
    if (NANDInit() || SCReloadConfFileAsync(ConfBuf, 0x4000, 0))
        BgJobStatus = 2;
}

u8 SCCheckStatus(void) {
    u8 status;
    BOOL ints = OSDisableInterrupts();
    status = BgJobStatus;
    if (status == 3) {
        BgJobStatus = 1;
        OSRestoreInterrupts(ints);
        if (ParseConfBuf(Control.fileBuffers[1], Control.fileSizes[1]) == 0) {
            BOOL ints2 = OSDisableInterrupts();
            if (ConfBuf != Control.fileBuffers[1]) {
                memcpy(ConfBuf, Control.fileBuffers[1], 0x4000);
            }
            DirtyFlag = 0;
            OSRestoreInterrupts(ints2);
        } else {
            BOOL ints2 = OSDisableInterrupts();
            u8 *buf = Control.fileBuffers[1];
            u32 bufSize = 0x4000;
            memset(buf, 0, bufSize);
            if (bufSize > 0xc) {
                memcpy(buf, "SCv0", 4);
                memcpy(buf + 0x3ffc, "SCed", 4);
                *(u16 *)(buf + 6) = 8;
            }
            DirtyFlag = 0;
            OSRestoreInterrupts(ints2);
        }
        status = 0;
        BgJobStatus = 0;
    } else {
        OSRestoreInterrupts(ints);
    }
    return status;
}
