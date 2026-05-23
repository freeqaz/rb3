#include "movie/Movie.h"
#include "movie/TexMovie.h"
#include "obj/Data.h"
#include "obj/DataFunc.h"
#include "obj/ObjMacros.h"
#include "obj/Task.h"
#include "os/CritSec.h"
#include "os/Debug.h"
#include "os/OSFuncs.h"
#include "os/System.h"
#include "os/Timer.h"
#include "utl/MemMgr.h"
#include <list>
#include <vector>

extern "C" {
    void BinkSetMemory(void *(*)(unsigned int), void (*)(void *));
}

int gBinkCore0 = -1;
int gBinkCore1 = -1;

static const unsigned int kNoThread = 0;

std::vector<Movie::Impl *> Movie::Impl::sActiveMovies;

namespace {
    CriticalSection gMovieCrit;
    bool gInitialized;
    int gForceTrack;

    void *RadAlloc(unsigned int size) { return _MemAlloc(size, 0x80); }
    void RadFree(void *p) { _MemFree(p); }
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
    DataArray *cfg = SystemConfig(Symbol("movie"));
    cfg->FindData(Symbol("bink_core0"), gBinkCore0, true);
    cfg->FindData(Symbol("bink_core1"), gBinkCore1, true);
    if (!gInitialized) {
        sActiveMovies.reserve(0x10);
        REGISTER_OBJ_FACTORY(TexMovie)
        TheDebug.AddExitCallback(Movie::Terminate);
        BinkSetMemory(RadAlloc, RadFree);
        Movie::Impl::PlatformInit();
        gInitialized = true;
    }
    DataRegisterFunc(Symbol("set_bink_track"), OnMovieSetTrack);
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
        ms = 1000.0f;
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
    extern void BinkNextFrame(HBINK);
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
