# Pair 28 — verification evidence

**Claimed identity:** Wii `TrackTypeToScoreType__F9TrackTypebb`  ==  Xenon `0x82670e70`

| field | value |
|---|---|
| pair_id | 28 |
| stratum | SwitchSig |
| match_type | `SwitchSigHasher` |
| BSim sim×conf | n/a (non-BSim) |
| TU (Wii) | `Defines.o` |
| Wii symbol (demangled) | `TrackTypeToScoreType(...)   [free function]` |
| Wii addr (Bank 8) | `0x80171cd0` |
| Xenon addr | `0x82670e70` |
| Xenon func name | `FUN_82670e70` (stripped binary — name is auto-generated) |
| Wii body size | 65 asm lines (lines 181-244 in `build/SZBE69_B8/asm/band3/game/Defines.s`) |
| Xenon body size | 160 bytes |

## How to read this

The Wii symbol is **ground truth** (from the CodeWarrior linker map). The Xenon
function is from a **stripped** XEX — its name `Function_<addr>` is meaningless.
The claim is that these two functions are the **same source function** compiled by
two different toolchains (Wii MWCC/Gekko PPC vs Xbox360 MSVC/Xenon PPC). Judge
whether the *control-flow shape, constant pool, field offsets, and especially the
resolved callees* are consistent with that claim. The 'resolved callee' column
below maps each Xenon callee to its matched Wii symbol where a match exists —
**agreeing callee names are the strongest cross-compiler signal.**

## Xenon callees (0 total, 0 resolved to a matched Wii symbol)

_(no direct callees — leaf function)_

## Referenced strings (Xenon side, 0)

_(none)_

## Xenon pseudo-C (Ghidra, fork 12.2, PowerPC:BE:64:Xenon)

```c

byte FUN_82670e70(undefined4 param_1,char param_2,char param_3)

{
  switch(param_1) {
  case 0:
    return -(param_3 != '\0') & 6;
  case 1:
    return 2;
  case 2:
    return 1;
  case 3:
    return (param_2 != '\0') + 3;
  case 4:
    return 5;
  case 5:
    return 9;
  case 6:
    return 7;
  default:
    return 0xb;
  case 8:
    return 8;
  case 10:
  case 0xb:
  case 0xc:
    return 10;
  }
}


```

## Wii m2c decompilation (target Bank-8 asm → C, `m2c --target ppc`)

```c
? Fail__5DebugFPCc(Debug *this, s8 *arg0);          /* extern */
s8 *Str__12FormatStringFv(FormatString *this);      /* extern */
void *__ct__12FormatStringFPCc(FormatString *this, s8 *arg0); /* extern */
extern Debug TheDebug;
static ? *@15533[0xD] = {
    &.L_80171CFC,
    &.L_80171D18,
    &.L_80171D10,
    &.L_80171D20,
    &.L_80171D34,
    &.L_80171D4C,
    &.L_80171D3C,
    &.L_80171D5C,
    &.L_80171D44,
    &.L_80171D5C,
    &.L_80171D54,
    &.L_80171D54,
    &.L_80171D54,
};
static ? @stringBase0;                              /* unable to generate initializer: unknown type */

/* TrackTypeToScoreType (TrackType, bool, bool) */
s32 TrackTypeToScoreType__F9TrackTypebb(? arg0, s32 arg1, s32 arg2) {
    FormatString sp8;

    if ((u32) arg0 <= 0xCU) {
        return (s32) arg0;
    }
    __ct__12FormatStringFPCc(&sp8, "Defines.cpp\0( 0) <= (controllerType) && (controllerType) <= ( kNumControllerTypes)\0CHAR_INSTRUMENT_SYMBOLS\0false\0unrecognized TrackType!\0no TrackType for this ScoreType!\0SCORE_TYPE_SYMBOLS\0DIFF_SYMBOLS\0default_difficulty\0tour\0DIFF_SHORT_SYMBOLS\0No tracks playable by controller %i\0No tracks representative of part %i\0No priority tracks for controller %i" + 0x71);
    Fail__5DebugFPCc(&TheDebug, Str__12FormatStringFv(&sp8));
    return 0xB;
}
```

## Wii target asm (Bank 8, ground-truth body)

