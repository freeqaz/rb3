#include "meta/StorePackedMetadata.h"
#include "StoreMainPanel.h"
#include "obj/Data.h"
#include "os/Debug.h"
#include "os/System.h"
#include "utl/MakeString.h"
#include "utl/Std.h"

DECOMP_FORCEFUNC(StoreMainPanel, StoreMetadataManager, GetString(0))

StoreMainPanel::~StoreMainPanel() { ClearConfigData(); }

#pragma pool_data off
void StoreMainPanel::SetType(Symbol type) {
    static DataArray *types = SystemConfig(StaticClassName(), "types", "objects");
    if (type.Null()) {
        SetTypeDef(0);
    } else {
        DataArray *found = types->FindArray(type, false);
        if (found != 0) {
            SetTypeDef(found);
        } else {
            TheDebug.Notify(MakeString(
                "%s:%s couldn't find type %s", ClassName(), PathName(this), type
            ));
            SetTypeDef(0);
        }
    }
}
#pragma pool_data reset

void StoreMainPanel::ClearConfigData() {
    unk6c = false;
    DeleteAll(mCoverArtTexs);
    mCoverArtTexs.resize(0);
    ClearAndShrink(mNewReleaseList);
}

const StoreMainPanel::NewReleaseEntry *StoreMainPanel::CurrentEntry() const {
    MILO_ASSERT(mCurrentEntry < mNewReleaseList.size(), 0x134);
    return &mNewReleaseList[mCurrentEntry];
}

const char *StoreMainPanel::MarqueePath() const {
    if (mNewReleaseList.size() && mCurrentEntry >= 0) {
        return CurrentEntry()->mText4;
    }
    return gNullStr;
}
