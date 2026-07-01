# Task T1 — String hashers: global 1:1-uniqueness gate — IMPLEMENTATION

**Status:** DONE. Landed + offline-verified. No full ghidriff run; no service touched.
**Date:** 2026-06-10
**Author:** T1 implementer (opus)

**Commits**
- ghidriff (`rb3-improvements`): `5b9cc4ee70eec153f26b99951f831a67befab667`
  — `ghidriff/correlators.py` + `tests/test_string_hasher_gate.py`
- rb3 (`master`): forensics scripts/outputs (see end) — `<see StructuredOutput>`

---

## 1. What changed and why

The two string correlators measured **0.000** judged precision on the Wii↔Xenon
run (26 SRH + 2 StrUnique judged-wrong pairs). Forensics
(`scout-failure-forensics.md` §1A/§1C) proved the mechanism: every judged-wrong
pair keyed on a **single shared class-name string** (`nuniq==1`) that is
referenced by **2–9 functions per program**. Ghidra's `one_to_one` acceptance
launders the leftover non-unique survivor into a confident-looking 1:1 match.

The fix (forensics §2, calibration doc 2026-06-09): a **global 1:1-uniqueness
gate**. The same lever that took Implied 26.5%→90.4%.

### File: `ghidriff/ghidriff/correlators.py`

All changes are inside the hasher classes + new module-level pure helpers. (T2's
`version_tracking_diff.py` was NOT touched.)

