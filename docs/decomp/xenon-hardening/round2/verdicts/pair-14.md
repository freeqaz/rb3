# Pair 14 verdict — Hit__4TailFv ↔ 0x82b7e618

**Verdict: CORRECT — confidence HIGH**

## Claim
Wii `Hit__4TailFv` (`Tail::Hit()`, Bank-8 `0x80144ea0`, TU `Tail.o`) == Xenon `0x82b7e618`.
Nominated by BSIM, sim×conf 23.693 (similarity 0.876 / confidence 27.047), stratum BSIM 20-30.

Note: my task header listed the Wii addr as `0x803b8c60`; that is a header typo. The
CW map (`orig/SZBE69_B8/files/band_r_wii.map:6968`) and the evidence pack both place
`Hit__4TailFv` at `0x80144ea0`. The symbol is unambiguous regardless.

## Decisive evidence — byte-identical field offsets + memset callee
Both functions implement the exact same 3-step body. Field-by-field:

| Operation | Wii | Xenon | |
|---|---|---|---|
| store constant `2` | `stw r4,0x18(r3)` | `*(p+0x18)=2` | ✓ same offset + constant |
| guard: flag at 0x4f8 == 0 | `lbz r0,0x4f8` | `if *(p+0x4f8)==0` | ✓ |
| load ptr at 0xc | `lwz r4,0xc(r3)` | `*(p+0xc)` | ✓ |
| set byte at ptr+8 | `ori 0x80; stb r0,8(r4)` | `*(ptr+8)=1` | ✓ same field (bit-set vs Ghidra `=1` render) |
| guard: flag at 0x28 != 0 | `lbz r0,0x28` | `if *(p+0x28)!=0` | ✓ |
| memset(base, 0, len) | `addi r3,r3,0x2c; li r5,0x4b0; bl memset` | `FUN_82805b10(p+0x2c,0,0x4b0)` | ✓ |
| store `0` | `stw r0,0x4dc(r31)` | `*(p+0x4dc)=0` | ✓ |

Five distinct field offsets (0x18, 0x4f8, 0xc, 0x28, 0x4dc), the destination
offset 0x2c, AND the distinctive memset length **0x4b0** all match exactly. The
Xenon callee `FUN_82805b10(p+0x2c, 0, 0x4b0)` — arity 3, args `(dest, 0, count)`,
magic length 0x4b0 — is unmistakably `memset`, matching the Wii `bl memset` call
1:1. The control-flow skeleton is identical: one unconditional store + two
independent guarded blocks.

## Caveats considered (none weaken the verdict)
- **Offsets byte-identical**, not shifted — unusual for cross-compiler (substrate
  caveat #1 warns offsets usually differ), but here they don't. Strengthens, not
  weakens: shared Milo-engine struct layout survived MWCC→MSVC.
- **`unk8 |= 0x80` (Wii) vs `= 1` (Xenon)**: Ghidra renders the byte-store as `= 1`;
  Wii does `ori r0,r0,0x80; stb`. Same target field (ptr@0xc, +8); the rendered RHS
  is a Ghidra simplification artifact on the byte op, immaterial given the
  surrounding 7-of-7 offset agreement.
- The single Xenon callee (`FUN_82805b10` = memset) is unresolved in matches.json
  (memset isn't a band3 seed) — absence of a resolved name is not evidence against
  (substrate caveat #4); the call signature itself identifies it.
- Size ratio: Wii ~13 body instrs vs Xenon 108 B (~27 instrs) ≈ 2x — squarely in
  the expected MSVC/Xenon 1.0-2.5x band.

## For the next agent
This is a textbook-clean BSIM correct identity from the BSIM 20-30 stratum: a small
non-leaf with five matching field offsets, two distinctive constants (0x2c, 0x4b0),
and a memset callee whose signature matches the Wii body. It is a positive data
point for band3 BSIM ACCEPT precision and argues the dc3-BinDiff pessimism (0.193)
on band3 is an oracle artifact, not a real precision dip — at least for this pair.
