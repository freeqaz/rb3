#include "types.h"

s32 CBGetBytesAvailableForRead(void *cb) {
    (void)cb;
    return 0;
}

void CircleBufferInitialize(void *cb, void *buf, s32 size) {
    (void)cb; (void)buf; (void)size;
}

s32 CircleBufferWriteBytes(void *cb, const void *data, s32 size) {
    (void)cb; (void)data; (void)size;
    return 0;
}

s32 CircleBufferReadBytes(void *cb, void *data, s32 size) {
    (void)cb; (void)data; (void)size;
    return 0;
}
