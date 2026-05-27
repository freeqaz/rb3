#ifndef AX_AXOUT_H
#define AX_AXOUT_H

#include "types.h"

typedef void (*AXFrameCallback)(void);
typedef void (*AXExceedCallback)(void);

void __AXOutInit(int param);
void __AXOutQuit();
AXFrameCallback AXRegisterCallback(AXFrameCallback callback);

#endif
