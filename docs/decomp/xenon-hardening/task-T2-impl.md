# T2 impl — per-match score export + VT STL/template gate

**Status:** DONE (offline-verified; no full ghidriff run, no service touched).
**Date:** 2026-06-10. **Author:** T2 implementer (opus).
**Repo:** `/home/free/code/milohax/ghidriff`, branch `rb3-improvements`.
**Commit:** `ghidriff:31a6f6c862ee1a791fd93a96dc58e0e6a342f832`

Read first (context this builds on): `PLAN.md` (§3 schema contract, §4 T2 rationale),
`scout-code-autopsy.md` (VT score-discard at vt_ref.py:146/171), `scout-failure-forensics.md`
(§1D STL examples, §2 gate measurements). This doc is the authoritative description of the
EMITTED SCHEMA and the offline verification.

---

## 1. What changed and why

Two changes, both from the cross-cutting lesson "tune offline, not by 2h re-run" and the
forensics STL-exclusion measurement.

### PART A — per-match score export (highest leverage)
The scored stages computed `(product, similarity, confidence)` (VT) and the difflib mnemonic
`ratio` (Implied) and **threw them away** (`matches[...][name] += 1`, a count). So
`--vt-ref-min-score` could not be tuned without a 2h re-run (autopsy "CRITICAL ARTIFACT GAP",
forensics §1E). Now a **side-channel dict** carries the scores to `matches.json` WITHOUT
changing the `{type: count}` value shape of the `matches` dict that bsim seed-membership, the
`Counter` logs, `decomp_correlate`, and the matched-list builder all read.

Design: `pair_scores[(str(src_addr), str(dst_addr))][match_type] = {...}`, owned by
`find_matches`, populated by each scored stage, returned as an OPTIONAL 4th `find_matches`
element, joined into the `function_matches` writer by **address string**.

| file:line (post-commit) | change |
|---|---|
| `ghidriff/vt_ref.py:144-146,243-252` | VT accept records `{similarity,confidence,product}` (STORED units) under `'VTCombinedReference'`; accept log promoted DEBUG→INFO (a log alone now supports a sweep). |
| `ghidriff/implied_matches.py:224-263` | Each gated Implied accept records `{ratio}` (the difflib mnemonic ratio at the `implied_min_ratio=0.9` gate) under `'Implied Match'`. Only the gated path records (ungated `implied_min_ratio==0.0` computes no ratio → no score, by design). |
| `ghidriff/bsim.py:126-140` | Each BSIM accept OPTIONALLY records `{similarity,confidence}` under `'BSIM'` (wrapped in try/except so score export can never break a BSIM accept). |
| `ghidriff/version_tracking_diff.py:103-110,197-209,239-243,256-269,355-362` | Owns `pair_scores = {}`; threads it to `correlate_bsim`/`correlate_vt_refs`/`correlate_implied_matches`; returns `[unmatched, matched, skip_types, pair_scores]`. Matched entries stay 3-element `[sym1, sym2, m_types]`. |
| `ghidriff/ghidra_diff_engine.py:32-56` (helper), `:1659-1700` (writer) | `build_function_match_entry(...)` emits the OPTIONAL `'scores'` field; writer unpacks `find_matches` robustly (`len(find_result) > 3 → pair_scores`, else `{}` for sibling engines). |

### PART B — VT STL/template exclusion gate
The VT accept loop now rejects a candidate when the **p1 (Wii) symbol OWNER** is an
STL/template internal. Forensics §2 measured this `0.324 → 0.393` on the judged subset at
~5% pool cost. My owner-anchored pattern set is tighter (2.6% pool, 19/722) and avoids
excluding real functions.

- `ghidriff/vt_ref.py:39-73` — `DEFAULT_VT_EXCLUDE_PATTERNS` (module constant).
- `ghidriff/vt_ref.py:130-175` (`_accept_vt_candidates`) — the gate; rejects counted as
  `stl/template excluded: N` in the existing summary log line.
