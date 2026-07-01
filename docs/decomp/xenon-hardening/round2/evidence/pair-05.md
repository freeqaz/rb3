# Pair 05 — verification evidence

**Claimed identity:** Wii `Poll__15GemTrainerPanelFv`  ==  Xenon `0x8268d410`

| field | value |
|---|---|
| pair_id | 05 |
| stratum | BSIM>=30 |
| match_type | `BSIM` |
| BSim sim×conf | 31.46 |
| BSim similarity / confidence | 0.405 / 77.68 |
| TU (Wii) | `GemTrainerPanel.o` |
| Wii symbol (demangled) | `GemTrainerPanel::Poll(...)` |
| Wii addr (Bank 8) | `0x801a2ab0` |
| Xenon addr | `0x8268d410` |
| Xenon func name | `Function_8268D410` (stripped binary — name is auto-generated) |
| Wii body size | 314 asm lines (lines 952-1264 in `build/SZBE69_B8/asm/band3/game/GemTrainerPanel.s`) |
| Xenon body size | 792 bytes |

## How to read this

The Wii symbol is **ground truth** (from the CodeWarrior linker map). The Xenon
function is from a **stripped** XEX — its name `Function_<addr>` is meaningless.
The claim is that these two functions are the **same source function** compiled by
two different toolchains (Wii MWCC/Gekko PPC vs Xbox360 MSVC/Xenon PPC). Judge
whether the *control-flow shape, constant pool, field offsets, and especially the
resolved callees* are consistent with that claim. The 'resolved callee' column
below maps each Xenon callee to its matched Wii symbol where a match exists —
**agreeing callee names are the strongest cross-compiler signal.**

## Xenon callees (27 total, 20 resolved to a matched Wii symbol)

| xenon callee addr | xenon name | resolved Wii symbol (via matches.json) |
|---|---|---|
| `0x82803f30` | `FUN_82803f30` | `FUN_80a3937c` |
| `0x82b565d8` | `FUN_82b565d8` | `GetGuideTrack__15VocalGuidePitchCFv` |
| `0x8269fbc8` | `Function_8269FBC8` | `ResetGemStates__9GemPlayerFf` |
| `0x826790b8` | `FUN_826790b8` | `Radius__11CharCollideCFv` |
| `0x827dd308` | `FUN_827dd308` | `FocusPanel__9UIManagerFv` |
| `0x827a3e90` | `thunk_FUN_827ad150` | _(unmatched / Function_)_ |
| `0x826aa980` | `FUN_826aa980` | `GetStartTick__14TrainerSectionCFv` |
| `0x8279b788` | `??0Symbol@@QAA@PBD@Z` | `Enter__11RndPollableFv` |
| `0x8268bee8` | `Function_8268BEE8` | `AddBeatMask__15GemTrainerPanelFi` |
| `0x82b699b0` | `Function_82B699B0` | `ClearMissedPhrases__10GemManagerFv` |
| `0x826d2098` | `Function_826D2098` | _(unmatched / Function_)_ |
| `0x82260570` | `Function_82260570` | `MakeString<PCc,i,PCc>__FPCcPCciPCc_PCc` |
| `0x827b8dc0` | `Function_827B8DC0` | _(unmatched / Function_)_ |
| `0x82263eb0` | `FUN_82263eb0` | `TrackNum__11TrackConfigCFv` |
| `0x82261bc0` | `Function_82261BC0` | _(unmatched / Function_)_ |
| `0x8268bc08` | `Function_8268BC08` | `HandleTrackShifting__15GemTrainerPanelFv` |
| `0x826aad78` | `Function_826AAD78` | `Obj<13LocalBandUser>__9DataArrayCFi_P13LocalBandUser` |
| `0x8265a0f0` | `Function_8265A0F0` | `IsWaiting__4GameFv` |
| `0x82769680` | `Function_82769680` | _(unmatched / Function_)_ |
| `0x82b67f58` | `Function_82B67F58` | _(unmatched / Function_)_ |
| `0x82667218` | `FUN_82667218` | `RecalculateGemTimes__6SongDBFi` |
| `0x826aa990` | `FUN_826aa990` | `GetListString__20TokenRedemptionPanelCFi` |
| `0x8268c550` | `Function_8268C550` | _(unmatched / Function_)_ |
| `0x828050f8` | `Function_828050F8` | `MsToTick__Ff` |
| `0x824590d8` | `FUN_824590d8` | `GetCurrSection__12TrainerPanelCFv` |
| `0x826aa890` | `FUN_826aa890` | `GetTick__12TrainerPanelCFv` |
| `0x827ee118` | `Function_827EE118` | `Poll__7UIPanelFv` |

## Referenced strings (Xenon side, 0)

_(none)_

## Xenon pseudo-C (Ghidra, fork 12.2, PowerPC:BE:64:Xenon)

