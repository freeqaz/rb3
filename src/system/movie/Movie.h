#pragma once

#include "utl/BinStream.h"
#include "utl/BINK.h"
#include "utl/FilePath.h"
#include "utl/Loader.h"
#include "utl/Str.h"
#include "os/File.h"
#include "os/Timer.h"
#include <list>
#include <map>
#include <vector>

// Wii-specific Bink frame-buffer wrapper (defined in Movie_Wii.cpp).
// Layout exposed here so Movie.cpp can poke the fields used by Begin/EndFrame.
struct MovieInternalBuffers {
    void *mUnk0; // 0x00
    BINKFRAMEBUFFERS mBuffers; // 0x04 (TotalFrames at 0x04, FrameNum at 0x18)
    char mPadInner[0x1DC - 0x4 - 0x78]; // pad up to 0x1DC
    int mPendingBlits; // 0x1DC (incremented on async submit, decremented on completion)
    int mNextFrameIdx; // 0x1E0
};

class Movie {
public:
    class Impl {
    public:
        class MovieLoader : public Loader {
        public:
            MovieLoader(const FilePath &, Movie::Impl *);
            virtual ~MovieLoader();
            virtual const char *DebugText() { return Loader::DebugText(); }
            virtual bool IsLoaded() const;
            virtual const char *StateName() const;
            virtual void PollLoading();

            void OpenFile();
            void LoadFile();
            void DoneLoading();

            typedef void (MovieLoader::*StateFunc)();

            File *mFile; // 0x18 (open file handle)
            StateFunc mOpenState; // 0x1C (12 bytes: fn_ptr_word0, adj=-1, fn_ptr)
            char mBuffer[0x20]; // 0x28 (read buffer)
            Movie::Impl *mImpl; // 0x48
        };

        Impl();
        ~Impl();
        static void Init();
        static void PlatformInit();
        static std::vector<Impl *> sActiveMovies;
        static Impl *sAsyncMovie;
        static int sActivePending;
        static Impl *sNextMovie;
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
        void DiscContentionCheck(Loader *);
        void DiscContentionPublish();
        void SharedFinishOpen(bool);
        void FinishOpen();
        void SetRect();
        void DoFrame();
        void BeginFrame();
        void EndFrame();

        // Layout from binary analysis
        MovieLoader *mLoader; // 0x00
        MovieLoader *mLoader2; // 0x04
        String mFilename; // 0x08 (12 bytes)
        BINK *mBink; // 0x14
        bool mPreloadFlag; // 0x18 (is preloaded)
        char pad19[3];
        char *mPreloadBuf; // 0x1C
        int mPreloadBufLen; // 0x20
        bool mLoop; // 0x24
        bool mSoundEnabled; // 0x25
        bool mStretchToFit; // 0x26
        char pad27[1];
        float mAspect; // 0x28
        float mRectX1; // 0x2C
        float mRectX2; // 0x30
        float mRectY1; // 0x34
        float mRectY2; // 0x38
        int mWidth; // 0x3C
        int mHeight; // 0x40
        bool mPaused; // 0x44
        char pad45[3];
        Timer mPollTimer; // 0x48 (0x30 bytes)
        Timer mFrameTimer; // 0x78 (0x30 bytes)
        float (*mTimeCallback)(); // 0xA8
        int mCurFrame; // 0xAC
        int mNextFrame; // 0xB0
        int mBinkHandle; // 0xB4
        std::map<void *, String> mDiscContentionMap; // 0xB8 (0x18 bytes for _Rb_tree)
        bool mLoading; // 0xD0
        bool mMidFrame; // 0xD1
        bool mIsCachedStream; // 0xD2
        char padD3[1];
        unsigned int mThreadId; // 0xD4
        int mForceTrack; // 0xD8 (sound track index)
        MovieInternalBuffers *mMovieBuffers; // 0xDC
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
