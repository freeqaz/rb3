# Pair 15 — verification evidence

**Claimed identity:** Wii `Unload__21CampaignSongInfoPanelFv`  ==  Xenon `0x826035a8`

| field | value |
|---|---|
| pair_id | 15 |
| stratum | BSIM 15-20 |
| match_type | `BSIM` |
| BSim sim×conf | 17.746 |
| BSim similarity / confidence | 0.895 / 19.828 |
| TU (Wii) | `CampaignSongInfoPanel.o` |
| Wii symbol (demangled) | `CampaignSongInfoPanel::Unload(...)` |
| Wii addr (Bank 8) | `0x8028b9c0` |
| Xenon addr | `0x826035a8` |
| Xenon func name | `Function_826035A8` (stripped binary — name is auto-generated) |
| Wii body size | 25 asm lines (lines 777-800 in `build/SZBE69_B8/asm/band3/meta_band/CampaignSongInfoPanel.s`) |
| Xenon body size | 84 bytes |

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
| `0x827eecf8` | `Function_827EECF8` | _(unmatched / Function_)_ |

## Referenced strings (Xenon side, 0)

_(none)_

## Xenon pseudo-C (Ghidra, fork 12.2, PowerPC:BE:64:Xenon)

```c

void Function_826035A8(int param_1)

{
  undefined4 *puVar1;
  
  Function_827EECF8();
  puVar1 = *(undefined4 **)(param_1 + 0x44);
  if (puVar1 != (undefined4 *)0x0) {
    (**(code **)*puVar1)(puVar1,1);
  }
  *(undefined4 *)(param_1 + 0x44) = 0;
  return;
}


```

## Wii m2c decompilation (target Bank-8 asm → C, `m2c --target ppc`)

```c
typedef struct CampaignSongInfoPanel {
    /* 0x00 */ char pad0[0x38];
    /* 0x38 */ void **unk38;                        /* inferred */
} CampaignSongInfoPanel;                            /* size >= 0x3C */

? Unload__7UIPanelFv(UIPanel *this);                /* extern */
void Unload__21CampaignSongInfoPanelFv(CampaignSongInfoPanel *this); /* static */

/* CampaignSongInfoPanel::Unload (void) */
void Unload__21CampaignSongInfoPanelFv(CampaignSongInfoPanel *this) {
    void **temp_r3;

    Unload__7UIPanelFv((UIPanel *) this);
    temp_r3 = this->unk38;
    if (temp_r3 != NULL) {
        (*temp_r3)->unk8(1);
    }
    this->unk38 = NULL;
}
```

## Wii target asm (Bank 8, ground-truth body)

```asm
# CampaignSongInfoPanel::Unload()
.fn Unload__21CampaignSongInfoPanelFv, global
/* 8028B9C0 002803E0  94 21 FF F0 */	stwu r1, -0x10(r1)
/* 8028B9C4 002803E4  7C 08 02 A6 */	mflr r0
/* 8028B9C8 002803E8  90 01 00 14 */	stw r0, 0x14(r1)
/* 8028B9CC 002803EC  93 E1 00 0C */	stw r31, 0xc(r1)
/* 8028B9D0 002803F0  7C 7F 1B 78 */	mr r31, r3
/* 8028B9D4 002803F4  48 56 09 AD */	bl Unload__7UIPanelFv
/* 8028B9D8 002803F8  80 7F 00 38 */	lwz r3, 0x38(r31)
/* 8028B9DC 002803FC  2C 03 00 00 */	cmpwi r3, 0x0
/* 8028B9E0 00280400  41 82 00 18 */	beq .L_8028B9F8
/* 8028B9E4 00280404  81 83 00 00 */	lwz r12, 0x0(r3)
/* 8028B9E8 00280408  38 80 00 01 */	li r4, 0x1
/* 8028B9EC 0028040C  81 8C 00 08 */	lwz r12, 0x8(r12)
/* 8028B9F0 00280410  7D 89 03 A6 */	mtctr r12
/* 8028B9F4 00280414  4E 80 04 21 */	bctrl
.L_8028B9F8:
/* 8028B9F8 00280418  38 00 00 00 */	li r0, 0x0
/* 8028B9FC 0028041C  90 1F 00 38 */	stw r0, 0x38(r31)
/* 8028BA00 00280420  83 E1 00 0C */	lwz r31, 0xc(r1)
/* 8028BA04 00280424  80 01 00 14 */	lwz r0, 0x14(r1)
/* 8028BA08 00280428  7C 08 03 A6 */	mtlr r0
/* 8028BA0C 0028042C  38 21 00 10 */	addi r1, r1, 0x10
/* 8028BA10 00280430  4E 80 00 20 */	blr
.endfn Unload__21CampaignSongInfoPanelFv
```
