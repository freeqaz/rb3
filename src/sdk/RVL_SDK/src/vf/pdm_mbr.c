#include "types.h"

s32 VFipdm_mbr_get_table(void *disk, void *table) {
    (void)disk; (void)table;
    return 0;
}

s32 VFipdm_mbr_get_mbr_part_table(const void *buf, void *table) {
    (void)buf; (void)table;
    return 0;
}

s32 VFipdm_mbr_get_epbr_part_table(void *disk, u32 extStart, void *table) {
    (void)disk; (void)extStart; (void)table;
    return 0;
}

s32 VFipdm_mbr_check_master_boot_record(void *disk, void *mbr) {
    (void)disk; (void)mbr;
    return 0;
}
