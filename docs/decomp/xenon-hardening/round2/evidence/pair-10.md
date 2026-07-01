# Pair 10 — verification evidence

**Claimed identity:** Wii `UpdateVocalStyle__10VocalTrackFv`  ==  Xenon `0x82b743e8`

| field | value |
|---|---|
| pair_id | 10 |
| stratum | BSIM 20-30 |
| match_type | `BSIM` |
| BSim sim×conf | 22.293 |
| BSim similarity / confidence | 0.414 / 53.847 |
| TU (Wii) | `VocalTrack.o` |
| Wii symbol (demangled) | `VocalTrack::UpdateVocalStyle(...)` |
| Wii addr (Bank 8) | `0x801529a0` |
| Xenon addr | `0x82b743e8` |
| Xenon func name | `Function_82B743E8` (stripped binary — name is auto-generated) |
| Wii body size | 180 asm lines (lines 5379-5557 in `build/SZBE69_B8/asm/band3/bandtrack/VocalTrack.s`) |
| Xenon body size | 512 bytes |

## How to read this

The Wii symbol is **ground truth** (from the CodeWarrior linker map). The Xenon
function is from a **stripped** XEX — its name `Function_<addr>` is meaningless.
The claim is that these two functions are the **same source function** compiled by
two different toolchains (Wii MWCC/Gekko PPC vs Xbox360 MSVC/Xenon PPC). Judge
whether the *control-flow shape, constant pool, field offsets, and especially the
resolved callees* are consistent with that claim. The 'resolved callee' column
below maps each Xenon callee to its matched Wii symbol where a match exists —
**agreeing callee names are the strongest cross-compiler signal.**

## Xenon callees (10 total, 8 resolved to a matched Wii symbol)

| xenon callee addr | xenon name | resolved Wii symbol (via matches.json) |
|---|---|---|
| `0x82563f98` | `FUN_82563f98` | `Current__13MetaPerformerFv` |
| `0x82564f80` | `Function_82564F80` | `OSSetCurrentContext` |
| `0x82b727b8` | `Function_82B727B8` | `RebuildHUD__10VocalTrackFv` |
| `0x82803f28` | `FUN_82803f28` | `__nw__FUl` |
| `0x82659f08` | `FUN_82659f08` | `GetActivePlayers__4GameFv` |
| `0x82263eb0` | `FUN_82263eb0` | `TrackNum__11TrackConfigCFv` |
| `0x82804da8` | `Function_82804DA8` | _(unmatched / Function_)_ |
| `0x822e9450` | `Function_822E9450` | `UpdateConfiguration__13VocalTrackDirFv` |
| `0x825864e8` | `FUN_825864e8` | `ScrollSpeed__16BandSongMetadataCFv` |
| `0x822a7fa0` | `Function_822A7FA0` | _(unmatched / Function_)_ |

## Referenced strings (Xenon side, 1)

- `'tambourine_preview.anim'`

## Xenon pseudo-C (Ghidra, fork 12.2, PowerPC:BE:64:Xenon)

