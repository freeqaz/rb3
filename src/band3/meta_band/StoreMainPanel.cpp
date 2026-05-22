#include "meta/StorePackedMetadata.h"
#include "StoreMainPanel.h"
#include "meta_band/AppLabel.h"
#include "obj/Data.h"
#include "obj/Task.h"
#include "os/Debug.h"
#include "os/System.h"
#include "rndobj/Anim.h"
#include "rndobj/Mat.h"
#include "rndobj/Tex.h"
#include "utl/MakeString.h"
#include "utl/Std.h"
#include "utl/Symbols.h"
#include "utl/Symbols2.h"

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

void StoreMainPanel::FinishLoad() {
    UIPanel::FinishLoad();
    mNoneTex = mDir->Find<RndTex>("cover_art_none.tex", true);
    for (int i = 0; i < 6; i++) {
        RndMat *mat = mDir->Find<RndMat>(MakeString("cover_art_%02i.mat", i + 1), true);
        mCoverArtMats[i] = mat;
        mat->SetDiffuseTex(mNoneTex);
        mat->mShaderVariation = (ShaderVariation)(mat->mShaderVariation | 2);
    }
    mLabel1 = mDir->Find<AppLabel>("text_line_1.lbl", true);
    mLabel2 = mDir->Find<AppLabel>("text_line_2.lbl", true);
    mLabel3 = mDir->Find<AppLabel>("text_line_3.lbl", true);
    mScrollAnim = mDir->Find<RndAnimatable>("album_scroll.anim", true);
    mScrollAnim->Animate(
        mScrollAnim->EndFrame(), mScrollAnim->EndFrame(), kTaskUISeconds, 0, 0
    );
    mCurrentEntry = -1;
    mLabel1->SetTextToken(gNullStr);
    mLabel2->SetTextToken(gNullStr);
    MILO_ASSERT(TypeDef(), 0x57);
    mDisplayRate = TypeDef()->FindArray(display_rate, true)->Float(1);
    mCrossfadeDuration = TypeDef()->FindArray(crossfade_duration, true)->Float(1);
    ParseConfigData();
}
