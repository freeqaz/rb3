#include "types.h"

s32 VFiPFFILE_Cursor_AdvanceToRead(void *cursor, u32 size) {
    (void)cursor; (void)size;
    return 0;
}

s32 VFiPFFILE_Cursor_ReadHeadSector(void *vol, void *cursor, void *buf, u32 size) {
    (void)vol; (void)cursor; (void)buf; (void)size;
    return 0;
}

s32 VFiPFFILE_Cursor_ReadBodySectors(void *vol, void *cursor, void *buf, u32 size) {
    (void)vol; (void)cursor; (void)buf; (void)size;
    return 0;
}

s32 VFiPFFILE_Cursor_ReadTailSector(void *vol, void *cursor, void *buf, u32 size) {
    (void)vol; (void)cursor; (void)buf; (void)size;
    return 0;
}

s32 VFiPFFILE_Cursor_Read(void *vol, void *cursor, void *buf, u32 size) {
    (void)vol; (void)cursor; (void)buf; (void)size;
    return 0;
}

s32 VFiPFFILE_Cursor_WriteHeadSector(void *vol, void *cursor, const void *buf, u32 size) {
    (void)vol; (void)cursor; (void)buf; (void)size;
    return 0;
}

s32 VFiPFFILE_Cursor_WriteTailSector(void *vol, void *cursor, const void *buf, u32 size) {
    (void)vol; (void)cursor; (void)buf; (void)size;
    return 0;
}

s32 VFiPFFILE_Cursor_Write_Overwrite(void *vol, void *cursor, const void *buf, u32 size) {
    (void)vol; (void)cursor; (void)buf; (void)size;
    return 0;
}

s32 VFiPFFILE_Cursor_Write(void *vol, void *cursor, const void *buf, u32 size) {
    (void)vol; (void)cursor; (void)buf; (void)size;
    return 0;
}

s32 VFiPFFILE_Cursor_MoveToClusterEnd(void *vol, void *cursor) {
    (void)vol; (void)cursor;
    return 0;
}

s32 VFiPFFILE_GetSFD(void *vol, void *fd, void **sfd) {
    (void)vol; (void)fd; (void)sfd;
    return 0;
}

s32 VFiPFFILE_p_fopen(void *vol, const void *path, s32 mode, void **fd) {
    (void)vol; (void)path; (void)mode; (void)fd;
    return 0;
}

s32 VFiPFFILE_p_fread(void *vol, void *fd, void *buf, u32 size, u32 *read) {
    (void)vol; (void)fd; (void)buf; (void)size; (void)read;
    return 0;
}

s32 VFiPFFILE_p_fwrite(void *vol, void *fd, const void *buf, u32 size, u32 *written) {
    (void)vol; (void)fd; (void)buf; (void)size; (void)written;
    return 0;
}

s32 VFiPFFILE_p_finfo(void *vol, void *fd, void *info) {
    (void)vol; (void)fd; (void)info;
    return 0;
}

void *VFiPFFILE_GetOpenedFile(void *vol, s32 idx) {
    (void)vol; (void)idx;
    return 0;
}

s32 VFiPFFILE_FinalizeAllFiles(void *vol) {
    (void)vol;
    return 0;
}

s32 VFiPFFILE_remove(void *vol, const void *path) {
    (void)vol; (void)path;
    return 0;
}

s32 VFiPFFILE_fopen(void *vol, const void *path, s32 mode, void **fd) {
    (void)vol; (void)path; (void)mode; (void)fd;
    return 0;
}

s32 VFiPFFILE_fclose(void *vol, void *fd) {
    (void)vol; (void)fd;
    return 0;
}

s32 VFiPFFILE_fread(void *vol, void *fd, void *buf, u32 size, u32 *read) {
    (void)vol; (void)fd; (void)buf; (void)size; (void)read;
    return 0;
}

s32 VFiPFFILE_fwrite(void *vol, void *fd, const void *buf, u32 size, u32 *written) {
    (void)vol; (void)fd; (void)buf; (void)size; (void)written;
    return 0;
}

s32 VFiPFFILE_fseek(void *vol, void *fd, s32 offset, s32 whence) {
    (void)vol; (void)fd; (void)offset; (void)whence;
    return 0;
}

s32 VFiPFFILE_fsync(void *vol, void *fd) {
    (void)vol; (void)fd;
    return 0;
}

s32 VFiPFFILE_finfo(void *vol, void *fd, void *info) {
    (void)vol; (void)fd; (void)info;
    return 0;
}