```c

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void Function_82B743E8(void)

{
  int iVar1;
  int iVar3;
  int *piVar4;
  char cVar7;
  int iVar5;
  int *piVar6;
  undefined8 uVar2;
  uint uVar8;
  int iVar9;
  double dVar10;
  undefined4 local_50 [20];
  
  iVar3 = FUN_82803f28();
  piVar4 = (int *)FUN_82659f08(DAT_82dd09e8);
  if (*(int **)(iVar3 + 0xa4) != (int *)0x0) {
    cVar7 = (**(code **)(**(int **)(iVar3 + 0xa4) + 0x1c))();
    if (cVar7 == '\0') {
      iVar1 = *piVar4;
      uVar8 = 0;
      if (piVar4[1] - iVar1 >> 2 != 0) {
        iVar9 = 0;
        do {
          piVar6 = *(int **)(iVar9 + iVar1);
          if (((piVar6 != (int *)0x0) && (piVar6[0x9f] == 3)) &&
             (iVar1 = piVar6[0x9e], iVar5 = FUN_82263eb0(iVar3 + 0x28), iVar1 != iVar5)) {
            cVar7 = (**(code **)(*piVar6 + 0x1c))(piVar6);
            if (cVar7 != '\0') {
              piVar6 = (int *)Function_82804DA8(*(undefined4 *)(piVar6[0x98] + 0x88),0,
                                                0xffffffff82c44eb4,0xffffffff82c44fac,0);
              if (piVar6 != (int *)0x0) {
                (**(code **)(*piVar6 + 0x180))(piVar6,*(undefined4 *)(iVar3 + 0x7c));
              }
            }
          }
          iVar1 = *piVar4;
          uVar8 = uVar8 + 1;
          iVar9 = iVar9 + 4;
        } while (uVar8 < (uint)(piVar4[1] - iVar1 >> 2));
      }
    }
  }
  if ((*(int *)(iVar3 + 0x98) != 0) &&
     ((*(int *)(iVar3 + 0xa4) == 0 ||
      ((iVar1 = *(int *)(*(int *)(iVar3 + 0xa4) + 0x280), iVar1 != 1 && (iVar1 != 4)))))) {
    Function_822E9450();
    *(float *)(iVar3 + 0x88) =
         *(float *)(*(int *)(iVar3 + 0x98) + 0x468) - *(float *)(*(int *)(iVar3 + 0x98) + 0x464);
    uVar2 = FUN_82563f98();
    Function_82564F80(local_50,uVar2);
    uVar2 = (**(code **)(*(int *)PTR_DAT_82c424e0 + 0x5c))(PTR_DAT_82c424e0,local_50[0],1);
    (**(code **)(*(int *)PTR_DAT_82c424e0 + 0x40))(PTR_DAT_82c424e0,uVar2);
    dVar10 = (double)FUN_825864e8();
    *(float *)(iVar3 + 0x84) = (float)((double)*(float *)(iVar3 + 0x88) * dVar10) * _DAT_82199358;
    piVar4 = (int *)Function_822A7FA0(*(undefined4 *)(iVar3 + 0x98),0xffffffff82198f50,1);
    (**(code **)(*piVar4 + 0xc))((double)DAT_82000d6c,(double)DAT_8200099c);
    Function_82B727B8(iVar3);
  }
  return;
}


```

## Wii m2c decompilation (target Bank-8 asm → C, `m2c --target ppc`)

