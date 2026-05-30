#include "types.h"

s32 st_uhs_ker_sem_status;
u8 st_uhs_ker_sem_mng[0xA4];
u8 st_uhf_ker_sem[0x90];

s32 uhf_ker_create_sem(void *sem, s32 init, s32 max) {
    (void)sem; (void)init; (void)max;
    return 0;
}

s32 uhf_ker_delete_sem(void *sem) {
    (void)sem;
    return 0;
}

s32 uhf_ker_get_sem(void *sem, s32 timeout) {
    (void)sem; (void)timeout;
    return 0;
}
