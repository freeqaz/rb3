#include "types.h"

s32 VFInitEx(void *param) {
    (void)param;
    return 0;
}

s32 VFFinalize(void) {
    return 0;
}

s32 VFIsAvailable(void) {
    return 0;
}

s32 VFCreateSystemFileNANDFlash(const char *path, u32 size) {
    (void)path; (void)size;
    return 0;
}

s32 VFDeleteSystemFileNANDFlash(const char *path) {
    (void)path;
    return 0;
}

s32 VFiActivateDriveCommon(s32 drive, void *drv) {
    (void)drive; (void)drv;
    return 0;
}

s32 VFMountDriveNANDFlash(s32 drive, const char *path) {
    (void)drive; (void)path;
    return 0;
}

s32 VFUnmountDrive(s32 drive) {
    (void)drive;
    return 0;
}

s32 VFCreateFile(const char *path) {
    (void)path;
    return 0;
}

s32 VFOpenFile(const char *path, s32 mode) {
    (void)path; (void)mode;
    return 0;
}

s32 VFCloseFile(s32 fd) {
    (void)fd;
    return 0;
}

s32 VFSeekFile(s32 fd, s32 offset, s32 whence) {
    (void)fd; (void)offset; (void)whence;
    return 0;
}

s32 VFReadFile(s32 fd, void *buf, u32 size) {
    (void)fd; (void)buf; (void)size;
    return 0;
}

s32 VFWriteFile(s32 fd, const void *buf, u32 size) {
    (void)fd; (void)buf; (void)size;
    return 0;
}

s32 VFDeleteFile(const char *path) {
    (void)path;
    return 0;
}

s32 VFCreateDir(const char *path) {
    (void)path;
    return 0;
}

s32 VFChangeDir(const char *path) {
    (void)path;
    return 0;
}

s32 VFFormatDrive(s32 drive) {
    (void)drive;
    return 0;
}

s32 VFGetFileSizeByFd(s32 fd, u32 *size) {
    (void)fd; (void)size;
    return 0;
}

s32 VFGetFileSize(const char *path, u32 *size) {
    (void)path; (void)size;
    return 0;
}

s32 VFGetDriveFreeSize(s32 drive, u32 *size) {
    (void)drive; (void)size;
    return 0;
}

s32 VFFileSearchFirst(const char *path, void *dirent) {
    (void)path; (void)dirent;
    return 0;
}

s32 VFFileSearchNext(void *dirent) {
    (void)dirent;
    return 0;
}

s32 VFFileSync(s32 fd) {
    (void)fd;
    return 0;
}

s32 VFGetLastError(s32 drive) {
    (void)drive;
    return 0;
}

s32 VFGetLastDeviceError(s32 drive) {
    (void)drive;
    return 0;
}

const char *VFGetApiErrorString(s32 err) {
    (void)err;
    return 0;
}

s32 VFiConvertFileTimeToSeconds(u16 date, u16 time) {
    (void)date; (void)time;
    return 0;
}

s32 VFiPath2HandleIndex(const char *path, s32 *driveIdx, void **vol) {
    (void)path; (void)driveIdx; (void)vol;
    return 0;
}
