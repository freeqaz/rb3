#include "types.h"

s32 hid_ios_send_message_get_attach(void *p0, void *p1) {
    (void)p0; (void)p1;
    return 0;
}

static void hid_attach_callback(void *p0, void *p1) {
    (void)p0; (void)p1;
}

void hid_set_suspend_callback(void *p0) {
    (void)p0;
}

s32 hid_set_suspend(void *p0, void *p1) {
    (void)p0; (void)p1;
    return 0;
}

static void hid_async_callback_ctrl(void *p0, void *p1) {
    (void)p0; (void)p1;
}

s32 hid_set_report(void *p0, void *p1, void *p2, void *p3, void *p4) {
    (void)p0; (void)p1; (void)p2; (void)p3; (void)p4;
    return 0;
}

s32 hid_get_report(void *p0, void *p1, void *p2, void *p3, void *p4) {
    (void)p0; (void)p1; (void)p2; (void)p3; (void)p4;
    return 0;
}

static void hid_async_callback_protocol(void *p0, void *p1) {
    (void)p0; (void)p1;
}

s32 hid_set_protocol(void *p0, void *p1, void *p2) {
    (void)p0; (void)p1; (void)p2;
    return 0;
}

s32 hid_set_idle(void *p0, void *p1, void *p2) {
    (void)p0; (void)p1; (void)p2;
    return 0;
}

static void hid_async_callback_intr(void *p0, void *p1) {
    (void)p0; (void)p1;
}

s32 hid_get_intr(void *p0, void *p1, void *p2, void *p3) {
    (void)p0; (void)p1; (void)p2; (void)p3;
    return 0;
}

void hid_set_cancel_intr_callback(void *p0) {
    (void)p0;
}

s32 hid_cancel_intr(void *p0) {
    (void)p0;
    return 0;
}
