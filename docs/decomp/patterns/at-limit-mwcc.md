# At-Limit Patterns (MWCC / RB3)

Reference for residual diffs that look stuck. Targets the Wii build under MetroWorks CodeWarrior (`mwcceppc 4.3.172`, `-O4,p -inline noauto -ipa file -sdata 2 -sdata2 2`).

"At-limit" is **not** a single category. There are two distinct buckets that this doc deliberately keeps separate:

1. **Source-immune** — the diff is an artifact of the build environment (anonymous-namespace hash, address relocation noise, linker-merged ICF). No source change can move it. The correct outcome is to accept the match.
2. **Permuter-class** — register-allocation cascades, FPR scheduling, bool materialization, stack-slot inversion, dead-store elimination, `subfic`/`subic` polarity, `fmadd` vs separate ops. Tedious to fix by hand, but mechanical for the source permuter. The correct first response is to run the permuter on the function/unit before marking anything at-limit.

The DC3 sister doc (`unfixable-compiler.md`) calls many of these "unfixable without binary patching." That framing was wrong for RB3 and led agents to give up on functions that the permuter would have fixed in one sweep. Use the bucket distinction below.

See also: [permuter-roi.md](permuter-roi.md), [fixable-bool-mask.md](fixable-bool-mask.md), [fixable-declarations.md](fixable-declarations.md), [verifiable-icf.md](verifiable-icf.md).

---

## Quick triage

| Symptom in objdiff | Category | Action |
|---|---|---|
| `diff_arg` swarm on `lis`/`addi`/`lwz` pairs, no `diff_op` | source-immune | Accept; note "address relocation noise" in commit |
| `bl` target name differs but functions are structurally identical, addresses collapse to one symbol | source-immune | Confirm ICF via address (`verifiable-icf.md`), accept |
| Mangled name contains `__N` or anonymous-namespace numeric tag that differs | source-immune | Accept; note "anonymous-namespace hash" |
| Callee-saved register swap (`r19-r31`, `f14-f31`) is the dominant `diff_arg` | permuter-class | Run permuter on the function; only mark at-limit after a clean sweep |
| Single `li 0`/`stw` delete near an inlined dtor or RAII wrapper | permuter-class | Try permuter; failing that, hand-edit per [fixable-bool-mask.md](fixable-bool-mask.md) |
| `subfic` vs `subic` on a bool/pointer negate, or `fmadds` vs `fmuls`+`fadds` | permuter-class | Run permuter; hand-edit only if permuter exhausts |

> WHY: Source-immune patterns waste cycles if you keep editing. Permuter-class patterns waste cycles if you accept them without running the permuter.

---

## Source-immune patterns

These three patterns are environment- or layout-driven. No source mutation can change them. Accept the match and note the pattern in the commit message.

### Anonymous namespace hash

> ANCHOR: `#anonymous-namespace-hash`

When a TU references a symbol declared in an anonymous namespace, MWCC encodes a TU-derived tag into the mangled name. The tag in our build does not match the original build's tag because the source path and compilation context differ.

Look for: `diff_arg` on `bl`/`lis`/`addi` instructions where the *structural* mismatch count is zero. The mangled symbol pair will show identical class/method names with a different numeric or hash-like suffix on one side. The instruction bytes for the call site itself are identical; only the relocation target name differs.

No source-level fix exists short of reproducing the original build's file paths and TU layout. Accept the match; in the commit message write something like `at-limit: anonymous-namespace symbol tag differs`.

### Address relocation noise

> ANCHOR: `#address-relocation-noise`

Our `.text` section is not byte-identical to the original (cumulative effect of all remaining non-100% functions). Every global reference compiles to a `lis`/`addi` pair where the high and low halves encode the global's address. Because the global's address drifts with `.text` size, the immediates differ even though the instructions are structurally identical.

Look for: high `diff_arg` count (often 10-40), zero or near-zero `diff_op` count, all mismatches on `lis`/`addi`/`lwz`/`stw`/`stb` with address-low or address-high immediates. The objdiff `verdict.classification` will be `AT_LIMIT` with `fixability: rarely_hand_fixable`.

Cannot be fixed at the source level. Will resolve naturally as more functions reach 100% and `.text` collapses toward the original size. Accept; note `at-limit: address relocation noise` in commit.

### Linker-merged ICF

> ANCHOR: `#linker-merged-icf`

`mwldeppc` folds identical function bodies. A function may compile correctly and then get merged into a sibling, so the symbol you're diffing actually resolves to a different function's bytes. Sometimes the fold is desired (target was also merged); sometimes the fold is *new* in our build and the target had two distinct bodies.

Look for: a function at suspiciously high or suspiciously low match% where the symbol address coincides with another function's. objdiff will note the address overlap. See [verifiable-icf.md](verifiable-icf.md) for the verification workflow.

If the target was also ICF-merged, accept. If only our build merged, the fix is to make this function's body diverge from its twin — typically by adjusting the *other* function (not the one being diffed) to break the byte-for-byte equality. Not a source change to the symbol under test.

---

## Permuter-class patterns

These look at-limit but the source permuter can shift them. Do not mark at-limit before running a sweep.

### Dead store elimination

> ANCHOR: `#dead-store-elimination`

Target eliminates a store to a member or stack slot whose value is never read on any path through the function; our build keeps the store. Or vice versa. Typically shows as 1-3 extra `stw`/`stb`/`stfs` to a `r1`-relative or `r31`-relative offset that the target lacks.

The store/no-store decision is driven by MWCC's reaching-definitions analysis, which is sensitive to liveness inferred from surrounding control flow. Permuting branch polarity, hoisting/sinking declarations, or splitting a compound statement frequently flips the analysis.

Run the source permuter on this function before marking at-limit.

### Boolean negation: subfic vs subic

