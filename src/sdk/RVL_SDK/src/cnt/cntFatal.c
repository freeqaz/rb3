#include "revolution/cnt/cnt.h"
#include <revolution/CNT.h>
#include <revolution/OS.h>
#include <revolution/os/OSFatal.h>
#include <revolution/os/OSFont.h>
#include <revolution/sc/scapi.h>
#include <revolution/sc/scapi_prdinfo.h>
#include <revolution/gx/GXTypes.h>

extern const char* const __CNTFatalErrorMessage[];
extern const char __CNTFatalErrorMessageWithError[];
extern const char* const __CNTCorruptErrorMessage[];
extern const char __CNTCorruptErrorMessageWithError[];

void __CNTShowFatalMessage(void) {
    GXColor textColor;
    GXColor bgColor;
    const char* const* table;
    const char* msg;
    u8 lang;
    SCProductGameRegion region;

    *(u32*)&bgColor = 0;
    *(u32*)&textColor = 0xFFFFFF00u;

    region = SCGetProductGameRegion();
    if ((unsigned)(region - 4) > 1u) {
        lang = SCGetLanguage();
        if (lang == 0) {
            OSSetFontEncode(1);
        } else {
            OSSetFontEncode(0);
        }

        table = __CNTFatalErrorMessage;

        lang = SCGetLanguage();
        if (lang > 6) {
            msg = table[1];
        } else {
            lang = SCGetLanguage();
            msg = table[lang];
        }
    } else {
        OSSetFontEncode(0);
        msg = __CNTFatalErrorMessageWithError;
    }

    OSFatal(textColor, bgColor, msg);
}

void __CNTShowCorruptMessage(void) {
    GXColor textColor;
    GXColor bgColor;
    const char* const* table;
    const char* msg;
    u8 lang;
    SCProductGameRegion region;

    *(u32*)&bgColor = 0;
    *(u32*)&textColor = 0xFFFFFF00u;

    region = SCGetProductGameRegion();
    if ((unsigned)(region - 4) > 1u) {
        lang = SCGetLanguage();
        if (lang == 0) {
            OSSetFontEncode(1);
        } else {
            OSSetFontEncode(0);
        }

        table = __CNTCorruptErrorMessage;

        lang = SCGetLanguage();
        if (lang > 6) {
            msg = table[1];
        } else {
            lang = SCGetLanguage();
            msg = table[lang];
        }
    } else {
        OSSetFontEncode(0);
        msg = __CNTCorruptErrorMessageWithError;
    }

    OSFatal(textColor, bgColor, msg);
}
