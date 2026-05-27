#include "revolution/dvd/dvdDeviceError.h"
#include "revolution/dvd/dvdfatal.h"
#include "revolution/dvd/dvd_broadway.h"
#include "revolution/os/OS.h"
#include "revolution/os/OSFont.h"
#include "revolution/os/OSFatal.h"
#include "revolution/os/OSMemory.h"
#include "revolution/sc/scapi.h"
#include "revolution/gx/GXTypes.h"

// Error messages indexed by SCLanguage (JP, EN, DE, FR, SP, IT, NL)
// Pointer table in .rodata, strings in .data
const char* __DVDDeviceErrorMessage[7] = {
    "\n\n\n\x83G\x83\x89\x81[\x83\x82\x83\x88\x83\x82\x83\x82\x82\xb1\x82\xd4\x82\xdc\x82\xb5\x82\xbd\x81B",
    "\n\n\nError #001,\nunauthorized device has been detected.",
    "\n\n\nFehler #001:\nEs wurde eine unzul\xc3ssige Komponente\nentdeckt.",
    "\n\n\nErreur 001:\nun dispositif non autoris\xe9 a \xe9t\xe9 d\xe9tect\xe9.",
    "\n\n\nError 001:\nSe ha detectado un dispositivo no\nautorizado.",
    "\n\n\nErrore #001:\nrilevato un dispositivo non autorizzato.",
    "\n\n\nFout #001:\nongeoorloofd onderdeel gevonden.",
};

// sdata/sbss globals for callback state
u32 lowIntType;
u32 lowDone = 1;

// BSS buffer (32-byte aligned)
static u8 CheckBuffer[0x20];

static void lowCallback(u32 intType) {
    lowIntType = intType;
    lowDone = 1;
}

BOOL __DVDCheckDevice(void) {
    OSIOSRev rev;
    GXColor bgColor;
    GXColor textColor;
    u32 checkOffset;
    u32 val;
    u8 lang;
    const char* msg;
    const char** msgTable;

    checkOffset = 0x460a0000;

    if (OSGetPhysicalMem2Size() == 0x08000000u) {
        return 1;
    }

    __OSGetIOSRev(&rev);
    if (rev.idLo < 0x1e) {
        return 1;
    }
    if (rev.idLo >= 0xfe) {
        return 1;
    }

    if (*(u8*)0x8000319c == 0x81) {
        checkOffset = 0x7ed40000;
    }

    // First unencrypted read
    lowDone = 0;
    DVDLowUnencryptedRead(CheckBuffer, 0x20, checkOffset, lowCallback);
    while (lowDone == 0) {}

    if ((int)lowIntType == 2) {
        // DV interrupt: request error info
        lowDone = 0;
        DVDLowRequestError(lowCallback);
        while (lowDone == 0) {}

        DVDLowGetImmBufferReg();
        if ((int)lowIntType != 1) {
            goto show_fatal;
        }
        // Check high byte of first imm reg
        val = DVDLowGetImmBufferReg();
        if (val & 0xFF000000) {
            goto bad_device;
        }
        // Check lower 24 bits of second imm reg for 0x052100
        val = DVDLowGetImmBufferReg();
        if ((val & 0x00FFFFFF) == 0x052100) {
            goto second_read;
        }
    } else if ((int)lowIntType > 2) {
        goto show_fatal;
    } else if ((int)lowIntType > 0) {
        goto show_error;
    } else {
        goto show_fatal;
    }
    goto show_error;

second_read:
    // Second read: DVDLowReportKey
    lowDone = 0;
    DVDLowReportKey(CheckBuffer, 0x40000, 0, lowCallback);
    while (lowDone == 0) {}

    if ((int)lowIntType == 2) {
        // DV interrupt: request error info
        lowDone = 0;
        DVDLowRequestError(lowCallback);
        while (lowDone == 0) {}

        DVDLowGetImmBufferReg();
        if ((int)lowIntType != 1) {
            goto show_fatal;
        }
        // Check high byte of first imm reg
        val = DVDLowGetImmBufferReg();
        if (val & 0xFF000000) {
            goto bad_device;
        }
        // Check lower 24 bits
        val = DVDLowGetImmBufferReg();
        val = val & 0x00FFFFFF;
        if (val == 0x053100) {
            return 1;
        } else if (val >= 0x053100) {
            goto show_error;
        }
        if (val == 0x052000) {
            return 1;
        } else {
            goto show_error;
        }
    } else if ((int)lowIntType > 2) {
        goto show_fatal;
    } else if ((int)lowIntType > 0) {
        goto show_error;
    } else {
        goto show_fatal;
    }
    goto show_error;

show_error:
    *(u32*)&bgColor = 0;
    *(u32*)&textColor = 0xFFFFFF00u;
    msgTable = __DVDDeviceErrorMessage;

    lang = SCGetLanguage();
    if (lang == 0) {
        OSSetFontEncode(1);
    } else {
        OSSetFontEncode(0);
    }

    lang = SCGetLanguage();
    if (lang > 6) {
        msg = msgTable[1];
    } else {
        lang = SCGetLanguage();
        msg = msgTable[lang];
    }

    OSFatal(textColor, bgColor, msg);
    return 0;

bad_device:
    return 0;

show_fatal:
    __DVDShowFatalMessage();
    return 0;
}
