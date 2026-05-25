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

#pragma push
#pragma dont_inline on
    static void EndianSwapBuffer(void *buf, int size) {
        if ((size & 3) != 0) {
            TheDebug.Fail(MakeString(kAssertStr, "Movie.cpp", 0xae, "size % sizeof(uint32) == 0"));
        }
        char *p = (char *)buf;
        char *end = p + size;
        while (p < end) {
            unsigned int *cur = (unsigned int *)p;
            p += 4;
            EndianSwapEq(*cur);
        }
    }
#pragma pop
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
      mPreloadBuf(0), mPreloadBufLen(0), mWidth(0), mHeight(0), mPaused(false),
      mTimeCallback(0), mBinkHandle(kNoHandle), mLoading(false), mMidFrame(false),
      mThreadId((unsigned int)gMainThreadID), mMovieBuffers(0) {
    // Set time callback if is_timed_movie is configured
    bool timed;
    {
        DataArray *cfg = SystemConfig(movie, is_timed_movie);
        DataNode result = cfg->ExecuteScript(1, 0, 0, 1);
        timed = result != DataNode(0);
    }
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

Movie::Impl::MovieLoader::MovieLoader(const FilePath &fp, Movie::Impl *impl)
    : Loader(fp, kLoadFront), mFile(NULL), mOpenState(&MovieLoader::OpenFile), mImpl(impl) {}

Movie::Impl::MovieLoader::~MovieLoader() {
    delete mFile;
}

bool Movie::Impl::MovieLoader::IsLoaded() const {
    return mOpenState == &MovieLoader::DoneLoading;
}

const char *Movie::Impl::MovieLoader::StateName() const {
    return "MovieLoader";
}

void Movie::Impl::MovieLoader::PollLoading() {
    while (!TheLoadMgr.CheckSplit()) {
        Loader *front = TheLoadMgr.GetFirstLoading();
        bool atFront;
        if (front == 0) {
            atFront = (this == 0);
        } else {
            atFront = (front == this);
        }
        if (!atFront) return;
        if (IsLoaded()) return;
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
    for (std::vector<Impl *>::iterator it = sActiveMovies.begin();
         it != sActiveMovies.end();
         ++it) {
        if (*it == this) {
            sActiveMovies.erase(it);
            break;
        }
    }
    if (mPreloadBuf == NULL) {
        DataArray *videos = SystemConfig()->FindArray(Symbol("videos"), false);
        if (videos != NULL) {
            DataArray *stream_end = videos->FindArray(Symbol("stream_end"), false);
            if (stream_end != NULL) {
                stream_end->ExecuteScript(1, NULL, NULL, 1);
            }
        }
        DiscContentionPublish();
    }
    if (mBink != NULL) {
        MovieClose();
    }
    if (mLoader != NULL) {
        delete mLoader;
    }
    mLoader = NULL;
    if (mLoader2 != NULL) {
        delete mLoader2;
    }
    mLoader2 = NULL;
    if (mPreloadBuf != NULL) {
        _MemFree(mPreloadBuf);
        mPreloadBuf = NULL;
    }
    if (mMovieBuffers != NULL) {
        mMovieBuffers->mPendingBlits--;
        if (mMovieBuffers->mPendingBlits == 0) {
            delete mMovieBuffers;
        }
        mMovieBuffers = NULL;
    }
    mThreadId = (unsigned int)gMainThreadID;
}

void Movie::Impl::MovieClose() {
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
    MILO_ASSERT(ok, 0x19A);
    for (std::list<Movie::Impl *>::iterator it = Movie::openMovieFiles.begin();
         it != Movie::openMovieFiles.end();
         ++it) {
        if (*it == this) {
            Movie::openMovieFiles.erase(it);
            break;
        }
    }
    BinkClose(mBink);
    mBink = NULL;
}

void Movie::Impl::SharedFinishOpen(bool unpause) {
    sActivePending--;
    MILO_ASSERT(sActivePending >= 0, 0x282);
    if (sActivePending <= 0) {
        std::vector<Impl *> readyMovies;
        std::vector<BINK *> readyBinks;
        for (int i = 0; i < (int)sActiveMovies.size(); i++) {
            Impl *cur = sActiveMovies[i];
            if (cur->mMovieBuffers == NULL) {
                readyMovies.push_back(cur);
                readyBinks.push_back(cur->mBink);
            }
        }
        MovieInternalBuffers *buffers = MovieInternalBuffers::New(readyBinks);
        if (buffers != NULL) {
            buffers->mPendingBlits = readyMovies.size();
            for (int i = 0; i < (int)readyMovies.size(); i++) {
                Impl *cur = readyMovies[i];
                cur->mMovieBuffers = buffers;
                cur->FinishOpen();
            }
        }
        if (unpause && readyMovies.size() == 1) {
            readyMovies[0]->SetPaused(false);
        }
    }
}

void Movie::Impl::SetRect() {}

void Movie::Impl::Begin(
    const char *file,
    float aspect,
    bool soundEnabled,
    bool loop,
    bool preload,
    bool stretchToFit,
    int forceTrack,
    BinStream *stream
) {
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
    MILO_ASSERT(ok, 0x200);
    if (TheLoadMgr.mPlatform == 0) return;
    mFilename = FileMakePath(FileRoot(), file, NULL);
    if (!UsingCD()) {
        FileQualifiedFilename(mFilename, mFilename.c_str());
    }
    mPreloadFlag = preload;
    if (!PlatformCacheFile(file)) return;
    mLoop = loop;
    mSoundEnabled = soundEnabled;
    mStretchToFit = stretchToFit;
    mForceTrack = forceTrack;
    mAspect = 0.0f;
    mBinkHandle = kNoHandle;
    mIsCachedStream = false;
    mPollTimer.Reset();
    MILO_ASSERT(!mLoader, 0x21D);
    MILO_ASSERT(!mBink, 0x21E);
    MILO_ASSERT(!mPreloadBuf, 0x21F);
    if (preload) {
        const char *fn = mFilename.c_str();
        FilePath fp(fn);
        mLoader = (MovieLoader *)new FileLoader(
            fp, fn, kLoadFront, 0, false, stream != NULL, stream
        );
    } else {
        const char *fn = mFilename.c_str();
        FilePath fp(fn);
        mLoader2 = new MovieLoader(fp, this);
    }
    sActiveMovies.push_back(this);
    sActivePending++;
    if (sActivePending > 1 && !preload) {
        String localFn = mFilename;
        MILO_WARN("%s, multiple movies must be preloaded", localFn);
    }
    mLoading = true;
}

int Movie::Impl::MovieOpen(const char *file, unsigned int flags) {
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
    MILO_ASSERT(ok, 0x167);
    MILO_ASSERT(!mBink, 0x169);
    mPaused = false;
    if (gInitialized) {
        BinkInit();
        if (mForceTrack != 0) {
            if (gForceTrack != 0) mForceTrack = gForceTrack;
            MILO_LOG("localization track %d\n", mForceTrack);
            int track = mForceTrack - 1;
            BinkSetSoundTrack(1, &track);
            flags |= 0x4000;
        }
        if ((flags & 0x4000000) == 0) {
            AutoSlowFrame asf("BinkOpen");
            mBink = BinkOpen(file, flags);
        } else {
            mBink = BinkOpen(file, flags);
        }
        if (mBink != NULL) {
            openMovieFiles.push_back(this);
        } else {
            String fn = mFilename;
            MILO_WARN("BinkOpen '%s' error: %s\n", fn, BinkGetError());
        }
    }
    return 0;
}

bool Movie::Impl::CheckOpen(bool b) {
    if (!mLoading) return false;
    if (mLoader != NULL) {
        MILO_ASSERT(!mPreloadBuf, 0x3CB);
        if (!mLoader->IsLoaded()) return true;
        mLoading = false;
        mPreloadBuf = (char *)((FileLoader *)mLoader)->GetBuffer(NULL);
        mPreloadBufLen = ((FileLoader *)mLoader)->GetSize();
        if (mLoader != NULL) delete mLoader;
        mLoader = NULL;
        if (mPreloadBuf == NULL) {
            SharedFinishOpen(b);
            End();
            return false;
        }
        if (strncmp(mPreloadBuf, "BIKi", 4) == 0) {
            EndianSwapBuffer(mPreloadBuf, mPreloadBufLen);
        }
        MovieOpen(mPreloadBuf, 0x4000400);
        SharedFinishOpen(b);
    } else if (mLoader2 != NULL) {
        if (mBink != NULL) return false;
        if (!mLoader2->IsLoaded()) return true;
        mLoading = false;
        DataArray *videos = SystemConfig()->FindArray(Symbol("videos"), false);
        if (videos != NULL) {
            DataArray *stream_begin = videos->FindArray(Symbol("stream_begin"), false);
            if (stream_begin != NULL) {
                stream_begin->ExecuteScript(1, NULL, NULL, 1);
            }
        }
        UsingCD();
        MovieOpen(mFilename.c_str(), 0x2000400);
        SharedFinishOpen(b);
    }
    return false;
}

void Movie::Impl::FinishOpen() {
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
    MILO_ASSERT(ok, 0x2AC);
    if (mBink == NULL) {
        MILO_WARN("BinkOpen '%s' error: %s\n", mFilename, BinkGetError());
        return;
    }
    mSoundEnabled = mSoundEnabled || (mMovieBuffers->mPendingBlits > 1);
    BinkSetSoundOnOff(mBink, !mSoundEnabled);
    BINKSUMMARY summary;
    BinkGetSummary(mBink, &summary);
    mAspect = (float)(unsigned int)summary.Width / (float)(unsigned int)summary.Height;
    unsigned int wMod = summary.Width & 0xF;
    unsigned int hMod = summary.Height & 0xF;
    if (wMod != 0 || hMod != 0) {
        unsigned int padW = wMod != 0 ? summary.Width + (0x10 - wMod) : summary.Width;
        unsigned int padH = hMod != 0 ? summary.Height + (0x10 - hMod) : summary.Height;
        MILO_FAIL(
            "Bink movie %s must have multiples of 16 for its width and height.\nTry changing from %d x %d to %d x %d.\n",
            mFilename.c_str(),
            summary.Width,
            summary.Height,
            padW,
            padH
        );
    }
    SetRect();
    SetPaused(true);
}

void Movie::Impl::DiscContentionCheck(Loader *loader) {
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
    MILO_ASSERT(ok, 0x59);
    for (std::list<Loader *>::iterator it = TheLoadMgr.mLoading.begin();
         it != TheLoadMgr.mLoading.end();
         ++it) {
        Loader *cur = *it;
        if (cur == loader) continue;
        void *key = (void *)cur->LoaderFile().c_str();
        mDiscContentionMap[key] = cur->LoaderFile();
    }
}

void Movie::Impl::DoFrame() {
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
    MILO_ASSERT(ok, 0x342);
    if (!mPreloadFlag) {
        TheBlockMgr.MarkDiscRead();
    }
    if (mTimeCallback != NULL) {
        float dt = mTimeCallback();
        if (dt == 0.0f) {
            SetPaused(true);
            return;
        }
    }
    BeginFrame();
    BinkDoFrame(mBink);
    EndFrame();
    bool skip = true;
    if (mBink->ReadError == 0) {
        bool atEnd = false;
        if (!mLoop) {
            if (mBink->FrameNum == mBink->Frames) atEnd = true;
        }
        if (!atEnd) skip = false;
    }
    if (!skip) NextFrame();
}

void Movie::Impl::DiscContentionPublish() {
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
    MILO_ASSERT(ok, 0x68);
    String list;
    int count = 0;
    bool first = true;
    for (std::map<void *, String>::iterator it = mDiscContentionMap.begin();
         it != mDiscContentionMap.end();
         ++it) {
        if (!first) list += ", ";
        list += it->second;
        first = false;
        count++;
    }
    if (count != 0) {
        MILO_LOG("Streaming Bink Thrashed with %d files: (%s)\n", count, list);
        if (!mDiscContentionMap.empty()) {
            mDiscContentionMap.clear();
        }
    }
}

void Movie::Impl::SetPaused(bool b) {
    if (mTimeCallback != NULL) {
        float dt = mTimeCallback();
        if (dt == 0.0f) {
            b = true;
        }
    }
    if (mPaused == b) return;
    if (mBink == NULL) return;
    if (mMovieBuffers->mPendingBlits > 1 && sAsyncMovie != NULL && !b) {
        sNextMovie = this;
        return;
    }
    if (!b) LockThread();
    if (!b && mPreloadBuf != NULL && mTimeCallback != NULL) {
        MovieClose();
        MovieOpen(mPreloadBuf, 0x4000400);
        BINKFRAMEBUFFERS info;
        BinkGetFrameBuffersInfo(mBink, &info);
        BinkRegisterFrameBuffers(mBink, &mMovieBuffers->mBuffers);
        mPaused = true;
        FinishOpen();
    }
    BinkPause(mBink, b);
    mPaused = b;
    if (b) UnlockThread();
}
