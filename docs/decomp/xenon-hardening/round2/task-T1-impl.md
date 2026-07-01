# Task T1 — Two-pass VT rescue: `--matches-only` + ACCEPT-only seeds (impl record)

Agent: opus (T1). Date: 2026-06-11. Branch checks done immediately before each commit.

This doc covers the **implementation + verification** of the three code changes
and the seed builder. The **experiment results** (THE RUN + eval) live in the
sibling `task-T1-twopass.md`.

## TL;DR

- ghidriff fork `rb3-improvements` gains `--matches-only` (commit
  `e52d935`): early-exits `diff_bins()` right after `pdiff['matches']` is built,
  skipping the ~107-min post-match dedup/esym/report stage. Writes only
  `json/<name>.matches.json`.
- rb3 `master` (commit `e1693918`): runner gains `RB3_XENON_SEEDS` +
  `RB3_XENON_MATCHES_ONLY` env knobs (defaults unchanged); new
  `tools/ghidra/build_accept_seeds.py` builds the 2,130-pair ACCEPT-only seed
  set with anti-leak + strict-1:1 filtering.
- All offline verification passed (help, argparse replay, bash -n, 4-row
  dry-run table, seed-builder assertions, JVM-free ghidriff tests 45/47 —
  the 2 failures are pre-existing stale-artifact replay tests, unrelated).

## What changed (file:line, commit shas)

### ghidriff fork (`/home/free/code/milohax/ghidriff`, branch `rb3-improvements`, commit `e52d935`)

Per scout-report-cost.md §10's six-item checklist, plus one extra guard the
scout's design missed (item 5b below):

1. **`ghidra_diff_engine.py` `add_ghidra_args_to_parser()`** (~line 410, after
   `--skip-correlators`): added `--matches-only` `store_true`, default `False`.

