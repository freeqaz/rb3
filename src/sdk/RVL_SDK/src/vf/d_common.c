#include "types.h"

u8 l_driveInfo[0x208];

s32 dCommon_DevideBuff32(void *buf, s32 size, s32 *count, void **parts, s32 partCount) {
    (void)buf;
    (void)size;
    (void)count;
    (void)parts;
    (void)partCount;
    return 0;
}

s32 dCommon_IsPrfFile(const void *header) {
    (void)header;
    return 0;
}

s32 dCommon_CopyPrfFileHeader(void *dst, const void *src) {
    (void)dst;
    (void)src;
    return 0;
}

void dCommon_PrintSignature(void) {
}

s32 dCommon_MakeFsInfoSec(void *buf, s32 freeCount, s32 nextFree) {
    (void)buf;
    (void)freeCount;
    (void)nextFree;
    return 0;
}

s32 dCommon_GetPhysicalOffset(s32 logicalOffset) {
    (void)logicalOffset;
    return 0;
}

s32 dCommon_GetNiceFatType(u32 totalSectors) {
    (void)totalSectors;
    return 0;
}

s32 dCommon_GetReservedSecFromFatType(s32 fatType) {
    (void)fatType;
    return 0;
}

s32 dCommon_GetRootEntNumFromFatType(s32 fatType) {
    (void)fatType;
    return 0;
}

s32 dCommon_MakeBootSector(void *buf, s32 totalSectors, s32 fatType, s32 clusterSize,
                            s32 rootEntNum, s32 resvSecNum, const char *label,
                            u32 volId, s32 mediaType) {
    (void)buf;
    (void)totalSectors;
    (void)fatType;
    (void)clusterSize;
    (void)rootEntNum;
    (void)resvSecNum;
    (void)label;
    (void)volId;
    (void)mediaType;
    return 0;
}

s32 dCommon_ReadDummyBPB(s32 driveIdx, void *bpb) {
    (void)driveIdx;
    (void)bpb;
    return 0;
}

s32 dCommon_WriteDummyBPB(s32 driveIdx, const void *bpb) {
    (void)driveIdx;
    (void)bpb;
    return 0;
}

void dCommon_initDriveInfo(void) {
}

s32 dCommon_getFileSizeFromDisk(s32 driveIdx, s32 *size) {
    (void)driveIdx;
    (void)size;
    return 0;
}

s32 dCommon_setFileSizeToDisk(s32 driveIdx, s32 size) {
    (void)driveIdx;
    (void)size;
    return 0;
}

s32 dCommon_getLastDeviceErrorFromDisk(s32 driveIdx, s32 *err) {
    (void)driveIdx;
    (void)err;
    return 0;
}

s32 dCommon_setLastDeviceErrorToDisk(s32 driveIdx, s32 err) {
    (void)driveIdx;
    (void)err;
    return 0;
}

s32 dCommon_setLastDeviceErrorToDisk2(s32 driveIdx, s32 err) {
    (void)driveIdx;
    (void)err;
    return 0;
}

s32 dCommon_getHandleIdxFromDisk(s32 driveIdx) {
    (void)driveIdx;
    return 0;
}

s32 dCommon_setFatTypeToDisk(s32 driveIdx, s32 fatType) {
    (void)driveIdx;
    (void)fatType;
    return 0;
}

s32 dCommon_getResvSecNumFromDisk(s32 driveIdx, s32 *resvSecNum) {
    (void)driveIdx;
    (void)resvSecNum;
    return 0;
}

s32 dCommon_setResvSecNumToDisk(s32 driveIdx, s32 resvSecNum) {
    (void)driveIdx;
    (void)resvSecNum;
    return 0;
}

s32 dCommon_getRootEntNumFromDisk(s32 driveIdx, s32 *rootEntNum) {
    (void)driveIdx;
    (void)rootEntNum;
    return 0;
}

s32 dCommon_setRootEntNumToDisk(s32 driveIdx, s32 rootEntNum) {
    (void)driveIdx;
    (void)rootEntNum;
    return 0;
}

s32 dCommon_FlushFromHandleIdx(s32 handleIdx) {
    (void)handleIdx;
    return 0;
}
