# Pair 24 verdict — GlobalOptionsNeedsSave__10ProfileMgrFv ↔ Xenon 0x82532198

**Verdict: CORRECT — confidence HIGH**

## Claim
Wii `ProfileMgr::GlobalOptionsNeedsSave()` (Bank 8 `0x8034aa10`, TU `ProfileMgr.o`)
== Xenon `FUN_82532198` (`0x82532198`). Match type: `ExactInstructionsFunctionHasher`.

(Note: the mission-line Wii addr `0x8034ce40` is WRONG; the CW map and asm body
both say `0x8034aa10`. Verified in `orig/SZBE69_B8/files/band_r_wii.map:15552`
→ `0033cbf0 00001c 8034aa10 ... GlobalOptionsNeedsSave__10ProfileMgrFv ... ProfileMgr.o`.
This is a header typo, not a substance issue — the body addr `0x8034aa10` is the
one in the evidence pack and the one I judged.)

## Decisive evidence — byte-for-byte semantic equivalence

Both bodies, side by side:

Wii (Bank 8 asm → m2c), `ProfileMgr.s:2342`:
```c
u8 GlobalOptionsNeedsSave(ProfileMgr *this) {
    if (this->unk554 != 1) return 0;   // lwz r0,0x554(r3); cmpwi r0,1; beq; li r3,0; blr
    return this->unk558;               // lbz r3,0x558(r3); blr
}
```
Xenon (Ghidra pseudo-C):
```c
undefined1 FUN_82532198(int param_1) {
    if (*(int *)(param_1 + 0x34) != 1) return 0;
    return *(undefined1 *)(param_1 + 0x38);
}
```

Identical on every axis a leaf getter can be judged on:
- **Arity:** one `this` pointer, no other args. ✓
- **Control-flow skeleton:** load a 32-bit field → compare to literal `1` →
  early-return `0` if not-equal → else load+return a single byte. Same branch
  structure, same single early-out. ✓
- **Distinctive constant:** the compare literal is `1` on BOTH sides. ✓
- **Return type:** single byte (`u8` / `undefined1`) — i.e. `bool`. ✓
- **Field gap:** dirty byte sits exactly 4 bytes after the state int on both
  (Wii 0x554→0x558, Xenon 0x34→0x38). Same intra-struct layout relationship;
  the absolute base differs (toolchain struct layout — expected per substrate
  caveat #1). ✓

## Source corroboration (independent of the asm)
`src/band3/meta_band/ProfileMgr.cpp:412`:
```cpp
bool ProfileMgr::GlobalOptionsNeedsSave() {
    if (mGlobalOptionsSaveState != kMetaProfileLoaded)
        return false;
    else
        return mGlobalOptionsDirty;
}
```
Maps perfectly: `mGlobalOptionsSaveState` (0x554) compared to `kMetaProfileLoaded`,
returning `mGlobalOptionsDirty` (bool, 0x558). The Wii asm compares against literal
`1`, so `kMetaProfileLoaded == 1`; the Xenon body compares against the same `1`.
The Wii name `GlobalOptionsNeedsSave` is semantically consistent with a tiny
state-check getter — no name/behavior contradiction.

## Caveats on confidence
This is a tiny leaf getter (11 Wii asm lines / 28 Xenon bytes) with no strings and
no callees, so the *default rule* ("thin on both sides → uncertain") is in play.
But this is NOT thin-on-both-sides: the two bodies are an exact structural+constant
match (compare-to-1, +4-byte byte-field, early-return-0), `ExactInstructionsFunctionHasher`
already established instruction-level equivalence, and the source semantics pin it.
A different source function would have to coincidentally have the identical
"compare field to 1, return adjacent byte else 0" shape AND be hashed as the exact
match — very low probability. There IS a small generic-shape collision risk for a
2-field getter of this form, which is why this is HIGH and not "certain," but the
constant `1`, the 4-byte field gap, and the exact-instruction hasher together push
it firmly to CORRECT/HIGH.

## For the next agent
- Verdict: **correct / high**. No further digging needed.
- Header typo to flag upstream: pair-24 mission line Wii addr `0x8034ce40` should
  be `0x8034aa10` (CW map + asm agree on 0x8034aa10).
- Pattern note for the other ExactInstr leaf getters (pairs 22/23/25/26): a
  2-field getter with an exact-instruction hash + matching compare-constant +
  matching field-gap is a strong CORRECT even with no strings/callees. Judge the
  constant and the field-offset *delta* (not absolute), since toolchain struct
  bases differ.