2. **`ghidra_diff_engine.py` `__init__()`** (~line 238, after
   `self.decomp_correlate_min_ratio`):
   `self.matches_only = getattr(args, 'matches_only', False)`
   (`getattr` default so sibling engines passing `args=None` don't crash).

3. **`ghidra_diff_engine.py` `diff_bins()`** (after line 1707,
   `pdiff['matches']['function_matches'] = function_matches`): early-exit block —
   logs, `self.shutdown_decompilers(p1, p2)`, `self.esym_memo = {}`,
   `self.project.close(p1)`, `self.project.close(p2)`, two "Finished diffing"
   log lines, `return pdiff`. This mirrors the normal return path (engine lines
   1988–2001) exactly, so no decompiler/program leak. `pdiff` at this point holds
   only `program_options` (set at line 1653–1665) + `matches` — both valid.

4. **`ghidra_diff_engine.py` `minimise_pdiff()`** (first line of body, ~line
   2118): `if 'functions' not in pdiff: return pdiff`. REQUIRED — without it,
   `dump_pdiff_to_path` → `minimise_pdiff` KeyErrors on `pdiff['functions']`
   (absent in matches-only mode).

5. **`__main__.py` `main()`** (the `for diff in diffs:` loop): wrapped
   `json.dumps(pdiff)` + `validate_diff_json` + `dump_pdiff_to_path(...)` in
   `if not getattr(args, 'matches_only', False):`, with an `else:` branch calling
   `dump_pdiff_to_path(diff_name, pdiff, output_path, write_diff=False,
   write_json=False, write_matches=True)`.

5b. **`ghidra_diff_engine.py` `dump_pdiff_to_path()`** (the early-out guard,
   ~line 2172) — **NOT in the scout checklist; a bug the scout's design would
   have hit.** The original guard was
   `if not write_diff and not write_json and not side_by_side: return`. In
   matches-only mode the `else:` branch calls with `write_diff=False,
   write_json=False, side_by_side=False` → the guard `return`s BEFORE the
   `write_matches` block runs, so **matches.json would never be written**. Fix:
   added `and not write_matches` to the guard condition. Now the function only
   short-circuits when literally nothing is requested.

Net: ~38 lines added across 2 files, all additive. Flag off by default → the
full-report path is byte-for-byte unchanged.

### rb3 (`/home/free/code/milohax/rb3`, branch `master`, commit `e1693918`)

6. **`tools/ghidra/run_ghidriff_xenon.sh`**:
   - `SEEDS="${RB3_XENON_SEEDS:-…/xenon-seeds/seeds.json}"` (seed override).
   - After the `CMD=(…)` array: `MATCHES_ONLY_FLAG="${RB3_XENON_MATCHES_ONLY:+--matches-only}"`
     then `[[ -n "${MATCHES_ONLY_FLAG}" ]] && CMD+=("${MATCHES_ONLY_FLAG}")`
     — appended only when set, so an empty arg is never passed to argparse.
   - `OUT_DIR`/`PROJ_DIR` UNCHANGED (constraint 2: the analyzed project is reused).

7. **`tools/ghidra/build_accept_seeds.py`** (new): reads
   `run3-archive/vetted_identities.json`, keeps `tier=="ACCEPT"`, drops
   holdout-overlap (anti-leak), enforces strict 1:1, writes
   `xenon-seeds/seeds_accept_run3.json` (+ `.stats.json`). The output JSON is
   **gitignored** (`git check-ignore` confirmed) — it's a regenerable artifact,
   so only the script is committed.

## Seed-builder numbers (all assertions PASS)

```
ACCEPT entries:                       2207   (== expected 2207; 0 null wii_addr)
holdout xenon addrs:                  146
ACCEPT in holdout (dropped, anti-leak): 73   (== expected 73)
  drop match_types[0]: ExactInstructionsFunctionHasher 24, BSIM 47, Implied Match 2
kept after holdout drop:              2134
duplicated p1 (Wii):   ['0x807995c0']
duplicated p2 (Xenon): ['0x827a6378']
pairs removed by strict-1:1 dedup:    4
  0x80497d00 -> 0x827a6378  [SeedMatch]     __ct__6HxGuidFv
  0x807995c0 -> 0x827bb4f0  [BSIM]          Init__11TrackWidgetFv
  0x807995c0 -> 0x827bb458  [Implied Match] Init__11TrackWidgetFv
  0x80497e20 -> 0x827a6378  [Implied Match] Clear__6HxGuidFv
FINAL seed pairs:                     2130
ASSERT zero seeds∩holdout:            PASS  (2130 seed p2 addrs, 0 in holdout)
ASSERT strict 1:1 (unique p1 and p2): PASS
```

Final count **2,130** (planner estimated ~2,131–2,133; the difference is that
dropping ALL pairs sharing a duplicated addr removes 4 pairs total — 2 from the
dup p1, 2 from the dup p2, no overlap — not just 2). 2,130 is within the
broader expected band and the discrepancy is fully accounted for above.

Address orientation (round-1 ground truth, re-verified): vetted `wii_addr`
(0x80…) → seed `p1_addr` (Wii Bank 8); vetted `xenon_addr` (0x82…) → seed
`p2_addr` (Xenon). Matches the canonical seeds.json (seeds[0]
p1=0x8000fb10/p2=0x82260018 == vetted[0]).

## Offline verification (all done BEFORE the run)

- **`--help` shows the flag**:
  `ghidriff-venv/bin/python -m ghidriff --help | grep matches-only` →
  `--matches-only        Skip all post-match processing …`  ✓
- **Syntax**: `ast.parse` both edited ghidriff files → SYNTAX OK; `bash -n`
  runner → OK. ✓
- **Argparse replay of the EXACT runner CMD** (the THE-RUN config: matches-only
  + accept seeds), fed through `get_parser()` + `add_ghidra_args_to_parser()`:
  - `args.matches_only is True` ✓
  - all existing flags intact (`force_diff`, `bsim`, `seed_matches` ends with
    `seeds_accept_run3.json`, `vt_ref_correlators`, `vt_ref_min_score=9.5`,
    `min_func_len=16`, `implied_min_ratio=0.9`, `skip_correlators=…`,
    `decomp_correlate is False`, `decompiler_timeout=20`,
    `engine=VersionTrackingDiff`) ✓
  - negative control: without `--matches-only`, `args.matches_only is False` ✓
- **4-row dry-run env table** (`{MATCHES_ONLY unset/1} × {SEEDS unset/accept}`):
  | MATCHES_ONLY | SEEDS | --matches-only in CMD? | seed path |
  |---|---|---|---|
  | unset | unset | NO | …/seeds.json |
  | 1 | unset | YES | …/seeds.json |
  | unset | accept | NO | …/seeds_accept_run3.json |
  | 1 | accept | YES | …/seeds_accept_run3.json |
  (rows 3–4 also exercised the prereq check: with a non-existent seeds path the
  runner correctly errors out before launching the JVM.)
- **JVM-free ghidriff tests**: `pytest tests/test_fast_core.py
  tests/test_score_export.py tests/test_string_hasher_gate.py` →
  **45 passed, 2 failed**. The 2 failures
  (`test_replay_stl_exclusion_over_real_vt_pool`,
  `test_replay_existing_matches_json_has_no_scores_field`) are **pre-existing
  stale-artifact replay tests**: they read the LIVE run-3 matches.json
  (`…/ghidriff-xenon/json/…matches.json`, 1093 VT matches WITH scores) but
  assert an old baseline (722 VT, no scores). They are independent of my edits
  (which touch the `--matches-only` flow + `dump_pdiff_to_path` guard, not
  `build_function_match_entry` or VT export). pytest + pytest-datadir were
  pip-installed into the ghidriff venv to run these (not committed; venv is not
  tracked).

## Caveats

- `dump_pdiff_to_path` guard fix (5b) is load-bearing and was NOT in the scout
  checklist — without it the matches-only run writes nothing. Verified by
  reading the function body (engine line 2142 original guard).
- The 2 stale-artifact test failures will keep failing until someone refreshes
  the test's expected baselines to the run-3 (or round-2) artifact; that is out
  of T1's scope and unrelated to `--matches-only`.
- Seed count is 2,130, not the planner's ~2,131–2,133 point estimate (explained
  above). The anti-leak (73) and ACCEPT (2207) counts match exactly.

