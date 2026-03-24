#pragma once
#include "os/Memcard.h"

class MemcardWii : public Memcard {
public:
    virtual void Init();
    virtual void Terminate();
    virtual void SetContainerName(const char *) {}
    virtual void SetContainerDisplayName(const wchar_t *) {}
    virtual const char *GetContainerName() { return ""; }
    virtual const wchar_t *GetDisplayName() { return L""; }
    virtual void ShowDeviceSelector(const ContainerId &, bool, Hmx::Object *, int) {}
    virtual bool DeviceAvailable(const ContainerId &) { return false; }
    virtual int DeleteContainer(const ContainerId &) { return 0; }
    virtual void CreateContainer(const ContainerId &) {}
    virtual void Mount(CreateType) {}
    virtual void Unmount() {}
    virtual int GetPathFreeSpace(const char *, unsigned long long *) { return 0; }
    virtual int GetDeviceFreeSpace(unsigned long long *) { return 0; }
    virtual int Delete(const char *) { return 0; }
    virtual int RemoveDir(const char *) { return 0; }
    virtual int MakeDir(const char *) { return 0; }
    virtual int GetSize(const char *, int *) { return 0; }
    virtual int Format() { return 0; }
    virtual int Unformat() { return 0; }
    virtual MCFile *CreateMCFile() { return 0; }
    virtual int PrintDir(const char *, bool) { return 0; }
};

extern MemcardWii TheMC;
