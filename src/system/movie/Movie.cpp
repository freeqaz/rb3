#include "movie/Movie.h"
#include "movie/TexMovie.h"
#include "obj/Data.h"
#include "obj/DataFunc.h"
#include "obj/ObjMacros.h"
#include "obj/Task.h"
#include "os/BlockMgr.h"
#include "os/CritSec.h"
#include "os/Debug.h"
#include "os/Endian.h"
#include "os/File.h"
#include "os/OSFuncs.h"
#include "os/System.h"
#include "os/Timer.h"
#include "utl/BinkIntegration.h"
#include "utl/Loader.h"
#include "utl/MemMgr.h"
#include "utl/Symbols3.h"
#include "utl/Symbols4.h"
#include <list>
#include <vector>

extern "C" {
    void BinkSetMemory(void *(*)(unsigned int), void (*)(void *));
    BINK *BinkOpen(const char *, unsigned int);
    void BinkClose(BINK *);
    void BinkDoFrame(BINK *);
    void BinkNextFrame(BINK *);
    int BinkWait(BINK *);
    int BinkShouldSkip(BINK *);
    char *BinkGetError();
    void BinkSetSoundOnOff(BINK *, int);
    void BinkGetSummary(BINK *, void *);
    void BinkSetSoundTrack(int, int *);
    void BinkGetFrameBuffersInfo(BINK *, BINKFRAMEBUFFERS *);
    void BinkRegisterFrameBuffers(BINK *, BINKFRAMEBUFFERS *);
    void BinkPause(BINK *, int);
}

extern int kNoHandle;
int gBinkCore0 = -1;
int gBinkCore1 = -1;

static const unsigned int kNoThread = 0;

std::vector<Movie::Impl *> Movie::Impl::sActiveMovies;
Movie::Impl *Movie::Impl::sAsyncMovie;
int Movie::Impl::sActivePending;
Movie::Impl *Movie::Impl::sNextMovie;

namespace {
    CriticalSection gMovieCrit;
    bool gInitialized;
    int gForceTrack;

    void *RadAlloc(unsigned int size) { return _MemAlloc(size, 0x80); }
    void RadFree(void *p) { _MemFree(p); }
    static void EndianSwapBuffer(void *buf, int len) {
        MILO_ASSERT((len & 3) == 0, 0xae);
        unsigned int *p = (unsigned int *)buf;
        unsigned int *end = p + (len / 4);
        while (p < end) {
            unsigned int *cur = p;
            p++;
            EndianSwapEq(*cur);
        }
    }
}

static DataNode OnMovieSetTrack(DataArray *arr) {
    gForceTrack = arr->Node(1).Int(arr);
    return DataNode();
}

std::list<Movie::Impl *> Movie::openMovieFiles;

Movie::Movie() {
    mImpl = new Movie::Impl();
    MILO_ASSERT(mImpl, 0x647);
}

Movie::~Movie() {
    delete mImpl;
}

void Movie::Impl::Init() {
    CriticalSection *cs = &gMovieCrit;
    if (cs) cs->Enter();
    DataArray *cfg = SystemConfig("movie");
    cfg->FindData("bink_core0", gBinkCore0, true);
    cfg->FindData("bink_core1", gBinkCore1, true);
    if (!gInitialized) {
        sActiveMovies.reserve(0x10);
        REGISTER_OBJ_FACTORY(TexMovie)
        TheDebug.AddExitCallback(Movie::Terminate);
        BinkSetMemory(RadAlloc, RadFree);
        Movie::Impl::PlatformInit();
        gInitialized = true;
    }
    DataRegisterFunc("set_bink_track", OnMovieSetTrack);
    if (cs) cs->Exit();
}

void Movie::Init() { Movie::Impl::Init(); }

void Movie::Terminate() {
    CriticalSection *cs = &gMovieCrit;
    if (cs)
        cs->Enter();
    std::list<Movie::Impl *>::iterator sentinel = openMovieFiles.end();
    int count;
    goto check;
    do {
        openMovieFiles.back()->Terminate();
    check:
        count = 0;
        for (std::list<Movie::Impl *>::iterator it = openMovieFiles.begin(); it != sentinel; ++it)
            count++;
    } while (count != 0);
    gInitialized = false;
    if (cs)
        cs->Exit();
}

bool Movie::Poll() {
    START_AUTO_TIMER("movie");
    return mImpl->Poll();
}

void Movie::Draw() {
    START_AUTO_TIMER("movie");
    mImpl->Draw();
}

