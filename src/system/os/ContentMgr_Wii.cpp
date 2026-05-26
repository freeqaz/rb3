#include "os/ContentMgr_Wii.h"
#include "decomp.h"
#include "meta/StorePackedMetadata.h"
#include "obj/Data.h"
#include "os/CommerceMgr_Wii.h"
#include "os/ContentMgr.h"
#include "os/Debug.h"
#include "os/PlatformMgr.h"
#include "revolution/cnt/cnt.h"
#include "revolution/ec/ec.h"
#include "revolution/mem/mem_allocator.h"
#include <list>
#include <revolution/CNT.h>

Symbol RootContent::FileName() { return Symbol(mRoot.c_str()); }

std::list<DataArrayPtr> gPlatformErrorMsg;

bool gCNTThreadInUse;

void (*CNTSDInitRSO)(void *workBuffer, int workBufferSize);
bool (*CNTSDIsInsertedRSO)(void);
void (*CNTSDListFirstRSO)(void);
void (*CNTSDListNextRSO)(void);
void (*CNTSDRestoreGetBlocksRSO)(void);
void (*CNTSDInitThreadRSO)(void);
void (*CNTSDSetThreadRestoreRSO)(void);
void (*CNTSDStartThreadRSO)(void);
void (*CNTSDCardGetAvailableBlocksRSO)(void);
void (*CNTSDGetBackupBlocksFromCntRSO)(void);
void (*CNTSDBackupRSO)(void);
void (*CNTSDSetThreadBackupRSO)(void);
int (*CNTSDFinishThreadRSO)(CNTSDThreadInfo *, int *);
void (*CNTSDFinishRSO)(void);
int (*CNTSDDeleteBackupRSO)(unsigned long long titleId, unsigned short contentId);
void (*CNTSDNANDCheckRSO)(void);
void (*CNTSDGetAvailableAreaRSO)(void);
void (*CNTSDGetTmpDirUsageRSO)(void);
void (*CNTSDGetUserAvailableAreaRSO)(void);
void (*CNTSDCacheClearRSO)(void);
void (*CNTSDCacheInUseRSO)(void);
void (*CNTSDCachePushDeleteContentVRSO)(void);
void (*CNTSDCachePopRSO)(void);
void (*FAMountRSO)(void);
void (*FAIsWriteProtectedRSO)(void);
void (*CNTSDSetEventCallbackRSO)(void *);

void *cntsdModule;
void *cntsdBss;
void *cntsdCode;

MEMAllocator gCNTAllocator;

Timer gLastPlatformErrorTimer;

struct CNTSDProgress {
    int unk0;
    int unk4;
    int unk8;
    int unkC;
};
CNTSDProgress gCNTSDProgress;

void *gCNTThreadStackBuffer;
void *gCNTThreadWorkBuffer;

CNTSDThreadInfo *gCNTThreadInfo;

WiiContentMgr TheWiiContentMgr;
ContentMgr *TheContentMgr;

static struct TheContentMgrInit {
    TheContentMgrInit() { TheContentMgr = &TheWiiContentMgr; }
} _theContentMgrInit;

DECOMP_FORCEACTIVE(ContentMgr_Wii, "_unresolved func.\n")

void unresolved_cntsdModule() {
    OSReport("\nError: call cntsdModule unlinked function.\n");
}

void CM_CNTSDCacheClearRSO() { CNTSDCacheClearRSO(); }

void SDCallback(int unk) {
    ThePlatformMgr.mStorageChanged = true;
    if (unk == 0) {
        WiiContent::mSDCardRemoved = true;
    }
}

void HandleErrorFromRestore(WiiContent *content, OpResult result) {}

