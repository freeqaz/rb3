# pragma_pool_data_wrap

**Pattern file**: `decomp_synth/patterns/pragma_pool_data_wrap.py`
**Status**: B3b (new; opt-in. Proven on `CacheWii::WriteAsync` 85.5 -> 100%
and `CustomizePanel::RotatePatch` 92.9 -> 99.4%).

## What

Bracket an entire function definition with `#pragma pool_data off` / `#pragma
pool_data reset` (or the inverse `on` polarity) so MWCC's BSS/static-pool
allocator does not pool addresses into a callee-saved register for the
duration of that function.

```cpp
#pragma pool_data off
void MyClass::WriteAsync(...) {
    // body
}
#pragma pool_data reset
```

The pattern emits two variants per call site:

- `pool_data off / reset` — disables pooling (the common case; `CacheWii::WriteAsync`).
- `pool_data on  / reset` — re-enables pooling for one function inside a
  surrounding `pool_data off` block (the `RotatePatch` polarity flip).

## Why this moves match%

With `-O4,p -ipa file`, MWCC will hoist a hot BSS/static segment base
into a callee-saved GPR (typically `r29`/`r30`/`r31`) at function entry
and reuse that base across every static access. When the target was
compiled without that pool (or with a different polarity), the prologue
saves one fewer callee-saved register and the rest of the body picks up
the freed register. Wrapping forces our build to match the target's
pooling decision and the prologue + cascade collapses.

The inverse polarity (`pool_data on` inside an outer `off`) is needed when
constants in one function are used exactly once each, so the target emits
each as its own weak `.sdata2` symbol rather than aggregating them into a
per-function `@floatBase0` blob.

## Asm signal

- **Strong** (priority 0.8): `has_prologue_mismatch` AND `gpr_save_delta <
  0` (target saves fewer callee-saved GPRs than we do).
- **Weak** (priority 0.3): any `addi` / `addis` / `lis` diff op or any
  insert/delete cluster — the BSS-base pool is implicated whenever the
  prologue or absolute-addressing shape disagrees.

Because the wrap can regress sibling functions that legitimately share the
pool, the pattern is `opt_in = True`: it is **never** picked up by default
batch sweeps. Trigger it explicitly via:

```bash
python -m decomp_synth.pattern_scan --patterns pragma_pool_data_wrap
```

…or by listing it in `--patterns` on `hill_climber` / `batch_auto`.

## When the pattern fires

- AST: any function definition whose preceding ~4 KB of source does NOT
  already contain a `#pragma pool_data {off|on|reset}` directive.  The
  scan window keeps us from re-wrapping a function that's already inside
  an outer block; we deliberately stop at 4 KB so a stray pragma at the
  top of a huge TU doesn't suppress every other function in the file.
- Two variants per matching function (`off` and `on` polarity).

## Repo footprint

`#pragma pool_data` appears in ~25 source files today — see
`rg 'pragma pool_data' src/`. The pattern will fire on any function in
those files that lives outside an existing pragma block, plus every
function in clean files. Use the asm-signal gate (prologue delta) to pick
real candidates from the firehose.

## Cross-reference

- MEMORY: `feedback_pragma_pool_data_off.md`, `feedback_customizepanel_rotatepatch_pool.md`
- Original proofs:
  - `CacheWii::WriteAsync` 85.5 -> 100% (`off` polarity)
  - `CustomizePanel::RotatePatch` 92.9 -> 99.4% (`on` polarity inside outer
    `off` block, commit `450a7a35`)
- Related: `fixable-macros.md`, `lwzu-idiom.md` (sister codegen-control
  pattern), `fixable-inline-boundary.md`
