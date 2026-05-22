#include "synth/VoiceBeat.h"
#include "math/Utl.h"
#include <algorithm>

VoiceBeat::VoiceBeat() {
    mEnabled = true;
    Reset();
}

void VoiceBeat::SetEnable(bool enable) {
    if (enable && !mEnabled)
        Reset();
    mEnabled = enable;
}

void VoiceBeat::Analyze(float *, int, bool, bool, float) {}

void VoiceBeat::Reset() {
    memset(mXVVoice, 0, sizeof(mXVVoice));
    memset(mYVVoice, 0, sizeof(mYVVoice));
    memset(mXVEnvAntiAlias, 0, sizeof(mXVEnvAntiAlias));
    memset(mYVEnvAntiAlias, 0, sizeof(mYVEnvAntiAlias));
    memset(mXVSyllables, 0, sizeof(mXVSyllables));
    memset(mYVSyllables, 0, sizeof(mYVSyllables));
    memset(mXVSpamSyllables, 0, sizeof(mXVSpamSyllables));
    memset(mYVSpamSyllables, 0, sizeof(mYVSpamSyllables));
    unk0 = false;
    unk1 = false;
    unk4 = 0;
    mSpamAvg = 0;
    mSylDeltaPrev = 0;
    mSylEnvSigma = 0;
    mFloorSigma = 0;
    mCount = 0;
    mRate = 1;
    mPeaks.clear();
    mTimes.clear();
    mTriggered = false;
}

void VoiceBeat::ClearTrigger() { mTriggered = false; }

void VoiceBeat::ClearEventList() {
    mPeaks.clear();
    mTimes.clear();
}

EventTracker::EventTracker() : mSelFrom(-1), mSelTo(-1), mAvgHitTime(0) {}

void EventTracker::invalidate() {
    mSelFrom = -1;
    mSelTo = -1;
}

int EventTracker::findEarliest(float t, int start) {
    int n = mTimes.size();
    if (n == 0) return -1;
    int last = n - 1;
    MaxEq(start, 0);
    if (start > last) start = last;
    while (start >= 0 && mTimes[start] >= t) {
        start--;
    }
    if (start < 0) return 0;
    while (start < n && mTimes[start] < t) {
        start++;
    }
    return start;
}

int EventTracker::findLatest(float t, int start) {
    int n = mTimes.size();
    if (n == 0) return -1;
    int idx = start;
    if (idx > n) idx = n - 1;
    if (idx < 0) idx = 0;
    while (idx < n && mTimes[idx] < t) {
        idx++;
    }
    if (idx >= n) return n - 1;
    while (idx >= 0 && mTimes[idx] >= t) {
        idx--;
    }
    return idx;
}

void EventTracker::Reset() {
    mMisses.clear();
    mMisses.resize(mTimes.size(), false);
    mHits.clear();
    mHits.resize(mTimes.size(), false);
    mSwings.clear();
    mSwings.resize(mTimes.size(), 0);
    mAvgHitTime = 0;
    invalidate();
}

bool EventTracker::Miss(float msFrom, float msUpTo) {
    mSelFrom = findEarliest(msFrom, mSelFrom);
    mSelTo = findLatest(msUpTo, mSelTo);
    bool result = false;
    for (int i = mSelFrom; i <= mSelTo; i++) {
        if (!mHits[i]) {
            mMisses[i] = true;
            result = true;
        }
    }
    return result;
}

TalkyMatcher::TalkyMatcher() { memset(mBuffer, 0, sizeof(mBuffer)); }

void TalkyMatcher::updateScoring(float f) {
    if (mVoiceBeat.mTriggered) {
        mRefEvents.Hit(f - 180.0f, f + 180.0f, f);
        mVoiceBeat.ClearEventList();
    }
    std::vector<double> unused;
    mRefEvents.Miss(f - 120.0f, f - 60.0f);
    mVoiceBeat.ClearTrigger();
}

void TalkyMatcher::LoadEvents(
    const std::vector<float> &times, const std::vector<float> &peaks
) {
    mRefEvents.mTimes = times;
    mRefEvents.mPeaks = peaks;
    mRefEvents.Reset();
}

void TalkyMatcher::Reset() { mVoiceBeat.Reset(); }

void TalkyMatcher::SetEnableTalkyMatcher(bool enable) { mVoiceBeat.SetEnable(enable); }