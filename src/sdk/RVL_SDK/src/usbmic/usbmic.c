/*
 * usbmic.c - Logitech USB Microphone driver for RVL_SDK
 * Compiled as C++ with namespace usbmic
 * Target: SZBE69_B8 (Rock Band 3 Wii)
 */

#include <revolution/OS.h>
#include <revolution/USB.h>
#include <string.h>

/* Additional USB functions not declared in the header */
extern "C" {
    void IUSB_CloseDevice(s32 fd);
    IPCResult IUSB_OpenDeviceIdsAsync(const char* path, u16 vid, u16 pid,
                                       USBCallback callback, void* arg);
    void IUSB_DeviceRemovalNotifyAsync(s32 fd, USBCallback callback, void* arg);
    void IUSB_RegisterInsertionNotifyWithIdAsync(const char* path, u16 vid, u16 pid,
                                                  u32 flags, USBCallback callback,
                                                  unsigned int* idOut, u32 reserved);
    void IUSB_CancelInsertionNotify(const char* path, unsigned int handle);
    void OSSleepTicks(s64 ticks);
    void OSRegisterVersion(const char* str);
}

/* micOpenParam is a global (non-namespaced) type: the mic*() public API and
 * the internal Mic_Open() both take it unqualified, so it is declared at
 * global scope rather than inside namespace usbmic. */
struct micOpenParam { char _[0x20]; };

