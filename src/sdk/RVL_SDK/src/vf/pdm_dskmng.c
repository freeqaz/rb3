#include "types.h"

u8 VFipdm_disk_set[0xBCC];

s32 VFipdm_init_diskmanager(void) {
    return 0;
}

s32 VFipdm_open_disk(s32 driveIdx) {
    (void)driveIdx;
    return 0;
}

s32 VFipdm_close_disk(void *disk) {
    (void)disk;
    return 0;
}

s32 VFipdm_open_partition(void *disk, s32 partIdx) {
    (void)disk; (void)partIdx;
    return 0;
}

s32 VFipdm_close_partition(void *part) {
    (void)part;
    return 0;
}
