# Verdict — Pair 26

**Claim:** Wii `TambourineGems__17TambourineManagerCFv` (0x801ebda0) == Xenon `0x826dbaa8`
**Match type:** `ExactInstructionsFunctionHasher` (non-BSim, simconf n/a)
**Verdict: CORRECT — confidence MEDIUM**

## Evidence

Both sides are a tiny const getter: a 4-level pointer chase ending in `field + small_const`.
Leaf function on both sides (0 callees), no strings, no float/magic constants — so judged
purely on the dereference-chain fingerprint.

Wii asm (build/SZBE69_B8/asm/band3/game/TambourineManager.s:643-650, confirmed against CW map line 9908, size 0x18):
```
lwz r3,0x1c(r3); lwz r3,0x358(r3); lwz r3,0x0(r3); lwz r3,0x8(r3); addi r3,r3,0x18; blr
```
→ `return *(*(*(*(this+0x1C)+0x358)+0x0)+0x8) + 0x18;`

Xenon pseudo-C (evidence pack):
```c
return (ulonglong)*(uint *)(**(int **)(*(int *)(param_1 + 0x28) + 0x390) + 8) + 0x24;
```
→ `return *(*(*(*(this+0x28)+0x390)+0x0)+0x8) + 0x24;`

| step | Wii | Xenon |
|---|---|---|
| load 1 | +0x1C | +0x28 |
| load 2 | +0x358 | +0x390 |
| deref @0 | +0x0 | **+0x0** |
| load 4 | +0x8 | **+0x8** |
| final addend | +0x18 | +0x24 |

## Reasoning

- **Identical control-flow skeleton:** exactly 4 chained loads (offset, offset, deref-at-0,
  +0x8) then add a small constant, then return. No branches, same arity (one `this`, one ret).
- **Inner offsets byte-identical** (+0x0, +0x8) — distinctive; only the outer member offsets
  shift (0x1C→0x28, 0x358→0x390, addend 0x18→0x24), which is exactly the expected MWCC-vs-MSVC
  struct-layout divergence (substrate caveat #1). The addend grows in lockstep with the layout.
- Matched by `ExactInstructionsFunctionHasher` — a high-precision near-identical-instruction
  matcher; consistent with a 6-instruction getter whose only differences are reloc-able offsets.
- Semantically consistent with the demangled name: a "get tambourine gems" accessor chasing
  into a sub-object (large 0x358/0x390 offset, typical of reaching a track's gem/note list)
  and returning `inner + 0x18`. Not contradicted (not matched to a parser/heavy fn).

## Why MEDIUM not HIGH

No independent corroboration is available (no callees, strings, or constants — the substrate's
strongest signal, the resolved-callee column, is empty for leaf getters). The exact-instruction
hash on a 4-deref chain is the sole basis; a coincidental collision with another identically
shaped getter is improbable given the specific 0x358/0x390 + inner 0x0/0x8 sequence, but cannot
be fully excluded without callee/string evidence. Hence MEDIUM, not HIGH.

## For the next agent

This is the canonical "tiny leaf getter, ExactInstr, no corroboration" case. The 4-deref offset
sequence with byte-identical inner offsets (+0x0, +0x8) and a small additive constant scaling
with the layout shift is a strong-but-not-conclusive fingerprint. Treat such ExactInstr leaf
getters as CORRECT/MEDIUM by default unless the offset sequences actually diverge.
