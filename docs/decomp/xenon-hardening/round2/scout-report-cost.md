# Scout S3: ghidriff post-match cost autopsy + --matches-only design

**Task:** Trace where the ~107-min post-match stage goes, identify the earliest
point matches.json content is ready, and design a `--matches-only` flag to skip
everything else.

---

## 1. Run-3 timing breakdown

The log at `build/SZBE69_B8/ghidra/ghidriff-xenon/ghidriff.log` covers four
separate runs (four JVM starts). Run 3 (the "validating re-run") started at
`2026-06-10 18:53:30`. All times below are from that run.

| Phase | Start | End | Duration |
|---|---|---|---|
| JVM start + analysis + matching | 18:53:30 | 19:01:53 | **8.4 min** |
| Dedup O(n×m) loop | 19:01:54 | 20:26:44 | **84.8 min** |
| esym lookups (145,448 syms, 32 threads) | 20:26:45 | 20:48:42 | **22.0 min** |
| Write md + full json + matches json | 20:48:42 | 20:48:55 | **0.2 min** |
| **Post-match total** | 19:01:54 | 20:48:55 | **107.0 min** |

Key log lines:
```
2026-06-10 19:01:54,590  Generating matches json...        # pdiff['matches'] built
2026-06-10 19:01:54,698  Deduping symbols and functions... # O(n×m) starts
2026-06-10 20:26:44,616  Sorting symbols and strings...    # O(n×m) ends
2026-06-10 20:26:45,437  Starting esym lookups for 145448 symbols
2026-06-10 20:48:42,850  Finished diffing old program      # diff_bins() returns
2026-06-10 20:48:52,978  Writing md diff...
2026-06-10 20:48:53,263  Writing pdiff json...             # 209MB
2026-06-10 20:48:55,353  Writing matches json...           # 4.9MB
2026-06-10 20:48:55,501  Wrote matches json                # done
```

Write times: md 0.3s, full json (209MB) 2.1s, matches json (4.9MB) 0.15s.
All three writes are negligible once pdiff is built — writing is not the
bottleneck.

---

## 2. Where each phase lives in the code

### 2a. The 84.8-min dedup loop

`ghidra_diff_engine.py:1709–1725` (`diff_bins()`):

```python
# line 1709
self.logger.info('Deduping symbols and functions...')

dupes = []
for func in unmatched:                     # ~54k unmatched functions
    for sym in unmatched_nf_syms:          # ~unknown number of non-func syms
        if func.getName(True) == sym.getName(True) and func.getProgram() != sym.getProgram():
            dupes.append(func)
            dupes.append(sym)
```

This is O(|unmatched| × |unmatched_nf_syms|). At run-3 scale:
- `p1 missing = 26,169`, `p2 missing = 27,949` → ~54k unmatched functions
- `unmatched_nf_syms` comes from `diff_nf_symbols()` which walks ALL
  non-function, non-DEFAULT symbols in both programs. At 65k+37k functions
  each program has many more non-function symbols (strings, data labels,
  thunks). A rough count is likely tens of thousands.
- The `in` membership checks on lists (`dupe in unmatched`) are O(n) each.
- The double loop plus the `remove()` calls makes this effectively O(n²) at
  54k×tens-of-thousands scale → 84.8 min is plausible.

**In matches-only mode:** this entire block is computing
`deleted_funcs / added_funcs` — categorizing unmatched symbols for the report.
`function_matches` (the content of matches.json) was already built at line 1687–1707
(the `'Generating matches json'` log line at 19:01:54). The dedup loop is
**pure report infrastructure** and has zero effect on `pdiff['matches']`.

### 2b. The 22-min esym lookup

`ghidra_diff_engine.py:1788–1799` (still within `diff_bins()`):

```python
self.logger.info(f'Starting esym lookups for {len(esym_lookups)} symbols using {self.max_workers} threads')

with concurrent.futures.ThreadPoolExecutor(...) as executor:
    futures = (executor.submit(self.enhance_sym, sym, ..., (sym in funcs_need_decomp), ...)
               for thread_id, sym in enumerate(esym_lookups))
```

`esym_lookups` contains:
- `unmatched` (all ~54k unmatched functions) → `get_decomp_info=True` → decompiles each
- `funcs_need_decomp` from matched pairs where `syms_need_diff()` → True

This decompiles every unmatched function (via `enhance_sym` with
`get_decomp_info=True`, which calls `decompile_func`). In matches-only mode
we do not care about the decompiled text of unmatched functions — they appear
in `pdiff['functions']['added'/'deleted']` for the report, not in matches.json.

