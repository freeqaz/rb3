#include "types.h"

s32 VFipdm_part_get_start_sector(void *part, u32 *sector) {
    (void)part; (void)sector;
    return 0;
}

s32 VFipdm_part_convert_lsector_to_block(void *part, u32 lsector, u32 *block) {
    (void)part; (void)lsector; (void)block;
    return 0;
}

s32 VFipdm_part_get_handle(s32 driveIdx, s32 partIdx, void **handle) {
    (void)driveIdx; (void)partIdx; (void)handle;
    return 0;
}

s32 VFipdm_part_open_partition(s32 driveIdx, s32 partIdx, void **handle) {
    (void)driveIdx; (void)partIdx; (void)handle;
    return 0;
}

s32 VFipdm_part_close_partition(void *part) {
    (void)part;
    return 0;
}

s32 VFipdm_part_get_permission(void *part, s32 mode) {
    (void)part; (void)mode;
    return 0;
}

s32 VFipdm_part_release_permission(void *part) {
    (void)part;
    return 0;
}

s32 VFipdm_part_format(void *part, void *param) {
    (void)part; (void)param;
    return 0;
}

s32 VFipdm_part_logical_read(void *part, u32 sector, u32 count, void *buf) {
    (void)part; (void)sector; (void)count; (void)buf;
    return 0;
}

s32 VFipdm_part_logical_write(void *part, u32 sector, u32 count, const void *buf) {
    (void)part; (void)sector; (void)count; (void)buf;
    return 0;
}

s32 VFipdm_part_logical_erase(void *part, u32 sector, u32 count) {
    (void)part; (void)sector; (void)count;
    return 0;
}

s32 VFipdm_part_get_media_information(void *part, void *info) {
    (void)part; (void)info;
    return 0;
}

s32 VFipdm_part_check_media_write_protect(void *part) {
    (void)part;
    return 0;
}

s32 VFipdm_part_check_media_insert(void *part) {
    (void)part;
    return 0;
}

s32 VFipdm_part_check_media_detect(void *part) {
    (void)part;
    return 0;
}

s32 VFipdm_part_check_data_erase(void *part) {
    (void)part;
    return 0;
}

s32 VFipdm_part_set_change_media_state(void *part, s32 state) {
    (void)part; (void)state;
    return 0;
}

s32 VFipdm_part_set_driver_error_code(void *part, s32 err) {
    (void)part; (void)err;
    return 0;
}

s32 VFipdm_part_get_driver_error_code(void *part) {
    (void)part;
    return 0;
}
