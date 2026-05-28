# Scanner confidence & IPA penalty

Wave F2 (2026-05-28) tightened the two main scanners to demote false
positives the LLM agents kept biting on. Output is unchanged unless you
opt in via new flags — both scanners stay backward compatible.

## `regswap_classify.py` — IPA penalty

Game and Milo-engine units compile with `-ipa file`, which locks
callee-saved register choices at the whole-TU level. Once the function is
at 99%+ match, the swap remaining is virtually always one of those IPA
decisions — declaration reorders inside the function will not move it
(confirmed by Wave E1: 12 candidates, 0 wins after permuter + manual edits).

New per-record fields (JSON via `--json` or verbose via `--show-ipa-penalty`):

- `fixable_raw` — original var-type verdict (param_save × param_save etc.).
- `ipa_penalty_class`:
  - `ipa_locked` — unit has `-ipa file` AND match ≥ 99%. **Demotes `fixable=False`.**
  - `ipa_partial` — `-ipa file` unit at 95-99%. Borderline; `fixable_uncertain=True`.
  - `intra_function` — `-ipa file` unit below 95%. Structural fixes plausible.
  - `none` — unit has no `-ipa file` (rare; mostly `network/Platform`, `system/zlib`).
  - `unknown` — objdiff.json missing or unit didn't resolve.
- `fixable` — `fixable_raw` modulated by IPA penalty (always `False` when `ipa_locked`).

Flags:
- `--ipa-aware` (default ON), `--no-ipa-aware` to disable.
- `--show-ipa-penalty` to surface the new column in verbose output.
- `--json` for machine-readable per-fn records.

## `pattern_scan.py` — asm-signal gating

Each pattern declares a `relevant(diagnosis)` predicate. Default mode ignores
it and reports every AST match (4,153 in the Wave E sweep). The new
`--require-asm-signal` mode fuses AST hits with cached diagnoses to drop or
demote hits whose target asm doesn't actually show the signal the pattern
was built to address.

Each hit gains a `confidence` field:

- `ast_only` — gating off; legacy behaviour.
- `asm_signal_match` — diagnosis available, `relevant()` returned True.
- `unknown` — diagnosis unavailable (no cached diff). Kept by default.
- `excluded` — diagnosis available, `relevant()` returned False. Dropped
  unless `--include-unmatched-asm` is also set.

Flags:
- `--require-asm-signal` — enable gating.
- `--include-unmatched-asm` — keep excluded hits (tagged `confidence=excluded`).
- `--fresh-objdiff` — opt-in: shell out to `bin/objdiff-cli` to populate
  missing diffs (~1s per fn).
- `--diff-cache-dir DIR` — override the diff cache location (default `/tmp/claude`).

In the Wave E rerun (1,302 files, 13 patterns), gating dropped 117 in-scope
hits (~31% of those with diagnoses) and surfaced 254 `asm_signal_match`
high-confidence candidates. All 36 cached `symbol_str_compare` hits were
excluded — confirming the GemManager false-positive class.
