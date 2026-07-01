# Task T1 — String-hasher 1:1-uniqueness gate — ADVERSARIAL VERIFICATION

**Verifier:** Fable (adversarial)
**Date:** 2026-06-10
**Implementer claim doc:** `docs/decomp/xenon-hardening/task-T1-impl.md`
**Verdict: CONFIRMED** (see §Verdict; measurement caveats in §5 — none load-bearing)

---

## 1. Commits + scope (verified)

- `ghidriff` `5b9cc4ee70ee` exists on `rb3-improvements` (HEAD), touches ONLY
  `ghidriff/correlators.py` + new `tests/test_string_hasher_gate.py` (2 files) —
  within the assigned file scope; `version_tracking_diff.py` (T2's file) untouched.
  Working tree clean.
- `rb3` `10b98155b347` exists — **on branch `xenon-hardening-t1`, NOT master**
  (the impl doc's header line "rb3 (`master`)" is wrong; the implementer's
  StructuredOutput caveat got it right). 5 files: impl doc + 2 forensics scripts +
  2 output JSONs. **The orchestrator must merge/cherry-pick `xenon-hardening-t1`
  → master for the T1 docs to be visible on the default branch.** Note the rb3
  working tree is currently checked out on `xenon-hardening-t1` while other tasks
  (T2/T3/T5 docs) committed to master.

## 2. Code review of the diff (read in full)

All six claimed changes are real and correct in `ghidriff/correlators.py`:

1. **`gate_string_keys` (correlators.py:~427)** implements exactly the spec:
   `nuniq==0 → NO_KEY`; `nuniq==1 → keep iff ref_counts[only]==1 in THIS program`;
   `nuniq>=2 → keep`. Key = sorted multiset (SRH) / sorted set (StrUnique).
   `NO_KEY → (uuid.uuid4(),)` in the hasher — behaviorally identical to the
   historical no-strings branch.
2. **MIN_STRING_LEN=5 actually enforced** (`_filter_min_len`, content measured
   inside the `ds "..."` wrapper; applied consistently in both the per-func filter
   AND `build_string_ref_counts`, so the rarity map and the gate see the same
   universe — no asymmetry bug).
3. **`ref_count` removed** from StrUnique's key (was `hash((tuple(strings), ref_count))`,
   now `hash(key)`); StrUnique is now a strict set-keyed variant of SRH as claimed.
4. **`ONE_TO_MANY = True → False`** — verified this attribute is consumed NOWHERE
   in the codebase (grepped all of ghidriff/): documentation-only flip, zero
   behavioral risk. The cascade tuples at `version_tracking_diff.py:71-72` and
   `:79-80` are `(MATCH_TYPE, hasher(), True, False)` = `one_to_one=True,
   one_to_many=False` (confirmed against the tuple-shape comment in
   `find_matches`) — the changed `hash()` is reachable through the same entrypoint
   as before, at both cascade positions.
5. **Dead `strings_in_func` deleted**; `get_defined_data`'s return value unchanged
   (`func_str_map` only, same as before). Verified `get_defined_data` has **no
   consumers other than the two string hashers** — the removal can't affect anyone
   else.
6. **Caching**: `get_string_ref_counts` is `@lru_cache(None)` on the Program object
   — identical pattern/lifetime to the pre-existing `get_defined_data` cache.
   Editable install verified: the ghidriff-venv's `__editable__.ghidriff-1.0.0.pth`
   resolves to `/home/free/code/milohax/ghidriff/ghidriff/__init__.py`, and
   `import ghidriff` under the venv python succeeds (real jpype, deferred
   JImplements, no JVM) — the next run picks up this code.

Hunted for and did NOT find: filter/rarity-map asymmetry, key-space collisions
between uuid fallback and real keys, Java-object dict-key hazards (Address keys
were already used as dict keys pre-change), other `get_defined_data` consumers,
attribute consumers of `ONE_TO_MANY`/`DEDUP`.

