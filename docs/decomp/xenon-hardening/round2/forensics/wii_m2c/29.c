s32 ControllerTypeToTrackType__F14ControllerTypeb(ControllerType arg0, s32 arg1); /* extern */
? Fail__5DebugFPCc(Debug *this, s8 *arg0);          /* extern */
? GetBandUsersInSession__11BandUserMgrCFRQ211stlpmtx_std63vector<P8BandUser,Us,Q211stlpmtx_std24StlNodeAlloc<P8BandUser>>(BandUserMgr *this, stlpmtx_std::vector<BandUser *, short unsigned, stlpmtx_std::StlNodeAlloc<BandUser *>> *arg0); /* extern */
s32 GetControllerType__8BandUserCFv(BandUser *this); /* extern */
s32 GetPreferredScoreType__8BandUserCFv(BandUser *this); /* extern */
s32 GetTrackType__8BandUserCFv(BandUser *this);     /* extern */
s8 *MakeString<PCc,i,PCc>__FPCcPCciPCc_PCc(s8 *arg0, s8 *arg1, s32 arg2, s8 *arg3); /* extern */
s32 TrackTypeToScoreType__F9TrackTypebb(TrackType arg0, s32 arg1, s32 arg2); /* extern */
? _MemOrPoolFreeSTL__Fi8PoolTypePv(s32 arg0, PoolType arg1, void *arg2); /* extern */
extern BandUserMgr *TheBandUserMgr;
extern Debug TheDebug;
extern s8 *kAssertStr;
static u32 @72433[0xA] = {
    (u32) &.L_803010C8,
    (u32) &.L_803010B4,
    (u32) &.L_803010A0,
    (u32) &.L_803010F0,
    (u32) &.L_80301104,
    (u32) &.L_80301118,
    (u32) &.L_803010DC,
    (u32) &.L_80301140,
    (u32) &.L_80301154,
    (u32) &.L_8030112C,
};
static s8 @stringBase0[0x82C] = "MusicLibrary.cpp\0!TheMusicLibrary\0TheMusicLibrary\0music_library\0song_select_panel\0song_preview_delay\0trainer_from_main_menu\0TheSongSortMgr->GetSort(kSongSortByDiff)->IsReady()\0%i:\0,\0;\0Bad ControllerType %i in MusicLibrary::SetupTaskForTrainer!\0music_library/sort_and_filters\0ssn\0node\0remote_not_ready\0setlist_content_restricted_screen\0bss\0move_on_quickplay\0leader_party_shuffle_warning_screen\0no_valid_songs_screen\0full_setlist_screen\0songNode\0parental_control_panel\0parental_control_screen\0setlistNode\0leader_setlist_warning_screen\0data\0invalid_version_screen\0demos_allowed\0demo_mode_screen\0demo_online_screen\0demo_setlist_screen\0content_restricted_screen\0invalid_selection_screen\0ix >= 0 && ix < GetCurrentSort()->GetDataCount()\0Failed to find a sort for the symbol %s, refreshing current sort instead\n\0header.mat\0subheader.mat\0function.mat\0function_setlist.mat\0rockcentral.mat\0song_disc_dark.mat\0song_disc_light.mat\0song_dlc_dark.mat\0song_dlc_light.mat\0song_store_dark.mat\0song_store_light.mat\0song_ugc_dark.mat\0song_ugc_light.mat\0setlist_dark.mat\0setlist_light.mat\0p9_label\0famousby\0famousby_group\0group\0song_count\0subgroup\0!subheaderNode->mCover\0song\0difficulty\0percentage\0function\0setlist_name\0battle_instrument_rank\0bg\0ossn\0difficulty_bg\0stars\0stars_head\0stars_title\0review_title\0Bad ScoreType in MusicLibrary::DifficultySortPart!\0singleUser\0!(val && mTask.setlistMode == kSetlistForbidden)\0!(!val && mTask.setlistMode == kSetlistForced)\0band\0Bad ScoreType %i in AllSetlistSongsHaveScoreType!\00 && \"Net setlist contains unusual setlist type\"\0pSetlist->GetArtTex() == NULL && \"NetSaveSestlist has texture?  Tell Ian S.\"\0setlistNode->GetSetlistRecord()->IsLocal()\0lss\0!TheSessionMgr->IsLocal()\0!mySharedSongChanged || aSharedSongChanged\0!myRestrictedSongChanged || aRestrictedSongChanged\0Attempted to create a filtered random setlist but there weren't enough songs available!\0bSuccess\0std::find( vSongs.begin(), vSongs.end(), gNullStr ) == vSongs.end()\0vSongs.size() == numSongs\0performer\0recorded %i points and %i stars on %s for user %s\0cheat_display\0show\0%s(%d): %s unhandled msg: %s";