int ConvertCNTSDError(int error) {
    switch (error) {
        case -0xBBC:
        case -0xBB8:
        case -0xBC2:
            return 1;
        case -0xBF8:
            return 2;
        case -0xBF9:
            return 3;
        case -0xBFA:
            return 7;
        case -0xBC5:
        case -0xBC6:
            return 0x10;
        case -0xBBD:
            return 8;
        case -0xBFC:
            return 0xB;
        case -0xBFB:
            return 0xC;
        case -0xBC1:
            return 0xE;
        case -0xBBE:
            return 0xF;
        case -0xBBB:
        case -0xBC4:
        case -0xC37:
            return 0xD;
        case -0xBBF:
        case -0xBC0:
            return 1;
        case -0xBF6:
        case -0xBF7:
        case -0xBC3:
            return 1;
        case 0:
            return 0;
        default:
            MILO_WARN("ConvertCNTSDError unhandled error: %d\n", error);
            return 1;
    }
}

void DoIndentPrint(int i) {
    while (i--) {
        MILO_LOG("\t");
    }
}

WiiContent::WiiContent(
    Symbol name,
    unsigned long long titleId,
    unsigned int contentId,
    bool inNand,
    bool needsMount
) {
    mName = name;
    mTitleId = titleId;
    mContentId = contentId;
    mLocation = inNand ? kLocationHDD : kLocationRemovableMem;
    unk20 = 3;
    unk24 = 0;
    unk31 = 0;
    unk32 = 0;
    mState = needsMount ? kNeedsMounting : kUnmounted;
    unk30 = 0;
    mHandle = nullptr;
}

WiiContent::~WiiContent() {
    switch (mState) {
    case kMounting:
    case kMounted:
        Unmount();
        while (mState == kMounting) {
            Timer::Sleep(2);
            Poll();
        }
        break;
    case kUnmounted:
    case kNeedsMounting:
    case kAlwaysMounted:
    case kDeleted:
    case kFailed:
        break;
    default:
        MILO_LOG("Unknown state: %d", mState);
        break;
    }
}

Symbol WiiContent::FileName() { return mName; }

Symbol WiiContent::DisplayName() { return mName; }

const char *WiiContent::Root() { return mName.Str(); }

ContentLocT WiiContent::Location() { return mLocation; }

int WiiContent::OnMemcard() { return true; }

void WiiContent::Mount() {
    if (mState == kUnmounted) {
        mState = kNeedsMounting;
        return;
    } else if (mState != kMounting) {
        return;
    } else if (unk31) {
        unk32 = true;
    }
}

void WiiContent::Backup() {
    int oldState = mState;
    if (oldState == kMounted || (oldState > kUnmounted && oldState < kUnmounting)) {
        Unmount();
        while (mState == kMounting) {
            Timer::Sleep(2);
            Poll();
        }
        if (mState == kUnmounted && oldState == kMounted) {
            TheWiiContentMgr.NotifyUnmounted(this);
        }
    }
    mState = kNeedsBackup;
}

void WiiContent::StartMount() {
    mState = kMounting;
    unk31 = false;
    unk32 = false;
    unk24 = 0;
    if (mLocation == kLocationRemovableMem) {
        unk20 = 0;
        unk28 = 1;
        ThreadCall(this);
    } else {
        unk20 = 2;
    }
}

void WiiContent::StartBackup() {
    if (ThePlatformMgr.IsShuttingDown()) {
        MILO_LOG("WiiContent: skipping backup because we are shutting down.\n");
        return;
    }
    mState = kBackingUp;
    unk31 = false;
    unk24 = 0;
    unk20 = 0;
    unk28 = 0;
    ThreadStart();
    ThreadDone(1);
}

CNTHandle *WiiContent::GetHandle(long *result) {
    unk30++;
    if (mHandle != nullptr) {
        *result = 0;
        return mHandle;
    }
    mHandle = new CNTHandle;
    *result = 0;
    *result =
        (s32)contentInitHandleTitleNAND(mTitleId, mContentId, mHandle, &gCNTAllocator);
    if (*result) {
        MILO_FAIL("CM: %s: CNTInitHandleTitle Failed: %i\n", mName.Str(), (long)*result);
        delete mHandle;
        mHandle = nullptr;
        unk30 = 0;
    }
    return mHandle;
}

int WiiContent::FreeHandle() {
    unk30--;
    int r = 0;
    if (unk30 < 1) {
        unk30 = 0;
        if (mHandle != NULL) {
            r = CNTReleaseHandle(mHandle);
            delete mHandle;
            mHandle = NULL;
        }
    }
    return r;
}