> ANCHOR: `#boolean-negation-subfic-vs-subic`

Target uses `subfic rN, rX, 0x0` + `subfe`; our build uses `subic rN, rX, 0x1` + `subfe`. Both compute `!x` correctly. The two forms come from MWCC making different choices about whether the value being negated is treated as a `bool` (one-bit) or as a general integer/pointer.

The trigger is usually a `bool` vs pointer typing decision a few lines upstream — sometimes inside a helper that returns into a local. Permuting the local's declaration site, its type, or whether the negation is folded into a single expression usually flips MWCC's choice.

Run the source permuter on this function before marking at-limit.

### Callee-saved register cascades (r19-r31, f14-f31)

A swap on a callee-saved register (e.g. r30 ↔ r31, f30 ↔ f31) usually cascades through the function: prologue saves, every use, epilogue restores. The dominant `diff_arg` count comes from the cascade, not from the root swap.

MWCC's register allocator picks callee-saved registers based on declaration order, first-use order, and liveness graph topology. Source permutation that touches any of those (declaration reorder, hoisting/sinking, splitting compound expressions) routinely fixes the cascade.

Run the source permuter on this function before marking at-limit. See [permuter-roi.md](permuter-roi.md) for sweep parameters.

### Volatile-register FPR swaps (f0-f13)

f0-f13 swaps look like the callee-saved case but are scheduling-driven rather than allocation-driven. MWCC's scheduler picks volatile FPRs based on the instruction window it's currently scheduling, not on a global allocation pass. Source permutation has lower hit rate here than for callee-saved swaps, but it's still nonzero — restructuring the surrounding float expression (operand reorder for commutative ops, splitting a `fmadds` chain, hoisting a constant) sometimes shifts the choice.

Run the source permuter on this function before marking at-limit. If the permuter returns zero improvements after a full sweep, this one is genuinely at-limit; classify as permuter-exhausted.

### fmadds vs fmuls + fadds

Target fuses `fmuls + fadds` into `fmadds`; our build emits the separate pair, or vice versa. MWCC's FMA decision depends on expression scheduling and the live range of the multiply intermediate.

Try the source permuter first; if it doesn't move, see [fixable-fsel-fma.md](fixable-fsel-fma.md) for the hand-edit shapes (expression restructuring, splitting via a `volatile float` intermediate).

Run the source permuter on this function before marking at-limit.

### Stack-slot inversion / OFFSET_SWAP on r1

Paired `stw rN, A(r1)` / `stw rM, B(r1)` where the offsets are swapped between target and base. The base register is `r1` (frame pointer), so this is *not* a struct-field layout issue — it's the frame builder picking slot order based on the order locals went live. Allocation-order, not source-order.

Permutations that touch declaration order or first-use of the two locals routinely flip the slot pair. Run the source permuter on this function before marking at-limit.

---

## When to mark at-limit

Mark a function at-limit only when ALL of the following are true:

- [ ] `verdict.classification == AT_LIMIT` in the objdiff JSON output.
- [ ] Every detected pattern in `analysis.patterns[]` is either:
  - one of the three source-immune patterns above, OR
  - permuter-class AND a full source-permuter sweep on the function returned zero improvements.
- [ ] If permuter-class patterns are present, the permuter sweep result is recorded (so future agents know not to re-attempt). Record in `decomp.db` under the function's note field, or in the commit message.

If any pattern in the diff is `LikelyFixable` or `MaybeFixable`, the function is not at-limit — work the fixable pattern first.

> WHY: A "permuter-exhausted" mark blocks future agents from wasting cycles re-running the same sweep. A bare "at-limit" mark doesn't carry that information and invites re-attempts.

---

## Reading objdiff output

objdiff JSON includes per-function `verdict` and `analysis.patterns` fields. The two enums you need:

**`verdict.classification`** — overall function state:

| Value | Meaning |
|---|---|
| `LIKELY_FIXABLE` | At least one pattern is hand-fixable now |
| `MAYBE_FIXABLE` | Pattern set is fixable but needs investigation |
| `PERMUTER_CLASS` | Dominant blocker is permuter territory |
| `AT_LIMIT` | All patterns are source-immune or permuter-exhausted |

**`analysis.patterns[].fixability`** — per-pattern classification (RB3 milohax objdiff fork; the enum was renamed from the upstream `Unfixable`/`UsuallyUnfixable` to clarify the distinction):

| Value | Meaning |
|---|---|
| `likely_fixable` | Known hand-edit fix exists |
| `maybe_fixable` | Hand-edit might work; investigate |
| `permuter_class` | Run the source permuter |
| `rarely_hand_fixable` | EITHER source-immune (sec. 2 above) OR permuter-class volatile-register churn — check the pattern type to know which |

> CRITICAL: `rarely_hand_fixable` does NOT mean "give up." It is a union of source-immune and permuter-class volatile-register patterns. Read the pattern type (e.g. `anonymous_namespace_hash` vs `volatile_fpr_swap`) to know which bucket applies. If unsure, run the permuter — a wasted permuter sweep costs less than a wasted hand-edit attempt.

> WHY: The upstream `Unfixable` name encouraged "accept and move on" for any tag carrying it. The renamed `RarelyHandFixable` keeps the same conservative meaning ("hand-edits rarely work") while leaving the permuter path open.

---

## See also

- [permuter-roi.md](permuter-roi.md) — sweep ROI, parameters, when to escalate from default sweep to deeper search
- [fixable-bool-mask.md](fixable-bool-mask.md) — when a bool-materialization diff *is* hand-fixable
- [fixable-declarations.md](fixable-declarations.md) — declaration-order shapes that flip register allocation
- [verifiable-icf.md](verifiable-icf.md) — confirming a function got linker-merged before accepting the diff
