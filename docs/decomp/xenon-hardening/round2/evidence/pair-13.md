# Pair 13 — verification evidence

**Claimed identity:** Wii `clear__Q211stlpmtx_std120_List_base<Q211stlpmtx_std21pair<6Symbol,6Symbol>,Q211stlpmtx_std52StlNodeAlloc<Q211stlpmtx_std21pair<6Symbol,6Symbol>>>Fv`  ==  Xenon `0x82518de0`

| field | value |
|---|---|
| pair_id | 13 |
| stratum | BSIM 20-30 |
| match_type | `BSIM` |
| BSim sim×conf | 20.79 |
| BSim similarity / confidence | 0.917 / 22.672 |
| TU (Wii) | `AccomplishmentProgress.o` |
| Wii symbol (demangled) | `stlpmtx_std::_List_base<Q211stlpmtx_std21pair<6Symbol,6Symbol>,Q211stlpmtx_std52StlNodeAlloc<Q211stlpmtx_std21pair<6Symbol,6Symbol>>>::clear(...)` |
| Wii addr (Bank 8) | `0x80246770` |
| Xenon addr | `0x82518de0` |
| Xenon func name | `?clear@?$_List_base@VAsyncTask@@V?$StlNodeAlloc@VAsyncTask@@@stlpmtx_std@@@stlpmtx_std@@QAAXXZ` (stripped binary — name is auto-generated) |
| Wii body size | 26 asm lines (0x80246770 (size 0x5c, llvm-objdump by-addr from bank8_target.elf)) |
| Xenon body size | 88 bytes |

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
| `0x82798278` | `FUN_82798278` | `_MemOrPoolFreeSTL__Fi8PoolTypePv` |

## Referenced strings (Xenon side, 0)

_(none)_

## Xenon pseudo-C (Ghidra, fork 12.2, PowerPC:BE:64:Xenon)

```c

/* [decomp] public: void __cdecl stlpmtx_std::_List_base<class AsyncTask, class
   stlpmtx_std::StlNodeAlloc<class AsyncTask> >::clear(void) */

void _clear____List_base_VAsyncTask__V__StlNodeAlloc_VAsyncTask___stlpmtx_std___stlpmtx_std__QAAXXZ
               (undefined4 *param_1)

{
  undefined4 *puVar1;
  undefined4 *puVar2;
  
  puVar2 = (undefined4 *)*param_1;
  while (puVar2 != param_1) {
    puVar1 = (undefined4 *)*puVar2;
    FUN_82798278(0x24,puVar2);
    puVar2 = puVar1;
  }
  *param_1 = param_1;
  param_1[1] = param_1;
  return;
}


```

## Wii m2c decompilation (target Bank-8 asm → C, `m2c --target ppc`)

_(m2c not available: not in dtk split (used llvm-objdump) — see raw asm below)_

## Wii target asm (Bank 8, ground-truth body)

```asm

80246770 <clear__Q211stlpmtx_std120_List_base<Q211stlpmtx_std21pair<6Symbol,6Symbol>,Q211stlpmtx_std52StlNodeAlloc<Q211stlpmtx_std21pair<6Symbol,6Symbol>>>Fv>:
80246770: 94 21 ff f0  	stwu 1, -16(1)
80246774: 7c 08 02 a6  	mflr 0
80246778: 90 01 00 14  	stw 0, 20(1)
8024677c: 93 e1 00 0c  	stw 31, 12(1)
80246780: 93 c1 00 08  	stw 30, 8(1)
80246784: 7c 7e 1b 78  	mr	30, 3
80246788: 83 e3 00 00  	lwz 31, 0(3)
8024678c: 48 00 00 18  	b 0x802467a4 <clear__Q211stlpmtx_std120_List_base<Q211stlpmtx_std21pair<6Symbol,6Symbol>,Q211stlpmtx_std52StlNodeAlloc<Q211stlpmtx_std21pair<6Symbol,6Symbol>>>Fv+0x34>
80246790: 7f e5 fb 78  	mr	5, 31
80246794: 83 ff 00 00  	lwz 31, 0(31)
80246798: 38 60 00 10  	li 3, 16
8024679c: 38 80 00 01  	li 4, 1
802467a0: 48 25 b8 b1  	bl 0x804a2050 <_MemOrPoolFreeSTL__Fi8PoolTypePv>
802467a4: 7c 1f f0 40  	cmplw	31, 30
802467a8: 40 82 ff e8  	bf	2, 0x80246790 <clear__Q211stlpmtx_std120_List_base<Q211stlpmtx_std21pair<6Symbol,6Symbol>,Q211stlpmtx_std52StlNodeAlloc<Q211stlpmtx_std21pair<6Symbol,6Symbol>>>Fv+0x20>
802467ac: 93 de 00 00  	stw 30, 0(30)
802467b0: 93 de 00 04  	stw 30, 4(30)
802467b4: 83 e1 00 0c  	lwz 31, 12(1)
802467b8: 83 c1 00 08  	lwz 30, 8(1)
802467bc: 80 01 00 14  	lwz 0, 20(1)
802467c0: 7c 08 03 a6  	mtlr 0
802467c4: 38 21 00 10  	addi 1, 1, 16
802467c8: 4e 80 00 20  	blr

```
