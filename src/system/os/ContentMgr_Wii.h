#pragma once
#include "os/ContentMgr.h"
#include "os/ThreadCall.h"
#include "revolution/cnt/cnt.h"
#include "revolution/mem/mem_allocator.h"

bool CntSdRsoInit(struct RSOObjectHeader *);
void CntSdRsoTerminate();

class RootContent : public Content {
public:
    RootContent(const char *str) : mRoot(str) {}
    virtual ~RootContent() {}
    virtual const char *Root() { return mRoot.c_str(); }
    virtual int OnMemcard() { return false; }
    virtual ContentLocT Location() { return kLocationRoot; }
    virtual int LicenseBits() { return 0; }
    virtual bool HasValidLicenseBits() { return true; }
    virtual Content::State GetState() { return kAlwaysMounted; }
    virtual void Poll() {}
    virtual void Mount() {}
    virtual void Unmount() {}
    virtual Symbol FileName();
    virtual Symbol DisplayName() { return Symbol(mRoot.c_str()); }

    String mRoot; // 0x4
};

enum CNTSDThreadStatus {
    CNTSD_THREAD_NOT_INITIALIZED = 0,
    CNTSD_THREAD_STACK_INITIALIZED = 1,
    CNTSD_THREAD_READY_TO_START = 2,
    CNTSD_THREAD_CREATED_RESUMED = 3,
    CNTSD_THREAD_DONE = 4,
};
struct CNTSDThreadInfo {
    // total size: 0x350
    struct OSThread thread; // offset 0x0, size 0x318
    enum CNTSDThreadStatus status; // offset 0x318, size 0x4
    void *stack; // offset 0x31C, size 0x4
    unsigned long stackSize; // offset 0x320, size 0x4
    long priority; // offset 0x324, size 0x4
    unsigned short attribute; // offset 0x328, size 0x2
    void *(*func)(void *); // offset 0x32C, size 0x4
    unsigned long long args8B[1]; // offset 0x330, size 0x8
    void *args4B[5]; // offset 0x338, size 0x14
};

enum OpResult {
    kOpSuccess = 0,
    kOpFail = 1,
    kOpMissingCard = 2,
    kOpIncompatibleCard = 3,
    kOpNoNANDSpace = 4,
    kOpNoNANDInodes = 5,
    kOpMissingData = 6,
    kOpCorruptCard = 7,
    kOpIncorrectCard = 8,
    kOpBackupNeeded = 9,
    kOpDataPresent = 10,
    kOpSDSize = 11,
    kOpSDWriteProtected = 12,
    kOpCorruptData = 13,
    kOpOutOfDate = 14,
    kOpNoLicense = 15,
    kOpCorruptContent = 16
};

class WiiContent : public Content, public ThreadCallback {
public:
    WiiContent(Symbol, unsigned long long, unsigned int, bool, bool);
    virtual ~WiiContent();
    virtual const char *Root();
    virtual int OnMemcard();
    virtual ContentLocT Location();
    virtual State GetState();
    virtual void Poll();
    virtual void Mount();
    virtual void Unmount();
    virtual void Delete();
    virtual Symbol FileName();
    virtual Symbol DisplayName();
    virtual int ThreadStart();
    virtual void ThreadDone(int);

    void
    Enumerate(const char *, void (*)(const char *, const char *), bool, const char *);
    CNTHandle *GetHandle(long *);
    int FreeHandle();
    void StartMount();
    void StartBackup();
    void Backup();
    void PollTransfer();

    static void SetPassiveErrorsEnabled(bool);

    int mState; // 0x8
    Symbol mName; // 0xc
    u64 mTitleId; // 0x10
    unsigned int mContentId; // 0x18
    CNTHandle *mHandle; // 0x1c
    int unk20; // 0x20 - mTransferState
    int unk24; // 0x24
    int unk28; // 0x28 - next CNTSD action?
    ContentLocT mLocation; // 0x2c - location
    char unk30; // 0x30 - times a handle has been opened
    char unk31; // 0x31
    bool unk32; // 0x32

    static bool mSDCardRemoved;
    static bool mHandleRestoreErrors;
};

class WiiContentMgr : public ContentMgr {
public:
    WiiContentMgr();
    virtual ~WiiContentMgr();
    virtual DataNode Handle(DataArray *, bool);
    virtual void PreInit();
    virtual void Init();
    virtual void Terminate();
    virtual void StartRefresh();
    virtual void PollRefresh();
    virtual bool CanRefreshOnDone();
    virtual bool ShowCurRefreshProgress();
    virtual bool MountContent(Symbol);
    virtual bool IsMounted(Symbol);
    virtual bool DeleteContent(Symbol);
    virtual bool IsDeleteDone(Symbol);
    virtual void NotifyMounted(Content *);
    virtual void NotifyUnmounted(Content *);
    virtual void NotifyDeleted(Content *);
    virtual void NotifyFailed(Content *);

    void UnmountContents(Symbol);
    WiiContent *ContentOf(Symbol);
    CNTHandle *GetChannelContentHandle();
    CNTHandle *ContentHandleOf(Symbol);

    void *mSDBuffer; // 68
    int mMode; // 6c
    bool mNeedShopAccount; // 70
    bool mCNTSDInited; // 71
    bool mCreateSongCache; // 72
    bool unk73; // 73
    int unk74; // 74
    int unk78; // 78
    bool unk7c; // 7c
    bool unk7d; // 7d
    char pad7e[2]; // 7e - padding
    int unk80; // 80
    int unk84; // 84
    int unk88; // 88
    float unk8c; // 8c
    bool unk90; // 90
    char pad91[3]; // 91
    int mLastTransferResult; // 94
    int unk98; // 98
    bool unk9c; // 9c
};

extern WiiContentMgr TheWiiContentMgr;
extern const char *gCurContentName;
extern MEMAllocator gCNTAllocator;
extern bool gCNTThreadInUse;
