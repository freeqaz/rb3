# Pair 08 — verification evidence

**Claimed identity:** Wii `SendSongsToMetaPerformer__17SetlistMergePanelFRCQ211stlpmtx_std45vector<i,Us,Q211stlpmtx_std15StlNodeAlloc<i>>`  ==  Xenon `0x82617830`

| field | value |
|---|---|
| pair_id | 08 |
| stratum | BSIM 20-30 |
| match_type | `BSIM` |
| BSim sim×conf | 23.666 |
| BSim similarity / confidence | 0.518 / 45.687 |
| TU (Wii) | `SetlistMergePanel.o` |
| Wii symbol (demangled) | `SetlistMergePanel::SendSongsToMetaPerformer(...)` |
| Wii addr (Bank 8) | `0x80365020` |
| Xenon addr | `0x82617830` |
| Xenon func name | `Function_82617830` (stripped binary — name is auto-generated) |
| Wii body size | 93 asm lines (0x80365020 (size 0x168, llvm-objdump by-addr from bank8_target.elf)) |
| Xenon body size | 328 bytes |

## How to read this

The Wii symbol is **ground truth** (from the CodeWarrior linker map). The Xenon
function is from a **stripped** XEX — its name `Function_<addr>` is meaningless.
The claim is that these two functions are the **same source function** compiled by
two different toolchains (Wii MWCC/Gekko PPC vs Xbox360 MSVC/Xenon PPC). Judge
whether the *control-flow shape, constant pool, field offsets, and especially the
resolved callees* are consistent with that claim. The 'resolved callee' column
below maps each Xenon callee to its matched Wii symbol where a match exists —
**agreeing callee names are the strongest cross-compiler signal.**

## Xenon callees (5 total, 3 resolved to a matched Wii symbol)

| xenon callee addr | xenon name | resolved Wii symbol (via matches.json) |
|---|---|---|
| `0x82563f98` | `FUN_82563f98` | `Current__13MetaPerformerFv` |
| `0x825691d0` | `Function_825691D0` | `SetBattle__13MetaPerformerFPC18BattleSavedSetlist` |
| `0x825692d0` | `Function_825692D0` | _(unmatched / Function_)_ |
| `0x8256af10` | `Function_8256AF10` | `SetSetlist__13MetaPerformerFPC12SavedSetlist` |
| `0x82804da8` | `Function_82804DA8` | _(unmatched / Function_)_ |

## Referenced strings (Xenon side, 0)

_(none)_

## Xenon pseudo-C (Ghidra, fork 12.2, PowerPC:BE:64:Xenon)

```c

void Function_82617830(undefined8 param_1,int *param_2)

{
  int *piVar1;
  char cVar4;
  int iVar3;
  undefined8 uVar2;
  bool bVar5;
  int iVar6;
  int iVar7;
  
  bVar5 = false;
  iVar3 = param_2[1] - *param_2 >> 2;
  piVar1 = *(int **)(DAT_82dcbf70 + 0x160);
  if ((piVar1 != (int *)0x0) && (piVar1[5] - piVar1[4] >> 2 == iVar3)) {
    bVar5 = true;
    iVar6 = 0;
    if (0 < iVar3) {
      iVar7 = 0;
      do {
        if (*(int *)(iVar7 + *param_2) != *(int *)(iVar7 + piVar1[4])) {
          bVar5 = false;
          break;
        }
        iVar6 = iVar6 + 1;
        iVar7 = iVar7 + 4;
      } while (iVar6 < iVar3);
    }
  }
  if (bVar5) {
    cVar4 = (**(code **)(*piVar1 + 0x14))(piVar1);
    if (cVar4 != '\0') {
      iVar3 = Function_82804DA8(piVar1,0,0xffffffff82c42080,0xffffffff82c4209c,0);
      if ((*(int *)(iVar3 + 0x30) == 6) || (bVar5 = false, *(int *)(iVar3 + 0x30) == 7)) {
        bVar5 = true;
      }
      if (!bVar5) {
        uVar2 = FUN_82563f98();
        Function_825691D0(uVar2,iVar3);
        return;
      }
    }
    uVar2 = FUN_82563f98();
    Function_8256AF10(uVar2,piVar1);
  }
  else {
    uVar2 = FUN_82563f98();
    Function_825692D0(uVar2,param_2);
  }
  return;
}


```

## Wii m2c decompilation (target Bank-8 asm → C, `m2c --target ppc`)

_(m2c not available: not in dtk split (used llvm-objdump) — see raw asm below)_

## Wii target asm (Bank 8, ground-truth body)

