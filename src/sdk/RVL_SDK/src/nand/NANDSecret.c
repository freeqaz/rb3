#include "revolution/nand/nand.h"
#include <revolution/NAND.h>
#include <revolution/FS.h>

s32 NANDSecretGetUsage(const char* path, s32* fsBlock, s32* inode) {
    if (!nandIsInitialized()) {
        return NAND_RESULT_FATAL_ERROR;
    }
    else {
        char absPath[64] = "";
        nandGenerateAbsPath(absPath, path);
        return nandConvertErrorCode(ISFS_GetUsage(absPath, fsBlock, inode));
    }
}

NANDResult NANDSecretGetFileSystemStatus(ISFSStats* statsOut) {
    if (!nandIsInitialized()) return NAND_RESULT_FATAL_ERROR;

    ISFSStats stats;
    ISFSError err = ISFS_GetStats(&stats);
    if (err == 0) {
        statsOut->blockSize = stats.blockSize;
        statsOut->freeBlocks = stats.freeBlocks;
        statsOut->occupiedBlcocks = stats.occupiedBlcocks;
        statsOut->badBlocks = stats.badBlocks;
        statsOut->reservedBlocks = stats.reservedBlocks;
        statsOut->freeInodes = stats.freeInodes;
        statsOut->occupedInodes = stats.occupedInodes;
    }
    return nandConvertErrorCode(err);
}
