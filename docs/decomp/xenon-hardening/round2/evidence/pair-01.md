# Pair 01 — verification evidence

**Claimed identity:** Wii `RebuildSharedSongData__12MusicLibraryFv`  ==  Xenon `0x8252a9f0`

| field | value |
|---|---|
| pair_id | 01 |
| stratum | BSIM>=30 |
| match_type | `BSIM` |
| BSim sim×conf | 35.635 |
| BSim similarity / confidence | 0.621 / 57.383 |
| TU (Wii) | `MusicLibrary.o` |
| Wii symbol (demangled) | `MusicLibrary::RebuildSharedSongData(...)` |
| Wii addr (Bank 8) | `0x80303300` |
| Xenon addr | `0x8252a9f0` |
| Xenon func name | `Function_8252A9F0` (stripped binary — name is auto-generated) |
| Wii body size | 98 asm lines (lines 10985-11081 in `build/SZBE69_B8/asm/band3/meta_band/MusicLibrary.s`) |
| Xenon body size | 268 bytes |

## How to read this

The Wii symbol is **ground truth** (from the CodeWarrior linker map). The Xenon
function is from a **stripped** XEX — its name `Function_<addr>` is meaningless.
The claim is that these two functions are the **same source function** compiled by
two different toolchains (Wii MWCC/Gekko PPC vs Xbox360 MSVC/Xenon PPC). Judge
whether the *control-flow shape, constant pool, field offsets, and especially the
resolved callees* are consistent with that claim. The 'resolved callee' column
below maps each Xenon callee to its matched Wii symbol where a match exists —
**agreeing callee names are the strongest cross-compiler signal.**

## Xenon callees (10 total, 7 resolved to a matched Wii symbol)

| xenon callee addr | xenon name | resolved Wii symbol (via matches.json) |
|---|---|---|
| `0x8257e7b0` | `FUN_8257e7b0` | `GetSort__11SongSortMgrF12SongSortType` |
| `0x82560170` | `Function_82560170` | _(unmatched / Function_)_ |
| `0x825a1e50` | `Function_825A1E50` | `UpdateSharedStatus__10SongRecordFv` |
| `0x82529df0` | `Function_82529DF0` | `PushHighlightToScreen__12MusicLibraryFb` |
| `0x82803f2c` | `FUN_82803f2c` | `FUN_80a39378` |
| `0x825a6b38` | `FUN_825a6b38` | `GetNode__8NodeSortCFi` |
| `0x82261818` | `FUN_82261818` | `_M_increment__Q211stlpmtx_std13_Rb_global<b>FPQ211stlpmtx_std18_Rb_tree_node_base` |
| `0x8252a408` | `Function_8252A408` | `PushSonglistToScreen__12MusicLibraryFv` |
| `0x82804da8` | `Function_82804DA8` | _(unmatched / Function_)_ |
| `0x825a1ec0` | `Function_825A1EC0` | _(unmatched / Function_)_ |

## Referenced strings (Xenon side, 0)

_(none)_

## Xenon pseudo-C (Ghidra, fork 12.2, PowerPC:BE:64:Xenon)

```c

void Function_8252A9F0(void)

{
  bool bVar1;
  bool bVar2;
  int iVar3;
  int iVar5;
  undefined8 uVar4;
  int iVar6;
  char cVar8;
  int iVar7;
  char cVar9;
  
  iVar5 = FUN_82803f2c();
  Function_82560170(PTR_DAT_82c424e0);
  uVar4 = FUN_8257e7b0(DAT_82dcd8a8,*(undefined4 *)(iVar5 + 0xfc));
  uVar4 = FUN_825a6b38(uVar4,*(undefined4 *)(iVar5 + 0xf0));
  iVar6 = Function_82804DA8(uVar4,0,0xffffffff82c41ed8,0xffffffff82c41ef0,0);
  iVar3 = DAT_82dcd8a8;
  if (iVar6 != 0) {
    cVar9 = '\x01';
    if (*(char *)(*(int *)(iVar6 + 0x40) + 0x2e) != '\0') goto LAB_8252aa60;
  }
  cVar9 = '\0';
LAB_8252aa60:
  bVar1 = false;
  for (iVar7 = *(int *)(DAT_82dcd8a8 + 0xc); iVar7 != iVar3 + 4; iVar7 = FUN_82261818(iVar7)) {
    cVar8 = Function_825A1EC0(iVar7 + 0x14);
    if (cVar8 != '\0') {
      bVar1 = true;
      Function_825A1E50(iVar7 + 0x14);
    }
  }
  if ((iVar6 == 0) || (bVar2 = true, *(char *)(*(int *)(iVar6 + 0x40) + 0x2e) == cVar9)) {
    bVar2 = false;
  }
  if ((bVar1) && (Function_8252A408(iVar5), bVar2)) {
    Function_82529DF0(iVar5,0);
  }
  return;
}


```

