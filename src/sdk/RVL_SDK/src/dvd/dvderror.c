#include "revolution/dvd/dvderror.h"
#include "revolution/dvd/dvd_broadway.h"
#include "revolution/nand/NANDOpenClose.h"
#include "revolution/nand/nand.h"
#include "revolution/os/OSTime.h"
#include "revolution/os/OSCache.h"
#include "types.h"

// File paths for error log (local data, not rodata)
static char sErrorPath[] = "/shared2/test2/dvderror.dat";
static char sDirPath[] = "/shared2/test2";

// External declarations for async NAND functions not in headers
extern s32 NANDPrivateCreateAsync(const char* path, u8 perm, u8 attr,
                                   NANDCallback cb, NANDCommandBlock* block);
extern s32 NANDPrivateCreateDirAsync(const char* path, u8 perm, u8 attr,
                                      NANDCallback cb, NANDCommandBlock* block);

// NAND state (bss, consecutive)
static NANDFileInfo NandInfo;
static NANDCommandBlock NandCb;
static DVDErrorInfo __ErrorInfo;
static DVDErrorInfo __FirstErrorInfo;

// sbss: async state
static s32 ExistFlag;
static u32 NextOffset;
static void (*Callback)(s32 result, s32 arg1);

// Invoke the DVD-level callback with appropriate code
// NAND result 0 (success) -> DVD code 1 (OK), non-zero -> DVD code 2 (error)
// cntlzw(0)=32, extrwi bit5=1, neg=-1, +2=1 -> Callback(1,0)
// cntlzw(nonzero)<32, extrwi bit5=0, neg=0, +2=2 -> Callback(2,0)
static void cbForNandClose(s32 result, NANDCommandBlock* block) {
    if (Callback == NULL) {
        return;
    }
    u32 czw = __cntlzw((u32)result);
    s32 dvdResult = (s32)((czw >> 5) & 1);
    dvdResult = -dvdResult;
    dvdResult += 2;
    Callback(dvdResult, 0);
}

static void cbForNandWrite(s32 result, NANDCommandBlock* block) {
    if (NANDCloseAsync(&NandInfo, cbForNandClose, &NandCb) != 0) {
        if (Callback != NULL) {
            Callback(2, 0);
        }
    }
}

static void cbForNandSeek(s32 result, NANDCommandBlock* block) {
    s32 ret;
    u32 nextPlus1 = NextOffset + 1;

    if ((u32)result == nextPlus1 * 0x80) {
        if (NextOffset == 0) {
            __ErrorInfo.nextOffset = nextPlus1 % 7;
        }
        DCFlushRange(&__ErrorInfo, 0x80);
        ret = NANDWriteAsync(&NandInfo, &__ErrorInfo, 0x80,
                             cbForNandWrite, &NandCb);
        if (ret != 0) {
            if (NANDCloseAsync(&NandInfo, cbForNandClose, &NandCb) != 0) {
                if (Callback != NULL) {
                    Callback(2, 0);
                }
            }
        }
    } else {
        if (Callback != NULL) {
            Callback(2, 0);
        }
    }
}

static void cbForNandWrite0(s32 result, NANDCommandBlock* block) {
    u32 nextPos;

    if ((u32)result == 0x80) {
        nextPos = NextOffset + 1;
        if (NANDSeekAsync(&NandInfo, (s32)(nextPos * 0x80), 0, cbForNandSeek, &NandCb) == 0) {
            return;
        }
        {
            u32 np = NextOffset + 1;
            u32 seekPos = np * 0x80;
            if ((u32)(seekPos + 0x10000) == 0xffff) {
                if (NextOffset == 0) {
                    __ErrorInfo.nextOffset = np % 7;
                }
                DCFlushRange(&__ErrorInfo, 0x80);
                if (NANDWriteAsync(&NandInfo, &__ErrorInfo, 0x80,
                                   cbForNandWrite, &NandCb) != 0) {
                    if (NANDCloseAsync(&NandInfo, cbForNandClose, &NandCb) != 0) {
                        if (Callback != NULL) {
                            Callback(2, 0);
                        }
                    }
                }
                return;
            } else {
                if (Callback != NULL) {
                    Callback(2, 0);
                }
                return;
            }
        }
    }

    if (Callback != NULL) {
        Callback(2, 0);
    }
}

static void cbForNandSeek2(s32 result, NANDCommandBlock* block) {
    u32 nextPlus1;

    if ((u32)result == 0x80) {
        nextPlus1 = __FirstErrorInfo.nextOffset + 1;
        __FirstErrorInfo.nextOffset = nextPlus1 % 7;

        if (NANDWriteAsync(&NandInfo, &__FirstErrorInfo, 0x80,
                           cbForNandWrite0, &NandCb) != 0) {
            if (Callback != NULL) {
                Callback(2, 0);
            }
        }
        return;
    }

    if (Callback != NULL) {
        Callback(2, 0);
    }
}

