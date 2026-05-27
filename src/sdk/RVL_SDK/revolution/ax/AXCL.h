#ifndef AX_AXCL_H
#define AX_AXCL_H

#include "types.h"

void __AXClInit();
void __AXClQuit();
u32 __AXGetCommandListAddress();
void __AXNextFrame(void* sbuffer, void* outbuffer, void* rmtbuffers);

#endif
