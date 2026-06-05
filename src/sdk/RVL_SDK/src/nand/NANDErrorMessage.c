#include "revolution/nand/nand.h"
#include <revolution/NAND.h>
#include <revolution/FS.h>
#include <revolution/OS.h>
#include <revolution/os/OSFatal.h>
#include <revolution/os/OSFont.h>
#include <revolution/sc/scapi.h>
#include <revolution/sc/scapi_prdinfo.h>
#include <revolution/gx/GXTypes.h>

extern const char* const __NANDMaxBlocksErrorMessageDefault[];
extern const char* const __NANDMaxBlocksErrorMessageEurope[];
extern const char* const __NANDMaxFilesErrorMessageDefault[];
extern const char* const __NANDMaxFilesErrorMessageEurope[];
extern const char* const __NANDCorruptErrorMessageDefault[];
extern const char* const __NANDCorruptErrorMessageEurope[];
extern const char* const __NANDBusyErrorMessageDefault[];
extern const char* const __NANDBusyErrorMessageEurope[];
extern const char* const __NANDUnknownErrorMessageDefault[];
extern const char* const __NANDUnknownErrorMessageEurope[];

static void (*ErrorFunc)(ISFSError) = NULL;

void __NANDShowErrorMessage(ISFSError error) {
    GXColor textColor;
    GXColor bgColor;
    const char* const* defaultTable;
    const char* const* europeTable;
    const char* msg;
    u8 lang;
    SCProductGameRegion region;

    *(u32*)&bgColor = 0;
    *(u32*)&textColor = 0xFFFFFF00u;

    if (error == ISFS_ERROR_MAXBLOCKS) {
        defaultTable = __NANDMaxBlocksErrorMessageDefault;
        europeTable = __NANDMaxBlocksErrorMessageEurope;
    } else if (error == ISFS_ERROR_MAXFILES) {
        defaultTable = __NANDMaxFilesErrorMessageDefault;
        europeTable = __NANDMaxFilesErrorMessageEurope;
    } else if (error == ISFS_ERROR_CORRUPT) {
        defaultTable = __NANDCorruptErrorMessageDefault;
        europeTable = __NANDCorruptErrorMessageEurope;
    } else if (error == ISFS_ERROR_BUSY || error == IOS_ERROR_QFULL) {
        defaultTable = __NANDBusyErrorMessageDefault;
        europeTable = __NANDBusyErrorMessageEurope;
    } else {
        defaultTable = __NANDUnknownErrorMessageDefault;
        europeTable = __NANDUnknownErrorMessageEurope;
    }

    lang = SCGetLanguage();
    if (lang == 0) {
        OSSetFontEncode(1);
    } else {
        OSSetFontEncode(0);
    }

    region = SCGetProductGameRegion();
    if ((unsigned)(region - 4) > 1u) {
        const char* const* table;
        if (region != SC_PRD_GAME_REG_EU) {
            table = defaultTable;
        } else {
            table = europeTable;
        }

        lang = SCGetLanguage();
        if (lang > 6) {
            msg = table[1];
        } else {
            lang = SCGetLanguage();
            msg = table[lang];
        }
    } else {
        lang = SCGetLanguage();
        if (lang > 6) {
            msg = defaultTable[1];
        } else {
            lang = SCGetLanguage();
            msg = defaultTable[lang];
        }
    }

    OSFatal(textColor, bgColor, msg);
}

BOOL NANDSetAutoErrorMessaging(BOOL enable) {
    BOOL prev;
    BOOL enabled = OSDisableInterrupts();
    prev = ((-((int)ErrorFunc) | (int)ErrorFunc) >> 31) & 1;
    if (enable) {
        ErrorFunc = __NANDShowErrorMessage;
    } else {
        ErrorFunc = NULL;
    }
    OSRestoreInterrupts(enabled);
    return prev;
}

void __NANDPrintErrorMessage(ISFSError error) {
    if (ErrorFunc == NULL) {
        return;
    }
    (*ErrorFunc)(error);
}