```c

void Function_8268D410(void)

{
  int *piVar5;
  int iVar6;
  char cVar9;
  undefined8 uVar1;
  undefined8 uVar2;
  longlong lVar3;
  longlong lVar4;
  undefined4 *puVar7;
  int *piVar8;
  double dVar10;
  undefined1 local_50 [8];
  undefined1 local_48 [8];
  undefined4 local_40;
  uint local_3c;
  
  piVar5 = (int *)FUN_82803f30();
  Function_827EE118();
  if ((piVar5[0x21] != 0) && (piVar5[0x1b] != 0)) {
    iVar6 = FUN_824590d8(piVar5);
    if (-1 < iVar6) {
      cVar9 = Function_8265A0F0(DAT_82dd09e8);
      if (cVar9 == '\0') {
        *(undefined1 *)(piVar5 + 0x3c) = 0;
        if (*(char *)((int)piVar5 + 0xc9) != '\0') {
          uVar1 = FUN_824590d8(piVar5);
          uVar1 = FUN_826aa980(piVar5,uVar1);
          uVar2 = FUN_824590d8(piVar5);
          lVar3 = FUN_826aa990(piVar5,uVar2);
          lVar4 = FUN_82b565d8(uVar1);
          Function_8268BEE8(piVar5,lVar3 + lVar4);
          *(undefined1 *)((int)piVar5 + 0xc9) = 0;
        }
        uVar1 = FUN_826aa890(piVar5);
        if ((piVar5[0x25] <= (int)uVar1) && (piVar5[0x25] != 0)) {
          if (0 < piVar5[0x33]) {
            if ((DAT_82dd12a8 & 1) == 0) {
              DAT_82dd12a8 = DAT_82dd12a8 | 1;
              puVar7 = (undefined4 *)__0Symbol__QAA_PBD_Z(local_48,0xffffffff8201ca90);
              Function_82261BC0(0xffffffff82dd12a0,*puVar7);
              Function_828050F8(0xffffffff82c106c8);
            }
            (**(code **)(*(int *)((int)piVar5 + *(int *)(piVar5[1] + 4) + 4) + 0x18))
                      (&local_40,(int)piVar5 + *(int *)(piVar5[1] + 4) + 4,DAT_82dd12a4,1);
            if ((local_3c & 0x10) != 0) {
              Function_82260570(local_40);
            }
          }
          uVar2 = FUN_824590d8(piVar5);
          iVar6 = Function_826AAD78(piVar5,uVar2);
          piVar5[0x25] = iVar6 + piVar5[0x25];
          piVar5[0x33] = piVar5[0x33] + 1;
        }
        uVar2 = 0;
        piVar8 = (int *)FUN_827dd308(PTR_DAT_82c41b48);
        if (piVar8 == piVar5) {
          dVar10 = (double)FUN_826790b8(DAT_82dd09e8);
          if (dVar10 != (double)DAT_8200099c) {
            uVar2 = 1;
          }
        }
        else {
          uVar2 = 2;
        }
        Function_826D2098(piVar5[0x36],uVar1,uVar2);
        Function_8268BC08(piVar5);
        uVar2 = FUN_82263eb0(*(int *)(*(int *)(piVar5[0x23] + 4) + 8) + piVar5[0x23] + 4);
        if (piVar5[0x22] != (int)uVar2) {
          (**(code **)(*piVar5 + 0x58))(piVar5,uVar1,uVar2);
        }
      }
      else {
        iVar6 = FUN_82263eb0(*(int *)(*(int *)(piVar5[0x23] + 4) + 8) + piVar5[0x23] + 4);
        if ((piVar5[0x22] != iVar6) && (*(char *)(piVar5 + 0x3c) == '\0')) {
          if (piVar5[0x18] != piVar5[0x19]) {
            Function_8268C550(piVar5 + 0x18,piVar5[0x18],piVar5[0x19],local_50);
          }
          Function_82769680(piVar5[iVar6 + 0x1c],0,0,0x1e0,piVar5 + 0x18,1);
          Function_82B67F58(piVar5[0x21]);
          Function_82B699B0(piVar5[0x21]);
          Function_827B8DC0(*(undefined4 *)(piVar5[0x20] + 0x84));
          FUN_82667218(DAT_82dd0c98,*(undefined4 *)(piVar5[0x1b] + 0x278));
          iVar6 = FUN_826aa890(piVar5);
          thunk_FUN_827ad150((double)(longlong)iVar6);
          Function_8269FBC8(piVar5[0x1b]);
          *(undefined1 *)(piVar5 + 0x3c) = 1;
        }
      }
    }
  }
  return;
}


```

## Wii m2c decompilation (target Bank-8 asm → C, `m2c --target ppc`)

