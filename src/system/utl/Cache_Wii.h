

#ifndef UTL_CACHEWII_H
#define UTL_CACHEWII_H

#include "Cache.h"
#include "system/os/ThreadCall.h"
#include <vector>

class CacheIDWii {
public:
    CacheIDWii();
    inline CacheIDWii(const CacheIDWii &other)
        : mStrCacheName(other.mStrCacheName), m0x10(other.m0x10), m0x1c(other.m0x1c),
          m0x28(other.m0x28) {}
    virtual ~CacheIDWii();
    virtual const char *GetCachePath(const char *);
    virtual const char *GetCacheSearchPath(const char *);
    String mStrCacheName; // 0x04
    String m0x10; // 0x10
    String m0x1c; // 0x1c
    int m0x28; // 0x28
};

class CacheWii : public Cache, public ThreadCallback {
    CacheWii(const CacheIDWii &);
    virtual ~CacheWii();
    virtual const char *GetCacheName();
    virtual void Poll();
    virtual bool IsConnectedSync();
    virtual int GetFreeSpaceSync(unsigned long long *);
    virtual bool DeleteSync(const char *);
    virtual bool
    GetDirectoryAsync(const char *, std::vector<CacheDirEntry> *, Hmx::Object *);
    virtual bool GetFileSizeAsync(const char *, unsigned int *, Hmx::Object *);
    virtual bool ReadAsync(const char *, void *, uint, Hmx::Object *);
    virtual bool WriteAsync(const char *, void *, uint, Hmx::Object *);
    virtual bool DeleteAsync(const char *, Hmx::Object *);
    virtual int ThreadStart();
    virtual void ThreadDone(int);
    int ThreadGetDir(String);
    int ThreadGetFileSize();
    int ThreadRead();
    int ThreadWrite();
    int ThreadDelete();

    CacheIDWii m0x10;
    String mThreadStr; // 0x3c
    String m0x48;
    void *m0x54;
    int m0x58;
    std::vector<CacheDirEntry> *mCacheDirList; // 0x5c
    int m0x60;
    char *m0x64;
    char *m0x68;
    char *drive; // 0x6c
    int m0x70; // padding
    bool m0x74;
};

#endif