#pragma once
#include "meta/MemcardAction.h"
#include "meta/Profile.h"
#include "obj/Msg.h"
#include "os/ThreadCall.h"

class MemcardMgr : public MsgSource, public ThreadCallback {
public:
    MemcardMgr();
    void Init();
    virtual DataNode Handle(DataArray *, bool);
    virtual ~MemcardMgr();
    virtual int ThreadStart();
    virtual void ThreadDone(int);
    void SaveLoadProfileComplete(Profile *, int);
    void SetDevice(unsigned int);
    void SelectDevice(Profile *, bool, Hmx::Object *, int);
    void OnSearchForDevice(Profile *);
    void OnCheckForSaveContainer(Profile *);
    void UnLoadBanner();
    int GetSizeNeeded();
    void DisableWriting(bool);
    bool IsDisableWriting() const;
    bool IsWriteMode() const;
    void OnSaveGame(Profile *, MemcardAction *, int);
    void OnLoadGame(Profile *, MemcardAction *);

    void SaveLoadAllComplete();

    bool unk20;
    char unk21[64];
    char unk61[64];
    int unka4;
    int unka8; // bufstreamnand
    int unkac;
    void *mBannerIcons; // 0xB0
    void *mBanner; // 0xB4
    bool unkb8;
    unsigned char mFlags; // 0xB9: bit0 = mDisableWriting, bit1 = mIsWriteMode
    int unkbc;
    int unkc0; // mState
    int unkc4;
    int unkc8;
    int unkcc; // memcardaction*
    int unkd0;
    int unkd4;
    int unkd8;
    Profile *unkdc;
};

DECLARE_MESSAGE(SaveLoadAllCompleteMsg, "save_load_all_complete_msg")
SaveLoadAllCompleteMsg() : Message(Type()) {}
END_MESSAGE

extern MemcardMgr TheMemcardMgr;