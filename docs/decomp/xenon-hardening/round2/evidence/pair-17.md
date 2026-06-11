# Pair 17 — verification evidence

**Claimed identity:** Wii `Load__Q226@unnamed@MainHubPanel_cpp@17MainHubAdvanceMsgFR9BinStream`  ==  Xenon `0x82603958`

| field | value |
|---|---|
| pair_id | 17 |
| stratum | BSIM 15-20 |
| match_type | `BSIM` |
| BSim sim×conf | 16.738 |
| BSim similarity / confidence | 1.0 / 16.738 |
| TU (Wii) | `MainHubPanel.o` |
| Wii symbol (demangled) | `@unnamed@MainHubPanel_cpp@::MainHubAdvanceMsg::Load(...)` |
| Wii addr (Bank 8) | `0x802e0320` |
| Xenon addr | `0x82603958` |
| Xenon func name | `Function_82603958` (stripped binary — name is auto-generated) |
| Wii body size | 25 asm lines (0x802e0320 (size 0x58, llvm-objdump by-addr from bank8_target.elf)) |
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

## Xenon callees (2 total, 2 resolved to a matched Wii symbol)

| xenon callee addr | xenon name | resolved Wii symbol (via matches.json) |
|---|---|---|
| `0x8279ff18` | `Function_8279FF18` | `Write__9BinStreamFPCvi` |
| `0x827a0358` | `??5BinStream@@QAAAAV0@AAVString@@@Z` | `__rs__9BinStreamFR6String` |

## Referenced strings (Xenon side, 0)

_(none)_

## Xenon pseudo-C (Ghidra, fork 12.2, PowerPC:BE:64:Xenon)

```c

void Function_82603958(int param_1,undefined8 param_2)

{
  byte local_20 [8];
  
  Function_8279FF18(param_2,local_20,1);
  *(uint *)(param_1 + 4) = (uint)local_20[0];
  __5BinStream__QAAAAV0_AAVString___Z(param_2,param_1 + 8);
  return;
}


```

## Wii m2c decompilation (target Bank-8 asm → C, `m2c --target ppc`)

_(m2c not available: not in dtk split (used llvm-objdump) — see raw asm below)_

## Wii target asm (Bank 8, ground-truth body)

```asm

802e0320 <Load__Q226@unnamed@MainHubPanel_cpp@17MainHubAdvanceMsgFR9BinStream>:
802e0320: 94 21 ff e0  	stwu 1, -32(1)
802e0324: 7c 08 02 a6  	mflr 0
802e0328: 38 a0 00 01  	li 5, 1
802e032c: 90 01 00 24  	stw 0, 36(1)
802e0330: 93 e1 00 1c  	stw 31, 28(1)
802e0334: 7c 9f 23 78  	mr	31, 4
802e0338: 38 81 00 08  	addi 4, 1, 8
802e033c: 93 c1 00 18  	stw 30, 24(1)
802e0340: 7c 7e 1b 78  	mr	30, 3
802e0344: 7f e3 fb 78  	mr	3, 31
802e0348: 48 1a 8d f9  	bl 0x80489140 <Read__9BinStreamFPvi>
802e034c: 88 01 00 08  	lbz 0, 8(1)
802e0350: 7f e3 fb 78  	mr	3, 31
802e0354: 90 1e 00 04  	stw 0, 4(30)
802e0358: 38 9e 00 08  	addi 4, 30, 8
802e035c: 48 1a 8b 35  	bl 0x80488e90 <__rs__9BinStreamFR6String>
802e0360: 80 01 00 24  	lwz 0, 36(1)
802e0364: 83 e1 00 1c  	lwz 31, 28(1)
802e0368: 83 c1 00 18  	lwz 30, 24(1)
802e036c: 7c 08 03 a6  	mtlr 0
802e0370: 38 21 00 20  	addi 1, 1, 32
802e0374: 4e 80 00 20  	blr

```