## 3. The 26-vs-23 discrepancy (resolved — implementer is right, the brief was self-contradictory)

`eval_report.json` says SRH judged=26 wrong=26; the impl claims "all 23 SRH
judged-wrong killed" + 3 survivors. I re-derived the judged-wrong set **independently
from eval_report.json** replicating `eval_xenon_matches.py`'s attribution
(high_conf=True ∧ verdict=disagree ∧ match_type∈match_types, + holdout
recovered_wrong): exactly **26 SRH + 2 STU pairs**, and the 3 sub-mode-B pairs
(`Poll__9UITriggerFv`→0x827ffbf8, `StartScroll__6UIListFRC11UIListStateib`,
`SendScrollSelected__12ScrollSelect…`) **ARE among the 26 judged-WRONG**. The brief's
"all 26 killed AND the 3 sub-mode-B survive" is literally unsatisfiable (B ⊂ 26).
Forensics §1B/§2 is the canonical resolution: the 3 are **oracle errors** (BinDiff
pairs same-shape `return Symbol(...)` stubs arbitrarily, with no string evidence;
the gate table at forensics line ~164 explicitly says "keeps the 3 sub-mode-B").
PLAN.md §T1 carries the same contradictory phrasing; the implementer took the only
coherent reading and disclosed it. **Set-equality verified:** the test file's
hardcoded 23+2 kill list ∪ 3 survivor list == my independently-extracted 28 pairs,
exactly (no missing, no extra).

## 4. Verification evidence — re-run / independently reproduced

1. **Unit+replay tests re-run myself**: `python3 -m pytest
   tests/test_string_hasher_gate.py -v -o required_plugins=` → **19/19 passed**.
   Read the test file in full: it loads the REAL `correlators.py` by file path
   (jpype/ghidra stubbed) and exercises `gate_string_keys` itself — the tests do
   exercise the changed path, incl. both forensics sub-modes, min-len boundary
   (5 kept / 4 dropped), external-whole-program ref_counts precedence, and
   multiset-vs-set keying. The replay layer uses the real artifacts + the real
   gate function, asserting `(25,25)` killed and 3/3 sub-mode-B survival.
2. **Cross-artifact honesty check (offline)**: `gate_refcounts_out.json`
   (implementer's gzf recompute) vs `string_uniqueness_out.json` (the *scout's*
   earlier independent recompute) — **zero count mismatches** on all overlapping
   strings. Per-string gate verdicts confirmed: all 25 judged-wrong key strings
   have wii-count ≥2 → KILL (`LayerDir` 4/2, `CamShot` 3/1, `user_login` 9/1, …);
   the 3 sub-mode-B strings are exactly (wii=1, xenon=1) → SURVIVE.
3. **Independent gzf re-run (read-only, ~4 min, no service touched, no full run)**:
   I copied `forensics/survivor_analysis.py` to `/tmp/t1verify/` (output path
   redirected) and re-ran it under the fork Ghidra
   (`GHIDRA_INSTALL_DIR=/home/free/code/milohax/ghidra/build/ghidra`). Result is
   **byte-identical** (`a == b` on parsed JSON) to the committed
   `survivor_analysis_out.json`:
   `SRH 610 → 283 survivors (155 unique-single + 128 multi) / 327 killed;
   STU 45 → 10 (4+6) / 35 killed; submode_b 3/3 survive; judged-wrong 0/25 survive.`
   128/610 = 21.0% matches the forensics nuniq≥2 estimate, as claimed. This run
   also executed the real `gate_string_keys`/`build_string_ref_counts` imported
   from the edited module *under the JVM* against whole-program gzf data.
4. **Inputs verified, not trusted**: `/tmp/claude/all_str_pairs.json` (the
   survivor-analysis pair list) compared against `matches.json` — SRH and STU
   pair-sets are **exactly equal** (610/45). Also verified **zero** SRH/STU pairs
   carry multiple match_types, so kill counts map 1:1 to matches removed.