```asm

80365020 <SendSongsToMetaPerformer__17SetlistMergePanelFRCQ211stlpmtx_std45vector<i,Us,Q211stlpmtx_std15StlNodeAlloc<i>>>:
80365020: 94 21 ff f0  	stwu 1, -16(1)
80365024: 7c 08 02 a6  	mflr 0
80365028: 3c 60 80 c9  	lis 3, -32567
8036502c: 38 e0 00 00  	li 7, 0
80365030: 90 01 00 14  	stw 0, 20(1)
80365034: 93 e1 00 0c  	stw 31, 12(1)
80365038: 93 c1 00 08  	stw 30, 8(1)
8036503c: 7c 9e 23 78  	mr	30, 4
80365040: 80 63 04 b8  	lwz 3, 1208(3)
80365044: a0 a4 00 04  	lhz 5, 4(4)
80365048: 83 e3 01 3c  	lwz 31, 316(3)
8036504c: 2c 1f 00 00  	cmpwi	31, 0
80365050: 41 82 00 50  	bt	2, 0x803650a0 <SendSongsToMetaPerformer__17SetlistMergePanelFRCQ211stlpmtx_std45vector<i,Us,Q211stlpmtx_std15StlNodeAlloc<i>>+0x80>
80365054: a0 1f 00 14  	lhz 0, 20(31)
80365058: 7c 05 00 40  	cmplw	5, 0
8036505c: 40 82 00 44  	bf	2, 0x803650a0 <SendSongsToMetaPerformer__17SetlistMergePanelFRCQ211stlpmtx_std45vector<i,Us,Q211stlpmtx_std15StlNodeAlloc<i>>+0x80>
80365060: 38 e0 00 01  	li 7, 1
80365064: 38 60 00 00  	li 3, 0
80365068: 7c a9 03 a6  	mtctr 5
8036506c: 2c 05 00 00  	cmpwi	5, 0
80365070: 40 81 00 30  	bf	1, 0x803650a0 <SendSongsToMetaPerformer__17SetlistMergePanelFRCQ211stlpmtx_std45vector<i,Us,Q211stlpmtx_std15StlNodeAlloc<i>>+0x80>
80365074: 60 00 00 00  	nop
80365078: 80 c4 00 00  	lwz 6, 0(4)
8036507c: 80 bf 00 10  	lwz 5, 16(31)
80365080: 7c c6 18 2e  	lwzx 6, 6, 3
80365084: 7c 05 18 2e  	lwzx 0, 5, 3
80365088: 7c 06 00 00  	cmpw	6, 0
8036508c: 41 82 00 0c  	bt	2, 0x80365098 <SendSongsToMetaPerformer__17SetlistMergePanelFRCQ211stlpmtx_std45vector<i,Us,Q211stlpmtx_std15StlNodeAlloc<i>>+0x78>
80365090: 38 e0 00 00  	li 7, 0
80365094: 48 00 00 0c  	b 0x803650a0 <SendSongsToMetaPerformer__17SetlistMergePanelFRCQ211stlpmtx_std45vector<i,Us,Q211stlpmtx_std15StlNodeAlloc<i>>+0x80>
80365098: 38 63 00 04  	addi 3, 3, 4
8036509c: 42 00 ff dc  	bdnz 0x80365078 <SendSongsToMetaPerformer__17SetlistMergePanelFRCQ211stlpmtx_std45vector<i,Us,Q211stlpmtx_std15StlNodeAlloc<i>>+0x58>
803650a0: 2c 07 00 00  	cmpwi	7, 0
803650a4: 41 82 00 c0  	bt	2, 0x80365164 <SendSongsToMetaPerformer__17SetlistMergePanelFRCQ211stlpmtx_std45vector<i,Us,Q211stlpmtx_std15StlNodeAlloc<i>>+0x144>
803650a8: 81 9f 00 00  	lwz 12, 0(31)
803650ac: 7f e3 fb 78  	mr	3, 31
803650b0: 81 8c 00 1c  	lwz 12, 28(12)
803650b4: 7d 89 03 a6  	mtctr 12
803650b8: 4e 80 04 21  	bctrl
803650bc: 2c 03 00 00  	cmpwi	3, 0
803650c0: 41 82 00 94  	bt	2, 0x80365154 <SendSongsToMetaPerformer__17SetlistMergePanelFRCQ211stlpmtx_std45vector<i,Us,Q211stlpmtx_std15StlNodeAlloc<i>>+0x134>
803650c4: 3c a0 80 ba  	lis 5, -32582
803650c8: 3c c0 80 b9  	lis 6, -32583
803650cc: 7f e3 fb 78  	mr	3, 31
803650d0: 38 80 00 00  	li 4, 0
803650d4: 38 a5 a0 f8  	addi 5, 5, -24328
803650d8: 38 c6 cc b0  	addi 6, 6, -13136
803650dc: 38 e0 00 00  	li 7, 0
803650e0: 48 6d 3e 75  	bl 0x80a38f54 <__dynamic_cast>
803650e4: 2c 03 00 00  	cmpwi	3, 0
803650e8: 7c 7e 1b 78  	mr	30, 3
803650ec: 40 82 00 34  	bf	2, 0x80365120 <SendSongsToMetaPerformer__17SetlistMergePanelFRCQ211stlpmtx_std45vector<i,Us,Q211stlpmtx_std15StlNodeAlloc<i>>+0x100>
803650f0: 3c 60 80 bb  	lis 3, -32581
803650f4: 3c c0 80 ba  	lis 6, -32582
803650f8: 38 c6 2a 48  	addi 6, 6, 10824
803650fc: 80 63 28 58  	lwz 3, 10328(3)
80365100: 38 86 00 13  	addi 4, 6, 19
80365104: 38 a0 01 2f  	li 5, 303
80365108: 38 c6 01 6c  	addi 6, 6, 364
8036510c: 4b ca bf 35  	bl 0x80011040 <MakeString<PCc,i,PCc>__FPCcPCciPCc_PCc>
80365110: 3c a0 80 c9  	lis 5, -32567
80365114: 7c 64 1b 78  	mr	4, 3
80365118: 38 65 73 d8  	addi 3, 5, 29656
8036511c: 48 0b 98 a5  	bl 0x8041e9c0 <Fail__5DebugFPCc>
80365120: 80 7e 00 2c  	lwz 3, 44(30)
80365124: 38 00 00 01  	li 0, 1
80365128: 2c 03 00 06  	cmpwi	3, 6
8036512c: 41 82 00 10  	bt	2, 0x8036513c <SendSongsToMetaPerformer__17SetlistMergePanelFRCQ211stlpmtx_std45vector<i,Us,Q211stlpmtx_std15StlNodeAlloc<i>>+0x11c>
80365130: 2c 03 00 07  	cmpwi	3, 7
80365134: 41 82 00 08  	bt	2, 0x8036513c <SendSongsToMetaPerformer__17SetlistMergePanelFRCQ211stlpmtx_std45vector<i,Us,Q211stlpmtx_std15StlNodeAlloc<i>>+0x11c>
80365138: 38 00 00 00  	li 0, 0
8036513c: 2c 00 00 00  	cmpwi	0, 0
80365140: 40 82 00 14  	bf	2, 0x80365154 <SendSongsToMetaPerformer__17SetlistMergePanelFRCQ211stlpmtx_std45vector<i,Us,Q211stlpmtx_std15StlNodeAlloc<i>>+0x134>
80365144: 4b f8 79 8d  	bl 0x802ecad0 <Current__13MetaPerformerFv>
80365148: 7f c4 f3 78  	mr	4, 30
8036514c: 4b f8 88 25  	bl 0x802ed970 <SetBattle__13MetaPerformerFPC18BattleSavedSetlist>
80365150: 48 00 00 20  	b 0x80365170 <SendSongsToMetaPerformer__17SetlistMergePanelFRCQ211stlpmtx_std45vector<i,Us,Q211stlpmtx_std15StlNodeAlloc<i>>+0x150>
80365154: 4b f8 79 7d  	bl 0x802ecad0 <Current__13MetaPerformerFv>
80365158: 7f e4 fb 78  	mr	4, 31
8036515c: 4b f8 81 85  	bl 0x802ed2e0 <SetSetlist__13MetaPerformerFPC12SavedSetlist>
80365160: 48 00 00 10  	b 0x80365170 <SendSongsToMetaPerformer__17SetlistMergePanelFRCQ211stlpmtx_std45vector<i,Us,Q211stlpmtx_std15StlNodeAlloc<i>>+0x150>
80365164: 4b f8 79 6d  	bl 0x802ecad0 <Current__13MetaPerformerFv>
80365168: 7f c4 f3 78  	mr	4, 30
8036516c: 4b f8 89 f5  	bl 0x802edb60 <SetSongs__13MetaPerformerFRCQ211stlpmtx_std45vector<i,Us,Q211stlpmtx_std15StlNodeAlloc<i>>>
80365170: 80 01 00 14  	lwz 0, 20(1)
80365174: 83 e1 00 0c  	lwz 31, 12(1)
80365178: 83 c1 00 08  	lwz 30, 8(1)
8036517c: 7c 08 03 a6  	mtlr 0
80365180: 38 21 00 10  	addi 1, 1, 16
80365184: 4e 80 00 20  	blr

```
