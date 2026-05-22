#include "ui/UIListState.h"
#include "ui/UIListProvider.h"
#include "math/Utl.h"
#include <stdlib.h>

UIListState::UIListState(UIListProvider *provider, UIListStateCallback *callback)
    : mCircular(0), mNumDisplay(5), mGridSpan(1), mSpeed(0.25f), mMinDisplay(0),
      mScrollPastMinDisplay(0), mMaxDisplay(-1), mScrollPastMaxDisplay(1),
      mProvider(provider), mHiddenData(), mFirstShowing(0), mTargetShowing(0),
      mSelectedDisplay(0), mStepPercent(0.0f), mStepTime(-1.0f), mCallback(callback) {}

int UIListState::Selected() const { return Display2Showing(SelectedDisplay()); }

int UIListState::SelectedNoWrap() const {
    int i1 = mFirstShowing + SelectedDisplay();
    if (mScrollPastMinDisplay && !mCircular) {
        i1 -= mMinDisplay;
        if (i1 < 0 || i1 >= NumShowing())
            return -1;
    }
    return i1;
}

int UIListState::SelectedData() const { return Display2Data(SelectedDisplay()); }

int UIListState::SelectedDisplay() const {
    if (mCircular)
        return mMinDisplay;
    else
        return mSelectedDisplay;
}

#pragma push
#pragma force_active on
inline bool UIListState::IsScrolling() const { return mFirstShowing != mTargetShowing; }
inline int UIListState::MinDisplay() const { return mMinDisplay; }
#pragma pop

int UIListState::MaxDisplay() const { return mMaxDisplay; }
float UIListState::Speed() const { return mSpeed; }

#pragma push
#pragma force_active on
inline bool UIListState::ScrollPastMinDisplay() const { return mScrollPastMinDisplay; }
#pragma pop

bool UIListState::ScrollPastMaxDisplay() const { return mScrollPastMaxDisplay; }

bool UIListState::CanScrollBack(bool b) const {
    if (mCircular)
        return true;
    int count = b ? Display2Data(mSelectedDisplay) : Showing2Data(mFirstShowing);
    for (count = count - 1; count >= 0; count--) {
        if (mProvider->IsActive(count))
            return true;
    }
    return false;
}

bool UIListState::CanScrollNext(bool b) const {
    if (mCircular)
        return true;
    else if (b) {
        for (int data = Display2Data(mSelectedDisplay) + 1; data < mProvider->NumData();
             data++) {
            if (mProvider->IsActive(data))
                return true;
        }
    } else {
        return MaxFirstShowing() > mFirstShowing;
    }
    return false;
}

float UIListState::StepPercent() const { return mStepPercent; }

DECOMP_FORCEFUNC(UIListState, UIListState, Provider())

#pragma push
#pragma force_active on
inline UIListProvider *UIListState::Provider() { return mProvider; }
inline UIListProvider *UIListState::Provider() const { return mProvider; }
#pragma pop

int UIListState::WrapShowing(int i) const {
    if (NumShowing() == 0)
        return 0;
    else
        return Mod(i, NumShowing());
}

int UIListState::Display2Data(int i) const {
    int disp = Display2Showing(i);
    if (disp == -1)
        return -1;
    else
        return Showing2Data(disp);
}

int UIListState::Display2Showing(int i) const {
    int offset = mFirstShowing + i;
    if (mScrollPastMinDisplay && !mCircular) {
        offset = offset - mMinDisplay;
        if (offset < 0 || offset >= NumShowing())
            return -1;
    }
    return WrapShowing(offset);
}

int UIListState::Showing2Data(int i) const {
    int count = WrapShowing(i);
    for (std::vector<int>::const_iterator it = mHiddenData.begin();
         it != mHiddenData.end();
         it++) {
        if (*it <= count)
            count++;
    }
    return count;
}

int UIListState::NumDisplayWithData() const {
    int ret = NumDisplay();
    if (!mCircular) {
        int num = Provider()->NumData();
        if (ScrollPastMinDisplay())
            num += MinDisplay();
        ret = Min(ret, num);
    }
    return ret;
}

int UIListState::MaxFirstShowing() const {
    MILO_ASSERT(!mCircular, 0xE8);
    int curshowing = NumShowing();
    int maxshowing = Max(0, curshowing - mNumDisplay);
    if (mMaxDisplay != -1 && mScrollPastMaxDisplay) {
        maxshowing +=
            (Min(curshowing, mNumDisplay) - Clamp(0, mNumDisplay, mMaxDisplay)) - 1;
    }
    if (mScrollPastMinDisplay) {
        maxshowing += mMinDisplay;
    }
    return Max(0, maxshowing);
}

