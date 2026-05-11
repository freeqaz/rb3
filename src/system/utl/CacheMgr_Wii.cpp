#include "CacheMgr_Wii.h"
#include "Cache_Wii.h"
#include "VF.h"
#include "MemMgr.h"
#include "os/Debug.h"
#include "utl/MakeString.h"

extern "C" int CM_CNTSDNANDCheckRSO(unsigned long, unsigned long, unsigned long *);

extern const char *kVFSysFileName;
extern const char *kCacheMgrVFDir;

CacheMgrWii::CacheMgrWii() : mVar1(), mVar2(0), mVar3(0), mVar4(0), mVar5(0) {
    CreateVFCache();
}

void CacheMgrWii::CreateVFCache() {
    String vfDir(kCacheMgrVFDir);
    const char *vfSysFile = kVFSysFileName;
    const char *vfDrive = "A";
    bool success = true;

    mUnk = (int)_MemAlloc(0x68000, 0x20);
    VFInitEx((void *)mUnk, 0x4000);
    int mountResult = VFMountDriveNANDFlash(vfDrive, vfSysFile);
    bool needsUnmount = (mountResult == 0);
    if (mountResult == 0xb006) {
        int delResult = VFDeleteSystemFileNANDFlash(vfSysFile);
        if (delResult == 0) {
            mountResult = 0xb001;
        } else {
            FormatString fs("Can't delete system file.");
            TheDebug.Notify(fs.Str());
        }
    }
    if (mountResult == 0xb001) {
        unsigned long avail = -1;
        if (CM_CNTSDNANDCheckRSO(0x80, 1, &avail) != 0 || (avail & 0xf) != 0) {
            success = false;
            FormatString fs("Not enough NAND available for VF.");
            TheDebug.Notify(fs.Str());
        } else if (VFCreateSystemFileNANDFlash(vfSysFile, 0x200000) != 0) {
            success = false;
            FormatString fs("Can't create sytem file.");
            TheDebug.Notify(fs.Str());
        } else if (VFMountDriveNANDFlash(vfDrive, vfSysFile) != 0) {
            success = false;
            FormatString fs("Can't mount nand drive.");
            TheDebug.Notify(fs.Str());
        } else {
            needsUnmount = true;
            if (VFFormatDrive(vfDrive) != 0) {
                success = false;
                FormatString fs("Can't format nand drive");
                TheDebug.Notify(fs.Str());
            }
        }
    }
    if (success) {
        VFCreateDir(vfDir.c_str());
        VFChangeDir(vfDir.c_str());
    }
    if (needsUnmount && VFUnmountDrive(vfDrive) != 0) {
        FormatString fs("Can't unmount nand drive.");
        TheDebug.Notify(fs.Str());
    }
}

CacheMgrWii::~CacheMgrWii() {
    VFFinalize();
    _MemFree((void *)mUnk);
}

void CacheMgrWii::Poll() {
    CacheMgr::OpType op = GetOp();
    if (op != 0) {
        switch (op) {
        case 1:
            PollSearch();
            break;
        case 3:
            PollMount();
            break;
        case 4:
            PollUnmount();
            break;
        default: {
            FormatString fmt("Unknown OpType encountered in CacheMgr::Poll()\n");
            TheDebug.Fail(fmt.Str());
        }
            break;
        }
    }
}

const char *unusedStrings2[] = { "\n"

};

bool CacheMgrWii::SearchAsync(const char *param_1, CacheID **param_2) {
    if (!IsDone()) {
        SetLastResult(kCache_ErrorBusy);
    } else {
        if (param_2 != NULL && (*param_2) != NULL) {
            SetLastResult(kCache_ErrorBadParam);
            return true;
        }
        TheDebug << "SearchAsync BAD PARAM: mStrCacheName is empty\n";

        if (param_2 != NULL) {
            TheDebug << MakeString(", *ppCacheID = 0x%X", param_2);
        }
        TheDebug << "";
        SetLastResult(kCache_NoError);
        return true;
    }
    TheDebug << MakeString("SearchAsync BAD PARAM: ppCacheID = 0x%X", param_2);
    if (param_2 != NULL) {
        TheDebug << MakeString(", *ppCacheID = 0x%X", param_2);
    }
    SetLastResult(kCache_ErrorBadParam);

    return false;
}

/*
bool CacheMgrWii::CreateCacheID(const char* param_1, const char* param_2, const char*
param_3, const char* param_4, const char* param_5, int param_6, CacheID** param_7) { if
(param_2 == 0 || param_4 == 0) { SetLastResult(kCache_ErrorBadParam); return false; } else
{ CacheIDWii* id = new CacheIDWii(); id->unk2 = param_2; id->unk3 = param_4; id->unk4 =
param_6; return true;
    }
}
*/

/*
bool CacheMgrWii::MountAsync(CacheID*, Cache*, Hmx::Object*) {}
*/
bool CacheMgrWii::UnmountAsync(Cache **, Hmx::Object *) {}
bool CacheMgrWii::DeleteAsync(CacheID *) {}
void CacheMgrWii::PollSearch() {}
void CacheMgrWii::EndSearch(CacheResult) {}

void CacheMgrWii::PollMount() {}

void CacheMgrWii::PollUnmount() {}
