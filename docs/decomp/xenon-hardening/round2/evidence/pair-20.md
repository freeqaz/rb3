# Pair 20 — verification evidence

**Claimed identity:** Wii `__ct__14PlayerBehaviorFv`  ==  Xenon `0x826d0108`

| field | value |
|---|---|
| pair_id | 20 |
| stratum | BSIM 15-20 |
| match_type | `BSIM` |
| BSim sim×conf | 15.772 |
| BSim similarity / confidence | 0.839 / 18.798 |
| TU (Wii) | `PlayerBehavior.o` |
| Wii symbol (demangled) | `PlayerBehavior::PlayerBehavior(...)` |
| Wii addr (Bank 8) | `0x801c7380` |
| Xenon addr | `0x826d0108` |
| Xenon func name | `Function_826D0108` (stripped binary — name is auto-generated) |
| Wii body size | 28 asm lines (lines 10-36 in `build/SZBE69_B8/asm/band3/game/PlayerBehavior.s`) |
| Xenon body size | 100 bytes |

## How to read this

The Wii symbol is **ground truth** (from the CodeWarrior linker map). The Xenon
function is from a **stripped** XEX — its name `Function_<addr>` is meaningless.
The claim is that these two functions are the **same source function** compiled by
two different toolchains (Wii MWCC/Gekko PPC vs Xbox360 MSVC/Xenon PPC). Judge
whether the *control-flow shape, constant pool, field offsets, and especially the
resolved callees* are consistent with that claim. The 'resolved callee' column
below maps each Xenon callee to its matched Wii symbol where a match exists —
**agreeing callee names are the strongest cross-compiler signal.**

## Xenon callees (1 total, 1 resolved to a matched Wii symbol)

| xenon callee addr | xenon name | resolved Wii symbol (via matches.json) |
|---|---|---|
| `0x8279b788` | `??0Symbol@@QAA@PBD@Z` | `Enter__11RndPollableFv` |

## Referenced strings (Xenon side, 1)

- `'default'`

## Xenon pseudo-C (Ghidra, fork 12.2, PowerPC:BE:64:Xenon)

```c

undefined1 * Function_826D0108(undefined1 *param_1)

{
  param_1[1] = 0;
  *param_1 = 1;
  param_1[2] = 0;
  param_1[3] = 0;
  param_1[4] = 0;
  param_1[5] = 0;
  __0Symbol__QAA_PBD_Z(param_1 + 8,0xffffffff82085e20);
  *(undefined4 *)(param_1 + 0xc) = 2;
  return param_1;
}


```

## Wii m2c decompilation (target Bank-8 asm → C, `m2c --target ppc`)

```c
typedef struct PlayerBehavior {
    /* 0x00 */ s8 unk0;                             /* inferred */
    /* 0x01 */ s8 unk1;                             /* inferred */
    /* 0x02 */ s8 unk2;                             /* inferred */
    /* 0x03 */ s8 unk3;                             /* inferred */
    /* 0x04 */ s8 unk4;                             /* inferred */
    /* 0x05 */ s8 unk5;                             /* inferred */
    /* 0x06 */ char pad6[2];                        /* maybe part of unk5[3]? */
    /* 0x08 */ Symbol unk8;                         /* inferred */
    /* 0x08 */ char pad8[4];
    /* 0x0C */ s32 unkC;                            /* inferred */
} PlayerBehavior;                                   /* size >= 0x10 */

void *__ct__6SymbolFPCc(Symbol *this, s8 *arg0);    /* extern */
static s8 @stringBase0[8] = "default";

/* PlayerBehavior::PlayerBehavior (void) */
PlayerBehavior *__ct__14PlayerBehaviorFv(PlayerBehavior *this) {
    this->unk0 = 1;
    this->unk1 = 0;
    this->unk2 = 0;
    this->unk3 = 0;
    this->unk4 = 0;
    this->unk5 = 0;
    __ct__6SymbolFPCc(&this->unk8, "default");
    this->unkC = 2;
    return this;
}
```

## Wii target asm (Bank 8, ground-truth body)

```asm
# PlayerBehavior::PlayerBehavior()
.fn __ct__14PlayerBehaviorFv, global
/* 801C7380 001BBDA0  94 21 FF F0 */	stwu r1, -0x10(r1)
/* 801C7384 001BBDA4  7C 08 02 A6 */	mflr r0
/* 801C7388 001BBDA8  3C 80 80 B8 */	lis r4, "@stringBase0"@ha
/* 801C738C 001BBDAC  38 A0 00 01 */	li r5, 0x1
/* 801C7390 001BBDB0  90 01 00 14 */	stw r0, 0x14(r1)
/* 801C7394 001BBDB4  38 00 00 00 */	li r0, 0x0
/* 801C7398 001BBDB8  38 84 0B E0 */	addi r4, r4, "@stringBase0"@l
/* 801C739C 001BBDBC  93 E1 00 0C */	stw r31, 0xc(r1)
/* 801C73A0 001BBDC0  7C 7F 1B 78 */	mr r31, r3
/* 801C73A4 001BBDC4  98 A3 00 00 */	stb r5, 0x0(r3)
/* 801C73A8 001BBDC8  98 03 00 01 */	stb r0, 0x1(r3)
/* 801C73AC 001BBDCC  98 03 00 02 */	stb r0, 0x2(r3)
/* 801C73B0 001BBDD0  98 03 00 03 */	stb r0, 0x3(r3)
/* 801C73B4 001BBDD4  98 03 00 04 */	stb r0, 0x4(r3)
/* 801C73B8 001BBDD8  98 03 00 05 */	stb r0, 0x5(r3)
/* 801C73BC 001BBDDC  38 63 00 08 */	addi r3, r3, 0x8
/* 801C73C0 001BBDE0  48 2F 5E 01 */	bl __ct__6SymbolFPCc
/* 801C73C4 001BBDE4  38 00 00 02 */	li r0, 0x2
/* 801C73C8 001BBDE8  90 1F 00 0C */	stw r0, 0xc(r31)
/* 801C73CC 001BBDEC  7F E3 FB 78 */	mr r3, r31
/* 801C73D0 001BBDF0  83 E1 00 0C */	lwz r31, 0xc(r1)
/* 801C73D4 001BBDF4  80 01 00 14 */	lwz r0, 0x14(r1)
/* 801C73D8 001BBDF8  7C 08 03 A6 */	mtlr r0
/* 801C73DC 001BBDFC  38 21 00 10 */	addi r1, r1, 0x10
/* 801C73E0 001BBE00  4E 80 00 20 */	blr
.endfn __ct__14PlayerBehaviorFv
```
