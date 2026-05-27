#include "AccomplishmentGroup.h"

#include "system/os/Debug.h"
#include "system/utl/Symbols2.h"
#include "system/utl/Symbols3.h"
#include "system/utl/Symbols4.h"

#include "decomp.h"

AccomplishmentGroup::AccomplishmentGroup(DataArray *i_pConfig, int index)
    : mName(""), mIndex(index), mInstrumentIcon(0x47), mScoreType((ScoreType)10),
      mAward("") {
    Configure(i_pConfig);
}

AccomplishmentGroup::~AccomplishmentGroup() {}

void AccomplishmentGroup::Configure(DataArray *i_pConfig) {
    MILO_ASSERT(i_pConfig, 0x1d);

    mName = i_pConfig->Sym(0);

    i_pConfig->FindData(award, mAward, false);
    String instrumentIcon;
#ifdef HX_NATIVE
    // Some instrument_icon glyphs are bare digits (e.g. `(instrument_icon 3)` for
    // harmony vox) which the DTA tokenizer reads as kDataInt, not kDataString.
    // FindData(...,String&) -> DataNode::Str MILO_FAILs on a non-string under the
    // native MILO_DEBUG build (retail is non-debug and coerces). Read the value
    // node directly and stringify whatever scalar type it is.
    {
        DataArray *iconArr = i_pConfig->FindArray(instrument_icon, true);
        const DataNode &iconNode = iconArr->Node(1);
        if (iconNode.Type() == kDataString || iconNode.Type() == kDataSymbol)
            instrumentIcon = iconNode.Str(nullptr);
        else if (iconNode.Type() == kDataInt)
            instrumentIcon = MakeString("%d", iconNode.Int(nullptr));
    }
#else
    i_pConfig->FindData(instrument_icon, instrumentIcon, true);
#endif
    int scoreType;
    i_pConfig->FindData(preferred_scoretype, scoreType, true);
    mScoreType = (ScoreType)scoreType;
    if (1 < instrumentIcon.length()) {
        TheDebug.Notify(MakeString(
            "Accomplishment Group has an instrument icon that is more than 1 character long! GROUP: %s\n",
            mName.Str()
        ));
        instrumentIcon = instrumentIcon.substr(0, 1);
    }
    if (instrumentIcon.length() == 0) {
        TheDebug.Notify(MakeString(
            "Accomplishment Group has an instrument icon that is 0 characters long! GROUP: %s\n",
            mName.Str()
        ));
    } else {
        mInstrumentIcon = instrumentIcon[0];
    }
}

int AccomplishmentGroup::GetIndex() const { return mIndex; }

Symbol AccomplishmentGroup::GetName() const { return mName; }

char AccomplishmentGroup::GetInstrumentIcon() { return mInstrumentIcon; }

Symbol AccomplishmentGroup::GetAward() const { return mAward; }

bool AccomplishmentGroup::HasAward() const { return !(mAward == ""); }

DECOMP_FORCEACTIVE(
    AccomplishmentGroup, "%s_desc", "ui/accomplishments/group_art/%s_keep.png", "%s_gray"
)
