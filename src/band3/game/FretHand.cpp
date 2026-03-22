#include "FretHand.h"

#include "beatmatch/GameGem.h"
#include "os/Debug.h"
#include "utl/MakeString.h"

FretHand::FretHand() { Reset(); }

FretHand::~FretHand() {}

void FretHand::SetFinger(uint finger, int fret, int lowstr, int highstr) {
    MILO_ASSERT(finger < kNumFingers, 24);
    FretFinger &thefinger = mFinger[finger];
    thefinger.mFret = fret;
    thefinger.mLowString = lowstr;
    thefinger.mHighString = highstr;
}

void FretHand::GetFinger(uint finger, int &fret, int &lowstr, int &highstr) const {
    MILO_ASSERT(finger < kNumFingers, 33);
    fret = mFinger[finger].mFret;
    lowstr = mFinger[finger].mLowString;
    highstr = mFinger[finger].mHighString;
}

void FretHand::Reset() {
    for (int i = 0; i < kNumFingers; i++)
        SetFinger(i, -1, -1, -1);
}

bool FretHand::BarAll(const GameGem &gem) {
    unsigned int last;
    unsigned int i;
    int first;
    first = -1;
    last = -1U;
    i = 0;
    do {
        if (gem.GetFret(i) > 0) {
            if (first == -1)
                first = (int)i;
            last = i;
        }
        i++;
    } while (i < 6);
    if (first == -1)
        return false;
    int bareFret = -1;
    while (first <= (int)last) {
        if (bareFret >= 0 && gem.GetFret((unsigned)first) < 0)
            return false;
        if (bareFret < 0 && gem.GetFret((unsigned)first) > 0) {
            bareFret = gem.GetFret((unsigned)first);
        } else if (gem.GetFret((unsigned)first) >= 0 && bareFret != gem.GetFret((unsigned)first)) {
            return false;
        }
        first++;
    }
    int first2 = -1;
    unsigned int last2 = -1U;
    unsigned int k = 0;
    do {
        if (gem.GetFret(k) > 0) {
            if (first2 == -1)
                first2 = (int)k;
            last2 = k;
        }
        k++;
    } while (k < 6);
    SetFinger(0, (int)bareFret, first2, (int)last2);
    return true;
}

void FretHand::SetFingers(const GameGem &gem) {
    Reset();
    if (!BarAll(gem)) {
        int maxPos;
        unsigned int finger;
        int lastStr;
        int numFingers;
        int advanced;
        int handpos;
        handpos = gem.GetHandPosition();
        if (handpos < 1)
            handpos = 1;
        finger = 0;
        numFingers = gem.GetNumFingers();
        maxPos = handpos + 5;
        advanced = 0;
        while (handpos < maxPos && finger < 4U && numFingers != 0) {
            lastStr = -1;
            int placed = 0;
            unsigned int str = 0;
            while (str < 6U && numFingers != 0) {
                if (handpos == gem.GetFret(str)) {
                    if ((unsigned)numFingers <= (unsigned)(4 - finger) && lastStr == -1) {
                        SetFinger(finger, handpos, (int)str, -1);
                        placed = 1;
                        advanced = 1;
                        finger++;
                        numFingers--;
                    } else if (lastStr == -1) {
                        SetFinger(finger, handpos, (int)str, -1);
                        lastStr = (int)str;
                        placed = 1;
                        advanced = 1;
                        numFingers--;
                    } else {
                        int canMerge = 1;
                        int mid = lastStr + 1;
                        while (mid < (int)str) {
                            if (gem.GetFret((unsigned)mid) != -1 && gem.GetFret((unsigned)mid) < handpos)
                                canMerge = 0;
                            mid++;
                        }
                        if (canMerge) {
                            SetFinger(finger, handpos, lastStr, (int)str);
                            placed = 1;
                            advanced = 1;
                            numFingers--;
                        } else {
                            finger++;
                            SetFinger(finger, handpos, (int)str, -1);
                            lastStr = (int)str;
                            placed = 1;
                            advanced = 1;
                            numFingers--;
                        }
                    }
                }
                str++;
            }
            if (lastStr != -1) {
                finger++;
            } else if (placed == 0 && advanced != 0 && (unsigned)numFingers < (unsigned)(4 - finger)) {
                finger++;
            }
            handpos++;
        }
        if (numFingers != 0) {
            MILO_WARN("Unable to build fret hand chord.");
        }
    }
}

int FretHand::GetFret(int x) const {
    int ret = 0;
    const FretFinger *f = mFinger;
    if (f->mLowString == x)
        return f->mFret;
    if (f->mLowString < x && x <= f->mHighString)
        ret = f->mFret;
    f++;
    if (f->mLowString == x)
        return f->mFret;
    if (f->mLowString < x && x <= f->mHighString)
        ret = f->mFret;
    f++;
    if (f->mLowString == x)
        return f->mFret;
    if (f->mLowString < x && x <= f->mHighString)
        ret = f->mFret;
    f++;
    if (f->mLowString == x)
        return f->mFret;
    if (f->mLowString < x && x <= f->mHighString)
        ret = f->mFret;
    return ret;
}
