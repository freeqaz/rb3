#include "Cache_Wii.h"
#include "VF.h"
#include "system/os/ThreadCall.h"

CacheIDWii::CacheIDWii() {}

CacheIDWii::~CacheIDWii() {}

const char *CacheIDWii::GetCachePath(const char *param_1) {
    if (mStrCacheName.empty()) {
        FormatString fs("CacheID::GetCachePath() - mStrCacheName is empty.");
        TheDebug.Fail(fs.Str());
    }

    if (param_1 == NULL) {
        return MakeString("%s/", mStrCacheName.c_str());
    } else {
        String a = param_1;
        a.ReplaceAll('\\', '/');
        if (a[0] == '/') {
            a.erase(0, 1);
        }
        return MakeString("%s", a.c_str());
    }
}

const char *CacheIDWii::GetCacheSearchPath(const char *param_1) {
    if (mStrCacheName.empty()) {
        FormatString fs("CacheID::GetCacheSearchPath() - mStrCacheName is empty.\n");
        TheDebug.Fail(fs.Str());
    }
    if (param_1 == NULL) {
        return MakeString("%s/", mStrCacheName.c_str());
    } else {
        return GetCachePath(param_1);
    }
}

CacheWii::CacheWii(const CacheIDWii &param_1) {
    m0x10.mStrCacheName = String(param_1.mStrCacheName);
    m0x10.m0x10 = String(param_1.m0x10);
    m0x10.m0x1c = String(param_1.m0x1c);
    m0x10.m0x28 = param_1.m0x28;
    // m0x3c = String();
    // m0x48 = String();
    m0x54 = 0;
    m0x58 = 0;
    s_mCacheDirList = 0;
    m0x60 = 0;
    m0x64 = "A:/DLC";
    m0x68 = "MSTORE.vff";
    drive = "A";

    bool result = VFMountDriveNANDFlash();
    if (result != 0) {
        FormatString fs("Can't mount nand drive.");
        TheDebug.Notify(fs.Str());
    } else {
        int dirResult = VFCreateDir(m0x64);
        if (dirResult != 0 && dirResult != 0x11) {
            const char *error = VFGetApiErrorString();
            OSReport("VFCreateDir vfErr %s Line %d\n", error, 0x5d);
        }
        dirResult = VFChangeDir(m0x64);
        if (dirResult != 0) {
            const char *error = VFGetApiErrorString();
            OSReport("VFChangeDir vfErr %s Line %d\n", error, 0x67);
        }
    }
}

CacheWii::~CacheWii() {
    if (m0x74) {
        int result = VFUnmountDrive(drive);
        if (result != 0) {
            FormatString fs("Can't unmount nand drive.");
            TheDebug.Fail(fs.Str());
        } else {
            m0x74 = false;
        }
    }
}

const char *CacheWii::GetCacheName() { return m0x10.mStrCacheName.c_str(); }

void CacheWii::Poll() {}

bool CacheWii::IsConnectedSync() { return true; }

int CacheWii::GetFreeSpaceSync(unsigned long long *param_1) {
    *param_1 = VFGetDriveFreeSize(drive);
    mLastResult = kCache_NoError;
    return true;
}

bool CacheWii::DeleteSync(const char *param_1) {
    if (!IsDone()) {
        mLastResult = kCache_ErrorBusy;
        return false;
    } else if (!param_1) {
        mLastResult = kCache_ErrorBadParam;
        return false;
    }
    String filePath(m0x64);
    filePath = filePath + "/" + param_1;
    int iVar1 = VFDeleteFile(filePath.c_str());
    if (iVar1 != 0 && iVar1 != 2) {
        TheDebug.Notify(MakeString("Couldn't delete file %s", filePath.c_str()));
    }
    mOpCur = kOpNone;
    return true;
}

bool CacheWii::
    GetDirectoryAsync(const char *param_1, std::vector<CacheDirEntry> *param_2, Hmx::Object *) {
    if (!IsDone()) {
        mLastResult = kCache_ErrorBusy;
        return false;
    } else if (param_2 == NULL) {
        mLastResult = kCache_ErrorBadParam;
        return false;
    } else {
        MILO_ASSERT(s_mThreadStr.empty(), 0xc2);
        s_mThreadStr = m0x10.GetCacheSearchPath(param_1);
        MILO_ASSERT(s_mCacheDirList == NULL, 0xc5);
        s_mCacheDirList = param_2;
        mLastResult = kCache_NoError;
        mOpCur = kOpDirectory;
        ThreadCall(this);
        return true;
    }
}

bool CacheWii::GetFileSizeAsync(const char *param_1, unsigned int *param_2, Hmx::Object *) {
    if (!IsDone()) {
        mLastResult = kCache_ErrorBusy;
        return false;
    } else if (param_2 == 0) {
        mLastResult = kCache_ErrorBadParam;
        return false;
    } else {
        s_mThreadStr = m0x10.GetCachePath(param_1);
        m0x54 = param_2;
        mLastResult = kCache_NoError;
        mOpCur = kOpFileSize;
        ThreadCall(this);
        return true;
    }
}

bool CacheWii::ReadAsync(
    const char *param_1, void *param_2, uint param_3, Hmx::Object *param_4
) {
    if (!IsDone()) {
        mLastResult = kCache_ErrorBusy;
        return false;
    } else if (param_1 == NULL || param_2 == NULL || param_3 == NULL) {
        mLastResult = kCache_ErrorBadParam;
        return false;
    } else {
        s_mThreadStr = m0x10.GetCachePath(param_1);
        m0x54 = param_2;
        m0x58 = param_3;
        mLastResult = kCache_NoError;
        mOpCur = kOpRead;
        ThreadCall(this);
        return true;
    }
}

