#include "synth/Pollable.h"

std::list<SynthPollable *> SynthPollable::sPollables;

SynthPollable::SynthPollable() : mItr(sPollables.end()), mIsActive(0) {}

SynthPollable::~SynthPollable() { CancelPolling(); }

void SynthPollable::StartPolling() {
    if (mItr == sPollables.end()) {
        sPollables.push_front(this);
        mItr = sPollables.begin();
        mIsActive = true;
    }
}

void SynthPollable::CancelPolling() {
    if (mIsActive) {
        std::list<SynthPollable *>::iterator end = sPollables.end();
        if (mItr != end) {
            sPollables.erase(mItr);
            mItr = end;
            mIsActive = false;
        }
    }
}

void SynthPollable::PollAll() {
    for (std::list<SynthPollable *>::const_iterator it = sPollables.begin();
         it != sPollables.end();
         it) {
        (*it++)->SynthPoll();
    }
}