#include "types.h"

u8 st_uhs_ker_msg_mng[0x14];

s32 uhf_ker_send_message(void *queue, void *msg) {
    (void)queue; (void)msg;
    return 0;
}