```c
typedef struct BandUser {
    /* 0x00 */ char pad0[0x7C];
    /* 0x7C */ s32 unk7C;                           /* inferred */
} BandUser;                                         /* size >= 0x80 */

typedef struct DataArray {
    /* 0x0 */ char pad0[0xA];
    /* 0xA */ s16 unkA;                             /* inferred */
} DataArray;                                        /* size >= 0xC */

typedef struct GemTrack {
    /* 0x00 */ char pad0[0x74];
    /* 0x74 */ TrackDir *unk74;                     /* inferred */
} GemTrack;                                         /* size >= 0x78 */

typedef struct GemTrainerPanel {
    /* 0x00 */ char pad0[4];
    /* 0x04 */ void *unk4;                          /* inferred */
    /* 0x08 */ char pad8[0x54];                     /* maybe part of unk4[0x16]? */
    /* 0x5C */ void *unk5C;                         /* inferred */
    /* 0x60 */ s32 unk60;                           /* inferred */
    /* 0x64 */ char pad64[0xC];                     /* maybe part of unk60[4]? */
    /* 0x70 */ GemTrack *unk70;                     /* inferred */
    /* 0x74 */ GemManager *unk74;                   /* inferred */
    /* 0x78 */ s32 unk78;                           /* inferred */
    /* 0x7C */ BandUser **unk7C;                    /* inferred */
    /* 0x80 */ char pad80[4];
    /* 0x84 */ s32 unk84;                           /* inferred */
    /* 0x88 */ stlpmtx_std::_Vector_impl<GameGem, short unsigned, stlpmtx_std::StlNodeAlloc<GameGem>> unk88; /* inferred */
    /* 0x88 */ char pad88[0x21];
    /* 0xA9 */ u8 unkA9;                            /* inferred */
    /* 0xAA */ char padAA[2];                       /* maybe part of unkA9[3]? */
    /* 0xAC */ s32 unkAC;                           /* inferred */
    /* 0xB0 */ char padB0[4];
    /* 0xB4 */ TrainerGemTab *unkB4;                /* inferred */
    /* 0xB8 */ Metronome *unkB8;                    /* inferred */
    /* 0xBC */ char padBC[0x14];                    /* maybe part of unkB8[6]? */
    /* 0xD0 */ u8 unkD0;                            /* inferred */
} GemTrainerPanel;                                  /* size >= 0xD1 */

typedef struct ObjectDir {
    /* 0x0 */ Hmx::Object *unk0;                    /* inferred */
} ObjectDir;                                        /* size >= 0x4 */

typedef struct stlpmtx_std::_Vector_impl<GameGem, short unsigned, stlpmtx_std::StlNodeAlloc<GameGem>> {
    /* 0x0 */ char pad0[8];
    /* 0x8 */ ? unk8;                               /* inferred */
    /* 0x8 */ char pad8[1];
} stlpmtx_std::_Vector_impl<GameGem, short unsigned, stlpmtx_std::StlNodeAlloc<GameGem>>; /* size >= 0x9 */

RndDir *@STRING@Find<6RndDir>__9ObjectDirFPCcb_P6RndDir(ObjectDir *this, s8 *arg0, s32 arg1); /* extern */
? ClearAllGemWidgets__8TrackDirFv(TrackDir *this);  /* extern */
? ClearAllGems__10GemManagerFv(GemManager *this);   /* extern */
? ClearMissedPhrases__10GemManagerFv(GemManager *this); /* extern */
? Fail__5DebugFPCc(Debug *this, s8 *arg0);          /* extern */
? FindObject__9ObjectDirFPCcb(ObjectDir *this, s8 *arg0, s32 arg1); /* extern */
u32 FocusPanel__9UIManagerFv(UIManager *this);      /* extern */
s32 GetCurrSection__12TrainerPanelCFv(TrainerPanel *this); /* extern */
s32 GetDifficulty__8BandUserCFv(BandUser *this);    /* extern */
s32 GetGemListByDiff__6SongDBCFii(SongDB *this, s32 arg0, s32 arg1); /* extern */
GemManager *GetGemManager__8GemTrackFv(GemTrack *this); /* extern */
s32 GetLoopTicks__12TrainerPanelCFi(TrainerPanel *this, s32 arg0); /* extern */
f32 GetMusicSpeed__4GameCFv(Game *this);            /* extern */
s32 GetSectionTicks__12TrainerPanelCFi(TrainerPanel *this, s32 arg0); /* extern */
TrainerSection *GetSection__12TrainerPanelFi(TrainerPanel *this, s32 arg0); /* extern */
s32 GetStartTick__14TrainerSectionCFv(TrainerSection *this); /* extern */
s32 GetTick__12TrainerPanelCFv(TrainerPanel *this); /* extern */
s32 GetType__5TrackCFv(Track *this);                /* extern */
? Init__13TrainerGemTabFP6RndDir9TrackType(TrainerGemTab *this, RndDir *arg0, TrackType arg1); /* extern */
s32 IsWaiting__4GameFv(Game *this);                 /* extern */
s8 *MakeString<PCc,PCc>__FPCcPCcPCc_PCc(s8 *arg0, s8 *arg1, s8 *arg2); /* extern */
s8 *MakeString<PCc,i,PCc>__FPCcPCciPCc_PCc(s8 *arg0, s8 *arg1, s32 arg2, s8 *arg3); /* extern */
s8 *PathName__FPCQ23Hmx6Object(Hmx::Object *arg0);  /* extern */
? Poll__7UIPanelFv(UIPanel *this);                  /* extern */
? Poll__9MetronomeFiQ29Metronome15OverrideEnabled(Metronome *this, s32 arg0, Metronome::OverrideEnabled arg1); /* extern */
s32 SymToTrackType__F6Symbol(Symbol arg0);          /* extern */
void *__dt__9DataArrayFv(DataArray *this, s16 destroyFlag); /* extern */
GemTrack *__dynamic_cast(?, struct RTTI *, struct RTTI *, ?); /* extern */
? AddBeatMask__15GemTrainerPanelFi(GemTrainerPanel *this, s32 arg0); /* static */
? HandleTrackShifting__15GemTrainerPanelFv(GemTrainerPanel *this); /* static */
void Poll__15GemTrainerPanelFv(GemTrainerPanel *this); /* static */
? __as__Q211stlpmtx_std65_Vector_impl<7GameGem,Us,Q211stlpmtx_std22StlNodeAlloc<7GameGem>>FRCQ211stlpmtx_std65_Vector_impl<7GameGem,Us,Q211stlpmtx_std22StlNodeAlloc<7GameGem>>(stlpmtx_std::_Vector_impl<GameGem, short unsigned, stlpmtx_std::StlNodeAlloc<GameGem>> *this, stlpmtx_std::_Vector_impl<GameGem, short unsigned, stlpmtx_std::StlNodeAlloc<GameGem>> *arg0); /* static */
extern f32 @F_0000803f;
extern Debug TheDebug;
extern Game *TheGame;
extern SongDB *TheSongDB;
extern UIManager *TheUI;
extern struct RTTI __RTTI__5Track;
extern struct RTTI __RTTI__6RndDir;
extern struct RTTI __RTTI__8GemTrack;
extern struct RTTI __RTTI__Q23Hmx6Object;
extern s8 *kAssertStr;
extern s8 *kNotObjectMsg;
extern ? loop_msg;
static ? @stringBase0;                              /* unable to generate initializer: unknown type */

/* GemTrainerPanel::Poll (void) */
void Poll__15GemTrainerPanelFv(GemTrainerPanel *this, ? arg_sp0) {
    DataArray *sp10;
    s32 sp8;
    ? *temp_r4;
    ? *temp_r6;
    ? *temp_r6_2;
    ? var_r27_2;
    GemManager *temp_r3_2;
    GemTrack *temp_r3;
    GemTrack *temp_r3_5;
    GemTrainerPanel *var_r28;
    ObjectDir *temp_r3_4;
    ObjectDir *var_r28_2;
    ObjectDir *var_r3;
    TrainerSection *temp_r27;
    s16 temp_r0_2;
    s32 temp_r0;
    s32 temp_r31;
    s32 temp_r3_3;
    s32 temp_r3_7;
    s32 temp_r3_8;
    s32 temp_r3_9;
    s32 var_r27;
    s32 var_r4;
    s8 *temp_r29;
    s8 *var_r5;
    stlpmtx_std::_Vector_impl<GameGem, short unsigned, stlpmtx_std::StlNodeAlloc<GameGem>> *var_r31;
    void *temp_r3_6;

    if (((GemTrack *) this->unk70 == NULL) && ((void *) this->unk5C != NULL) && ((s32) (*this->unk7C)->unk7C != 0)) {
        temp_r3 = __dynamic_cast(0, &__RTTI__8GemTrack, &__RTTI__5Track, 0);
        this->unk70 = temp_r3;
        if (temp_r3 == NULL) {
            temp_r6 = "reset_score\0metronome_hi.cue\0metronome_lo.cue\0m\0\0set_key\0trainers/song_name\0GemTrainerPanel.cpp\0mTrack != NULL\0mGemManager != NULL\0gem_preview\0range == 10.0f\0update_thermometer\0i < mGemPlayer->GetGemStatus()->GetSize()\0!mPattern.empty()\0trainers/speed\0beat_mask.wid\0trainers/metronome\0song_lessons\0%s(%d): %s unhandled msg: %s\0vector";
            Fail__5DebugFPCc(&TheDebug, MakeString<PCc,i,PCc>__FPCcPCciPCc_PCc(kAssertStr, temp_r6 + 0x4C, 0xC1, temp_r6 + 0x60));
        }
        temp_r3_2 = GetGemManager__8GemTrackFv(this->unk70);
        this->unk74 = temp_r3_2;
        if (temp_r3_2 == NULL) {
            temp_r6_2 = "reset_score\0metronome_hi.cue\0metronome_lo.cue\0m\0\0set_key\0trainers/song_name\0GemTrainerPanel.cpp\0mTrack != NULL\0mGemManager != NULL\0gem_preview\0range == 10.0f\0update_thermometer\0i < mGemPlayer->GetGemStatus()->GetSize()\0!mPattern.empty()\0trainers/speed\0beat_mask.wid\0trainers/metronome\0song_lessons\0%s(%d): %s unhandled msg: %s\0vector";
            Fail__5DebugFPCc(&TheDebug, MakeString<PCc,i,PCc>__FPCcPCciPCc_PCc(kAssertStr, temp_r6_2 + 0x4C, 0xC5, temp_r6_2 + 0x6F));
        }
        var_r28 = this;
        var_r31 = &this->unk88;
        var_r27 = 0;
        do {
            temp_r3_3 = GetGemListByDiff__6SongDBCFii(TheSongDB, this->unk5C->unk248, var_r27);
            var_r28->unk60 = temp_r3_3;
            __as__Q211stlpmtx_std65_Vector_impl<7GameGem,Us,Q211stlpmtx_std22StlNodeAlloc<7GameGem>>FRCQ211stlpmtx_std65_Vector_impl<7GameGem,Us,Q211stlpmtx_std22StlNodeAlloc<7GameGem>>(var_r31, temp_r3_3 + 4);
            var_r27 += 1;
            var_r31 += 8;
            var_r28 += 4;
        } while (var_r27 < 4);
        temp_r4 = "reset_score\0metronome_hi.cue\0metronome_lo.cue\0m\0\0set_key\0trainers/song_name\0GemTrainerPanel.cpp\0mTrack != NULL\0mGemManager != NULL\0gem_preview\0range == 10.0f\0update_thermometer\0i < mGemPlayer->GetGemStatus()->GetSize()\0!mPattern.empty()\0trainers/speed\0beat_mask.wid\0trainers/metronome\0song_lessons\0%s(%d): %s unhandled msg: %s\0vector";
        temp_r29 = temp_r4 + 0x83;
        temp_r3_4 = this->unk4->unk20(this, temp_r4);
        var_r28_2 = temp_r3_4;
        FindObject__9ObjectDirFPCcb(temp_r3_4, temp_r29, 0);
        temp_r3_5 = __dynamic_cast(0, &__RTTI__6RndDir, &__RTTI__Q23Hmx6Object, 0);
        if (temp_r3_5 == NULL) {
            var_r3 = var_r28_2;
            if (var_r28_2 != NULL) {
                var_r3 = (ObjectDir *) var_r28_2->unk0;
            }
            if (PathName__FPCQ23Hmx6Object((Hmx::Object *) var_r3) != NULL) {
                if (var_r28_2 != NULL) {
                    var_r28_2 = (ObjectDir *) var_r28_2->unk0;
                }
                var_r5 = PathName__FPCQ23Hmx6Object((Hmx::Object *) var_r28_2);
            } else {
                var_r5 = (s8 *) @STRING@Find<6RndDir>__9ObjectDirFPCcb_P6RndDir;
            }
            Fail__5DebugFPCc(&TheDebug, MakeString<PCc,PCc>__FPCcPCcPCc_PCc(kNotObjectMsg, temp_r29, var_r5));
        }
        sp8 = GetType__5TrackCFv((Track *) this->unk70);
        Init__13TrainerGemTabFP6RndDir9TrackType(this->unkB4, (RndDir *) temp_r3_5, (TrackType) SymToTrackType__F6Symbol((Symbol) &sp8));
    }
    Poll__7UIPanelFv((UIPanel *) this);
    if (((GemManager *) this->unk74 != NULL) && ((void *) this->unk5C != NULL)) {
        if (GetCurrSection__12TrainerPanelCFv((TrainerPanel *) this) < 0) {
            return;
        }
        if (IsWaiting__4GameFv(TheGame) != 0) {
            if (((s32) this->unk78 != GetDifficulty__8BandUserCFv(*this->unk7C)) && ((s32) this->unkD0 == 0)) {
                var_r4 = 0;
loop_31:
                temp_r3_6 = this->unk5C->unk2D0;
                if (var_r4 < (s32) temp_r3_6->unk4) {
                    if (var_r4 != -1) {
                        temp_r3_7 = temp_r3_6->unk0;
                        *(temp_r3_7 + var_r4) = *(temp_r3_7 + var_r4) | 0x40;
                    }
                    var_r4 += 1;
                    goto loop_31;
                }
                this->unkD0 = 1;
            }
        } else {
            this->unkD0 = 0;
            if ((s32) this->unkA9 != 0) {
                temp_r27 = GetSection__12TrainerPanelFi((TrainerPanel *) this, GetCurrSection__12TrainerPanelCFv((TrainerPanel *) this));
                temp_r31 = GetSectionTicks__12TrainerPanelCFi((TrainerPanel *) this, GetCurrSection__12TrainerPanelCFv((TrainerPanel *) this));
                AddBeatMask__15GemTrainerPanelFi(this, GetStartTick__14TrainerSectionCFv(temp_r27) + temp_r31);
                this->unkA9 = 0;
            }
            temp_r3_8 = GetTick__12TrainerPanelCFv((TrainerPanel *) this);
            temp_r0 = this->unk84;
            if ((temp_r3_8 >= temp_r0) && (temp_r0 != 0)) {
                if ((s32) this->unkAC > 0) {
                    this->unk4->unk10(&sp10, this, loop_msg.unk4, 1);
                    if (sp14 & 0x10) {
                        temp_r0_2 = sp10->unkA - 1;
                        sp10->unkA = temp_r0_2;
                        if (temp_r0_2 == 0) {
                            __dt__9DataArrayFv(sp10, 1);
                        }
                    }
                }
                this->unk84 += GetLoopTicks__12TrainerPanelCFi((TrainerPanel *) this, GetCurrSection__12TrainerPanelCFv((TrainerPanel *) this));
                this->unkAC += 1;
            }
            var_r27_2 = 0;
            if (FocusPanel__9UIManagerFv(TheUI) != this) {
... [truncated 16 of 236 m2c lines]
```

