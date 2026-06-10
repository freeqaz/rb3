# Scout: ghidriff correlator code autopsy (Wii↔Xenon cross-compiler)

Status: COMPLETE (read-only code analysis + offline artifact inspection). No runs started, no service touched.
Date: 2026-06-10.
Author handoff for: the agents tuning VT thresholds, gating StringsRefsHasher, and exporting scores.

## What to read first (and where the numbers come from)
- Code: `/home/free/code/milohax/ghidriff/ghidriff/` — `correlators.py`, `version_tracking_diff.py`, `vt_ref.py`, `implied_matches.py`, `bsim.py`, `ghidra_diff_engine.py`.
- Artifacts (rb3 repo): `build/SZBE69_B8/ghidra/ghidriff-xenon/` — `ghidriff.log` (3 runs; the **last** is authoritative), `json/*.matches.json`, `eval_report.json`.
- The authoritative run's exact command is in `ghidriff.log:1606`. It ran with: `--min-func-len 16 --implied-min-ratio 0.9 --skip-correlators BulkBasicBlockMnemonicHash,SigCallingCalledHasher,StructuralGraphExactHash --seed-matches seeds.json --vt-ref-correlators --vt-ref-min-score 9.5` and **BSim OFF** (`ghidriff.log:1123` "Skipping BSIM correlator. BSIM disabled with arg --no-bsim").

### Final match tally that produced matches.json (ghidriff.log:1605 Counter)
`SeedMatch 1186, VTCombinedReference 722, StringsRefsHasher 610, ExactInstructionsFunctionHasher 63, StrUniqueFuncRefsHasher 45, Implied Match 11, ExactMnemonicsFunctionHasher 4, SwitchSigHasher 3, SymbolsHash 1`. Total matched_funcs 13,965; unmatched 64,742 (`func_match_overall_percent 17.74%`).

NOTE on the log: it contains 3 stacked runs. Run 1 (`ghidriff.log:38-898`) had **bsim:True** and produced different counts (VTCombinedReference accepted 1190, StringsRefsHasher 486). Runs 2+3 (`:1032+`, `:1318+`) are `bsim:False` and identical (722 / 606). The matches.json + eval_report reflect a `bsim:False` run. **When comparing future runs, hold BSim state constant** — it shifts the unmatched pool every later stage drains from.

---

## The cascade that actually ran (version_tracking_diff.py:63-82, find_matches)
Order matters: each stage **subtracts its accepts from the unmatched pool** (`:214-215`), so earlier stages starve later ones. Pre-cascade, two non-hash stages run first:
1. **SeedMatch** (`:103-139`) — pre-accepts `--seed-matches` pairs by `getFunctionContaining`→entryPoint. 1186 in output (3 dropped, p2 func missing — `ghidriff.log:111-113`).
2. **SymbolsHash** (`:146-168`) — `MatchSymbol.matchSymbol`, one-to-one, min name len 3, plus a `getName(True)` equality guard (`:162`). Cross-stripped-XEX this yields **1** match (Xenon is stripped → almost no shared symbol names).

Then `func_correlators` (`:63-82`), each `MatchFunctions.matchFunctions(p1_unmatched, p2_unmatched, min_func_len, one_to_one, one_to_many, hasher)`:

| # | Stage | one_to_many | Xenon matches | Notes |
|---|---|---|---|---|
| 1 | ExactBytesFunctionHasher | F | 0 | different ISA → never matches |
| 2 | ExactInstructionsFunctionHasher | F | 63 | **gold, 0.935 precision** (Ghidra normalizes operands → survives reloc/addr diffs) |
| 3 | StructuralGraphExactHash | F | — | **skipped** via `--skip-correlators` |
| 4 | ExactMnemonicsFunctionHasher | F | 4 | mnemonic multiset equality; precise but tiny |
| 5 | BSIM | (n/a) | **OFF** | `--no-bsim`; would consume exact-stage seeds (bsim.py:83) |
| 6 | BulkInstructionHash | F | 0 | full-instruction-string multiset; cross-ISA → 0 |
| 7 | SigCallingCalledHasher | F | — | **skipped** |
| 8 | StringsRefsHasher | F | 606 | **0.000 precision — the leak (see below)** |
| 9 | StrUniqueFuncRefsHasher | F | 45 | **0.000 precision** |
| 10 | SwitchSigHasher | F | 3 | sig+switch-label multiset |
| 11 | StructuralGraphHash | **T** | 0 | fuzzy CFG 3-tuple + name+refs; one_to_many |
| 12 | BulkBasicBlockMnemonicHash | T | — | **skipped** |
| 13-16 | (Sig/StringsRefs/StrUnique/SwitchSig repeat) | F | StringsRefs +4 | second pass mops up newly-freed pool |

