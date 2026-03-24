#pragma once
#include "utl/Str.h"

class Friend {
public:
    void SetName(String name) { mName = name; }
    void SetGame(String game) { mGame = game; }

    String mName; // 0x0
    bool mOnline; // 0xc
    String mGame; // 0x10
    int mFriendKey; // 0x1c
};