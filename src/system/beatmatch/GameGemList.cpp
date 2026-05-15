#include "beatmatch/GameGemList.h"
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

void GameGemList::Reset() {
    for (std::vector<GameGem>::iterator it = mGems.begin(); it != mGems.end(); it++) {
        it->mPlayed = false;
        it->unk10b1 = false;
    }
}