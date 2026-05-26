#include "movie/Movie.h"
#include "movie/TexMovie.h"
#include "obj/Data.h"
#include "obj/DataFunc.h"
#include "obj/Dir.h"
#include "obj/Msg.h"
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
#include "rndobj/Rnd.h"
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

// MILO_ASSERT stringifies #cond verbatim. mThreadId is unsigned int but
// ::CurrentThreadId() returns OSThread*, so we wrap the call to insert the
// cast at evaluation time while keeping the source text clean for #cond.
// (Without this, source must write `(unsigned int)CurrentThreadId()`, which
// pool-shifts the assert string compared to the target binary.)
#define CurrentThreadId() ((unsigned int)::CurrentThreadId())

// The on-thread check appears as imperative ladder code in target asm (one
// outer if-test, then a MainThread() check), but the assertion FAIL path
// stringifies the FULL logical expression. We reproduce both: the imperative
// ladder lives in ASSERT_MOVIE_THREAD's body (computes `ok`), and the failure
// path uses an explicit string literal that matches target's pool entry.
#define MOVIE_THREAD_COND_STR \
    "mThreadId == CurrentThreadId() || (mThreadId == kNoThread && MainThread())"
#define ASSERT_MOVIE_THREAD(line)                                                          \
    do {                                                                                   \
        bool ok = true;                                                                    \
        if (mThreadId != (unsigned int)OSGetCurrentThread()) {                             \
            unsigned int tid = mThreadId;                                                  \
            bool b = false;                                                                \
            if (tid == kNoThread) {                                                        \
                bool main = true;                                                          \
                if (gMainThreadID != 0 && gMainThreadID != OSGetCurrentThread())           \
                    main = false;                                                          \
                if (main) b = true;                                                        \
            }                                                                              \
            if (!b) ok = false;                                                            \
        }                                                                                  \
        (ok) || (TheDebugFailer                                                            \
                 << (MakeString(kAssertStr, __FILE__, line, MOVIE_THREAD_COND_STR)),       \
                 0);                                                                       \
    } while (false)

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
            TheDebug.Fail(MakeString(kAssertStr, __FILE__, 0xae, "size % sizeof(uint32) == 0"));
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

float gsw;
float gsh;
float gmw;
float gmh;
float gdw;
float gdh;

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
    int count;
    goto check;
    do {
        openMovieFiles.back()->Terminate();
    check:
        count = 0;
        for (std::list<Movie::Impl *>::iterator it = openMovieFiles.begin(); it != openMovieFiles.end(); ++it)
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
    MILO_ASSERT(mThreadId == kNoThread, 0x5C2);
    mThreadId = (unsigned int)OSGetCurrentThread();
}

void Movie::Impl::UnlockThread() {
    MILO_ASSERT(mThreadId == CurrentThreadId(), 0x5BD);
    mThreadId = 0;
}

bool Movie::Impl::IsOpen() const {
    ASSERT_MOVIE_THREAD(0x159);
    return mBink != 0;
}

float (*Movie::Impl::SetTimeCallback(float (*cb)()))() {
    ASSERT_MOVIE_THREAD(0x3B2);
    float (*old)() = mTimeCallback;
    mTimeCallback = cb;
    return old;
}

void Movie::Impl::SetWidthHeight(int w, int h) {
    ASSERT_MOVIE_THREAD(0x555);
    mWidth = w;
    mHeight = h;
}

float Movie::Impl::MsPerFrame() const {
    float ms;
    ASSERT_MOVIE_THREAD(0x634);
    if (mBink != NULL) {
        ms = 1000.0f * (float)mBink->FrameRateDiv / (float)mBink->FrameRate;
    } else {
        ms = 0.0f;
    }
    return ms;
}

void Movie::Impl::NextFrame() {
    ASSERT_MOVIE_THREAD(0x2D0);
    BinkNextFrame(mBink);
}