namespace usbmic {

/* -------------------------------------------------------------------------
 * Types used across translation units
 * ---------------------------------------------------------------------- */

/* Forward-declared opaque types */
struct Mic;
struct micDesc;
struct DPCContext;
struct DPCEntry;
struct IsoTransfer;

/* DPCEntry definition (used by queueing code in this file) */
struct DPCEntry {
    void (*handler)(DPCEntry*); /* at +0x00 */
    void* arg;                   /* at +0x04 */
    unsigned int pad0;           /* at +0x08 */
    unsigned int pad1;           /* at +0x0c */
};

/* USB Audio descriptor types */
#pragma pack(1)
struct USB_CommonDescr {
    unsigned char  bLength;
    unsigned char  bDescriptorType;
};

struct UA_CommonDescr {
    unsigned char  bLength;
    unsigned char  bDescriptorType;
    unsigned char  bDescriptorSubtype;
};

struct USB_ConfigDescr {
    unsigned char  bLength;
    unsigned char  bDescriptorType;
    unsigned short wTotalLength;
    unsigned char  bNumInterfaces;
    unsigned char  bConfigurationValue;
    unsigned char  iConfiguration;
    unsigned char  bmAttributes;
    unsigned char  bMaxPower;
};

struct USB_InterfaceDescr {
    unsigned char  bLength;
    unsigned char  bDescriptorType;
    unsigned char  bInterfaceNumber;
    unsigned char  bAlternateSetting;
    unsigned char  bNumEndpoints;
    unsigned char  bInterfaceClass;
    unsigned char  bInterfaceSubClass;
    unsigned char  bInterfaceProtocol;
    unsigned char  iInterface;
};
#pragma pack()

/* -------------------------------------------------------------------------
 * Extern declarations for functions in other translation units
 * ---------------------------------------------------------------------- */

/* mic.c */
extern s32  Mic_Initialize(Mic* mic);
extern s32  Mic_DeInitialize(Mic* mic);
extern s32  Mic_GetDescription(Mic* mic, micDesc* desc);
extern s32  Mic_Open(Mic* mic, micOpenParam* param);
extern s32  Mic_Close(Mic* mic);
extern s32  Mic_CloseFinalize(Mic* mic);
extern s32  Mic_Read(Mic* mic, void* buf, unsigned long* size);
extern s32  Mic_IncOutstandingRequests(Mic* mic);
extern s32  Mic_DecOutstandingRequests(Mic* mic);
extern s32  Mic_SetVolume(Mic* mic, u16 vol);
extern s32  Mic_SetMute(Mic* mic, bool mute);

/* dpc.c */
extern void DPC_Initialize(DPCContext* ctx);
extern void DPC_Deinitialize(DPCContext* ctx);
extern void DPC_Queue(DPCContext* ctx, DPCEntry* entry);
extern void DPC_Process(DPCContext* ctx);

/* usbhelpers.c */
extern s32  StartGetDescriptor(s32 fd, u8 type, void* buf, u16 size,
                                void (*cb)(s32, void*), void* arg);
extern s32  StartSetInterface(s32 fd, u8 iface, u8 alt, void (*cb)(s32, void*),
                               void* arg);
extern s32  StartGetMinMaxReq(s32 fd, u8 req, u8 cs, u8 unit, u8 channel,
                               u8 iface, void* buf, u16 size,
                               void (*cb)(s32, void*), void* arg);
extern USB_CommonDescr* ScanDescriptor(USB_CommonDescr* start, u8 type,
                                        USB_CommonDescr* cfgBase, u32 cfgLen);
extern UA_CommonDescr*  ScanUADescriptor(UA_CommonDescr* start, u8 type, u8 sub,
                                          UA_CommonDescr* cfgBase, u32 cfgLen);
extern UA_CommonDescr*  ScanUADescriptorById(UA_CommonDescr* start, u8 id,
                                              UA_CommonDescr* cfgBase, u32 cfgLen);

/* -------------------------------------------------------------------------
 * Opaque struct size stubs
 * ---------------------------------------------------------------------- */

/* Mic = 0x74 bytes */
struct Mic { char _[0x74]; };

/* DPCContext = 0x8c bytes */
struct DPCContext { char _[0x8c]; };

/* micDesc = 0x2c bytes (from standardMic symbol size) */
struct micDesc {
    unsigned char  header[8];   /* +0x00 */
    unsigned int   numSampRates;/* +0x08 */
    unsigned int   sampRates[5];/* +0x0c */
    unsigned int   _pad[3];     /* +0x20 */
};

/* -------------------------------------------------------------------------
 * Library global struct
 * ---------------------------------------------------------------------- */

typedef void* (*MallocFn)(unsigned long size, unsigned long align);
typedef void  (*FreeFn)(void* ptr);

struct Library {
    u8           initialized;       /* +0x00 */
    u8           _pad[3];           /* +0x01 */
    OSMutex      mutex;             /* +0x04 (0x18 bytes on Wii) */
    MallocFn     mallocFn;          /* +0x1c */
    FreeFn       freeFn;            /* +0x20 */
    Mic          mics[4];           /* +0x24 (4 * 0x74 = 0x1d0 bytes) */
    DPCContext   dpc;               /* +0x1f4 (0x8c bytes) */
    void*        descBuf;           /* +0x280 */
    unsigned int descStep;          /* +0x284 */
    unsigned int insertNotifyHandle;/* +0x288 */
};

/* Single library instance */
Library g_lib;

/* USB device path */
static const char s_devPath[] = "/dev/usb/oh0";

/* Standard microphone descriptor (rodata) */
const micDesc standardMic = {
    { 0x01, 0x00, 0x00, 0x00, 0x01, 0x10, 0x00, 0x00 },
    5,
    { 8000, 11025, 22050, 44100, 48000 },
    { 0, 0, 0 }
};

/* Library version string (sdata) */
const char* __LOGITECH_MICVersion =
    "<< RVL_MWM - LOGITECH_MIC \trelease build: Aug 17 2009 16:44:22 (0x4302_145)  >>";

/* -------------------------------------------------------------------------
 * Static function prototypes
 * ---------------------------------------------------------------------- */

void Library_HandleRemovalDPC(DPCEntry* entry);
void Library_HandleCloseFinalizeDPC(DPCEntry* entry);
void Library_QueueForRemoval(Mic* mic);
void Library_QueueForCloseFinalize(Mic* mic);
void Library_HandleHotplugRegistrationDPC(DPCEntry* entry);

/* Constant DPCEntry templates (rodata) */
static const DPCEntry kRemovalEntry    = { Library_HandleRemovalDPC, 0, 0, 0 };
static const DPCEntry kCloseFinalEntry = { Library_HandleCloseFinalizeDPC, 0, 0, 0 };
static const DPCEntry kHotplugEntry    = { Library_HandleHotplugRegistrationDPC, 0, 0, 0 };
s32  Library_OnRemove(s32 result, void* arg);
void Library_OnSetInterfaceDone(s32 result, void* arg);
void Library_OnGetVolDone(s32 result, void* arg);
s32  Library_DecodeACI(USB_ConfigDescr* cfg, USB_InterfaceDescr* iface, Mic* mic);
s32  Library_DecodeASI(USB_ConfigDescr* cfg, USB_InterfaceDescr* iface, Mic* mic);
s32  Library_AnalyzeDescriptor(USB_ConfigDescr* cfg, Mic* mic);
void Library_OnGetDescriptorDone(s32 result, void* arg);
void Library_OnFakeCloseDone(s32 result, void* arg);
s32  Library_OnFakeRemove(s32 result, void* arg);
void Library_OnFakeOpenDone(s32 result, void* arg);
void Library_OnOpenDone(s32 result, void* arg);
void Library_OnAttach(s32 result, void* arg);

/* -------------------------------------------------------------------------
 * DPC handler implementations
 * ---------------------------------------------------------------------- */

void Library_HandleRemovalDPC(DPCEntry* entry)
{
    Mic* mic = (Mic*)entry->arg;
    if (*(signed char*)((char*)mic + 0x10) <= 0) {
        Mic_Close(mic);
        IUSB_CloseDevice(*(s32*)((char*)mic + 8));
        Mic_DeInitialize(mic);
        *(s32*)mic = 0;
    }
}

void Library_HandleCloseFinalizeDPC(DPCEntry* entry)
{
    Mic* mic = (Mic*)entry->arg;
    if (*(s32*)((char*)mic + 4) != 3) {
        return;
    }
    Mic_CloseFinalize(mic);
}

void Library_QueueForRemoval(Mic* mic)
{
    DPCEntry entry = kRemovalEntry;
    entry.arg = mic;
    DPC_Queue(&g_lib.dpc, &entry);
}

void Library_QueueForCloseFinalize(Mic* mic)
{
    DPCEntry entry = kCloseFinalEntry;
    entry.arg = mic;
    DPC_Queue(&g_lib.dpc, &entry);
}

void Library_HandleHotplugRegistrationDPC(DPCEntry* entry)
{
    static unsigned int insertNotifyId;
    insertNotifyId = 0;
    IUSB_RegisterInsertionNotifyWithIdAsync(s_devPath, 0x046d, 0x0a03,
                                             0, (USBCallback)Library_OnAttach,
                                             &insertNotifyId, 0);
    g_lib.insertNotifyHandle = insertNotifyId;
}

/* -------------------------------------------------------------------------
 * USB event callbacks
 * ---------------------------------------------------------------------- */

s32 Library_OnRemove(s32 result, void* arg)
{
    Mic* mic = (Mic*)arg;
    if (!g_lib.initialized) {
        return 0;
    }
    *(s32*)mic = 3;
    Mic_DecOutstandingRequests(mic);
    return 0;
}

void Library_OnSetInterfaceDone(s32 result, void* arg)
{
    Mic* mic = (Mic*)arg;

    *(u16*)((char*)mic + 0x12) = 500;
    *(u32*)((char*)mic + 0x14) = OSGetTick();
    *(s32*)mic = 2;
    Mic_DecOutstandingRequests(mic);

    {
        DPCEntry entry = kHotplugEntry;
        DPC_Queue(&g_lib.dpc, &entry);
    }
}

void Library_OnGetVolDone(s32 result, void* arg)
{
    Mic* mic = (Mic*)arg;
    BOOL failed = TRUE;
    s32 ret;

    if (result >= 0) {
        unsigned int step = g_lib.descStep + 1;
        void* buf = g_lib.descBuf;
        g_lib.descStep = step;
        switch (step) {
        case 3:
            {
                u16 raw = *(u16*)buf;
                *(u16*)((char*)mic + 0x6c) =
                    (u16)(raw >> 8) | (u16)(raw << 8);
                ret = StartGetMinMaxReq(*(s32*)((char*)mic + 8),
                                        0x83, 2,
                                        *(u8*)((char*)mic + 0x62),
                                        *(u8*)((char*)mic + 0x61),
                                        *(u8*)((char*)mic + 0x60),
                                        buf, 2,
                                        Library_OnGetVolDone, mic);
                if (ret >= 0) {
                    failed = FALSE;
                }
            }
            break;
        case 4:
            {
                u16 raw = *(u16*)buf;
                *(u16*)((char*)mic + 0x6e) =
                    (u16)(raw >> 8) | (u16)(raw << 8);
                ret = StartSetInterface(*(s32*)((char*)mic + 8),
                                        *(u8*)((char*)mic + 0x69),
                                        *(u8*)((char*)mic + 0x6a),
                                        Library_OnSetInterfaceDone, mic);
                if (ret >= 0) {
                    failed = FALSE;
                }
            }
            break;
        }
    }

    if (failed) {
        Mic_DecOutstandingRequests(mic);
        DPCEntry entry = kHotplugEntry;
        DPC_Queue(&g_lib.dpc, &entry);
    }
}

/* -------------------------------------------------------------------------
 * USB Audio descriptor parsing
 * ---------------------------------------------------------------------- */

s32 Library_DecodeACI(USB_ConfigDescr* cfg, USB_InterfaceDescr* iface,
                       Mic* mic)
{
    UA_CommonDescr* hdr;
    UA_CommonDescr* inputTerm;
    UA_CommonDescr* featureUnit;
    UA_CommonDescr* cur;
    u8 nextId;
    u8 subtype;
    u16 cfgLen;
    u16 headerLen;

    cfgLen = (u16)(cfg->wTotalLength >> 8 | cfg->wTotalLength << 8);

    hdr = ScanUADescriptor((UA_CommonDescr*)iface, 0x24, 1,
                            (UA_CommonDescr*)iface, cfgLen);
    if (!hdr) {
        return 0;
    }

    headerLen = *(u16*)((char*)hdr + 5);
    headerLen = (u16)(headerLen << 8 | headerLen >> 8);

    inputTerm = NULL;
    for (inputTerm = ScanUADescriptor(hdr, 0x24, 3, hdr, headerLen);
         inputTerm && *(short*)((char*)inputTerm + 4) != 0x0101;
         inputTerm = ScanUADescriptor(inputTerm, 0x24, 3, hdr, headerLen))
    {
    }

    if (!inputTerm) {
        return 0;
    }

    featureUnit = 0;
    nextId = *(u8*)((char*)inputTerm + 7);

    {
        u16 hl2 = *(u16*)((char*)hdr + 5);
        hl2 = (u16)(hl2 << 8 | hl2 >> 8);
        cur = ScanUADescriptorById(hdr, nextId, hdr, hl2);
    }

    while (cur && (subtype = *(u8*)((char*)cur + 2)) != 2) {
        if (subtype == 4) {
            nextId = *(u8*)((char*)cur + 5);
        } else if (subtype == 5) {
            nextId = *(u8*)((char*)cur + 5);
        } else if (subtype == 6) {
            featureUnit = cur;
            nextId = *(u8*)((char*)cur + 4);
        } else if (subtype == 7) {
            nextId = *(u8*)((char*)cur + 7);
        } else if (subtype == 8) {
            nextId = *(u8*)((char*)cur + 7);
        } else {
            return 0;
        }

        {
            u16 hl2 = *(u16*)((char*)hdr + 5);
            hl2 = (u16)(hl2 << 8 | hl2 >> 8);
            cur = ScanUADescriptorById(hdr, nextId, hdr, hl2);
        }
    }

    if (!cur) {
        return 0;
    }

    *(u8*)((char*)mic + 100)  = *(u8*)((char*)inputTerm + 3);
    *(u8*)((char*)mic + 0x62) = 0xff;
    *(u8*)((char*)mic + 99)   = 0xff;

    if (!featureUnit) {
        *(u8*)((char*)mic + 0x61) = 0;
    } else {
        u8* controls = (u8*)featureUnit + 6;
        u8  chCount  = *(u8*)((char*)featureUnit + 5);

        *(u8*)((char*)mic + 0x61) = *(u8*)((char*)featureUnit + 3);

        if (controls[0] & 2) {
            *(u8*)((char*)mic + 0x62) = 0;
        }
        if (controls[0] & 1) {
            *(u8*)((char*)mic + 99) = 0;
        }
        if (controls[chCount] & 2) {
            *(u8*)((char*)mic + 0x62) = 1;
        }
        if (controls[chCount] & 1) {
            *(u8*)((char*)mic + 99) = 1;
        }
    }
    return 1;
}

s32 Library_DecodeASI(USB_ConfigDescr* cfg, USB_InterfaceDescr* iface,
                       Mic* mic)
{
    UA_CommonDescr* hdr;
    UA_CommonDescr* fmt;
    USB_CommonDescr* endpt;
    u32 cfgLen;
    u32 offset;

    cfgLen = (u32)((cfg->wTotalLength & 0xff) << 8 |
                   (u32)(cfg->wTotalLength >> 8));

    endpt = ScanDescriptor((USB_CommonDescr*)iface, 4, (USB_CommonDescr*)cfg, cfgLen);
    if (endpt) {
        offset = (u32)endpt - (u32)cfg;
    } else {
        offset = cfgLen;
    }

    hdr = ScanUADescriptor((UA_CommonDescr*)iface, 0x24, 1,
                            (UA_CommonDescr*)cfg, offset);
    if (!hdr) {
        return 0;
    }

    if (*(signed char*)((char*)hdr + 3) != *(signed char*)((char*)mic + 100)) {
        return 0;
    }

    if (*(short*)((char*)hdr + 5) != 0x0100) {
        return 0;
    }

    fmt = ScanUADescriptor(hdr, 0x24, 2, (UA_CommonDescr*)cfg, offset);
    if (!fmt) {
        return 0;
    }

    if (*(signed char*)((char*)fmt + 3) != 1 ||
        *(signed char*)((char*)fmt + 4) != 1 ||
        *(signed char*)((char*)fmt + 5) != 2 ||
        *(signed char*)((char*)fmt + 6) != 16) {
        return 0;
    }

    if (*(u8*)((char*)fmt + 7) == 0) {
        u32 minRate = *(u8*)((char*)fmt + 8) ^ 8000;
        u32 maxRate = *(u8*)((char*)fmt + 0xb) ^ 48000;

        if (*(signed char*)((char*)fmt + 10) != 0 ||
            *(signed char*)((char*)fmt + 9) != 0 ||
            (int)(((int)minRate >> 1) - (minRate & *(u8*)((char*)fmt + 8))) < 0) {
            return 0;
        }
        if (*(signed char*)((char*)fmt + 0xd) != 0 ||
            *(signed char*)((char*)fmt + 0xc) != 0 ||
            (int)(((int)maxRate >> 1) - (maxRate & 48000)) < 0) {
            return 0;
        }
    } else {
        u8  n = *(u8*)((char*)fmt + 7);
        u32 found = 0;
        u8  i;

        for (i = 0; i < n; i++) {
            u8* p = (u8*)fmt + i * 3;
            u32 rate = (u32)p[10] << 16 | (u32)p[9] << 8 | (u32)p[8];

            if (rate == 0x5622) {
                found |= 4;
            } else if (rate < 0x5622) {
                if (rate == 0x2b11) {
                    found |= 2;
                } else if (rate < 0x2b11 && rate == 8000) {
                    found |= 1;
                }
            } else if (rate == 48000) {
                found |= 0x10;
            } else if (rate < 48000 && rate == 0xac44) {
                found |= 8;
            }
        }
        if (found != 0x1f) {
            return 0;
        }
    }

    endpt = ScanDescriptor((USB_CommonDescr*)iface, 5,
                            (USB_CommonDescr*)cfg, cfgLen);
    do {
        if (!endpt) {
            return 0;
        }
    } while (((*(u8*)((char*)endpt + 3) & 3) != 1) ||
             ((*(u8*)((char*)endpt + 2) & 0x80) != 0x80));

    *(u8*)((char*)mic + 0x68) = *(u8*)((char*)endpt + 2);
    *(u16*)((char*)mic + 0x66) = (u16)(*(u16*)((char*)endpt + 4) >> 8 |
                                        *(u16*)((char*)endpt + 4) << 8);
    *(u8*)((char*)mic + 0x69) = *(u8*)((char*)iface + 2);
    *(u8*)((char*)mic + 0x6a) = *(u8*)((char*)iface + 3);

    return 1;
}

s32 Library_AnalyzeDescriptor(USB_ConfigDescr* cfg, Mic* mic)
{
    USB_InterfaceDescr* iface;
    u16 cfgLen = (u16)(cfg->wTotalLength << 8 | cfg->wTotalLength >> 8);
    s32 ret;

    iface = (USB_InterfaceDescr*)ScanDescriptor(NULL, 4,
                                                 (USB_CommonDescr*)cfg, cfgLen);
    do {
        if (!iface) {
            return 0;
        }

        if (*(signed char*)((char*)iface + 5) == 1) {
            u8 subclass = *(u8*)((char*)iface + 6);
            if (subclass == 2) {
                ret = Library_DecodeASI(cfg, iface, mic);
                if (ret) {
                    return 1;
                }
            } else if (subclass < 2 && subclass != 0) {
                *(u8*)((char*)mic + 0x60) = *(u8*)((char*)iface + 2);
                ret = Library_DecodeACI(cfg, iface, mic);
                if (!ret) {
                    return 0;
                }
            }
        }

        iface = (USB_InterfaceDescr*)ScanDescriptor(
            (USB_CommonDescr*)iface, 4, (USB_CommonDescr*)cfg, cfgLen);
    } while (1);
}

void Library_OnGetDescriptorDone(s32 result, void* arg)
{
    Mic* mic = (Mic*)arg;
    BOOL failed = TRUE;
    s32 ret;

    if (result >= 0) {
        unsigned int step = g_lib.descStep + 1;
        void* buf = g_lib.descBuf;
        g_lib.descStep = step;
        if (step == 1) {
            u16 totalLen;
            u16 raw = *(u16*)((char*)buf + 2);
            totalLen = (u16)((raw >> 8) | ((raw & 0xff) << 8));
            if (totalLen <= 0x200) {
                ret = StartGetDescriptor(*(s32*)((char*)mic + 8), 2,
                                         buf, totalLen,
                                         Library_OnGetDescriptorDone, mic);
                if (ret >= 0) {
                    failed = FALSE;
                }
            }
        } else if (step == 2) {
            ret = Library_AnalyzeDescriptor((USB_ConfigDescr*)buf, mic);
            if (ret) {
                if (*(u8*)((char*)mic + 0x61) == 0 ||
                    *(u8*)((char*)mic + 0x62) == 0xff) {
                    g_lib.descStep = 4;
                    ret = StartSetInterface(*(s32*)((char*)mic + 8),
                                            *(u8*)((char*)mic + 0x69),
                                            *(u8*)((char*)mic + 0x6a),
                                            Library_OnSetInterfaceDone, mic);
                    if (ret >= 0) {
                        failed = FALSE;
                    }
                } else {
                    ret = StartGetMinMaxReq(*(s32*)((char*)mic + 8),
                                            0x82, 2,
                                            *(u8*)((char*)mic + 0x62),
                                            *(u8*)((char*)mic + 0x61),
                                            *(u8*)((char*)mic + 0x60),
                                            g_lib.descBuf, 2,
                                            Library_OnGetVolDone, mic);
                    if (ret >= 0) {
                        failed = FALSE;
                    }
                }
            }
        }
    }

    if (failed) {
        Mic_DecOutstandingRequests(mic);
        DPCEntry entry = kHotplugEntry;
        DPC_Queue(&g_lib.dpc, &entry);
    }
}

void Library_OnFakeCloseDone(s32 result, void* arg)
{
}

s32 Library_OnFakeRemove(s32 result, void* arg)
{
    IUSB_CloseDeviceAsync((s32)arg, (USBCallback)Library_OnFakeCloseDone, 0);
    return 0;
}

void Library_OnFakeOpenDone(s32 result, void* arg)
{
    if (result >= 0) {
        IUSB_DeviceRemovalNotifyAsync(result, (USBCallback)Library_OnFakeRemove,
                                      (void*)result);
    }

    {
        DPCEntry entry = kHotplugEntry;
        DPC_Queue(&g_lib.dpc, &entry);
    }
}

void Library_OnOpenDone(s32 result, void* arg)
{
    Mic* mic = (Mic*)arg;

    *(s32*)((char*)mic + 8) = result;

    if (result >= 0) {
        s32 ret;
        IUSB_DeviceRemovalNotifyAsync(result, (USBCallback)Library_OnRemove, mic);
        g_lib.descStep = 0;
        ret = StartGetDescriptor(*(s32*)((char*)mic + 8), 2,
                                  g_lib.descBuf, 9,
                                  Library_OnGetDescriptorDone, mic);
        if (ret < 0) {
            Mic_DecOutstandingRequests(mic);
            DPCEntry entry = kHotplugEntry;
            DPC_Queue(&g_lib.dpc, &entry);
        }
    } else {
        Mic_DecOutstandingRequests(mic);
        if (g_lib.initialized) {
            *(s32*)mic = 3;
            Mic_DecOutstandingRequests(mic);
        }
        DPCEntry entry = kHotplugEntry;
        DPC_Queue(&g_lib.dpc, &entry);
    }
}

void Library_OnAttach(s32 result, void* arg)
{
    Mic* freeMic;
    BOOL failed = TRUE;
    int i;

    if (!g_lib.initialized) {
        return;
    }

    if (result == 0) {
        g_lib.insertNotifyHandle = 0;

        /* find a free Mic slot (check mics[i].field0 for availability) */
        freeMic = NULL;
        i = 0;
        do {
            if (*(s32*)((char*)&g_lib + i * 0x74 + 0x24) == 0) {
                *(s32*)((char*)&g_lib + i * 0x74 + 0x24) = 1;
                freeMic = (Mic*)((char*)&g_lib + i * 0x74 + 0x24);
                break;
            }
            i++;
        } while (i < 4);

        if (freeMic == NULL) {
            IUSB_OpenDeviceIdsAsync(s_devPath, 0x046d, 0x0a03,
                                    (USBCallback)Library_OnFakeOpenDone, 0);
            failed = FALSE;
        } else {
            s32 ret;
            Mic_Initialize(freeMic);
            *(s32*)freeMic = 1;
            Mic_IncOutstandingRequests(freeMic);
            ret = IUSB_OpenDeviceIdsAsync(s_devPath, 0x046d, 0x0a03,
                                          (USBCallback)Library_OnOpenDone, freeMic);
            if (ret >= 0) {
                failed = FALSE;
            } else {
                Mic_DecOutstandingRequests(freeMic);
                Mic_DeInitialize(freeMic);
                *(s32*)freeMic = 0;
            }
        }

        if (failed) {
            DPCEntry entry = kHotplugEntry;
            DPC_Queue(&g_lib.dpc, &entry);
        }
    }
}

/* -------------------------------------------------------------------------
 * Memory helpers (in namespace usbmic)
 * ---------------------------------------------------------------------- */

void* Library_malloc(unsigned long size, unsigned long align)
{
    if (g_lib.mallocFn) {
        return g_lib.mallocFn(size, align);
    }
    return 0;
}

void Library_free(void* ptr)
{
    if (g_lib.freeFn) {
        g_lib.freeFn(ptr);
    }
}

/* -------------------------------------------------------------------------
 * Mic lookup
 * ---------------------------------------------------------------------- */

Mic* Library_LookupMicByIndex(long index)
{
    Mic* ret;

    if ((unsigned long)index > 3U) {
        ret = 0;
    } else {
        unsigned int* ptr = (unsigned int*)((char*)&g_lib + index * 0x74);
        unsigned int state = *((unsigned int*&)ptr)++;   /* lwzu: load state, advance ptr to &mics[i] */
        Mic* mic = (Mic*)ptr;

        if (state != 2) {
            ret = 0;
        } else {
            u16* timeout = (u16*)((char*)mic + 0x12);

            if (*timeout == 0) {
                ret = mic;
            } else {
                s32  now    = OSGetTick();
                u32  busClk = OS_BUS_CLOCK_SPEED;
                u32  ticksPerMs = (busClk / 4) / 1000;
                u32  diff   = (u32)(now - *(s32*)((char*)mic + 0x14));
                u32  elapsed = diff / ticksPerMs;
                u16  tv = *timeout;

                if (elapsed > (u32)tv) {
                    *timeout = 0;
                } else {
                    *timeout = tv - (u16)elapsed;
                }
                *(s32*)((char*)mic + 0x14) = now;
                ret = 0;
            }
        }
    }
    return ret;
}

/* -------------------------------------------------------------------------
 * Library Initialize / DeInitialize
 * ---------------------------------------------------------------------- */

s32 Library_Initialize(MallocFn mallocFn, FreeFn freeFn)
{
    if (!g_lib.initialized) {
        OSRegisterVersion(__LOGITECH_MICVersion);

        if (mallocFn != NULL && freeFn != NULL) {
            int j;
            int i;
            memset(&g_lib, 0, sizeof(Library));
            g_lib.mallocFn = mallocFn;
            g_lib.freeFn   = freeFn;
            g_lib.descBuf  = mallocFn(0x200, 0x20);

            if (g_lib.descBuf != NULL) {
                j = 0;
                i = 0;
                do {
                    Mic_Initialize((Mic*)((char*)&g_lib + i + 0x24));
                    j++;
                    i += 0x74;
                } while (j < 4);

                DPC_Initialize(&g_lib.dpc);
                OSInitMutex(&g_lib.mutex);

                if (IUSB_OpenLib() >= 0) {
                    static unsigned int insertNotifyId;
                    g_lib.insertNotifyHandle = 0;
                    insertNotifyId = 0;
                    g_lib.initialized = TRUE;
                    IUSB_RegisterInsertionNotifyWithIdAsync(s_devPath, 0x046d, 0x0a03,
                                                             0, (USBCallback)Library_OnAttach,
                                                             &insertNotifyId, 0);
                    g_lib.insertNotifyHandle = insertNotifyId;
                    return 0;
                }
                return -6;
            }
            return -3;
        }
        return -2;
    }
    return -4;
}

s32 Library_DeInitialize(void)
{
    if (g_lib.initialized) {
        int i;
        Mic* mic;

        g_lib.initialized = FALSE;

        mic = (Mic*)((char*)&g_lib + 0x24);
        i = 0;

        do {
            while (*(signed char*)((char*)mic + 0x10) > 1) {
                u32 busClk = OS_BUS_CLOCK_SPEED;
                OSSleepTicks((s64)(busClk / 4 / 1000));
            }

            Mic_Close(mic);
            IUSB_CloseDevice(*(s32*)((char*)mic + 8));
            Mic_DeInitialize(mic);
            *(s32*)mic = 0;

            i++;
            mic = (Mic*)((char*)mic + 0x74);
        } while (i < 4);

        DPC_Deinitialize(&g_lib.dpc);

        if (g_lib.insertNotifyHandle) {
            IUSB_CancelInsertionNotify(s_devPath, g_lib.insertNotifyHandle);
            g_lib.insertNotifyHandle = 0;
        }

        IUSB_CloseLib();

        if (g_lib.descBuf) {
            if (g_lib.freeFn) {
                g_lib.freeFn(g_lib.descBuf);
            }
            g_lib.descBuf = 0;
        }

        return 0;
    }
    return -5;
}

} // namespace usbmic