static void cbForNandRead(s32 result, NANDCommandBlock* block) {
    if ((u32)result == 0x80) {
        NextOffset = __FirstErrorInfo.nextOffset;
        if (NANDSeekAsync(&NandInfo, 0x80, 0, cbForNandSeek2, &NandCb) != 0) {
            if (Callback != NULL) {
                Callback(2, 0);
            }
        }
    } else {
        __ErrorInfo.nextOffset = 1;
        if (NANDWriteAsync(&NandInfo, &__ErrorInfo, 0x80,
                           cbForNandWrite, &NandCb) != 0) {
            if (NANDCloseAsync(&NandInfo, cbForNandClose, &NandCb) != 0) {
                if (Callback != NULL) {
                    Callback(2, 0);
                }
            }
        }
    }
}

static void cbForNandSeek0(s32 result, NANDCommandBlock* block) {
    if (result == 0) {
        NextOffset = 0;
        __ErrorInfo.nextOffset = 1;
        if (NANDWriteAsync(&NandInfo, &__FirstErrorInfo, 0x80,
                           cbForNandWrite0, &NandCb) != 0) {
            if (Callback != NULL) {
                Callback(2, 0);
            }
        }
        return;
    }

    if (Callback != NULL) {
        Callback(2, 0);
    }
}

static void cbForNandSeek1(s32 result, NANDCommandBlock* block) {
    if ((u32)result == 0x80) {
        if (NANDReadAsync(&NandInfo, &__FirstErrorInfo, 0x80,
                          cbForNandRead, &NandCb) != 0) {
            __ErrorInfo.nextOffset = 1;
            if (NANDWriteAsync(&NandInfo, &__ErrorInfo, 0x80,
                               cbForNandWrite, &NandCb) != 0) {
                if (NANDCloseAsync(&NandInfo, cbForNandClose, &NandCb) != 0) {
                    if (Callback != NULL) {
                        Callback(2, 0);
                    }
                }
            }
        }
    } else {
        if (NANDSeekAsync(&NandInfo, 0, 0, cbForNandSeek0, &NandCb) != 0) {
            if (Callback != NULL) {
                Callback(2, 0);
            }
        }
    }
}

static void cbForNandOpen(s32 result, NANDCommandBlock* block) {
    if (result == 0) {
        if (ExistFlag != 0) {
            if (NANDSeekAsync(&NandInfo, 0x80, 0, cbForNandSeek1, &NandCb) != 0) {
                if (NANDSeekAsync(&NandInfo, 0, 0, cbForNandSeek0, &NandCb) != 0) {
                    if (Callback != NULL) {
                        Callback(2, 0);
                    }
                }
            }
        } else {
            NextOffset = 0;
            __ErrorInfo.nextOffset = 1;
            if (NANDWriteAsync(&NandInfo, &__FirstErrorInfo, 0x80,
                               cbForNandWrite0, &NandCb) != 0) {
                if (Callback != NULL) {
                    Callback(2, 0);
                }
            }
        }
    } else {
        if (Callback != NULL) {
            Callback(2, 0);
        }
    }
}

static void cbForNandCreate(s32 result, NANDCommandBlock* block) {
    if (result == 0 || result == -6) {
        if (result == -6) {
            ExistFlag = 1;
        }
        if (NANDPrivateOpenAsync(sErrorPath, &NandInfo, 0x3,
                                  cbForNandOpen, &NandCb) != 0) {
            if (Callback != NULL) {
                Callback(2, 0);
            }
        }
    } else {
        if (Callback != NULL) {
            Callback(2, 0);
        }
    }
}

static void cbForNandCreateDir(s32 result, NANDCommandBlock* block) {
    if (result == 0 || result == -6) {
        if (NANDPrivateCreateAsync(sErrorPath, 0x3f, 0,
                                   cbForNandCreate, &NandCb) != 0) {
            if (Callback != NULL) {
                Callback(2, 0);
            }
        }
    } else {
        if (Callback != NULL) {
            Callback(2, 0);
        }
    }
}

static void cbForPrepareControlRegister(u32 result) {
    if (result == 1) {
        __ErrorInfo.status2 = DVDLowGetControlRegister();
    } else {
        __ErrorInfo.status2 = (u32)-1;
    }
    if (NANDPrivateCreateDirAsync(sDirPath, 0x3f, 0,
                                  cbForNandCreateDir, &NandCb) != 0) {
        if (Callback != NULL) {
            Callback(2, 0);
        }
    }
}

static void cbForPrepareStatusRegister(u32 result) {
    if (result == 1) {
        __ErrorInfo.status = DVDLowGetStatusRegister();
    } else {
        __ErrorInfo.status = (u32)-1;
    }
    if (DVDLowPrepareControlRegister(cbForPrepareControlRegister) == 0) {
        if (Callback != NULL) {
            Callback(2, 0);
        }
    }
}

void __DVDStoreErrorCode(u32 error, void (*cb)(s32, s32)) {
    OSTime time;

    __ErrorInfo.error = error;
    time = OSGetTime();
    __ErrorInfo.dateTime = (u32)(time / (*(u32*)0x800000F8 >> 2));
    Callback = cb;
    DVDLowPrepareStatusRegister(cbForPrepareStatusRegister);
}