After the hash cascade: **VTCombinedReference** (vt_ref.py, gated by `--vt-ref-correlators`), then **Implied Match** (implied_matches.py), then **decomp_correlate** (decomp_correlate.py — see log, contributes 0 named types to the final Counter here).

---

## Per-correlator autopsy: key, uniqueness gate, cross-compiler failure mode

### ExactInstructionsFunctionHasher (Ghidra builtin) — 0.935, KEEP AS-IS
- **Key:** hash of the function's *normalized instruction stream* (Ghidra masks out operand bytes/relocs, keeps mnemonic+operand structure). one_to_one.
- **Uniqueness:** `MatchFunctions` with one_to_one already enforces 1:1 (a hash shared by >1 func on either side is dropped). This is the implicit gate that makes it precise.
- **Cross-compiler:** survives because PPC Gekko and PPC Xenon share the base PowerPC ISA and Ghidra's normalization erases addresses; only genuinely byte-identical-after-normalization bodies match. 63 matches, 29/31 judged correct. **No change needed.**

### StringsRefsHasher — 610 matches, **precision 0.000** (THE primary leak)
- **Key (correlators.py:380-398):** `hash(tuple(sorted(strings)))` where `strings = func_str_map.get(func.entryPoint)` — the **sorted MULTISET of every string the function references** (duplicates kept, no namespace, no length filter). If a function references no strings, key = a fresh `uuid4()` (forces a unique, never-colliding key — good).
- **String discovery (`get_defined_data`, :308-361):** two collections, but **only one is used**:
  - `strings_in_func` (:325, the `DefinedDataIterator.definedStrings` walk, :338-342) is built then **never returned** — dead code.
  - `func_str_map` (:344-359, the **returned** map) is built by walking *all symbols*, keeping non-FUNCTION symbols that `hasStringValue()`, and for each `sym.references` mapping the referencing function's `entryPoint`→`str(data)`.
- **`MIN_STRING_LEN = 5` is declared (:374) but NEVER read** — verified the only two source offsets are the class-body declarations themselves. **No length filtering happens.** Tiny shared strings ("", "%d", file separators) all count.
- **Why it collapses cross-compiler (root cause):**
  1. **Shared-assert-string collision.** RB3 emits `MILO_ASSERT`/`MILO_FAIL` with the *same source-file-path string* in dozens of functions per TU. The multiset key for "func that asserts with path X and nothing else distinctive" is identical across many functions → **not 1:1-unique**. The cascade's one_to_one only fires *after* the pool has been drained by exact stages; on the ~64k cross-compiler unmatched pool the exact stages drain almost nothing, so StringsRefsHasher is matching inside a huge, collision-dense pool. one_to_one then picks an essentially arbitrary single survivor per colliding key → 0/26 judged correct.
  2. **String-pooling asymmetry MSVC vs MWCC.** The symbolized Wii ELF (`band_r_wii` types ported onto bank8) and the stripped XEX define/merge string literals differently — different dedup, different which-function-owns-the-reference, different `hasStringValue` data typing. So even a string that *is* unique can map to a different function set on each side.
  3. **Refcount/reference-discovery asymmetry.** `func_str_map` depends on `sym.references` being analyzed; the stripped XEX's auto-analysis finds a different reference set than the symbolized ELF.
- The docstring at :369 literally says **"DO NOT RUN THIS with one_to_many = TRUE"** yet `ONE_TO_MANY = True` is set on the class (:376). It is *invoked* one_to_one in the cascade (the tuple flag at version_tracking_diff.py:71/79 is `True,False`), so the class attribute is misleading but the actual run was one_to_one. The damage is the **collision density**, not a one_to_many invocation.
- **Minimal fix (uniqueness gate):** precompute, per program, the multiset-key→[funcs] map for both p1 and p2; **only emit a match when the key maps to exactly one function in p1 AND one in p2** (a true 1:1-unique key). Additionally: (a) actually apply `MIN_STRING_LEN` (drop strings < 5 chars), (b) drop the universal assert-path strings from the key (or weight by global rarity — a string referenced by >N functions is noise), (c) require the surviving distinctive-string set to be non-trivial (e.g. ≥1 string of length ≥ some threshold, or ≥2 strings). This mirrors the calibration lesson (`docs/decomp/ghidriff-calibration-2026-06-09.md`): 1:1-uniqueness of the key was the single biggest precision lever for Implied (26.5%→90.4%).

