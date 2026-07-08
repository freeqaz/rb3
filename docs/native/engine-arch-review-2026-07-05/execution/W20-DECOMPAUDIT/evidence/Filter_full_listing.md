# Diff: BandCharacter::Filter(Hmx::Object*, Hmx::Object*, ObjectDir*)

- **Symbol**: `Filter__13BandCharacterFPQ23Hmx6ObjectPQ23Hmx6ObjectP9ObjectDir`
- **Demangled**: `BandCharacter::Filter(Hmx::Object*, Hmx::Object*, ObjectDir*)`
- **Unit**: `bandobj/BandCharacter`
- **Match**: 94.8% normalized (94.7% raw)
  - Fine-tuning band. Check comparison patterns (>= vs >, signed vs unsigned), casting, commutative-operand ordering, then run the permuter on any residual cascade.
- **Target Size**: 1928 bytes
- **Base Size**: 1904 bytes
- **Diff Score**: 2526 / 48200

## Instruction Summary

| Type | Count | Percent |
|------|------:|--------:|
| equal | 292 | 60.3% |
| diff_arg | 169 | 34.9% |
| diff_op | 3 | 0.6% |
| replace | 10 | 2.1% |
| delete | 8 | 1.7% |
| insert | 2 | 0.4% |
| **Total** | 484 | 100.0% |

## Region Summary

| Region | Instructions | Match % | Notes |
|--------|------------:|--------:|-------|
| 0-87 | 88 | 62% | 28 register swaps, 1 control flow, 2 addr relocation, 1 inserts, 1 deletes |
| 88-98 | 11 | 100% |  |
| 99-146 | 48 | 60% | 7 register swaps, 1 inserts, 3 deletes |
| 147-167 | 21 | 100% |  |
| 168-210 | 43 | 35% | 28 register swaps, 2 addr relocation |
| 211-224 | 14 | 100% |  |
| 225-266 | 42 | 55% | 18 register swaps, 1 addr relocation |
| 267-279 | 13 | 100% |  |
| 280-483 | 204 | 54% | 67 register swaps, 2 control flow, 1 offset swaps, 12 addr relocation, 4 deletes |

## Patterns Detected

