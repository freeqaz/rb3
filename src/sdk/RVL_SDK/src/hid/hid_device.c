#include "types.h"

void hid_init_device_module(void) {
}

void hid_device_detach(void *p0) {
    (void)p0;
}

s32 hid_device_attach(void *p0, void *p1) {
    (void)p0; (void)p1;
    return 0;
}

void hid_device_manage_change(void *p0) {
    (void)p0;
}

void hid_device_adopt_orphans(void *p0) {
    (void)p0;
}
