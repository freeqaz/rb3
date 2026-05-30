#include "types.h"

#define TRK_MSG_BUF_COUNT  6
#define TRK_MSG_BUF_SIZE   0x440

typedef struct TRKBuffer {
    s32 id;
    s32 length;
    s32 position;
    s32 maxLength;
    u8 data[TRK_MSG_BUF_SIZE];
} TRKBuffer;

typedef struct TRKMsgBufs {
    TRKBuffer bufs[TRK_MSG_BUF_COUNT];
    s32 initialized;
    s32 freeList;
} TRKMsgBufs;

TRKMsgBufs gTRKMsgBufs;

s32 TRK_InitializeMessageBuffers(void) {
    s32 i;
    gTRKMsgBufs.initialized = 1;
    gTRKMsgBufs.freeList = 0;
    for (i = 0; i < TRK_MSG_BUF_COUNT; i++) {
        gTRKMsgBufs.bufs[i].id = i;
        gTRKMsgBufs.bufs[i].length = 0;
        gTRKMsgBufs.bufs[i].position = 0;
        gTRKMsgBufs.bufs[i].maxLength = TRK_MSG_BUF_SIZE;
    }
    return 0;
}

s32 TRK_GetFreeBuffer(s32 *bufId, void **buf) {
    s32 i;
    for (i = 0; i < TRK_MSG_BUF_COUNT; i++) {
        if (gTRKMsgBufs.bufs[i].id >= 0) {
            *bufId = i;
            *buf = &gTRKMsgBufs.bufs[i];
            return 0;
        }
    }
    return -1;
}

void *TRKGetBuffer(s32 bufId) {
    if (bufId < 0 || bufId >= TRK_MSG_BUF_COUNT)
        return NULL;
    return &gTRKMsgBufs.bufs[bufId];
}

s32 TRK_ReleaseBuffer(s32 bufId) {
    if (bufId < 0 || bufId >= TRK_MSG_BUF_COUNT)
        return -1;
    gTRKMsgBufs.bufs[bufId].length = 0;
    gTRKMsgBufs.bufs[bufId].position = 0;
    return 0;
}

s32 TRKResetBuffer(void *buf, s32 full) {
    TRKBuffer *b = (TRKBuffer *)buf;
    b->position = 0;
    if (full)
        b->length = 0;
    return 0;
}

s32 TRK_SetBufferPosition(void *buf, s32 pos) {
    TRKBuffer *b = (TRKBuffer *)buf;
    if (pos < 0 || pos > b->length)
        return -1;
    b->position = pos;
    return 0;
}

s32 TRK_AppendBuffer(void *buf, const void *data, s32 len) {
    TRKBuffer *b = (TRKBuffer *)buf;
    const u8 *src = (const u8 *)data;
    s32 i;
    if (b->length + len > b->maxLength)
        return -1;
    for (i = 0; i < len; i++) {
        b->data[b->length++] = src[i];
    }
    return 0;
}

s32 TRK_ReadBuffer(void *buf, void *data, s32 len) {
    TRKBuffer *b = (TRKBuffer *)buf;
    u8 *dst = (u8 *)data;
    s32 i;
    if (b->position + len > b->length)
        return -1;
    for (i = 0; i < len; i++) {
        dst[i] = b->data[b->position++];
    }
    return 0;
}

s32 TRKAppendBuffer1_ui32(void *buf, u32 val) {
    u8 tmp[4];
    tmp[0] = (u8)(val >> 24);
    tmp[1] = (u8)(val >> 16);
    tmp[2] = (u8)(val >> 8);
    tmp[3] = (u8)(val);
    return TRK_AppendBuffer(buf, tmp, 4);
}

s32 TRKAppendBuffer1_ui64(void *buf, u32 hi, u32 lo) {
    s32 r;
    r = TRKAppendBuffer1_ui32(buf, hi);
    if (r != 0) return r;
    return TRKAppendBuffer1_ui32(buf, lo);
}

s32 TRKAppendBuffer_ui8(void *buf, u8 val) {
    return TRK_AppendBuffer(buf, &val, 1);
}

s32 TRKAppendBuffer_ui32(void *buf, u32 val) {
    return TRKAppendBuffer1_ui32(buf, val);
}

s32 TRKReadBuffer1_ui64(void *buf, u32 *hi, u32 *lo) {
    u8 tmp[8];
    s32 r = TRK_ReadBuffer(buf, tmp, 8);
    if (r != 0) return r;
    *hi = ((u32)tmp[0] << 24) | ((u32)tmp[1] << 16) | ((u32)tmp[2] << 8) | tmp[3];
    *lo = ((u32)tmp[4] << 24) | ((u32)tmp[5] << 16) | ((u32)tmp[6] << 8) | tmp[7];
    return 0;
}

s32 TRKReadBuffer_ui8(void *buf, u8 *val) {
    return TRK_ReadBuffer(buf, val, 1);
}

s32 TRKReadBuffer_ui32(void *buf, u32 *val) {
    u8 tmp[4];
    s32 r = TRK_ReadBuffer(buf, tmp, 4);
    if (r != 0) return r;
    *val = ((u32)tmp[0] << 24) | ((u32)tmp[1] << 16) | ((u32)tmp[2] << 8) | tmp[3];
    return 0;
}
