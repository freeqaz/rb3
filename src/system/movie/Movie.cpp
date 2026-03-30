#include "movie/Movie.h"
#include "os/Debug.h"
#include "os/Timer.h"

class Movie::Impl {
public:
    Impl();
    ~Impl();
    static void Terminate();
    bool Poll();
    void Draw();
};

Movie::Movie() {
    mImpl = new Movie::Impl();
    MILO_ASSERT(mImpl, 0x647);
}

Movie::~Movie() {
    delete mImpl;
}

void Movie::Terminate() {
    Movie::Impl::Terminate();
}

bool Movie::Poll() {
    START_AUTO_TIMER("movie");
    return mImpl->Poll();
}

void Movie::Draw() {
    START_AUTO_TIMER("movie");
    mImpl->Draw();
}

void Movie::End() {}
int Movie::GetFrame() const { return 0; }
void Movie::CheckOpen(bool) {}
