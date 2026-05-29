#ifndef AX_AXCL_H
#define AX_AXCL_H

#include "types.h"

void __AXClInit();
void __AXClQuit();
u16 *__AXGetCommandListAddress(void);
void __AXNextFrame(u32 param_1, u32 param_2, u32 *param_3);

#endif
