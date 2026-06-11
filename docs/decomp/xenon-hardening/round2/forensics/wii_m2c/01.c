typedef struct MusicLibrary {
    /* 0x00 */ char pad0[0xD0];
    /* 0xD0 */ s32 unkD0;                           /* inferred */
    /* 0xD4 */ char padD4[8];                       /* maybe part of unkD0[3]? */
    /* 0xDC */ s32 unkDC;                           /* inferred */
} MusicLibrary;                                     /* size >= 0xE0 */

typedef struct SongSortMgr {
    /* 0x00 */ char pad0[4];
    /* 0x04 */ ? unk4;                              /* inferred */
    /* 0x04 */ char pad4[8];
    /* 0x0C */ stlpmtx_std::_Rb_global<bool> *unkC; /* inferred */
} SongSortMgr;                                      /* size >= 0x10 */

typedef struct stlpmtx_std::_Rb_global<bool> {
    /* 0x00 */ char pad0[0x14];
    /* 0x14 */ SongRecord unk14;                    /* inferred */
    /* 0x14 */ char pad14[1];
} stlpmtx_std::_Rb_global<bool>;                    /* size >= 0x15 */

? Fail__5DebugFPCc(Debug *this, s8 *arg0);          /* extern */
? GetNode__8NodeSortCFi(NodeSort *this, s32 arg0);  /* extern */
NodeSort *GetSort__11SongSortMgrF12SongSortType(SongSortMgr *this, SongSortType arg0); /* extern */
s8 *MakeString<PCc,i,PCc>__FPCcPCciPCc_PCc(s8 *arg0, s8 *arg1, s32 arg2, s8 *arg3); /* extern */
s32 UpdateSharedStatus__10SongRecordFv(SongRecord *this); /* extern */
stlpmtx_std::_Rb_global<bool> *_M_increment__Q211stlpmtx_std13_Rb_global<b>FPQ211stlpmtx_std18_Rb_tree_node_base(stlpmtx_std::_Rb_global<bool> *this, stlpmtx_std::_Rb_tree_node_base *arg0); /* extern */
void *__dynamic_cast(?, struct RTTI *, struct RTTI *, ?); /* extern */
? PushHighlightToScreen__12MusicLibraryFb(MusicLibrary *this, s32 arg0); /* static */
? PushSonglistToScreen__12MusicLibraryFv(MusicLibrary *this); /* static */
extern Debug TheDebug;
extern SongSortMgr *TheSongSortMgr;
extern struct RTTI __RTTI__8SortNode;
extern s8 *kAssertStr;
static struct RTTI __RTTI__17OwnedSongSortNode;     /* unable to generate initializer: cannot parse @54219 as integer */
static s8 @stringBase0[0x82C] = "MusicLibrary.cpp\0!TheMusicLibrary\0TheMusicLibrary\0music_library\0song_select_panel\0song_preview_delay\0trainer_from_main_menu\0TheSongSortMgr->GetSort(kSongSortByDiff)->IsReady()\0%i:\0,\0;\0Bad ControllerType %i in MusicLibrary::SetupTaskForTrainer!\0music_library/sort_and_filters\0ssn\0node\0remote_not_ready\0setlist_content_restricted_screen\0bss\0move_on_quickplay\0leader_party_shuffle_warning_screen\0no_valid_songs_screen\0full_setlist_screen\0songNode\0parental_control_panel\0parental_control_screen\0setlistNode\0leader_setlist_warning_screen\0data\0invalid_version_screen\0demos_allowed\0demo_mode_screen\0demo_online_screen\0demo_setlist_screen\0content_restricted_screen\0invalid_selection_screen\0ix >= 0 && ix < GetCurrentSort()->GetDataCount()\0Failed to find a sort for the symbol %s, refreshing current sort instead\n\0header.mat\0subheader.mat\0function.mat\0function_setlist.mat\0rockcentral.mat\0song_disc_dark.mat\0song_disc_light.mat\0song_dlc_dark.mat\0song_dlc_light.mat\0song_store_dark.mat\0song_store_light.mat\0song_ugc_dark.mat\0song_ugc_light.mat\0setlist_dark.mat\0setlist_light.mat\0p9_label\0famousby\0famousby_group\0group\0song_count\0subgroup\0!subheaderNode->mCover\0song\0difficulty\0percentage\0function\0setlist_name\0battle_instrument_rank\0bg\0ossn\0difficulty_bg\0stars\0stars_head\0stars_title\0review_title\0Bad ScoreType in MusicLibrary::DifficultySortPart!\0singleUser\0!(val && mTask.setlistMode == kSetlistForbidden)\0!(!val && mTask.setlistMode == kSetlistForced)\0band\0Bad ScoreType %i in AllSetlistSongsHaveScoreType!\00 && \"Net setlist contains unusual setlist type\"\0pSetlist->GetArtTex() == NULL && \"NetSaveSestlist has texture?  Tell Ian S.\"\0setlistNode->GetSetlistRecord()->IsLocal()\0lss\0!TheSessionMgr->IsLocal()\0!mySharedSongChanged || aSharedSongChanged\0!myRestrictedSongChanged || aRestrictedSongChanged\0Attempted to create a filtered random setlist but there weren't enough songs available!\0bSuccess\0std::find( vSongs.begin(), vSongs.end(), gNullStr ) == vSongs.end()\0vSongs.size() == numSongs\0performer\0recorded %i points and %i stars on %s for user %s\0cheat_display\0show\0%s(%d): %s unhandled msg: %s";

