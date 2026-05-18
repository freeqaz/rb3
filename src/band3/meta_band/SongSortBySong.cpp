#include "meta_band/SongSortBySong.h"
#include "meta/Sorting.h"
#include "obj/Data.h"
#include "os/Debug.h"
#include "os/System.h"
#include "utl/MemMgr.h"
#include "utl/Symbol.h"
#include "utl/Symbols.h"

int SongCmp::Compare(const SongSortCmp *s, SongNodeType nodeType) const {
    SongCmp *cmp = (SongCmp *)s;
    switch (nodeType) {
    case kNodeShortcut:
    case kNodeHeader:
        return mHeaderSym.mStr == cmp->mHeaderSym.mStr
            ? 0
            : strcmp(mHeaderSym.mStr, cmp->mHeaderSym.mStr);
    case kNodeSong:
    case kNodeStoreSong:
        return AlphaKeyStrCmp(mName, cmp->mName, true);
    default:
        MILO_FAIL("invalid type of node comparison.\n");
        return 0;
    }
}

void SongSortBySong::Init() {
    DataArray *cfg = SystemConfig(song_select);
    DataArray *alphas = cfg->FindArray(alpha_shortcuts);
    for (int i = 1; i < alphas->Size(); i++) {
        MemDoTempAllocations m(true, false);
        Symbol curSym = alphas->Sym(i);
        SongCmp *cmp = new SongCmp(gNullStr, curSym);
        mTree.push_back(new ShortcutNode(cmp, curSym, false));
    }
}