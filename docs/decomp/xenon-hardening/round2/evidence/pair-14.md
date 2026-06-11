# Pair 14 — verification evidence

**Claimed identity:** Wii `Hit__4TailFv`  ==  Xenon `0x82b7e618`

| field | value |
|---|---|
| pair_id | 14 |
| stratum | BSIM 20-30 |
| match_type | `BSIM` |
| BSim sim×conf | 23.693 |
| BSim similarity / confidence | 0.876 / 27.047 |
| TU (Wii) | `Tail.o` |
| Wii symbol (demangled) | `Tail::Hit(...)` |
| Wii addr (Bank 8) | `0x80144ea0` |
| Xenon addr | `0x82b7e618` |
| Xenon func name | `Function_82B7E618` (stripped binary — name is auto-generated) |
| Wii body size | 33 asm lines (lines 1386-1417 in `build/SZBE69_B8/asm/band3/bandtrack/Tail.s`) |
| Xenon body size | 108 bytes |

## How to read this

The Wii symbol is **ground truth** (from the CodeWarrior linker map). The Xenon
function is from a **stripped** XEX — its name `Function_<addr>` is meaningless.
The claim is that these two functions are the **same source function** compiled by
two different toolchains (Wii MWCC/Gekko PPC vs Xbox360 MSVC/Xenon PPC). Judge
whether the *control-flow shape, constant pool, field offsets, and especially the
resolved callees* are consistent with that claim. The 'resolved callee' column
below maps each Xenon callee to its matched Wii symbol where a match exists —
**agreeing callee names are the strongest cross-compiler signal.**

## Xenon callees (1 total, 0 resolved to a matched Wii symbol)

| xenon callee addr | xenon name | resolved Wii symbol (via matches.json) |
|---|---|---|
| `0x82805b10` | `FUN_82805b10` | _(unmatched / Function_)_ |

## Referenced strings (Xenon side, 0)

_(none)_

## Xenon pseudo-C (Ghidra, fork 12.2, PowerPC:BE:64:Xenon)

```c

void Function_82B7E618(int param_1)

{
  *(undefined4 *)(param_1 + 0x18) = 2;
  if (*(char *)(param_1 + 0x4f8) == '\0') {
    *(undefined1 *)(*(int *)(param_1 + 0xc) + 8) = 1;
  }
  if (*(char *)(param_1 + 0x28) != '\0') {
    FUN_82805b10(param_1 + 0x2c,0,0x4b0);
    *(undefined4 *)(param_1 + 0x4dc) = 0;
  }
  return;
}


```

## Wii m2c decompilation (target Bank-8 asm → C, `m2c --target ppc`)

```c
typedef struct Tail {
    /* 0x000 */ char pad0[0xC];
    /* 0x00C */ void *unkC;                         /* inferred */
    /* 0x010 */ char pad10[8];                      /* maybe part of unkC[3]? */
    /* 0x018 */ s32 unk18;                          /* inferred */
    /* 0x01C */ char pad1C[0xC];                    /* maybe part of unk18[4]? */
    /* 0x028 */ u8 unk28;                           /* inferred */
    /* 0x029 */ char pad29[3];                      /* maybe part of unk28[4]? */
    /* 0x02C */ ? unk2C;                            /* inferred */
    /* 0x02C */ char pad2C[0x4B0];
    /* 0x4DC */ s32 unk4DC;                         /* inferred */
    /* 0x4E0 */ char pad4E0[0x18];                  /* maybe part of unk4DC[7]? */
    /* 0x4F8 */ u8 unk4F8;                          /* inferred */
} Tail;                                             /* size >= 0x4F9 */

? memset(? *, ?, ?);                                /* extern */

/* Tail::Hit (void) */
void Hit__4TailFv(Tail *this) {
    void *temp_r4;

    this->unk18 = 2;
    if ((s32) this->unk4F8 == 0) {
        temp_r4 = this->unkC;
        temp_r4->unk8 = (u8) (temp_r4->unk8 | 0x80);
    }
    if ((s32) this->unk28 != 0) {
        memset(&this->unk2C, 0, 0x4B0);
        this->unk4DC = 0;
    }
}
```

## Wii target asm (Bank 8, ground-truth body)

```asm
# Tail::Hit()
.fn Hit__4TailFv, global
/* 80144EA0 001398C0  94 21 FF F0 */	stwu r1, -0x10(r1)
/* 80144EA4 001398C4  7C 08 02 A6 */	mflr r0
/* 80144EA8 001398C8  38 80 00 02 */	li r4, 0x2
/* 80144EAC 001398CC  90 01 00 14 */	stw r0, 0x14(r1)
/* 80144EB0 001398D0  93 E1 00 0C */	stw r31, 0xc(r1)
/* 80144EB4 001398D4  7C 7F 1B 78 */	mr r31, r3
/* 80144EB8 001398D8  88 03 04 F8 */	lbz r0, 0x4f8(r3)
/* 80144EBC 001398DC  90 83 00 18 */	stw r4, 0x18(r3)
/* 80144EC0 001398E0  2C 00 00 00 */	cmpwi r0, 0x0
/* 80144EC4 001398E4  40 82 00 14 */	bne .L_80144ED8
/* 80144EC8 001398E8  80 83 00 0C */	lwz r4, 0xc(r3)
/* 80144ECC 001398EC  88 04 00 08 */	lbz r0, 0x8(r4)
/* 80144ED0 001398F0  60 00 00 80 */	ori r0, r0, 0x80
/* 80144ED4 001398F4  98 04 00 08 */	stb r0, 0x8(r4)
.L_80144ED8:
/* 80144ED8 001398F8  88 03 00 28 */	lbz r0, 0x28(r3)
/* 80144EDC 001398FC  2C 00 00 00 */	cmpwi r0, 0x0
/* 80144EE0 00139900  41 82 00 1C */	beq .L_80144EFC
/* 80144EE4 00139904  38 80 00 00 */	li r4, 0x0
/* 80144EE8 00139908  38 A0 04 B0 */	li r5, 0x4b0
/* 80144EEC 0013990C  38 63 00 2C */	addi r3, r3, 0x2c
/* 80144EF0 00139910  4B EB F4 61 */	bl memset
/* 80144EF4 00139914  38 00 00 00 */	li r0, 0x0
/* 80144EF8 00139918  90 1F 04 DC */	stw r0, 0x4dc(r31)
.L_80144EFC:
/* 80144EFC 0013991C  80 01 00 14 */	lwz r0, 0x14(r1)
/* 80144F00 00139920  83 E1 00 0C */	lwz r31, 0xc(r1)
/* 80144F04 00139924  7C 08 03 A6 */	mtlr r0
/* 80144F08 00139928  38 21 00 10 */	addi r1, r1, 0x10
/* 80144F0C 0013992C  4E 80 00 20 */	blr
.endfn Hit__4TailFv
```