void WiiContent::Delete() {
    int r = 0;
    int oldState = mState;
    if (oldState == kMounted || (oldState > kUnmounted && oldState < kUnmounting)) {
        Unmount();
        while (mState == kMounting) {
            Timer::Sleep(2);
            Poll();
        }
        if (mState == kUnmounted && oldState == kMounted) {
            TheWiiContentMgr.NotifyUnmounted(this);
        }
    }
    if (mState == kUnmounted || oldState == kFailed) {
        switch (mLocation) {
        case kLocationHDD: {
            unsigned short contentIds[1] = { mContentId };
            int ecR = EC_DeleteContents(mTitleId, contentIds, 1);
            if (ecR == 0) {
                mState = kDeleted;
                r = 0;
            } else {
                mState = kFailed;
                r = 1;
            }
            break;
        }
        case kLocationRemovableMem: {
            int cntsdR = CNTSDDeleteBackupRSO(mTitleId, mContentId);
            if (cntsdR == 0) {
                mState = kDeleted;
                r = 0;
            } else {
                r = ConvertCNTSDError(cntsdR);
                mState = kFailed;
            }
            break;
        }
        default:
            break;
        }
    } else {
        mState = kFailed;
        r = 1;
    }
    if (mState == kDeleted) {
        TheWiiContentMgr.mDirty = true;
    } else {
        TheWiiContentMgr.NotifyFailed(this);
    }
    TheWiiContentMgr.mLastTransferResult = r;
}

int CM_CNTSDCachePopRSO(long);

int WiiContent::PopAfterRestore() {
    int r = CM_CNTSDCachePopRSO(-1);
    if (r != 0) {
        MILO_FAIL(
            "CM: %s: Failed: Unmount, CNTSDCachePop() returned %d\n", mName.Str(), (long)r
        );
    }
    unsigned short contentIds[1] = { (unsigned short)mContentId };
    EC_DeleteContents(mTitleId, contentIds, 1);
    return r;
}

void WiiContent::PollTransfer() {
    MILO_ASSERT(unk20 == 1, 1119);
    if (unk20 == 1) {
        if (unk31) {
            gCNTSDProgress.unkC = 1;
        }
        if (gCNTSDProgress.unk8 != 0) {
            int innerResult;
            int r = CNTSDFinishThreadRSO(gCNTThreadInfo, &innerResult);
            if (r == 0) {
                r = innerResult;
            }
            gCNTThreadInUse = false;
            if (gCNTSDProgress.unkC == 1) {
                if (r != -0xBC0) {
                    PopAfterRestore();
                }
                r = 0;
            }
            if (r != 0 && (mSDCardRemoved || !CNTSDIsInsertedRSO())) {
                r = -0xBF8;
            }
            unk24 = ConvertCNTSDError(r);
            if (unk24 == 0xB && mState == 7) {
                unk24 = 1;
            }
            unk20 = 2;
        } else if (ThePlatformMgr.IsShuttingDown()) {
            unk31 = 1;
            gCNTSDProgress.unkC = 1;
        }
    }
}

void DebugPrintContents(CNTHandle *) {}