## Wii target asm (Bank 8, ground-truth body)

```asm
# GemTrainerPanel::Poll()
.fn Poll__15GemTrainerPanelFv, global
/* 801A2AB0 001974D0  94 21 FF D0 */	stwu r1, -0x30(r1)
/* 801A2AB4 001974D4  7C 08 02 A6 */	mflr r0
/* 801A2AB8 001974D8  90 01 00 34 */	stw r0, 0x34(r1)
/* 801A2ABC 001974DC  39 61 00 30 */	addi r11, r1, 0x30
/* 801A2AC0 001974E0  48 89 68 71 */	bl _savegpr_27
/* 801A2AC4 001974E4  80 03 00 70 */	lwz r0, 0x70(r3)
/* 801A2AC8 001974E8  7C 7E 1B 78 */	mr r30, r3
/* 801A2ACC 001974EC  2C 00 00 00 */	cmpwi r0, 0x0
/* 801A2AD0 001974F0  40 82 01 E8 */	bne .L_801A2CB8
/* 801A2AD4 001974F4  80 03 00 5C */	lwz r0, 0x5c(r3)
/* 801A2AD8 001974F8  2C 00 00 00 */	cmpwi r0, 0x0
/* 801A2ADC 001974FC  41 82 01 DC */	beq .L_801A2CB8
/* 801A2AE0 00197500  80 63 00 7C */	lwz r3, 0x7c(r3)
/* 801A2AE4 00197504  80 63 00 00 */	lwz r3, 0x0(r3)
/* 801A2AE8 00197508  80 63 00 7C */	lwz r3, 0x7c(r3)
/* 801A2AEC 0019750C  2C 03 00 00 */	cmpwi r3, 0x0
/* 801A2AF0 00197510  41 82 01 C8 */	beq .L_801A2CB8
/* 801A2AF4 00197514  3C A0 80 B7 */	lis r5, __RTTI__8GemTrack@ha
/* 801A2AF8 00197518  3C C0 80 B7 */	lis r6, __RTTI__5Track@ha
/* 801A2AFC 0019751C  38 80 00 00 */	li r4, 0x0
/* 801A2B00 00197520  38 E0 00 00 */	li r7, 0x0
/* 801A2B04 00197524  38 A5 6A D0 */	addi r5, r5, __RTTI__8GemTrack@l
/* 801A2B08 00197528  38 C6 6C 28 */	addi r6, r6, __RTTI__5Track@l
/* 801A2B0C 0019752C  48 89 64 49 */	bl __dynamic_cast
/* 801A2B10 00197530  2C 03 00 00 */	cmpwi r3, 0x0
/* 801A2B14 00197534  90 7E 00 70 */	stw r3, 0x70(r30)
/* 801A2B18 00197538  40 82 00 34 */	bne .L_801A2B4C
/* 801A2B1C 0019753C  3C 60 80 BB */	lis r3, kAssertStr@ha
/* 801A2B20 00197540  3C C0 80 B8 */	lis r6, "@stringBase0"@ha
/* 801A2B24 00197544  38 C6 EB B8 */	addi r6, r6, "@stringBase0"@l
/* 801A2B28 00197548  80 63 28 58 */	lwz r3, kAssertStr@l(r3)
/* 801A2B2C 0019754C  38 86 00 4C */	addi r4, r6, 0x4c
/* 801A2B30 00197550  38 A0 00 C1 */	li r5, 0xc1
/* 801A2B34 00197554  38 C6 00 60 */	addi r6, r6, 0x60
/* 801A2B38 00197558  4B E6 E5 09 */	bl "MakeString<PCc,i,PCc>__FPCcPCciPCc_PCc"
/* 801A2B3C 0019755C  3C A0 80 C9 */	lis r5, TheDebug@ha
/* 801A2B40 00197560  7C 64 1B 78 */	mr r4, r3
/* 801A2B44 00197564  38 65 73 D8 */	addi r3, r5, TheDebug@l
/* 801A2B48 00197568  48 27 BE 79 */	bl Fail__5DebugFPCc
.L_801A2B4C:
/* 801A2B4C 0019756C  80 7E 00 70 */	lwz r3, 0x70(r30)
/* 801A2B50 00197570  4B F9 D1 11 */	bl GetGemManager__8GemTrackFv
/* 801A2B54 00197574  2C 03 00 00 */	cmpwi r3, 0x0
/* 801A2B58 00197578  90 7E 00 74 */	stw r3, 0x74(r30)
/* 801A2B5C 0019757C  40 82 00 34 */	bne .L_801A2B90
/* 801A2B60 00197580  3C 60 80 BB */	lis r3, kAssertStr@ha
/* 801A2B64 00197584  3C C0 80 B8 */	lis r6, "@stringBase0"@ha
/* 801A2B68 00197588  38 C6 EB B8 */	addi r6, r6, "@stringBase0"@l
/* 801A2B6C 0019758C  80 63 28 58 */	lwz r3, kAssertStr@l(r3)
/* 801A2B70 00197590  38 86 00 4C */	addi r4, r6, 0x4c
/* 801A2B74 00197594  38 A0 00 C5 */	li r5, 0xc5
/* 801A2B78 00197598  38 C6 00 6F */	addi r6, r6, 0x6f
/* 801A2B7C 0019759C  4B E6 E4 C5 */	bl "MakeString<PCc,i,PCc>__FPCcPCciPCc_PCc"
/* 801A2B80 001975A0  3C A0 80 C9 */	lis r5, TheDebug@ha
/* 801A2B84 001975A4  7C 64 1B 78 */	mr r4, r3
/* 801A2B88 001975A8  38 65 73 D8 */	addi r3, r5, TheDebug@l
/* 801A2B8C 001975AC  48 27 BE 35 */	bl Fail__5DebugFPCc
.L_801A2B90:
/* 801A2B90 001975B0  7F DC F3 78 */	mr r28, r30
/* 801A2B94 001975B4  3B FE 00 88 */	addi r31, r30, 0x88
/* 801A2B98 001975B8  3B 60 00 00 */	li r27, 0x0
/* 801A2B9C 001975BC  3F A0 80 C9 */	lis r29, TheSongDB@ha
.L_801A2BA0:
/* 801A2BA0 001975C0  80 9E 00 5C */	lwz r4, 0x5c(r30)
/* 801A2BA4 001975C4  7F 65 DB 78 */	mr r5, r27
/* 801A2BA8 001975C8  80 7D F0 48 */	lwz r3, TheSongDB@l(r29)
/* 801A2BAC 001975CC  80 84 02 48 */	lwz r4, 0x248(r4)
/* 801A2BB0 001975D0  48 03 93 E1 */	bl GetGemListByDiff__6SongDBCFii
/* 801A2BB4 001975D4  7C 64 1B 78 */	mr r4, r3
/* 801A2BB8 001975D8  90 7C 00 60 */	stw r3, 0x60(r28)
/* 801A2BBC 001975DC  7F E3 FB 78 */	mr r3, r31
/* 801A2BC0 001975E0  38 84 00 04 */	addi r4, r4, 0x4
/* 801A2BC4 001975E4  48 00 3B 2D */	bl "__as__Q211stlpmtx_std65_Vector_impl<7GameGem,Us,Q211stlpmtx_std22StlNodeAlloc<7GameGem>>FRCQ211stlpmtx_std65_Vector_impl<7GameGem,Us,Q211stlpmtx_std22StlNodeAlloc<7GameGem>>"
/* 801A2BC8 001975E8  3B 7B 00 01 */	addi r27, r27, 0x1
/* 801A2BCC 001975EC  3B FF 00 08 */	addi r31, r31, 0x8
/* 801A2BD0 001975F0  2C 1B 00 04 */	cmpwi r27, 0x4
/* 801A2BD4 001975F4  3B 9C 00 04 */	addi r28, r28, 0x4
/* 801A2BD8 001975F8  41 80 FF C8 */	blt .L_801A2BA0
/* 801A2BDC 001975FC  81 9E 00 04 */	lwz r12, 0x4(r30)
/* 801A2BE0 00197600  3C 80 80 B8 */	lis r4, "@stringBase0"@ha
/* 801A2BE4 00197604  38 84 EB B8 */	addi r4, r4, "@stringBase0"@l
/* 801A2BE8 00197608  7F C3 F3 78 */	mr r3, r30
/* 801A2BEC 0019760C  81 8C 00 20 */	lwz r12, 0x20(r12)
/* 801A2BF0 00197610  3B A4 00 83 */	addi r29, r4, 0x83
/* 801A2BF4 00197614  7D 89 03 A6 */	mtctr r12
/* 801A2BF8 00197618  4E 80 04 21 */	bctrl
/* 801A2BFC 0019761C  7C 7C 1B 78 */	mr r28, r3
/* 801A2C00 00197620  7F A4 EB 78 */	mr r4, r29
/* 801A2C04 00197624  38 A0 00 00 */	li r5, 0x0
/* 801A2C08 00197628  48 2C 32 79 */	bl FindObject__9ObjectDirFPCcb
/* 801A2C0C 0019762C  3C A0 80 B5 */	lis r5, __RTTI__6RndDir@ha
/* 801A2C10 00197630  3C C0 80 B5 */	lis r6, __RTTI__Q23Hmx6Object@ha
/* 801A2C14 00197634  38 A5 AE A0 */	addi r5, r5, __RTTI__6RndDir@l
/* 801A2C18 00197638  38 80 00 00 */	li r4, 0x0
/* 801A2C1C 0019763C  38 C6 A2 78 */	addi r6, r6, __RTTI__Q23Hmx6Object@l
/* 801A2C20 00197640  38 E0 00 00 */	li r7, 0x0
/* 801A2C24 00197644  48 89 63 31 */	bl __dynamic_cast
/* 801A2C28 00197648  2C 03 00 00 */	cmpwi r3, 0x0
/* 801A2C2C 0019764C  7C 7F 1B 78 */	mr r31, r3
/* 801A2C30 00197650  40 82 00 64 */	bne .L_801A2C94
/* 801A2C34 00197654  2C 1C 00 00 */	cmpwi r28, 0x0
/* 801A2C38 00197658  7F 83 E3 78 */	mr r3, r28
/* 801A2C3C 0019765C  41 82 00 08 */	beq .L_801A2C44
/* 801A2C40 00197660  80 7C 00 00 */	lwz r3, 0x0(r28)
.L_801A2C44:
/* 801A2C44 00197664  48 2E 0F 6D */	bl PathName__FPCQ23Hmx6Object
/* 801A2C48 00197668  2C 03 00 00 */	cmpwi r3, 0x0
/* 801A2C4C 0019766C  41 82 00 20 */	beq .L_801A2C6C
/* 801A2C50 00197670  2C 1C 00 00 */	cmpwi r28, 0x0
/* 801A2C54 00197674  41 82 00 08 */	beq .L_801A2C5C
/* 801A2C58 00197678  83 9C 00 00 */	lwz r28, 0x0(r28)
.L_801A2C5C:
/* 801A2C5C 0019767C  7F 83 E3 78 */	mr r3, r28
/* 801A2C60 00197680  48 2E 0F 51 */	bl PathName__FPCQ23Hmx6Object
/* 801A2C64 00197684  7C 65 1B 78 */	mr r5, r3
/* 801A2C68 00197688  48 00 00 0C */	b .L_801A2C74
.L_801A2C6C:
/* 801A2C6C 0019768C  3C A0 80 B7 */	lis r5, "@STRING@Find<6RndDir>__9ObjectDirFPCcb_P6RndDir"@ha
/* 801A2C70 00197690  38 A5 5F 2C */	addi r5, r5, "@STRING@Find<6RndDir>__9ObjectDirFPCcb_P6RndDir"@l
.L_801A2C74:
/* 801A2C74 00197694  3C 60 80 BC */	lis r3, kNotObjectMsg@ha
/* 801A2C78 00197698  7F A4 EB 78 */	mr r4, r29
/* 801A2C7C 0019769C  80 63 8A 44 */	lwz r3, kNotObjectMsg@l(r3)
/* 801A2C80 001976A0  4B E7 08 41 */	bl "MakeString<PCc,PCc>__FPCcPCcPCc_PCc"
/* 801A2C84 001976A4  3C A0 80 C9 */	lis r5, TheDebug@ha
/* 801A2C88 001976A8  7C 64 1B 78 */	mr r4, r3
/* 801A2C8C 001976AC  38 65 73 D8 */	addi r3, r5, TheDebug@l
/* 801A2C90 001976B0  48 27 BD 31 */	bl Fail__5DebugFPCc
.L_801A2C94:
/* 801A2C94 001976B4  80 7E 00 70 */	lwz r3, 0x70(r30)
/* 801A2C98 001976B8  4B FA 3E 59 */	bl GetType__5TrackCFv
/* 801A2C9C 001976BC  90 61 00 08 */	stw r3, 0x8(r1)
/* 801A2CA0 001976C0  38 61 00 08 */	addi r3, r1, 0x8
/* 801A2CA4 001976C4  48 4C 12 BD */	bl SymToTrackType__F6Symbol
/* 801A2CA8 001976C8  7C 65 1B 78 */	mr r5, r3
/* 801A2CAC 001976CC  80 7E 00 B4 */	lwz r3, 0xb4(r30)
/* 801A2CB0 001976D0  7F E4 FB 78 */	mr r4, r31
/* 801A2CB4 001976D4  48 05 5F 9D */	bl Init__13TrainerGemTabFP6RndDir9TrackType
.L_801A2CB8:
/* 801A2CB8 001976D8  7F C3 F3 78 */	mr r3, r30
/* 801A2CBC 001976DC  48 64 A1 25 */	bl Poll__7UIPanelFv
/* 801A2CC0 001976E0  80 1E 00 74 */	lwz r0, 0x74(r30)
/* 801A2CC4 001976E4  2C 00 00 00 */	cmpwi r0, 0x0
/* 801A2CC8 001976E8  41 82 02 5C */	beq .L_801A2F24
/* 801A2CCC 001976EC  80 1E 00 5C */	lwz r0, 0x5c(r30)
/* 801A2CD0 001976F0  2C 00 00 00 */	cmpwi r0, 0x0
/* 801A2CD4 001976F4  41 82 02 50 */	beq .L_801A2F24
/* 801A2CD8 001976F8  7F C3 F3 78 */	mr r3, r30
... [truncated 164 of 314 asm lines]
```