### StrUniqueFuncRefsHasher — 45 matches, **precision 0.000**
- **Key (:421-442):** `hash((tuple(sorted(set(strings))), ref_count))` — same string discovery, but **de-duplicated to a set** plus the function's `getSymbol().getReferenceCount()`.
- **Cross-compiler failure:** identical string-pooling/discovery asymmetry as above, PLUS `ref_count` is *not comparable across compilers* — MSVC vs MWCC produce different call-site counts and the stripped XEX's analyzer finds a different reference count than the symbolized ELF. So adding `ref_count` to the key actively *hurts* here (it's neither stable nor a uniqueness signal). Same fix: gate on 1:1-uniqueness of the *distinctive-string set alone*, drop `ref_count` from the cross-compiler key (or only use it as a tiebreak, never a key component).

### SwitchSigHasher — 3 matches
- **Key (:492-510):** `hash((sig_without_name, tuple(sorted(switch_syms))))` where `switch_syms` = `switchD_*`/`caseD_*` labels in/referenced-by the function (`get_func_to_switch`, :449-475); uuid4 fallback if none.
- **Cross-compiler:** `sig` is `func.getSignature().toString()` minus the name — on a stripped XEX the signature is the *recovered* one (often `undefined4 FUN_x(void)`), so it carries almost no info and the switch-label names (`switchD_<addr>`) embed addresses → won't match across programs unless the jump-table structure is recovered near-identically. Low yield (3) but the 3 weren't flagged wrong. Not a priority; if tuned, gate on switch-label *count + case count* structural signature rather than label strings.

