#pragma once

#include "meta/StoreArtLoaderPanel.h"
#include "obj/Data.h"
#include "utl/Str.h"

class StoreMainPanel : public StoreArtLoaderPanel {
public:
    class NewReleaseEntry {
    public:
        String mStrName; // 0x0
        String mText1; // 0xC
        String mText2; // 0x18
        String mText3; // 0x24
        String mText4; // 0x30
        // sizeof == 0x3C
    };

    StoreMainPanel();
    ~StoreMainPanel();

    void ClearConfigData();

    // 0x40
    int unk40;
    float mTimeNextEvent; // 0x44
    int unk48;
    float unk4c;
    float unk50;
    int unk54;
    int unk58;
    int unk5c;
    int unk60;
    int unk64;
    int unk68;
    bool unk6c; // 0x6C
    DataArray *mPendingConfigData; // 0x70
    std::vector<class RndMat *> unk74; // 0x74
    int unk7c;
    int unk80;
    int unk84;
    int unk88;
    std::vector<NewReleaseEntry> mNewReleases; // 0x8C
};
