# Constraint-solver loop (Wave C1)

**Created**: 2026-05-27
**Status**: Implemented (default ON)
**Owner**: Wave C1 sub-agent

## What changed

`decomp_synth/hill_climber.py::hill_climb` now re-fires `constraint_solver.synthesize` at **every** round, not only round 1. Each round re-extracts a fresh `FunctionContext` from the newly-applied source (`extract_function` already runs per round), so subsequent synthesis runs see updated diagnosis state, mutated var-order, and post-edit `mismatch_regions`. Variants that produce identical source bytes to an earlier attempt are filtered out before reaching the scorer.

## Why

The roadmap audit flagged this as the single highest-impact engine change.

- Round 1's `synthesize()` builds a `ConstraintSet` from the *initial* asm. After round 1 lands a winning edit, the next round's asm has a different residual mismatch profile — different `diff_ops`, different `reg_swap_pairs`, different `target_var_order` resolution. The deterministic Ghidra/m2c oracle was previously ignored for that fresh state and the hill-climber fell back to blind pattern search.
- The Scorer already MD5-dedupes source bytes (`scorer.py:_check_dedup`), so re-firing on a stable source is naturally cheap. The new round-local `synth_seen_sources` set skips even the score_batch round-trip for already-attempted bytes, cutting worker spin-up.
- `beam_search.py` already runs `synthesize()` on every beam state (`beam_search.py:611-622`) — the hill-climber was the laggard. This unifies behavior.

## How it works

```
for round in 1..max_rounds:
    ctx = extract_function(...)            # re-parsed each round
    baseline = scorer.get_baseline(...)
    if constrained and (loop_constraints or round == 1):
        synthesis = synthesize(ctx)
        # round-1 only: honor skip_reason="unfixable"; post-round-1 the
        # source has mutated and skip_reason is unreliable.
        fresh = [v for v in synthesis.variants if v.source not in synth_seen_sources]
        synth_seen_sources |= {v.source for v in fresh}
        if fresh:
            results = scorer.score_batch(fresh)
            if best > baseline: apply and update baseline
```

Design choice **A** (run-every-round + scheduler-level dedup) over B (state-fingerprint gate) or C (cadence-based). Justification:
- A is the simplest and most predictable.
- Caches make the cost-when-unchanged near-zero: Ghidra cache hits, m2c cache hits, no scorer round-trip on dup.
- B's state fingerprint adds another moving piece to debug.
- C's cadence is arbitrary and may miss a productive moment.

## Disabling it

If looping ever regresses, two escape hatches:

1. **CLI**: `python -m decomp_synth.hill_climber --no-loop-constraints` (CLI default is ON).
2. **Env var**: `PERMUTER_LOOP_CONSTRAINTS=0` (wins over the CLI default; useful for batch_auto sweeps or temporary kill-switch without code edits).

Both fall back to the legacy round-1-only behavior — identical pre-change codepath.

## Files modified

- `decomp_synth/hill_climber.py`
  - Added `loop_constraints: bool = True` parameter to `hill_climb()` (plus the matching CLI flags `--loop-constraints` / `--no-loop-constraints`).
  - Added `synth_seen_sources: set[bytes]` initialized with the original source before the round loop.
  - Replaced the `round_num == 1` synthesis gate with `(loop_constraints or round_num == 1)`.
  - `skip_reason` is still only fatal on round 1.
  - `RoundResult.round_num` for a synth-driven perfect-match is now the actual `round_num` (was hard-coded to `0`).
- `decomp_synth/tests/test_loop_constraints.py` (new, 7 tests).

## Files unchanged (intentionally)

- `decomp_synth/constraint_solver.py` — no behavior change required. `synthesize()` is idempotent on identical input (its own `seen_sources` already dedups within a single call; the new cross-round dedup is the caller's responsibility).
- `decomp_synth/beam_search.py` — already loops `synthesize()` per beam state.

## Validation

- Unit tests: 7 new tests in `test_loop_constraints.py` cover the API surface, the
  loop-fires-every-round behavior, dedup of repeat synth output, and the env-var override.
- All 204 hill-climber-adjacent tests still pass.
- Full permuter test count vs pre-change: 0 new failures introduced. The
  remaining 18 failures pre-date this change — 15 of them fail identically when
  run from the dc3-decomp checkout (shared test-code bugs), and 3 are
  CWD-or-dialect dependencies inherent to the dc3 fixtures (`msvc-src/...`).

## Future work

- The new `synth_seen_sources` set could be exposed to `beam_search.py` for the
  same cross-state dedup it currently doesn't do (today, beam_search re-fires
  synth at each state but lets the Scorer's MD5 layer absorb the dup work).
- The opportunity surface for C2 (stack-slot oracle wired into the solver) is
  unchanged by C1.
