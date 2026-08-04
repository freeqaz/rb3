# Liveness and Scheduling (register-swap causes)

Register swaps are **symptoms**. Nobody permutes a register name — the whole swap set flips
at once when the underlying cause is fixed, because register assignment is downstream of the
interference graph, and the interference graph is downstream of **live ranges** and
**schedule**.

This page exists because the standing guidance — reorder the local variable declarations —
is the right lever attached to the wrong pattern, and RB3's own dead-end record already says
so without naming the alternative.

## Provenance — read this before citing anything here

The claims below fall into three buckets and are labelled inline. **Do not flatten them.**

| Bucket | Status |
|---|---|
| **MWCC-measured (this repo)** | Established here. Cited to the RB3 doc or wave that measured it. |
| **ABI / determinism consequence** | Compiler-independent. Follows from the PowerPC ABI or from the compiler being deterministic, not from a sample. |
| **MSVC-measured, UNVERIFIED on MWCC** | Measured in `../dc3-decomp` (MSVC X360, n=4 functions, 2026-08-02/04). A **hypothesis** here. MWCC is a different backend with `-ipa file`, and the one place the two have been compared directly they *agreed* — but nobody has run these levers on Gekko. |

The MSVC work is written up at
[`../../../../dc3-decomp/docs/decomp/patterns/fixable-liveness.md`](../../../../dc3-decomp/docs/decomp/patterns/fixable-liveness.md)
and, tool-side, at
[`../../../../objdiff/docs/research/register-swap-symptom-not-cause.md`](../../../../objdiff/docs/research/register-swap-symptom-not-cause.md).

### Standing note: percentages here are point-in-time

**Every per-function percentage on this page is a reading taken on one repo at one commit.
Re-measure before citing one** — dc3-decomp figures with `mcp__orchestrator__run_objdiff`
against that tree, RB3 figures with this repo's own tooling. Two ways they rot:

1. **Neighbours drift.** A match% can move when a *different* function in the same
   translation unit changes; inlining, ICF and `.text` layout are all TU-wide.
2. **The number in a commit message is not a measurement.** `RndText::SizeCheck` carried a
   fabricated **99.1%** here for two days, sourced from a dc3-decomp commit subject line
   rather than a diff. Corrected to 98.6% on 2026-08-04; see Lever 3.

**Always name the repo next to a number.** rb3, rb3-xenon and dc3-decomp share the Milo
engine, so the *same symbol name* exists in all three with different code and different
match%. An unattributed figure will eventually be refuted against the wrong binary — that
has already happened.

### What does NOT apply here

**The whole EH-funclet class is absent on Wii.** MSVC `/EHsc` emits one ~40-byte
`__unwind$` / `__catch$` funclet per cleanup state, and each funclet's prologue encodes its
**parent's** frame size — so on the MSVC targets any edit that grows a parent's frame shows
up as a fresh mismatch inside a tiny sibling symbol, and a *correct* parent fix can read as a
regression. RB3 compiles with `-Cpp_exceptions off`; `config/SZBE69*/symbols.txt` contains
**zero** unwind/catch symbols. If you are reading a DC3 or rb3-xenon note about funclet score
wobble, it has no counterpart here — ignore it.

---

## The boundary (MWCC-measured, and it is the useful part)

| You change… | It moves… | Diff signal |
|---|---|---|
| **Scoping / packing** — which block a declaration lives in, braces, named-vs-temp | **Stack slots**: frame size, slot order, packing | `OFFSET_SWAP`, `[off:-N]` on `r1` |
| **Liveness / scheduling** — what is carried across a call, where a value is computed | **Registers**: which callee-saved register holds what, how many are saved | callee-saved swap clusters, prologue save-range delta |

RB3 has measured both halves independently, and neither was labelled as such at the time:

