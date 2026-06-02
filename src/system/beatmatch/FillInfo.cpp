#include "beatmatch/FillInfo.h"
#include <algorithm>

bool FillExtentCmp(const FillExtent &ext, int tick) { return ext.end - 1 < tick; }

bool FillExtentCmpIncludingEnd(const FillExtent &ext, int tick) { return ext.end < tick; }

void FillInfo::Clear() {
    mFills.clear();
    mLanes.mInfos.clear();
}

// fn_8045F964
bool FillInfo::AddFill(int start, int duration, bool bre) {
    int newStart = ((start + 15) / 30) * 30;
    int newEnd = ((start + duration + 15) / 30) * 30;
    int newDuration = newEnd - newStart;
    if (mFills.empty() || mFills.back().end <= newStart) {
        mFills.push_back(FillExtent(newStart, newStart + newDuration, bre));
        return true;
    } else
        return false;
}

bool FillInfo::FillAt(int tick, bool include_end) const {
    const FillExtent *ext =
#ifdef HX_NATIVE
        mFills.data() + (std::lower_bound(mFills.begin(), mFills.end(), tick, include_end ? FillExtentCmpIncludingEnd : FillExtentCmp) - mFills.begin());
#else
        std::lower_bound(mFills.begin(), mFills.end(), tick, include_end ? FillExtentCmpIncludingEnd : FillExtentCmp);
#endif
    if (ext == mFills.data() + mFills.size())
        return false;
    else
        return ext->CheckBounds(tick);
}

bool FillInfo::FillAt(int tick, FillExtent &outExtent, bool include_end) const {
    const FillExtent *e =
#ifdef HX_NATIVE
        mFills.data() + (std::lower_bound(mFills.begin(), mFills.end(), tick, include_end ? FillExtentCmpIncludingEnd : FillExtentCmp) - mFills.begin());
#else
        std::lower_bound(mFills.begin(), mFills.end(), tick, include_end ? FillExtentCmpIncludingEnd : FillExtentCmp);
#endif
    if (e == mFills.data() + mFills.size())
        return false;
    else if (!e->CheckBounds(tick))
        return false;
    else {
        outExtent.start = e->start;
        outExtent.end = e->end;
        return true;
    }
}

bool FillInfo::NextFillExtents(int tick, FillExtent &outExtent) const {
    for (std::vector<FillExtent>::const_iterator it = mFills.begin(); it != mFills.end();
         ++it) {
        if (tick <= it->start) {
            outExtent.start = it->start;
            outExtent.end = it->end;
            return true;
        }
    }
    return false;
}

bool FillInfo::FillExtentAtOrBefore(int tick, FillExtent &outExtent) const {
    std::vector<FillExtent>::const_iterator it;
    for (it = mFills.begin(); it != mFills.end() && it->start <= tick; ++it)
        ;
    if (it == mFills.begin())
        return false;
    else {
        --it;
        outExtent.start = it->start;
        outExtent.end = it->end;
        return true;
    }
}

// fn_8045CCBC
bool FillInfo::AddLanes(int tick, int lanes) { return mLanes.AddInfo(tick, lanes); }

int FillInfo::LanesAt(int tick) const {
    const TickedInfo<int> *info =
#ifdef HX_NATIVE
        mLanes.mInfos.data() + (std::upper_bound(mLanes.mInfos.begin(), mLanes.mInfos.end(), tick, TickedInfoCollection<int>::Cmp) - mLanes.mInfos.begin());
#else
        std::upper_bound(mLanes.mInfos.begin(), mLanes.mInfos.end(), tick, TickedInfoCollection<int>::Cmp);
#endif
    if (info != mLanes.mInfos.data())
        info--;
    return info->mInfo;
}