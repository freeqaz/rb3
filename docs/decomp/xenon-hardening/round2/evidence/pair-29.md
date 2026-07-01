# Pair 29 — verification evidence

**Claimed identity:** Wii `ActiveScoreType__12MusicLibraryCFv`  ==  Xenon `0x8233afb0`

| field | value |
|---|---|
| pair_id | 29 |
| stratum | SwitchSig |
| match_type | `SwitchSigHasher` |
| BSim sim×conf | n/a (non-BSim) |
| TU (Wii) | `MusicLibrary.o` |
| Wii symbol (demangled) | `MusicLibrary const::ActiveScoreType(...)` |
| Wii addr (Bank 8) | `0x80300f00` |
| Xenon addr | `0x8233afb0` |
| Xenon func name | `Function_8233AFB0` (stripped binary — name is auto-generated) |
| Wii body size | 199 asm lines (lines 8269-8466 in `build/SZBE69_B8/asm/band3/meta_band/MusicLibrary.s`) |
| Xenon body size | 632 bytes |

## How to read this

The Wii symbol is **ground truth** (from the CodeWarrior linker map). The Xenon
function is from a **stripped** XEX — its name `Function_<addr>` is meaningless.
The claim is that these two functions are the **same source function** compiled by
two different toolchains (Wii MWCC/Gekko PPC vs Xbox360 MSVC/Xenon PPC). Judge
whether the *control-flow shape, constant pool, field offsets, and especially the
resolved callees* are consistent with that claim. The 'resolved callee' column
below maps each Xenon callee to its matched Wii symbol where a match exists —
**agreeing callee names are the strongest cross-compiler signal.**

## Xenon callees (3 total, 3 resolved to a matched Wii symbol)

| xenon callee addr | xenon name | resolved Wii symbol (via matches.json) |
|---|---|---|
| `0x8279b788` | `??0Symbol@@QAA@PBD@Z` | `Enter__11RndPollableFv` |
| `0x82803f10` | `FUN_82803f10` | `FUN_80a3935c` |
| `0x8227caa0` | `Function_8227CAA0` | `MakeString<i>__FPCci_PCc` |

## Referenced strings (Xenon side, 7)

- `'guitar'`
- `'vocals'`
- `'real_guitar'`
- `'real_bass'`
- `'real_keys'`
- `'pending'`
- `'unrecognized instrument type "%d"'`

## Xenon pseudo-C (Ghidra, fork 12.2, PowerPC:BE:64:Xenon)