- **Liveness moves registers.** [`harmful-avoid.md`: Child Pointer in
  Loop](harmful-avoid.md#child-pointer-in-loop) is exactly this pattern, measured at **−6.5%**:
  hoisting `auto* child = *it;` out of the loop body *lengthens* a live range across the inner
  call, MWCC then needs `child` in a callee-saved register to survive that call, which steals a
  slot the target spends on a member load or loop bound — and the diff surfaces as an
  `r28`↔`r29`-class swap concentrated in the loop body. That is the **mirror image** of the
  MSVC "call through the cached local" lever below: same mechanism, opposite direction.
- **Declaration order does not.** Wave E1 (`docs/plans/wave-e-targets.md`,
  `docs/plans/permuter-mechanization-roadmap.md` §E1) ran `declaration_reorder` +
  `statement_reorder` + all patterns for 8-12 rounds each on **11** regswap targets and got
  **0 improvements**, plus manual edits that regressed or did nothing. Its own stated root
  cause is a liveness statement: *"MWCC assigns callee-saves based on **whole-TU live-range
  analysis**, not declaration order within the function."*

`-ipa file` is why that reads more strongly on MWCC than on MSVC: the allocation decision is
made across the whole translation unit, so a within-function declaration permutation is even
further from the thing being decided.

**So the RB3-native reading is not "the MSVC finding might apply here" — it is "RB3 already
measured the negative half and stopped one step short of the positive half."** The levers
below are the candidate positive half. They are unverified here.

---

## Two consequences that need no sample at all

### 1. A volatile-register swap can never *be* a live-across-call disagreement

A volatile register cannot hold a value across a call — that is the ABI, not a statistic. So:

| Swap set | What is excluded | Where to look |
|---|---|---|
| **volatile only** (`r0`, `r3`-`r12`, `f0`-`f13`) | liveness across calls | emission order: a producer scheduled after its consumer, a compare with the operands the other way round |
| **callee-saved only** (`r14`-`r31`, `f14`-`f31`) | — | what is live across a call |
| **mixed** | — | **one** liveness cause; the volatile half is its shadow |

The converse is **not** symmetric: a volatile swap can be *downstream* of a liveness problem
elsewhere even though it cannot *be* one. This sharpens
[`at-limit-mwcc.md`: Volatile-register FPR swaps](at-limit-mwcc.md#volatile-register-fpr-swaps-f0-f13),
which correctly says those are scheduling-driven — it just did not say that the *reason* is
decidable rather than empirical.

### 2. Byte-identical is not "no improvement" — it is a routing signal

A deterministic compiler produces identical output for two inputs only if the mutation did
not change the program it sees. So a source edit that yields a **byte-identical `.o`** is
positive evidence that **you are on the wrong axis**, not an invitation to try more points on
the same axis. Distinguish it from "compiled differently, scored the same", which means the
axis is live and you have not found the right point on it.

Practically: the moment a declaration reorder comes back byte-identical, stop reordering.

---

## The candidate levers (MSVC-measured — UNVERIFIED on MWCC)

Everything in this section is a hypothesis for Gekko. The numbers are DC3's. Try them when
the residual is register swaps and the declaration axis has already gone byte-identical.

### Lever 1 — read the call's arguments back out of an aggregate you just built

`f(a, b)` → `f(agg.first, agg.second)` where `agg` was constructed from `a` and `b` on the
preceding line and neither is modified in between. A provable source-level no-op that ends
one value's live range **at the aggregate store** instead of carrying it across the call.

DC3 `ObjectDir::Iterate`, 99.4% → 100%: all 17 swapped registers flipped with **zero** change
to the instruction stream. Tell: the swap is a **rotation of length ≥ 3**, not a 2-cycle —
that means the *set* of simultaneously live values differs, not just the colours.

### Lever 2 — call through the local you already have; don't re-spell the member path

The function caches a member pointer in a local, then a later call site spells the member path
again. The re-load forces the base object to stay live across the call **as well**, costing one
whole callee-saved register. **dc3-decomp** `RndText::FitTextScroll`, 92.7% → 96.7%; the
prologue save range differed by exactly one register and ~40 swaps collapsed from the single
edit. **96.7% is an intermediate, not that function's final figure** — Lever 4 below takes
the same function to **98.2%**, where it stands on dc3-decomp `main` today.

This is [`harmful-avoid.md`: Child Pointer in Loop](harmful-avoid.md#child-pointer-in-loop)
run backwards, and that page's RB3 measurement is the strongest reason to expect this one to
port. Note the trap in the middle: creating the local and then *not* calling through it is the
worst of both worlds — you pay for the local's live range *and* for the reload.

**Sub-lever:** drop `= 0` / `= 0.0f` on a pure out-param. The target emits no init store for a
value the callee writes on every reachable path; an initialiser adds a store the target lacks
and starts the live range artificially early. **Check the callee writes unconditionally** — if
it writes conditionally the initialiser is load-bearing and removing it is a real bug.

### Lever 3 — fix the schedule, *then* the comparison polarity

FPR swaps clustered around a compare usually mean the arithmetic feeding it is materialised at
a different point, not that the FPRs are miscoloured. Hoist the producer so it lands ahead of
its consumer **first**; only then flip the compare to the target's operand order.

`a <= b` and `b >= a` are exact equivalences including NaN (both false when either operand is
NaN). `a <= b` → `!(a > b)` is **not** — do not do that.

Ordering matters: flipping the compare before fixing the schedule just moves the swap to the
other side of the compare and scores as a wash. **dc3-decomp** `RndText::SizeCheck`,
96.5% → **98.6%**, nine FPR swaps resolved automatically once the schedule was right.

> Corrected 2026-08-04: this said 99.1%. That figure was never a direct measurement — it
> came from dc3-decomp commit `0c2b0c38`'s subject line. Re-measured with `run_objdiff`
> in **dc3-decomp**: 96.5% at the parent, 98.6% at `0c2b0c38` itself, 98.6% on `main`.
> The residual is one `mr r4, r27` moved two slots, and it was already there at 96.5% —
> this lever never touched it. Not an rb3 measurement.

### Lever 4 — scope a declaration into the block that uses it

A **stack** lever, listed here for contrast: it moves slots and no registers. Same-scope locals
pack together, so moving two into the inner block that uses them makes them pack adjacently
instead of each claiming an outer-frame slot. **dc3-decomp** `RndText::FitTextScroll`,
96.7% → **98.2%**: 14 offset diffs killed at once. This is the second of that function's two
levers, so 98.2% is its final figure.

**Confirm the block is faithful before adding it** — in the DC3 case the target genuinely
branched past the whole block on the null path, so the `if` reproduced real control flow rather
than being a match hack.

### Lever 5 — name the temporaries built inside a call argument list

The inverse of Lever 4. An unnamed aggregate passed by const-ref dies at the end of its own
full expression, so N consecutive calls each building one share **one** stack slot. Naming them
widens each live range so the frame packer sees them all — and *then* re-coalesces the pairs
whose ranges still do not overlap. **dc3-decomp** `LabelShrinkWrapper::UpdateAndDrawWrapper`:
68.1% → **99.9%** (+31.8%), four names, three slots, frame `0xb0` → `0xc0`. 68.1% is the
honest baseline; an 80.4% figure circulates for this function but it was a `_tmp0` match-hack
state, not a clean starting point, and quoting it understates the lever as +19.4%.

**Trap: fewer names is not closer.** Two intermediate spellings measured 90.6% and 86.2% and
both read exactly like floors. Match the target's *number of live values* and let the packer
choose the sharing; do not hand-recycle a slot.

---

## Triage: which residuals are worth opening

*MSVC-measured on 31 AT_LIMIT functions — the split itself is a routing heuristic and should
transfer, the hit rates should not be quoted here.*

| Residual implicates… | Verdict |
|---|---|
| **A statement** — control flow, which field is read, which call is made, what stays live across a call, the shape of an *explicitly parenthesized* expression | **Investigate** |
| **One arithmetic expression** — commutative operand order, flat-sum term order, which of two independent loads issues first | **Floor. Skip.** |

**The parenthesization exception is MWCC-confirmed here**, independently and before the MSVC
work: [`at-limit-catalog.md`'s `BSPFace::Update`](../../knowledge/at-limit-catalog.md) records
92.87% → 96.96% from restoring *"a proper parenthesized shoelace-formula 3-pair form — **parens
are load-bearing, a flat expr re-serializes**"*. That is the same rule from the other side: a
flat sum is canonicalised by the compiler and its term order is not a source-visible degree of
freedom, while an explicitly nested chain keeps its shape and its term order is real.

Two consequences for RB3 work:

1. If your sum is flat, term reorder is not a lever — stop.
2. If it is (or should be) explicitly parenthesized, the term order is **recoverable**, not a
   guess: target and base share a schedule, so emission-position → sum-slot is a fixed
   permutation. Read it off our own build, invert it, apply it to the target's operand offsets.
   (Confirm the nesting actually survived first — MWCC can flatten it, in which case the
   exception does not apply.)

`BSPFace::Update` also carries the warning that goes with all of this: *"Do not 'clean up' the
hoisted `fvx/fvy/fc` temps to match DC3's form — DC3 is a different compiler build and removing
them regresses to 95.6%."* **Porting a DC3 source shape is not the same as porting a DC3
finding.** This page ports findings.

---

## Floor evidence: the three-part standard

*Methodology; compiler-independent.* [`at-limit-mwcc.md`: When to mark
at-limit](at-limit-mwcc.md#when-to-mark-at-limit) requires (a) hand variants and (b) a
permuter sweep. Add a third, and sharpen the first:

- **(a) Hand variants return byte-identical output** — *identical bytes*, not "no improvement"
  (see the routing signal above).
- **(b) A permuter sweep returns zero improvements.** Record date, config and candidate count,
  as `at-limit-mwcc.md` already requires.
- **(c) Decompile the *target* and name the construct as an allocator artifact.** ← the one
  worth adding.

(a) and (b) only ever prove "I ran out of ideas". (c) converts that into "I proved this is
unreachable from C++". Decompile the **target** function (RB3 has full DWARF, so this is
cheaper here than on the MSVC targets) and read the residual instructions in the context of the
target's own decompilation. You are looking for one of:

- a spill/reload of a value with no source-level identity (allocator scratch),
- a slot reused for two unrelated values (live-range splitting),
- a store with no corresponding read on any path (dead conditional spill).

Any of those three names an allocator artifact and the floor claim is defensible. Anything else
— a real computation, an extra call, a different constant — means there **is** a missing source
construct and the function is not at a floor.

RB3 has a whole category this standard closes cleanly rather than by exhaustion: the
`psq_l`/`psq_st` inline-asm TUs in
[`at-limit-catalog.md`](../../knowledge/at-limit-catalog.md) and
[`paired-single-boxmap-lighting.md`](paired-single-boxmap-lighting.md) are at-limit **by
construction** — hand-written paired-single asm in the original — which is a (c)-grade proof,
not an (a)-grade one. Aim for that standard of evidence elsewhere.

---

## Diagnostic order for a register-swap residual

1. **Register class.** Volatile-only → scheduling (Lever 3). Callee-saved or mixed → liveness
   (Levers 1-2). This step is decidable; do it first.
2. **Prologue save-range delta.** A different callee-saved save count means the two builds
   disagree about how many values must survive calls — that *is* the cause, and the swaps are
   its shadow. **This is a lead, not floor evidence.**
3. **Instruction counts and sizes.** Equal, with every mismatch on a register operand → the
   logic is right; you are purely in allocation territory.
4. **Swap cycle length.** 2-cycle = colouring flip. 3+ rotation = the live set differs
   (Lever 1).
5. **Is a producer at a different instruction index than the target's?** → schedule problem
   (Lever 3). Fix the schedule before touching polarity.
6. **Only then** declaration order — and stop at the first byte-identical result.
7. **Offsets rather than registers?** Different problem: Lever 4, and
   [`at-limit-mwcc.md`: Stack-slot inversion](at-limit-mwcc.md#stack-slot-inversion--offset_swap-on-r1).

Do not expect swap *count* to predict fix size: DC3 cascades of 17 and ~40 swaps both collapsed
to zero from a single edit.

---

## If you test one of these on MWCC, record the result here

The value of this page is currently asymmetric — the negatives are RB3's own, the positives are
borrowed. A single measured MWCC result on any of Levers 1, 2, 3 or 5 (win **or** byte-identical)
is worth more than anything above it. Wave E1's 11 targets are the obvious candidate pool: they
are already known to be immune to the declaration axis, which is precisely the precondition
these levers are for.

---

## See also

- [at-limit-mwcc.md](at-limit-mwcc.md) — source-immune vs permuter-class triage; the at-limit checklist this page adds a third condition to
- [permuter-roi.md](permuter-roi.md) — when to dispatch the permuter; its register-allocation-cascades section is qualified by this page
- [harmful-avoid.md: Child Pointer in Loop](harmful-avoid.md#child-pointer-in-loop) — RB3's own measured liveness lever, running the other way
- [fixable-declarations.md](fixable-declarations.md) — declaration order and scope; still the right tool for stack-slot residuals
- [../../knowledge/at-limit-catalog.md](../../knowledge/at-limit-catalog.md) — Wave E1's 11 declaration-axis-immune regswap targets, and `BSPFace::Update`'s load-bearing parens
