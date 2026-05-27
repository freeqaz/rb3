// Native shim for <revolution/cnt/cnt.h> (Wii content filesystem).
// os/ContentMgr_Wii.h only uses CNTHandle* (pointers); no CNT* functions run on
// the DTA-parse path. Forward-declare the handle type.
#pragma once
#ifdef HX_NATIVE

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _CNTHandle CNTHandle;

#ifdef __cplusplus
}
#endif

#endif // HX_NATIVE
