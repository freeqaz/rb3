#include "types.h"

void *p_klc_free;
u8 klc[0x200];

void kbd_free_led(void *p0) {
    (void)p0;
}

void *kbd_alloc_led(void) {
    return 0;
}

void kbd_initialize_led_module(void) {
}
