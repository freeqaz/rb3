? Fail__5DebugFPCc(Debug *this, s8 *arg0);          /* extern */
s8 *Str__12FormatStringFv(FormatString *this);      /* extern */
void *__ct__12FormatStringFPCc(FormatString *this, s8 *arg0); /* extern */
u32 *__ct__6SymbolFPCc(Symbol *this, s8 *arg0);     /* extern */
u32 ActiveScoreType__12MusicLibraryCFv(MusicLibrary *this); /* static */
extern Debug TheDebug;
extern s8 *gNullStr;
static ? *@72368[0xB] = {
    &.L_80300E64,
    &.L_80300E58,
    &.L_80300E4C,
    &.L_80300E70,
    &.L_80300E70,
    &.L_80300E7C,
    &.L_80300E64,
    &.L_80300E88,
    &.L_80300E94,
    &.L_80300EA0,
    &.L_80300E40,
};
static ? @stringBase0;                              /* unable to generate initializer: unknown type */

/* MusicLibrary::DifficultySortPart (void) const */
u32 DifficultySortPart__12MusicLibraryCFv(MusicLibrary *this) {
    FormatString spC;
    Symbol sp8;
    u32 temp_r3;

    temp_r3 = ActiveScoreType__12MusicLibraryCFv(this);
    if (temp_r3 <= 0xAU) {
        return temp_r3;
    }
    __ct__12FormatStringFPCc(&spC, "MusicLibrary.cpp\0!TheMusicLibrary\0TheMusicLibrary\0music_library\0song_select_panel\0song_preview_delay\0trainer_from_main_menu\0TheSongSortMgr->GetSort(kSongSortByDiff)->IsReady()\0%i:\0,\0;\0Bad ControllerType %i in MusicLibrary::SetupTaskForTrainer!\0music_library/sort_and_filters\0ssn\0node\0remote_not_ready\0setlist_content_restricted_screen\0bss\0move_on_quickplay\0leader_party_shuffle_warning_screen\0no_valid_songs_screen\0full_setlist_screen\0songNode\0parental_control_panel\0parental_control_screen\0setlistNode\0leader_setlist_warning_screen\0data\0invalid_version_screen\0demos_allowed\0demo_mode_screen\0demo_online_screen\0demo_setlist_screen\0content_restricted_screen\0invalid_selection_screen\0ix >= 0 && ix < GetCurrentSort()->GetDataCount()\0Failed to find a sort for the symbol %s, refreshing current sort instead\n\0header.mat\0subheader.mat\0function.mat\0function_setlist.mat\0rockcentral.mat\0song_disc_dark.mat\0song_disc_light.mat\0song_dlc_dark.mat\0song_dlc_light.mat\0song_store_dark.mat\0song_store_light.mat\0song_ugc_dark.mat\0song_ugc_light.mat\0setlist_dark.mat\0setlist_light.mat\0p9_label\0famousby\0famousby_group\0group\0song_count\0subgroup\0!subheaderNode->mCover\0song\0difficulty\0percentage\0function\0setlist_name\0battle_instrument_rank\0bg\0ossn\0difficulty_bg\0stars\0stars_head\0stars_title\0review_title\0Bad ScoreType in MusicLibrary::DifficultySortPart!\0singleUser\0!(val && mTask.setlistMode == kSetlistForbidden)\0!(!val && mTask.setlistMode == kSetlistForced)\0band\0Bad ScoreType %i in AllSetlistSongsHaveScoreType!\00 && \"Net setlist contains unusual setlist type\"\0pSetlist->GetArtTex() == NULL && \"NetSaveSestlist has texture?  Tell Ian S.\"\0setlistNode->GetSetlistRecord()->IsLocal()\0lss\0!TheSessionMgr->IsLocal()\0!mySharedSongChanged || aSharedSongChanged\0!myRestrictedSongChanged || aRestrictedSongChanged\0Attempted to create a filtered random setlist but there weren't enough songs available!\0bSuccess\0std::find( vSongs.begin(), vSongs.end(), gNullStr ) == vSongs.end()\0vSongs.size() == numSongs\0performer\0recorded %i points and %i stars on %s for user %s\0cheat_display\0show\0%s(%d): %s unhandled msg: %s" + 0x507);
    Fail__5DebugFPCc(&TheDebug, Str__12FormatStringFv(&spC));
    return *__ct__6SymbolFPCc(&sp8, gNullStr);
}