1. **New pure gating logic** (Ghidra-free, unit-testable), inserted after
   `get_defined_data` (correlators.py:373–490):
   - `MIN_STRING_LEN = 5` module constant (correlators.py:373).
   - `NO_KEY = None` sentinel (correlators.py:379).
   - `_filter_min_len(strings, min_len)` (correlators.py:382) — drops strings whose
     **content** (inside the `ds "..."` wrapper Ghidra's `str(data)` emits) is
     shorter than `min_len`. Order/duplicates preserved.
   - `build_string_ref_counts(func_strings, min_len)` (correlators.py:406) —
     inverts `{func: [strings]}` → `{string: #distinct-referencing-funcs}`. The
     global rarity signal.
   - **`gate_string_keys(func_strings, dedup, min_len, ref_counts)`** (correlators.py:427)
     — THE load-bearing function. Per func: filter by len; `nuniq = len(distinct)`.
     - `nuniq == 0` → `NO_KEY` (no usable strings; historical uuid branch).
     - `nuniq == 1` → keep ONLY if that single string's global `ref_counts` is
       exactly 1 in this program; else `NO_KEY`.
     - `nuniq >= 2` → always keep.
     - Key = `tuple(sorted(set))` when `dedup` (StrUnique) else `tuple(sorted(multiset))` (SRH).
   - `get_string_ref_counts(program)` (correlators.py:484, `@lru_cache`) — builds
     the inverted map once per program from `get_defined_data(program)`. This is
     the "lazily build per-program maps via func.getProgram(), cached per program"
     requirement; lru_cache keys on the Java `Program` object.

2. **`StringsRefsHasher`** (correlators.py:493):
   - `hash()` now calls `gate_string_keys({entry: strings}, dedup=False, ref_counts=get_string_ref_counts(prog))`;
     `NO_KEY` → fresh `uuid4()` (never matches), exactly like the old no-strings path.
   - `ONE_TO_MANY = True` → **`False`** (the misleading attribute now matches the
     docstring "DO NOT RUN with one_to_many=TRUE"; the cascade always invoked it
     one_to_one anyway — version_tracking_diff.py:71/79).
   - `MIN_STRING_LEN` now actually read (was declared-but-dead).
   - New `DEDUP = False` flag (multiset keying).

3. **`StrUniqueFuncRefsHasher`** (correlators.py:549):
   - Same gate, `dedup=True` (set keying).
   - **`ref_count` REMOVED from the key** — symbol reference counts are not
     comparable across MSVC↔MWCC and the stripped XEX recovers a different count;
     it was noise, never a uniqueness signal (forensics §1C).
   - With ref_count gone + the gate, it is now a strict **set-keyed variant** of
     SRH (differs only when a func references the same string >1×). See §4 for the
     redundancy recommendation.

4. **Dead code removed**: the `strings_in_func` collection + its
   `DefinedDataIterator.definedStrings` walk in `get_defined_data` (was built,
   never returned). `get_defined_data` now returns only `func_str_map`.

### File: `ghidriff/tests/test_string_hasher_gate.py` (NEW)

19 pytest cases (`@pytest.mark.fast`). Loads `correlators.py` **by file path** with
stubbed `jpype`/`ghidra` modules so the pure helpers import with **no JVM** (the
package `__init__` pulls pyghidra, which we avoid). Three layers: unit fixtures,
forensics replay, aggregate guard.

---

## 2. Verification protocol + FULL numbers

### Layer 1 — synthetic unit tests (no Ghidra)
`python3 -m pytest tests/test_string_hasher_gate.py -v -o required_plugins=`
(run under any python with pytest; the ghidriff-venv lacks pytest, system pytest
9.0.2 was used). **19 passed in 0.04s.** Covers:
- `_filter_min_len`: drops short content inside `ds "..."`; boundary len==5 kept,
  len==4 dropped; unwrapped content measured raw; order/dupes preserved.
- `build_string_ref_counts`: distinct-func counts; short strings excluded; dup in
  one func counts once.
- `gate_string_keys` (the forensics sub-modes):
  - **sub-mode A**: shared single string referenced by 3 Wii funcs + 1 Xenon →
    all 3 Wii funcs `NO_KEY`; the lone Xenon func keys but no Wii key equals it →
    no cross-program match.
  - **sub-mode B**: globally-1:1-unique single string (`ui_trigger_complete`) →
    survives, identical key both sides.
  - `nuniq>=2` multiset survives regardless of rarity.
  - strings <5 chars excluded (collapse to `NO_KEY`); a short string does not
    inflate `nuniq`.
  - external `ref_counts` (whole-program) respected over the passed-in subset.
  - multiset (SRH) vs set (StrUnique) keying.

### Layer 2 — replay over the real run's data (forensics artifacts)
The replay uses the **gzf-derived global ref-counts** (`gate_refcounts_out.json`,
see Layer 3) as the authoritative rarity signal, and the per-func string multisets
from `strkeys_out.json`. Results (all assertions PASS):

| group | judged-wrong pairs | killed by gate | survived |
|---|---:|---:|---:|
| StringsRefsHasher | 23 | **23** | 0 |
| StrUniqueFuncRefsHasher | 2 | **2** | 0 |
| **sub-mode-B (must survive)** | 3 | 0 | **3** |

- `test_judged_wrong_srh_all_killed` — 0 leaked.
- `test_judged_wrong_strunique_all_killed` — 0 leaked (`ParticleSys` wii=4/xen=2,
  `InstrumentDifficultyDisplay` wii=4/xen=2 → both non-unique → killed).
- `test_submode_b_pairs_survive` — `ui_trigger_complete`,
  `component_scroll_start`, `component_scroll_select` all key-equal both sides.
- `test_all_judged_wrong_killed_count` — `(25, 25)` killed.

### Layer 3 — read-only pyghidra recompute (minutes-bounded, NO full run)
Script: `forensics/verify_gate_refcounts.py` (~2–3 min, `analyze=False`, no save).
Recomputes the per-program global `string→#funcs` via the **exact**
`get_defined_data` path, on the real gzfs under the FORK ghidra
(`GHIDRA_INSTALL_DIR=/home/free/code/milohax/ghidra/build/ghidra`). Output:
`forensics/gate_refcounts_out.json`.

- **Cross-check vs `string_uniqueness_out.json`**: zero mismatches on all
  overlapping strings (the prior forensics inverted map is reproduced exactly).
- **Gate verdict per key string** (SURVIVE iff wii==1 AND xenon==1):
  all 25 judged-wrong strings have wii count ≥2 → **KILL**; the 3 sub-mode-B
  strings are exactly (wii=1, xenon=1) → **SURVIVE**. Sample:
  `LayerDir wii=4 xenon=2`, `CamShot wii=3 xenon=1`, `user_login wii=9 xenon=1`,
  `ui_trigger_complete wii=1 xenon=1`.

### Survivor analysis over the WHOLE matched population
Script: `forensics/survivor_analysis.py` (~3–5 min, read-only). Builds the COMPLETE
whole-program inverted map on both gzfs and replays the gate over all 610 SRH + 45
StrUnique matched pairs. Output: `forensics/survivor_analysis_out.json`.

```
SRH:       total 610  survivors 283  killed 327
           (155 via unique-single-string  +  128 via multi-string nuniq>=2)
StrUnique: total  45  survivors  10  killed  35
           (4 via unique-single-string  +  6 via multi-string)
submode_b_survived: 3/3
judged_wrong_srh_survived: 0/23   judged_wrong_stu_survived: 0/2
```

The 128 SRH multi-string survivors == the forensics "~21% nuniq>=2" estimate
(128/610 = 21.0%). The gate keeps **155 additional** unique-single-string keys on
top (the sub-mode-B class), for 283/610 (46.4%) total survival — kills the 327
shared-single-string collisions while preserving the structurally-forced unique
matches. This is exactly the intended behaviour ("yours should be that [21%] plus
unique-single-string keeps").

> **Recall caveat (from forensics §0/§2):** there are ZERO judged-CORRECT SRH/
> StrUnique pairs on this run, so the gate's recall on *true* matches is estimated
> from the population key-shape, not measured. The survivor SET (283/610) is the
> candidate pool the next gated run will actually emit; its precision is measured
> only after that run. What IS measured here: 100% of the known-wrong are killed,
> and the only 3 pairs with positive string evidence survive.

---

## 3. Caveats / things the verifier should know

- **`str(data)` wrapper.** Ghidra renders a string datum as `ds "content"`.
  `_filter_min_len` measures the quoted content length; `build_string_ref_counts`
  keys on the **full `str(data)` value** (wrapper included) at run time — which is
  exactly what the old hashers used and what cross-program key equality needs, so
  Wii and Xenon `str(data)` for the same literal must render identically (they do
  for these cases; verified by the replay matching). The forensics replay strips
  the wrapper only to align with the content-keyed `gate_refcounts_out.json`; the
  in-engine hasher does NOT strip — it gates on `str(data)`-keyed ref-counts built
  from the same `get_defined_data`, so it is self-consistent.
- **Single-program gate; both sides via the cascade.** Each `hash()` only sees one
  program. The gate enforces "single string referenced by exactly one func in THIS
  program". The OTHER side is enforced by the cascade's `one_to_one=True` tuple
  flag. I did NOT precompute both-sides (the FunctionHasher API gives one program
  per call); documented as designed. Net effect proven equivalent by the replay
  (each side independently gated → a single-string pair matches only if 1:1 on
  both).
- **lru_cache lifetime.** `get_string_ref_counts` and `get_defined_data` are
  `@lru_cache(None)` keyed on the `Program` object — same lifetime/behaviour as the
  pre-existing `get_defined_data` cache. Two programs per run → 2 cached entries.

---

## 4. StrUnique redundancy recommendation (for T3, do NOT edit the runner here)

With `ref_count` removed and the gate applied, `StrUniqueFuncRefsHasher` is a
strict **set-keyed** variant of `StringsRefsHasher` (multiset). They differ only
for functions referencing the same string multiple times — rare for the
ClassName/Type accessor population this correlator hits. On this run StrUnique
contributed 45 matches (all judged-wrong, all now killed; 10 survive post-gate, 6
of which are also nuniq>=2 SRH survivors). **Recommendation for T3:** consider
adding `StrUniqueFuncRefsHasher` to the runner's `--skip-correlators` to avoid two
string correlators fighting over the same drained pool. I did NOT touch
`run_ghidriff_xenon.sh` (T3's file) — this is a documented recommendation only. If
kept, it is harmless (a strict subset of SRH's behaviour after the gate).

---

## For the verifier — exactly what to re-check

1. **Run the unit + replay tests** (no Ghidra, ~0.05s):
   ```bash
   cd /home/free/code/milohax/ghidriff
   python3 -m pytest tests/test_string_hasher_gate.py -v -o required_plugins=
   ```
   Expect **19 passed**. (The ghidriff-venv has no pytest; use system pytest or
   `/home/free/code/milohax/dc3-decomp/venv/bin/pytest`. `-o required_plugins=`
   clears the repo's `pytest-datadir` requirement, which this test doesn't use.)

2. **Confirm the gate logic in `gate_string_keys`** (correlators.py:~419) matches
   the spec: `nuniq>=2` OR (`nuniq==1` AND `ref_counts[only]==1`); else `NO_KEY`.

3. **Spot-check the gzf recompute is honest** (optional, ~3 min, read-only):
   ```bash
   cd /home/free/code/milohax/rb3
   GHIDRA_INSTALL_DIR=/home/free/code/milohax/ghidra/build/ghidra \
   JAVA_HOME=/usr/lib/jvm/java-17-openjdk \
     build/SZBE69_B8/ghidra/ghidriff-venv/bin/python \
     docs/decomp/xenon-hardening/forensics/verify_gate_refcounts.py
   ```
   Expect: zero mismatches vs `string_uniqueness_out.json`; all judged-wrong
   strings wii≥2; the 3 sub-mode-B strings (wii=1, xenon=1).

4. **Survivor counts** are in `forensics/survivor_analysis_out.json`
   (SRH 283/610, StrUnique 10/45; submode_b 3/3 survive; judged-wrong 0 survive).
   Re-derivable via `forensics/survivor_analysis.py` (needs
   `/tmp/claude/all_str_pairs.json`, regenerable from `matches.json`'s
   `function_matches` filtered by match_type — one-liner in the script header
   region; see commit message of the rb3 forensics commit).

5. **Module loads under the JVM** (catches decorator/import regressions):
   ```bash
   GHIDRA_INSTALL_DIR=.../ghidra/build/ghidra JAVA_HOME=... \
     build/SZBE69_B8/ghidra/ghidriff-venv/bin/python -c \
     "import pyghidra; pyghidra.start(); from ghidriff.version_tracking_diff import VersionTrackingDiff; print('OK')"
   ```

## For the next agent (T3 runner + the gated re-run)
- The gate is ON by default for both string correlators (no flag needed). After the
  next gated run, the SRH match count should drop from 610 → ~283 and its judged
  precision should jump off 0.000.
- Consider `--skip-correlators StrUniqueFuncRefsHasher` (§4) — documented, not wired.
- The gate does NOT need T2's score export; they are independent.
- New artifacts you can reuse: `forensics/gate_refcounts_out.json` (whole-program
  string ref-counts), `forensics/survivor_analysis_out.json` (per-pair survival).