int UIListState::ScrollMaxDisplay() const {
    MILO_ASSERT(!mCircular, 0xF7);
    int max = Max(0, Min(NumShowing() - 1, mNumDisplay - 1));
    if (mMaxDisplay != -1) {
        max = Clamp(0, max, mMaxDisplay);
    }
    return max;
}

int UIListState::CurrentScroll() const { return ScrollToTarget(mTargetShowing); }

bool UIListState::ShouldHoldDisplayInPlace(int i2) const {
    bool shouldCheck = (mTargetShowing > mFirstShowing && i2 == 0)
        || (mTargetShowing < mFirstShowing && i2 == -1);
    if (!shouldCheck)
        return false;
    bool ret = false;
    bool b = false;
    if (SnappedDataForDisplay(i2) >= 0) {
        if (i2 + 1 != mNumDisplay && Display2Data(i2 + 1) != -1) {
            b = true;
        }
        if (b) {
            if (!Provider()->IsSnappableAtData(Display2Data(i2 + 1))) {
                ret = true;
            }
        }
    }
    return ret;
}

int UIListState::SnappedDataForDisplay(int i2) const {
    bool b1 = (!IsScrolling() && i2 == 0) || (mTargetShowing > mFirstShowing && i2 == 0)
        || (mTargetShowing < mFirstShowing && i2 == -1);
    if (b1) {
        int data = Display2Data(i2);
        return Provider()->SnappableAtOrBeforeData(data);
    } else
        return -1;
}

void UIListState::SetProvider(UIListProvider *provider, RndDir *rdir) {
    MILO_ASSERT(provider, 0x126);
    provider->InitData(rdir);
    mProvider = provider;
    mHiddenData.clear();
    for (int i = 0; i < mProvider->NumData(); i++) {
        if (mProvider->IsHidden(i)) {
            mHiddenData.push_back(i);
        }
    }
    SetSelected(0, -1, true);
}

void UIListState::SetNumDisplay(int num, bool b) {
    MILO_ASSERT(num > 0, 0x139);
    mNumDisplay = num;
    if (b) {
        SetSelected(0, -1, true);
    }
}

void UIListState::SetGridSpan(int span, bool b) {
    MILO_ASSERT(span > 0, 0x141);
    mGridSpan = span;
    if (b) {
        SetSelected(0, -1, true);
    }
}

void UIListState::SetMinDisplay(int min) {
    MILO_ASSERT(min >= 0, 0x149);
    mMinDisplay = min;
    mSelectedDisplay = Max(min, mSelectedDisplay);
}

void UIListState::SetMaxDisplay(int max) {
    MILO_ASSERT(max >= -1, 0x150);
    if (LOADMGR_EDITMODE) {
        ClampEq<int>(max, -1, mNumDisplay - 1);
    }
    mMaxDisplay = max;
}

void UIListState::SetCircular(bool c, bool b) {
    mCircular = c;
    if (b) {
        SetSelected(0, -1, true);
    }
}

void UIListState::SetSpeed(float speed) {
    MILO_ASSERT(speed >= 0, 0x15F);
    mSpeed = speed;
}

void UIListState::SetScrollPastMinDisplay(bool scroll) {
    mScrollPastMinDisplay = scroll;
    if (mScrollPastMinDisplay) {
        mSelectedDisplay = Max(mSelectedDisplay, mMinDisplay);
    }
}

void UIListState::SetScrollPastMaxDisplay(bool scroll) { mScrollPastMaxDisplay = scroll; }

void UIListState::SetSelected(int i, int j, bool b) {
    int data;
    int showing = WrapShowing(i);

    if (b) {
        data = showing;
        goto check_active;
    increment:
        data++;
        if (Showing2Data(data) == Showing2Data(showing)) goto done_loop;
    check_active:
        if (!mProvider->IsActive(Showing2Data(data))) goto increment;
    done_loop:
        showing = WrapShowing(data);
    }

    if (mCircular) {
        mFirstShowing = WrapShowing(showing - SelectedDisplay());
    } else {
        if (j != -1) {
            mFirstShowing = j;
        } else {
            int firstVal = mScrollPastMinDisplay ? showing : showing - mMinDisplay;
            mFirstShowing = Max(0, firstVal);
        }

        int maxFirst = MaxFirstShowing();
        int curFirst = mFirstShowing;
        if (maxFirst < curFirst) {
            curFirst = maxFirst;
        }

        int tempDiff = showing - curFirst;
        mFirstShowing = curFirst;
        mSelectedDisplay = tempDiff;

        if (mScrollPastMinDisplay) {
            mSelectedDisplay = tempDiff + mMinDisplay;
        }
    }

    mTargetShowing = mFirstShowing;
    mStepTime = -1.0f;
    mStepPercent = 0.0f;
}