```c

void Function_8233AFB0(undefined8 param_1,undefined8 param_2)

{
  int iVar1;
  undefined4 uVar2;
  
  iVar1 = FUN_82803f10();
  *(int *)(iVar1 + 0x10) = (int)param_2;
  if ((DAT_82c8d094 & 1) == 0) {
    DAT_82c8d094 = DAT_82c8d094 | 1;
    __0Symbol__QAA_PBD_Z(0xffffffff82c8d090,0xffffffff82013988);
  }
  if ((DAT_82c8d094 & 2) == 0) {
    DAT_82c8d094 = DAT_82c8d094 | 2;
    __0Symbol__QAA_PBD_Z(0xffffffff82c8d08c,0xffffffff82013980);
  }
  if ((DAT_82c8d094 & 4) == 0) {
    DAT_82c8d094 = DAT_82c8d094 | 4;
    __0Symbol__QAA_PBD_Z(0xffffffff82c8d088,0xffffffff820108e4);
  }
  if ((DAT_82c8d094 & 8) == 0) {
    DAT_82c8d094 = DAT_82c8d094 | 8;
    __0Symbol__QAA_PBD_Z(0xffffffff82c8d084,0xffffffff8201d27c);
  }
  if ((DAT_82c8d094 & 0x10) == 0) {
    DAT_82c8d094 = DAT_82c8d094 | 0x10;
    __0Symbol__QAA_PBD_Z(0xffffffff82c8d080,0xffffffff8201d274);
  }
  if ((DAT_82c8d094 & 0x20) == 0) {
    DAT_82c8d094 = DAT_82c8d094 | 0x20;
    __0Symbol__QAA_PBD_Z(0xffffffff82c8d07c,0xffffffff8201d268);
  }
  if ((DAT_82c8d094 & 0x40) == 0) {
    DAT_82c8d094 = DAT_82c8d094 | 0x40;
    __0Symbol__QAA_PBD_Z(0xffffffff82c8d078,0xffffffff8201d250);
  }
  if ((DAT_82c8d094 & 0x80) == 0) {
    DAT_82c8d094 = DAT_82c8d094 | 0x80;
    __0Symbol__QAA_PBD_Z(0xffffffff82c8d074,0xffffffff8201d25c);
  }
  if ((DAT_82c8d094 & 0x100) == 0) {
    DAT_82c8d094 = DAT_82c8d094 | 0x100;
    __0Symbol__QAA_PBD_Z(0xffffffff82c8d070,0xffffffff820126fc);
  }
  if ((DAT_82c8d094 & 0x200) == 0) {
    DAT_82c8d094 = DAT_82c8d094 | 0x200;
    __0Symbol__QAA_PBD_Z(0xffffffff82c8d06c,0xffffffff8201d248);
  }
  switch((int)param_2 + 2) {
  case 0:
    uVar2 = DAT_82c8d06c;
    break;
  case 1:
    uVar2 = DAT_82c8d070;
    break;
  case 2:
    uVar2 = DAT_82c8d090;
    break;
  case 3:
    uVar2 = DAT_82c8d088;
    break;
  case 4:
    uVar2 = DAT_82c8d08c;
    break;
  case 5:
    uVar2 = DAT_82c8d084;
    break;
  case 6:
    uVar2 = DAT_82c8d080;
    break;
  case 7:
    uVar2 = DAT_82c8d07c;
    break;
  case 8:
    uVar2 = DAT_82c8d078;
    break;
  case 9:
    uVar2 = DAT_82c8d074;
    break;
  default:
    Function_8227CAA0(0xffffffff82039384,param_2);
    return;
  }
  *(undefined4 *)(iVar1 + 0xc) = uVar2;
  return;
}


```

## Wii m2c decompilation (target Bank-8 asm → C, `m2c --target ppc`)

```c
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
```

## Wii target asm (Bank 8, ground-truth body)