### VTCombinedReference (vt_ref.py) — 722 matches, **precision 0.324**, THE WORKHORSE
- **Key/scoring:** NOT a hash. Runs Ghidra's `CombinedFunctionAndDataReferenceProgramCorrelatorFactory`. Every already-accepted match (seeds + exact stages + BSim) is seeded into a temp `VTSessionDB` as an ACCEPTED association (vt_ref.py:93-124). The correlator builds, per remaining function, an LSH cosine vector over its call/data-reference edges to accepted matches (weighted by feature rarity), then pairs by vector similarity. Internal factory thresholds: similarity ≥ 0.5, confidence(raw) ≥ 1.0 (vt_ref.py:24-32 docstring).
- **External accept gate:** `product = similarity * confidence` (confidence already ×10 when stored, so ≥10 typically), accept greedily best-product-first, one-to-one (`p1_matches.contains`/`p2_matches.contains` reject already-claimed), reject `< min_func_len` bodies. `min_score=9.5` (stored units = conf×10 × sim; effectively sim≥~0.95 at conf≈1.0). See vt_ref.py:151-182.
- **"already matched/taken: 1942"** (ghidriff.log:1164) = candidates whose src OR dst entryPoint was already in `p1_matches`/`p2_matches` when the greedy loop reached them (claimed by an earlier exact/string stage or by a higher-scoring VT candidate in the same loop). `below min_score: 0` means **every emitted candidate cleared 9.5** — the internal factory thresholds already removed everything below, so `min_score=9.5` is currently a **no-op on the emitted set** (it only filters what the correlator itself didn't already drop). 128 dropped for `< min_func_len 16`. 722 accepted of 2792 candidates.
- **Cross-compiler failure (why 0.324 not 0.9):** the reference vectors are built from *accepted-match identities*, and the seed set is dominated by 1186 SeedMatch pairs whose **p2 (Xenon) names are stripped** (`Function_82260018`). When many functions share a similar small reference-fanout to the same few popular accepted callees (e.g. everyone calls the same allocator/logging/MILO_ASSERT helper), their LSH vectors are near-collinear → the top-5 refine picks a plausible-but-wrong neighbor. It's the same "popular shared callee = low-information feature" problem as the assert strings.
- **CRITICAL ARTIFACT GAP — scores are NOT exported.** `product/similarity/confidence` are computed at vt_ref.py:146 and used only locally for the accept decision. They are written into `matches[(src,dst)][name] += 1` as a **count of 1** (vt_ref.py:171-172) — the numeric score is discarded. Downstream, `matched` is built as `[sym, sym2, list(match_types.keys())]` (version_tracking_diff.py:341) and matches.json stores only `{p1_addr,p2_addr,match_types,p1_name,p2_name}` (ghidra_diff_engine.py:1657-1663). **I confirmed matches.json has NO score field (2645 entries, keys = exactly those 5).** ⇒ **You cannot sweep the VT min_score threshold offline from the existing artifacts.** Tuning 9.5 currently *requires a 2h re-run*. **This is the highest-leverage recommended action: export per-match similarity/confidence/product** so thresholds can be calibrated against the holdout/BinDiff oracle without re-running.

### Implied Match (implied_matches.py) — 11 matches, precision 0.75
- **Mechanism:** for each accepted match (f1,f2), for each *call* reference out of f1 (data refs ignored, :57), use `VTHashedFunctionAddressCorrelation` to map the call site f1→f2, read the parallel call target in f2, and propose (calleeA, calleeB) as an implied match if both are still unmatched.
- **0.9 ratio gate (`implied_min_ratio=0.9`, :222-240):** before accepting, compute `difflib.SequenceMatcher(mnemonics(f1_callee), mnemonics(f2_callee)).ratio()`; reject if `< 0.9`. This is the calibrated gate from the prior lesson. In the run it **gated out 1779** and accepted 30 (11 new). 0.9 mnemonic-similarity is a real cross-compiler-tolerant uniqueness/distinctiveness gate (mnemonic sequences survive reloc/operand diffs) — this is why Implied held 0.75 while StringsRefs collapsed. **Working as intended; the model to copy for the string hashers.**

### BSIM (bsim.py) — OFF in the authoritative run
- Consumes seeds: reads `matches` whose type is in `['SeedMatch','SymbolsHash','ExactBytes...','ExactInstructions...','StructuralGraphExactHasher','ExactMnemonics...']` (bsim.py:83-90), adds each as an ACCEPTED VTMatchInfo, runs `BSimProgramCorrelator` over the unmatched pool, writes accepts back as type `'BSIM'`. So **--seed-matches pairs DO seed BSim.** It was disabled here for the single-threaded-at-scale stall (per repo memory + `8f998915`). The top-32 candidate-cap patch (`scripts/ghidra/bsim-topk-cap.patch`) is the path to re-enabling it; that's a different scout's territory.

---

## Cross-cutting failure pattern (the one-paragraph summary)
Every precise stage (ExactInstructions 0.935, Implied 0.75) has a **distinctiveness/1:1-uniqueness gate baked in** — Ghidra's one_to_one on a normalized-instruction key, or the 0.9 mnemonic-ratio check. Every collapsed stage (StringsRefs 0.0, StrUnique 0.0, VTCombined 0.32) keys on **a low-information shared feature** (the same assert-path string, the same popular callee edge, an incomparable refcount) with **no uniqueness gate** and no cross-compiler-stability filter. The fix is uniform: gate each on 1:1-uniqueness of the key across *both* programs and strip the globally-popular (low-rarity) features from the key.

## For the next agent
- **HIGHEST LEVERAGE / DO FIRST:** export VT scores. In `vt_ref.py` carry `(similarity, confidence, product)` alongside the accept (store into the matches value, not just `+=1`), and in `ghidra_diff_engine.py:1657-1663` add the score fields to each `function_matches` entry. Then VT `min_score` can be swept offline against `eval_report.json`'s holdout/BinDiff oracle via a pure-python replay — no re-run. (Also consider exporting Implied's mnemonic ratio and a per-StringsRefs key-collision count for the same offline-tuning reason.) Stay within `vt_ref.py` + the matches.json writer block in `ghidra_diff_engine.py` (other scouts touch correlators.py/bsim.py).
- **StringsRefs/StrUnique fix (correlators.py):** add a per-program `key→[funcs]` precompute and only emit 1:1-unique keys; actually apply `MIN_STRING_LEN=5`; drop the dead `strings_in_func` collection (:325/338-342); add a global string-rarity filter (drop assert-path strings referenced by > N funcs); for StrUnique drop `ref_count` from the key. Offline-validate by recomputing keys on the *existing* gzfs read-only (allowed, finishes in minutes) and counting how many of the current 610/45 matches survive a 1:1-unique gate vs how many were judged wrong.
- **VT noise:** the 9.5 min_score is a no-op on the emitted set (`below min_score: 0`). The real knob is the **internal** similarity/confidence thresholds + the popular-callee feature weighting; but you can't tune any of it without exported scores first.
- **Hold BSim state + seed set constant** when comparing any future run to this baseline (the log shows BSim materially shifts every downstream stage's pool).
- Numbers cross-checked against: `eval_report.json` (precision_by_match_type), `ghidriff.log:1605` (final Counter), `json/*.matches.json` (2645 entries, no score field). Authoritative run = the `--no-bsim` one at `ghidriff.log:1606`.