void WiiContent::Poll() {
    switch (mState) {
    case kNeedsMounting:
        StartMount();
        break;
    case kMounting:
        if (unk20 == 1) {
            PollTransfer();
        }
        switch (unk20) {
        case 0:
            break;
        case 1:
            ThePlatformMgr.mIgnorePowerOperations = true;
            if (ThePlatformMgr.mHomeMenuWii->mSDIconActive == false) {
                ThePlatformMgr.mHomeMenuWii->ActivateSDIcon(true);
            }
            break;
        case 2:
            if (unk31 == false) {
                if (unk24 == 0) {
                    s32 result = 0;
                    CNTHandle *handle = GetHandle(&result);
                    if (result == 0) {
                        DebugPrintContents(handle);
                        mState = kMounted;
                    } else {
                        MILO_FAIL(
                            "CM: %s: CNTInitHandleTitle Failed: %i\n",
                            mName.Str(),
                            result
                        );
                        unk24 = 1;
                        mState = kFailed;
                    }
                    FreeHandle();
                } else {
                    if (mHandleRestoreErrors) {
                        HandleErrorFromRestore(this, (OpResult)unk24);
                    }
                    mState = kFailed;
                }
            } else if (unk32) {
                StartMount();
            } else {
                mState = kUnmounted;
            }
            break;
        default:
            MILO_ASSERT(0, 636);
            unk24 = 1;
            mState = kFailed;
            break;
        }
        if (mState != kMounting) {
            TheWiiContentMgr.mLastTransferResult = unk24;
            unk20 = 3;
            unk31 = false;
            unk32 = false;
            ThePlatformMgr.mIgnorePowerOperations = false;
        }
        break;
    case kNeedsBackup:
        StartBackup();
        break;
    case kBackingUp:
        if (unk20 == 1) {
            PollTransfer();
        }
        switch (unk20) {
        case 0:
            break;
        case 1:
            ThePlatformMgr.mIgnorePowerOperations = true;
            if (ThePlatformMgr.mHomeMenuWii->mSDIconActive == false) {
                ThePlatformMgr.mHomeMenuWii->ActivateSDIcon(true);
            }
            break;
        case 2:
            if (unk24 == 0) {
                if (TheWiiContentMgr.unk7d) {
                    TheStoreMetadata.MarkDownloaded(mTitleId, (u16)mContentId);
                    TheWiiCommerceMgr.MarkChanged(true);
                    mState = kDeleted;
                } else {
                    unsigned short cid = (u16)mContentId;
                    int r = EC_DeleteContents(mTitleId, &cid, 1);
                    if (r == 0) {
                        mState = kDeleted;
                    } else if (r == -0x6F) {
                        MILO_FAIL(
                            "EC_DeleteContents error: ISFS_ERROR_OPENFD -- you still have the file open\n",
                            (long)r
                        );
                        unk24 = 1;
                        mState = kFailed;
                    } else {
                        MILO_FAIL("EC_DeleteContents error: %d\n", (long)r);
                        unk24 = 1;
                        mState = kFailed;
                    }
                }
            } else {
                mState = kFailed;
            }
            break;
        default:
            MILO_ASSERT(0, 740);
            unk24 = 1;
            mState = kFailed;
            break;
        }
        if (mState != kBackingUp) {
            TheWiiContentMgr.mLastTransferResult = unk24;
            unk20 = 3;
            ThePlatformMgr.mIgnorePowerOperations = false;
        }
        break;
    }
}

extern "C" void *WiiCntAlloc(MEMAllocator *, u32 size) { return _MemAlloc(size, 0x20); }

extern "C" void WiiCntFree(MEMAllocator *, void *block) { _MemFree(block); }

extern "C" void InitAllocator(MEMAllocator *allocator) {
    static MEMAllocatorFuncs cntAllocFunc = { WiiCntAlloc, WiiCntFree };
    allocator->funcs = &cntAllocFunc;
}

void *ecAlloc(unsigned long size, unsigned long align) {
    void *r = NULL;
    if (size != 0)
        r = _MemAlloc(size, align);
    return r;
}
ECNameValue ecAllocFunc = { "alloc", ecAlloc };

int ecFree(void *block) {
    if (block != NULL)
        _MemFree(block);
    return 0;
}
ECNameValue ecFreeFunc = { "free", ecFree };

WiiContentMgr::WiiContentMgr() {
    mLastTransferResult = 0;
    unk98 = 0;
    mCNTSDInited = false;
}

void WiiContentMgr::PreInit() {}