## For the next agent / verifier

- **THE RUN executed successfully** (2026-06-11 10:27–10:35 UTC, exit 0,
  matching 7.4 min, `--matches-only` early-exit fired, report stage skipped).
  Results + verdict in `task-T1-twopass.md`. Headline: VT NOT rescued
  (0.093 raw / 0.222 alias vs run-3 0.109/0.236) → **hypothesis REFUTED**,
  demote VT to CAUTION permanently. `--matches-only` saved ~107 min.
- The seed file `build/SZBE69_B8/ghidra/xenon-seeds/seeds_accept_run3.json`
  (2,130 pairs) + `.stats.json` are gitignored but on disk. Regenerate with
  `ghidriff-venv/bin/python tools/ghidra/build_accept_seeds.py`.
- THE RUN command (already executed by T1; see task-T1-twopass.md):
  `RB3_XENON_MATCHES_ONLY=1 RB3_XENON_SEEDS=$PWD/build/SZBE69_B8/ghidra/xenon-seeds/seeds_accept_run3.json ./tools/ghidra/run_ghidriff_xenon.sh`
- EVAL must pass `--seeds …/seeds_accept_run3.json` (eval defaults to the OLD
  seeds.json, which corrupts seed-exclusion/holdout-eligibility math).
- Commits: ghidriff `e52d935` (rb3-improvements); rb3 `e1693918` (master).