/* MusicLibrary::RebuildSharedSongData (void) */
void RebuildSharedSongData__12MusicLibraryFv(MusicLibrary *this, ? arg_sp0) {
    ? *temp_r31;
    s32 temp_ret;
    s32 var_r0;
    s32 var_r27;
    s32 var_r31;
    s8 *temp_r4;
    stlpmtx_std::_Rb_global<bool> *var_r28;
    u32 var_r29;
    void *temp_r3;

    GetNode__8NodeSortCFi(GetSort__11SongSortMgrF12SongSortType(TheSongSortMgr, (SongSortType) this->unkDC), this->unkD0);
    temp_r3 = __dynamic_cast(0, &__RTTI__17OwnedSongSortNode, &__RTTI__8SortNode, 0);
    var_r29 = 0U;
    if ((temp_r3 != NULL) && ((s32) temp_r3->unk34->unk20 != 0)) {
        var_r29 = 1U;
    }
    var_r27 = 0;
    var_r28 = TheSongSortMgr->unkC;
    temp_r31 = &TheSongSortMgr->unk4;
loop_7:
    if (var_r28 != temp_r31) {
        temp_ret = UpdateSharedStatus__10SongRecordFv(&var_r28->unk14);
        if (temp_ret != 0) {
            var_r27 = 1;
        }
        var_r28 = _M_increment__Q211stlpmtx_std13_Rb_global<b>FPQ211stlpmtx_std18_Rb_tree_node_base(var_r28, (stlpmtx_std::_Rb_tree_node_base *) (u32) (u64) temp_ret);
        goto loop_7;
    }
    var_r31 = 0;
    if ((temp_r3 != NULL) && (var_r29 != (u8) temp_r3->unk34->unk20)) {
        var_r31 = 1;
    }
    var_r0 = 0;
    if ((var_r31 == 0) || (var_r27 != 0)) {
        var_r0 = 1;
    }
    if (var_r0 == 0) {
        temp_r4 = "MusicLibrary.cpp\0!TheMusicLibrary\0TheMusicLibrary\0music_library\0song_select_panel\0song_preview_delay\0trainer_from_main_menu\0TheSongSortMgr->GetSort(kSongSortByDiff)->IsReady()\0%i:\0,\0;\0Bad ControllerType %i in MusicLibrary::SetupTaskForTrainer!\0music_library/sort_and_filters\0ssn\0node\0remote_not_ready\0setlist_content_restricted_screen\0bss\0move_on_quickplay\0leader_party_shuffle_warning_screen\0no_valid_songs_screen\0full_setlist_screen\0songNode\0parental_control_panel\0parental_control_screen\0setlistNode\0leader_setlist_warning_screen\0data\0invalid_version_screen\0demos_allowed\0demo_mode_screen\0demo_online_screen\0demo_setlist_screen\0content_restricted_screen\0invalid_selection_screen\0ix >= 0 && ix < GetCurrentSort()->GetDataCount()\0Failed to find a sort for the symbol %s, refreshing current sort instead\n\0header.mat\0subheader.mat\0function.mat\0function_setlist.mat\0rockcentral.mat\0song_disc_dark.mat\0song_disc_light.mat\0song_dlc_dark.mat\0song_dlc_light.mat\0song_store_dark.mat\0song_store_light.mat\0song_ugc_dark.mat\0song_ugc_light.mat\0setlist_dark.mat\0setlist_light.mat\0p9_label\0famousby\0famousby_group\0group\0song_count\0subgroup\0!subheaderNode->mCover\0song\0difficulty\0percentage\0function\0setlist_name\0battle_instrument_rank\0bg\0ossn\0difficulty_bg\0stars\0stars_head\0stars_title\0review_title\0Bad ScoreType in MusicLibrary::DifficultySortPart!\0singleUser\0!(val && mTask.setlistMode == kSetlistForbidden)\0!(!val && mTask.setlistMode == kSetlistForced)\0band\0Bad ScoreType %i in AllSetlistSongsHaveScoreType!\00 && \"Net setlist contains unusual setlist type\"\0pSetlist->GetArtTex() == NULL && \"NetSaveSestlist has texture?  Tell Ian S.\"\0setlistNode->GetSetlistRecord()->IsLocal()\0lss\0!TheSessionMgr->IsLocal()\0!mySharedSongChanged || aSharedSongChanged\0!myRestrictedSongChanged || aRestrictedSongChanged\0Attempted to create a filtered random setlist but there weren't enough songs available!\0bSuccess\0std::find( vSongs.begin(), vSongs.end(), gNullStr ) == vSongs.end()\0vSongs.size() == numSongs\0performer\0recorded %i points and %i stars on %s for user %s\0cheat_display\0show\0%s(%d): %s unhandled msg: %s";
        Fail__5DebugFPCc(&TheDebug, MakeString<PCc,i,PCc>__FPCcPCciPCc_PCc(kAssertStr, temp_r4, 0xAFD, &temp_r4[0x6A3]));
    }
    if (var_r27 != 0) {
        PushSonglistToScreen__12MusicLibraryFv(this);
        if (var_r31 != 0) {
            PushHighlightToScreen__12MusicLibraryFb(this, 0);
        }
    }
}