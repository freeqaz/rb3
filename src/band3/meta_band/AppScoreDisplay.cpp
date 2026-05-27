#include "meta_band/AppScoreDisplay.h"
#include "decomp.h"
#include "meta_band/AppLabel.h"
#include "os/Debug.h"

void AppScoreDisplay::UpdateDisplay() {
    AppLabel *label = dynamic_cast<AppLabel *>(mCombinedLabel);
#ifdef HX_NATIVE
    // The combined-score AppLabel is absent from the 360-ARK extract's score
    // display milo (online-score UI). With no label there is nothing to update
    // (offline has no scores). Skip rather than assert. Reached during the
    // song_select song-list refresh.
    if (!label)
        return;
#else
    MILO_ASSERT(label, 0x10);
#endif
    label->SetFromScoreDisplayData(unk114, mScore, mRank, mGlobally);
}

DECOMP_FORCEFUNC(AppScoreDisplay, AppScoreDisplay, ClassName())
DECOMP_FORCEFUNC(AppScoreDisplay, AppScoreDisplay, SetType(Symbol()))
DECOMP_FORCEDTOR(AppScoreDisplay, AppScoreDisplay)