```asm
# TrackTypeToScoreType(TrackType, bool, bool)
.fn TrackTypeToScoreType__F9TrackTypebb, global
/* 80171CD0 001666F0  94 21 F7 E0 */	stwu r1, -0x820(r1)
/* 80171CD4 001666F4  7C 08 02 A6 */	mflr r0
/* 80171CD8 001666F8  28 03 00 0C */	cmplwi r3, 0xc
/* 80171CDC 001666FC  90 01 08 24 */	stw r0, 0x824(r1)
/* 80171CE0 00166700  41 81 00 7C */	bgt .L_80171D5C
/* 80171CE4 00166704  3C C0 80 B8 */	lis r6, "@15533"@ha
/* 80171CE8 00166708  54 60 10 3A */	slwi r0, r3, 2
/* 80171CEC 0016670C  38 C6 BA 2C */	addi r6, r6, "@15533"@l
/* 80171CF0 00166710  7C C6 00 2E */	lwzx r6, r6, r0
/* 80171CF4 00166714  7C C9 03 A6 */	mtctr r6
/* 80171CF8 00166718  4E 80 04 20 */	bctr
.L_80171CFC:
/* 80171CFC 0016671C  2C 05 00 00 */	cmpwi r5, 0x0
/* 80171D00 00166720  38 60 00 00 */	li r3, 0x0
/* 80171D04 00166724  41 82 00 88 */	beq .L_80171D8C
/* 80171D08 00166728  38 60 00 06 */	li r3, 0x6
/* 80171D0C 0016672C  48 00 00 80 */	b .L_80171D8C
.L_80171D10:
/* 80171D10 00166730  38 60 00 01 */	li r3, 0x1
/* 80171D14 00166734  48 00 00 78 */	b .L_80171D8C
.L_80171D18:
/* 80171D18 00166738  38 60 00 02 */	li r3, 0x2
/* 80171D1C 0016673C  48 00 00 70 */	b .L_80171D8C
.L_80171D20:
/* 80171D20 00166740  2C 04 00 00 */	cmpwi r4, 0x0
/* 80171D24 00166744  38 60 00 03 */	li r3, 0x3
/* 80171D28 00166748  41 82 00 64 */	beq .L_80171D8C
/* 80171D2C 0016674C  38 60 00 04 */	li r3, 0x4
/* 80171D30 00166750  48 00 00 5C */	b .L_80171D8C
.L_80171D34:
/* 80171D34 00166754  38 60 00 05 */	li r3, 0x5
/* 80171D38 00166758  48 00 00 54 */	b .L_80171D8C
.L_80171D3C:
/* 80171D3C 0016675C  38 60 00 07 */	li r3, 0x7
/* 80171D40 00166760  48 00 00 4C */	b .L_80171D8C
.L_80171D44:
/* 80171D44 00166764  38 60 00 08 */	li r3, 0x8
/* 80171D48 00166768  48 00 00 44 */	b .L_80171D8C
.L_80171D4C:
/* 80171D4C 0016676C  38 60 00 09 */	li r3, 0x9
/* 80171D50 00166770  48 00 00 3C */	b .L_80171D8C
.L_80171D54:
/* 80171D54 00166774  38 60 00 0A */	li r3, 0xa
/* 80171D58 00166778  48 00 00 34 */	b .L_80171D8C
.L_80171D5C:
/* 80171D5C 0016677C  3C 80 80 B8 */	lis r4, "@stringBase0"@ha
/* 80171D60 00166780  38 61 00 08 */	addi r3, r1, 0x8
/* 80171D64 00166784  38 84 BB 50 */	addi r4, r4, "@stringBase0"@l
/* 80171D68 00166788  38 84 00 71 */	addi r4, r4, 0x71
/* 80171D6C 0016678C  48 32 AD E5 */	bl __ct__12FormatStringFPCc
/* 80171D70 00166790  38 61 00 08 */	addi r3, r1, 0x8
/* 80171D74 00166794  48 32 BD DD */	bl Str__12FormatStringFv
/* 80171D78 00166798  3C A0 80 C9 */	lis r5, TheDebug@ha
/* 80171D7C 0016679C  7C 64 1B 78 */	mr r4, r3
/* 80171D80 001667A0  38 65 73 D8 */	addi r3, r5, TheDebug@l
/* 80171D84 001667A4  48 2A CC 3D */	bl Fail__5DebugFPCc
/* 80171D88 001667A8  38 60 00 0B */	li r3, 0xb
.L_80171D8C:
/* 80171D8C 001667AC  80 01 08 24 */	lwz r0, 0x824(r1)
/* 80171D90 001667B0  7C 08 03 A6 */	mtlr r0
/* 80171D94 001667B4  38 21 08 20 */	addi r1, r1, 0x820
/* 80171D98 001667B8  4E 80 00 20 */	blr
.endfn TrackTypeToScoreType__F9TrackTypebb
```
