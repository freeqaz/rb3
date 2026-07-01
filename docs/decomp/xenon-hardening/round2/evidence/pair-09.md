# Pair 09 — verification evidence

**Claimed identity:** Wii `SetGameOver__4GameFb`  ==  Xenon `0x8265a168`

| field | value |
|---|---|
| pair_id | 09 |
| stratum | BSIM 20-30 |
| match_type | `BSIM` |
| BSim sim×conf | 28.524 |
| BSim similarity / confidence | 0.898 / 31.764 |
| TU (Wii) | `Game.o` |
| Wii symbol (demangled) | `Game::SetGameOver(...)` |
| Wii addr (Bank 8) | `0x80181720` |
| Xenon addr | `0x8265a168` |
| Xenon func name | `Function_8265A168` (stripped binary — name is auto-generated) |
| Wii body size | 41 asm lines (lines 6829-6868 in `build/SZBE69_B8/asm/band3/game/Game.s`) |
| Xenon body size | 144 bytes |

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
| `0x824fedf8` | `?SetCollectStats@AutoTimer@@SAX_N0@Z` | `SetCollectStats__9AutoTimerFbb` |
| `0x823d2a58` | `Function_823D2A58` | `EndGame__10NetSessionFibf` |
| `0x82659ec0` | `Function_82659EC0` | `GetResult__4GameFb` |

## Referenced strings (Xenon side, 0)

_(none)_

## Xenon pseudo-C (Ghidra, fork 12.2, PowerPC:BE:64:Xenon)

```c

void Function_8265A168(int param_1,ulonglong param_2)

{
  undefined8 uVar1;
  
  if (*(int *)(DAT_82dd0ea4 + 0x74) != 3) {
    if ((param_2 & 0xff) == 0) {
      *(undefined4 *)(param_1 + 0x128) = *(undefined4 *)(param_1 + 0xb4);
    }
    _SetCollectStats_AutoTimer__SAX_N0_Z(0,PTR_DAT_82c45ba8[0x104]);
    uVar1 = Function_82659EC0(param_1,param_2);
    Function_823D2A58((double)*(float *)(param_1 + 0x128),DAT_82c8e6f0,uVar1,0);
  }
  return;
}


```

## Wii m2c decompilation (target Bank-8 asm → C, `m2c --target ppc`)

```c
typedef struct Game {
    /* 0x000 */ char pad0[0xAC];
    /* 0x0AC */ f32 unkAC;                          /* inferred */
    /* 0x0B0 */ char padB0[0x74];                   /* maybe part of unkAC[0x1E]? */
    /* 0x124 */ f32 unk124;                         /* inferred */
} Game;                                             /* size >= 0x128 */

? EndGame__10NetSessionFibf(NetSession *this, s32 arg0, s32 arg1, f32 arg2); /* extern */
? SetCollectStats__9AutoTimerFbb(AutoTimer *this, u8 arg0, s32 arg1); /* extern */
s32 GetResult__4GameFb(Game *this, s32 arg0);       /* static */
extern void *TheGamePanel;
extern NetSession *TheNetSession;
extern void *TheRnd;

/* Game::SetGameOver (bool) */
void SetGameOver__4GameFb(Game *this, s32 arg0) {
    if ((s32) TheGamePanel->unk90 != 3) {
        if (arg0 == 0) {
            this->unk124 = this->unkAC;
        }
        SetCollectStats__9AutoTimerFbb(NULL, TheRnd->unkEC, (s32) TheGamePanel);
        EndGame__10NetSessionFibf(TheNetSession, GetResult__4GameFb(this, arg0), 0, this->unk124);
    }
}
```

## Wii target asm (Bank 8, ground-truth body)

```asm
# Game::SetGameOver(bool)
.fn SetGameOver__4GameFb, global
/* 80181720 00176140  94 21 FF F0 */	stwu r1, -0x10(r1)
/* 80181724 00176144  7C 08 02 A6 */	mflr r0
/* 80181728 00176148  3C A0 80 C9 */	lis r5, TheGamePanel@ha
/* 8018172C 0017614C  90 01 00 14 */	stw r0, 0x14(r1)
/* 80181730 00176150  93 E1 00 0C */	stw r31, 0xc(r1)
/* 80181734 00176154  7C 9F 23 78 */	mr r31, r4
/* 80181738 00176158  93 C1 00 08 */	stw r30, 0x8(r1)
/* 8018173C 0017615C  7C 7E 1B 78 */	mr r30, r3
/* 80181740 00176160  80 A5 EB C0 */	lwz r5, TheGamePanel@l(r5)
/* 80181744 00176164  80 05 00 90 */	lwz r0, 0x90(r5)
/* 80181748 00176168  2C 00 00 03 */	cmpwi r0, 0x3
/* 8018174C 0017616C  41 82 00 4C */	beq .L_80181798
/* 80181750 00176170  2C 04 00 00 */	cmpwi r4, 0x0
/* 80181754 00176174  40 82 00 0C */	bne .L_80181760
/* 80181758 00176178  C0 03 00 AC */	lfs f0, 0xac(r3)
/* 8018175C 0017617C  D0 03 01 24 */	stfs f0, 0x124(r3)
.L_80181760:
/* 80181760 00176180  3C 80 80 D2 */	lis r4, TheRnd@ha
/* 80181764 00176184  38 60 00 00 */	li r3, 0x0
/* 80181768 00176188  80 84 12 18 */	lwz r4, TheRnd@l(r4)
/* 8018176C 0017618C  88 84 00 EC */	lbz r4, 0xec(r4)
/* 80181770 00176190  48 2C 36 61 */	bl SetCollectStats__9AutoTimerFbb
/* 80181774 00176194  7F C3 F3 78 */	mr r3, r30
/* 80181778 00176198  7F E4 FB 78 */	mr r4, r31
/* 8018177C 0017619C  4B FF E6 25 */	bl GetResult__4GameFb
/* 80181780 001761A0  3C A0 80 C9 */	lis r5, TheNetSession@ha
/* 80181784 001761A4  7C 64 1B 78 */	mr r4, r3
/* 80181788 001761A8  80 65 9F 28 */	lwz r3, TheNetSession@l(r5)
/* 8018178C 001761AC  38 A0 00 00 */	li r5, 0x0
/* 80181790 001761B0  C0 3E 01 24 */	lfs f1, 0x124(r30)
/* 80181794 001761B4  4B F8 7D ED */	bl EndGame__10NetSessionFibf
.L_80181798:
/* 80181798 001761B8  80 01 00 14 */	lwz r0, 0x14(r1)
/* 8018179C 001761BC  83 E1 00 0C */	lwz r31, 0xc(r1)
/* 801817A0 001761C0  83 C1 00 08 */	lwz r30, 0x8(r1)
/* 801817A4 001761C4  7C 08 03 A6 */	mtlr r0
/* 801817A8 001761C8  38 21 00 10 */	addi r1, r1, 0x10
/* 801817AC 001761CC  4E 80 00 20 */	blr
.endfn SetGameOver__4GameFb
```
