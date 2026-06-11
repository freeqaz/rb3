# Pair 16 — verification evidence

**Claimed identity:** Wii `__ct<PCc,i>__Q211stlpmtx_std24pair<C6Symbol,8DataNode>FRCQ211stlpmtx_std11pair<PCc,i>_Pv`  ==  Xenon `0x824e51e0`

| field | value |
|---|---|
| pair_id | 16 |
| stratum | BSIM 15-20 |
| match_type | `BSIM` |
| BSim sim×conf | 16.656 |
| BSim similarity / confidence | 1.0 / 16.656 |
| TU (Wii) | `RockCentral.o` |
| Wii symbol (demangled) | `__ct<PCc,i>__Q211stlpmtx_std24pair<C6Symbol,8DataNode>FRCQ211stlpmtx_std11pair<PCc,i>_Pv` |
| Wii addr (Bank 8) | `0x803e12b0` |
| Xenon addr | `0x824e51e0` |
| Xenon func name | `Function_824E51E0` (stripped binary — name is auto-generated) |
| Wii body size | 23 asm lines (0x803e12b0 (size 0x50, llvm-objdump by-addr from bank8_target.elf)) |
| Xenon body size | 80 bytes |

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

## Referenced strings (Xenon side, 0)

_(none)_

## Xenon pseudo-C (Ghidra, fork 12.2, PowerPC:BE:64:Xenon)

```c

int Function_824E51E0(int param_1,undefined4 *param_2)

{
  __0Symbol__QAA_PBD_Z(param_1,*param_2);
  *(undefined4 *)(param_1 + 4) = param_2[1];
  *(undefined4 *)(param_1 + 8) = 1;
  return param_1;
}


```

## Wii m2c decompilation (target Bank-8 asm → C, `m2c --target ppc`)

_(m2c not available: not in dtk split (used llvm-objdump) — see raw asm below)_

## Wii target asm (Bank 8, ground-truth body)

```asm

803e12b0 <__ct<PCc,i>__Q211stlpmtx_std24pair<C6Symbol,8DataNode>FRCQ211stlpmtx_std11pair<PCc,i>_Pv>:
803e12b0: 94 21 ff f0  	stwu 1, -16(1)
803e12b4: 7c 08 02 a6  	mflr 0
803e12b8: 90 01 00 14  	stw 0, 20(1)
803e12bc: 93 e1 00 0c  	stw 31, 12(1)
803e12c0: 7c 9f 23 78  	mr	31, 4
803e12c4: 80 84 00 00  	lwz 4, 0(4)
803e12c8: 93 c1 00 08  	stw 30, 8(1)
803e12cc: 7c 7e 1b 78  	mr	30, 3
803e12d0: 48 0d be f1  	bl 0x804bd1c0 <__ct__6SymbolFPCc>
803e12d4: 80 7f 00 04  	lwz 3, 4(31)
803e12d8: 38 00 00 06  	li 0, 6
803e12dc: 90 7e 00 04  	stw 3, 4(30)
803e12e0: 7f c3 f3 78  	mr	3, 30
803e12e4: 90 1e 00 08  	stw 0, 8(30)
803e12e8: 83 e1 00 0c  	lwz 31, 12(1)
803e12ec: 83 c1 00 08  	lwz 30, 8(1)
803e12f0: 80 01 00 14  	lwz 0, 20(1)
803e12f4: 7c 08 03 a6  	mtlr 0
803e12f8: 38 21 00 10  	addi 1, 1, 16
803e12fc: 4e 80 00 20  	blr

```