void Movie::End() { mImpl->End(); }
bool Movie::IsOpen() const { return mImpl->IsOpen(); }
bool Movie::IsLoading() const { return mImpl->IsLoading(); }
bool Movie::CheckOpen(bool b) { return mImpl->CheckOpen(b); }
void Movie::LockThread() { mImpl->LockThread(); }
void Movie::UnlockThread() { mImpl->UnlockThread(); }
int Movie::GetFrame() const { return mImpl->GetFrame(); }
float Movie::MsPerFrame() const { return mImpl->MsPerFrame(); }
int Movie::NumFrames() const { return mImpl->NumFrames(); }
void Movie::SetPaused(bool b) { mImpl->SetPaused(b); }
bool Movie::Paused() const { return mImpl->Paused(); }
void Movie::Begin(const char *file, float aspect, bool b1, bool b2, bool b3, bool b4, int i, BinStream *bs) {
    MILO_ASSERT(gInitialized, 0x65A);
    mImpl->Begin(file, aspect, b1, b2, b3, b4, i, bs);
}
bool Movie::Ready() const { return mImpl->Ready(); }
void Movie::SetAspect(float f) { mImpl->SetAspect(f); }
float (*Movie::SetTimeCallback(float (*cb)()))() { return mImpl->SetTimeCallback(cb); }
void Movie::SetWidthHeight(int w, int h) { mImpl->SetWidthHeight(w, h); }
void Movie::Validate() {}

float TaskMgrDeltaSeconds() { return TheTaskMgr.DeltaSeconds(); }

void Movie::Impl::SetAspect(float f) { mAspect = f; }

void Movie::Impl::LockThread() {
    MILO_ASSERT(mThreadId == 0, 0x5C2);
    mThreadId = (unsigned int)OSGetCurrentThread();
}

void Movie::Impl::UnlockThread() {
    MILO_ASSERT(mThreadId == (unsigned int)CurrentThreadId(), 0x5BD);
    mThreadId = 0;
}

bool Movie::Impl::IsOpen() const {
    bool ok = true;
    if (mThreadId != (unsigned int)OSGetCurrentThread()) {
        unsigned int tid = mThreadId;
        bool b = false;
        if (tid == kNoThread) {
            bool main = true;
            if (gMainThreadID != 0 && gMainThreadID != OSGetCurrentThread()) main = false;
            if (main) b = true;
        }
        if (!b) ok = false;
    }
    MILO_ASSERT(ok, 0x159);
    return mBink != 0;
}

float (*Movie::Impl::SetTimeCallback(float (*cb)()))() {
    bool ok = true;
    if (mThreadId != (unsigned int)OSGetCurrentThread()) {
        unsigned int tid = mThreadId;
        bool b = false;
        if (tid == kNoThread) {
            bool main = true;
            if (gMainThreadID != 0 && gMainThreadID != OSGetCurrentThread()) main = false;
            if (main) b = true;
        }
        if (!b) ok = false;
    }
    MILO_ASSERT(ok, 0x3B2);
    float (*old)() = mTimeCallback;
    mTimeCallback = cb;
    return old;
}

void Movie::Impl::SetWidthHeight(int w, int h) {
    bool ok = true;
    if (mThreadId != (unsigned int)OSGetCurrentThread()) {
        unsigned int tid = mThreadId;
        bool b = false;
        if (tid == kNoThread) {
            bool main = true;
            if (gMainThreadID != 0 && gMainThreadID != OSGetCurrentThread()) main = false;
            if (main) b = true;
        }
        if (!b) ok = false;
    }
    MILO_ASSERT(ok, 0x555);
    mWidth = w;
    mHeight = h;
}

float Movie::Impl::MsPerFrame() const {
    bool ok = true;
    if (mThreadId != (unsigned int)OSGetCurrentThread()) {
        unsigned int tid = mThreadId;
        bool b = false;
        if (tid == kNoThread) {
            bool main = true;
            if (gMainThreadID != 0 && gMainThreadID != OSGetCurrentThread()) main = false;
            if (main) b = true;
        }
        if (!b) ok = false;
    }
    MILO_ASSERT(ok, 0x634);
    float ms;
    if (mBink != NULL) {
        ms = 1000.0f * (float)mBink->FrameRateDiv / (float)mBink->FrameRate;
    } else {
        ms = 0.0f;
    }
    return ms;
}

