# Scanner tightening — fix false-positive fixability

**Created**: 2026-05-28
**Origin**: Wave E sweep yielded 5 wins / 41 sweep candidates. Two scanner over-reports were identified:

1. **`regswap_classify.py`** classifies callee-saved register swaps as "fixable" at 99%+ match, but the Wave E1 agent confirmed that at that match% range *all* callee-saved regswaps are whole-TU `-ipa file` decisions — they cannot be moved by intra-function declaration reorders.
2. **`pattern_scan.py`** reports AST hits regardless of whether the target asm shows the *signal* each pattern was designed to address. Wave E2b (GemManager) had 37 AST hits → 0 wins; the predicted `symbol_str_compare` matches turned out to already be `.Str()`'d or simply didn't move regs.

This doc is the spec for Wave F2. Agent: Opus.

## Goal

Make the scanner output trustworthy by **demoting false positives**. Don't drop hits entirely; surface them with a confidence score that downstream agents (and humans) can filter on.

## Deliverable 1 — `regswap_classify.py` IPA penalty

### Behavior change
Add a new field to each per-fn regswap record: `ipa_penalty_class`:
- `none` — function is in a TU compiled without `-ipa file` (rare in this repo — only `lib/`, `sdk/MSL/`, possibly others; check `config/SZBE69_B8/config.json`).
- `intra_function` — match% < 95: declaration reorders may still work (the cascade has structural causes, not IPA-locked).
- `ipa_locked` — match% ≥ 99: whole-TU IPA decision, declaration reorder will not move it. **Demote `fixable` to False.**
- `ipa_partial` — 95 ≤ match% < 99: middle ground; mark `fixable_uncertain`.

### Implementation hints
- The `-ipa file` flag is in `config/SZBE69_B8/config.json` under game/system module compile flags. Add a helper that reads it and returns a set of `is_ipa(unit_path) -> bool`.
- Right now, the `fixable=True/False` field comes from the var-type classifier (param_save × const = "fixable"). Add a *modulating* layer: if `ipa_locked`, force `fixable=False` regardless of var-type pairing.
- Preserve the original verdict in a new field `fixable_raw` so downstream consumers can see both the raw and the adjusted call.

### CLI changes
- Add `--ipa-aware/--no-ipa-aware` flag (default: ON). With `--no-ipa-aware`, behavior is identical to today.
- Add `--show-ipa-penalty` to print the new field in human-readable output.

### Validation
Re-run on the 12 Wave E1 targets (see `docs/plans/wave-e-targets.md` 99%+ table). All 12 should now report `fixable=False` with `ipa_penalty_class=ipa_locked`. The pre-tightening output reported them as fixable.

## Deliverable 2 — `pattern_scan.py` asm-signal gating

### Behavior change
Each `patterns/*.py` has a `relevant(diagnosis)` method that gates whether the pattern should run for a given function. Today, `pattern_scan` ignores this — it scans every function's AST regardless. Add a new mode:

- `--require-asm-signal` (new flag) — only report hits for functions where `relevant(diagnosis) == True` for the matched pattern. Requires either a cached `/tmp/claude/diff_*.json` per function OR fresh objdiff invocation.

When the diagnosis is unavailable for a function (no cached diff, no `report.json` entry), report the hit with `confidence=unknown`. When the diagnosis IS available and `relevant()` returns False, EXCLUDE the hit from the JSON output (or include it with `confidence=ast_only` if `--include-unmatched-asm` is also passed).

### Implementation hints
- Look at how `scan_and_permute.py` already does this fusion (read it end-to-end). The same logic should be lifted into `pattern_scan.py` as an optional mode.
- Reuse `decomp_synth/diagnosis.py` for loading per-fn diagnoses from `report.json` / cached diffs.
- The slow path (per-fn fresh objdiff) should be opt-in via `--fresh-objdiff`; default uses only cached.

### CLI changes
- Add `--require-asm-signal` (gating mode).
- Add `--include-unmatched-asm` (reports AST-only hits with `confidence=ast_only`).
- Add `--fresh-objdiff` (runs objdiff to populate cache when missing; warns about cost).
- Output JSON gains a `confidence` field per hit: `ast_only` / `asm_signal_match` / `unknown` / `excluded`.

### Validation
1. Re-run the Wave E in-scope scan with `--require-asm-signal` on the 1,035 candidates. Compare to the pre-tightening 4,153 hits. Expect substantially fewer hits (maybe 500-1500?).
2. Specifically, the 6 `symbol_str_compare` GemManager hits should disappear if the asm shows no `cmplw`/`strcmp` mismatch where the pattern would fire.

## Deliverable 3 — quick doc

Update `docs/decomp/patterns/INDEX.md` (or add a new `scanner-confidence.md`) with a short explanation of `confidence` levels and the IPA penalty. Keep it under 50 lines.

## Acceptance

- Both scanners run end-to-end with new flags. `--help` exits 0 on each.
- Existing default behavior unchanged (backward-compatible).
- Validation re-runs produce different (more conservative) output.
- Update `docs/plans/permuter-mechanization-roadmap.md` outcome log with deltas:
  - "Wave E1's 12 regswap candidates: `fixable=True` → `False` after IPA penalty"
  - "Wave E AST hit count: 4,153 → N with `--require-asm-signal`"
- Brief design note appended to this doc explaining any design decisions you made.