int UIListState::ScrollToTarget(int target) const {
    int diff = target - mFirstShowing;

    if (mCircular) {
        int sign = diff > 0 ? 1 : -1;
        int adjusted;
        if (sign > 0) {
            adjusted = diff - NumShowing();
        } else {
            adjusted = NumShowing() + diff;
        }

        if (abs(adjusted) < abs(diff)) {
            return adjusted;
        }
        if (abs(adjusted) == abs(diff)) {
            return 1;
        }
    }

    return diff;
}

bool UIListState::BuildScroll(int direction, int firstShowing, int selectedDisplay, ScrollState &state) const {
    state.mFirstShowing = firstShowing;
    state.mSelectedDisplay = selectedDisplay;

    if (mFirstShowing != firstShowing) {
        int scrollSign = ScrollToTarget(firstShowing) > 0 ? 1 : -1;
        int dirSign = direction > 0 ? 1 : -1;
        if (dirSign != scrollSign)
            return false;
    }

    if (mCircular) {
        int newFirst = WrapShowing(state.mFirstShowing + direction);
        int curScroll = ScrollToTarget(state.mFirstShowing);
        if (curScroll != 0) {
            int newSign = ScrollToTarget(newFirst) > 0 ? 1 : -1;
            int curSign = curScroll > 0 ? 1 : -1;
            if (curSign != newSign)
                return false;
        }
        state.mFirstShowing = newFirst;
    } else {
        state.mSelectedDisplay += direction;
        int scrollMax = ScrollMaxDisplay();
        if (mScrollPastMinDisplay && mMinDisplay >= scrollMax) {
            scrollMax = mMinDisplay;
        }

        if (state.mSelectedDisplay < 0) {
            state.mFirstShowing += state.mSelectedDisplay;
            int sel = mMinDisplay;
            if (mMinDisplay >= firstShowing) {
                sel = firstShowing;
            }
            state.mSelectedDisplay = sel;
        } else if (state.mSelectedDisplay > scrollMax) {
            state.mFirstShowing += (state.mSelectedDisplay - scrollMax);
            state.mSelectedDisplay = scrollMax;
        } else if (mScrollPastMinDisplay && state.mSelectedDisplay < mMinDisplay) {
            state.mFirstShowing += (state.mSelectedDisplay - mMinDisplay);
            state.mSelectedDisplay = mMinDisplay;
        } else {
            int origFirst = state.mFirstShowing;
            if (state.mSelectedDisplay < mMinDisplay) {
                state.mFirstShowing = Max(0, origFirst - 1);
            }
            state.mSelectedDisplay = (state.mSelectedDisplay - state.mFirstShowing) + origFirst;
            return origFirst != state.mFirstShowing;
        }

        int result;
        if (state.mSelectedDisplay > scrollMax) {
            result = scrollMax;
        } else {
            if (state.mSelectedDisplay < 0) {
                result = 0;
            } else {
                result = state.mSelectedDisplay;
            }
        }
        state.mSelectedDisplay = result;

        int maxFirst = MaxFirstShowing();
        if (state.mFirstShowing <= maxFirst) {
            if (state.mFirstShowing < 0) {
                maxFirst = 0;
            } else {
                maxFirst = state.mFirstShowing;
            }
        }
        state.mFirstShowing = maxFirst;
    }

    return (state.mSelectedDisplay == selectedDisplay) || (state.mFirstShowing != firstShowing);
}

void UIListState::Poll(float fArg0) {
    if (mFirstShowing != mTargetShowing) {
        if (-1.0f == mStepTime) {
            mStepTime = fArg0;
            int s1 = ScrollToTarget(mTargetShowing);
            mCallback->StartScroll(*this, s1 > 0 ? 1 : -1, 1);
        }
        if (fArg0 >= mStepTime + mSpeed) {
            int dir = ScrollToTarget(mTargetShowing) > 0 ? 1 : -1;
            mFirstShowing = WrapShowing(dir + mFirstShowing);
            mCallback->CompleteScroll(*this);
            if (mFirstShowing != mTargetShowing) {
                mStepTime = fArg0 - (fArg0 - (mStepTime + mSpeed));
                int s2 = ScrollToTarget(mTargetShowing);
                mCallback->StartScroll(
                    *this, s2 > 0 ? 1 : -1, 1
                );
            } else {
                mStepTime = -1.0f;
            }
        }
        if (mFirstShowing != mTargetShowing) {
            float zero = 0.0f;
            if (mSpeed != zero) {
                float one = 1.0f;
                mStepPercent = (fArg0 - mStepTime) / mSpeed * one + zero;
                return;
            }
        }
        mStepPercent = 0.0f;
        if (mSpeed == 0.0f) {
            while (mFirstShowing != mTargetShowing) {
                Poll(fArg0);
            }
        }
    } else {
        mStepTime = -1.0f;
        mStepPercent = 0.0f;
    }
}

