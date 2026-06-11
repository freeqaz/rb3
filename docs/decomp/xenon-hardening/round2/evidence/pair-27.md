# Pair 27 — verification evidence

**Claimed identity:** Wii `DifficultySortPart__12MusicLibraryCFv`  ==  Xenon `0x8252c728`

| field | value |
|---|---|
| pair_id | 27 |
| stratum | SwitchSig |
| match_type | `SwitchSigHasher` |
| BSim sim×conf | n/a (non-BSim) |
| TU (Wii) | `MusicLibrary.o` |
| Wii symbol (demangled) | `MusicLibrary const::DifficultySortPart(...)` |
| Wii addr (Bank 8) | `0x80300e10` |
| Xenon addr | `0x8252c728` |
| Xenon func name | `Function_8252C728` (stripped binary — name is auto-generated) |
| Wii body size | 73 asm lines (lines 8193-8264 in `build/SZBE69_B8/asm/band3/meta_band/MusicLibrary.s`) |
| Xenon body size | 584 bytes |

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
| `0x8252b900` | `Function_8252B900` | `Save__Q229@unnamed@WaitingUserGate_cpp@18OpenWaitingGateMsgCFR9BinStream` |
| `0x82803f14` | `FUN_82803f14` | `__restore_gpr` |

## Referenced strings (Xenon side, 5)

- `'guitar'`
- `'vocals'`
- `'real_guitar'`
- `'real_bass'`
- `'real_keys'`

## Xenon pseudo-C (Ghidra, fork 12.2, PowerPC:BE:64:Xenon)

```c

undefined4 * Function_8252C728(undefined8 param_1,undefined8 param_2)

{
  undefined4 *puVar1;
  undefined4 uVar2;
  
  puVar1 = (undefined4 *)FUN_82803f14();
  if ((DAT_82dcc0e8 & 1) == 0) {
    DAT_82dcc0e8 = DAT_82dcc0e8 | 1;
    __0Symbol__QAA_PBD_Z(0xffffffff82dcc0e4,0xffffffff820105d0);
  }
  if ((DAT_82dcc0e8 & 2) == 0) {
    DAT_82dcc0e8 = DAT_82dcc0e8 | 2;
    __0Symbol__QAA_PBD_Z(0xffffffff82dcc0e0,0xffffffff82013988);
  }
  if ((DAT_82dcc0e8 & 4) == 0) {
    DAT_82dcc0e8 = DAT_82dcc0e8 | 4;
    __0Symbol__QAA_PBD_Z(0xffffffff82dcc0dc,0xffffffff82013980);
  }
  if ((DAT_82dcc0e8 & 8) == 0) {
    DAT_82dcc0e8 = DAT_82dcc0e8 | 8;
    __0Symbol__QAA_PBD_Z(0xffffffff82dcc0d8,0xffffffff820108e4);
  }
  if ((DAT_82dcc0e8 & 0x10) == 0) {
    DAT_82dcc0e8 = DAT_82dcc0e8 | 0x10;
    __0Symbol__QAA_PBD_Z(0xffffffff82dcc0d4,0xffffffff8201d27c);
  }
  if ((DAT_82dcc0e8 & 0x20) == 0) {
    DAT_82dcc0e8 = DAT_82dcc0e8 | 0x20;
    __0Symbol__QAA_PBD_Z(0xffffffff82dcc0d0,0xffffffff8201d274);
  }
  if ((DAT_82dcc0e8 & 0x40) == 0) {
    DAT_82dcc0e8 = DAT_82dcc0e8 | 0x40;
    __0Symbol__QAA_PBD_Z(0xffffffff82dcc0cc,0xffffffff8201d268);
  }
  if ((DAT_82dcc0e8 & 0x80) == 0) {
    DAT_82dcc0e8 = DAT_82dcc0e8 | 0x80;
    __0Symbol__QAA_PBD_Z(0xffffffff82dcc0c8,0xffffffff8201d250);
  }
  if ((DAT_82dcc0e8 & 0x100) == 0) {
    DAT_82dcc0e8 = DAT_82dcc0e8 | 0x100;
    __0Symbol__QAA_PBD_Z(0xffffffff82dcc0c4,0xffffffff8201d25c);
  }
  uVar2 = Function_8252B900(param_2);
  switch(uVar2) {
  case 0:
  case 6:
    uVar2 = DAT_82dcc0d8;
    break;
  case 1:
    uVar2 = DAT_82dcc0dc;
    break;
  case 2:
    uVar2 = DAT_82dcc0e0;
    break;
  case 3:
  case 4:
    uVar2 = DAT_82dcc0d4;
    break;
  case 5:
    uVar2 = DAT_82dcc0d0;
    break;
  case 7:
    uVar2 = DAT_82dcc0cc;
    break;
  case 8:
    uVar2 = DAT_82dcc0c8;
    break;
  case 9:
    uVar2 = DAT_82dcc0c4;
    break;
  case 10:
    uVar2 = DAT_82dcc0e4;
    break;
  default:
    __0Symbol__QAA_PBD_Z(puVar1,PTR_DAT_82c411b0);
    return puVar1;
  }
  *puVar1 = uVar2;
  return puVar1;
}


```

## Wii m2c decompilation (target Bank-8 asm → C, `m2c --target ppc`)

```c
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
```

## Wii target asm (Bank 8, ground-truth body)

