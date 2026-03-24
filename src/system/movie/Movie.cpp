#include "movie/Movie.h"

namespace MovieImpl {
    struct Impl;
    void CheckOpen(Impl *, bool);
    void End(Impl *);
    int GetFrame(const Impl *);
}

void Movie::CheckOpen(bool b) {
    MovieImpl::CheckOpen((MovieImpl::Impl *)mImpl, b);
}

void Movie::End() {
    MovieImpl::End((MovieImpl::Impl *)mImpl);
}

int Movie::GetFrame() const {
    return MovieImpl::GetFrame((MovieImpl::Impl *)mImpl);
}
