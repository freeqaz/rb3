# Permuter ROI

When objdiff reports a mismatch class that looks "unfixable from source" (register swaps,
bool-mask shape, stack slot offsets, FPR scheduling, addi/subi base-pointer arithmetic),
do not give up and do not hand-edit cascades longer than ~10 instructions. Run the source
permuter first. This page is the deep-link target from objdiff verdicts that point here.

## TL;DR

The permuter at `decomp_synth/` is a semantics-preserving source mutator. It hill-climbs
across a large family of mutation patterns (declaration reorder, scope widening, bool-cast
insertion, comma split, FMA reorder, branch polarity, etc. — and the catalog grows
constantly), recompiles with MWCC each variant, scores against the target via objdiff, and
keeps the best. Most "the asm shape is wrong but the C looks right" mismatches collapse to a
single-edit win the permuter finds in minutes; a unit sweep finishes in tens of minutes.
Conversion rate empirically runs ~5-10% of attempted functions improved per sweep, with
occasional 0 -> 100% wins. Invoke via `python -m decomp_synth.batch_auto` (see
[How to invoke](#how-to-invoke)).

Compiler is MWCC for Wii (Gekko/Broadway PowerPC, `mwcceppc 4.3.172`, `-O4,p -inline noauto
-ipa file -sdata 2 -sdata2 2`). Every category below is grounded in observed RB3 wins.

## When to dispatch the permuter

Each subsection corresponds to a deep-link anchor used by objdiff verdicts. If a verdict
classification matches one of these, the permuter is the right next step — not manual edits.

### Register allocation cascades
<a id="register-allocation-cascades"></a>

Callee-saved swaps across `r19-r31` and `f14-f31`, live-range shuffles where two locals trade
registers and cascade into ten or more `lwz`/`stw`/`mr` deltas, and "this function would
match if r28 and r30 were swapped"-shaped diffs. The permuter brute-forces the search space
without asking you to reason about coloring, and it is still the right first move here
because it is cheap. Run the source permuter; do NOT hand-edit cascades > ~10 instructions.

> ⚠ **Corrected 2026-08-04 — "mechanically controlled by declaration order and scope
> boundaries" is too strong, and it is the sentence that sends sweeps down a dead end.**
> Wave E1 (`../../plans/wave-e-targets.md`,
> `../../plans/permuter-mechanization-roadmap.md` §E1) ran `declaration_reorder` +
> `statement_reorder` + all patterns for 8-12 rounds each on **11** callee-saved regswap
> targets and returned **0 improvements**, with manual edits regressing or doing nothing.
> Its own root-cause note is the correction: *"MWCC assigns callee-saves based on whole-TU
> live-range analysis, not declaration order within the function"* — which `-ipa file`
> predicts.
>
> **Scoping and declaration order move stack slots; liveness and scheduling move
> registers.** Declaration order is the right lever for
> [Stack slot inversion](#stack-slot-inversion), which is unchanged. For registers, see
> [fixable-liveness.md](fixable-liveness.md) — including RB3's own measured liveness lever
> ([Child Pointer in Loop](harmful-avoid.md#child-pointer-in-loop), −6.5%) running in the
> opposite direction.
>
> Practical consequence for sweep budgeting: **a zero-gain declaration-axis sweep is not
> evidence that a function is at a register floor.** It is evidence that the axis is wrong.
> If the sweep came back byte-identical, that is stronger still — see
> [fixable-liveness.md § Byte-identical is not "no improvement"](fixable-liveness.md#2-byte-identical-is-not-no-improvement--it-is-a-routing-signal).

See [fixable-declarations.md](fixable-declarations.md) for the declaration/scope mechanism.

### Bool materialization
<a id="bool-materialization"></a>

`clrlwi.` vs `cmpwi` on byte-stored bools, `cntlzw + srwi.` vs `cmpwi + beq + li`,
`mfcr + cror` vs cascaded short-circuit branches, and bool-mask emission where the value is
correct but the carrier shape is wrong. The permuter inserts/removes explicit `bool` locals,
flips condition polarity, and rewrites `&&` chains into bitwise accumulation to flip the
emission shape. See [fixable-bool-mask.md](fixable-bool-mask.md). Run the source permuter;
do NOT hand-edit cascades > ~10 instructions.

### Stack slot inversion
<a id="stack-slot-inversion"></a>

`OFFSET_SWAP` between two locals (e.g. target uses `0x10`/`0x18`, base emits `0x18`/`0x10`),
slot padding deltas where the frame is the right size but the locals sit at different
offsets, and scope-widening situations where moving a declaration in or out of a nested block
shifts everything. The permuter tries declaration reorders, scope narrows/widens, and slot
padding insertions across the whole function body. Run the source permuter; do NOT hand-edit
cascades > ~10 instructions.

### FPR scheduling
<a id="fpr-scheduling"></a>

`f0`/`f1` vs `f1`/`f2` cascades on functions heavy with inline asm-block math (Mtx.h,
Vec.h's `Normalize`/`Length`/`Cross`/`Multiply`). The permuter explores intermediate-variable
insertion, FMA operand commutation, and float literal hoisting that can knock the scheduler
into the target's choice. Note: some functions in this family are documented at-limit because
the scheduler decision is locked by header-side inline asm, not the .cpp source — see
[paired-single-boxmap-lighting.md](paired-single-boxmap-lighting.md) for an example of the
hard floor. Still try the permuter first. Run the source permuter; do NOT hand-edit cascades
> ~10 instructions.

### Instruction scheduling
<a id="instruction-scheduling"></a>

`subi` vs `addi` for base-pointer arithmetic, `fmadds` vs `fmuls + fadds`, fused-multiply-add
operand commutation, and any "the instructions are right but in the wrong order" diff. The
permuter handles all of these through FMA reorder, statement reorder, and
`#pragma fp_contract` toggling without requiring you to predict which knob the scheduler is
reading. Run the source permuter; do NOT hand-edit cascades > ~10 instructions.

## When NOT to bother

The permuter cannot help with these. Don't burn a sweep slot on them.

- **Source-immune classifications.** ADDRESS_RELOCATION (positional drift from `.text` size
  changes elsewhere), ANON_NAMESPACE_HASH (`?A0x<hash>` path-dependent symbols patched
  post-build), SCOPE_COUNTER_MISMATCH (`$S` static-guard numbering), PROLOGUE_MISMATCH
  (compiler prologue generation quirks), LINKER_MERGED / verifiable ICF cases. See
  [verifiable-icf.md](verifiable-icf.md) for the ICF verification flow.
- **Pure stubs.** The base implementation is empty (or just `return 0`) and the target has a
  real body — there is no source for the permuter to mutate toward the answer. Decomp from
  scratch via `/analyze-function` and m2c instead.
- **Massive structural diffs (<50% match).** The function shape is wrong, not the codegen.
  Permuter mutations are local — they will not invent missing branches or restructure a loop
  body. Start from m2c output and Ghidra pseudo-C; come back to the permuter once the
  structure is right.
- **Symbols on the auto-skip list.** `merged_*`, `fn_*`, `??_B`, `??_9`, anything in
  `stlpmtx_std::`. The permuter refuses these in `batch_auto.py`'s `SKIP_PATTERNS` and the
  query layer; they are unrunnable.
- **Header-locked codegen.** Mtx.h / Vec.h / Timer.h inline-asm cascade families (Part.cpp,
  CharHair.cpp). The decision is made in the header, not the .cpp — see
  [paired-single-boxmap-lighting.md](paired-single-boxmap-lighting.md) for an extreme
  example. Permuter still worth one try since it's cheap, but don't grind.

## How to invoke

The CLI lives at `decomp_synth/batch_auto.py` and is invoked as a module from the repo
root. `--target` selects scope: `unit` for a single source file (or glob fragment),
`workable` for the whole backlog. There is no `--target single`/`--symbol` mode — the
sweeper works in unit batches and picks workable functions out of `decomp.db`. To attack
exactly one function, pass `--unit` for the file that contains it (the unit name in
`objdiff.json`) plus `--limit 1` and a tight `--min-pct`/`--max-pct` window — or just let
the unit sweep cover it.

```bash
# Whole-unit sweep — the most common form
python -m decomp_synth.batch_auto --target unit --unit "system/rndobj/Part"

# Restrict to one function inside a unit by bracketing its current %
python -m decomp_synth.batch_auto --target unit --unit "system/rndobj/Part" \
    --min-pct 92 --max-pct 92.5

# Sweep all workable functions, capped (e.g. an idle-cores background sweep)
python -m decomp_synth.batch_auto --target workable --limit 200

# Dry run — print triage and the list of functions that would be attempted
python -m decomp_synth.batch_auto --target unit --unit "system/char/" --dry-run

# Resume an interrupted run
python -m decomp_synth.batch_auto --resume logs/permuter/auto_YYYYMMDD_HHMMSS
```

Defaults worth knowing:

- `--max-rounds 5` — hill-climbing rounds per function.
- `--max-variants 50` — variants explored per round.
- `--plateau-limit 2` — stop after N rounds without improvement.
- `--workers 0` — auto = `min(nproc - 2, 16)` parallel variant scorers.
- `--include-at-limit` is OFF by default. Pass it explicitly if you want to re-attack
  AT_LIMIT-classified functions after a baseline shift.
- `--no-apply` — score variants but do not write improvements back to source (useful for
  recon).
- The DB is automatically synced from `build/SZBE69_B8/report.json` at startup so candidate
  selection isn't stale; pass `--no-db-sync` to skip.

## Expected outcomes

Empirical baselines from recent sweeps (use these to sanity-check whether a run is healthy
before killing it):

- **Single function:** roughly ~250 variant builds, runs in a few minutes on a workstation
  with 8+ scorer workers. A 5-round/50-variant hill climb caps at 250 builds even when it
  plateaus immediately.
- **Unit sweep (50-200 functions):** ~30-60 minutes for a typical engine file. Wall time
  scales with both the function count and how often hill-climbing actually keeps improving
  (plateau-limited runs finish much faster than 5-round runs).
- **Whole-backlog sweep (`--target workable`):** hours. Use `--limit` for time-boxed runs.
- **Conversion rate:** ~5-10% of attempted functions improve per sweep is typical. A May 26
  2026 sweep of 400 functions improved 25 (+14.9 total match%), 2 going to 100%.
- **"Workable" is conservative.** Many functions report 0 candidates from the classifier yet
  still improve when re-attempted after the baseline shifts (a header tweak, a neighboring
  function reaching 100%, or a new pattern added to the permuter). Re-sweep periodically.

## When the permuter exhausts

A sweep returning 0 improvements is information, not failure. In order:

1. **Read `objdiff verdict.classification`.** If it's AT_LIMIT and the listed mismatches are
   all source-immune (ADDRESS_RELOCATION, ANON_NAMESPACE_HASH, SCOPE_COUNTER_MISMATCH,
   PROLOGUE_MISMATCH, LINKER_MERGED), accept and document. Mark the function AT_LIMIT in
   `decomp.db` so future sweeps skip it. The skip is correct, not defeat.
2. **Try a second sweep after baseline changes.** Hill-climbing is greedy and stops at local
   optima. A header tweak, a neighboring-function fix, or a new permuter pattern landing can
   re-open the candidate space. Re-sweep after meaningful repo changes — especially after
   pulling new patterns from `decomp_synth/patterns/`.
3. **Escalate to manual structural analysis.** Run `/analyze-function SYMBOL` to get
   objdiff + Ghidra + m2c side by side. If the diff is structural rather than scheduling
   (wrong branch shape, missing call, transposed loop), the permuter was never going to
   solve it; reach for `/dc3-pair` for a sister-project reference and rebuild the function
   from the m2c skeleton.
4. **Check the documented at-limit catalog.** Many of the worst offenders (Mtx.h-cascade
   units, paired-singles, header-locked inline asm) are pre-classified — see
   [paired-single-boxmap-lighting.md](paired-single-boxmap-lighting.md) and the at-limit
   notes in the wave-session logs ([wave-session-2026-05-23.md](wave-session-2026-05-23.md)).

## Where the run output goes

`logs/` is **gitignored wholesale** and always should have been — it is run output,
not repo material. (6,700 permuter JSON logs got tracked by accident in 2026-05,
more files than `src/` had at the time; removed 2026-08-30.) Sweeps keep writing
`logs/permuter/auto_YYYYMMDD_HHMMSS/` exactly as before, they just stay local.

A *finished* campaign worth keeping is filed in decomp-bench as a corpus —
`archive/corpora/permuter-sweeps/`, one member per campaign, parquet plus a
byte-faithful snapshot. The 2026-05 campaign that produced most of this page's
empirical numbers is `permuter-sweeps/rb3-2026-05/`. Read its README before
quoting anything from it: those percentages are the sweep's own self-report,
read at objdiff's **`none`** default (this repo did not ship
`functionRelocDiffs=name_check` until `ca01cdbfd`), so a 100 there is not a
byte-exact claim.

The live per-candidate history — `score_cache`, `pattern_runs`, `climb_variant` —
is `permuter_cache.db` at the repo root. It is gitignored, it grows daily, and its
only off-box copy is `decomp-synth tools/backup_dbs.py` → `b2:.../db-backups`.

## Cache & coordination

- The permuter caches scoring results per `(symbol, source_md5, dep_set)` in
  `decomp_synth/cache.db` (SQLite, `busy_timeout` 30s). Stale rows auto-evict when a
  dependency file changes, so concurrent sweeps on disjoint units share the cache safely.
- Do not dispatch overlapping waves on the same unit. Before starting a sweep, check
  `ps -ef | grep batch_auto` and `ls logs/permuter/auto_*` for an in-flight run. Two
  permuter processes writing to the same .cpp will race and one will overwrite the other's
  improvement, silently losing the win.
- Concurrent builds across worktrees are safe (the build dir is serialized via
  `tools/ninja-locked`), but two permuter processes mutating the **same source file** in
  the **same worktree** are not. Either coordinate by unit, or run each agent in its own
  worktree (`tools/setup-worktree.sh`).
- The decomp.db update path is `sync_db_from_report()` at sweep startup. If you run a sweep,
  then a build, then another sweep without restarting, the second sweep may attack already-
  fixed functions. The startup sync handles this for normal usage; only worry if you have
  the DB out of band.

## Cross-references

- [fixable-declarations.md](fixable-declarations.md) — declaration-order / register-coloring
  mechanism behind register cascades.
- [fixable-bool-mask.md](fixable-bool-mask.md) — bool materialization shapes the permuter
  searches over.
- [fixable-control-flow.md](fixable-control-flow.md) — control-flow mutations the permuter
  also covers (branch polarity, early-return inversion).
- [verifiable-icf.md](verifiable-icf.md) — verifying ICF / LINKER_MERGED before accepting an
  AT_LIMIT classification.
- [paired-single-boxmap-lighting.md](paired-single-boxmap-lighting.md) — example of a hard
  floor the permuter cannot cross (paired-singles, header inline asm).
- [INDEX.md](INDEX.md) — full pattern catalog and the match%-tiered decision tree.