```asm
# MusicLibrary::DifficultySortPart() const
.fn DifficultySortPart__12MusicLibraryCFv, global
/* 80300E10 002F5830  94 21 F7 E0 */	stwu r1, -0x820(r1)
/* 80300E14 002F5834  7C 08 02 A6 */	mflr r0
/* 80300E18 002F5838  90 01 08 24 */	stw r0, 0x824(r1)
/* 80300E1C 002F583C  48 00 00 E5 */	bl ActiveScoreType__12MusicLibraryCFv
/* 80300E20 002F5840  28 03 00 0A */	cmplwi r3, 0xa
/* 80300E24 002F5844  41 81 00 88 */	bgt .L_80300EAC
/* 80300E28 002F5848  3C 80 80 BA */	lis r4, "@72368"@ha
/* 80300E2C 002F584C  54 60 10 3A */	slwi r0, r3, 2
/* 80300E30 002F5850  38 84 9E F0 */	addi r4, r4, "@72368"@l
/* 80300E34 002F5854  7C 84 00 2E */	lwzx r4, r4, r0
/* 80300E38 002F5858  7C 89 03 A6 */	mtctr r4
/* 80300E3C 002F585C  4E 80 04 20 */	bctr
.L_80300E40:
/* 80300E40 002F5860  3C 60 80 D1 */	lis r3, band@ha
/* 80300E44 002F5864  80 63 21 10 */	lwz r3, band@l(r3)
/* 80300E48 002F5868  48 00 00 A4 */	b .L_80300EEC
.L_80300E4C:
/* 80300E4C 002F586C  3C 60 80 D1 */	lis r3, guitar@ha
/* 80300E50 002F5870  80 63 41 D4 */	lwz r3, guitar@l(r3)
/* 80300E54 002F5874  48 00 00 98 */	b .L_80300EEC
.L_80300E58:
/* 80300E58 002F5878  3C 60 80 D1 */	lis r3, bass@ha
/* 80300E5C 002F587C  80 63 21 9C */	lwz r3, bass@l(r3)
/* 80300E60 002F5880  48 00 00 8C */	b .L_80300EEC
.L_80300E64:
/* 80300E64 002F5884  3C 60 80 D1 */	lis r3, drum@ha
/* 80300E68 002F5888  80 63 2F 98 */	lwz r3, drum@l(r3)
/* 80300E6C 002F588C  48 00 00 80 */	b .L_80300EEC
.L_80300E70:
/* 80300E70 002F5890  3C 60 80 D1 */	lis r3, vocals@ha
/* 80300E74 002F5894  80 63 15 7C */	lwz r3, vocals@l(r3)
/* 80300E78 002F5898  48 00 00 74 */	b .L_80300EEC
.L_80300E7C:
/* 80300E7C 002F589C  3C 60 80 D1 */	lis r3, keys@ha
/* 80300E80 002F58A0  80 63 4B F4 */	lwz r3, keys@l(r3)
/* 80300E84 002F58A4  48 00 00 68 */	b .L_80300EEC
.L_80300E88:
/* 80300E88 002F58A8  3C 60 80 D1 */	lis r3, real_guitar@ha
/* 80300E8C 002F58AC  80 63 61 00 */	lwz r3, real_guitar@l(r3)
/* 80300E90 002F58B0  48 00 00 5C */	b .L_80300EEC
.L_80300E94:
/* 80300E94 002F58B4  3C 60 80 D1 */	lis r3, real_bass@ha
/* 80300E98 002F58B8  80 63 60 F4 */	lwz r3, real_bass@l(r3)
/* 80300E9C 002F58BC  48 00 00 50 */	b .L_80300EEC
.L_80300EA0:
/* 80300EA0 002F58C0  3C 60 80 D1 */	lis r3, real_keys@ha
/* 80300EA4 002F58C4  80 63 61 30 */	lwz r3, real_keys@l(r3)
/* 80300EA8 002F58C8  48 00 00 44 */	b .L_80300EEC
.L_80300EAC:
/* 80300EAC 002F58CC  3C 80 80 BA */	lis r4, "@stringBase0"@ha
/* 80300EB0 002F58D0  38 61 00 0C */	addi r3, r1, 0xc
/* 80300EB4 002F58D4  38 84 A4 E4 */	addi r4, r4, "@stringBase0"@l
/* 80300EB8 002F58D8  38 84 05 07 */	addi r4, r4, 0x507
/* 80300EBC 002F58DC  48 19 BC 95 */	bl __ct__12FormatStringFPCc
/* 80300EC0 002F58E0  38 61 00 0C */	addi r3, r1, 0xc
/* 80300EC4 002F58E4  48 19 CC 8D */	bl Str__12FormatStringFv
/* 80300EC8 002F58E8  3C A0 80 C9 */	lis r5, TheDebug@ha
/* 80300ECC 002F58EC  7C 64 1B 78 */	mr r4, r3
/* 80300ED0 002F58F0  38 65 73 D8 */	addi r3, r5, TheDebug@l
/* 80300ED4 002F58F4  48 11 DA ED */	bl Fail__5DebugFPCc
/* 80300ED8 002F58F8  3C 80 80 BB */	lis r4, gNullStr@ha
/* 80300EDC 002F58FC  38 61 00 08 */	addi r3, r1, 0x8
/* 80300EE0 002F5900  80 84 5D 30 */	lwz r4, gNullStr@l(r4)
/* 80300EE4 002F5904  48 1B C2 DD */	bl __ct__6SymbolFPCc
/* 80300EE8 002F5908  80 63 00 00 */	lwz r3, 0x0(r3)
.L_80300EEC:
/* 80300EEC 002F590C  80 01 08 24 */	lwz r0, 0x824(r1)
/* 80300EF0 002F5910  7C 08 03 A6 */	mtlr r0
/* 80300EF4 002F5914  38 21 08 20 */	addi r1, r1, 0x820
/* 80300EF8 002F5918  4E 80 00 20 */	blr
.endfn DifficultySortPart__12MusicLibraryCFv
```
