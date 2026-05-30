#include "meta_band/SongSetlistProvider.h"
#include "meta_band/AppLabel.h"
#include "meta_band/BandSongMetadata.h"
#include "meta_band/BandSongMgr.h"
#include "meta_band/MusicLibrary.h"
#include "obj/Dir.h"
#include "os/Debug.h"
#include "ui/UI.h"
#include "ui/UIListLabel.h"
#include "ui/UIScreen.h"
#include "utl/Locale.h"
#include "utl/MakeString.h"
#include "utl/Symbol.h"
#include "utl/Symbols.h"

void SetlistProvider::Text(int, int data, UIListLabel *slot, UILabel *label) const {
    if (slot->Matches("song")) {
        AppLabel *appLabel = dynamic_cast<AppLabel *>(label);
#ifdef HX_NATIVE
        // 360-ARK song_select.milo authors some setlist slots as plain UILabels;
        // the AppLabel cast yields null. Wii path asserts. Fall back to the
        // base UILabel API so setlist rows render with a song title instead of
        // being silently empty (W6 V1).
        if (!appLabel) {
            int song = TheMusicLibrary->SongAtSetlistIndex(data);
            if (song != 0) {
                BandSongMetadata *meta =
                    dynamic_cast<BandSongMetadata *>(TheSongMgr.Data(song));
                const char *title = meta ? meta->Title() : gNullStr;
                label->SetDisplayText(
                    MakeString("%d. %s", data + 1, title), true
                );
            } else if (TheMusicLibrary->SetlistSize() == data) {
                label->SetDisplayText(
                    MakeString("%d. %s", data + 1, Localize(choosing, nullptr)),
                    true
                );
            } else {
                label->SetDisplayText(MakeString("%d.", data + 1), true);
            }
            return;
        }
#else
        MILO_ASSERT(appLabel, 0x1D);
#endif
        int song = TheMusicLibrary->SongAtSetlistIndex(data);
        if (song == 0) {
            if (TheMusicLibrary->SetlistSize() == data) {
                appLabel->SetSongNameWithNumber(
                    song, data + 1, Localize(choosing, nullptr)
                );
            } else
                appLabel->SetSongNameWithNumber(song, data + 1, gNullStr);
        } else {
            appLabel->SetSongNameWithNumber(song, data + 1, nullptr);
        }
    } else {
        label->SetTextToken(gNullStr);
    }
}

int SetlistProvider::NumData() const {
    if (TheMusicLibrary->GetForcedSetlist()
        && TheMusicLibrary->GetMaxSetlistSize() != 0) {
        return TheMusicLibrary->GetMaxSetlistSize();
    } else {
        UIScreen *songSelectScreen =
            ObjectDir::Main()->Find<UIScreen>("song_select_screen", true);
        UIScreen *diffScreen =
            ObjectDir::Main()->Find<UIScreen>("part_difficulty_screen", true);
        if (TheUI.BottomScreen() == songSelectScreen
            && (TheUI.TransitionScreen() != diffScreen
                || TheUI.mTransitionState == kTransitionFrom)
            && !TheMusicLibrary->SetlistIsFull()) {
            return TheMusicLibrary->SetlistSize() + 1;
        } else
            return TheMusicLibrary->SetlistSize();
    }
}