```c
typedef struct VocalTrack {
    /* 0x00 */ char pad0[0x1C];
    /* 0x1C */ TrackConfig unk1C;                   /* inferred */
    /* 0x1C */ char pad1C[0x50];
    /* 0x6C */ s32 unk6C;                           /* inferred */
    /* 0x70 */ char pad70[4];
    /* 0x74 */ f32 unk74;                           /* inferred */
    /* 0x78 */ f32 unk78;                           /* inferred */
    /* 0x7C */ char pad7C[0xC];                     /* maybe part of unk78[4]? */
    /* 0x88 */ VocalTrackDir *unk88;                /* inferred */
    /* 0x8C */ char pad8C[8];                       /* maybe part of unk88[3]? */
    /* 0x94 */ void *unk94;                         /* inferred */
} VocalTrack;                                       /* size >= 0x98 */

typedef struct VocalTrackDir {
    /* 0x000 */ Hmx::Object *unk0;                  /* inferred */
    /* 0x004 */ char pad4[0x40C];                   /* maybe part of unk0[0x104]? */
    /* 0x410 */ f32 unk410;                         /* inferred */
    /* 0x414 */ f32 unk414;                         /* inferred */
} VocalTrackDir;                                    /* size >= 0x418 */

RndAnimatable *@STRING@Find<13RndAnimatable>__9ObjectDirFPCcb_P13RndAnimatable(ObjectDir *this, s8 *arg0, s32 arg1); /* extern */
MetaPerformer *Current__13MetaPerformerFv(MetaPerformer *this); /* extern */
? Fail__5DebugFPCc(Debug *this, s8 *arg0);          /* extern */
? FindObject__9ObjectDirFPCcb(ObjectDir *this, s8 *arg0, s32 arg1); /* extern */
void *GetActivePlayers__4GameFv(Game *this);        /* extern */
s8 *MakeString<PCc,PCc>__FPCcPCcPCc_PCc(s8 *arg0, s8 *arg1, s8 *arg2); /* extern */
s8 *PathName__FPCQ23Hmx6Object(Hmx::Object *arg0);  /* extern */
f64 ScrollSpeed__16BandSongMetadataCFv(BandSongMetadata *this); /* extern */
s32 Song__13MetaPerformerCFv(MetaPerformer *this);  /* extern */
s32 TrackNum__11TrackConfigCFv(TrackConfig *this);  /* extern */
? UpdateConfiguration__13VocalTrackDirFv(VocalTrackDir *this); /* extern */
void **__dynamic_cast(s32, struct RTTI *, struct RTTI *, struct RTTI *, ?); /* extern */
? RebuildHUD__10VocalTrackFv(VocalTrack *this);     /* static */
extern f32 @F_00000000;
extern f32 @F_0000803f;
extern Debug TheDebug;
extern Game *TheGame;
extern void *TheSongMgr;
extern struct RTTI __RTTI__13RndAnimatable;
extern struct RTTI __RTTI__5Track;
extern struct RTTI __RTTI__Q23Hmx6Object;
extern s8 *kNotObjectMsg;
static struct RTTI __RTTI__10VocalTrack;            /* unable to generate initializer: cannot parse @60983 as integer */
static ? @stringBase0;                              /* unable to generate initializer: unknown type */

/* VocalTrack::UpdateVocalStyle (void) */
void UpdateVocalStyle__10VocalTrackFv(VocalTrack *this, ? arg_sp0) {
    s32 sp8;
    VocalTrackDir *temp_r3_2;
    VocalTrackDir *temp_r3_3;
    VocalTrackDir *var_r26;
    VocalTrackDir *var_r3;
    s32 temp_r0_2;
    s32 temp_r26;
    s32 var_r30;
    s8 *temp_r27_2;
    s8 *var_r5;
    u32 var_r25;
    void **temp_r3;
    void **temp_r3_4;
    void *temp_r0;
    void *temp_r24;
    void *temp_r27;
    void *temp_r4;

    temp_r0 = this->unk94;
    temp_r27 = GetActivePlayers__4GameFv(TheGame);
    if ((temp_r0 != NULL) && (temp_r0->unk4->unk2C(temp_r0) == 0)) {
        var_r25 = 0U;
        var_r30 = 0;
loop_10:
        if (var_r25 < (u16) temp_r27->unk4) {
            temp_r24 = *(temp_r27->unk0 + var_r30);
            if ((temp_r24 != NULL) && ((s32) temp_r24->unk24C == 3)) {
                temp_r26 = temp_r24->unk248;
                if ((temp_r26 != TrackNum__11TrackConfigCFv(&this->unk1C)) && (temp_r24->unk4->unk2C(temp_r24) != 0)) {
                    temp_r3 = __dynamic_cast(temp_r24->unk230->unk7C, NULL, &__RTTI__10VocalTrack, &__RTTI__5Track, 0);
                    if (temp_r3 != NULL) {
                        (*temp_r3)->unk188(this->unk6C);
                    }
                }
            }
            var_r25 += 1;
            var_r30 += 4;
            goto loop_10;
        }
    }
    temp_r3_2 = this->unk88;
    if (temp_r3_2 != NULL) {
        temp_r4 = this->unk94;
        if (temp_r4 != NULL) {
            temp_r0_2 = temp_r4->unk250;
            if (temp_r0_2 != 1) {
                if (temp_r0_2 == 4) {
                    return;
                }
                goto block_16;
            }
        } else {
block_16:
            UpdateConfiguration__13VocalTrackDirFv(temp_r3_2);
            temp_r3_3 = this->unk88;
            this->unk78 = temp_r3_3->unk414 - temp_r3_3->unk410;
            sp8 = Song__13MetaPerformerCFv(Current__13MetaPerformerFv((MetaPerformer *) temp_r3_3));
            temp_r27_2 = "popping unbaked plate\0%s recycling plate at %.2f sec\n\0%s baking plate at %.2f sec\n\0Too many tube plates - please file a bug to Josh Stoddard and include the Watson output.\0max plates queued -> %d\n\0max verts in a plate -> %d\n\0max faces in a plate -> %d\n\0resetting all plates\n\0dumping plates in %s\n\0\t[%d] @ %x, xPos: %.2f, xStart: %.2f, XEnd: %.2f, verts: %d, faces: %d, baked: %d\n\0\t[%d] @ %x, <empty>, verts: %d, faces: %d, baked: %d\n\0part %d front\0part %d back\0part %d phoneme\0lead deploy\0harmony deploy\0deploy_mask_lead.mat\0deploy_mask_harmony.mat\0%s new plate added.  Please alert HUD/Track owner and include the Watson output.\0vocal_jitter_debug\0VocalTrack.cpp\0list.empty()\0pUser\0force_static_vocals\0track_graphics\0markers.grp\0beat_marker.mesh\0config/track_graphics.dta\0lyric_overlap_ms\0static_vocal_parameters\0static_deploy_x_size\0static_deploy_buffer_x\0static_phrase_margin_x\0lyric_shift_ms\0lyric_shift_anticipation_ms\0min_lyric_highlight_ms\0phrase_highlight_ms\0lyric timing data:\n\0\t overlap window ms %.0f\n\0\t static deploy size %.2f\n\0\t static deploy gap size %.2f\n\0\t now bar offset %.2f\n\0\t standard lyric shift ms %.0f\n\0\t fast lyric shift ms %.0f\n\0\t lyric shift anticipation ms %.0f\n\0\t min lyric highlight ms %.0f\n\0\t phrase highlight anticipation ms %.0f\n\0part < 3\0creating new %s lyric plate\n\0lead\0harmony\0Max Lyric Plates: %d\n\0GraphicsUtl.h\0result\0VocalTrack::CreateMarker() added new %s mesh at run-time (total %d); please alert HUD/Track owner\0mesh\0mesh->GeomOwner() != mesh\0tambourine_preview.anim\0lead_color\0harmony_1_color\0harmony_2_color\0Range Shift Data\n\0[%d]\tstart ms: %.2f, intro ms: %.2f, min: %.1f -> %.1f, max: %.1f -> %.1f\n\0( 0) <= (part) && (part) < ( 3)\0Dumping %s lyric plates\n\0[%d] %x (%.2f - %.2f) %s\n\0\t<empty>\n\0\t[%d] %x\0 %s x:%.2f (%.2f - %.2f)\n\0\n\0recycling lyric plate at %.2f sec %s\n\0current: %i\n\0debug_score_current.txt\0\0relative lyric placement changed in baked plate (lead)\0relative lyric placement changed in baked plate (harmony)\0\t%3.2f\t(%6.2fms)\t\0| \0\"%s\"\0 |\0downbeat_marker.mesh\0phrase_marker.mesh\0Finished shifting lyrics for part %d to %.2f at %.2f sec\n\0Sliding lyrics for part %d to %.2f at %.2f sec\n\0lyrics.grp\0lyrics_harmony.grp\0deploy zones for part %d by song seconds\n\0[%d] %.2f - %.2f\n\0Adding lyrics for part %d: %d - %d\n\0Skipping redundant lyric \"%s\" @ %d\n\0NEW EXTRA LYRIC: \"%s\" @ %d\n\0NEW LYRIC: \"%s\" @ %d\n\0lyric.mDeployIdx (%d) < mNextDeployZone (%d) for part %d\n\0tight shift between '%s' and '%s' at %.2f sec: %.0f ms preview\n\0lyric phrase too big for window: \"%s\"\0new final deploy section for part %d\n\0tambourine_gems.mm\0--------\n\0singer->FrameTargetPitch()\0: \0singer->FrameMicPitch()\0singer->FrameBestHit()\0mPlayer->Freestyling()\0phoneme phrase\n\0non-singing section\n\0singing\n\0pitchFrame\0frameScore\0harmonyScore\0pitchZ\0v.z\0vocal_feedback.anim\0last: %i\n\0clearing all lyric plates\n\0LINE %d NOTE %d TIME %.2f PITCHES \0UNPITCHED \0tube pitch to z: %d -> %1.2f\n\0mCharOptMicID != -1\0trying to increment unimplemented vocal param %d\0scrolling\0static\0unrecognized\0invalid vocal player\0%s(%d): %s unhandled msg: %s\0New LyricShift begin %.2f sec, end %.2f x, fast: %d\n" + 0x5CD;
            var_r26 = this->unk88;
            this->unk74 = (f32) ScrollSpeed__16BandSongMetadataCFv(TheSongMgr->unk4->unk70(TheSongMgr, TheSongMgr->unk4->unkA8(TheSongMgr, &sp8, 1))) * (this->unk78 / 16.8f);
            FindObject__9ObjectDirFPCcb((ObjectDir *) var_r26, temp_r27_2, 0);
            temp_r3_4 = __dynamic_cast(0, &__RTTI__13RndAnimatable, &__RTTI__Q23Hmx6Object, NULL);
            if (temp_r3_4 == NULL) {
                var_r3 = var_r26;
                if (var_r26 != NULL) {
                    var_r3 = (VocalTrackDir *) var_r26->unk0;
                }
                if (PathName__FPCQ23Hmx6Object((Hmx::Object *) var_r3) != NULL) {
                    if (var_r26 != NULL) {
                        var_r26 = (VocalTrackDir *) var_r26->unk0;
                    }
                    var_r5 = PathName__FPCQ23Hmx6Object((Hmx::Object *) var_r26);
                } else {
                    var_r5 = (s8 *) @STRING@Find<13RndAnimatable>__9ObjectDirFPCcb_P13RndAnimatable;
                }
                Fail__5DebugFPCc(&TheDebug, MakeString<PCc,PCc>__FPCcPCcPCc_PCc(kNotObjectMsg, temp_r27_2, var_r5));
            }
            temp_r3_4->unk4->unk34(temp_r3_4, &@F_0000803f, &@F_00000000, @F_00000000, @F_0000803f);
            RebuildHUD__10VocalTrackFv(this);
        }
    }
}
```

