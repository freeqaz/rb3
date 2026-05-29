# `switch_case_reorder` pattern

**File**: `decomp_synth/patterns/switch_case_reorder.py`
**Wave**: B5 of the [permuter mechanization roadmap](../../plans/permuter-mechanization-roadmap.md)
**Origin**: MEMORY `feedback_switch_case_emission_order.md`. Canonical win is
`SaveLoadManager::Poll` 64.1% -> 88.2% (+24pp) from a pure case-clause reorder.

## What it does

MWCC emits switch-case bodies in **source order**, not in jump-table-index
order. When the source case order disagrees with the order the target compiler
used, every downstream stack offset, branch target, and register pick shifts —
producing a huge cascade of `insert` / `delete` / `diff_arg` that looks
structural but is purely ordering.

The pattern reorders the case clauses while preserving each clause's body
verbatim. It only fires when every case has a hard terminator (`break;`,
`return ...;`, `continue;`, `goto`, `throw`), so the reorder is
behaviour-neutral.

## Two phases

### Phase 1 — AST-only permutation fallback (always available)

For any switch with **>=3** reorderable case groups, yields up to 6 variants:

- `reverse` — full reversal
- `swap_i_i+1` — a few adjacent pair swaps
- `rand` — a couple of randomised permutations (seeded)

This is the safety net: cheap, always available, sometimes lucky. It's the
brute-force complement to the deterministic Phase 2.

### Phase 2 — asm-jump-table-guided reorder (when target `.s` is available)

When the target asm listing is reachable (`build/<BUILD_ID>/asm/<unit>.s`),
the pattern:

1. Scans the function body for `"@NNNNN"@ha` / `@l` reloc references — these
   are the jump tables the function dispatches through.
2. Finds the matching `.obj "@NNNNN", local` block and parses each
   `.rel <SYMBOL>, .L_<hex>` entry. Position in the list = source case value
   (state index); the `.L_<hex>` address = the body label.
3. Detects the **default label** as the most common destination (every
   out-of-range state slot routes to the default body).
4. For each source case group, resolves its case-value(s) to body addresses
   via the jump table; defaults map to the detected default label.
5. Sorts the source groups by **ascending body address**. That sorted order
   IS the source-emission order the target compiler used.
6. Emits one variant rearranging source case groups to match.

Phase 2 is the high-signal variant — it's deterministic and informed by the
target binary.

## Safety rules

- Refuses to reorder if **any case has no terminator** (fall-through into the
  next case).
- Treats fall-through groups (`case 3: case 4: body`) as a single unit that
  moves together.
- Refuses if **any case body contains a `goto` to a label inside another
  case**. Such cross-case `goto`s rely on source layout (see
  `src/system/synth/StreamReceiver.cpp` `Play()` for an example we
  correctly refuse).
- Phase 2 cleanly bails when:
  - The unit's `.s` file is missing
  - The function references no jump-table object
  - Any case value is a symbolic enum we can't resolve to an int
    (e.g. `kS_Start`). Phase 1 still fires in that case.

## Detection (`relevant`)

The pattern is gated on diagnosis signals typical of structural reorder:

- Branch-opcode mismatches in `diff_ops` (`beq`/`bne`/`b`/`bl`/`lwzx`)
- Large clusters (`size >= 4`)
- `replace_real >= 3`

These all suggest the kind of layout shift a case reorder fixes. `priority`
returns 0.3 - 0.7 depending on cluster size; `context_priority` boosts to at
least 0.5 when the function actually contains a reorderable switch.

## Phase status

- Phase 1: implemented and smoke-tested. `pattern_scan` finds hits across
  `band3/meta_band/SaveLoadManager.cpp` (Poll = 30 cases, SetState = 81
  cases via inner switches, several smaller ones).
- Phase 2: implemented and unit-tested against a synthetic jump table.
  Cleanly bails on switches whose case values are symbolic enums and on
  switches not dispatched via a jump table. The synthetic test in the
  pattern's docstring exercises the full pipeline.

## Use it

```bash
python -m decomp_synth.pattern_scan --patterns switch_case_reorder \
    --source src/band3/meta_band/SaveLoadManager.cpp --show-variants
```

For a real hill-climb on a candidate, drive it via `batch_auto` or
`hill_climber.py`. Phase 2 fires automatically when the `.s` listing is
present (every RB3 build dir has them).

## Related

- MEMORY: `feedback_switch_case_emission_order.md`
- Sibling pattern: `switch_if_convert` (switch <-> if/else-if chains; different
  axis — converts shape rather than reorders cases)
- Follow-ups: `branch_polarity`, `declaration_reorder`