- **LINKER_MERGED** (RarelyHandFixable): 2 call(s) to 2 merged function(s) [docs](docs/decomp/patterns/verifiable-icf.md#linker-merged-icf)
  - `ICF:_restgpr_23 (cross-function merge)`: 1 call(s)
  - `ICF:_savegpr_23 (cross-function merge)`: 1 call(s)
- **REGISTER_SWAP** (MaybeFixable): 169 instructions across 11 pairs, dominated by r23↔r24 (36 of 169) [mixed volatile+callee-saved] [docs](docs/decomp/patterns/permuter-roi.md#register-allocation-cascades)
  - r23↔r24: 36
  - r26↔r27: 28
  - r3↔r4: 26
  - ...and 8 more
- **CONTROL_FLOW** (LikelyFixable): 3 condition inversion(s) (beq↔bne) [docs](docs/decomp/patterns/fixable-control-flow.md#branch-polarity-steering-beqbne-blebge)
  - idx 62: beq vs bne (diff_op)
  - idx 356: bne vs beq (diff_op)
  - idx 419: beq vs bne (diff_op)
- **OFFSET_SWAP** (LikelyFixable): 1 swap(s) of (0x0,0x2) [docs](docs/decomp/patterns/fixable-declarations.md#offset-swap)
  - (0x0,0x2): 1 swap(s)
- **ADDRESS_RELOCATION_NOISE** (RarelyHandFixable): 21 address relocation(s), 0 lis/addi pair(s) (linker artifact — different .text layout) [docs](docs/decomp/patterns/at-limit-mwcc.md#address-relocation-noise)

**Unattributed mismatches**: 0 | **Patterns checked**: 21

## Function Call Diff

**Target only:** `_restgpr_24` (1), `_savegpr_24` (1)
**Base only:** `_restgpr_23` (1), `_savegpr_23` (1)

## Insert/Delete Clusters

| Range | Inserts | Deletes | Dominant Opcodes |
|-------|--------:|--------:|------------------|
| 353-355 | 0 | 3 | bne, cmpwi, li |

## Verdict: LikelyFixable (Medium confidence)

13 control flow difference(s) detected with low merged ratio (1.0%).

### Verdict Factors

| Factor | Value | Threshold | Result |
|--------|-------|-----------|--------|
| bool_mask_detected | false | - | not_detected |
| merged_call_ratio | 0.01 | 0.8 | below_threshold |
| control_flow_diffs | 13.00 | 1.0 | detected |

**Recommendation**: Investigate control flow structure.

### Suggestions

1. 3 condition inversion(s) (beq↔bne) ([docs](docs/decomp/patterns/fixable-control-flow.md#branch-polarity-steering-beqbne-blebge))
2. idx 62: beq vs bne (diff_op)
3. idx 356: bne vs beq (diff_op)
4. Try `> 0` vs `!= 0`, `>=` vs `>`, if/else inversion ([docs](docs/decomp/patterns/fixable-comparison.md#unsigned-zero-comparison))

### Related Documentation

- [docs/decomp/patterns/at-limit-mwcc.md#address-relocation-noise](docs/decomp/patterns/at-limit-mwcc.md#address-relocation-noise)
- [docs/decomp/patterns/at-limit-mwcc.md#linker-merged-icf](docs/decomp/patterns/at-limit-mwcc.md#linker-merged-icf)
- [docs/decomp/patterns/fixable-comparison.md#unsigned-zero-comparison](docs/decomp/patterns/fixable-comparison.md#unsigned-zero-comparison)
- [docs/decomp/patterns/fixable-control-flow.md#branch-polarity-steering-beqbne-blebge](docs/decomp/patterns/fixable-control-flow.md#branch-polarity-steering-beqbne-blebge)
- [docs/decomp/patterns/fixable-declarations.md#offset-swap](docs/decomp/patterns/fixable-declarations.md#offset-swap)
- [docs/decomp/patterns/fixable-declarations.md#variable-declaration-order](docs/decomp/patterns/fixable-declarations.md#variable-declaration-order)
- [docs/decomp/patterns/permuter-roi.md#register-allocation-cascades](docs/decomp/patterns/permuter-roi.md#register-allocation-cascades)
- [docs/decomp/patterns/permuter-roi.md#stack-slot-inversion](docs/decomp/patterns/permuter-roi.md#stack-slot-inversion)
- [docs/decomp/patterns/verifiable-icf.md#linker-merged-icf](docs/decomp/patterns/verifiable-icf.md#linker-merged-icf)

## Full Instruction Listing

| Index | Target | Base | Match |
|------:|--------|------|-------|
| 0 | `stwu r1, -0x40, r1` | `stwu r1, -0x40, r1` |  |
| 1 | `mflr r0` | `mflr r0` |  |
| 2 | `stw r0, 0x44, r1` | `stw r0, 0x44, r1` |  |
| 3 | `addi r11, r1, 0x40` | `addi r11, r1, 0x40` |  |
| 4 | `bl _savegpr_24` | `bl _savegpr_23` | diff_arg |
| 5 | `lbz r0, @GUARD@Filter__13BandCharacterFPQ23Hmx6ObjectPQ23Hmx6ObjectP9ObjectDir@meshName` | `lbz r0, @GUARD@Filter__13BandCharacterFPQ23Hmx6ObjectPQ23Hmx6ObjectP9ObjectDir@meshName` |  |
| 6 | `lis r31, @37187` | `lis r31, ...bss.0` |  |
| 7 | `mr r27, r3` | `mr r26, r3` | diff_arg |
| 8 | `mr r28, r4` | `mr r27, r4` | diff_arg |
| 9 | `extsb. r0, r0` | `extsb. r0, r0` |  |
| 10 | `mr r29, r5` | `mr r28, r5` | diff_arg |
| 11 | `mr r30, r6` | `mr r29, r6` | diff_arg |
| 12 | `addi r31, r31, @37187` | `addi r31, r31, ...bss.0` |  |
| 13 | `bne 0xcb18` | `bne 0xe524` |  |
| 14 | `lis r4, @stringBase0` | `lis r4, @stringBase0` |  |
| 15 | `lis r3, @LOCAL@Filter__13BandCharacterFPQ23Hmx6ObjectPQ23Hmx6ObjectP9ObjectDir@meshName` | - | delete |
| 16 | `addi r4, r4, @stringBase0` | `addi r3, r31, 0xf8, @LOCAL@Filter__13BandCharacterFPQ23Hmx6ObjectPQ23Hmx6ObjectP9ObjectDir@meshName` | replace |
| 17 | `addi r3, r3, @LOCAL@Filter__13BandCharacterFPQ23Hmx6ObjectPQ23Hmx6ObjectP9ObjectDir@meshName` | `addi r4, r4, @stringBase0` | diff_arg |
| 18 | `addi r4, r4, 0x870, @stringBase0` | `addi r4, r4, 0x870, @stringBase0` |  |
| 19 | `bl __ct__6SymbolFPCc` | `bl __ct__6SymbolFPCc` |  |
| 20 | `li r0, 0x1` | `li r0, 0x1` |  |
| 21 | `stb r0, @GUARD@Filter__13BandCharacterFPQ23Hmx6ObjectPQ23Hmx6ObjectP9ObjectDir@meshName` | `stb r0, @GUARD@Filter__13BandCharacterFPQ23Hmx6ObjectPQ23Hmx6ObjectP9ObjectDir@meshName` |  |
| 22 | `lwz r3, 0x540, r27` | `lwz r3, 0x540, r26` | diff_arg |
| 23 | `cmpwi r3, 0x0` | `cmpwi r3, 0x0` |  |
| 24 | `beq 0xcb28` | `beq 0xe534` |  |
| 25 | `lwz r3, 0x0, r3` | `lwz r3, 0x0, r3` |  |
| 26 | `cmplw r29, r3` | `cmplw r28, r3` | diff_arg |
| 27 | `bne 0xcb68` | `bne 0xe578` |  |
| 28 | `lis r5, __RTTI__9Character` | `lis r5, __RTTI__9Character` |  |
| 29 | `lis r6, __RTTI__Q23Hmx6Object` | `lis r6, __RTTI__Q23Hmx6Object` |  |
| 30 | `mr r3, r28` | `mr r3, r27` | diff_arg |
| 31 | `li r4, 0x0` | `li r4, 0x0` |  |
| 32 | `addi r5, r5, __RTTI__9Character` | `addi r5, r5, __RTTI__9Character` |  |
| 33 | `addi r6, r6, __RTTI__Q23Hmx6Object` | `addi r6, r6, __RTTI__Q23Hmx6Object` |  |
| 34 | `li r7, 0x0` | `li r7, 0x0` |  |
| 35 | `bl __dynamic_cast` | `bl __dynamic_cast` |  |
| 36 | `mr r4, r3` | `mr r4, r3` |  |
| 37 | `lwz r3, 0x540, r27` | `lwz r3, 0x540, r26` | diff_arg |
| 38 | `bl CopyBoundingSphere__9CharacterFP9Character` | `bl CopyBoundingSphere__9CharacterFP9Character` |  |
| 39 | `lwz r3, 0x540, r27` | `lwz r3, 0x540, r26` | diff_arg |
| 40 | `mr r4, r27` | `mr r4, r26` | diff_arg |
| 41 | `bl RepointSphereBase__9CharacterFP9ObjectDir` | `bl RepointSphereBase__9CharacterFP9ObjectDir` |  |
| 42 | - | `b 0xe5bc` | insert |
| 43 | `cmpwi r29, 0x0` | `cmpwi r28, 0x0` | diff_arg |
| 44 | `li r24, 0x0` | `li r23, 0x0` | diff_arg |
| 45 | `bne 0xcb9c` | `bne 0xe5ac` |  |
| 46 | `lwz r12, 0x0, r28` | `lwz r12, 0x0, r27` | diff_arg |
| 47 | `mr r3, r28` | `mr r3, r27` | diff_arg |
| 48 | `lwz r12, 0x18, r12` | `lwz r12, 0x18, r12` |  |
| 49 | `mtctr r12` | `mtctr r12` |  |
| 50 | `bctrl` | `bctrl` |  |
| 51 | `lis r4, AmbientOcclusion` | `lis r4, AmbientOcclusion` |  |
| 52 | `lwz r0, AmbientOcclusion, r4` | `lwz r0, AmbientOcclusion, r4` |  |
| 53 | `cmplw r3, r0` | `cmplw r3, r0` |  |
| 54 | `bne 0xcb9c` | `bne 0xe5ac` |  |
| 55 | `li r24, 0x1` | `li r23, 0x1` | diff_arg |
| 56 | `cmpwi r24, 0x0` | `cmpwi r23, 0x0` | diff_arg |
| 57 | `beq 0xcbac` | `beq 0xe5bc` |  |
| 58 | `li r3, 0x3` | `li r3, 0x3` |  |
| 59 | `b 0xd230` | `b 0xec28` |  |
| 60 | `cmpwi r29, 0x0` | `cmpwi r28, 0x0` | diff_arg |
| 61 | `li r24, 0x0` | `li r23, 0x0` | diff_arg |
| 62 | `beq 0xcbe0` | `bne 0xe5f0` | diff_op |
| 63 | `lwz r12, 0x0, r28` | `lwz r12, 0x0, r27` | diff_arg |
| 64 | `mr r3, r28` | `mr r3, r27` | diff_arg |
| 65 | `lwz r12, 0x18, r12` | `lwz r12, 0x18, r12` |  |
| 66 | `mtctr r12` | `mtctr r12` |  |
| 67 | `bctrl` | `bctrl` |  |
| 68 | `lis r4, CharWeightSetter` | `lis r4, CharWeightSetter` |  |
| 69 | `lwz r0, CharWeightSetter, r4` | `lwz r0, CharWeightSetter, r4` |  |
| 70 | `cmplw r3, r0` | `cmplw r3, r0` |  |
| 71 | `bne 0xcbe0` | `bne 0xe5f0` |  |
| 72 | `li r24, 0x1` | `li r23, 0x1` | diff_arg |
| 73 | `cmpwi r24, 0x0` | `cmpwi r23, 0x0` | diff_arg |
| 74 | `beq 0xcbf0` | `beq 0xe600` |  |
| 75 | `li r3, 0x2` | `li r3, 0x2` |  |
| 76 | `b 0xd230` | `b 0xec28` |  |
| 77 | `lwz r12, 0x0, r28` | `lwz r12, 0x0, r27` | diff_arg |
| 78 | `lis r4, @stringBase0` | `lis r4, @stringBase0` |  |
| 79 | `addi r4, r4, @stringBase0` | `addi r4, r4, @stringBase0` |  |
| 80 | `mr r3, r28` | `mr r3, r27` | diff_arg |
| 81 | `lwz r12, 0x18, r12` | `lwz r12, 0x18, r12` |  |
| 82 | `addi r24, r4, 0x4fb, @stringBase0` | `addi r23, r4, 0x4fb, @stringBase0` | diff_arg |
| 83 | `mtctr r12` | `mtctr r12` |  |
| 84 | `bctrl` | `bctrl` |  |
| 85 | `cmpwi r24, 0x0` | `cmpwi r23, 0x0` | diff_arg |
| 86 | `beq 0xcc2c` | `beq 0xe63c` |  |
| 87 | `mr r4, r24, @stringBase0` | `mr r4, r23, @stringBase0` | diff_arg |
| 88 | `bl strcmp` | `bl strcmp` |  |
| 89 | `cntlzw r0, r3` | `cntlzw r0, r3` |  |
| 90 | `srwi r0, r0, 5` | `srwi r0, r0, 5` |  |
| 91 | `b 0xcc40` | `b 0xe650` |  |
| 92 | `lis r4, gNullStr` | `lis r4, gNullStr` |  |
| 93 | `lwz r0, gNullStr, r4` | `lwz r0, gNullStr, r4` |  |
| 94 | `subf r0, r3, r0` | `subf r0, r3, r0` |  |
| 95 | `cntlzw r0, r0` | `cntlzw r0, r0` |  |
| 96 | `srwi r0, r0, 5` | `srwi r0, r0, 5` |  |
| 97 | `cmpwi r0, 0x0` | `cmpwi r0, 0x0` |  |
| 98 | `beq 0xce4c` | `beq 0xe854` |  |
| 99 | `cmpwi r29, 0x0` | `cmpwi r28, 0x0` | diff_arg |
| 100 | `beq 0xccdc` | `beq 0xe6e4` |  |
| 101 | `lbz r0, @GUARD@Filter__13BandCharacterFPQ23Hmx6ObjectPQ23Hmx6ObjectP9ObjectDir@_dw@0` | `lbz r0, @GUARD@Filter__13BandCharacterFPQ23Hmx6ObjectPQ23Hmx6ObjectP9ObjectDir@_dw@0` |  |
| 102 | `extsb. r0, r0` | `extsb. r0, r0` |  |
| 103 | `bne 0xcc98` | `bne 0xe6a4` |  |
| 104 | `lis r3, @LOCAL@Filter__13BandCharacterFPQ23Hmx6ObjectPQ23Hmx6ObjectP9ObjectDir@_dw@0` | - | delete |
| 105 | `li r0, 0x0` | `li r0, 0x0` |  |
| 106 | `addi r3, r3, @LOCAL@Filter__13BandCharacterFPQ23Hmx6ObjectPQ23Hmx6ObjectP9ObjectDir@_dw@0` | `addi r3, r31, 0x108, @LOCAL@Filter__13BandCharacterFPQ23Hmx6ObjectPQ23Hmx6ObjectP9ObjectDir@_dw@0` | replace |
| 107 | `lis r4, __dt__16DebugNotifyOncerFv` | `lis r4, __dt__16DebugNotifyOncerFv` |  |
| 108 | `stw r0, 0x8, r1` | `stw r0, 0x8, r1` |  |
| 109 | `addi r4, r4, __dt__16DebugNotifyOncerFv` | `addi r5, r31, 0xfc, @37599` | replace |
| 110 | - | `stw r0, 0xc, r1` | insert |
| 111 | `addi r5, r31, 0x70, @41045` | `addi r4, r4, __dt__16DebugNotifyOncerFv` | replace |
| 112 | `stw r0, 0xc, r1` | `stw r0, 0x10, r1` | diff_arg |
| 113 | `stw r0, 0x10, r1` | `stw r0, 0x14, r1` | diff_arg |
| 114 | `stw r0, 0x14, r1` | `stw r3, 0x0, r3, @LOCAL@Filter__13BandCharacterFPQ23Hmx6ObjectPQ23Hmx6ObjectP9ObjectDir@_dw@0` | replace |
| 115 | `stw r3, 0x0, r3, @LOCAL@Filter__13BandCharacterFPQ23Hmx6ObjectPQ23Hmx6ObjectP9ObjectDir@_dw@0` | `stw r3, 0x4, r3, @LOCAL@Filter__13BandCharacterFPQ23Hmx6ObjectPQ23Hmx6ObjectP9ObjectDir@_dw@0` | diff_arg |
| 116 | `stw r3, 0x4, r3, @LOCAL@Filter__13BandCharacterFPQ23Hmx6ObjectPQ23Hmx6ObjectP9ObjectDir@_dw@0` | - | delete |
| 117 | `bl __register_global_object` | `bl __register_global_object` |  |
| 118 | `li r0, 0x1` | `li r0, 0x1` |  |
| 119 | `stb r0, @GUARD@Filter__13BandCharacterFPQ23Hmx6ObjectPQ23Hmx6ObjectP9ObjectDir@_dw@0` | `stb r0, @GUARD@Filter__13BandCharacterFPQ23Hmx6ObjectPQ23Hmx6ObjectP9ObjectDir@_dw@0` |  |
| 120 | `mr r3, r29` | `mr r3, r28` | diff_arg |
| 121 | `bl PathName__FPCQ23Hmx6Object` | `bl PathName__FPCQ23Hmx6Object` |  |
| 122 | `lis r5, @stringBase0` | `lis r5, @stringBase0` |  |
| 123 | `mr r4, r3` | `mr r4, r3` |  |
| 124 | `addi r5, r5, @stringBase0` | `addi r5, r5, @stringBase0` |  |
| 125 | `addi r3, r5, 0x875, @stringBase0` | `addi r3, r5, 0x875, @stringBase0` |  |
| 126 | `bl MakeString<PCc>__FPCcPCc_PCc` | `bl MakeString<PCc>__FPCcPCc_PCc` |  |
| 127 | `lis r4, @LOCAL@Filter__13BandCharacterFPQ23Hmx6ObjectPQ23Hmx6ObjectP9ObjectDir@_dw@0` | - | delete |
| 128 | `mr r24, r3` | `mr r23, r3` | diff_arg |
| 129 | `addi r4, r4, @LOCAL@Filter__13BandCharacterFPQ23Hmx6ObjectPQ23Hmx6ObjectP9ObjectDir@_dw@0` | `addi r4, r31, 0x108, @LOCAL@Filter__13BandCharacterFPQ23Hmx6ObjectPQ23Hmx6ObjectP9ObjectDir@_dw@0` | replace |
| 130 | `bl AddToNotifies__27@unnamed@BandCharacter_cpp@FPCcRQ211stlpmtx_std52list<6String,Q211stlpmtx_std21StlNodeAlloc<6String>>` | `bl AddToNotifies__27@unnamed@BandCharacter_cpp@FPCcRQ211stlpmtx_std52list<6String,Q211stlpmtx_std21StlNodeAlloc<6String>>` |  |
| 131 | `cmpwi r3, 0x0` | `cmpwi r3, 0x0` |  |
| 132 | `beq 0xccdc` | `beq 0xe6e4` |  |
| 133 | `lis r3, TheDebug` | `lis r3, TheDebug` |  |
| 134 | `mr r4, r24` | `mr r4, r23` | diff_arg |
| 135 | `addi r3, r3, TheDebug` | `addi r3, r3, TheDebug` |  |
| 136 | `bl Notify__5DebugFPCc` | `bl Notify__5DebugFPCc` |  |
| 137 | `lis r5, __RTTI__12OutfitConfig` | `lis r5, __RTTI__12OutfitConfig` |  |
| 138 | `lis r6, __RTTI__Q23Hmx6Object` | `lis r6, __RTTI__Q23Hmx6Object` |  |
| 139 | `mr r3, r28` | `mr r3, r27` | diff_arg |
| 140 | `li r4, 0x0` | `li r4, 0x0` |  |
| 141 | `addi r5, r5, __RTTI__12OutfitConfig` | `addi r5, r5, __RTTI__12OutfitConfig` |  |
| 142 | `addi r6, r6, __RTTI__Q23Hmx6Object` | `addi r6, r6, __RTTI__Q23Hmx6Object` |  |
| 143 | `li r7, 0x0` | `li r7, 0x0` |  |
| 144 | `bl __dynamic_cast` | `bl __dynamic_cast` |  |
| 145 | `lwz r0, 0x63c, r27` | `lwz r0, 0x63c, r26` | diff_arg |
| 146 | `mr r25, r3` | `mr r24, r3` | diff_arg |
| 147 | `extlwi r0, r0, 9, 24` | `extlwi r0, r0, 9, 24` |  |
| 148 | `srawi. r0, r0, 24` | `srawi. r0, r0, 24` |  |
| 149 | `bne 0xcd48` | `bne 0xe750` |  |
| 150 | `cmpwi r3, 0x0` | `cmpwi r3, 0x0` |  |
| 151 | `bne 0xcd48` | `bne 0xe750` |  |
| 152 | `lis r3, kAssertStr` | `lis r3, kAssertStr` |  |
| 153 | `lis r4, @STRING@insert__37ObjPtrList<12OutfitConfig,9ObjectDir>FQ237ObjPtrList<12OutfitConfig,9ObjectDir>8iteratorP12OutfitConfig` | `lis r4, @STRING@insert__37ObjPtrList<12OutfitConfig,9ObjectDir>FQ237ObjPtrList<12OutfitConfig,9ObjectDir>8iteratorP12OutfitConfig` |  |
| 154 | `lis r6, @STRING@insert__37ObjPtrList<12OutfitConfig,9ObjectDir>FQ237ObjPtrList<12OutfitConfig,9ObjectDir>8iteratorP12OutfitConfig@0` | `lis r6, @STRING@insert__37ObjPtrList<12OutfitConfig,9ObjectDir>FQ237ObjPtrList<12OutfitConfig,9ObjectDir>8iteratorP12OutfitConfig@0` |  |
| 155 | `lwz r3, kAssertStr, r3` | `lwz r3, kAssertStr, r3` |  |
| 156 | `addi r4, r4, @STRING@insert__37ObjPtrList<12OutfitConfig,9ObjectDir>FQ237ObjPtrList<12OutfitConfig,9ObjectDir>8iteratorP12OutfitConfig` | `addi r4, r4, @STRING@insert__37ObjPtrList<12OutfitConfig,9ObjectDir>FQ237ObjPtrList<12OutfitConfig,9ObjectDir>8iteratorP12OutfitConfig` |  |
| 157 | `li r5, 0x15a` | `li r5, 0x15a` |  |
| 158 | `addi r6, r6, @STRING@insert__37ObjPtrList<12OutfitConfig,9ObjectDir>FQ237ObjPtrList<12OutfitConfig,9ObjectDir>8iteratorP12OutfitConfig@0` | `addi r6, r6, @STRING@insert__37ObjPtrList<12OutfitConfig,9ObjectDir>FQ237ObjPtrList<12OutfitConfig,9ObjectDir>8iteratorP12OutfitConfig@0` |  |
| 159 | `bl MakeString<PCc,i,PCc>__FPCcPCciPCc_PCc` | `bl MakeString<PCc,i,PCc>__FPCcPCciPCc_PCc` |  |
| 160 | `lis r5, TheDebug` | `lis r5, TheDebug` |  |
| 161 | `mr r4, r3` | `mr r4, r3` |  |
| 162 | `addi r3, r5, TheDebug` | `addi r3, r5, TheDebug` |  |
| 163 | `bl Fail__5DebugFPCc` | `bl Fail__5DebugFPCc` |  |
| 164 | `li r3, 0xc` | `li r3, 0xc` |  |
| 165 | `li r4, 0xc` | `li r4, 0xc` |  |
| 166 | `li r5, 0x1` | `li r5, 0x1` |  |
| 167 | `bl _PoolAlloc__Fii8PoolType` | `bl _PoolAlloc__Fii8PoolType` |  |
| 168 | `cmpwi r25, 0x0` | `cmpwi r24, 0x0` | diff_arg |
| 169 | `stw r25, 0x0, r3` | `stw r24, 0x0, r3` | diff_arg |
| 170 | `mr r24, r3` | `mr r23, r3` | diff_arg |
| 171 | `li r26, 0x0` | `li r30, 0x0` | diff_arg |
| 172 | `beq 0xcd7c` | `beq 0xe784` |  |
| 173 | `lwz r3, 0x0, r25` | `lwz r3, 0x0, r24` | diff_arg |
| 174 | `addi r4, r27, 0x630` | `addi r4, r26, 0x630` | diff_arg |
| 175 | `lwz r3, 0x0, r3` | `lwz r3, 0x0, r3` |  |
| 176 | `bl AddRef__Q23Hmx6ObjectFP6ObjRef` | `bl AddRef__Q23Hmx6ObjectFP6ObjRef` |  |
| 177 | `stw r26, 0x4, r24` | `stw r30, 0x4, r23` | diff_arg |
| 178 | `lwz r3, 0x634, r27` | `lwz r3, 0x634, r26` | diff_arg |
| 179 | `cmplw r26, r3` | `cmplw r30, r3` | diff_arg |
| 180 | `bne 0xcdb4` | `bne 0xe7bc` |  |
| 181 | `cmpwi r3, 0x0` | `cmpwi r3, 0x0` |  |
| 182 | `beq 0xcda8` | `beq 0xe7b0` |  |
| 183 | `lwz r0, 0x8, r3` | `lwz r0, 0x8, r3` |  |
| 184 | `stw r0, 0x8, r24` | `stw r0, 0x8, r23` | diff_arg |
| 185 | `lwz r3, 0x634, r27` | `lwz r3, 0x634, r26` | diff_arg |
| 186 | `stw r24, 0x8, r3` | `stw r23, 0x8, r3` | diff_arg |
| 187 | `b 0xcdac` | `b 0xe7b4` |  |
| 188 | `stw r24, 0x8, r24` | `stw r23, 0x8, r23` | diff_arg |
| 189 | `stw r24, 0x634, r27` | `stw r23, 0x634, r26` | diff_arg |
| 190 | `b 0xcdf0` | `b 0xe7f8` |  |
| 191 | `cmpwi r26, 0x0` | `cmpwi r30, 0x0` | diff_arg |
| 192 | `bne 0xcddc` | `bne 0xe7e4` |  |
| 193 | `lwz r0, 0x8, r3` | `lwz r0, 0x8, r3` |  |
| 194 | `stw r0, 0x8, r24` | `stw r0, 0x8, r23` | diff_arg |
| 195 | `lwz r3, 0x634, r27` | `lwz r3, 0x634, r26` | diff_arg |
| 196 | `lwz r3, 0x8, r3` | `lwz r3, 0x8, r3` |  |
| 197 | `stw r24, 0x4, r3` | `stw r23, 0x4, r3` | diff_arg |
| 198 | `lwz r3, 0x634, r27` | `lwz r3, 0x634, r26` | diff_arg |
| 199 | `stw r24, 0x8, r3` | `stw r23, 0x8, r3` | diff_arg |
| 200 | `b 0xcdf0` | `b 0xe7f8` |  |
| 201 | `lwz r0, 0x8, r26` | `lwz r0, 0x8, r30` | diff_arg |
| 202 | `stw r0, 0x8, r24` | `stw r0, 0x8, r23` | diff_arg |
| 203 | `lwz r3, 0x8, r26` | `lwz r3, 0x8, r30` | diff_arg |
| 204 | `stw r24, 0x4, r3` | `stw r23, 0x4, r3` | diff_arg |
| 205 | `stw r24, 0x8, r26` | `stw r23, 0x8, r30` | diff_arg |
| 206 | `lwz r4, 0x63c, r27` | `lwz r4, 0x63c, r26` | diff_arg |
| 207 | `lis r3, 0x80` | `lis r3, 0x80` |  |
| 208 | `subi r0, r3, 0x1` | `subi r0, r3, 0x1` |  |
| 209 | `srawi r24, r4, 8` | `srawi r23, r4, 8` | diff_arg |
| 210 | `addi r3, r24, 0x1` | `addi r3, r23, 0x1` | diff_arg |
| 211 | `cmpw r3, r0` | `cmpw r3, r0` |  |
| 212 | `blt 0xce3c` | `blt 0xe844` |  |
| 213 | `lis r3, kAssertStr` | `lis r3, kAssertStr` |  |
| 214 | `lis r4, @STRING@link__37ObjPtrList<12OutfitConfig,9ObjectDir>FQ237ObjPtrList<12OutfitConfig,9ObjectDir>8iteratorPQ237ObjPtrList<12OutfitConfig,9ObjectDir>4Node` | `lis r4, @STRING@link__37ObjPtrList<12OutfitConfig,9ObjectDir>FQ237ObjPtrList<12OutfitConfig,9ObjectDir>8iteratorPQ237ObjPtrList<12OutfitConfig,9ObjectDir>4Node` |  |
| 215 | `lis r6, @STRING@link__37ObjPtrList<12OutfitConfig,9ObjectDir>FQ237ObjPtrList<12OutfitConfig,9ObjectDir>8iteratorPQ237ObjPtrList<12OutfitConfig,9ObjectDir>4Node@0` | `lis r6, @STRING@link__37ObjPtrList<12OutfitConfig,9ObjectDir>FQ237ObjPtrList<12OutfitConfig,9ObjectDir>8iteratorPQ237ObjPtrList<12OutfitConfig,9ObjectDir>4Node@0` |  |
| 216 | `lwz r3, kAssertStr, r3` | `lwz r3, kAssertStr, r3` |  |
| 217 | `addi r4, r4, @STRING@link__37ObjPtrList<12OutfitConfig,9ObjectDir>FQ237ObjPtrList<12OutfitConfig,9ObjectDir>8iteratorPQ237ObjPtrList<12OutfitConfig,9ObjectDir>4Node` | `addi r4, r4, @STRING@link__37ObjPtrList<12OutfitConfig,9ObjectDir>FQ237ObjPtrList<12OutfitConfig,9ObjectDir>8iteratorPQ237ObjPtrList<12OutfitConfig,9ObjectDir>4Node` |  |
| 218 | `li r5, 0x244` | `li r5, 0x244` |  |
| 219 | `addi r6, r6, @STRING@link__37ObjPtrList<12OutfitConfig,9ObjectDir>FQ237ObjPtrList<12OutfitConfig,9ObjectDir>8iteratorPQ237ObjPtrList<12OutfitConfig,9ObjectDir>4Node@0` | `addi r6, r6, @STRING@link__37ObjPtrList<12OutfitConfig,9ObjectDir>FQ237ObjPtrList<12OutfitConfig,9ObjectDir>8iteratorPQ237ObjPtrList<12OutfitConfig,9ObjectDir>4Node@0` |  |
| 220 | `bl MakeString<PCc,i,PCc>__FPCcPCciPCc_PCc` | `bl MakeString<PCc,i,PCc>__FPCcPCciPCc_PCc` |  |
| 221 | `lis r5, TheDebug` | `lis r5, TheDebug` |  |
| 222 | `mr r4, r3` | `mr r4, r3` |  |
| 223 | `addi r3, r5, TheDebug` | `addi r3, r5, TheDebug` |  |
| 224 | `bl Fail__5DebugFPCc` | `bl Fail__5DebugFPCc` |  |
| 225 | `addi r3, r24, 0x1` | `addi r3, r23, 0x1` | diff_arg |
| 226 | `lwz r0, 0x63c, r27` | `lwz r0, 0x63c, r26` | diff_arg |
| 227 | `rlwimi r0, r3, 8, 0, 23` | `rlwimi r0, r3, 8, 0, 23` |  |
| 228 | `stw r0, 0x63c, r27` | `stw r0, 0x63c, r26` | diff_arg |
| 229 | `lwz r4, 0x10, r28` | `lwz r3, 0x10, r27` | diff_arg |
| 230 | `lwz r0, 0x60, r31, sCharSharedDir` | `lwz r0, 0xc, r31, sCharSharedDir` | diff_arg |
| 231 | `cmplw r4, r0` | `cmplw r3, r0` | diff_arg |
| 232 | `bne 0xcf2c` | `bne 0xe934` |  |
| 233 | `lwz r25, 0xc, r28` | `lwz r24, 0xc, r27` | diff_arg |
| 234 | `mr r3, r27` | `mr r3, r26` | diff_arg |
| 235 | `li r5, 0x0` | `li r5, 0x0` |  |
| 236 | `mr r4, r25` | `mr r4, r24` | diff_arg |
| 237 | `bl FindObject__9ObjectDirFPCcb` | `bl FindObject__9ObjectDirFPCcb` |  |
| 238 | `cmpwi r3, 0x0` | `cmpwi r3, 0x0` |  |
| 239 | `mr r24, r3` | `mr r23, r3` | diff_arg |
| 240 | `bne 0xcedc` | `bne 0xe8e4` |  |
| 241 | `cmpwi r27, 0x0` | `cmpwi r26, 0x0` | diff_arg |
| 242 | `mr r3, r27` | `mr r3, r26` | diff_arg |
| 243 | `beq 0xce8c` | `beq 0xe894` |  |
| 244 | `lwz r3, 0x0, r27` | `lwz r3, 0x0, r26` | diff_arg |
| 245 | `bl PathName__FPCQ23Hmx6Object` | `bl PathName__FPCQ23Hmx6Object` |  |
| 246 | `cmpwi r3, 0x0` | `cmpwi r3, 0x0` |  |
| 247 | `beq 0xceb4` | `beq 0xe8bc` |  |
| 248 | `cmpwi r27, 0x0` | `cmpwi r26, 0x0` | diff_arg |
| 249 | `mr r3, r27` | `mr r3, r26` | diff_arg |
| 250 | `beq 0xcea8` | `beq 0xe8b0` |  |
| 251 | `lwz r3, 0x0, r27` | `lwz r3, 0x0, r26` | diff_arg |
| 252 | `bl PathName__FPCQ23Hmx6Object` | `bl PathName__FPCQ23Hmx6Object` |  |
| 253 | `mr r5, r3` | `mr r5, r3` |  |
| 254 | `b 0xcebc` | `b 0xe8c4` |  |
| 255 | `lis r5, @STRING@Find<Q23Hmx6Object>__9ObjectDirFPCcb_PQ23Hmx6Object` | `lis r5, @STRING@Find<Q23Hmx6Object>__9ObjectDirFPCcb_PQ23Hmx6Object` |  |
| 256 | `addi r5, r5, @STRING@Find<Q23Hmx6Object>__9ObjectDirFPCcb_PQ23Hmx6Object` | `addi r5, r5, @STRING@Find<Q23Hmx6Object>__9ObjectDirFPCcb_PQ23Hmx6Object` |  |
| 257 | `lis r3, kNotObjectMsg` | `lis r3, kNotObjectMsg` |  |
| 258 | `mr r4, r25` | `mr r4, r24` | diff_arg |
| 259 | `lwz r3, kNotObjectMsg, r3` | `lwz r3, kNotObjectMsg, r3` |  |
| 260 | `bl MakeString<PCc,PCc>__FPCcPCcPCc_PCc` | `bl MakeString<PCc,PCc>__FPCcPCcPCc_PCc` |  |
| 261 | `lis r5, TheDebug` | `lis r5, TheDebug` |  |
| 262 | `mr r4, r3` | `mr r4, r3` |  |
| 263 | `addi r3, r5, TheDebug` | `addi r3, r5, TheDebug` |  |
| 264 | `bl Fail__5DebugFPCc` | `bl Fail__5DebugFPCc` |  |
| 265 | `lwz r0, 0x10, r24` | `lwz r0, 0x10, r23` | diff_arg |
| 266 | `cmplw r0, r27` | `cmplw r0, r26` | diff_arg |
| 267 | `beq 0xcf18` | `beq 0xe920` |  |
| 268 | `lis r3, kAssertStr` | `lis r3, kAssertStr` |  |
| 269 | `lis r6, @stringBase0` | `lis r6, @stringBase0` |  |
| 270 | `addi r6, r6, @stringBase0` | `addi r6, r6, @stringBase0` |  |
| 271 | `lwz r3, kAssertStr, r3` | `lwz r3, kAssertStr, r3` |  |
| 272 | `addi r4, r6, 0x1bb, @stringBase0` | `addi r4, r6, 0x1bb, @stringBase0` |  |
| 273 | `li r5, 0xab8` | `li r5, 0xab8` |  |
| 274 | `addi r6, r6, 0x88d, @stringBase0` | `addi r6, r6, 0x88d, @stringBase0` |  |
| 275 | `bl MakeString<PCc,i,PCc>__FPCcPCciPCc_PCc` | `bl MakeString<PCc,i,PCc>__FPCcPCciPCc_PCc` |  |
| 276 | `lis r5, TheDebug` | `lis r5, TheDebug` |  |
| 277 | `mr r4, r3` | `mr r4, r3` |  |
| 278 | `addi r3, r5, TheDebug` | `addi r3, r5, TheDebug` |  |
| 279 | `bl Fail__5DebugFPCc` | `bl Fail__5DebugFPCc` |  |
| 280 | `mr r3, r28` | `mr r3, r27` | diff_arg |
| 281 | `mr r4, r24` | `mr r4, r23` | diff_arg |
| 282 | `bl ReplaceRefs__FPQ23Hmx6ObjectPQ23Hmx6Object` | `bl ReplaceRefs__FPQ23Hmx6ObjectPQ23Hmx6Object` |  |
| 283 | `li r3, 0x3` | `li r3, 0x3` |  |
| 284 | `b 0xd230` | `b 0xec28` |  |
| 285 | `lwz r0, 0x64, r31, sInstrumentDir` | `lwz r0, 0x10, r31, sInstrumentDir` | diff_arg |
| 286 | `cmplw r4, r0` | `cmplw r3, r0` | diff_arg |
| 287 | `beq 0xcf44` | `beq 0xe94c` |  |
| 288 | `lwz r0, 0x68, r31, sInstResourceDir` | `lwz r0, 0x14, r31, sInstResourceDir` | diff_arg |
| 289 | `cmplw r4, r0` | `cmplw r3, r0` | diff_arg |
| 290 | `bne 0xd018` | `bne 0xea20` |  |
| 291 | `lis r24, __RTTI__16RndTransformable` | `lis r23, __RTTI__16RndTransformable` | diff_arg |
| 292 | `lis r25, __RTTI__Q23Hmx6Object` | `lis r24, __RTTI__Q23Hmx6Object` | diff_arg |
| 293 | `mr r3, r28` | `mr r3, r27` | diff_arg |
| 294 | `li r4, 0x0` | `li r4, 0x0` |  |
| 295 | `addi r5, r24, __RTTI__16RndTransformable` | `addi r5, r23, __RTTI__16RndTransformable` | diff_arg |
| 296 | `addi r6, r25, __RTTI__Q23Hmx6Object` | `addi r6, r24, __RTTI__Q23Hmx6Object` | diff_arg |
| 297 | `li r7, 0x0` | `li r7, 0x0` |  |
| 298 | `bl __dynamic_cast` | `bl __dynamic_cast` |  |
| 299 | `cmpwi r3, 0x0` | `cmpwi r3, 0x0` |  |
| 300 | `mr r26, r3` | `mr r25, r3` | diff_arg |
| 301 | `beq 0xd0ac` | `beq 0xea20` | diff_arg |
| 302 | `lwz r4, 0xc, r28` | `lwz r4, 0xc, r27` | diff_arg |
| 303 | `mr r3, r27` | `mr r3, r26` | diff_arg |
| 304 | `li r5, 0x0` | `li r5, 0x0` |  |
| 305 | `bl FindObject__9ObjectDirFPCcb` | `bl FindObject__9ObjectDirFPCcb` |  |
| 306 | `cmpwi r3, 0x0` | `cmpwi r3, 0x0` |  |
| 307 | `mr r31, r3` | `mr r30, r3` | diff_arg |
| 308 | `beq 0xd0ac` | `beq 0xea20` | diff_arg |
| 309 | `lwz r0, 0x10, r26` | `lwz r0, 0x10, r25` | diff_arg |
| 310 | `cmpwi r0, 0x0` | `cmpwi r0, 0x0` |  |
| 311 | `beq 0xd004` | `beq 0xea0c` |  |
| 312 | `addi r26, r26, 0x1c` | `addi r25, r25, 0x1c` | diff_arg |
| 313 | `addi r5, r24, __RTTI__16RndTransformable` | `addi r5, r23, __RTTI__16RndTransformable` | diff_arg |
| 314 | `addi r6, r25, __RTTI__Q23Hmx6Object` | `addi r6, r24, __RTTI__Q23Hmx6Object` | diff_arg |
| 315 | `li r4, 0x0` | `li r4, 0x0` |  |
| 316 | `li r7, 0x0` | `li r7, 0x0` |  |
| 317 | `bl __dynamic_cast` | `bl __dynamic_cast` |  |
| 318 | `psq_lx f0, r0, r26, 0, qr0` | `psq_lx f0, r0, r25, 0, qr0` | diff_arg |
| 319 | `psq_st f0, 0x1c, r3, 0, qr0` | `psq_st f0, 0x1c, r3, 0, qr0` |  |
| 320 | `lfs f0, 0x8, r26` | `lfs f0, 0x8, r25` | diff_arg |
| 321 | `stfs f0, 0x24, r3` | `stfs f0, 0x24, r3` |  |
| 322 | `psq_l f0, 0xc, r26, 0, qr0` | `psq_l f0, 0xc, r25, 0, qr0` | diff_arg |
| 323 | `psq_st f0, 0x28, r3, 0, qr0` | `psq_st f0, 0x28, r3, 0, qr0` |  |
| 324 | `lfs f0, 0x14, r26` | `lfs f0, 0x14, r25` | diff_arg |
| 325 | `stfs f0, 0x30, r3` | `stfs f0, 0x30, r3` |  |
| 326 | `psq_l f0, 0x18, r26, 0, qr0` | `psq_l f0, 0x18, r25, 0, qr0` | diff_arg |
| 327 | `psq_st f0, 0x34, r3, 0, qr0` | `psq_st f0, 0x34, r3, 0, qr0` |  |
| 328 | `lfs f0, 0x20, r26` | `lfs f0, 0x20, r25` | diff_arg |
| 329 | `stfs f0, 0x3c, r3` | `stfs f0, 0x3c, r3` |  |
| 330 | `psq_l f0, 0x24, r26, 0, qr0` | `psq_l f0, 0x24, r25, 0, qr0` | diff_arg |
| 331 | `psq_st f0, 0x40, r3, 0, qr0` | `psq_st f0, 0x40, r3, 0, qr0` |  |
| 332 | `lfs f0, 0x2c, r26` | `lfs f0, 0x2c, r25` | diff_arg |
| 333 | `stfs f0, 0x48, r3` | `stfs f0, 0x48, r3` |  |
| 334 | `lwz r3, 0x7c, r3` | `lwz r3, 0x7c, r3` |  |
| 335 | `lwz r0, 0x8, r3` | `lwz r0, 0x8, r3` |  |
| 336 | `clrlwi. r0, r0, 31` | `clrlwi. r0, r0, 31` |  |
| 337 | `bne 0xd004` | `bne 0xea0c` |  |
| 338 | `bl SetDirty_Force__10DirtyCacheFv` | `bl SetDirty_Force__10DirtyCacheFv` |  |
| 339 | `mr r3, r28` | `mr r3, r27` | diff_arg |
| 340 | `mr r4, r31` | `mr r4, r30` | diff_arg |
| 341 | `bl ReplaceRefs__FPQ23Hmx6ObjectPQ23Hmx6Object` | `bl ReplaceRefs__FPQ23Hmx6ObjectPQ23Hmx6Object` |  |
| 342 | `li r3, 0x3` | `li r3, 0x3` |  |
| 343 | `b 0xd230` | `b 0xec28` |  |
| 344 | `lwz r0, 0x58, r31, sOutfitDir` | `lwz r3, 0x10, r27` | replace |
| 345 | `li r3, 0x0` | `lwz r0, 0x4, r31, sOutfitDir` | replace |
| 346 | `cmplw r4, r0` | `cmplw r3, r0` | diff_arg |
| 347 | `beq 0xd040` | `beq 0xeaa8` | diff_arg |
| 348 | `lwz r0, 0x5c, r31, sResourceDir` | `lwz r0, 0x8, r31, sResourceDir` | diff_arg |
| 349 | `cmplw r4, r0` | `cmplw r3, r0` | diff_arg |
| 350 | `beq 0xd040` | `beq 0xeaa8` | diff_arg |
| 351 | `lwz r0, 0x6c, r31, sToDir` | `lwz r0, 0x18, r31, sToDir` | diff_arg |
| 352 | `cmplw r4, r0` | `cmplw r3, r0` | diff_arg |
| 353 | `bne 0xd044` | - | delete |
| 354 | `li r3, 0x1` | - | delete |
| 355 | `cmpwi r3, 0x0` | - | delete |
| 356 | `bne 0xd0ac` | `beq 0xeaa8` | diff_op |
| 357 | `lwz r0, 0x54, r31, sBoneMergeDir` | `lwz r0, 0x0, r31, sBoneMergeDir` | diff_arg |
| 358 | `cmplw r4, r0` | `cmplw r3, r0` | diff_arg |
| 359 | `bne 0xd0a4` | `bne 0xeaa0` |  |
| 360 | `lis r5, __RTTI__16RndTransformable` | `lis r5, __RTTI__16RndTransformable` |  |
| 361 | `lis r6, __RTTI__Q23Hmx6Object` | `lis r6, __RTTI__Q23Hmx6Object` |  |
| 362 | `mr r3, r28` | `mr r3, r27` | diff_arg |
| 363 | `li r4, 0x0` | `li r4, 0x0` |  |
| 364 | `addi r5, r5, __RTTI__16RndTransformable` | `addi r5, r5, __RTTI__16RndTransformable` |  |
| 365 | `addi r6, r6, __RTTI__Q23Hmx6Object` | `addi r6, r6, __RTTI__Q23Hmx6Object` |  |
| 366 | `li r7, 0x0` | `li r7, 0x0` |  |
| 367 | `bl __dynamic_cast` | `bl __dynamic_cast` |  |
| 368 | `cmpwi r3, 0x0` | `cmpwi r3, 0x0` |  |
| 369 | `beq 0xd0a4` | `beq 0xeaa0` |  |
| 370 | `lwz r4, 0xc, r28` | `lwz r4, 0xc, r27` | diff_arg |
| 371 | `mr r3, r27` | `mr r3, r26` | diff_arg |
| 372 | `li r5, 0x0` | `li r5, 0x0` |  |
| 373 | `bl FindObject__9ObjectDirFPCcb` | `bl FindObject__9ObjectDirFPCcb` |  |
| 374 | `cmpwi r3, 0x0` | `cmpwi r3, 0x0` |  |
| 375 | `mr r4, r3` | `mr r4, r3` |  |
| 376 | `beq 0xd0a4` | `beq 0xeaa0` |  |
| 377 | `mr r3, r28` | `mr r3, r27` | diff_arg |
| 378 | `bl ReplaceRefs__FPQ23Hmx6ObjectPQ23Hmx6Object` | `bl ReplaceRefs__FPQ23Hmx6ObjectPQ23Hmx6Object` |  |
| 379 | `li r3, 0x3` | `li r3, 0x3` |  |
| 380 | `b 0xd230` | `b 0xec28` |  |
| 381 | `lis r31, @stringBase0` | `lis r25, @stringBase0` | diff_arg |
| 382 | `lwz r4, 0xc, r28` | `lwz r3, 0xc, r27` | diff_arg |
| 383 | `addi r31, r31, @stringBase0` | `addi r25, r25, @stringBase0` | diff_arg |
| 384 | `li r5, 0x5` | `li r5, 0x5` |  |
| 385 | `addi r3, r31, 0x8a1, @stringBase0` | `addi r4, r25, 0x8a1, @stringBase0` | diff_arg |
| 386 | `bl strnicmp` | `bl strnicmp` |  |
| 387 | `cmpwi r3, 0x0` | `cmpwi r3, 0x0` |  |
| 388 | `bne 0xd158` | `bne 0xeb54` |  |
| 389 | `lis r5, __RTTI__16RndTransformable` | `lis r5, __RTTI__16RndTransformable` |  |
| 390 | `lis r6, __RTTI__Q23Hmx6Object` | `lis r6, __RTTI__Q23Hmx6Object` |  |
| 391 | `mr r3, r28` | `mr r3, r27` | diff_arg |
| 392 | `li r4, 0x0` | `li r4, 0x0` |  |
| 393 | `addi r5, r5, __RTTI__16RndTransformable` | `addi r5, r5, __RTTI__16RndTransformable` |  |
| 394 | `addi r6, r6, __RTTI__Q23Hmx6Object` | `addi r6, r6, __RTTI__Q23Hmx6Object` |  |
| 395 | `li r7, 0x0` | `li r7, 0x0` |  |
| 396 | `bl __dynamic_cast` | `bl __dynamic_cast` |  |
| 397 | `cmpwi r3, 0x0` | `cmpwi r3, 0x0` |  |
| 398 | `mr r26, r3` | `mr r30, r3` | diff_arg |
| 399 | `beq 0xd158` | `beq 0xeb54` |  |
| 400 | `lwz r3, 0x10, r3` | `lwz r3, 0x10, r3` |  |
| 401 | `cmpwi r3, 0x0` | `cmpwi r3, 0x0` |  |
| 402 | `beq 0xd148` | `beq 0xeb4c` | diff_arg |
| 403 | `lwz r4, 0x0, r3` | `lwz r3, 0x0, r3` | diff_arg |
| 404 | `addi r3, r31, 0x8a1, @stringBase0` | `addi r4, r25, 0x8a1, @stringBase0` | diff_arg |
| 405 | `li r5, 0x5` | `li r5, 0x5` |  |
| 406 | `lwz r4, 0x0, r4` | `lwz r3, 0x0, r3` | diff_arg |
| 407 | `lwz r4, 0xc, r4` | `lwz r3, 0xc, r3` | diff_arg |
| 408 | `bl strnicmp` | `bl strnicmp` |  |
| 409 | `cmpwi r3, 0x0` | `cmpwi r3, 0x0` |  |
| 410 | `beq 0xd150` | `beq 0xeb44` | diff_arg |
| 411 | `lwz r4, 0x10, r26` | `lwz r3, 0x10, r30` | diff_arg |
| 412 | `addi r3, r31, 0x8a7, @stringBase0` | `addi r4, r25, 0x8a7, @stringBase0` | diff_arg |
| 413 | `li r5, 0x4` | `li r5, 0x4` |  |
| 414 | `lwz r4, 0x0, r4` | `lwz r3, 0x0, r3` | diff_arg |
| 415 | `lwz r4, 0x0, r4` | `lwz r3, 0x0, r3` | diff_arg |
| 416 | `lwz r4, 0xc, r4` | `lwz r3, 0xc, r3` | diff_arg |
| 417 | `bl strnicmp` | `bl strnicmp` |  |
| 418 | `cmpwi r3, 0x0` | `cmpwi r3, 0x0` |  |
| 419 | `beq 0xd150` | `bne 0xeb4c` | diff_op |
| 420 | `li r3, 0x2` | `li r3, 0x0` | diff_arg |
| 421 | `b 0xd230` | `b 0xec28` |  |
| 422 | `li r3, 0x0` | `li r3, 0x2` | diff_arg |
| 423 | `b 0xd230` | `b 0xec28` |  |
| 424 | `lwz r3, 0x548, r27` | `lwz r3, 0x548, r26` | diff_arg |
| 425 | `mr r4, r28` | `mr r4, r27` | diff_arg |
| 426 | `mr r5, r29` | `mr r5, r28` | diff_arg |
| 427 | `mr r6, r30` | `mr r6, r29` | diff_arg |
| 428 | `bl MergeAction__10FileMergerFPQ23Hmx6ObjectPQ23Hmx6ObjectP9ObjectDir` | `bl MergeAction__10FileMergerFPQ23Hmx6ObjectPQ23Hmx6ObjectP9ObjectDir` |  |
| 429 | `lis r5, @F_000080bf` | `lis r5, @F_000080bf` |  |
| 430 | `lis r4, sDrawOrder` | `lis r4, sDrawOrder` |  |
| 431 | `lfs f1, @F_000080bf, r5` | `lfs f1, @F_000080bf, r5` |  |
| 432 | `mr r25, r3` | `mr r24, r3` | diff_arg |
| 433 | `lfs f0, sDrawOrder, r4` | `lfs f0, sDrawOrder, r4` |  |
| 434 | `li r24, 0x0` | `li r23, 0x0` | diff_arg |
| 435 | `fcmpu cr0, f1, f0` | `fcmpu cr0, f1, f0` |  |
| 436 | `beq 0xd1b4` | `beq 0xebac` |  |
| 437 | `lwz r12, 0x0, r28` | `lwz r12, 0x0, r27` | diff_arg |
| 438 | `mr r3, r28` | `mr r3, r27` | diff_arg |
| 439 | `lwz r12, 0x18, r12` | `lwz r12, 0x18, r12` |  |
| 440 | `mtctr r12` | `mtctr r12` |  |
| 441 | `bctrl` | `bctrl` |  |
| 442 | `lis r4, @LOCAL@Filter__13BandCharacterFPQ23Hmx6ObjectPQ23Hmx6ObjectP9ObjectDir@meshName` | - | delete |
| 443 | `lwz r0, @LOCAL@Filter__13BandCharacterFPQ23Hmx6ObjectPQ23Hmx6ObjectP9ObjectDir@meshName, r4` | `lwz r0, 0xf8, r31, @LOCAL@Filter__13BandCharacterFPQ23Hmx6ObjectPQ23Hmx6ObjectP9ObjectDir@meshName` | replace |
| 444 | `cmplw r3, r0` | `cmplw r3, r0` |  |
| 445 | `bne 0xd1b4` | `bne 0xebac` |  |
| 446 | `li r24, 0x1` | `li r23, 0x1` | diff_arg |
| 447 | `cmpwi r24, 0x0` | `cmpwi r23, 0x0` | diff_arg |
| 448 | `beq 0xd208` | `beq 0xec00` |  |
| 449 | `lis r5, __RTTI__7RndMesh` | `lis r5, __RTTI__7RndMesh` |  |
| 450 | `lis r6, __RTTI__Q23Hmx6Object` | `lis r6, __RTTI__Q23Hmx6Object` |  |
| 451 | `mr r3, r28` | `mr r3, r27` | diff_arg |
| 452 | `li r4, 0x0` | `li r4, 0x0` |  |
| 453 | `addi r5, r5, __RTTI__7RndMesh` | `addi r5, r5, __RTTI__7RndMesh` |  |
| 454 | `addi r6, r6, __RTTI__Q23Hmx6Object` | `addi r6, r6, __RTTI__Q23Hmx6Object` |  |
| 455 | `li r7, 0x0` | `li r7, 0x0` |  |
| 456 | `bl __dynamic_cast` | `bl __dynamic_cast` |  |
| 457 | `lis r4, @F_00000000` | `lis r4, @F_00000000` |  |
| 458 | `lfs f0, 0x1c, r3` | `lfs f1, 0x1c, r3` | diff_arg |
| 459 | `lfs f1, @F_00000000, r4` | `lfs f0, @F_00000000, r4` | diff_arg |
| 460 | `fcmpu cr0, f1, f0` | `fcmpu cr0, f0, f1` | diff_arg |
| 461 | `bne 0xd208` | `bne 0xec00` |  |
| 462 | `lis r5, @F_0000a040` | `lis r5, @F_0000a040` |  |
| 463 | `lis r4, sDrawOrder` | `lis r4, sDrawOrder` |  |
| 464 | `lfs f1, @F_0000a040, r5` | `lfs f1, @F_0000a040, r5` |  |
| 465 | `lfs f0, sDrawOrder, r4` | `lfs f0, sDrawOrder, r4` |  |
| 466 | `fadds f0, f1, f0` | `fadds f0, f1, f0` |  |
| 467 | `stfs f0, 0x1c, r3` | `stfs f0, 0x1c, r3` |  |
| 468 | `cmpwi r29, 0x0` | `cmpwi r28, 0x0` | diff_arg |
| 469 | `bne 0xd22c` | `bne 0xec24` |  |
| 470 | `cmplw r30, r27` | `cmplw r29, r26` | diff_arg |
| 471 | `beq 0xd22c` | `beq 0xec24` |  |
| 472 | `cmplwi r25, 0x1` | `cmpwi r24, 0x1` | replace |
| 473 | `bgt 0xd22c` | `bgt 0xec24` |  |
| 474 | `mr r3, r27` | `mr r3, r26` | diff_arg |
| 475 | `mr r4, r28` | `mr r4, r27` | diff_arg |
| 476 | `bl AddObject__13BandCharacterFPQ23Hmx6Object` | `bl AddObject__13BandCharacterFPQ23Hmx6Object` |  |
| 477 | `mr r3, r25` | `mr r3, r24` | diff_arg |
| 478 | `addi r11, r1, 0x40` | `addi r11, r1, 0x40` |  |
| 479 | `bl _restgpr_24` | `bl _restgpr_23` | diff_arg |
| 480 | `lwz r0, 0x44, r1` | `lwz r0, 0x44, r1` |  |
| 481 | `mtlr r0` | `mtlr r0` |  |
| 482 | `addi r1, r1, 0x40` | `addi r1, r1, 0x40` |  |
| 483 | `blr` | `blr` |  |


## Key Mismatches

- [4] diff_arg: `bl _savegpr_24` vs `bl _savegpr_23`
- [7] diff_arg: `mr r27, r3` vs `mr r26, r3`
- [8] diff_arg: `mr r28, r4` vs `mr r27, r4`
- [10] diff_arg: `mr r29, r5` vs `mr r28, r5`
- [11] diff_arg: `mr r30, r6` vs `mr r29, r6`
- [15] delete: `lis r3, @LOCAL@Filter__13BandCharacterFPQ23Hmx6ObjectPQ23Hmx6ObjectP9ObjectDir@meshName` vs `---`
- [16] replace: `addi r4, r4, @stringBase0` vs `addi r3, r31, 0xf8, @LOCAL@Filter__13BandCharacterFPQ23Hmx6ObjectPQ23Hmx6ObjectP9ObjectDir@meshName`
- [17] diff_arg: `addi r3, r3, @LOCAL@Filter__13BandCharacterFPQ23Hmx6ObjectPQ23Hmx6ObjectP9ObjectDir@meshName` vs `addi r4, r4, @stringBase0`
- [22] diff_arg: `lwz r3, 0x540, r27` vs `lwz r3, 0x540, r26`
- [26] diff_arg: `cmplw r29, r3` vs `cmplw r28, r3`
- [30] diff_arg: `mr r3, r28` vs `mr r3, r27`
- [37] diff_arg: `lwz r3, 0x540, r27` vs `lwz r3, 0x540, r26`
- [39] diff_arg: `lwz r3, 0x540, r27` vs `lwz r3, 0x540, r26`
- [40] diff_arg: `mr r4, r27` vs `mr r4, r26`
- [42] insert: `---` vs `b 0xe5bc`

*(177 more mismatches not shown)*

## Auto-Diagnosis

======================================================================
DIAGNOSIS REPORT
======================================================================

Total instructions: 484
Match estimate:     ~60.3% (292/484 equal)

Instruction breakdown:
  equal       :   292 ( 60.3%)
  diff_arg    :   169 ( 34.9%)
  replace     :    10 (  2.1%)
  delete      :     8 (  1.7%)
  diff_op     :     3 (  0.6%)
  insert      :     2 (  0.4%)

----------------------------------------------------------------------
ROOT CAUSES
----------------------------------------------------------------------

  Stack/offset shift: dominant delta = -84 (6 instructions)
  Top offset deltas:
       -84:    6 instructions
        +4:    3 instructions
        -2:    1 instructions
        +2:    1 instructions

  Register swaps: 171 instructions across 12 pairs
  Top swap pairs:
    r23  <-> r24 :   36 (idx 44-447) [GPR]
    r26  <-> r27 :   28 (idx 7-474) [GPR]
    r3   <-> r4  :   26 (idx 17-416) [GPR]
    r27  <-> r28 :   25 (idx 8-475) [GPR]
    r24  <-> r25 :   12 (idx 146-477) [GPR]
    r25  <-> r26 :   12 (idx 300-332) [GPR]
    r26  <-> r30 :    9 (idx 171-411) [GPR]
    r28  <-> r29 :    8 (idx 10-468) [GPR]
    r25  <-> r31 :    6 (idx 381-412) [GPR]
    f0   <-> f1  :    4 (idx 458-460) [FPR]
    r29  <-> r30 :    3 (idx 11-470) [GPR]
    r30  <-> r31 :    2 (idx 307-340) [GPR]

  Symbol relocations: 3 arg differences
    Across 3 instructions

  Branch destination diffs: 6 (address relocation noise)

----------------------------------------------------------------------
ACTIONABLE MISMATCHES
----------------------------------------------------------------------

  diff_op (opcode mismatches): 3
    idx   62: TGT beq        0xcbe0
             SRC bne        0xe5f0
    idx  356: TGT bne        0xd0ac
             SRC beq        0xeaa8
    idx  419: TGT beq        0xd150
             SRC bne        0xeb4c

  insert/delete: 10 instructions in 8 clusters
    cluster 1: idx 15-15 (1 instrs: 0I/1D)
    cluster 2: idx 42-42 (1 instrs: 1I/0D)
    cluster 3: idx 104-104 (1 instrs: 0I/1D)
    cluster 4: idx 110-110 (1 instrs: 1I/0D)
    cluster 5: idx 116-116 (1 instrs: 0I/1D)
    cluster 6: idx 127-127 (1 instrs: 0I/1D)
    cluster 7: idx 353-355 (3 instrs: 0I/3D)
    cluster 8: idx 442-442 (1 instrs: 0I/1D)

  replace: 10 instructions (1 symbol-reloc noise, 9 real)
    idx   16: TGT addi     r4, r4, @stringBase0
             SRC addi     r3, r31, 0xf8, @LOCAL@Filter__13BandCharacterFPQ23Hmx6ObjectPQ23Hmx6ObjectP9ObjectDir@meshName
    idx  106: TGT addi     r3, r3, @LOCAL@Filter__13BandCharacterFPQ23Hmx6ObjectPQ23Hmx6ObjectP9ObjectDir@_dw@0
             SRC addi     r3, r31, 0x108, @LOCAL@Filter__13BandCharacterFPQ23Hmx6ObjectPQ23Hmx6ObjectP9ObjectDir@_dw@0
    idx  109: TGT addi     r4, r4, __dt__16DebugNotifyOncerFv
             SRC addi     r5, r31, 0xfc, @37599
    idx  111: TGT addi     r5, r31, 0x70, @41045
             SRC addi     r4, r4, __dt__16DebugNotifyOncerFv
    idx  129: TGT addi     r4, r4, @LOCAL@Filter__13BandCharacterFPQ23Hmx6ObjectPQ23Hmx6ObjectP9ObjectDir@_dw@0
             SRC addi     r4, r31, 0x108, @LOCAL@Filter__13BandCharacterFPQ23Hmx6ObjectPQ23Hmx6ObjectP9ObjectDir@_dw@0
    idx  344: TGT lwz      r0, 0x58, r31, sOutfitDir
             SRC lwz      r3, 0x10, r27
    idx  345: TGT li       r3, 0x0
             SRC lwz      r0, 0x4, r31, sOutfitDir
    idx  443: TGT lwz      r0, @LOCAL@Filter__13BandCharacterFPQ23Hmx6ObjectPQ23Hmx6ObjectP9ObjectDir@meshName, r4
             SRC lwz      r0, 0xf8, r31, @LOCAL@Filter__13BandCharacterFPQ23Hmx6ObjectPQ23Hmx6ObjectP9ObjectDir@meshName
    ... and 1 more real replaces

----------------------------------------------------------------------
NOISE BUDGET
----------------------------------------------------------------------

  diff_arg instructions: 169
    Explained by root causes: 169
      Offset shifts:     11 arg diffs
      Register swaps:    171 arg diffs
      Symbol relocs:     3 arg diffs
      Branch dests:      6 arg diffs
    Unexplained:         0

  Other non-equal: 23
    diff_op:   3
    replace:   10
    insert:    2
    delete:    8

[stderr]
objdiff: resolved unit `bandobj/BandCharacter` -> `main/system/bandobj/BandCharacter`
Building incremental: build/SZBE69_B8/src/system/bandobj/BandCharacter.o