**In matches-only mode:** none of these 145k esym lookups are needed.

### 2c. The full `diff_bins()` return + downstream calls

After `diff_bins()` returns `pdiff` (line 2001):

```python
# __main__.py:106-118
pdiff = d.diff_bins(diff[0], diff[1])
pdiff_json = json.dumps(pdiff)          # serializes 209MB dict
d.validate_diff_json(pdiff_json)        # json.loads(result) — parses the 209MB
d.dump_pdiff_to_path(...)               # writes .md, .json, .matches.json
```

`json.dumps(pdiff)` + `validate_diff_json` on the full 209MB pdiff is itself
~seconds but is still dominated by the diff_bins cost above.
`dump_pdiff_to_path` takes 0.2s total (all writes).

---

## 3. Earliest point matches.json content is ready

**Line 1687** of `ghidra_diff_engine.py`, after the `find_matches()` call returns:

```python
self.logger.info('Generating matches json...')  # line 1687
address_matches = {}
name_matches = {}
function_matches = []
for sym1, sym2, m_types in matched:
    ...
    function_matches.append(build_function_match_entry(...))

pdiff['matches'] = {}
pdiff['matches']['address_matches'] = address_matches
pdiff['matches']['name_matches'] = name_matches
pdiff['matches']['function_matches'] = function_matches   # line 1707
```

By `19:01:54,698` (`'Deduping symbols and functions...'`), `pdiff['matches']`
is fully populated. Everything after that is report infrastructure.

A `--matches-only` run could exit `diff_bins()` immediately after line 1707,
write matches.json, and return. That saves ~107 minutes.

---

## 4. Implementation design: `--matches-only`

### 4a. Parser addition

**File:** `ghidriff/ghidriff/ghidra_diff_engine.py`
**Function:** `add_ghidra_args_to_parser()` — the "Engine Options" group
**Insertion point:** after the `--skip-correlators` argument (approx. line 407)

```python
# After the --skip-correlators argument, add:
group.add_argument('--matches-only', action='store_true', default=False,
                   help='Skip all post-match processing (decompilation, per-function diff, '
                        'markdown/JSON report generation) and write ONLY matches.json. '
                        'Saves ~107 min on an 8.5k-match run. The full JSON and .md are NOT '
                        'written. Use when you only need function_matches (scores, match types, '
                        'p1/p2 addresses and names). --force-diff still works: the cached project '
                        'is reused and matching re-runs; only the report stage is skipped.')
```

**Constructor:** Add the corresponding `self.matches_only` field (approx. line 118,
after `self.decomp_correlate_enabled`):

```python
self.matches_only = getattr(args, 'matches_only', False)
```

