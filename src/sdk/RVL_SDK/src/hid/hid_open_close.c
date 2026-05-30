#include "types.h"

s32 __HIDVersion = 0;
void *__hid_mem;

static void hid_ios_version_callback(void *p0, void *p1) {
    (void)p0; (void)p1;
}

static void hid_ios_open_callback(void *p0, void *p1) {
    (void)p0; (void)p1;
}

s32 hid_open(void *p0, void *p1) {
    (void)p0; (void)p1;
    return 0;
}

static void hid_ios_close_callback(void *p0, void *p1) {
    (void)p0; (void)p1;
}

static void hid_close_attach_callback(void *p0, void *p1) {
    (void)p0; (void)p1;
}

s32 hid_close(void *p0) {
    (void)p0;
    return 0;
}
