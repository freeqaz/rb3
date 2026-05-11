#include "movie/Movie.h"
#include "movie/TexMovie.h"
#include "obj/Data.h"
#include "obj/DataFunc.h"
#include "obj/ObjMacros.h"
#include "os/CritSec.h"
#include "os/Debug.h"
#include "os/System.h"
#include "os/Timer.h"
#include "utl/MemMgr.h"
#include <list>
#include <vector>

extern "C" {
    void BinkSetMemory(void *(*)(unsigned int), void (*)(void *), void *(*)(unsigned int));
}

int gBinkCore0 = -1;
int gBinkCore1 = -1;

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

std::vector<Movie::Impl *> Movie::Impl::sActiveMovies;
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
        BinkSetMemory(RadAlloc, RadFree, RadAlloc);
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
void Movie::Validate() {}