(Using `getattr` with a default means sibling engines that pass `args=None`
don't crash.)

### 4b. Early-exit inside `diff_bins()`

**File:** `ghidriff/ghidriff/ghidra_diff_engine.py`
**Function:** `diff_bins()`
**Insertion point:** immediately after line 1707
(`pdiff['matches']['function_matches'] = function_matches`), before the
`'Deduping symbols and functions...'` log line.

```python
        pdiff['matches']['function_matches'] = function_matches   # existing line 1707

        # --matches-only: skip all post-match processing (dedup, esym, report).
        # pdiff['matches'] is complete at this point; everything below builds the
        # full diff report (deleted_funcs, modified_funcs, stats, metadata) which
        # is only needed for the .md and full .json outputs.
        if self.matches_only:
            self.logger.info('--matches-only: skipping post-match diff/report stage')
            self.shutdown_decompilers(p1, p2)
            self.esym_memo = {}
            self.project.close(p1)
            self.project.close(p2)
            return pdiff        # pdiff has only 'program_options' + 'matches'
```

### 4c. __main__.py: skip validate + dump

**File:** `ghidriff/ghidriff/__main__.py`
**Function:** `main()`
**Insertion point:** wrap the post-diff block (lines 107–118)

```python
    for diff in diffs:
        pdiff = d.diff_bins(diff[0], diff[1])

        if not getattr(args, 'matches_only', False):
            # full report path (unchanged)
            pdiff_json = json.dumps(pdiff)
            d.validate_diff_json(pdiff_json)
            d.dump_pdiff_to_path(diff_name,
                                 pdiff,
                                 output_path,
                                 side_by_side=args.side_by_side,
                                 max_section_funcs=args.max_section_funcs,
                                 md_title=args.md_title)
        else:
            # matches-only path: write just matches.json
            diff_name = f"{Path(diff[0]).name}-{Path(diff[1]).name}.ghidriff"
            d.dump_pdiff_to_path(diff_name,
                                 pdiff,
                                 output_path,
                                 write_diff=False,
                                 write_json=False,
                                 write_matches=True)
```

`dump_pdiff_to_path` already supports `write_diff=False, write_json=False,
write_matches=True` (lines 2128–2130, 2141–2142, 2176–2186). No changes needed
there. The `minimise_pdiff()` call inside it also handles a partial pdiff
(it only iterates `pdiff['functions']` which won't exist in matches-only mode;
it needs a guard — see §4d).

### 4d. Guard minimise_pdiff for absent keys

**File:** `ghidriff/ghidriff/ghidra_diff_engine.py`
**Function:** `minimise_pdiff()` (line 2087)

The function iterates `pdiff['functions']['added'/'deleted'/'modified']`
unconditionally. In matches-only mode `pdiff['functions']` does not exist.

```python
    def minimise_pdiff(self, pdiff: dict):
        # In --matches-only mode 'functions' is absent — nothing to minimise.
        if 'functions' not in pdiff:
            return pdiff

        for func_type in ['added', 'deleted', 'modified']:
            ...   # existing code unchanged
```

This guard is a 2-line addition (the `if` check + `return pdiff`) and is safe
for the normal path.

### 4e. Runner: pass the flag

**File:** `tools/ghidra/run_ghidriff_xenon.sh`

Add to the invocation block:

```bash
# After the existing flags (--force-diff, --no-decomp-correlate, etc.)
MATCHES_ONLY_FLAG=${RB3_XENON_MATCHES_ONLY:+--matches-only}
```

And append `$MATCHES_ONLY_FLAG` to the `python -m ghidriff` call. Default: not
set (full report). `RB3_XENON_MATCHES_ONLY=1` → matches-only.

---

## 5. What disappears with --matches-only

Outputs that are NOT written:
- `json/<name>.ghidriff.json` — the 184–209MB full pdiff (contains decompiled
  text, per-function instruction/mnemonic lists, diff_type, stats)
- `<name>.ghidriff.md` — the 4.5–5MB markdown report

Outputs that ARE still written:
- `json/<name>.ghidriff.matches.json` — the 1.3–4.9MB function_matches list
  (p1_addr, p2_addr, match_types, p1_name, p2_name, scores) — the ONLY file
  our pipeline reads

Fields absent from pdiff in matches-only mode (not populated, so not wasted):
- `pdiff['functions']` (deleted_funcs, added_funcs, modified_funcs)
- `pdiff['symbols']`, `pdiff['strings']`, `pdiff['stats']`
- `pdiff['old_meta']`, `pdiff['new_meta']`
- `pdiff['md_credits']`, `pdiff['html_credits']`

`pdiff['program_options']` IS populated (it's read before find_matches). That
is harmless — it's tiny and needed for the project open.

---

## 6. State that the SKIPPED stage mutates — dependency check

The dedup/esym/modified-funcs block inside `diff_bins()` only reads from:
- `unmatched` — built by find_matches, unmodified by matching
- `matched` — built by find_matches, unmodified by matching
- `unmatched_nf_syms` — built by diff_nf_symbols, separate from matching
- The open programs p1, p2 — read-only (RO open at line 1660)

It writes to:
- `self.esym_memo` — populated and then **cleared** at line 1991
  (`self.esym_memo = {}`) before returning. In matches-only we clear it in
  the early-exit. No persistent state.
- `pdiff` dict — populated with keys our matches-only caller ignores.

**Conclusion:** Nothing in the skipped stage mutates state that the NEXT run's
matching depends on. The cached Ghidra project (programs, analysis, BSim
database) is untouched. `--force-diff` re-runs matching and populates
`pdiff['matches']` fresh — the early-exit is safe on repeated runs.

---

## 7. --force-diff interaction

`--force-diff` is passed at line 58 of `__main__.py` to `GhidraDiffEngine.__init__`
and surfaced as `self.force_diff`. It bypasses the language-mismatch and
symbol-count preflight (`check_diff_preconditions`). The project cache (analyzed
gzfs) is always reused; `--force-analysis` would re-analyze. Neither touches
the post-match stage. Both flags continue to work identically with
`--matches-only`.

---

## 8. validate_diff_json interaction

`validate_diff_json` (line 2072) does `json.loads(results)` — a pure parse of
the serialized pdiff. In matches-only mode we skip both `json.dumps(pdiff)` and
`validate_diff_json`. This is safe: the matches.json file is written by
`json.dump(pdiff['matches'], f, indent=2)` which is independently correct. The
validation gate exists to catch encoding bugs in the full pdiff; for the
matches-only path, if the write succeeds, the file is valid JSON by construction.

---

## 9. Expected time savings

With the early-exit:

| Phase | Current | With --matches-only |
|---|---|---|
| Matching | 8.4 min | 8.4 min (unchanged) |
| Dedup O(n×m) | 84.8 min | 0 min (skipped) |
| esym lookups | 22.0 min | 0 min (skipped) |
| Write matches.json | ~0.15s | ~0.15s (unchanged) |
| **Total** | **115.4 min** | **~9 min** |

The two-pass experiment (Round 2's main run) benefits from this: the seeded run
uses only matches.json for the vetted-identity export and eval pipeline. The
~107 min saved per run makes rapid iteration (e.g., sweeping seed thresholds)
practical.

---

## 10. Implementation checklist for a mechanical implementer

In `ghidriff/ghidriff/ghidra_diff_engine.py` (branch `rb3-improvements`):

1. **`add_ghidra_args_to_parser()`** — "Engine Options" arggroup (after
   `--skip-correlators`, ~line 407): add `--matches-only` `store_true` flag.

2. **`GhidraDiffEngine.__init__()`** — after `self.decomp_correlate_enabled`
   (~line 234): add `self.matches_only = getattr(args, 'matches_only', False)`.

3. **`diff_bins()`** — after line 1707
   (`pdiff['matches']['function_matches'] = function_matches`): insert the
   early-exit block (shutdown decompilers, clear esym_memo, close p1/p2, return
   pdiff).

4. **`minimise_pdiff()`** — first line of the method body (~line 2092): add
   `if 'functions' not in pdiff: return pdiff`.

In `ghidriff/ghidriff/__main__.py`:

5. **`main()`** — the `for diff in diffs:` loop: wrap the
   `validate_diff_json` + `dump_pdiff_to_path` calls in
   `if not getattr(args, 'matches_only', False):` and add an `else:` branch
   that calls `dump_pdiff_to_path` with `write_diff=False, write_json=False,
   write_matches=True`.

In `tools/ghidra/run_ghidriff_xenon.sh`:

6. Add `MATCHES_ONLY_FLAG=${RB3_XENON_MATCHES_ONLY:+--matches-only}` and
   append to the invocation. Default (unset) preserves the full-report path.

No other files need changes. The test suite (`tests/test_score_export.py`,
`tests/test_diff.py`, etc.) should pass unchanged because all code paths are
additive: the flag is off by default and the normal path is unmodified.

---

## For the next agent

### What was measured
- Run 3 log: `build/SZBE69_B8/ghidra/ghidriff-xenon/ghidriff.log` (contains 4
  runs; the final run starts at line 3607 / `2026-06-10 18:53:30`).
- The 107 min post-match splits as: 84.8 min dedup O(n×m) + 22 min esym + 0.2
  min writes. The dedup dominates.
- matches.json content (`pdiff['matches']`) is complete at log line
  `'Deduping symbols and functions...'` (line 3608, 19:01:54). Everything
  after that is report-only.

### Implementation precision
- All insertion points are exact (file, function, line). The design requires
  ~30 lines of new code total across 2 files + 1 shell script.
- The `minimise_pdiff` guard is required: without it, `dump_pdiff_to_path`
  calls `minimise_pdiff` which KeyErrors on `pdiff['functions']`.
- No existing tests need to change; no existing behavior changes when the flag
  is absent.

### Next step
Implement the 6 items in §10. Then run:
```bash
# Test: full path still works (regression)
cd /home/free/code/milohax/rb3
RB3_XENON_BSIM=0 ./tools/ghidra/run_ghidriff_xenon.sh   # short run without bsim

# Test: matches-only path produces only matches.json
RB3_XENON_BSIM=0 RB3_XENON_MATCHES_ONLY=1 ./tools/ghidra/run_ghidriff_xenon.sh
# Verify: json/*.matches.json exists, json/*.json does NOT, *.md does NOT
```

The two-pass experiment (agent T1 this round) should request
`RB3_XENON_MATCHES_ONLY=1` for its matching run since it only needs
matches.json.
