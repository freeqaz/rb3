#include "meta_band/ViewSetting.h"

#include "bandobj/BandLabel.h"
#include "game/BandUser.h"
#include "game/Defines.h"
#include "game/Scoring.h"
#include "meta_band/AppLabel.h"
#include "meta_band/BandProfile.h"
#include "meta_band/BandSongMgr.h"
#include "meta_band/MusicLibrary.h"
#include "meta_band/ProfileMgr.h"
#include "net_band/RockCentral.h"
#include "meta_band/SongSortMgr.h"
#include "obj/DataFunc.h"
#include "obj/Dir.h"
#include "obj/ObjMacros.h"
#include "os/Debug.h"
#include "os/PlatformMgr.h"
#include "rndobj/Dir.h"
#include "rndobj/Mat.h"
#include "ui/UIColor.h"
#include "ui/UIList.h"
#include "ui/UIListCustom.h"
#include "ui/UIListLabel.h"
#include "ui/UIListMesh.h"
#include "ui/UIListSlot.h"
#include "ui/UIListWidget.h"
#include "utl/MakeString.h"
#include "utl/Symbols.h"
#include "utl/Symbols2.h"
#include "utl/Symbols3.h"
#include "utl/Symbols4.h"

// ------------------------------------------------------------------
// ViewSetting (base)
// ------------------------------------------------------------------

void ViewSetting::Text(int, int, UIListLabel *, UILabel *label) const {
    label->SetTextToken(gNullStr);
}

RndMat *ViewSetting::Mat(int, int row, UIListMesh *) const {
    if (row % 2 != 0) {
        return mOddMat;
    }
    return mEvenMat;
}

bool ViewSetting::IsActive(int) const { return true; }

void ViewSetting::InitData(RndDir *) {}

bool ViewSetting::CanSelectMultiple() const { return false; }

void ViewSetting::Reset() { SelectOption(0); }

void ViewSetting::Refresh() {}

bool ViewSetting::IsValid() const { return true; }

bool ViewSetting::IsHeader() const { return false; }

int ViewSetting::StartingOption() const { return 0; }

// ------------------------------------------------------------------
// HeaderViewSetting
// ------------------------------------------------------------------

void HeaderViewSetting::SelectOption(int) {
    MILO_ASSERT(false, 0x91);
}

// ------------------------------------------------------------------
// SortViewSetting
// ------------------------------------------------------------------

bool SortViewSetting::IsActive(int idx) const {
    if (idx == 4) {
        bool signedIn = false;
        if (TheProfileMgr.HasPrimaryProfile()) {
            BandProfile *prof = TheProfileMgr.GetPrimaryProfile();
            LocalBandUser *user = prof->GetAssociatedLocalBandUser();
            if (ThePlatformMgr.IsUserSignedIntoLive(user)) {
                signedIn = true;
            }
        }
        bool ok = false;
        if (signedIn && TheRockCentral.IsOnline()) {
            ok = true;
        }
        return ok;
    }
    return true;
}

void SortViewSetting::Text(int, int row, UIListLabel *slot, UILabel *label) const {
    if (slot->Matches("cd")) {
        Symbol s = TheSongSortMgr->GetSort((SongSortType)row)->GetName();
        label->SetTextToken(s);
    } else {
        label->SetTextToken(gNullStr);
    }
}

const char *SortViewSetting::GetCurrentStatus() const {
    return Localize(TheMusicLibrary->GetCurrentSortName(true), nullptr);
}

void SortViewSetting::SelectOption(int idx) {
    TheMusicLibrary->SetSort((SongSortType)idx);
}

int SortViewSetting::StartingOption() const {
    return TheMusicLibrary->GetCurrentSortType(true);
}

// ------------------------------------------------------------------
// ScoreTypeViewSetting
// ------------------------------------------------------------------

void ScoreTypeViewSetting::Text(int, int idx, UIListLabel *slot, UILabel *label)
    const {
    if (slot->Matches("cd")) {
        Symbol sym;
        if (idx == 0) {
            sym = ScoreTypeToSym(GetBaseScoreType());
        } else {
            sym = ScoreTypeToSym(GetAlternateScoreType());
        }
        label->SetTextToken(sym);
    } else {
        label->SetTextToken(gNullStr);
    }
}