int Movie::Impl::NextFrame() {
    bool ok = true;
    if (mThreadId != (unsigned int)OSGetCurrentThread()) {
        unsigned int tid = mThreadId;
        bool b = false;
        if (tid == kNoThread) {
            bool main = true;
            if (gMainThreadID != 0 && gMainThreadID != OSGetCurrentThread()) main = false;
            if (main) b = true;
        }
        if (!b) ok = false;
    }
    MILO_ASSERT(ok, 0x2D0);
    BinkNextFrame(mBink);
    return 0;
}

int Movie::Impl::GetFrame() const {
    bool ok = true;
    if (mThreadId != (unsigned int)OSGetCurrentThread()) {
        unsigned int tid = mThreadId;
        bool b = false;
        if (tid == kNoThread) {
            bool main = true;
            if (gMainThreadID != 0 && gMainThreadID != OSGetCurrentThread()) main = false;
            if (main) b = true;
        }
        if (!b) ok = false;
    }
    MILO_ASSERT(ok, 0x626);
    int frame;
    if (mBink != NULL) {
        unsigned int fn = mBink->FrameNum;
        if (fn == 1) {
            frame = mBink->Frames;
        } else {
            frame = (int)(fn - 1);
        }
    } else {
        frame = 0;
    }
    return frame;
}

int Movie::Impl::NumFrames() const {
    bool ok = true;
    if (mThreadId != (unsigned int)OSGetCurrentThread()) {
        unsigned int tid = mThreadId;
        bool b = false;
        if (tid == kNoThread) {
            bool main = true;
            if (gMainThreadID != 0 && gMainThreadID != OSGetCurrentThread()) main = false;
            if (main) b = true;
        }
        if (!b) ok = false;
    }
    MILO_ASSERT(ok, 0x63F);
    return mBink ? mBink->Frames : 0;
}

void Movie::Impl::Terminate() {
    bool ok = true;
    if (mThreadId != (unsigned int)OSGetCurrentThread()) {
        unsigned int tid = mThreadId;
        bool b = false;
        if (tid == kNoThread) {
            bool main = true;
            if (gMainThreadID != 0 && gMainThreadID != OSGetCurrentThread()) main = false;
            if (main) b = true;
        }
        if (!b) ok = false;
    }
    MILO_ASSERT(ok, 0x4D7);
    if (mBink != 0) MovieClose();
}

bool Movie::Impl::IsLoading() const {
    bool ok = true;
    if (mThreadId != (unsigned int)OSGetCurrentThread()) {
        unsigned int tid = mThreadId;
        bool b = false;
        if (tid == kNoThread) {
            bool main = true;
            if (gMainThreadID != 0 && gMainThreadID != OSGetCurrentThread()) main = false;
            if (main) b = true;
        }
        if (!b) ok = false;
    }
    MILO_ASSERT(ok, 0x160);
    return mLoader != NULL || mLoader2 != NULL;
}

Movie::Impl::Impl()
    : mLoader(0), mLoader2(0), mFilename(), mBink(0), mPreloadFlag(false),
      mPreloadBuf(0), mPreloadBufLen(0), mLoop(false), mSoundEnabled(false),
      mStretchToFit(false), mAspect(0.0f), mRectX1(0.0f), mRectX2(0.0f),
      mRectY1(0.0f), mRectY2(0.0f), mWidth(0), mHeight(0), mPaused(false),
      mTimeCallback(0), mCurFrame(0), mNextFrame(0),
      mBinkHandle(kNoHandle), mLoading(false), mMidFrame(false),
      mThreadId((unsigned int)gMainThreadID), mForceTrack(0),
      mMovieBuffers(0) {
    // Set time callback if is_timed_movie is configured
    DataArray *cfg = SystemConfig(movie, is_timed_movie);
    DataNode result = cfg->ExecuteScript(1, 0, 0, 1);
    bool timed = result != DataNode(0);
    if (timed) {
        mTimeCallback = TaskMgrDeltaSeconds;
    }
    // Check that we're on the appropriate thread
    bool ok = true;
    if (mThreadId != (unsigned int)OSGetCurrentThread()) {
        unsigned int tid = mThreadId;
        bool b = false;
        if (tid == kNoThread) {
            bool main = true;
            if (gMainThreadID != 0 && gMainThreadID != OSGetCurrentThread()) main = false;
            if (main) b = true;
        }
        if (!b) ok = false;
    }
    MILO_ASSERT(ok, 0x1C8);
}

