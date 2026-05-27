#ifndef RVL_SDK_GX_MISC_H
#define RVL_SDK_GX_MISC_H
#include "revolution/gx/GXTypes.h"
#include "types.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef void (*GXDrawDoneCallback)(void);
typedef void (*GXDrawSyncCallback)(u16);

void GXSetMisc(u32 token, u32 val);
void GXFlush(void);
void GXResetWriteGatherPipe(void);

void GXAbortFrame(void);

void GXDrawDone(void);
void GXPixModeSync(void);
void GXSetDrawSync(u16 token);
void GXSetDrawDone(void);

GXDrawSyncCallback GXSetDrawSyncCallback(GXDrawSyncCallback cb);
GXDrawDoneCallback GXSetDrawDoneCallback(GXDrawDoneCallback cb);

void GXPokeAlphaMode(GXCompare func, u8 threshold);
void GXPokeAlphaRead(u32 type);
void GXPokeAlphaUpdate(GXBool enable);
void GXPokeBlendMode(GXBlendMode type, GXBlendFactor srcFactor, GXBlendFactor dstFactor, GXLogicOp op);
void GXPokeColorUpdate(GXBool enable);
void GXPokeDstAlpha(GXBool enable, u8 alpha);
void GXPokeDither(GXBool dither);
void GXPokeZMode(GXBool enable, GXCompare func, GXBool update);
void GXPeekZ(u32 x, u32 y, u32 *z);

void __GXPEInit(void);

#ifdef __cplusplus
}
#endif
#endif