void UIListState::Scroll(int direction, bool skipActive) {
    if (mTargetShowing != mFirstShowing)
        return;

    ScrollState state;
    int hitBoundary;
    int changed = BuildScroll(direction, mTargetShowing, mSelectedDisplay, state);

    if (mCircular) {
        goto circ_accept;
    circ_loop:
        {
            int oldFirst = state.mFirstShowing;
            if (mTargetShowing == oldFirst)
                return;
            int step = direction > 0 ? 1 : -1;
            BuildScroll(step, oldFirst, state.mSelectedDisplay, state);
            if (state.mFirstShowing == oldFirst)
                return;
        }
    circ_accept:
        if (!skipActive && !mProvider->IsActive(State2Data(state)))
            goto circ_loop;
        mTargetShowing = state.mFirstShowing;
        MILO_ASSERT(state.mSelectedDisplay == mSelectedDisplay, 0x1d6);
    } else {
        hitBoundary = 0;
        goto scroll_accept;
    scroll_boundary:
        if (hitBoundary != 0)
            return;
        {
            int step = direction > 0 ? 1 : -1;
            changed = BuildScroll(step, state.mFirstShowing, state.mSelectedDisplay, state);
            if (step == 1) {
                hitBoundary = 0;
                if (state.mFirstShowing == MaxFirstShowing()) {
                    if (state.mSelectedDisplay == ScrollMaxDisplay()) {
                        hitBoundary = 1;
                    }
                }
            } else {
                bool firstIsZero = (state.mFirstShowing == 0);
                if (mScrollPastMinDisplay) {
                    hitBoundary = 0;
                    if (firstIsZero && state.mSelectedDisplay == mMinDisplay) {
                        hitBoundary = 1;
                    }
                } else {
                    hitBoundary = 0;
                    if (firstIsZero && state.mSelectedDisplay == 0) {
                        hitBoundary = 1;
                    }
                }
            }
        }
    scroll_accept:
        if (skipActive || mProvider->IsActive(State2Data(state))) {
            mTargetShowing = state.mFirstShowing;
            mSelectedDisplay = state.mSelectedDisplay;
            if (!skipActive && !changed) {
                int dir = -1;
                if (direction > 0) dir = 1;
                mCallback->StartScroll(*this, dir, false);
                mCallback->CompleteScroll(*this);
            }
        } else {
            goto scroll_boundary;
        }
    }
}

void UIListState::PageScroll(int amount) {
    int direction;
    if (amount > 0) {
        direction = 1;
    } else {
        direction = -1;
    }

    if (mCircular) {
        direction *= mNumDisplay;
    } else if (direction > 0) {
        int selectedDisplay = mSelectedDisplay;
        int numDisplay = mNumDisplay;
        if ((selectedDisplay == numDisplay - 1) || (selectedDisplay == mMaxDisplay)) {
            direction = numDisplay - mMinDisplay;
        } else {
            direction = (numDisplay - selectedDisplay - 1) - mMinDisplay;
        }
    } else if (direction < 0) {
        int minDisplay = mMinDisplay;
        int selectedDisplay = mSelectedDisplay;
        if (selectedDisplay == minDisplay) {
            direction = minDisplay - mNumDisplay;
        } else {
            int diff = minDisplay - selectedDisplay;
            direction = (diff >> 0x1F) & diff;
        }
    }

    Scroll(direction, false);
}

void UIListState::SetSelectedSimulateScroll(int i) {
    int showing = WrapShowing(i);
    mFirstShowing = mTargetShowing;
    mStepTime = -1.0f;
    mStepPercent = 0.0f;
    int diff = showing - SelectedNoWrap();
    if (diff != 0) {
        int numDisplay = mNumDisplay;
        if (abs(diff) > numDisplay * 2) {
            int dir = -1;
            if (diff > 0) dir = 1;
            SetSelected(showing - (dir * 2 * numDisplay), -1, true);
        }
        float negOne = -1.0f;
        float zero = 0.0f;
        goto check;
    loop:
        {
            int dir = -1;
            if ((showing - SelectedNoWrap()) > 0) dir = 1;
            Scroll(dir, true);
            mFirstShowing = mTargetShowing;
            mStepTime = negOne;
            mStepPercent = zero;
        }
    check:
        if (showing != SelectedNoWrap()) goto loop;
        MILO_ASSERT(showing == SelectedNoWrap(), 0x1BC);
        mCallback->CompleteScroll(*this);
    }
}

int UIListState::State2Data(const ScrollState &state) const {
    int sel;
    if (mCircular) {
        sel = SelectedDisplay();
    } else {
        sel = state.mSelectedDisplay;
    }
    if (mScrollPastMinDisplay) {
        sel -= mMinDisplay;
    }
    return Showing2Data(state.mFirstShowing + sel);
}