/* =========================================================================
 * Public C-linkage mic* functions (outside namespace)
 * ======================================================================= */

using usbmic::Library_Initialize;
using usbmic::Library_DeInitialize;
using usbmic::Library_LookupMicByIndex;
using usbmic::g_lib;
using usbmic::Mic;
using usbmic::DPC_Process;
using usbmic::Mic_GetDescription;
using usbmic::Mic_Open;
using usbmic::Mic_Close;
using usbmic::Mic_Read;
using usbmic::Mic_SetVolume;
using usbmic::Mic_SetMute;
using usbmic::micDesc;

extern "C" {

s32 micInit(usbmic::MallocFn mallocFn, usbmic::FreeFn freeFn)
{
    return Library_Initialize(mallocFn, freeFn);
}

s32 micShutdown(void)
{
    if (!g_lib.initialized) {
        return -5;
    }
    return Library_DeInitialize();
}

s32 micProbe(s32 index, micDesc* desc)
{
    s32 ret;
    Mic* mic;

    if (!g_lib.initialized) {
        return -5;
    }

    OSLockMutex(&g_lib.mutex);
    DPC_Process(&g_lib.dpc);

    ret = -1;
    mic = Library_LookupMicByIndex(index);
    if (mic) {
        ret = Mic_GetDescription(mic, desc);
    }

    OSUnlockMutex(&g_lib.mutex);
    return ret;
}

static s32 Library_CreateClientHandle(long index)
{
    static unsigned int counter;
    unsigned int c1 = counter + 1;
    unsigned int idx = (unsigned int)index & 3;
    counter = c1 & 0xFFFFFF;
    return (s32)(idx | (c1 << 8));
}

s32 micOpen(u32 index, micOpenParam* param)
{
    s32 ret;
    Mic* mic;

    if (!g_lib.initialized) {
        return -5;
    }

    OSLockMutex(&g_lib.mutex);
    DPC_Process(&g_lib.dpc);

    ret = -1;
    mic = Library_LookupMicByIndex((long)index);
    if (mic) {
        Mic* slot = (Mic*)((char*)&g_lib + index * 0x74 + 0x24);
        if (*(s32*)((char*)slot + 4) != 0) goto open_null;
        *(s32*)((char*)slot + 0xc) = Library_CreateClientHandle((long)index);
        goto open_keep;
open_null:
        slot = NULL;
open_keep:
        if (slot) {
            ret = Mic_Open(slot, param);
        } else {
            ret = -7;
        }
    }

    OSUnlockMutex(&g_lib.mutex);
    return ret;
}

s32 micClose(u32 handle)
{
    s32 ret;

    if (!g_lib.initialized) {
        return -5;
    }

    OSLockMutex(&g_lib.mutex);
    DPC_Process(&g_lib.dpc);

    ret = -1;
    {
        Mic* mic = (Mic*)((char*)&g_lib + (handle & 3) * 0x74 + 0x24);
        if (*(s32*)((char*)mic + 4) == 0) goto close_null;
        if ((s32)handle != *(s32*)((char*)mic + 0xc)) goto close_null;
        goto close_check;
close_null:
        mic = NULL;
close_check:
        if (mic)
            ret = Mic_Close(mic);
    }

    OSUnlockMutex(&g_lib.mutex);
    return ret;
}

s32 micRead(u32 handle, void* buf, u32* size)
{
    s32 ret;

    if (!g_lib.initialized) {
        return -5;
    }

    OSLockMutex(&g_lib.mutex);
    DPC_Process(&g_lib.dpc);

    ret = -1;
    {
        Mic* mic = (Mic*)((char*)&g_lib + (handle & 3) * 0x74 + 0x24);
        if (*(s32*)((char*)mic + 4) == 0) goto read_null;
        if ((s32)handle != *(s32*)((char*)mic + 0xc)) goto read_null;
        goto read_check;
read_null:
        mic = NULL;
read_check:
        if (mic)
            ret = Mic_Read(mic, buf, size);
    }

    OSUnlockMutex(&g_lib.mutex);
    return ret;
}

s32 micSetVolume(u32 handle, u16 volume)
{
    s32 ret;

    if (!g_lib.initialized) {
        return -5;
    }

    OSLockMutex(&g_lib.mutex);
    DPC_Process(&g_lib.dpc);

    ret = -1;
    {
        Mic* mic = (Mic*)((char*)&g_lib + (handle & 3) * 0x74 + 0x24);
        if (*(s32*)((char*)mic + 4) == 0) goto vol_null;
        if ((s32)handle != *(s32*)((char*)mic + 0xc)) goto vol_null;
        goto vol_check;
vol_null:
        mic = NULL;
vol_check:
        if (mic)
            ret = Mic_SetVolume(mic, volume);
    }

    OSUnlockMutex(&g_lib.mutex);
    return ret;
}

s32 micSetMute(u32 handle, u32 mute)
{
    s32 ret;

    if (!g_lib.initialized) {
        return -5;
    }

    OSLockMutex(&g_lib.mutex);
    DPC_Process(&g_lib.dpc);

    ret = -1;
    {
        Mic* mic = (Mic*)((char*)&g_lib + (handle & 3) * 0x74 + 0x24);
        if (*(s32*)((char*)mic + 4) == 0) goto mute_null;
        if ((s32)handle != *(s32*)((char*)mic + 0xc)) goto mute_null;
        goto mute_check;
mute_null:
        mic = NULL;
mute_check:
        if (mic)
            ret = Mic_SetMute(mic, (-mute | mute) >> 31);
    }

    OSUnlockMutex(&g_lib.mutex);
    return ret;
}

} // extern "C"