void WiiContentMgr::Init() {
    gLastPlatformErrorTimer.Restart();
    CNTInit();
    InitAllocator(&gCNTAllocator);

    mMode = 1; // kNANDMode
    unk74 = 1;
    unk78 = 0;
    unk7c = false;
    unk80 = 0;
    unk84 = 0;
    unk88 = 0;
    unk8c = 0.0f;
    unk98 = 0;
    unk90 = false;
    unk7d = false;
    unk9c = true;

    ContentMgr::Init();
    ThePlatformMgr.AddSink(this);

    ECNameValue ec_alloc_funcs[2] = { ecAllocFunc, ecFreeFunc };
    int ec_r = EC_Init(ec_alloc_funcs, 2);
    if (ec_r == -4080) {
        mNeedShopAccount = true;
    } else {
        mNeedShopAccount = false;
    }

    mSDBuffer = _MemAlloc(0x5680, 0x20);
    CNTSDInitRSO(mSDBuffer, 0x5680);
    mCNTSDInited = true;

    CNTSDSetEventCallbackRSO(SDCallback);

    gCNTThreadStackBuffer = _MemAlloc(0x8000, 0x20);
    gCNTThreadWorkBuffer = _MemAlloc(0x2BC40, 0x20);
    gCNTThreadInfo = new CNTSDThreadInfo;

    CM_CNTSDCacheClearRSO();
}

void WiiContentMgr::Terminate() {
    CNTSDFinishRSO();
    MILO_LOG("CM: Unmounting Content\n");
    TheWiiContentMgr.UnmountContents("");
    MILO_LOG("CM: Clearing the Cache\n");
    CM_CNTSDCacheClearRSO();
    if (mSDBuffer != NULL) {
        _MemFree(mSDBuffer);
        mSDBuffer = NULL;
    }
    CNTShutdown();
    if (gCNTThreadStackBuffer != NULL) {
        _MemFree(gCNTThreadStackBuffer);
        gCNTThreadStackBuffer = NULL;
    }
    if (gCNTThreadWorkBuffer != NULL) {
        _MemFree(gCNTThreadWorkBuffer);
        gCNTThreadWorkBuffer = NULL;
    }
    delete gCNTThreadInfo;
    gCNTThreadInfo = NULL;
}

void WiiContent::Enumerate(
    const char *cc, void (*func)(const char *, const char *), bool recurse, const char *cc2
) {
    if (mState != kMounted) {
        MILO_LOG("CM: Enumerate: %s not mounted\n", cc);
        return;
    }
    s32 result;
    CNTHandle *handle = GetHandle(&result);
    if (handle == NULL) {
        MILO_LOG("CM: Enumerate: could not get handle for %s\n", cc);
        return;
    }
    CNTDir directory;
    if (CNTOpenDir((CNTHandle *)handle, cc, &directory) == false) {
        FreeHandle();
        MILO_LOG("CM: Enumerate: could not open dir %s\n", cc);
        return;
    }
    CNTDirEntry entry;
    char buf[256];
    while (CNTReadDir(&directory, &entry)) {
        if (entry.arc.type != 0) {
            if (recurse) {
                String path(cc);
                if (path[strlen(path.c_str()) - 1] != '/') {
                    path += "/";
                }
                path += entry.arc.name;
                Enumerate(path.c_str(), func, recurse, cc2);
            }
        } else {
            sprintf(buf, "%s/%s", cc, entry.arc.name);
            if (cc2 == NULL || FileMatch(buf, cc2)) {
                sprintf(buf, "dlc/%s/%s", gCurContentName, cc);
                func(buf, entry.arc.name);
            }
        }
    }
    CNTCloseDir(&directory);
    FreeHandle();
}

void WiiContent::SetPassiveErrorsEnabled(bool enabled) { mHandleRestoreErrors = enabled; }

void WiiContentMgr::NotifyUnmounted(Content *c) {
    WiiContent *pc = dynamic_cast<WiiContent *>(c);
    MILO_ASSERT(pc, 2839);
    FOREACH (cb, mCallbacks) {
        (*cb)->ContentUnmounted(pc->FileName().Str());
    }
}

void WiiContentMgr::NotifyFailed(Content *c) {
    WiiContent *pc = dynamic_cast<WiiContent *>(c);
    MILO_ASSERT(pc, 2849);
    MILO_LOG("CM: Calling ContentFailed for %s\n", c->FileName());
    FOREACH (cb, mCallbacks) {
        (*cb)->ContentFailed(pc->FileName().Str());
    }
}