- `ghidriff/ghidra_diff_engine.py` (init `:84-86` param, `:202-208` parse, arg group
  `--vt-ref-exclude-patterns`) + `ghidriff/__main__.py:80` (constructor wiring).

**Why owner-anchored, not substring.** Many real game functions merely TAKE an stl container
*parameter* (e.g. `CollideList__13BandCharacterF...stlpmtx_std88list<...>`,
`DrawWidgets__9UIListDir...vector<...>`, `VocalTrack::PollLyricAnimations...deque<...>`). A
naive `stlpmtx_std`-substring filter would wrongly drop these. The patterns anchor on the
FUNCTION-OWNER position (immediately after the `__<len>` member separator, or the template
instantiation itself for free helpers) so container-parameter functions survive. Measured on
the real 722-match VT pool: **19 excluded** (all true internals), **17 container-param real
functions kept**. Covers every STL example named judged-wrong in forensics §1D
(`push_back__28ObjVector<12CamShotCrowd,Us>`, `_M_fill_insert_aux__...11MidiChannel...`,
`__as__35ObjVector<...EventTrigger4Anim...>`, `__as__40ObjVector<...EventTrigger9ProxyCall...>`,
`__rs<12CamShotCrowd,Us>__...`).

**NOT done (per PLAN):** no VT min-size floor (measured inverted, forensics §2: size≥128 →
0.182). `--vt-ref-min-score` semantics unchanged.

---

## 2. THE EMITTED SCHEMA (binds T4 — read this)

Each `function_matches` entry gains an OPTIONAL `scores` field. **Key = the match-type name
exactly as it appears in `match_types`.** Field is OMITTED entirely when no scored stage
produced this pair (exact hashers, SeedMatch) and absent in pre-T2 matches.json.

```json
{
  "p1_addr": "0x80012340",
  "p2_addr": "0x82261abc",
  "match_types": ["VTCombinedReference"],
  "p1_name": "Foo__3Bar",
  "p2_name": "Function_82261abc",
  "scores": {
    "VTCombinedReference": {"similarity": 0.97, "confidence": 12.3, "product": 11.931},
    "Implied Match":       {"ratio": 0.94},
    "BSIM":                {"similarity": 0.91, "confidence": 1.4}
  }
}
```

Per-type score keys (all rounded to 6 dp):
- `VTCombinedReference`: `similarity` (0–1), `confidence` (×10 stored, typically ≥10),
  `product` (= similarity × confidence — **the exact value `--vt-ref-min-score` compares
  against**, STORED units). Sweep `--vt-ref-min-score` against `product`.
- `Implied Match`: `ratio` (difflib mnemonic SequenceMatcher ratio, the value the
  `implied_min_ratio` 0.9 gate compares against). Only present on the gated path.
- `BSIM`: `similarity`, `confidence` (the VTMatch scores BSim emits; only when BSIM runs).

### Where T4's sweep should READ it
- Per `function_matches` entry: `entry.get('scores', {}).get('VTCombinedReference', {}).get('product')`.
- **Treat the field + each per-type key as OPTIONAL** (`filter inactive when missing`) so old
  artifacts and unscored types replay (PLAN §3). A `--min-vt-score` sweep keeps an entry iff
  it has no VT score (non-VT match) OR its VT `product` ≥ the sweep value.
- A log-only sweep is also possible: each VT accept now logs at INFO
  `VTCombinedReference: accepted <src> -> <dst> sim=... conf=... product=...`
  (`vt_ref.py:251` in `_accept_vt_candidates`).

### Invariants T4 can rely on
- The `matches` dict value shape is unchanged: `{match_type: count}`. Scores never touch it.
- `pair_scores` keys are `(str(src_addr), str(dst_addr))` — same string form as
  `p1_addr`/`p2_addr` in the entry (so the address-string join is exact).
