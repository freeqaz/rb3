#include "revolution/dvd/dvdfatal.h"
#include "revolution/OS.h"
#include "revolution/os/OSFatal.h"
#include "revolution/os/OSFont.h"
#include "revolution/sc/scapi.h"
#include "revolution/sc/scapi_prdinfo.h"

extern const char* const __DVDErrorMessageDefault[];
extern const char* const __DVDErrorMessageEurope[];
const char* __DVDErrorMessageChinaKorea[2];

static void (*FatalFunc)(void) = NULL;

void __DVDShowFatalMessage(void) {
    GXColor textColor;
    GXColor bgColor;
    const char* const* table;
    const char* msg;
    u8 lang;
    SCProductGameRegion region;

    *(u32*)&bgColor = 0;
    *(u32*)&textColor = 0xFFFFFF00u;

    lang = SCGetLanguage();
    if (lang == 0) {
        OSSetFontEncode(1);
    } else {
        OSSetFontEncode(0);
    }

    region = SCGetProductGameRegion();
    if ((unsigned)(region - 4) > 1u) {
        if (region != 2) {
            table = __DVDErrorMessageDefault;
        } else {
            table = __DVDErrorMessageEurope;
        }
    } else {
        table = __DVDErrorMessageChinaKorea;
    }

    lang = SCGetLanguage();
    if (lang > 6) {
        msg = table[1];
    } else {
        lang = SCGetLanguage();
        msg = table[lang];
    }

    OSFatal(textColor, bgColor, msg);
}

BOOL DVDSetAutoFatalMessaging(BOOL enable) {
    BOOL prev;
    BOOL enabled = OSDisableInterrupts();
    prev = ((-((int)FatalFunc) | (int)FatalFunc) >> 31) & 1;
    if (enable) {
        FatalFunc = __DVDShowFatalMessage;
    } else {
        FatalFunc = NULL;
    }
    OSRestoreInterrupts(enabled);
    return prev;
}

BOOL __DVDGetAutoFatalMessaging(void) {
    return ((-((int)FatalFunc) | (int)FatalFunc) >> 31) & 1;
}

void __DVDPrintFatalMessage(void) {
    if (FatalFunc == NULL) {
        return;
    }
    (*FatalFunc)();
}
