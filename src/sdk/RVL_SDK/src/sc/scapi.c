#include <revolution/sc/scapi.h>
#include <revolution/sc/scapi_prdinfo.h>
#include <revolution/sc/scsystem.h>
#include <revolution/OS.h>
#include <string.h>

static u8 TempSimpleAddress[0x1008];

u8 SCGetAspectRatio(void) {
    u8 val;
    if (!SCFindU8Item(&val, SC_ITEM_IPL_AR)) {
        val = 0;
    } else {
        if (val != 1) {
            val = 0;
        }
    }
    return val;
}

s8 SCGetDisplayOffsetH(void) {
    s8 val;
    if (!SCFindS8Item(&val, SC_ITEM_IPL_DH)) {
        val = 0;
    } else {
        if (val < -0x20) {
            val = -0x20;
        } else if (val > 0x20) {
            val = 0x20;
        }
    }
    val &= ~1;
    return val;
}

void SCGetIdleMode(SCIdleMode *mode) {
    SCFindByteArrayItem(mode, sizeof(SCIdleMode), SC_ITEM_IPL_IDL);
}

u8 SCGetLanguage(void) {
    u8 val;
    if (!SCFindU8Item(&val, SC_ITEM_IPL_LNG)) {
        if ((s8)SCGetProductArea() == 0) {
            val = 0;
        } else {
            val = 1;
        }
    } else {
        if (val > 9) {
            val = 1;
        }
    }
    return val;
}

void SCGetParentalControl(void *dst) {
    SCFindByteArrayItem(dst, 0x4a, SC_ITEM_IPL_PC);
}

u8 SCGetScreenSaverMode(void) {
    u8 val;
    if (!SCFindU8Item(&val, SC_ITEM_IPL_SSV)) {
        val = 1;
    } else {
        if (val != 1) {
            val = 0;
        }
    }
    return val;
}

u8 SCGetSoundMode(void) {
    u8 val;
    if (!SCFindU8Item(&val, SC_ITEM_IPL_SND)) {
        val = 1;
    } else {
        if (val > 2) {
            val = 1;
        }
    }
    return val;
}

u32 SCGetCounterBias(void) {
    u32 val;
    if (!SCFindU32Item(&val, SC_ITEM_IPL_CB)) {
        val = 0x0b49d800;
    }
    return val;
}

void SCGetBtDeviceInfoArray(SCBtDeviceInfoArray *array) {
    SCFindByteArrayItem(array, sizeof(SCBtDeviceInfoArray), SC_ITEM_BT_DINF);
}

BOOL SCSetBtDeviceInfoArray(const SCBtDeviceInfoArray *array) {
    return SCReplaceByteArrayItem(array, sizeof(SCBtDeviceInfoArray), SC_ITEM_BT_DINF);
}

void SCGetBtCmpDevInfoArray(SCBtCmpDevInfoArray *array) {
    SCFindByteArrayItem(array, sizeof(SCBtCmpDevInfoArray), SC_ITEM_BT_CDIF);
}

BOOL SCSetBtCmpDevInfoArray(const SCBtCmpDevInfoArray *array) {
    return SCReplaceByteArrayItem(array, sizeof(SCBtCmpDevInfoArray), SC_ITEM_BT_CDIF);
}

u32 SCGetBtDpdSensibility(void) {
    u32 val;
    if (!SCFindU32Item(&val, SC_ITEM_BT_SENS)) {
        val = 2;
    } else {
        if (val < 1) {
            val = 1;
        } else if (val > 5) {
            val = 5;
        }
    }
    return val;
}

u8 SCGetWpadMotorMode(void) {
    u8 val;
    if (!SCFindU8Item(&val, SC_ITEM_BT_MOT)) {
        val = 1;
    } else {
        if (val != 1) {
            val = 0;
        }
    }
    return val;
}

BOOL SCSetWpadMotorMode(u8 mode) {
    return SCReplaceU8Item(mode, SC_ITEM_BT_MOT);
}