int Movie::Impl::GetFrame() const {
    ASSERT_MOVIE_THREAD(0x626);
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
    ASSERT_MOVIE_THREAD(0x63F);
    return mBink ? mBink->Frames : 0;
}

void Movie::Impl::Terminate() {
    ASSERT_MOVIE_THREAD(0x4D7);
    if (mBink != 0) MovieClose();
}

bool Movie::Impl::IsLoading() const {
    ASSERT_MOVIE_THREAD(0x160);
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
    ASSERT_MOVIE_THREAD(0x1C8);
}

Movie::Impl::~Impl() {
    if (this == 0) return;
    ASSERT_MOVIE_THREAD(0x1CD);
    End();
}

bool Movie::Impl::Ready() const {
    ASSERT_MOVIE_THREAD(0x1D5);
    if (mLoader != 0) {
        return mLoader->IsLoaded();
    } else if (mLoader2 != 0) {
        return mLoader2->IsLoaded();
    } else {
        return true;
    }
}

inline Movie::Impl::MovieLoader::MovieLoader(const FilePath &fp, Movie::Impl *impl)
    : Loader(fp, kLoadStayBack), mFile(NULL), mOpenState(&MovieLoader::OpenFile), mImpl(impl) {}

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
    while (!TheLoadMgr.CheckSplit() && TheLoadMgr.GetFirstLoading() == this && !IsLoaded()) {
        (this->*mOpenState)();
    }
}

void Movie::Impl::MovieLoader::OpenFile() {
    // Loader::mFile is a FilePath (offset 0x8 in base). Our local File* is also mFile.
    // Use this-> on local to disambiguate, plus use Loader::mFile via .c_str() for filename.
    this->mFile = NewFile(Loader::mFile.c_str(), 2);
    if (NULL != this->mFile && !this->mFile->Fail()) {
        this->mFile->ReadAsync(mBuffer, 0x20);
        mOpenState = &MovieLoader::LoadFile;
    } else {
        MILO_WARN("Could not load: %s", (char *)FileLocalize(Loader::mFile.c_str(), NULL));
        mOpenState = &MovieLoader::DoneLoading;
    }
}
void Movie::Impl::MovieLoader::LoadFile() {
    if (this->mFile == NULL) {
        TheDebug.Fail(MakeString(kAssertStr, "MovieLoader_p.h", 0x47, "mFile"));
    }
    int bytesRead;
    if (this->mFile->ReadDone(bytesRead)) {
        if (!this->mFile->Fail()) {
            mImpl->DiscContentionCheck(this);
        }
        mOpenState = &MovieLoader::DoneLoading;
    }
}
void Movie::Impl::MovieLoader::DoneLoading() {}

void Movie::Impl::BeginFrame() {
    ASSERT_MOVIE_THREAD(0x370);
    sAsyncMovie = this;
    mMidFrame = true;
    MovieInternalBuffers *bufs = mMovieBuffers;
    mCurFrame = (bufs->mBuffers.FrameNum + 1) % bufs->mBuffers.TotalFrames;
    mNextFrame = (int)(bufs->mNextFrameIdx + 1) % (int)bufs->mBuffers.TotalFrames;
}

void Movie::Impl::EndFrame() {
    bool &_ref0 = mMidFrame;
    ASSERT_MOVIE_THREAD(0x38F);
    if (_ref0) {
        sAsyncMovie = NULL;
        _ref0 = false;
        mMovieBuffers->mNextFrameIdx++;
        if (mMovieBuffers->mNextFrameIdx >= (int)mMovieBuffers->mBuffers.TotalFrames) {
            mMovieBuffers->mNextFrameIdx = 0;
        }
    } else {
        MILO_WARN("mMidFrame");
    }
}

