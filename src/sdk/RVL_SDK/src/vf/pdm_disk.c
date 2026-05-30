#include "types.h"

s32 VFipdm_disk_get_handle(s32 driveIdx, void **handle) {
    (void)driveIdx; (void)handle;
    return 0;
}

s32 VFipdm_disk_do_get_permission(void *disk, s32 mode) {
    (void)disk; (void)mode;
    return 0;
}

s32 VFipdm_disk_check_disk_handle(void *disk) {
    (void)disk;
    return 0;
}

s32 VFipdm_disk_open_disk(s32 driveIdx, void **handle) {
    (void)driveIdx; (void)handle;
    return 0;
}

s32 VFipdm_disk_close_disk(void *disk) {
    (void)disk;
    return 0;
}

s32 VFipdm_disk_get_part_permission(void *disk, s32 partIdx, s32 mode) {
    (void)disk; (void)partIdx; (void)mode;
    return 0;
}

s32 VFipdm_disk_release_part_permission(void *disk, s32 partIdx) {
    (void)disk; (void)partIdx;
    return 0;
}

s32 VFipdm_disk_physical_read(void *disk, u32 sector, u32 count, void *buf) {
    (void)disk; (void)sector; (void)count; (void)buf;
    return 0;
}

s32 VFipdm_disk_physical_write(void *disk, u32 sector, u32 count, const void *buf) {
    (void)disk; (void)sector; (void)count; (void)buf;
    return 0;
}

s32 VFipdm_disk_physical_erase(void *disk, u32 sector, u32 count) {
    (void)disk; (void)sector; (void)count;
    return 0;
}

s32 VFipdm_disk_format(void *disk, void *param) {
    (void)disk; (void)param;
    return 0;
}

s32 VFipdm_disk_get_lba_size(void *disk, u32 *size) {
    (void)disk; (void)size;
    return 0;
}

s32 VFipdm_disk_get_media_information(void *disk, void *info) {
    (void)disk; (void)info;
    return 0;
}

s32 VFipdm_disk_check_media_insert(void *disk) {
    (void)disk;
    return 0;
}

s32 VFipdm_disk_check_data_erase(void *disk) {
    (void)disk;
    return 0;
}

s32 VFipdm_disk_set_disk(s32 driveIdx, void *drv) {
    (void)driveIdx; (void)drv;
    return 0;
}

s32 VFipdm_disk_notify_media_insert(void *disk, s32 inserted) {
    (void)disk; (void)inserted;
    return 0;
}
