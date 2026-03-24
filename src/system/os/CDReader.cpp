#include "os/CDReader.h"
#include "os/DiscErrorMgr_Wii.h"
#include "os/PlatformMgr.h"
#include <vector>

namespace {
    std::vector<DVDFileInfo> gArkFiles;
    std::vector<int> gFileLengths;
    int gPendingFile = -1;
}

int gRetryAttempts = 0x14;

int CDGetError() { return 0; }

bool CDReadDone() {
    s32 status = DVDGetCommandBlockStatus(&gArkFiles[gPendingFile].cb);
    if (status == DVD_STATE_END) {
        gRetryAttempts = 0x14;
        ThePlatformMgr.SetDiskError(kNoDiskError);
        return true;
    }
    if ((u32)(status - 1) <= 1) {
        // DVD_STATE_BUSY (1) or DVD_STATE_WAITING (2)
        gRetryAttempts = 0x14;
        DiscErrorMgrWii *discErrMgr = ThePlatformMgr.GetDiscErrorMgrWii();
        if (discErrMgr->mMovieReadError) {
            discErrMgr->LoopUntilNoDiscError(&gArkFiles[gPendingFile], true);
        } else {
            s32 driveStatus = DVDGetDriveStatus();
            if (driveStatus == DVD_STATE_RETRY) {
                if (--gRetryAttempts == 0) {
                    ThePlatformMgr.SetDiskError(kDiskError);
                    ThePlatformMgr.GetDiscErrorMgrWii()->LoopUntilNoDiscError(
                        &gArkFiles[gPendingFile], true
                    );
                }
            } else if (driveStatus == DVD_STATE_NO_DISK || driveStatus == DVD_STATE_WRONG_DISK) {
                gRetryAttempts = 0x14;
                ThePlatformMgr.SetDiskError(kWrongDisk);
                ThePlatformMgr.GetDiscErrorMgrWii()->LoopUntilNoDiscError(
                    &gArkFiles[gPendingFile], true
                );
            } else {
                ThePlatformMgr.SetDiskError(kNoDiskError);
            }
        }
        return false;
    }
    // other status — check original status directly (no DVDGetDriveStatus)
    if (status == DVD_STATE_RETRY) {
        if (--gRetryAttempts == 0) {
            ThePlatformMgr.SetDiskError(kDiskError);
            ThePlatformMgr.GetDiscErrorMgrWii()->LoopUntilNoDiscError(
                &gArkFiles[gPendingFile], false
            );
        }
        return false;
    }
    if (status == DVD_STATE_NO_DISK || status == DVD_STATE_WRONG_DISK) {
        gRetryAttempts = 0x14;
        ThePlatformMgr.SetDiskError(kWrongDisk);
        ThePlatformMgr.GetDiscErrorMgrWii()->LoopUntilNoDiscError(
            &gArkFiles[gPendingFile], false
        );
        return false;
    }
    return false;
}

int CDReadExternal(DVDFileInfo *&info, int, unsigned long long) {
    info = 0;
    return 0;
}