void Movie::Impl::End() {
    ASSERT_MOVIE_THREAD(0x498);
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
    ASSERT_MOVIE_THREAD(0x19A);
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
    MILO_ASSERT(0 <= sActivePending, 0x282);
    if (sActivePending <= 0) {
        std::vector<Impl *> readyMovies;
        std::vector<BINK *> readyBinks;
        for (int i = 0; i < sActiveMovies.size(); i++) {
            Impl *cur = sActiveMovies[i];
            if (cur->mMovieBuffers == NULL) {
                readyMovies.push_back(cur);
                readyBinks.push_back(cur->mBink);
            }
        }
        MovieInternalBuffers *buffers = MovieInternalBuffers::New(readyBinks);
        if (buffers != NULL) {
            buffers->mPendingBlits = readyMovies.size();
            for (int i = 0; i < readyMovies.size(); i++) {
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

void Movie::Impl::SetRect() {
    ASSERT_MOVIE_THREAD(0x24C);
    float screenW = mWidth != 0 ? (float)mWidth : (float)TheRnd->Width();
    float screenH = mHeight != 0 ? (float)mHeight : (float)TheRnd->Height();
    MILO_ASSERT(mAspect, 0x252);
    float h;
    float w;
    if (mStretchToFit) {
        h = screenW;
        w = screenW * mAspect;
    } else {
        w = screenH;
        h = screenH / mAspect;
        if (mWidth == 0 && TheRnd->mAspect == Rnd::kWidescreen) {
            h /= 1.3333334f;
        }
    }
    float dx = (h - screenW) * 0.5f;
    float dy = (w - screenH) * 0.5f;
    mRectX1 = -dx;
    mRectY1 = -dy;
    mRectX2 = h - dx;
    mRectY2 = w - dy;
    gsw = screenW;
    gsh = screenH;
    gmw = h;
    gmh = w;
    gdw = dx;
    gdh = dy;
}

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
    ASSERT_MOVIE_THREAD(0x200);
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
    ASSERT_MOVIE_THREAD(0x167);
    BINK * &_ref0 = mBink;
    MILO_ASSERT(!_ref0, 0x169);
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
            _ref0 = BinkOpen(file, flags);
        } else {
            _ref0 = BinkOpen(file, flags);
        }
        if (_ref0 != NULL) {
            openMovieFiles.push_back(this);
        } else {
            MILO_WARN("BinkOpen '%s' error: %s\n", mFilename, BinkGetError());
        }
    }
    return 0;
}

bool Movie::Impl::CheckOpen(bool b) {
    if (mLoading) {
        if (mLoader != NULL) {
            MILO_ASSERT(!mPreloadBuf, 0x3CB);
            if (!mLoader->IsLoaded()) return true;
            mLoading = false;
            mPreloadBuf = (char *)((FileLoader *)mLoader)->GetBuffer(NULL);
            mPreloadBufLen = ((FileLoader *)mLoader)->GetSize();
            delete mLoader;
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
            if (mBink == NULL) {
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
        }
    }
    return false;
}

void Movie::Impl::FinishOpen() {
    ASSERT_MOVIE_THREAD(0x2AC);
    BINK * &_ref0 = mBink;
    if (_ref0 == NULL) {
        MILO_WARN("BinkOpen '%s' error: %s\n", mFilename, BinkGetError());
        return;
    }
    mSoundEnabled = mSoundEnabled | (mMovieBuffers->mPendingBlits > 1);
    BinkSetSoundOnOff(_ref0, !mSoundEnabled);
    BINKSUMMARY summary;
    BinkGetSummary(_ref0, &summary);
    mAspect = (float)(unsigned int)summary.Width / (float)(unsigned int)summary.Height;
    unsigned int wMod = summary.Width & 0xF;
    unsigned int hMod = summary.Height & 0xF;
    if (wMod != 0 || hMod != 0) {
        unsigned int padH = summary.Height + (hMod != 0 ? (0x10 - hMod) : 0u);
        unsigned int padW = summary.Width + (wMod != 0 ? (0x10 - wMod) : 0u);
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
    ASSERT_MOVIE_THREAD(0x59);
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
    ASSERT_MOVIE_THREAD(0x342);
    if (!mPreloadFlag) {
        TheBlockMgr.MarkDiscRead();
    }
    if (mTimeCallback != NULL) {
        float dt = mTimeCallback();
        bool atZ = (dt == 0);
        if (atZ) {
            SetPaused(atZ);
            return;
        }
    }
    BeginFrame();
    BINK * &_ref0 = mBink;
    BinkDoFrame(_ref0);
    EndFrame();
    bool skip = true;
    if (_ref0->ReadError == 0) {
        bool atEnd = false;
        if (!mLoop) {
            if (_ref0->FrameNum == _ref0->Frames) atEnd = true;
        }
        if (!atEnd) skip = false;
    }
    if (!skip) NextFrame();
}

void Movie::Impl::DiscContentionPublish() {
    ASSERT_MOVIE_THREAD(0x68);
    unsigned char first = 1;
    int count = 0;
    String list;
    for (std::map<void *, String>::iterator it = mDiscContentionMap.begin();
         it != mDiscContentionMap.end();
         ++it) {
        if (!first) list += ", ";
        first = 0;
        list += it->second;
        count++;
    }
    if (count != 0) {
        TheDebug.Notify(MakeString("Streaming Bink Thrashed with %d files: (%s)\n", count, list));
        mDiscContentionMap.clear();
    }
}

bool Movie::Impl::Poll() {
    ASSERT_MOVIE_THREAD(0x424);
    if (sAsyncMovie != NULL && sAsyncMovie != this) {
        return sAsyncMovie->Poll();
    }
    if (CheckOpen(true)) {
        return true;
    }
    if (mBink == NULL) return false;
    if (!mPaused) {
        float ms = mPollTimer.SplitMs();
        mPollTimer.Restart();
        if (ms > 49.0f) {
            String fn(mFilename);
            float bms = mFrameTimer.SplitMs();
            const char *msg = MakeString("GLITCH: %g ms (%g ms bink), %s\n", ms, bms, fn);
            static DataNode &notify_level = DataVariable("notify_level");
            if (notify_level.Int(NULL) == 0) {
                TheDebug << MakeString("%s\n", msg);
            } else {
                static Hmx::Object *cd = ObjectDir::Main()->Find<Hmx::Object>("cheat_display", true);
                static Message show(Symbol("show"), DataNode(""));
                show[0] = DataNode(msg);
                cd->Handle(show, false);
            }
        }
        DiscContentionCheck(NULL);
    }
    if (mTimeCallback != NULL && sAsyncMovie == NULL) {
        float dt = mTimeCallback();
        SetPaused(dt == 0.0f);
    }
    mFrameTimer.Restart();
    if (BinkWait(mBink) == 0) {
        DoFrame();
        while (BinkShouldSkip(mBink)) {
            DoFrame();
        }
    }
    mFrameTimer.SplitMs();
    if (mBink->ReadError != 0) return false;
    if (!mLoop && mBink->FrameNum == mBink->Frames) return false;
    return true;
}

void Movie::Impl::SetPaused(bool b) {
    if (mTimeCallback != NULL) {
        float dt = mTimeCallback();
        if (dt == 0.0f) {
            b = true;
        }
    }
    bool &_ref0 = mPaused;
    if (_ref0 == b) return;
    if (mBink == NULL) return;
    if (mMovieBuffers->mPendingBlits > 1 && sAsyncMovie != NULL) {
        if (!b) {
        sNextMovie = this;
        return;
    }
    }
    if (!b) LockThread();
    if (!b && mPreloadBuf != NULL && mTimeCallback != NULL) {
        MovieClose();
        MovieOpen(mPreloadBuf, 0x4000400);
        BINKFRAMEBUFFERS info;
        BinkGetFrameBuffersInfo(mBink, &info);
        BinkRegisterFrameBuffers(mBink, &mMovieBuffers->mBuffers);
        _ref0 = true;
        FinishOpen();
    }
    BinkPause(mBink, b);
    _ref0 = b;
    if (b) UnlockThread();
}