5. **Replica audit**: `survivor_analysis.py`'s `full_func_str_map` was read
   line-by-line against `get_defined_data` — same symbol-table walk, same
   `hasStringValue` test, same references→containing-function mapping, same
   `str(data)` values; differs only in keying by offset (json-friendliness).
   `verify_gate_refcounts.py` was not re-run (its output is subsumed by checks
   2–3, which reproduce/triangulate the same counts).

## 5. Residual caveats (measurement, not implementation — none refute)

1. **Recall on TRUE matches is unmeasured** (zero judged-correct SRH/STU pairs
   exist on this run) — properly disclosed by the implementer. The gate discards
   327/610 (53.6%) of SRH's former yield; every *judged* one of those was wrong,
   but the unjudged majority's true-match fraction is unknowable offline.
2. **The next run's eval will STILL score the 3 sub-mode-B survivors as
   'disagree'** — the BinDiff oracle is unchanged, and the eval's `low_trust_stub`
   escape hatch (default OFF) can't rescue them because the Wii functions are
   516–868 B (way over stub size). If the gated re-run's SRH judged set is just
   these 3, the report will read "SRH 0/3 = 0.000" again. **The orchestrator must
   not read that as gate failure** — it is the oracle artifact forensics §1B
   already diagnosed. Consider a human re-judgement / oracle exception for those 3
   xenon addrs (0x827ffbf8, 0x827d2588, 0x827f7cc0) before the re-run's eval.
3. **Same-ISA regression risk (out of mission scope, worth recording)**: the
   correlators are shared; on same-ISA runs (Bank5↔Bank8, where SRH measured
   91.4%) the gate will kill drained-pool single-string matches that were mostly
   RIGHT there. Anyone reusing ghidriff same-ISA inherits lower string-correlator
   recall. Not flagged in the impl doc.
4. **Cascade displacement**: the 327 no-longer-drained functions now flow to later
   stages incl. the `one_to_many=True` StructuralGraph/BulkBB hashers, which may
   mint new matches of unknown precision. Only the gated human re-run measures
   this; not a T1 defect.
5. The literal in-engine `hash()` bodies (JImplements wrapper) have been
   import-checked and their logic executed via the file-path-imported module +
   faithful replica, but never run with live Ghidra `Function` objects. The
   wrapper is ~6 lines of previously-exercised patterns; risk is minimal and only
   the gated re-run can close it.

## Verdict

**CONFIRMED.** Every load-bearing claim was independently re-verified: the commits
exist and stay in scope; the diff implements the specified gate exactly and is
reachable from both cascade positions; the 19 tests pass and genuinely exercise
the changed code; the judged-wrong set was re-derived from eval_report.json and
matches the test fixture exactly (the 26-vs-23 delta is the brief's own
contradiction, resolved per forensics §1B in the only coherent way); and the
whole-population survivor numbers (283/610, 10/45, 0/25 wrong survive, 3/3
sub-mode-B survive) were reproduced **byte-identically** by my own read-only gzf
re-run. The only unverifiable item — recall on true matches — is unmeasurable
offline by construction and was explicitly disclosed; it gates nothing until the
human re-run.

## For the next agent

- **Merge `xenon-hardening-t1` → master** (rb3) so the T1 doc/forensics land on the
  default branch; ghidriff side is already on `rb3-improvements` HEAD.
- Before the gated re-run's eval: decide how to score the 3 sub-mode-B pairs
  (§5.2) — otherwise SRH will mechanically re-report 0.000 on a tiny judged set.
- T3 should consider `--skip-correlators StrUniqueFuncRefsHasher` (impl doc §4);
  post-gate it is a near-duplicate of SRH (only 4 unique-single + 6 multi survivors,
  6 of which SRH also finds).
- Reusable: my independent rerun artifacts in `/tmp/t1verify/` (ephemeral);
  committed artifacts in `forensics/` are verified-trustworthy.