bool CacheWii::WriteAsync(
    const char *param_1, void *param_2, uint param_3, Hmx::Object *param_4
) {
    if (!IsDone()) {
        mLastResult = kCache_ErrorBusy;
        return false;
    } else if (param_1 == NULL || param_2 == NULL || param_3 == NULL) {
        mLastResult = kCache_ErrorBadParam;
        return false;
    } else {
        s_mThreadStr = m0x10.GetCachePath(param_1);
        m0x54 = param_2;
        m0x58 = param_3;
        mLastResult = kCache_NoError;
        mOpCur = kOpWrite;
        ThreadCall(this);
        return true;
    }
}

bool CacheWii::DeleteAsync(const char *param_1, Hmx::Object *) {
    if (!IsDone()) {
        mLastResult = kCache_ErrorBusy;
        return false;
    } else if (param_1 == NULL) {
        mLastResult = kCache_ErrorBadParam;
        return false;
    } else {
        s_mThreadStr = m0x10.GetCachePath(param_1);
        mLastResult = kCache_NoError;
        mOpCur = kOpDelete;
        ThreadCall(this);
        return true;
    }
}

int CacheWii::ThreadStart() {
    MILO_ASSERT(!IsDone(), 0x14b);

    switch (mOpCur) {
    case kOpDirectory: {
        String tmp(m0x64);
        tmp = tmp + "/" + s_mThreadStr;
        return ThreadGetDir(tmp);
    }
    case kOpFileSize:
        return ThreadGetFileSize();
    case kOpWrite:
        return ThreadWrite();
    case kOpRead:
        return ThreadRead();
    case kOpDelete:
        return ThreadDelete();
    default:
        MILO_ASSERT(false, 0x163);
        return 0;
    }
}

void CacheWii::ThreadDone(int param_1) {
    MILO_ASSERT(!IsDone(), 0x16c);

    switch (mOpCur) {
    case kOpDirectory: {
        mLastResult = (CacheResult)param_1;
        s_mThreadStr = gNullStr;
        s_mCacheDirList = 0;
        m0x60 = 0;
        break;
    }
    case kOpFileSize: {
        mLastResult = (CacheResult)param_1;
        s_mThreadStr = gNullStr;
        m0x54 = 0;
        m0x60 = 0;
        break;
    }
    case kOpRead: {
        mLastResult = (CacheResult)param_1;
        s_mThreadStr = gNullStr;
        m0x54 = 0;
        m0x58 = 0;
        m0x60 = 0;
        break;
    }
    case kOpWrite: {
        mLastResult = (CacheResult)param_1;
        s_mThreadStr = gNullStr;
        m0x54 = 0;
        m0x58 = 0;
        if (m0x60 != 0) {
        }
        break;
    }
    case kOpDelete: {
        mLastResult = (CacheResult)param_1;
        s_mThreadStr = gNullStr;
        m0x60 = 0;
        break;
    }
    default: {
        MILO_ASSERT(false, 0x19b);
    }
    }
    mOpCur = kOpNone;
}

int CacheWii::ThreadGetDir(String) {}

int CacheWii::ThreadGetFileSize() {
    String filePath(m0x64);
    filePath = filePath + "/" + s_mThreadStr;
    int result = VFGetFileSize(filePath.c_str());
    if (result != -1) {
        *(unsigned int *)m0x54 = result;
        return 0;
    }
    return -1;
}

int CacheWii::ThreadRead() {
    String filePath(m0x64);
    filePath = filePath + "/" + s_mThreadStr;
    void *file = VFOpenFile(filePath.c_str(), "r", 0);
    if (file == NULL) {
        TheDebug.Notify(MakeString("Couldn't open file %s", filePath.c_str()));
        return -1;
    }
    int fileSize = VFGetFileSizeByFd(file);
    if (fileSize != -1) {
        fileSize = VFReadFile(file, m0x54, fileSize, 0);
        if (fileSize != 0) {
            TheDebug.Notify(MakeString("Couldn't read file %s", filePath.c_str()));
            VFCloseFile(file);
            return -1;
        }
    }
    if (VFCloseFile(file) != 0) {
        TheDebug.Notify(MakeString("Couldn't close the file pointer %s", filePath.c_str()));
    }
    return 0;
}

int CacheWii::ThreadWrite() {
    String filePath(m0x64);
    filePath = filePath + "/" + s_mThreadStr;
    void *file = VFOpenFile(filePath.c_str(), "w", 0);
    if (file == NULL) {
        file = VFCreateFile(filePath.c_str(), 0);
        if (file == NULL) {
            TheDebug.Notify(MakeString("Couldn't open file %s", filePath.c_str()));
            return -1;
        }
    }
    int returnValue = 0;
    if (VFWriteFile(file, m0x54, m0x58) != 0) {
        TheDebug.Notify(MakeString("Couldn't write file %s", filePath.c_str()));
        returnValue = -1;
    }
    if (VFFileSync(file) != 0) {
        FormatString fs("Can't sync file");
        TheDebug.Fail(fs.Str());
        returnValue = -1;
    }
    if (VFCloseFile(file) != 0) {
        TheDebug.Notify(MakeString("Couldn't close the file pointer to file %s", filePath.c_str()));
    }
    return returnValue;
}

int CacheWii::ThreadDelete() {
    String filePath(m0x64);
    filePath = filePath + "/" + s_mThreadStr;
    int result = VFDeleteFile(filePath.c_str());
    if (result != 0 && result != 2) {
        TheDebug.Notify(MakeString("Couldn't delete file %s", filePath.c_str()));
        return -1;
    }
    return 0;
}