- Matched-list entries stay 3-element; only the `find_matches` return grew a 4th element.

---

## 3. New / changed public surface

- `ghidriff/vt_ref.py`
  - `DEFAULT_VT_EXCLUDE_PATTERNS: list[str]` — the owner-anchored STL/template regexes.
  - `_compile_exclude_patterns(patterns) -> list[re.Pattern]` — `None` → defaults; `[]` →
    no exclusion.
  - `_accept_vt_candidates(candidates, matches, p1_matches, p2_matches, min_score,
    min_func_len, compiled_excludes, p1_name_of, pair_scores, logger, name) -> dict` — the
    pure, JVM-free greedy accept loop (returns counts dict). The real `correlate_vt_refs`
    builds a `p1_name_of` closure over the Ghidra FunctionManager and calls this.
  - `correlate_vt_refs(..., pair_scores=None, exclude_patterns=None)` — two new optional kwargs.
- `ghidriff/implied_matches.py`: `correlate_implied_matches(..., pair_scores=None)`.
- `ghidriff/bsim.py`: `correlate_bsim(..., pair_scores=None)`.
- `ghidriff/ghidra_diff_engine.py`: `build_function_match_entry(p1_addr, p2_addr, m_types,
  p1_name, p2_name, pair_scores=None) -> dict` (module-level, JVM-free).
- CLI: `--vt-ref-exclude-patterns` (comma-separated regex; omitted → defaults; `''` → disable).

---

## 4. Offline verification — protocol + FULL output

**Environment.** The ghidriff venv (`build/SZBE69_B8/ghidra/ghidriff-venv`, py3.10) has the
ghidriff deps (mdutils etc.) but no pytest. System `python3` (the dc3 venv, also py3.10) has
pytest 9.0.2 but no mdutils. Both are py3.10 (no ABI skew). So tests run with:

```bash
cd /home/free/code/milohax/ghidriff
GVENV=/home/free/code/milohax/rb3/build/SZBE69_B8/ghidra/ghidriff-venv/lib/python3.10/site-packages
PYTHONPATH="$GVENV" python3 -m pytest tests/test_score_export.py -v \
    -o required_plugins= -o addopts= -p no:cacheprovider
```