u8 SCGetWpadSensorBarPosition(void) {
    u8 val;
    if (!SCFindU8Item(&val, SC_ITEM_BT_BAR)) {
        val = 0;
    } else {
        if (val != 1) {
            val = 0;
        }
    }
    return val;
}

u8 SCGetWpadSpeakerVolume(void) {
    u8 val;
    if (!SCFindU8Item(&val, SC_ITEM_BT_SPKV)) {
        val = 0x59;
    } else {
        if (val > 0x7f) {
            val = 0x7f;
        }
    }
    return val;
}

BOOL SCSetWpadSpeakerVolume(u8 vol) {
    return SCReplaceU8Item(vol, SC_ITEM_BT_SPKV);
}

u32 SCGetSimpleAddressID(int region) {
    BOOL ok;
    u32 val;
    if (SCFindByteArrayItem(TempSimpleAddress, 0x1008, SC_ITEM_IPL_SADR)) {
        val = *(u32*)TempSimpleAddress;
        if ((u32)(val + 0x10000) != 0xffff) {
            if (val & 0xff000000) {
                if ((u32)((val & 0xff000000) + 0x01000000) != 0u) {
                    if ((u32)((val & 0x00ff0000) - 0x00ff0000) != 0u) {
                        BOOL ints = OSDisableInterrupts();
                        u32 saved = *(u32*)TempSimpleAddress;
                        if (!(saved & 0x00ff0000)) {
                            memset(TempSimpleAddress, 0, 0x1008);
                            *(u32*)TempSimpleAddress = saved;
                        }
                        OSRestoreInterrupts(ints);
                        ok = TRUE;
                        goto done;
                    }
                }
            }
        }
    }
    ok = FALSE;
done:
    if (ok) {
        return *(u32*)TempSimpleAddress;
    }
    return (u32)-1;
}

BOOL SCGetSimpleAddressRegionIdHi(u8 *dst) {
    BOOL ok;
    u32 val;
    if (SCFindByteArrayItem(TempSimpleAddress, 0x1008, SC_ITEM_IPL_SADR)) {
        val = *(u32*)TempSimpleAddress;
        if ((u32)(val + 0x10000) != 0xffff) {
            if (val & 0xff000000) {
                if ((u32)((val & 0xff000000) + 0x01000000) != 0u) {
                    if ((u32)((val & 0x00ff0000) - 0x00ff0000) != 0u) {
                        BOOL ints = OSDisableInterrupts();
                        u32 saved = *(u32*)TempSimpleAddress;
                        if (!(saved & 0x00ff0000)) {
                            memset(TempSimpleAddress, 0, 0x1008);
                            *(u32*)TempSimpleAddress = saved;
                        }
                        OSRestoreInterrupts(ints);
                        ok = TRUE;
                        goto done;
                    }
                }
            }
        }
    }
    ok = FALSE;
done:
    if (ok) {
        *dst = (u8)(*(u32*)TempSimpleAddress >> 24);
        return TRUE;
    }
    return FALSE;
}

u32 SCGetNetContentRestrictions(void) {
    u32 val;
    if (!SCFindU32Item(&val, SC_ITEM_NET_CTPC)) {
        val = 0;
    }
    return val;
}

u32 SCGetEULA(void) {
    u32 val;
    if (!SCFindBoolItem(&val, SC_ITEM_IPL_EULA)) {
        val = 0;
    }
    return val;
}

void SCGetFreeChannelAppCount(u32 *count) {
    *count = 0;
    SCFindU32Item(count, SC_ITEM_IPL_FRC);
}

BOOL SCCheckPCMessageRestriction(void) {
    u32 val;
    OSReport("<< RVL_SDK - SCCheckPCMessageRestriction >>\n");
    if (!SCFindU32Item(&val, SC_ITEM_NET_CTPC)) {
        val = 0;
    }
    return (val >> 1) & 1;
}

BOOL SCCheckPCShoppingRestriction(void) {
    u32 val;
    if (!SCFindU32Item(&val, SC_ITEM_NET_CTPC)) {
        val = 0;
    }
    return (val >> 2) & 1;
}
