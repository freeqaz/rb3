#include "meta_band/AppScoreDisplay.h"
#include "decomp.h"
#include "meta_band/AppLabel.h"
#include "os/Debug.h"
#include "ui/UILabel.h"

void AppScoreDisplay::UpdateDisplay() {
    AppLabel *label = dynamic_cast<AppLabel *>(mCombinedLabel);
#ifdef HX_NATIVE
    // The 360-ARK score display milo authors mCombinedLabel as a plain UILabel
    // rather than the AppLabel the RB3-Wii code expects. The Wii path asserts.
    // Fall back to writing the score directly via the base UILabel API so the
    // small score tile on song_select renders a number instead of being empty
    // (W6 V1).
    if (!label) {
        UILabel *plain = dynamic_cast<UILabel *>(mCombinedLabel);
        if (plain) {
            plain->SetInt(mScore, true);
        }
        return;
    }
#else
    MILO_ASSERT(label, 0x10);
#endif
    label->SetFromScoreDisplayData(unk114, mScore, mRank, mGlobally);
}

DECOMP_FORCEFUNC(AppScoreDisplay, AppScoreDisplay, ClassName())
DECOMP_FORCEFUNC(AppScoreDisplay, AppScoreDisplay, SetType(Symbol()))
DECOMP_FORCEDTOR(AppScoreDisplay, AppScoreDisplay)