const char *ScoreTypeViewSetting::GetCurrentStatus() const {
    return Localize(ScoreTypeToSym((ScoreType)mScoreType), nullptr);
}

void ScoreTypeViewSetting::Refresh() {
    mScoreType = TheMusicLibrary->ActiveScoreType();
    MILO_ASSERT(mScoreType != kNumScoreTypes, 0x104);
}

bool ScoreTypeViewSetting::IsValid() const {
    return mScoreType != kNumScoreTypes;
}

void ScoreTypeViewSetting::SelectOption(int idx) {
    ScoreType st;
    if (idx == 0) {
        st = GetBaseScoreType();
    } else {
        st = GetAlternateScoreType();
    }
    mScoreType = st;
    TheMusicLibrary->SetTaskScoreType(st);
}

int ScoreTypeViewSetting::StartingOption() const {
    ScoreType base = GetBaseScoreType();
    return mScoreType != base;
}

// ------------------------------------------------------------------
// FilterViewSetting
// ------------------------------------------------------------------

int FilterViewSetting::NumData() const { return mFilters.size(); }

bool FilterViewSetting::CanSelectMultiple() const { return true; }

const char *FilterViewSetting::GetCurrentStatus() const {
    String result;
    SongSortMgr::SongFilter &filter = TheMusicLibrary->GetFilter();
    const std::set<Symbol> &filterSet = filter.GetFilterSet(mFilterType);
    for (std::set<Symbol>::const_iterator it = filterSet.begin();
         it != filterSet.end();
         ++it) {
        if (result.c_str()[0]) {
            result += ", ";
        }
        result += Localize(*it, nullptr);
    }
    if (!result.c_str()[0]) {
        return Localize(filter_none, nullptr);
    }
    return MakeString("%s", result.c_str());
}

void FilterViewSetting::Reset() {
    TheMusicLibrary->ResetFilter(mFilterType);
}

bool FilterViewSetting::IsValid() const {
    return !TheMusicLibrary->GetFilterLocked();
}

void FilterViewSetting::SelectOption(int idx) {
    Filter &f = mFilters[idx];
    TheMusicLibrary->ToggleFilter(mFilterType, f.mSym);
}

bool FilterViewSetting::CompareFilters(const Filter &a, const Filter &b) {
    return a.mCount > b.mCount;
}

Symbol FilterViewSetting::FilterTypeToSym(FilterType ft) {
    switch (ft) {
    case kFilterGenre: return filter_setting_genres;
    case kFilterDecade: return filter_setting_decades;
    case kFilterDifficulty: return filter_setting_difficulties;
    case kFilterLength: return filter_setting_lengths;
    case kFilterRating: return filter_setting_ratings;
    case kFilterSource: return filter_setting_sources;
    case kFilterVocalParts: return filter_setting_vocal_parts;
    case 7: return filter_setting_pro_guitar;
    case 8: return filter_setting_keys;
    default:
        MILO_FAIL(MakeString("no symbol for FilterType %i", ft));
        return gNullStr;
    }
}

// ------------------------------------------------------------------
// ViewSettingsProvider
// ------------------------------------------------------------------

int ViewSettingsProvider::NumData() const { return mSettings.size(); }

void ViewSettingsProvider::ResetActiveSetting() {
    mActiveSetting->Reset();
}

void ViewSettingsProvider::RefreshAllSettings() {
    for (std::vector<ViewSetting *>::iterator it = mSettings.begin();
         it != mSettings.end();
         ++it) {
        (*it)->Refresh();
    }
}

void ViewSettingsProvider::ResetAllSettings() {
    for (std::vector<ViewSetting *>::iterator it = mSettings.begin();
         it != mSettings.end();
         ++it) {
        (*it)->Reset();
    }
}

int ViewSettingsProvider::SelectSetting(int idx) {
    ViewSetting *setting = mSettings[idx];
    if (setting->IsValid()) {
        mActiveSetting = mSettings[idx];
        return 1;
    }
    return 0;
}

bool ViewSettingsProvider::IsActive(int idx) const {
    return mSettings[idx]->IsValid();
}

RndMat *ViewSettingsProvider::Mat(int, int row, UIListMesh *) const {
    ViewSetting *setting = mSettings[row];
    if (setting->IsHeader()) {
        return mHeaderMat;
    }
    if (row % 2 != 0) {
        return mOddMat;
    }
    return mEvenMat;
}
