/**
 * NdevExi2A/DebuggerDriver.c
 * Wii debugger EXI2 communications driver - DebuggerDriver module
 */
#include <revolution/os/OSInterrupt.h>
#include <revolution/os/OSContext.h>

// External declarations from exi2.c
void __DBEXIInit(void);
BOOL __DBEXIReadReg(u32 cmd, void* buf, s32 size);

// Static sbss variables (SDA)
void (*__DBMtrCallback)(s32);
void (*__DBDbgCallback)(s32, OSContext*);
u8 __DBEXIInputFlag;
u32 __DBRecvMail;
u32 __DBRecvDataSize;


// Forward declarations from exi2.c
extern BOOL __DBEXIReadRam(u32 addr, void* buf, s32 size);
extern BOOL __DBEXIWriteRam(u32 addr, const void* buf, s32 size);
extern BOOL __DBEXIWriteReg(u32 cmd, const void* buf, s32 size);

static void __DBMtrHandler(s32 intr, OSContext* ctx) {
    void (*cb)(s32) = __DBMtrCallback;
    __DBEXIInputFlag = 1;
    if (cb == NULL) {
        return;
    }
    cb(0);
}

static void __DBIntrHandler(s32 intr, OSContext* ctx) {
    u32 val = 0x1000;
    *(volatile u32*)0xCC003000 = val;
    void (*cb)(s32, OSContext*) = __DBDbgCallback;
    if (cb == NULL) {
        return;
    }
    cb(intr, ctx);
}

void DBInitComm(u8** flag, void (*callback)(s32)) {
    void (*savedCb)(s32) = callback;
    u8** savedFlag = flag;
    BOOL enabled = OSDisableInterrupts();
    *savedFlag = &__DBEXIInputFlag;
    __DBMtrCallback = savedCb;
    __DBEXIInit();
    OSRestoreInterrupts(enabled);
}

void DBInitInterrupts(void) {
    __OSMaskInterrupts(0x18000);
    __OSMaskInterrupts(0x40);
    __DBDbgCallback = (void (*)(s32, OSContext*))__DBMtrHandler;
    __OSSetInterruptHandler(0x19, (OSInterruptHandler)__DBIntrHandler);
    __OSUnmaskInterrupts(0x40);
}

s32 DBQueryData(void) {
    __DBEXIInputFlag = 0;
    if (__DBRecvDataSize == 0) {
        BOOL enabled = OSDisableInterrupts();
        u8 statusBuf;
        __DBEXIReadReg(0x34000000, &statusBuf, 1);
        if (statusBuf & 0x08) {
            goto restore;
        }
        {
            u32 cmdBuf;
            __DBEXIReadReg(0x34000200, &cmdBuf, 4);
            u32 cmd = cmdBuf & 0x1F000000;
            if (cmd == 0x1F000000) {
                __DBRecvMail = cmdBuf;
                __DBRecvDataSize = cmdBuf & 0x1FFF;
                __DBEXIInputFlag = 1;
            }
        }
    restore:
        OSRestoreInterrupts(enabled);
    }
    return (s32)__DBRecvDataSize;
}

s32 DBRead(void* buf, s32 size) {
    BOOL enabled = OSDisableInterrupts();
    u32 mail = __DBRecvMail;
    u32 alignedSize = (u32)(size + 3) & ~3u;
    u32 bit = (mail >> 16) & 1;
    u32 negBit = -(s32)bit;
    u32 tmp = (negBit & 0x800u) + 0xD10000u + 0x1000u;
    u32 exi2Addr = (tmp << 6) & 0x3FFFFF00u;
    __DBEXIReadRam(exi2Addr, buf, alignedSize);
    __DBRecvDataSize = 0;
    __DBEXIInputFlag = 0;
    OSRestoreInterrupts(enabled);
    return 0;
}

s32 DBWrite(const void* buf, s32 size) {
    static u8 l_byOffsetCounter;
    BOOL enabled = OSDisableInterrupts();
    // Wait for TX ready (poll bit 2 set = busy)
    {
        u8 statusBuf;
        do {
            __DBEXIReadReg(0x34000000, &statusBuf, 1);
        } while (statusBuf & 0x04);
    }
    u8 counter = l_byOffsetCounter + 1;
    l_byOffsetCounter = counter;
    u32 negCounter = -(s32)(counter & 1);
    u32 tmp = (negCounter & 0x800u) + 0xD10000u;
    u32 exi2Addr = ((tmp << 6) & 0x3FFFFF00u) | 0x80000000u;
    u32 alignedSize = (u32)(size + 3) & ~3u;

    // Write data, retry if fails
    while (__DBEXIWriteRam(exi2Addr, buf, alignedSize) == 0) {}

    // Wait for TX ready again
    {
        u8 statusBuf;
        do {
            __DBEXIReadReg(0x34000000, &statusBuf, 1);
        } while (statusBuf & 0x04);
    }

    // Build command and send
    u8 counter2 = l_byOffsetCounter;
    u32 sizeMask = (u32)(size & 0x1FFF) | 0x1F000000u;
    u32 sendCmd = sizeMask | ((u32)counter2 << 16);
    u32 cmdReg = 0xB4000100u;
    while (__DBEXIWriteReg(cmdReg, &sendCmd, 4) == 0) {}

    // Wait for final TX done
    {
        u8 statusBuf;
        do {
            __DBEXIReadReg(0x34000000, &statusBuf, 1);
        } while (statusBuf & 0x04);
    }

    OSRestoreInterrupts(enabled);
    return 0;
}

void DBOpen(void) {
}

void DBClose(void) {
}