Movie::Impl::~Impl() {
    if (this == 0) return;
    bool ok = true;
    if (mThreadId != (unsigned int)OSGetCurrentThread()) {
        unsigned int tid = mThreadId;
        bool b = false;
        if (tid == kNoThread) {
            bool main = true;
            if (gMainThreadID != 0 && gMainThreadID != OSGetCurrentThread()) main = false;
            if (main) b = true;
        }
        if (!b) ok = false;
    }
    MILO_ASSERT(ok, 0x1CD);
    End();
    if (mDiscContentionMap.size() != 0) {
        mDiscContentionMap.clear();
    }
}

bool Movie::Impl::Ready() const {
    bool ok = true;
    if (mThreadId != (unsigned int)OSGetCurrentThread()) {
        unsigned int tid = mThreadId;
        bool b = false;
        if (tid == kNoThread) {
            bool main = true;
            if (gMainThreadID != 0 && gMainThreadID != OSGetCurrentThread()) main = false;
            if (main) b = true;
        }
        if (!b) ok = false;
    }
    MILO_ASSERT(ok, 0x1D5);
    if (mLoader != 0) {
        return mLoader->IsLoaded();
    } else if (mLoader2 != 0) {
        return mLoader2->IsLoaded();
    } else {
        return true;
    }
}

bool Movie::Impl::MovieLoader::IsLoaded() const {
    return mOpenState == &MovieLoader::DoneLoading;
}

const char *Movie::Impl::MovieLoader::StateName() const {
    return "MovieLoader";
}

void Movie::Impl::MovieLoader::PollLoading() {
    (this->*mOpenState)();
    while (!TheLoadMgr.CheckSplit()) {
        Loader *front = TheLoadMgr.GetFirstLoading();
        bool atFront;
        if (front == 0) {
            atFront = (this == 0);
        } else {
            atFront = (front == this);
        }
        if (!atFront) break;
        if (IsLoaded()) break;
        (this->*mOpenState)();
    }
}

void Movie::Impl::MovieLoader::OpenFile() {}
void Movie::Impl::MovieLoader::LoadFile() {}
void Movie::Impl::MovieLoader::DoneLoading() {}

void Movie::Impl::BeginFrame() {
    bool ok = true;
    if (mThreadId != (unsigned int)OSGetCurrentThread()) {
        unsigned int tid = mThreadId;
        bool b = false;
        if (tid == kNoThread) {
            bool main = true;
            if (gMainThreadID != 0 && gMainThreadID != OSGetCurrentThread()) main = false;
            if (main) b = true;
        }
        if (!b) ok = false;
    }
    MILO_ASSERT(ok, 0x370);
    sAsyncMovie = this;
    mMidFrame = true;
    MovieInternalBuffers *bufs = mMovieBuffers;
    mCurFrame = (bufs->mBuffers.FrameNum + 1) % bufs->mBuffers.TotalFrames;
    mNextFrame = (int)(bufs->mNextFrameIdx + 1) % (int)bufs->mBuffers.TotalFrames;
}

void Movie::Impl::EndFrame() {
    bool ok = true;
    if (mThreadId != (unsigned int)OSGetCurrentThread()) {
        unsigned int tid = mThreadId;
        bool b = false;
        if (tid == kNoThread) {
            bool main = true;
            if (gMainThreadID != 0 && gMainThreadID != OSGetCurrentThread()) main = false;
            if (main) b = true;
        }
        if (!b) ok = false;
    }
    MILO_ASSERT(ok, 0x38F);
    if (mMidFrame) {
        sAsyncMovie = NULL;
        mMidFrame = false;
        mMovieBuffers->mNextFrameIdx++;
        if (mMovieBuffers->mNextFrameIdx >= (int)mMovieBuffers->mBuffers.TotalFrames) {
            mMovieBuffers->mNextFrameIdx = 0;
        }
    } else {
        MILO_WARN("mMidFrame");
    }
}

// TODO: full implementation. Stub so ~Impl can link.
void Movie::Impl::End() {
    bool ok = true;
    if (mThreadId != (unsigned int)OSGetCurrentThread()) {
        unsigned int tid = mThreadId;
        bool b = false;
        if (tid == kNoThread) {
            bool main = true;
            if (gMainThreadID != 0 && gMainThreadID != OSGetCurrentThread()) main = false;
            if (main) b = true;
        }
        if (!b) ok = false;
    }
    MILO_ASSERT(ok, 0x498);
    if (mLoading) {
        mLoading = false;
        SharedFinishOpen(false);
    }
}