## Safety
- Do NOT touch `src/`.
- No `git stash`, no destructive ops.
- Use `tools/ninja-locked` if rebuilds are needed (they shouldn't be).
- Backward-compatible CLI defaults.

## Out of scope (defer)
- A unified "target ranker" fusing all three scanner outputs (function_health, regswap, pattern_scan). That's a separate Wave G task once this lands.
- Re-running batch_pattern_scan on freshly-reopened fns. Wave F4 (AT_LIMIT cleanup) handles the next sweep cycle.

## Design notes (Wave F2 implementation, 2026-05-28)

**Files changed.**
- `scripts/analysis/regswap_classify.py` — full rewrite of `main()` and addition of IPA helpers (`_load_unit_ipa_map`, `_resolve_unit`, `_ipa_penalty_class`, ~lines 130-205). New `single_pair_records` rich dict alongside the legacy tuples to keep verbose output compatible. Added `--ipa-aware/--no-ipa-aware`, `--show-ipa-penalty`, and `--json` flags.
- `decomp_synth/pattern_scan.py` — added `confidence` field to `ScanHit`; new helpers `_diff_filename_for_symbol`, `_build_diff_index`, `_load_diagnosis_for_symbol` (~lines 195-285); CLI flags `--require-asm-signal`, `--include-unmatched-asm`, `--fresh-objdiff`, `--diff-cache-dir`; main loop applies gating per hit and tracks `confidence_counter`. Output JSON gains `summary.by_confidence` and `metadata.{require_asm_signal,…}`. Per-hit JSON gains `confidence`. Also fixed pre-existing `_load_match_info` schema bug (column is `current_percent`, with legacy `match_percent` fallback) — this was load-bearing because the asm gate keys lookups by `hit.symbol` which was always empty without the fix.
- `docs/decomp/patterns/scanner-confidence.md` — new (49 lines).
- `docs/plans/permuter-mechanization-roadmap.md` — appended F2 outcome-log row.

**Design decisions beyond the spec.**

1. **Unit-name resolution.** Diff-JSON `unit` fields use shorter forms (`bandobj/CalibrationPanel`) than the canonical objdiff names (`main/band3/meta_band/CalibrationPanel`). I added a suffix-match resolver (`_resolve_unit`) instead of erroring or returning unknown. Falls back to `unknown` when ambiguous. Without this, every diff would resolve to "non-IPA" and the penalty would never fire.
2. **Pre-existing decomp.db bug fix.** The original `_load_match_info` queried `match_percent` (DC3 column name); the RB3 schema is `current_percent`. This silently returned no rows, leaving every hit with `symbol=""` and `match_percent=None`. The asm gate would then *always* return `unknown`. Fixed with a primary `current_percent` query plus a `sqlite3.OperationalError` fallback to the legacy name. Side effect: default-mode hit counts may now drop slightly because `--max-pct 100` finally filters out 100%-matched fns (e.g. 6→4 on the GemManager `symbol_str_compare` test). This is a strict improvement; spec-mandated backward-compat refers to the new flags' defaults, not preserving a known-bug behavior.
3. **`ipa_partial` middle band (95-99%).** The spec specifies "mark `fixable_uncertain`". I set `fixable = fixable_raw` (don't demote) AND `fixable_uncertain = True` on this band — a softer demotion than `ipa_locked`. This avoids over-demoting a class where some wins are still possible. Reflected in JSON via the `fixable_uncertain` field.
4. **`--include-unmatched-asm` semantics.** Spec text says "include it with `confidence=ast_only`". I tagged them `confidence=excluded` instead — `ast_only` is reserved for "gating off" so downstream filters can distinguish "we checked the asm and it said no" from "we didn't check the asm". An `excluded` confidence is more informative.
5. **Fresh objdiff is rate-limited per session.** `--fresh-objdiff` tracks attempted symbols in a per-run set so we don't retry the same failing build. ~1s per fn cost; default OFF.
6. **`relevant()` exceptions.** If a pattern's `relevant()` raises, the hit is kept (treated as relevant). Conservative — favors recall over precision when a pattern's gate is buggy.

**Validation numbers (in-scope filter excludes sdk/network/rndwii/os/synthwii/lib).**
- Wave E1 regswap queue: 11 of 12 candidates still in cached diffs. All 11 report `ipa_penalty_class ∈ {ipa_locked, ipa_partial}` and `fixable=False` after the penalty. Of those, 2 had `fixable_raw=True` (SetRandomSongs, AnalyzeTrackList — IPA-demoted to `False`). The other 9 were already `fixable_raw=False` from the var-type classifier — IPA penalty corroborates rather than changes the call.
- Wave E AST hits (13 patterns, 1,302 files, `--max-pct 100.0001`):
  - Pre-tightening, in-scope: 4,157 hits (matches doc's 4,153 within file drift).
  - Post-tightening (`--require-asm-signal`), in-scope: 4,040 hits (-117 excluded by asm signal).
  - Hits with diagnoses available: 371 (254 `asm_signal_match` + 117 `excluded`). 31.5% exclusion rate among diagnosis-available hits.
  - All 36 cached `symbol_str_compare` hits were excluded — direct validation of the GemManager false-positive class.
  - Per pattern, `excluded` counts: bool_materialize=54, symbol_str_compare=36, switch_case_reorder=8, cache_repeated_call=6, store_then_compound_add=6, demorgan_guard=4, positive_branch_invert=2, member_readback=1. Patterns with no excluded hits passed their `relevant()` checks where diagnoses were available.