(`-o required_plugins=` clears the repo's `pytest-datadir` requirement, which these JVM-free
tests do not use; `-o addopts=` drops the `-p no:faulthandler` default that isn't needed.)
The modules import WITHOUT a JVM because every Ghidra import is inside a stage-function body,
never at module top — verified directly.

**Result: 18/18 passed in 0.05s.** Tests:

```
test_scores_survive_to_side_channel            PASSED  (1) score survival, matches shape intact
test_pair_scores_none_does_not_crash           PASSED  (1) export-off path
test_stl_named_candidates_rejected_and_counted PASSED  (1) STL reject + count; real fn kept
test_exclusion_disabled_when_no_patterns       PASSED  (1) empty patterns -> accepts STL
test_min_score_gate_unchanged                  PASSED  (1) below_score regression
test_nan_score_rejected                        PASSED  (1) NaN product rejected (inverted cmp)
test_min_func_len_gate_unchanged               PASSED  (1) too_short regression (src and dst)
test_already_taken_gate_unchanged              PASSED  (1) already_matched regression
test_greedy_one_to_one_best_product_first      PASSED  (1) higher-product wins shared dst
test_writer_emits_scores_when_present          PASSED  (2) exact PLAN §3 entry shape
test_writer_omits_scores_when_absent           PASSED  (2) field OMITTED (not null) when absent
test_writer_omits_scores_when_pair_scores_none PASSED  (2) old-artifact / sibling-engine path
test_writer_implied_ratio_schema               PASSED  (2) Implied {'ratio':...} under its key
test_writer_multi_type_scores_keyed_by_type    PASSED  (2) two scored types both kept
test_replay_stl_exclusion_over_real_vt_pool    PASSED  (3) 19 excluded / 5 named / real kept
test_replay_existing_matches_json_has_no_scores PASSED (3) pre-T2 artifact schema-compatible
test_default_patterns_compile                  PASSED  (4) regex list compiles
test_compile_none_uses_defaults_empty_disables PASSED  (4) None->defaults, []->off
```

(Numbers in parens map to the four required verification items.)

### (1) accept-loop regression — counts vs current behavior
`_accept_vt_candidates` is the SAME code path the real stage runs (factored out, not
reimplemented). Tests assert: scores land in the side-channel and `matches[(src,dst)]` stays
`{type: count}`; STL-named p1 rejected & counted under `stl_excluded` while a real
container-param fn (`CollideList__13BandCharacter...`) is accepted; `below_score`/`too_short`
(src-short AND dst-short)/`already_matched`/NaN/greedy-best-product all unchanged.

### (2) writer test — schema + omission
`build_function_match_entry` feeds a synthetic matched pair + pair_scores through the exact
emission path. Asserts the entry equals the PLAN §3 shape when a score exists, and that
`'scores'` is OMITTED (not `null`/`{}`) when the pair has no score, when `pair_scores` is `{}`,
and when it is `None` (old artifacts / `simple_diff`/`structural_graph_diff` which return only
3 elements).

### (3) replay over the REAL matches.json (read-only artifact, no run)
`build/SZBE69_B8/ghidra/ghidriff-xenon/json/bank8_target.elf-42264e.gzf-...matches.json`.
Of its **722** VT entries, the DEFAULT patterns exclude **exactly 19** p1_names — all true
STL/template internals — INCLUDING the 5 forensics §1D named examples. The 17 STL-ish-but-real
container-param functions (e.g. `CollideList`, `DrawWidgets`, `PollLyricAnimations`,
`Mats__11TrackWidget`, and the non-template `__rs__9BinStream`/`__ls__FR9BinStream` operators)
are KEPT. A second replay confirms the existing artifact has NO `scores` field anywhere
(old-artifact compatibility).

### (4) audit-by-grep — no matches-dict reader/writer breaks
`grep` over `ghidriff/*.py` for every reader/writer of the `matches` dict value:
- writers (`matches[...][name] += 1`): `bsim.py:135-136`, `decomp_correlate.py:95-96,266-267`,
  `vt_ref.py:142-143`, `version_tracking_diff.py:137-138,174-175,213-214,335-336`,
  `implied_matches.py:256-257` — all still `{type: count}`.
- shape-dependent readers: `bsim.py:90-92` (`any(m_type in seed_match_types for m_type in
  m_types)` — iterates type keys, untouched), `version_tracking_diff.py:183,231`
  (`Counter([tuple(x) for x in matches.values()])` — untouched),
  `version_tracking_diff.py:340` (`for match_addrs, match_types in matches.items()` — reads
  `.keys()`, untouched), `ghidra_diff_engine.py:1524` (`skip_type in match_types`).
- matched-list consumers (must be 3-element): `ghidra_diff_engine.py:1691, 1775, 1822` — all
  unpack `[sym, sym2, m_types]`; my matched entries stay 3-element.
- `find_matches` return arity: `version_tracking_diff.find_matches` now returns 4 elements;
  `simple_diff`/`structural_graph_diff` return 3. The single caller
  (`ghidra_diff_engine.py:1665-1667`) handles both via `len(find_result) > 3`.

**Also:** pre-existing JVM-free `tests/test_fast_core.py` (10 tests, exercises
`add_ghidra_args_to_parser` + `parse_args`, which now include `--vt-ref-exclude-patterns`)
still passes (10/10). CLI parse + init-parse of the flag verified directly: omitted → `None`
→ defaults; `'foo,bar'` → `['foo','bar']`; `''` → `[]` → exclusion disabled.

---

## 5. Known caveats / deviations

1. **`__main__.py` is outside my assigned FILES set** but I added ONE line (`:80`,
   `vt_ref_exclude_patterns=args.vt_ref_exclude_patterns`) — without it the new CLI flag parses
   but never reaches the engine constructor (dead feature). The line touches a region no other
   concurrent task (T1 = `correlators.py`; T3 = Ghidra fork; T4/T5 = rb3 tools) edits, so the
   collision risk is nil. Documented here for transparency. If a reviewer insists on strict
   scope, this one line is the only thing to relocate.
2. **Owner-anchored exclusion = 19/722 (2.6%), not the forensics-estimated ~36 (~5%).** The
   forensics §2 number was a looser substring count over the JUDGED subset and bundled ~4
   ctor/dtor pairs with the 5 STL pairs. T2's scope (per the task) is STL/template internals
   ONLY (not ctor/dtor); the owner-anchored set is the precise STL subset and is strictly
   safer (no real function wrongly dropped). If a future calibration wants the ctor/dtor cut
   too, add patterns to `DEFAULT_VT_EXCLUDE_PATTERNS` or pass `--vt-ref-exclude-patterns`.
3. **The exclusion gate runs only on the `--vt-ref-correlators` stage** (the 722-match pool).
   It does not touch the string hashers — that is T1's `correlators.py` 1:1-uniqueness gate.
4. **Implied score is only recorded on the gated path** (`implied_min_ratio > 0.0`). The
   authoritative run uses `--implied-min-ratio 0.9`, so all real Implied matches will carry a
   `ratio`. If someone runs ungated (`0.0`), Implied accepts will have no score (no ratio is
   computed) — by design, not a bug.
5. **No full ghidriff run was performed** (hard constraint). The score export is verified by
   construction + unit tests + the real-artifact replay; the FIRST scored matches.json will be
   produced by the human-gated re-run (PLAN §6). The exact scores cannot be known until then.

---

## For the verifier

Re-check, in order:
1. **Schema:** `ghidriff/ghidra_diff_engine.py:32-56` (`build_function_match_entry`) emits
   `'scores'` only when present, keyed by match-type. Confirm it matches PLAN §3.
2. **Side-channel never pollutes `matches`:** `vt_ref.py:142-152`, `implied_matches.py:256-263`,
   `bsim.py:131-141` write the count to `matches` and the scores to `pair_scores` separately.
   Run the §4 grep audit to re-confirm no reader sees a changed value shape.
3. **STL patterns:** `vt_ref.py:39-73`. Re-run the replay test
   (`test_replay_stl_exclusion_over_real_vt_pool`) — it asserts 19 excluded + the 5 named +
   real-fn survival against the live matches.json. If the pool count assertion (`== 722`) ever
   fails, the artifact changed; update the baseline.
4. **`find_matches` 4th-element + sibling engines:** `version_tracking_diff.py:355-362` returns
   4; `simple_diff.py:286` / `structural_graph_diff.py:353` return 3; the writer
   (`ghidra_diff_engine.py:1665-1667`) handles both. Confirm no `ValueError: too many values to
   unpack` path exists.
5. **Run the suite:** the command in §4. Expect `18 passed`. Also `tests/test_fast_core.py`
   (10 passed) for the arg-wiring regression.
6. **Post-run (human-gated only):** after the next ghidriff run, T4 reads
   `entry['scores']['VTCombinedReference']['product']` and sweeps `--vt-ref-min-score`
   offline against the eval/oracle — no re-run needed. That closes the loop this task opened.

**Commit:** `ghidriff:31a6f6c862ee1a791fd93a96dc58e0e6a342f832` (branch `rb3-improvements`).
Files committed: `vt_ref.py`, `implied_matches.py`, `bsim.py`, `version_tracking_diff.py`,
`ghidra_diff_engine.py`, `__main__.py`, `tests/test_score_export.py`. NOT staged (concurrent
T1): `correlators.py`, `tests/test_string_hasher_gate.py`.
