# Wave-20 Lane N — BandCharacter::Filter remap branch hit-count table

Counted (lint 8): every "0" below is a COUNTED zero from `RB3_LOADBIND_PROBE=1`, not an
absent log line. Counters are cumulative per member, printed at every `OnInstallFilter`
(`LOADBIND_COUNTERS afterInstall`). Boot: `keyboard-to-gameplay.py --diff hard`,
RB3_FIXED_CLOCK=1, reached game_screen. Final per-member values (last afterInstall row).

Branches (src/system/bandobj/BandCharacter.cpp):
- **br0** `sCharSharedDir` outer-if entered — `:4182` (`if (o1->Dir() == sCharSharedDir)`, ReplaceRefs `:4185`)
- **br1** `sInstrumentDir`/`sInstResourceDir` ReplaceRefs fired — `:4188` (found!=0, ReplaceRefs `:4196`)
- **br2** `sBoneMergeDir` outer-if entered — `:4202` (`if (o1->Dir() == sBoneMergeDir)`)
- **br3** `sBoneMergeDir` ReplaceRefs actually fired — `:4207` (found!=0)

## CONTROL — shim ON (shipped default, RB3_LOADBIND_NOSHIM unset)

| member | calls | br0 charShared | br1 instrument | br2 boneMerge-entered | br3 boneMerge-replaced |
|---|---|---|---|---|---|
| player0 | 1377 | **0** | 192 | **0** | **0** |
| player1 (female) | 1358 | **0** | 191 | **0** | **0** |
| player2 | 1386 | **0** | 192 | **0** | **0** |
| player3 | 1367 | **0** | 191 | **0** | **0** |
| main | 510 | **0** | 0 | **0** | **0** |

**br2 = br3 = 0 across all members.** The `:4202` sBoneMergeDir ReplaceRefs branch is
**NEVER-FIRING on the shipped native build** — VERDICT §1's uninstrumented "never-firing
sBoneMergeDir ReplaceRefs remap" claim is now a COUNTED zero. br0 (sCharSharedDir) is also
0. Only br1 (instrument remap) fires under the shim.

## NOSHIM — shim OFF (retail kMerge, RB3_LOADBIND_NOSHIM=1) — reconciliation arm

| member | calls | br0 charShared | br1 instrument | br2 boneMerge-entered | br3 boneMerge-replaced |
|---|---|---|---|---|---|
| player0 | 52102 | 320 | 281 | **31488** | **31488** |
| player1 (female) | 51406 | 320 | 191 | **31488** | **31488** |
| player2 | 52452 | 320 | 333 | **31488** | **31488** |
| player3 | 51748 | 320 | 235 | **31488** | **31488** |
| main | 24745 | 155 | 0 | **15252** | **15252** |

**With the shim OFF the `:4202` remap fires 31,488× per member** (and br0 sCharSharedDir
320×). The shim's `kMerge→kReplace` override is exactly what suppresses these remaps: a
kReplace subdir is appended as a reference and its objects are NEVER iterated through
`Filter`, so `o1->Dir() == sBoneMergeDir` can never match; kMerge iterates them and the
remap fires.

## Interpretation
The shim IS the reason the sBoneMergeDir/sCharSharedDir remaps are dead on the shipped
build. This is a coherent mechanism-(a) candidate. But see the binding-topology result and
the visual gate — restoring the remap (shim OFF) re-points hand bones to per-member
instances yet the hands still VISUALLY fling (per-member ≠ gender-posed). See STATUS.md.
