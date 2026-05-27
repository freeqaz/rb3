// Native shim for <revolution/ec/ec.h> (Wii e-Commerce).
// os/CommerceMgr_Wii.h only needs the ECTitleInfo layout (it defines its own
// ECLicensePricing/ECContentCatalogInfo). No EC_* functions run on the
// DTA-parse path.
#pragma once
#ifdef HX_NATIVE

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _ECTitleInfo {
    unsigned long long titleId;
    long isTmdPresent;
    long isOnDevice;
    unsigned long type;
    unsigned long version;
    unsigned long occupiedUserBlocks;
    unsigned long occupiedUserInodes;
    unsigned long occupiedSysBlocks;
    unsigned long occupiedSysInodes;
} ECTitleInfo;

typedef struct _ECNameValue {
    const char *name;
    void *value;
} ECNameValue;

#ifdef __cplusplus
}
#endif

#endif // HX_NATIVE
