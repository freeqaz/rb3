# Pair 30 verdict — GetRankData__11SingerStatsCFi ↔ Xenon 0x826798b0

**Verdict: correct (confidence: medium)**

## Claim
Wii `GetRankData__11SingerStatsCFi` (`0x801e68a0`, `Stats.o`, 16 B) == Xenon
`FUN_826798b0` (16 B). Match type: **Implied Match** (no BSim score; inferred from
call-graph/neighbor context). Both are leaf functions, no strings, no callees.

## Evidence
Wii ground-truth asm (build/SZBE69_B8/asm/band3/game/Stats.s:4701-4706):
```
lwz   r3, 0x0(r3)    # load value at offset 0 of `this`
slwi  r0, r4, 3      # arg << 3  ==  arg * 8
add   r3, r3, r0     # base + arg*8
blr
```
→ semantics: `return *(this+0) + (arg * 8)`.

Xenon pseudo-C (evidence pack):
```c
longlong FUN_826798b0(uint *param_1, ulonglong param_2) {
  return (param_2 & 0x1fffffff) * 8 + (ulonglong)*param_1;
}
```
→ semantics: `return *(param_1+0) + (param_2 * 8)`.

`& 0x1fffffff` is the documented Xenon 64-bit-register-narrowing artifact (see
substrate caveat 2), not a real difference. Both sides compute **identical
arithmetic**: dereference value at offset 0 of `this`, add `arg * 8`.

## Why correct
- **Exact arithmetic agreement** on a distinctive shape: deref-at-offset-0 + `arg*8`.
  Both 16 bytes, both leaf.
- **Stride of 8 matches the TU.** SingerStats's whole sort machinery in the same
  `Stats.o` operates on `pair<i,f>` (exactly 8 bytes) — `PartPercentageSorter`,
  `__introsort_loop<...pair<i,f>...>`, etc. (map lines 9804-9818). `GetRankData`
  returning `base + idx*8` is a getter indexing an array of 8-byte entries,
  fully consistent with the demangled name `SingerStats::GetRankData(int) const`.
- **Implied Match** adds graph-context corroboration independent of body shape.
- **Zero contradicting signals**: no disjoint strings (neither side has any),
  no switch-count mismatch, no leaf-vs-call-heavy contradiction, size ratio 1.0.

## Why not high confidence
Tiny leaf function; generic `base + idx*8` arithmetic could in principle collide
with another getter. No strings/floats/magic-constants/callees to lock the
identity down further. The exact arithmetic + size + stride-matches-TU + Implied
Match together clear "correct" at medium confidence, but not high.

## Decisive evidence
Both sides compute the identical `*(this@0) + arg*8` (Xenon mask is a 64-bit
narrowing artifact); 16 B == 16 B leaf; stride 8 == sizeof(pair<i,f>) used by
the SingerStats sort machinery in the same TU.
