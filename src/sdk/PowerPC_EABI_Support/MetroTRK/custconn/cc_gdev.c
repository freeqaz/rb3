#include "types.h"

s32 gIsInitialized;
u8 gRecvBuf[500];
u8 gRecvCB[0x1C];

s32 gdev_cc_initialize(void *cb) {
    (void)cb;
    return 0;
}

s32 gdev_cc_shutdown(void) {
    return 0;
}

s32 gdev_cc_open(void *cb) {
    (void)cb;
    return 0;
}

s32 gdev_cc_close(void) {
    return 0;
}

s32 gdev_cc_read(void *buf, s32 *size) {
    (void)buf; (void)size;
    return 0;
}

s32 gdev_cc_write(const void *buf, s32 size) {
    (void)buf; (void)size;
    return 0;
}

s32 gdev_cc_pre_continue(void *cb) {
    (void)cb;
    return 0;
}

s32 gdev_cc_post_stop(void *cb) {
    (void)cb;
    return 0;
}

s32 gdev_cc_peek(void *buf, s32 *size) {
    (void)buf; (void)size;
    return 0;
}

s32 gdev_cc_initinterrupts(void *cb) {
    (void)cb;
    return 0;
}
