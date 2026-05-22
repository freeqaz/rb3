#include "meta_band/CharCache.h"
#include "bandobj/BandCharDesc.h"
#include "bandobj/BandCharacter.h"
#include "bandobj/BandWardrobe.h"
#include "bandobj/PatchDir.h"
#include "game/BandUser.h"
#include "game/BandUserMgr.h"
#include "meta_band/BandProfile.h"
#include "meta_band/CharData.h"
#include "meta_band/CustomizePanel.h"
#include "meta_band/PatchPanel.h"
#include "meta_band/PrefabMgr.h"
#include "meta_band/ProfileMgr.h"
#include "obj/Dir.h"
#include "obj/ObjMacros.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "rndobj/Rnd.h"
#include "rndobj/Tex.h"
#include "ui/UIPanel.h"
#include "utl/FilePath.h"
#include "utl/Loader.h"
#include "utl/Messages2.h"
#include "utl/Symbols.h"
#include "utl/Symbols2.h"
#include "utl/Symbols3.h"

CharCache *TheCharCache;

void CharCache::Init() {
    TheCharCache = new CharCache();
    TheCharCache->InitMe();
}

CharCache::~CharCache() {}

CharCache::CharCache() : unk28(0) {}

void CharCache::InitMe() {
    FilePathTracker tracker("char");
    SetName("char_cache", ObjectDir::Main());
    unk1c.LoadFile(FilePath("../world/shared/chars.milo"), false, true, kLoadFront, false);
}

void CharCache::Request(
    int idx, const std::vector<BandCharDesc *> &descs, bool b1, bool b2
) {
    if (!unk28) {
        BandCharacter *bchar = GetCharacter(idx);
        bchar->CopyCharDesc(descs.front());
        bchar->StartLoad(true, b1, b2);
    }
}

void CharCache::RecomposePatches(int idx, BandCharDesc *desc, int i2) {
    GetCharacter(idx)->RecomposePatches(desc, i2);
}

void CharCache::RecomposeCharsWithPatchIx(int idx) {
    if (idx >= 0) {
        for (int i = 0; i < 4; i++) {
            BandCharacter *c = GetCharacter(i);
            MILO_ASSERT(c, 0x60);
            int recomp = 0;
            for (std::vector<BandCharDesc::Patch>::iterator it = c->mPatches.begin();
                 it != c->mPatches.end();
                 ++it) {
                if (it->mTexture == idx) {
                    recomp |= it->mCategory;
                }
            }
            if (recomp != 0) {
                c->RecomposePatches(c, recomp);
            }
        }
    }
}

BandCharacter *CharCache::GetCharacter(int slot) {
    MILO_ASSERT(slot >= 0 && slot < BandWardrobe::kNumTargets, 0x76);
    return unk1c->Find<BandCharacter>(MakeString("player%d", slot), true);
}

int CharCache::FindSlot(BandCharacter *bchar) {
    int i = 0;
    for (; i < 4; i++) {
        if (bchar == GetCharacter(i))
            break;
    }
    return i;
}

bool CharCache::CharactersAreLoading() {
    for (int i = 0; i < 4; i++) {
        if (GetCharacter(i)->IsLoading())
            return true;
    }
    return false;
}

BEGIN_HANDLERS(CharCache)
    HANDLE(get_patch_tex, OnGetPatchTex)
    HANDLE_ACTION(recompose_chars_with_patch_ix, RecomposeCharsWithPatchIx(_msg->Int(2)))
    HANDLE_EXPR(characters_are_loading, CharactersAreLoading())
    HANDLE_ACTION(lock, Lock(_msg->Int(2), _msg->Size() > 3 ? _msg->Int(3) : false))
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0xAF)
END_HANDLERS

DataNode CharCache::OnGetPatchTex(DataArray *arr) {
    // The character whose patch texture we are being asked to resolve.
    BandCharacter *bchar = arr->Obj<BandCharacter>(2);
    int patchIdx = arr->Int(3);
    String patchName(arr->Str(4));

    // Map the character back to its band slot so we know which user owns it.
    int slot = FindSlot(bchar);
    if (slot == BandWardrobe::kNumTargets) {
        MILO_WARN(
            "CharCache: %s has no valid slot and hence no patch", PathName(bchar)
        );
        return DataNode(TheRnd->GetNullTexture());
    }

    BandUser *user = TheBandUserMgr->GetUserFromSlot(slot);
    if (user && user->HasChar()) {
        // While the patch panel is up, the in-progress edit lives on that panel
        // rather than in the profile, so prefer the panel's preview texture.
        PatchPanel *patchPanel =
            ObjectDir::Main()->Find<PatchPanel>("patch_panel", true);
        CustomizePanel *customizePanel =
            ObjectDir::Main()->Find<CustomizePanel>("customize_panel", true);
        if (patchPanel->GetState() == UIPanel::kUp) {
            if (user->IsLocal()) {
                BandProfile *profile =
                    TheProfileMgr.GetProfileForUser(user->GetLocalUser());
                PatchDir *patchDir =
                    patchPanel->Property("editing_patch")->Obj<PatchDir>();
                MILO_ASSERT(patchDir, 0xCC);
                if ((profile && patchIdx == profile->GetPatchIndex(patchDir))
                    || patchName == customizePanel->mPatchName.c_str()) {
                    return DataNode(
                        patchPanel->mDir->Find<RndTex>("patch_preview.tex", true)
                    );
                }
            }
        }

        // While the patch select panel is up, ask it for the highlighted tex.
        UIPanel *selectPanel =
            ObjectDir::Main()->Find<UIPanel>("patch_select_panel", true);
        if (selectPanel->GetState() == UIPanel::kUp) {
            if (user->IsLocal()
                && patchName == customizePanel->mPatchName.c_str()) {
                return DataNode(
                    selectPanel->HandleType(highlighted_tex_msg).Obj<RndTex>()
                );
            }
        }

        // Otherwise resolve the patch from the user's character data.
        CharData *charData = user->GetChar();
        if (!bchar->mPrefab.Null()) {
            if (PrefabMgr::GetPrefabMgr()->PrefabIsCustomizable()
                && PrefabMgr::GetPrefabMgr()->PrefabUsesProfilePatches()
                && dynamic_cast<PrefabChar *>(charData)) {
                LocalUser *localUser = dynamic_cast<LocalUser *>(user);
                if (localUser) {
                    BandProfile *profile =
                        TheProfileMgr.GetProfileForUser(localUser);
                    MILO_ASSERT(profile, 0xEF);
                    return DataNode(profile->GetTexAtPatchIndex(patchIdx));
                }
            }
            return DataNode((Hmx::Object *)nullptr);
        }
        return DataNode(charData->GetTexAtPatchIndex(patchIdx, true));
    }

    // No active user owns this character: it is a stand-in.
    if (bchar->mPrefab.Null()) {
        BandProfile *profile =
            TheProfileMgr.GetProfileForChar((BandCharDesc *)bchar);
        if (profile) {
            return DataNode(profile->GetTexAtPatchIndex(patchIdx));
        }
        MILO_WARN("Cannot find Tex for Stand-in");
    }
    return DataNode((Hmx::Object *)nullptr);
}