/* MusicLibrary::ActiveScoreType (void) const */
u32 ActiveScoreType__12MusicLibraryCFv(MusicLibrary *this) {
    u16 spE;
    u16 spC;
    BandUser **sp8;
    BandUser *var_r30;
    s32 temp_cr0_eq;
    s32 var_r31;
    s8 *temp_r4;
    s8 *temp_r4_2;
    s8 *temp_r4_3;

    sp8 = NULL;
    spC = 0;
    spE = 0;
    GetBandUsersInSession__11BandUserMgrCFRQ211stlpmtx_std63vector<P8BandUser,Us,Q211stlpmtx_std24StlNodeAlloc<P8BandUser>>(TheBandUserMgr, (stlpmtx_std::vector<BandUser *, short unsigned, stlpmtx_std::StlNodeAlloc<BandUser *>> *) &sp8);
    var_r30 = NULL;
    if (spC != 1) {
        var_r31 = 0xA;
    } else {
        var_r30 = *sp8;
        if (var_r30 == NULL) {
            temp_r4 = "MusicLibrary.cpp\0!TheMusicLibrary\0TheMusicLibrary\0music_library\0song_select_panel\0song_preview_delay\0trainer_from_main_menu\0TheSongSortMgr->GetSort(kSongSortByDiff)->IsReady()\0%i:\0,\0;\0Bad ControllerType %i in MusicLibrary::SetupTaskForTrainer!\0music_library/sort_and_filters\0ssn\0node\0remote_not_ready\0setlist_content_restricted_screen\0bss\0move_on_quickplay\0leader_party_shuffle_warning_screen\0no_valid_songs_screen\0full_setlist_screen\0songNode\0parental_control_panel\0parental_control_screen\0setlistNode\0leader_setlist_warning_screen\0data\0invalid_version_screen\0demos_allowed\0demo_mode_screen\0demo_online_screen\0demo_setlist_screen\0content_restricted_screen\0invalid_selection_screen\0ix >= 0 && ix < GetCurrentSort()->GetDataCount()\0Failed to find a sort for the symbol %s, refreshing current sort instead\n\0header.mat\0subheader.mat\0function.mat\0function_setlist.mat\0rockcentral.mat\0song_disc_dark.mat\0song_disc_light.mat\0song_dlc_dark.mat\0song_dlc_light.mat\0song_store_dark.mat\0song_store_light.mat\0song_ugc_dark.mat\0song_ugc_light.mat\0setlist_dark.mat\0setlist_light.mat\0p9_label\0famousby\0famousby_group\0group\0song_count\0subgroup\0!subheaderNode->mCover\0song\0difficulty\0percentage\0function\0setlist_name\0battle_instrument_rank\0bg\0ossn\0difficulty_bg\0stars\0stars_head\0stars_title\0review_title\0Bad ScoreType in MusicLibrary::DifficultySortPart!\0singleUser\0!(val && mTask.setlistMode == kSetlistForbidden)\0!(!val && mTask.setlistMode == kSetlistForced)\0band\0Bad ScoreType %i in AllSetlistSongsHaveScoreType!\00 && \"Net setlist contains unusual setlist type\"\0pSetlist->GetArtTex() == NULL && \"NetSaveSestlist has texture?  Tell Ian S.\"\0setlistNode->GetSetlistRecord()->IsLocal()\0lss\0!TheSessionMgr->IsLocal()\0!mySharedSongChanged || aSharedSongChanged\0!myRestrictedSongChanged || aRestrictedSongChanged\0Attempted to create a filtered random setlist but there weren't enough songs available!\0bSuccess\0std::find( vSongs.begin(), vSongs.end(), gNullStr ) == vSongs.end()\0vSongs.size() == numSongs\0performer\0recorded %i points and %i stars on %s for user %s\0cheat_display\0show\0%s(%d): %s unhandled msg: %s";
            Fail__5DebugFPCc(&TheDebug, MakeString<PCc,i,PCc>__FPCcPCciPCc_PCc(kAssertStr, temp_r4, 0x7D9, &temp_r4[0x53A]));
        }
        if (GetTrackType__8BandUserCFv(var_r30) != 0xA) {
            var_r31 = TrackTypeToScoreType__F9TrackTypebb((TrackType) GetTrackType__8BandUserCFv(var_r30), 0, 0);
        } else {
            var_r31 = TrackTypeToScoreType__F9TrackTypebb((TrackType) ControllerTypeToTrackType__F14ControllerTypeb((ControllerType) GetControllerType__8BandUserCFv(var_r30), 0), 0, 0);
        }
    }
    if (var_r31 == 0) {
        if (var_r30 == NULL) {
            temp_r4_2 = "MusicLibrary.cpp\0!TheMusicLibrary\0TheMusicLibrary\0music_library\0song_select_panel\0song_preview_delay\0trainer_from_main_menu\0TheSongSortMgr->GetSort(kSongSortByDiff)->IsReady()\0%i:\0,\0;\0Bad ControllerType %i in MusicLibrary::SetupTaskForTrainer!\0music_library/sort_and_filters\0ssn\0node\0remote_not_ready\0setlist_content_restricted_screen\0bss\0move_on_quickplay\0leader_party_shuffle_warning_screen\0no_valid_songs_screen\0full_setlist_screen\0songNode\0parental_control_panel\0parental_control_screen\0setlistNode\0leader_setlist_warning_screen\0data\0invalid_version_screen\0demos_allowed\0demo_mode_screen\0demo_online_screen\0demo_setlist_screen\0content_restricted_screen\0invalid_selection_screen\0ix >= 0 && ix < GetCurrentSort()->GetDataCount()\0Failed to find a sort for the symbol %s, refreshing current sort instead\n\0header.mat\0subheader.mat\0function.mat\0function_setlist.mat\0rockcentral.mat\0song_disc_dark.mat\0song_disc_light.mat\0song_dlc_dark.mat\0song_dlc_light.mat\0song_store_dark.mat\0song_store_light.mat\0song_ugc_dark.mat\0song_ugc_light.mat\0setlist_dark.mat\0setlist_light.mat\0p9_label\0famousby\0famousby_group\0group\0song_count\0subgroup\0!subheaderNode->mCover\0song\0difficulty\0percentage\0function\0setlist_name\0battle_instrument_rank\0bg\0ossn\0difficulty_bg\0stars\0stars_head\0stars_title\0review_title\0Bad ScoreType in MusicLibrary::DifficultySortPart!\0singleUser\0!(val && mTask.setlistMode == kSetlistForbidden)\0!(!val && mTask.setlistMode == kSetlistForced)\0band\0Bad ScoreType %i in AllSetlistSongsHaveScoreType!\00 && \"Net setlist contains unusual setlist type\"\0pSetlist->GetArtTex() == NULL && \"NetSaveSestlist has texture?  Tell Ian S.\"\0setlistNode->GetSetlistRecord()->IsLocal()\0lss\0!TheSessionMgr->IsLocal()\0!mySharedSongChanged || aSharedSongChanged\0!myRestrictedSongChanged || aRestrictedSongChanged\0Attempted to create a filtered random setlist but there weren't enough songs available!\0bSuccess\0std::find( vSongs.begin(), vSongs.end(), gNullStr ) == vSongs.end()\0vSongs.size() == numSongs\0performer\0recorded %i points and %i stars on %s for user %s\0cheat_display\0show\0%s(%d): %s unhandled msg: %s";
            Fail__5DebugFPCc(&TheDebug, MakeString<PCc,i,PCc>__FPCcPCciPCc_PCc(kAssertStr, temp_r4_2, 0x7EB, &temp_r4_2[0x53A]));
        }
        if (GetPreferredScoreType__8BandUserCFv(var_r30) == 6) {
            var_r31 = 6;
        }
    } else if (var_r31 == 3) {
        if (var_r30 == NULL) {
            temp_r4_3 = "MusicLibrary.cpp\0!TheMusicLibrary\0TheMusicLibrary\0music_library\0song_select_panel\0song_preview_delay\0trainer_from_main_menu\0TheSongSortMgr->GetSort(kSongSortByDiff)->IsReady()\0%i:\0,\0;\0Bad ControllerType %i in MusicLibrary::SetupTaskForTrainer!\0music_library/sort_and_filters\0ssn\0node\0remote_not_ready\0setlist_content_restricted_screen\0bss\0move_on_quickplay\0leader_party_shuffle_warning_screen\0no_valid_songs_screen\0full_setlist_screen\0songNode\0parental_control_panel\0parental_control_screen\0setlistNode\0leader_setlist_warning_screen\0data\0invalid_version_screen\0demos_allowed\0demo_mode_screen\0demo_online_screen\0demo_setlist_screen\0content_restricted_screen\0invalid_selection_screen\0ix >= 0 && ix < GetCurrentSort()->GetDataCount()\0Failed to find a sort for the symbol %s, refreshing current sort instead\n\0header.mat\0subheader.mat\0function.mat\0function_setlist.mat\0rockcentral.mat\0song_disc_dark.mat\0song_disc_light.mat\0song_dlc_dark.mat\0song_dlc_light.mat\0song_store_dark.mat\0song_store_light.mat\0song_ugc_dark.mat\0song_ugc_light.mat\0setlist_dark.mat\0setlist_light.mat\0p9_label\0famousby\0famousby_group\0group\0song_count\0subgroup\0!subheaderNode->mCover\0song\0difficulty\0percentage\0function\0setlist_name\0battle_instrument_rank\0bg\0ossn\0difficulty_bg\0stars\0stars_head\0stars_title\0review_title\0Bad ScoreType in MusicLibrary::DifficultySortPart!\0singleUser\0!(val && mTask.setlistMode == kSetlistForbidden)\0!(!val && mTask.setlistMode == kSetlistForced)\0band\0Bad ScoreType %i in AllSetlistSongsHaveScoreType!\00 && \"Net setlist contains unusual setlist type\"\0pSetlist->GetArtTex() == NULL && \"NetSaveSestlist has texture?  Tell Ian S.\"\0setlistNode->GetSetlistRecord()->IsLocal()\0lss\0!TheSessionMgr->IsLocal()\0!mySharedSongChanged || aSharedSongChanged\0!myRestrictedSongChanged || aRestrictedSongChanged\0Attempted to create a filtered random setlist but there weren't enough songs available!\0bSuccess\0std::find( vSongs.begin(), vSongs.end(), gNullStr ) == vSongs.end()\0vSongs.size() == numSongs\0performer\0recorded %i points and %i stars on %s for user %s\0cheat_display\0show\0%s(%d): %s unhandled msg: %s";
            Fail__5DebugFPCc(&TheDebug, MakeString<PCc,i,PCc>__FPCcPCciPCc_PCc(kAssertStr, temp_r4_3, 0x7F2, &temp_r4_3[0x53A]));
        }
        if (GetPreferredScoreType__8BandUserCFv(var_r30) == 4) {
            var_r31 = 4;
        }
    }
    if ((u32) var_r31 <= 9U) {
        return @72433[var_r31];
    }
    temp_cr0_eq = &sp8 == NULL;
    if ((temp_cr0_eq == 0) && (temp_cr0_eq == 0)) {
        if (sp8 != NULL) {
            _MemOrPoolFreeSTL__Fi8PoolTypePv(spE * 4, (PoolType) 1, sp8);
        }
        sp8 = NULL;
        spC = 0;
        spE = 0;
    }
    return (u32) var_r31;
}