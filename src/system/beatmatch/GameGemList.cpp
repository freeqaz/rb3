#include "beatmatch/GameGemList.h"
#include "beatmatch/BeatMatchUtl.h"
#include "utl/MemMgr.h"
#include "utl/Std.h"
#include <algorithm>

namespace stlpmtx_std {

template <>
inline less<GameGem> __less<GameGem>(GameGem*) { return less<GameGem>(); }

} // namespace stlpmtx_std

bool GameGemTickCmp(const GameGem &gem, int tick);

GameGemList::GameGemList(int thresh) : mHopoThreshold(thresh) {}

void GameGemList::Clear() { mGems.clear(); }

void GameGemList::CopyFrom(const GameGemList *gList) {
    mGems.clear();
    mGems.reserve(gList->mGems.size());
    mGems.insert(mGems.begin(), gList->mGems.begin(), gList->mGems.end());
}

bool GameGemList::AddMultiGem(const MultiGemInfo &info) {
    return AddGameGem(GameGem(info), info.no_strum);
}

bool GameGemList::AddRGGem(const RGGemInfo &info) {
    return AddGameGem(GameGem(info), info.no_strum);
}

int GameGemList::ClosestMarkerIdx(float f) const {
    float theFloat = f;
    const GameGem *theGem = std::lower_bound(mGems.begin(), mGems.end(), theFloat, GameGemCmp);
    if (theGem == mGems.begin())
        return 0;
    if (theGem == mGems.end())
        return mGems.size() - 1;
    MILO_ASSERT(theFloat <= theGem->mMs, 0x83);
    MILO_ASSERT(theFloat >= (theGem - 1)->mMs, 0x84);
    if (fabsf(theFloat - (theGem - 1)->mMs) < fabsf(theFloat - theGem->mMs))
        theGem--;
    return theGem - mGems.begin();
}

int GameGemList::ClosestMarkerIdxAtOrAfter(float f) const {
    const GameGem *theGem = std::lower_bound(mGems.begin(), mGems.end(), f, GameGemCmp);
    if (theGem == mGems.begin())
        return 0;
    if (theGem == mGems.end())
        return -1;
    return theGem - mGems.begin();
}

int GameGemList::ClosestMarkerIdxAtOrAfterTick(int tick) const {
    const GameGem *theGem =
        std::lower_bound(mGems.begin(), mGems.end(), tick, GameGemTickCmp);
    if (theGem == mGems.begin())
        return 0;
    if (theGem == mGems.end())
        return -1;
    return theGem - mGems.begin();
}

bool GameGemCmp(const GameGem &gem, float ms) { return gem.mMs < ms; }

bool GameGemTickCmp(const GameGem &gem, int tick) { return gem.mTick < tick; }

float GameGemList::TimeAt(int idx) const {
    MILO_ASSERT(idx < mGems.size(), 0xA5);
    return mGems[idx].mMs;
}

void GameGemList::RecalculateGemTimes(TempoMap *tmap) {
    for (std::vector<GameGem>::iterator it = mGems.begin(); it != mGems.end(); it++) {
        it->RecalculateTimes(tmap);
    }
    std::sort(mGems.begin(), mGems.end());
}

void GameGemList::MergeChordGems() {
    if (mGems.empty())
        return;
    std::vector<GameGem> merged;
    std::vector<GameGem>::iterator it = mGems.begin();
    while (it != mGems.end()) {
        int tick = it->mTick;
        int durTicks = it->mDurationTicks;
        GameGem chord = *it;
        bool keep = true;
        std::vector<GameGem>::iterator next = it + 1;
        while (next != mGems.end() && abs(next->mTick - tick) < 10) {
            if (abs(next->mDurationTicks - durTicks) < 10) {
                chord.mSlots |= next->mSlots;
            } else {
                keep = false;
            }
            next++;
        }
        if (keep) {
            merged.push_back(chord);
            it = next;
        } else {
            for (; it != next; it++) {
                merged.push_back(*it);
            }
        }
    }
    mGems = merged;
}

bool GameGemList::AddGameGem(const GameGem &gem, NoStrumState noStrum) {
    MemDoTempAllocations tmp(true, false);
    if (!mGems.empty()) {
        const GameGem &last = mGems.back();
        if (last.mMs > gem.mMs) {
            mGems.insert(
                std::lower_bound(mGems.begin(), mGems.end(), gem, GameGem::CompareTimes),
                gem);
            return true;
        }
        if (last.mTick != gem.mTick && last.mTick + 10 >= gem.mTick) {
            return false;
        }
    }
    if (noStrum == kStrumDefault) {
        bool willBeNoStrum = WillBeNoStrum(gem);
        mGems.push_back(gem);
        mGems.back().mForceStrum = willBeNoStrum;
    } else {
        mGems.push_back(gem);
    }
    return true;
}

void GameGemList::Finalize() {
    std::vector<GameGem>(mGems).swap(mGems);
}

bool GameGemList::WillBeNoStrum(const GameGem &gem) {
    if (gem.IsRealGuitar() && gem.RightHandTap())
        return true;
    if (mGems.empty() || gem.mTick - mGems.back().mTick > mHopoThreshold)
        return false;
    if (gem.IsRealGuitar()) {
        const GameGem &last = mGems.back();
        if (last.IsMuted())
            return false;
        if (gem.GetNumStrings() == 1 && last.GetNumStrings() == 1) {
            int str = gem.GetLowestString();
            if (str == (int)last.GetLowestString()) {
                return gem.GetFret(str) != last.GetFret(str);
            }
        }
        return false;
    }
    return !(gem.mSlots & mGems.back().mSlots) && GemNumSlots(gem.mSlots) == 1;
}

void GameGemList::Reset() {
    for (std::vector<GameGem>::iterator it = mGems.begin(); it != mGems.end(); it++) {
        it->mPlayed = false;
        it->unk10b1 = false;
    }
}