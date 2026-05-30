#include "types.h"

u8 __data[0x80];

static void keyboard_read_callback(void *p0, void *p1) {
    (void)p0; (void)p1;
}

static void set_protocol_callback(void *p0, void *p1) {
    (void)p0; (void)p1;
}

static void set_idle_callback(void *p0, void *p1) {
    (void)p0; (void)p1;
}
