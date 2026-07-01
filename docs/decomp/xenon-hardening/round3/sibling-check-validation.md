# Sibling-aliasing vet check — full-corpus validation (2026-06-23)

Round-4 follow-up that the T-A adversarial review surfaced: the sibling-aliasing
check (`vet_xenon_identities.py --sibling-check`) passed its 30-pair-sample
verification (selftest 26/26, catches both literal-discriminable known-negatives,
0/27 false positives), **but applying it to the full corpus revealed two
false-positive classes the small sample never contained.** Validated against the
real Wii+Xenon decompiled bodies and fixed; both fixes preserve the sample
verification.

## What full-corpus application exposed

Re-vetting the live ACCEPT set with the check ON (vs OFF) downgraded **67**
ACCEPT→REJECT (downgrade-only: 0 entries added/removed, 0 non-tier field changes).
Inspecting all 67 against the actual decompiled bodies:

| Class | Count | Verdict |
|---|---|---|
| call-arg node-size **differs** (`[16]≠[76]`, `[28]≠[40]`) | ~32–36 | **genuine** — different node size ⇒ different template instantiation |
| call-arg **trailing-arg-drop** (`[24,1]≠[24]`) | 9 | **FALSE POSITIVE** |
| store-const **`N ≠ 0`** (`6≠0`, `-1≠0`, `0≠1`) | 21 | **FALSE POSITIVE** |
| store-const both-non-zero (`6≠1`) | 1 | **genuine** (pair-16) |

### FP class 1 — call-arg trailing-arg drop
Xenon's decompiler renders `FUN_xxx(size)` for Wii's
`_MemOrPoolFreeSTL(size, PoolType=1, ptr)`, so `(size,1)` vs `(size,)` tripped
tuple-inequality even though the **node-size (the actual discriminator) AGREES**.
Fix: `_arg_conflict` flags only a genuine shared-**position** value conflict, never
a pure trailing drop.

### FP class 2 — store-const `N ≠ 0`
A `0` on one side is the cross-compiler decompiler failing to render a value, not a
real difference. Confirmed against bodies — all the SAME function:
- `Rnd::OnShowConsole`/`EventTrigger::OnTrigger` etc. (18×): both end
  `*p=0; p[1]=<kDataInt return-tag>`; MWCC renders the tag `6`, MSVC renders `0`.
- `StringTable::StringTable(int)`: the `0xffffffff` sentinel sits at `[3]` on Wii,
  `[4]` on Xenon (struct-packing index shift) → read as `-1≠0`.
- `Quazal::SingleThreadCallPolicy` ctor: same `{0,0,1,0}` values at permuted offsets.

Fix: require **both** store constants non-zero, so a real type discriminator
survives (pair-16: `kDataInt=6` vs `kDataFloat=1`).

## Validated result (both fixes applied)

- selftest **26/26**, sibling-eval **2/2** literal known-negatives, **0/27** FP — unchanged.
- Full-corpus sibling-flags **67 → 37** (30 cross-compiler artifact FPs removed):
  **36 call-arg node-size-differs + 1 store-const (pair-16)**.
- pair-13 and pair-16 both still REJECT.

## Disposition + residual

The check is a **precision-positive, downgrade-only** vet step: it removes
suspect ACCEPTs, never creates a new identity, so any residual false flag costs
**recall only** (a few of ~2165 ACCEPTs), never precision. It stays **default-OFF**
(`--sibling-check off`).

**Known residual (round-5 candidate, not blocking):** the 36 call-arg
node-size-differs are predominantly genuine (a different freed-node size ⇒ a
different instantiation; e.g. pair-13's `16` vs `36`), but a *same logical type with
different MWCC-vs-MSVC struct padding* could in principle produce a small node-size
delta and be a false-downgrade. PPC32 padding deltas are small (≤~8 B) whereas the
observed deltas are larger (e.g. `32` vs `20`), so the risk is low; validate the 36
individually (or gate on `|Δsize| > 8`) before the check is ever used to regenerate
`vetted_identities` / re-ingest.

**The lesson is round-2's, reconfirmed:** same-ISA discrimination signal does not
transfer cleanly cross-compiler. Every literal/offset/tag comparison must be guarded
against MWCC↔MSVC decompiler-rendering and struct-layout differences — small judged
samples won't surface those classes; full-corpus application does.
