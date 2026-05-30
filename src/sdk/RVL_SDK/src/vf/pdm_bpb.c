#include "types.h"

s32 VFipdm_bpb_calculate_common_bpb_fields(void *bpb) {
    (void)bpb;
    return 0;
}

s32 VFipdm_bpb_get_bpb_information(void *drv, void *bpb) {
    (void)drv; (void)bpb;
    return 0;
}

s32 VFipdm_bpb_get_fsinfo_information(void *drv, void *fsinfo) {
    (void)drv; (void)fsinfo;
    return 0;
}

s32 VFipdm_bpb_set_fsinfo_information(void *drv, const void *fsinfo) {
    (void)drv; (void)fsinfo;
    return 0;
}

s32 VFipdm_bpb_check_boot_sector(const void *buf) {
    (void)buf;
    return 0;
}

s32 VFipdm_bpb_check_fsinfo_sector(const void *buf) {
    (void)buf;
    return 0;
}
