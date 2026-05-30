#include "types.h"

const u8 l_nand_func[0x20] = {0};
s32 VF_nand_retry_max;
s32 VF_nand_sleep_msec;
u8 l_nandFunc[0x1A0];

s32 VFi_NandCreate(const char *path) {
    (void)path;
    return 0;
}

s32 VFi_NandDelete(const char *path) {
    (void)path;
    return 0;
}

s32 VFi_NandClose(s32 fd) {
    (void)fd;
    return 0;
}

s32 VFi_NandOpen(const char *path, s32 mode, s32 *fd) {
    (void)path; (void)mode; (void)fd;
    return 0;
}

s32 VFi_NandRead(s32 fd, void *buf, u32 size, u32 *read) {
    (void)fd; (void)buf; (void)size; (void)read;
    return 0;
}

s32 VFi_NandCreateDir(const char *path) {
    (void)path;
    return 0;
}

s32 VFi_NandGetLength(s32 fd, u32 *size) {
    (void)fd; (void)size;
    return 0;
}

s32 VFi_NandOpenSp(const char *path, s32 mode, s32 *fd) {
    (void)path; (void)mode; (void)fd;
    return 0;
}

s32 VFi_NandSetNANDFuncNormal(void) {
    return 0;
}

s32 A32_NANDRead(s32 fd, void *buf, u32 size) {
    (void)fd; (void)buf; (void)size;
    return 0;
}

s32 A32_NANDWrite(s32 fd, const void *buf, u32 size) {
    (void)fd; (void)buf; (void)size;
    return 0;
}

s32 _CreatePrfFile(const char *path, u32 size, void *drv) {
    (void)path; (void)size; (void)drv;
    return 0;
}

s32 NAND_CreatePrfFile(const char *path, u32 size) {
    (void)path; (void)size;
    return 0;
}

s32 VFi_NandFlushNANDFromHandleIdx(s32 handleIdx) {
    (void)handleIdx;
    return 0;
}

s32 _MountPrfFile(const char *path, void *drv, void *vol) {
    (void)path; (void)drv; (void)vol;
    return 0;
}

s32 nanddrv_BuildUpFSInfoSector(void *buf) {
    (void)buf;
    return 0;
}

s32 nanddrv_BuildUpBootSector(void *buf, u32 size) {
    (void)buf; (void)size;
    return 0;
}

s32 nanddrv_init(void *drv) {
    (void)drv;
    return 0;
}

s32 nanddrv_mount(void *drv, void *vol) {
    (void)drv; (void)vol;
    return 0;
}

s32 nanddrv_format(void *drv, void *param) {
    (void)drv; (void)param;
    return 0;
}

s32 nanddrv_pread(void *drv, u32 sector, u32 count, void *buf) {
    (void)drv; (void)sector; (void)count; (void)buf;
    return 0;
}

s32 nanddrv_pwrite(void *drv, u32 sector, u32 count, const void *buf) {
    (void)drv; (void)sector; (void)count; (void)buf;
    return 0;
}

s32 nanddrv_unmount(void *drv) {
    (void)drv;
    return 0;
}

s32 nanddrv_finalize(void *drv) {
    (void)drv;
    return 0;
}

s32 nanddrv_get_disk_info(void *drv, void *info) {
    (void)drv; (void)info;
    return 0;
}

void VFi_nanddrv_init_drv_tbl(void) {
}

s32 nanddrv_physical_read(void *drv, u32 sector, u32 count, void *buf) {
    (void)drv; (void)sector; (void)count; (void)buf;
    return 0;
}

s32 nanddrv_physical_write(void *drv, u32 sector, u32 count, const void *buf) {
    (void)drv; (void)sector; (void)count; (void)buf;
    return 0;
}
