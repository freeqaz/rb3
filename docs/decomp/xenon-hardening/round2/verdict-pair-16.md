# Pair 16 verdict — WRONG (sibling-template misattribution)

**Claimed identity:** Wii `__ct<PCc,i>__Q211stlpmtx_std24pair<C6Symbol,8DataNode>FRCQ211stlpmtx_std11pair<PCc,i>_Pv` (0x803e12b0) ↔ Xenon 0x824e51e0
**Match:** BSIM, similarity 1.0, sim×conf 16.656
**Verdict:** **wrong** (confidence: high)

## TL;DR
The Xenon function IS a `pair<Symbol,DataNode>` converting constructor from RockCentral.o
— same source *family* — but it is the **float** template instantiation
`__ct<PCc,f>` (Wii 0x803e6740), **not** the claimed **int** instantiation
`__ct<PCc,i>` (Wii 0x803e12b0). The two Wii siblings are byte-near-identical;
BSim (similarity 1.0) cannot tell them apart and picked the wrong one. The
disambiguating immediate constant — the `DataNode::mType` tag written at `this+8`
— is **`1` (kDataFloat)** on the Xenon side, which matches the float sibling.
The claimed int sibling writes **`6` (kDataInt)**.

## The decisive evidence: the DataNode type-tag constant

`enum DataType` (src/system/obj/Data.h:21): `kDataFloat = 1`, `kDataInt = 6`.

The `pair<Symbol,DataNode>` layout is: `+0` Symbol (char*), `+4` DataNode.mValue,
`+8` DataNode.mType. Every one of these converting ctors interns the first
(`Symbol::Symbol(const char*)`), copies the second value to `+4`, and writes the
DataNode type tag to `+8`. The tag is the ONLY thing distinguishing the siblings.

| | Wii `<PCc,i>` (claimed, 0x803e12b0) | Wii `<PCc,f>` (sibling, 0x803e6740) | **Xenon 0x824e51e0** |
|---|---|---|---|
| Symbol ctor | `bl __ct__6SymbolFPCc` | `bl __ct__6SymbolFPCc` | `??0Symbol@@QAA@PBD@Z` (Symbol::Symbol(char const*)) |
| value at +4 | `lwz 3,4(31)` / `stw 3,4(30)` (int) | `lfs 0,4(31)` / `stfs 0,4(30)` (float) | `*(u32*)(p1+4)=p2[1]` (Ghidra can't type int-vs-float) |
| **type tag at +8** | `li 0,6` → **stw 6 = kDataInt** | `li 0,1` → **stw 1 = kDataFloat** | `*(u32*)(p1+8) = 1` → **kDataFloat** |

Xenon writes **1 (kDataFloat)** → matches the **float** sibling, contradicts the
claimed **int** sibling (which writes 6).

## How the two Wii siblings differ (both in RockCentral.o, both size 0x50)
- `0x803e12b0 __ct<PCc,i>` (CLAIMED): `lwz 3,4(31)` + `li 0,6` + `stw` — int path, kDataInt.
- `0x803e6740 __ct<PCc,f>` (ACTUAL): `lfs 0,4(31)` + `li 0,1` + `stfs`/`stw` — float path, kDataFloat.
Confirmed by `llvm-objdump -d build/SZBE69_B8/ghidra/bank8_target.elf` over both addresses.
Both are real Bank-8 functions in `orig/SZBE69_B8/files/band_r_wii.map` (a whole
cluster of `<PCc,i>`, `<PCc,f>`, `<PCc,6Symbol>`, `<6Symbol,i>`, … instantiations exist).

## Why BSim got it wrong (not a pipeline bug, an intrinsic ambiguity)
The int and float siblings differ by exactly two things: an immediate (6 vs 1) and
load/store opcode flavor (lwz/stw vs lfs/stfs). BSim's feature vector treats these
as near-identical (similarity reported as 1.0). With two equally-good Wii
candidates, BSim has no way to pick the int over the float; here it picked int and
was wrong. The match is "right family, wrong instantiation."

## Caveats considered and rejected
- **"Ghidra typed +4 as int, so maybe the value is int after all."** No —
  Ghidra/MSVC copies the union word generically and cannot recover the source
  float type from a 4-byte move; the int-typed pseudo-C at +4 is not evidence.
  The load-bearing signal is the *type-tag immediate* at +8, which is an explicit
  constant store: `1`, not `6`.
- **Callee agreement doesn't disambiguate.** Both siblings call
  `Symbol::Symbol(const char*)`; the Xenon callee `??0Symbol@@QAA@PBD@Z` is that
  ctor (the evidence pack's resolution of it to `Enter__11RndPollableFv` via
  matches.json is spurious noise and irrelevant here).
- **Field-offset/toolchain differences** are not in play — the struct offsets
  (+4 value, +8 tag) agree across both sides; only the constant differs.

## For the next agent
- **This is a sibling-aliasing failure mode worth surfacing to the eval owner.**
  RockCentral.o contains a dense cluster of `pair<Symbol,DataNode>` converting
  ctors that differ only in the second template arg → only in one immediate +
  load flavor. BSim similarity-1.0 picks among them blind. A cheap guard: when a
  BSim ACCEPT's Wii symbol has same-size same-TU siblings, disambiguate by the
  distinctive immediate constants in the body (here the DataNode type tag).
- The CORRECT Wii partner for Xenon 0x824e51e0 is almost certainly
  `__ct<PCc,f>` at **0x803e6740** (kDataFloat=1). If the pipeline wants a salvage
  rather than a reject, re-point this identity to that symbol.
- No Ghidra/JVM launched; verdict rests entirely on the evidence pack + two
  llvm-objdump reads against the already-built bank8_target.elf.
