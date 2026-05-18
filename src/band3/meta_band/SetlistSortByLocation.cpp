#include "meta_band/SetlistSortByLocation.h"
#include "meta_band/ProfileMgr.h"
#include "meta_band/SavedSetlist.h"
#include "system/meta/Profile.h"
#include "os/Debug.h"
#include "utl/Str.h"

#include <string.h>

LocationCmp::LocationCmp(
    SavedSetlist::SetlistType type, const char *owner, const char *cmp, int id,
    const char *name
)
    : mCmp(cmp), mSetlistType(type), mOwner(owner), mId(id), mName(name) {
    switch (mSetlistType) {
    case SavedSetlist::kBattleHarmonix:
    case SavedSetlist::kBattleFriend:
    case SavedSetlist::kBattleHarmonixArchived:
    case SavedSetlist::kBattleFriendArchived:
        mField20 = 0;
        break;
    case SavedSetlist::kSetlistFriend:
    case SavedSetlist::kSetlistHarmonix:
    case SavedSetlist::kSetlistLocal:
        mField20 = 1;
        break;
    case SavedSetlist::kSetlistInternal:
        mField20 = 2;
        break;
    default:
        MILO_FAIL("Bad SetlistType in LocationCmp::LocationCmp!");
        break;
    }

    switch (mSetlistType) {
    case SavedSetlist::kBattleFriend:
    case SavedSetlist::kBattleFriendArchived: {
        MILO_ASSERT(owner, 0x55);
        Profile *profile = TheProfileMgr.GetPrimaryProfile();
        if (profile) {
            bool eq = strcmp(profile->GetName(), owner) == 0;
            if (eq) {
                mField24 = 0;
                break;
            }
        }
        mField24 = 2;
        break;
    }
    case SavedSetlist::kSetlistHarmonix:
    case SavedSetlist::kBattleHarmonix:
    case SavedSetlist::kBattleHarmonixArchived:
        mField24 = 1;
        break;
    case SavedSetlist::kSetlistFriend:
        mField24 = 2;
        break;
    case SavedSetlist::kSetlistLocal:
    case SavedSetlist::kSetlistInternal:
        mField24 = 0;
        break;
    default:
        MILO_FAIL("Bad SetlistType in LocationCmp::LocationCmp!");
        break;
    }
}