## Wii target asm (Bank 8, ground-truth body)

```asm
# VocalTrack::UpdateVocalStyle()
.fn UpdateVocalStyle__10VocalTrackFv, global
/* 801529A0 001473C0  94 21 FF D0 */	stwu r1, -0x30(r1)
/* 801529A4 001473C4  7C 08 02 A6 */	mflr r0
/* 801529A8 001473C8  90 01 00 34 */	stw r0, 0x34(r1)
/* 801529AC 001473CC  39 61 00 30 */	addi r11, r1, 0x30
/* 801529B0 001473D0  48 8E 69 75 */	bl _savegpr_24
/* 801529B4 001473D4  3C 80 80 C9 */	lis r4, TheGame@ha
/* 801529B8 001473D8  7C 7F 1B 78 */	mr r31, r3
/* 801529BC 001473DC  80 64 EB 18 */	lwz r3, TheGame@l(r4)
/* 801529C0 001473E0  48 02 E1 81 */	bl GetActivePlayers__4GameFv
/* 801529C4 001473E4  80 1F 00 94 */	lwz r0, 0x94(r31)
/* 801529C8 001473E8  7C 7B 1B 78 */	mr r27, r3
/* 801529CC 001473EC  2C 00 00 00 */	cmpwi r0, 0x0
/* 801529D0 001473F0  41 82 00 D0 */	beq .L_80152AA0
/* 801529D4 001473F4  7C 03 03 78 */	mr r3, r0
/* 801529D8 001473F8  81 83 00 04 */	lwz r12, 0x4(r3)
/* 801529DC 001473FC  81 8C 00 2C */	lwz r12, 0x2c(r12)
/* 801529E0 00147400  7D 89 03 A6 */	mtctr r12
/* 801529E4 00147404  4E 80 04 21 */	bctrl
/* 801529E8 00147408  7C 60 00 34 */	cntlzw r0, r3
/* 801529EC 0014740C  54 00 D9 7F */	srwi. r0, r0, 5
/* 801529F0 00147410  41 82 00 B0 */	beq .L_80152AA0
/* 801529F4 00147414  3B 20 00 00 */	li r25, 0x0
/* 801529F8 00147418  3B C0 00 00 */	li r30, 0x0
/* 801529FC 0014741C  3F 80 80 B8 */	lis r28, __RTTI__10VocalTrack@ha
/* 80152A00 00147420  3F A0 80 B7 */	lis r29, __RTTI__5Track@ha
/* 80152A04 00147424  48 00 00 90 */	b .L_80152A94
.L_80152A08:
/* 80152A08 00147428  80 7B 00 00 */	lwz r3, 0x0(r27)
/* 80152A0C 0014742C  7F 03 F0 2E */	lwzx r24, r3, r30
/* 80152A10 00147430  2C 18 00 00 */	cmpwi r24, 0x0
/* 80152A14 00147434  41 82 00 78 */	beq .L_80152A8C
/* 80152A18 00147438  80 18 02 4C */	lwz r0, 0x24c(r24)
/* 80152A1C 0014743C  2C 00 00 03 */	cmpwi r0, 0x3
/* 80152A20 00147440  40 82 00 6C */	bne .L_80152A8C
/* 80152A24 00147444  83 58 02 48 */	lwz r26, 0x248(r24)
/* 80152A28 00147448  38 7F 00 1C */	addi r3, r31, 0x1c
/* 80152A2C 0014744C  4B FF 59 05 */	bl TrackNum__11TrackConfigCFv
/* 80152A30 00147450  7C 1A 18 00 */	cmpw r26, r3
/* 80152A34 00147454  41 82 00 58 */	beq .L_80152A8C
/* 80152A38 00147458  81 98 00 04 */	lwz r12, 0x4(r24)
/* 80152A3C 0014745C  7F 03 C3 78 */	mr r3, r24
/* 80152A40 00147460  81 8C 00 2C */	lwz r12, 0x2c(r12)
/* 80152A44 00147464  7D 89 03 A6 */	mtctr r12
/* 80152A48 00147468  4E 80 04 21 */	bctrl
/* 80152A4C 0014746C  2C 03 00 00 */	cmpwi r3, 0x0
/* 80152A50 00147470  41 82 00 3C */	beq .L_80152A8C
/* 80152A54 00147474  80 78 02 30 */	lwz r3, 0x230(r24)
/* 80152A58 00147478  38 BC 88 50 */	addi r5, r28, __RTTI__10VocalTrack@l
/* 80152A5C 0014747C  38 DD 6C 28 */	addi r6, r29, __RTTI__5Track@l
/* 80152A60 00147480  38 80 00 00 */	li r4, 0x0
/* 80152A64 00147484  80 63 00 7C */	lwz r3, 0x7c(r3)
/* 80152A68 00147488  38 E0 00 00 */	li r7, 0x0
/* 80152A6C 0014748C  48 8E 64 E9 */	bl __dynamic_cast
/* 80152A70 00147490  2C 03 00 00 */	cmpwi r3, 0x0
/* 80152A74 00147494  41 82 00 18 */	beq .L_80152A8C
/* 80152A78 00147498  81 83 00 00 */	lwz r12, 0x0(r3)
/* 80152A7C 0014749C  80 9F 00 6C */	lwz r4, 0x6c(r31)
/* 80152A80 001474A0  81 8C 01 88 */	lwz r12, 0x188(r12)
/* 80152A84 001474A4  7D 89 03 A6 */	mtctr r12
/* 80152A88 001474A8  4E 80 04 21 */	bctrl
.L_80152A8C:
/* 80152A8C 001474AC  3B 39 00 01 */	addi r25, r25, 0x1
/* 80152A90 001474B0  3B DE 00 04 */	addi r30, r30, 0x4
.L_80152A94:
/* 80152A94 001474B4  A0 1B 00 04 */	lhz r0, 0x4(r27)
/* 80152A98 001474B8  7C 19 00 40 */	cmplw r25, r0
/* 80152A9C 001474BC  41 80 FF 6C */	blt .L_80152A08
.L_80152AA0:
/* 80152AA0 001474C0  80 7F 00 88 */	lwz r3, 0x88(r31)
/* 80152AA4 001474C4  2C 03 00 00 */	cmpwi r3, 0x0
/* 80152AA8 001474C8  41 82 01 78 */	beq .L_80152C20
/* 80152AAC 001474CC  80 9F 00 94 */	lwz r4, 0x94(r31)
/* 80152AB0 001474D0  2C 04 00 00 */	cmpwi r4, 0x0
/* 80152AB4 001474D4  41 82 00 1C */	beq .L_80152AD0
/* 80152AB8 001474D8  80 04 02 50 */	lwz r0, 0x250(r4)
/* 80152ABC 001474DC  2C 00 00 01 */	cmpwi r0, 0x1
/* 80152AC0 001474E0  41 82 01 60 */	beq .L_80152C20
/* 80152AC4 001474E4  2C 00 00 04 */	cmpwi r0, 0x4
/* 80152AC8 001474E8  40 82 00 08 */	bne .L_80152AD0
/* 80152ACC 001474EC  48 00 01 54 */	b .L_80152C20
.L_80152AD0:
/* 80152AD0 001474F0  48 49 3A 41 */	bl UpdateConfiguration__13VocalTrackDirFv
/* 80152AD4 001474F4  80 7F 00 88 */	lwz r3, 0x88(r31)
/* 80152AD8 001474F8  C0 23 04 14 */	lfs f1, 0x414(r3)
/* 80152ADC 001474FC  C0 03 04 10 */	lfs f0, 0x410(r3)
/* 80152AE0 00147500  EC 01 00 28 */	fsubs f0, f1, f0
/* 80152AE4 00147504  D0 1F 00 78 */	stfs f0, 0x78(r31)
/* 80152AE8 00147508  48 19 9F E9 */	bl Current__13MetaPerformerFv
/* 80152AEC 0014750C  48 19 A2 95 */	bl Song__13MetaPerformerCFv
/* 80152AF0 00147510  3F C0 80 C9 */	lis r30, TheSongMgr@ha
/* 80152AF4 00147514  90 61 00 08 */	stw r3, 0x8(r1)
/* 80152AF8 00147518  80 7E F7 80 */	lwz r3, TheSongMgr@l(r30)
/* 80152AFC 0014751C  38 81 00 08 */	addi r4, r1, 0x8
/* 80152B00 00147520  38 A0 00 01 */	li r5, 0x1
/* 80152B04 00147524  81 83 00 04 */	lwz r12, 0x4(r3)
/* 80152B08 00147528  81 8C 00 A8 */	lwz r12, 0xa8(r12)
/* 80152B0C 0014752C  7D 89 03 A6 */	mtctr r12
/* 80152B10 00147530  4E 80 04 21 */	bctrl
/* 80152B14 00147534  7C 64 1B 78 */	mr r4, r3
/* 80152B18 00147538  80 7E F7 80 */	lwz r3, TheSongMgr@l(r30)
/* 80152B1C 0014753C  81 83 00 04 */	lwz r12, 0x4(r3)
/* 80152B20 00147540  81 8C 00 70 */	lwz r12, 0x70(r12)
/* 80152B24 00147544  7D 89 03 A6 */	mtctr r12
/* 80152B28 00147548  4E 80 04 21 */	bctrl
/* 80152B2C 0014754C  48 11 8E 55 */	bl ScrollSpeed__16BandSongMetadataCFv
/* 80152B30 00147550  3C 80 80 B3 */	lis r4, "@F_66668641"@ha
/* 80152B34 00147554  3C 60 80 B8 */	lis r3, "@stringBase0"@ha
/* 80152B38 00147558  C0 7F 00 78 */	lfs f3, 0x78(r31)
/* 80152B3C 0014755C  38 63 91 00 */	addi r3, r3, "@stringBase0"@l
/* 80152B40 00147560  C0 44 4E AC */	lfs f2, "@F_66668641"@l(r4)
/* 80152B44 00147564  FC 00 08 18 */	frsp f0, f1
/* 80152B48 00147568  3B 63 05 CD */	addi r27, r3, 0x5cd
/* 80152B4C 0014756C  83 5F 00 88 */	lwz r26, 0x88(r31)
/* 80152B50 00147570  EC 23 10 24 */	fdivs f1, f3, f2
/* 80152B54 00147574  7F 64 DB 78 */	mr r4, r27
/* 80152B58 00147578  7F 43 D3 78 */	mr r3, r26
/* 80152B5C 0014757C  38 A0 00 00 */	li r5, 0x0
/* 80152B60 00147580  EC 00 00 72 */	fmuls f0, f0, f1
/* 80152B64 00147584  D0 1F 00 74 */	stfs f0, 0x74(r31)
/* 80152B68 00147588  48 31 33 19 */	bl FindObject__9ObjectDirFPCcb
/* 80152B6C 0014758C  3C A0 80 B5 */	lis r5, __RTTI__13RndAnimatable@ha
/* 80152B70 00147590  3C C0 80 B5 */	lis r6, __RTTI__Q23Hmx6Object@ha
/* 80152B74 00147594  38 A5 AE D0 */	addi r5, r5, __RTTI__13RndAnimatable@l
/* 80152B78 00147598  38 80 00 00 */	li r4, 0x0
/* 80152B7C 0014759C  38 C6 A2 78 */	addi r6, r6, __RTTI__Q23Hmx6Object@l
/* 80152B80 001475A0  38 E0 00 00 */	li r7, 0x0
/* 80152B84 001475A4  48 8E 63 D1 */	bl __dynamic_cast
/* 80152B88 001475A8  2C 03 00 00 */	cmpwi r3, 0x0
/* 80152B8C 001475AC  7C 7C 1B 78 */	mr r28, r3
/* 80152B90 001475B0  40 82 00 64 */	bne .L_80152BF4
/* 80152B94 001475B4  2C 1A 00 00 */	cmpwi r26, 0x0
/* 80152B98 001475B8  7F 43 D3 78 */	mr r3, r26
/* 80152B9C 001475BC  41 82 00 08 */	beq .L_80152BA4
/* 80152BA0 001475C0  80 7A 00 00 */	lwz r3, 0x0(r26)
.L_80152BA4:
/* 80152BA4 001475C4  48 33 10 0D */	bl PathName__FPCQ23Hmx6Object
/* 80152BA8 001475C8  2C 03 00 00 */	cmpwi r3, 0x0
/* 80152BAC 001475CC  41 82 00 20 */	beq .L_80152BCC
/* 80152BB0 001475D0  2C 1A 00 00 */	cmpwi r26, 0x0
/* 80152BB4 001475D4  41 82 00 08 */	beq .L_80152BBC
/* 80152BB8 001475D8  83 5A 00 00 */	lwz r26, 0x0(r26)
.L_80152BBC:
/* 80152BBC 001475DC  7F 43 D3 78 */	mr r3, r26
/* 80152BC0 001475E0  48 33 0F F1 */	bl PathName__FPCQ23Hmx6Object
/* 80152BC4 001475E4  7C 65 1B 78 */	mr r5, r3
/* 80152BC8 001475E8  48 00 00 0C */	b .L_80152BD4
.L_80152BCC:
/* 80152BCC 001475EC  3C A0 80 B7 */	lis r5, "@STRING@Find<13RndAnimatable>__9ObjectDirFPCcb_P13RndAnimatable"@ha
... [truncated 30 of 180 asm lines]
```