```asm
# MusicLibrary::ActiveScoreType() const
.fn ActiveScoreType__12MusicLibraryCFv, global
/* 80300F00 002F5920  94 21 FF E0 */	stwu r1, -0x20(r1)
/* 80300F04 002F5924  7C 08 02 A6 */	mflr r0
/* 80300F08 002F5928  3C 80 80 C9 */	lis r4, TheBandUserMgr@ha
/* 80300F0C 002F592C  90 01 00 24 */	stw r0, 0x24(r1)
/* 80300F10 002F5930  38 00 00 00 */	li r0, 0x0
/* 80300F14 002F5934  93 E1 00 1C */	stw r31, 0x1c(r1)
/* 80300F18 002F5938  93 C1 00 18 */	stw r30, 0x18(r1)
/* 80300F1C 002F593C  93 A1 00 14 */	stw r29, 0x14(r1)
/* 80300F20 002F5940  7C 7D 1B 78 */	mr r29, r3
/* 80300F24 002F5944  80 64 E9 B8 */	lwz r3, TheBandUserMgr@l(r4)
/* 80300F28 002F5948  38 81 00 08 */	addi r4, r1, 0x8
/* 80300F2C 002F594C  90 01 00 08 */	stw r0, 0x8(r1)
/* 80300F30 002F5950  B0 01 00 0C */	sth r0, 0xc(r1)
/* 80300F34 002F5954  B0 01 00 0E */	sth r0, 0xe(r1)
/* 80300F38 002F5958  4B E6 7D F9 */	bl "GetBandUsersInSession__11BandUserMgrCFRQ211stlpmtx_std63vector<P8BandUser,Us,Q211stlpmtx_std24StlNodeAlloc<P8BandUser>>"
/* 80300F3C 002F595C  A0 01 00 0C */	lhz r0, 0xc(r1)
/* 80300F40 002F5960  3B C0 00 00 */	li r30, 0x0
/* 80300F44 002F5964  28 00 00 01 */	cmplwi r0, 0x1
/* 80300F48 002F5968  41 82 00 0C */	beq .L_80300F54
/* 80300F4C 002F596C  3B E0 00 0A */	li r31, 0xa
/* 80300F50 002F5970  48 00 00 8C */	b .L_80300FDC
.L_80300F54:
/* 80300F54 002F5974  80 61 00 08 */	lwz r3, 0x8(r1)
/* 80300F58 002F5978  83 C3 00 00 */	lwz r30, 0x0(r3)
/* 80300F5C 002F597C  2C 1E 00 00 */	cmpwi r30, 0x0
/* 80300F60 002F5980  40 82 00 30 */	bne .L_80300F90
/* 80300F64 002F5984  3C 60 80 BB */	lis r3, kAssertStr@ha
/* 80300F68 002F5988  3C 80 80 BA */	lis r4, "@stringBase0"@ha
/* 80300F6C 002F598C  38 84 A4 E4 */	addi r4, r4, "@stringBase0"@l
/* 80300F70 002F5990  80 63 28 58 */	lwz r3, kAssertStr@l(r3)
/* 80300F74 002F5994  38 C4 05 3A */	addi r6, r4, 0x53a
/* 80300F78 002F5998  38 A0 07 D9 */	li r5, 0x7d9
/* 80300F7C 002F599C  4B D1 00 C5 */	bl "MakeString<PCc,i,PCc>__FPCcPCciPCc_PCc"
/* 80300F80 002F59A0  3C A0 80 C9 */	lis r5, TheDebug@ha
/* 80300F84 002F59A4  7C 64 1B 78 */	mr r4, r3
/* 80300F88 002F59A8  38 65 73 D8 */	addi r3, r5, TheDebug@l
/* 80300F8C 002F59AC  48 11 DA 35 */	bl Fail__5DebugFPCc
.L_80300F90:
/* 80300F90 002F59B0  7F C3 F3 78 */	mr r3, r30
/* 80300F94 002F59B4  4B E6 14 FD */	bl GetTrackType__8BandUserCFv
/* 80300F98 002F59B8  2C 03 00 0A */	cmpwi r3, 0xa
/* 80300F9C 002F59BC  41 82 00 20 */	beq .L_80300FBC
/* 80300FA0 002F59C0  7F C3 F3 78 */	mr r3, r30
/* 80300FA4 002F59C4  4B E6 14 ED */	bl GetTrackType__8BandUserCFv
/* 80300FA8 002F59C8  38 80 00 00 */	li r4, 0x0
/* 80300FAC 002F59CC  38 A0 00 00 */	li r5, 0x0
/* 80300FB0 002F59D0  4B E7 0D 21 */	bl TrackTypeToScoreType__F9TrackTypebb
/* 80300FB4 002F59D4  7C 7F 1B 78 */	mr r31, r3
/* 80300FB8 002F59D8  48 00 00 24 */	b .L_80300FDC
.L_80300FBC:
/* 80300FBC 002F59DC  7F C3 F3 78 */	mr r3, r30
/* 80300FC0 002F59E0  4B E6 17 A1 */	bl GetControllerType__8BandUserCFv
/* 80300FC4 002F59E4  38 80 00 00 */	li r4, 0x0
/* 80300FC8 002F59E8  4B E7 0C 89 */	bl ControllerTypeToTrackType__F14ControllerTypeb
/* 80300FCC 002F59EC  38 80 00 00 */	li r4, 0x0
/* 80300FD0 002F59F0  38 A0 00 00 */	li r5, 0x0
/* 80300FD4 002F59F4  4B E7 0C FD */	bl TrackTypeToScoreType__F9TrackTypebb
/* 80300FD8 002F59F8  7C 7F 1B 78 */	mr r31, r3
.L_80300FDC:
/* 80300FDC 002F59FC  2C 1F 00 00 */	cmpwi r31, 0x0
/* 80300FE0 002F5A00  40 82 00 50 */	bne .L_80301030
/* 80300FE4 002F5A04  2C 1E 00 00 */	cmpwi r30, 0x0
/* 80300FE8 002F5A08  40 82 00 30 */	bne .L_80301018
/* 80300FEC 002F5A0C  3C 60 80 BB */	lis r3, kAssertStr@ha
/* 80300FF0 002F5A10  3C 80 80 BA */	lis r4, "@stringBase0"@ha
/* 80300FF4 002F5A14  38 84 A4 E4 */	addi r4, r4, "@stringBase0"@l
/* 80300FF8 002F5A18  80 63 28 58 */	lwz r3, kAssertStr@l(r3)
/* 80300FFC 002F5A1C  38 C4 05 3A */	addi r6, r4, 0x53a
/* 80301000 002F5A20  38 A0 07 EB */	li r5, 0x7eb
/* 80301004 002F5A24  4B D1 00 3D */	bl "MakeString<PCc,i,PCc>__FPCcPCciPCc_PCc"
/* 80301008 002F5A28  3C A0 80 C9 */	lis r5, TheDebug@ha
/* 8030100C 002F5A2C  7C 64 1B 78 */	mr r4, r3
/* 80301010 002F5A30  38 65 73 D8 */	addi r3, r5, TheDebug@l
/* 80301014 002F5A34  48 11 D9 AD */	bl Fail__5DebugFPCc
.L_80301018:
/* 80301018 002F5A38  7F C3 F3 78 */	mr r3, r30
/* 8030101C 002F5A3C  4B E6 16 A5 */	bl GetPreferredScoreType__8BandUserCFv
/* 80301020 002F5A40  2C 03 00 06 */	cmpwi r3, 0x6
/* 80301024 002F5A44  40 82 00 5C */	bne .L_80301080
/* 80301028 002F5A48  3B E0 00 06 */	li r31, 0x6
/* 8030102C 002F5A4C  48 00 00 54 */	b .L_80301080
.L_80301030:
/* 80301030 002F5A50  2C 1F 00 03 */	cmpwi r31, 0x3
/* 80301034 002F5A54  40 82 00 4C */	bne .L_80301080
/* 80301038 002F5A58  2C 1E 00 00 */	cmpwi r30, 0x0
/* 8030103C 002F5A5C  40 82 00 30 */	bne .L_8030106C
/* 80301040 002F5A60  3C 60 80 BB */	lis r3, kAssertStr@ha
/* 80301044 002F5A64  3C 80 80 BA */	lis r4, "@stringBase0"@ha
/* 80301048 002F5A68  38 84 A4 E4 */	addi r4, r4, "@stringBase0"@l
/* 8030104C 002F5A6C  80 63 28 58 */	lwz r3, kAssertStr@l(r3)
/* 80301050 002F5A70  38 C4 05 3A */	addi r6, r4, 0x53a
/* 80301054 002F5A74  38 A0 07 F2 */	li r5, 0x7f2
/* 80301058 002F5A78  4B D0 FF E9 */	bl "MakeString<PCc,i,PCc>__FPCcPCciPCc_PCc"
/* 8030105C 002F5A7C  3C A0 80 C9 */	lis r5, TheDebug@ha
/* 80301060 002F5A80  7C 64 1B 78 */	mr r4, r3
/* 80301064 002F5A84  38 65 73 D8 */	addi r3, r5, TheDebug@l
/* 80301068 002F5A88  48 11 D9 59 */	bl Fail__5DebugFPCc
.L_8030106C:
/* 8030106C 002F5A8C  7F C3 F3 78 */	mr r3, r30
/* 80301070 002F5A90  4B E6 16 51 */	bl GetPreferredScoreType__8BandUserCFv
/* 80301074 002F5A94  2C 03 00 04 */	cmpwi r3, 0x4
/* 80301078 002F5A98  40 82 00 08 */	bne .L_80301080
/* 8030107C 002F5A9C  3B E0 00 04 */	li r31, 0x4
.L_80301080:
/* 80301080 002F5AA0  28 1F 00 09 */	cmplwi r31, 0x9
/* 80301084 002F5AA4  41 81 00 E0 */	bgt .L_80301164
/* 80301088 002F5AA8  3C 60 80 BA */	lis r3, "@72433"@ha
/* 8030108C 002F5AAC  57 E0 10 3A */	slwi r0, r31, 2
/* 80301090 002F5AB0  38 63 9F 1C */	addi r3, r3, "@72433"@l
/* 80301094 002F5AB4  7C 63 00 2E */	lwzx r3, r3, r0
/* 80301098 002F5AB8  7C 69 03 A6 */	mtctr r3
/* 8030109C 002F5ABC  4E 80 04 20 */	bctr
.L_803010A0:
/* 803010A0 002F5AC0  80 1D 00 70 */	lwz r0, 0x70(r29)
/* 803010A4 002F5AC4  2C 00 00 01 */	cmpwi r0, 0x1
/* 803010A8 002F5AC8  40 82 00 BC */	bne .L_80301164
/* 803010AC 002F5ACC  3B E0 00 01 */	li r31, 0x1
/* 803010B0 002F5AD0  48 00 00 B4 */	b .L_80301164
.L_803010B4:
/* 803010B4 002F5AD4  80 1D 00 70 */	lwz r0, 0x70(r29)
/* 803010B8 002F5AD8  2C 00 00 02 */	cmpwi r0, 0x2
/* 803010BC 002F5ADC  40 82 00 A8 */	bne .L_80301164
/* 803010C0 002F5AE0  3B E0 00 02 */	li r31, 0x2
/* 803010C4 002F5AE4  48 00 00 A0 */	b .L_80301164
.L_803010C8:
/* 803010C8 002F5AE8  80 1D 00 70 */	lwz r0, 0x70(r29)
/* 803010CC 002F5AEC  2C 00 00 06 */	cmpwi r0, 0x6
/* 803010D0 002F5AF0  40 82 00 94 */	bne .L_80301164
/* 803010D4 002F5AF4  3B E0 00 06 */	li r31, 0x6
/* 803010D8 002F5AF8  48 00 00 8C */	b .L_80301164
.L_803010DC:
/* 803010DC 002F5AFC  80 1D 00 70 */	lwz r0, 0x70(r29)
/* 803010E0 002F5B00  2C 00 00 00 */	cmpwi r0, 0x0
/* 803010E4 002F5B04  40 82 00 80 */	bne .L_80301164
/* 803010E8 002F5B08  3B E0 00 00 */	li r31, 0x0
/* 803010EC 002F5B0C  48 00 00 78 */	b .L_80301164
.L_803010F0:
/* 803010F0 002F5B10  80 1D 00 70 */	lwz r0, 0x70(r29)
/* 803010F4 002F5B14  2C 00 00 04 */	cmpwi r0, 0x4
/* 803010F8 002F5B18  40 82 00 6C */	bne .L_80301164
/* 803010FC 002F5B1C  3B E0 00 04 */	li r31, 0x4
/* 80301100 002F5B20  48 00 00 64 */	b .L_80301164
.L_80301104:
/* 80301104 002F5B24  80 1D 00 70 */	lwz r0, 0x70(r29)
/* 80301108 002F5B28  2C 00 00 03 */	cmpwi r0, 0x3
/* 8030110C 002F5B2C  40 82 00 58 */	bne .L_80301164
/* 80301110 002F5B30  3B E0 00 03 */	li r31, 0x3
/* 80301114 002F5B34  48 00 00 50 */	b .L_80301164
... [truncated 49 of 199 asm lines]
```
