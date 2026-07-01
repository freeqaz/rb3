# Pair 23 verdict — CORRECT (high)

**Claim:** Wii `PitchNote__5LyricCFv` (`0x80142cf0`, Bank 8) == Xenon `0x82b7d078`
**Match type:** `ExactInstructionsFunctionHasher` (simconf n/a — non-BSim)
**Verdict:** **correct**, confidence **high**

## Decisive evidence

The actual decompiled source confirms the identity directly. From
`src/band3/bandtrack/Lyric.cpp:216`:

```cpp
bool Lyric::PitchNote() const { return !mVocalNotes[0]->mUnpitchedNote; }
```

with `std::vector<const VocalNote *> mVocalNotes; // 0x30` (`Lyric.h:28`).

This expands to: load the vector member, deref its data pointer to get the first
`VocalNote*`, load the `mUnpitchedNote` byte field, negate. Both toolchains emit
exactly this:

| Operation | Wii asm | Xenon pseudo-C |
|---|---|---|
| Load `mVocalNotes` (vector member) | `lwz r3, 0x30(r3)` | `*(int**)(param_1 + 0x3c)` |
| Deref → first `VocalNote*` | `lwz r3, 0x0(r3)` | `**` |
| Load `mUnpitchedNote` byte | `lbz r0, 0x2a(r3)` | `*(char*)(... + 0x2a)` |
| Negate / `== 0` | `cntlzw r0,r0` + `srwi r3,r0,5` | `== '\0'` |

## Why this is high-confidence despite being a small leaf function

- **Inner field offset 0x2A matches byte-for-byte on both sides.** `mUnpitchedNote`
  is a field of `VocalNote` (shared engine struct `beatmatch/VocalNote.h`), whose
  layout is identical across the Wii and Xenon builds — which is exactly why the
  inner offset agrees even though the outer vector member offset differs
  (0x30 Wii vs 0x3c Xenon, the expected toolchain struct-layout delta, substrate
  caveat #1).
- **Distinctive control-flow fingerprint:** double-pointer-deref → byte-at-0x2A →
  boolean negate, returning a bool. Not a generic getter shape; the specific
  `==0`/negate idiom over a chained deref is a tight match.
- **Semantics match the demangled name:** `Lyric::PitchNote() const` returns whether
  the first vocal note is pitched (`!mUnpitchedNote`). The body does precisely this.
- Same arity (single `this`), same leaf-ness (0 callees on both sides), consistent
  size ratio (Wii 0x18=24 B, Xenon 24 B — ~1.0x).

## Ground-truth anchors
- Map: `orig/SZBE69_B8/files/band_r_wii.map` line 6926 — `PitchNote__5LyricCFv`
  `80142cf0` size `000018` in `Lyric.o`.
- Source: `src/band3/bandtrack/Lyric.cpp:216`, `src/band3/bandtrack/Lyric.h:18,28`.
- Wii asm: `build/SZBE69_B8/asm/band3/bandtrack/Lyric.s` (in evidence pack).

## For the next agent
No open questions. This is a textbook ExactInstr leaf-getter match corroborated by
the real source. The only "mismatch" (member offset 0x30 vs 0x3c) is the expected
MWCC-vs-MSVC struct-layout difference and is NOT evidence against. Pairs of this
shape (tiny shared-engine accessors with a matching inner field offset) should be
treated as reliable.