## Wii m2c decompilation (target Bank-8 asm → C, `m2c --target ppc`)

```c
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
```

## Wii target asm (Bank 8, ground-truth body)

```asm
# MusicLibrary::RebuildSharedSongData()
.fn RebuildSharedSongData__12MusicLibraryFv, global
/* 80303300 002F7D20  94 21 FF E0 */	stwu r1, -0x20(r1)
/* 80303304 002F7D24  7C 08 02 A6 */	mflr r0
/* 80303308 002F7D28  90 01 00 24 */	stw r0, 0x24(r1)
/* 8030330C 002F7D2C  39 61 00 20 */	addi r11, r1, 0x20
/* 80303310 002F7D30  48 73 60 1D */	bl _savegpr_26
/* 80303314 002F7D34  7C 7A 1B 78 */	mr r26, r3
/* 80303318 002F7D38  3C 60 80 C9 */	lis r3, TheSongSortMgr@ha
/* 8030331C 002F7D3C  80 63 12 48 */	lwz r3, TheSongSortMgr@l(r3)
/* 80303320 002F7D40  80 9A 00 DC */	lwz r4, 0xdc(r26)
/* 80303324 002F7D44  48 07 71 ED */	bl GetSort__11SongSortMgrF12SongSortType
/* 80303328 002F7D48  80 9A 00 D0 */	lwz r4, 0xd0(r26)
/* 8030332C 002F7D4C  48 06 C2 B5 */	bl GetNode__8NodeSortCFi
/* 80303330 002F7D50  3C A0 80 BA */	lis r5, __RTTI__17OwnedSongSortNode@ha
/* 80303334 002F7D54  3C C0 80 B9 */	lis r6, __RTTI__8SortNode@ha
/* 80303338 002F7D58  38 A5 A3 10 */	addi r5, r5, __RTTI__17OwnedSongSortNode@l
/* 8030333C 002F7D5C  38 80 00 00 */	li r4, 0x0
/* 80303340 002F7D60  38 C6 AF C8 */	addi r6, r6, __RTTI__8SortNode@l
/* 80303344 002F7D64  38 E0 00 00 */	li r7, 0x0
/* 80303348 002F7D68  48 73 5C 0D */	bl __dynamic_cast
/* 8030334C 002F7D6C  2C 03 00 00 */	cmpwi r3, 0x0
/* 80303350 002F7D70  7C 7E 1B 78 */	mr r30, r3
/* 80303354 002F7D74  3B A0 00 00 */	li r29, 0x0
/* 80303358 002F7D78  41 82 00 18 */	beq .L_80303370
/* 8030335C 002F7D7C  80 63 00 34 */	lwz r3, 0x34(r3)
/* 80303360 002F7D80  88 03 00 20 */	lbz r0, 0x20(r3)
/* 80303364 002F7D84  2C 00 00 00 */	cmpwi r0, 0x0
/* 80303368 002F7D88  41 82 00 08 */	beq .L_80303370
/* 8030336C 002F7D8C  3B A0 00 01 */	li r29, 0x1
.L_80303370:
/* 80303370 002F7D90  3C 60 80 C9 */	lis r3, TheSongSortMgr@ha
/* 80303374 002F7D94  3B 60 00 00 */	li r27, 0x0
/* 80303378 002F7D98  80 63 12 48 */	lwz r3, TheSongSortMgr@l(r3)
/* 8030337C 002F7D9C  83 83 00 0C */	lwz r28, 0xc(r3)
/* 80303380 002F7DA0  3B E3 00 04 */	addi r31, r3, 0x4
/* 80303384 002F7DA4  48 00 00 24 */	b .L_803033A8
.L_80303388:
/* 80303388 002F7DA8  38 7C 00 14 */	addi r3, r28, 0x14
/* 8030338C 002F7DAC  48 06 7C 95 */	bl UpdateSharedStatus__10SongRecordFv
/* 80303390 002F7DB0  2C 03 00 00 */	cmpwi r3, 0x0
/* 80303394 002F7DB4  41 82 00 08 */	beq .L_8030339C
/* 80303398 002F7DB8  3B 60 00 01 */	li r27, 0x1
.L_8030339C:
/* 8030339C 002F7DBC  7F 83 E3 78 */	mr r3, r28
/* 803033A0 002F7DC0  4B D1 61 A1 */	bl "_M_increment__Q211stlpmtx_std13_Rb_global<b>FPQ211stlpmtx_std18_Rb_tree_node_base"
/* 803033A4 002F7DC4  7C 7C 1B 78 */	mr r28, r3
.L_803033A8:
/* 803033A8 002F7DC8  7C 1C F8 40 */	cmplw r28, r31
/* 803033AC 002F7DCC  40 82 FF DC */	bne .L_80303388
/* 803033B0 002F7DD0  2C 1E 00 00 */	cmpwi r30, 0x0
/* 803033B4 002F7DD4  3B E0 00 00 */	li r31, 0x0
/* 803033B8 002F7DD8  41 82 00 18 */	beq .L_803033D0
/* 803033BC 002F7DDC  80 7E 00 34 */	lwz r3, 0x34(r30)
/* 803033C0 002F7DE0  88 03 00 20 */	lbz r0, 0x20(r3)
/* 803033C4 002F7DE4  7C 1D 00 40 */	cmplw r29, r0
/* 803033C8 002F7DE8  41 82 00 08 */	beq .L_803033D0
/* 803033CC 002F7DEC  3B E0 00 01 */	li r31, 0x1
.L_803033D0:
/* 803033D0 002F7DF0  2C 1F 00 00 */	cmpwi r31, 0x0
/* 803033D4 002F7DF4  38 00 00 00 */	li r0, 0x0
/* 803033D8 002F7DF8  41 82 00 0C */	beq .L_803033E4
/* 803033DC 002F7DFC  2C 1B 00 00 */	cmpwi r27, 0x0
/* 803033E0 002F7E00  41 82 00 08 */	beq .L_803033E8
.L_803033E4:
/* 803033E4 002F7E04  38 00 00 01 */	li r0, 0x1
.L_803033E8:
/* 803033E8 002F7E08  2C 00 00 00 */	cmpwi r0, 0x0
/* 803033EC 002F7E0C  40 82 00 30 */	bne .L_8030341C
/* 803033F0 002F7E10  3C 60 80 BB */	lis r3, kAssertStr@ha
/* 803033F4 002F7E14  3C 80 80 BA */	lis r4, "@stringBase0"@ha
/* 803033F8 002F7E18  38 84 A4 E4 */	addi r4, r4, "@stringBase0"@l
/* 803033FC 002F7E1C  80 63 28 58 */	lwz r3, kAssertStr@l(r3)
/* 80303400 002F7E20  38 C4 06 A3 */	addi r6, r4, 0x6a3
/* 80303404 002F7E24  38 A0 0A FD */	li r5, 0xafd
/* 80303408 002F7E28  4B D0 DC 39 */	bl "MakeString<PCc,i,PCc>__FPCcPCciPCc_PCc"
/* 8030340C 002F7E2C  3C A0 80 C9 */	lis r5, TheDebug@ha
/* 80303410 002F7E30  7C 64 1B 78 */	mr r4, r3
/* 80303414 002F7E34  38 65 73 D8 */	addi r3, r5, TheDebug@l
/* 80303418 002F7E38  48 11 B5 A9 */	bl Fail__5DebugFPCc
.L_8030341C:
/* 8030341C 002F7E3C  2C 1B 00 00 */	cmpwi r27, 0x0
/* 80303420 002F7E40  41 82 00 20 */	beq .L_80303440
/* 80303424 002F7E44  7F 43 D3 78 */	mr r3, r26
/* 80303428 002F7E48  4B FF E4 89 */	bl PushSonglistToScreen__12MusicLibraryFv
/* 8030342C 002F7E4C  2C 1F 00 00 */	cmpwi r31, 0x0
/* 80303430 002F7E50  41 82 00 10 */	beq .L_80303440
/* 80303434 002F7E54  7F 43 D3 78 */	mr r3, r26
/* 80303438 002F7E58  38 80 00 00 */	li r4, 0x0
/* 8030343C 002F7E5C  4B FF DF B5 */	bl PushHighlightToScreen__12MusicLibraryFb
.L_80303440:
/* 80303440 002F7E60  39 61 00 20 */	addi r11, r1, 0x20
/* 80303444 002F7E64  48 73 5F 35 */	bl _restgpr_26
/* 80303448 002F7E68  80 01 00 24 */	lwz r0, 0x24(r1)
/* 8030344C 002F7E6C  7C 08 03 A6 */	mtlr r0
/* 80303450 002F7E70  38 21 00 20 */	addi r1, r1, 0x20
/* 80303454 002F7E74  4E 80 00 20 */	blr
.endfn RebuildSharedSongData__12MusicLibraryFv
```
