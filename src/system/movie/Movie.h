#pragma once

#include "utl/BinStream.h"
#include <list>
#include <vector>

// Minimal Bink handle stub for matching the Wii target.
struct HBINK_t {
    char pad0[0x8];
    unsigned int Frames; // 0x08 (NumFrames)
    unsigned int FrameNum; // 0x0C (1-based current frame)
    char pad10[0x4];
    unsigned int FrameRate; // 0x14
    unsigned int FrameRateDiv; // 0x18
};
typedef HBINK_t *HBINK;

class Movie {
public:
    class Impl {
    public:
        class MovieLoader;

        Impl();
        ~Impl();
        static void Init();
        static void PlatformInit();
        static std::vector<Impl *> sActiveMovies;
        void End();
        bool IsOpen() const;
        bool IsLoading() const;
        bool CheckOpen(bool);
        void LockThread();
        void UnlockThread();
        int GetFrame() const;
        float MsPerFrame() const;
        int NumFrames() const;
        bool Paused() const { return mPaused; }
        void SetPaused(bool);
        bool Ready() const;
        void Draw();
        bool Poll();
        void SetWidthHeight(int, int);
        void SetAspect(float);
        float (*SetTimeCallback(float (*)()))();
        void Begin(const char *, float, bool, bool, bool, bool, int, BinStream *);
        void Terminate();
        void MovieClose();
        int MovieOpen(const char *, unsigned int);
        bool PlatformCacheFile(const char *);
        int NextFrame();

        // Layout reverse-engineered from compiled binary (m2c struct dump).
        MovieLoader *mLoader; // 0x00
        MovieLoader *mLoader2; // 0x04
        char mFilenamePad[0xC]; // 0x08 (String, 12 bytes)
        HBINK mBink; // 0x14 (bink handle)
        bool mPreloadFlag; // 0x18
        char pad19[3];
        int mUnk1C; // 0x1C
        char pad20[4];
        bool mUnk24; // 0x24
        bool mUnk25; // 0x25
        bool mUnk26; // 0x26
        char pad27[1];
        float mAspect; // 0x28
        char pad2C[0x10]; // 0x2C
        int mWidth; // 0x3C
        int mHeight; // 0x40
        bool mPaused; // 0x44
        char pad45[3];
        char mTimerPad[0x30]; // 0x48 Timer
        char pad78[0x30];
        float (*mTimeCallback)(); // 0xA8
        char padAC[8];
        int mBinkHandle; // 0xB4
        char padB8[0x18];
        bool mUnkD0; // 0xD0
        char padD1[1];
        bool mUnkD2; // 0xD2
        char padD3[1];
        unsigned int mThreadId; // 0xD4
        char padD8[4]; // 0xD8
        int mForceTrack; // 0xDC
    };

    Movie();
    ~Movie();

    int GetFrame() const;
    bool Paused() const;
    void SetAspect(float);
    bool Ready() const;
    bool Poll();
    void End();
    void SetPaused(bool);
    void Draw();
    void SetWidthHeight(int, int);
    float (*SetTimeCallback(float (*)()))();
    bool IsLoading() const;
    bool IsOpen() const;
    bool CheckOpen(bool);
    float MsPerFrame() const;
    int NumFrames() const;
    void UnlockThread();
    void LockThread();
    void Begin(const char *, float, bool, bool, bool, bool, int, BinStream *);

    static void Terminate();
    static void Validate();
    static void Init();

    static std::list<Impl *> openMovieFiles;
    Impl *mImpl;
};
