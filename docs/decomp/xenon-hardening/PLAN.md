# Xenon Hardening Plan — Wii↔Xenon ghidriff precision + recall (2026-06-10)

**Author:** planner (Fable), from four scout handoffs in this directory.
**Goal:** the NEXT gated Wii↔Xenon run (human-triggered, ~15-20 min with the fixes below)
produces a **high-precision (>0.85 on judged) identity tier** with meaningfully higher recall,
and every threshold becomes tunable OFFLINE afterward (no more 2h re-runs per knob).

**Read first:** the four scout docs in this directory:
`scout-code-autopsy.md`, `scout-failure-forensics.md`, `scout-bsim-deploy.md`, `scout-recall-levers.md`.

---

## 1. Strategy (why these five tasks)

The measured run is 0.440 overall judged precision. Decomposed:

| Component | Now | After this plan (projected) | How |
|---|---|---|---|
| StringsRefs/StrUnique (655 matches) | 0.000 | killed or ~0.8-0.9 on survivors | T1: 1:1-global-uniqueness gate (kills all 26+2 judged-wrong; keeps the 3 mis-judged sub-mode-B) |
| VTCombinedReference (722) | 0.324 measured / ~0.51 true | ~0.51 measured + offline-sweepable | T2: STL exclusion + score export; T4: eval rename/arity crediting |
| BSim (0 — disabled) | unmeasured | first measured number, quarantined by type | T3: deploy patches + `--no-decomp-correlate`, run BSim-ON |
| Eval/oracle artifacts | hides ~0.19 of VT precision | fixed | T4 |
| Seeds / game-code recall | DC3-only oracle, band3 absent | +41 direct band3 anchors; vetted-identity export | T4 (seeds) + T5 (vetting) |

The cross-cutting lesson (autopsy §"Cross-cutting failure pattern", calibration doc 2026-06-09):
**every precise stage has a 1:1-uniqueness/distinctiveness gate; every collapsed stage keys on a
popular shared feature without one.** T1 and T2 apply that lesson uniformly. T2's score export is
the single highest-leverage change because it converts every future threshold decision from a
2h re-run into a pure-python offline sweep against the existing oracle.

**How we get to >0.85 on judged:** not by making every correlator 0.85 — by making the OUTPUT
TIERED. The high-confidence tier = SeedMatch + ExactInstructions + gated string anchors +
Implied + VT-above-swept-score-threshold (threshold chosen offline post-run via T2's exported
scores against T4's fixed eval). BSim stays its own quarantined type until its first measured
precision exists. T5's vetting protocol is the template for the tier export.

---

## 2. The tasks (dependency graph)

```
T1 (correlators.py string-hasher gate)   ──┐
T2 (score export + VT gate, ghidriff)    ──┤── all land independently; the gated
T3 (BSim deploy + runner hardening)      ──┤── human re-run consumes all five
T4 (eval/oracle/seeds, rb3 tools)        ──┤
T5 (vetting script, rb3 tools)           ──┘
```

No hard sequencing. T2↔T4 share a **schema contract** (§3) instead of a dependency.
All files pairwise disjoint:
- T1: `ghidriff/ghidriff/correlators.py` + new ghidriff test file.
- T2: `ghidriff/ghidriff/{vt_ref.py, version_tracking_diff.py, ghidra_diff_engine.py, implied_matches.py, bsim.py}` + new test file.
- T3: Ghidra fork (branch `bsim-xenon-patches`) + `rb3/tools/ghidra/run_ghidriff_xenon.sh`.
- T4: `rb3/tools/ghidra/{eval_xenon_matches.py, test_eval_xenon_matches.py, build_xenon_seeds.py}` + regenerated `build/SZBE69_B8/ghidra/xenon-seeds/`.
- T5: new `rb3/tools/ghidra/vet_xenon_identities.py` + its output JSON.

---

## 3. Schema contract: exported scores (binds T2 and T4)

T2 makes ghidriff emit, per `function_matches` entry, an OPTIONAL field:

```json
"scores": {
  "VTCombinedReference": {"similarity": 0.97, "confidence": 1.23, "product": 11.93},
  "Implied Match":       {"ratio": 0.94}
}
```

- Key = match-type name exactly as it appears in `match_types`.
- Absent for types with no score (exact hashers, SeedMatch) and absent entirely in old
  matches.json files. **T4's consumers must treat the field as optional** (filter inactive when
  missing) so both tasks land in any order and old artifacts still replay.
- VT scores are the STORED units (confidence already ×10; product = sim × stored-conf, the same
  scale `--vt-ref-min-score 9.5` compares against).

---

## 4. Per-task rationale

### T1 — Rewrite the string hashers with a global 1:1-uniqueness gate (opus)
Forensics proved the exact mechanism (scout-failure-forensics §1A): all 26 judged-wrong SRH
pairs are single-shared-string keys (`nuniq==1`, key string referenced by 2-9 Wii funcs, median
3); earlier stages drain the true referencer and 1:1-survivor logic launders the leftover into a
confident wrong match. The proven gate (forensics §2): **keep a candidate only if `nuniq≥2` OR
its single string is referenced by exactly one function in BOTH programs.** That kills 26/26 +
2/2 judged-wrong and preserves the 3 sub-mode-B pairs that are almost certainly correct (the
BinDiff oracle, not ghidriff, is wrong on those). Also: enforce the declared-but-dead
`MIN_STRING_LEN=5`, drop the cross-compiler-incomparable `ref_count` from StrUnique's key, fix
the misleading `ONE_TO_MANY = True` class attribute, delete the dead `strings_in_func`
collection. This task SUBSUMES scout 4's proposed `UniqueStringAnchorHasher` — a gated SRH *is*
the unique-string-anchor correlator; two overlapping string correlators would fight over the
same pool.

### T2 — Export per-match scores + STL/template gate on VT (opus)
Autopsy §VTCombinedReference: scores computed at vt_ref.py:144-146 are discarded at :171-172
(`+= 1`); matches.json has exactly 5 keys, no score. `--vt-ref-min-score 9.5` culled ZERO
candidates (`below min_score: 0` — non-binding), so the floor cannot be tuned without scores.
Export (similarity, confidence, product) per VT accept and the mnemonic ratio per Implied
accept, thread them to the `function_matches` writer (ghidra_diff_engine.py:1652-1663) via a
side-channel keyed by (src,dst) — do NOT change the `{type: count}` value shape of the `matches`
dict (bsim.py:83-90, decomp_correlate, and the matched-list builder at
version_tracking_diff.py:330-341 all read it). Second change, from forensics §2: **exclude
STL/ObjVector/template-internal candidates from VT acceptance** (stlpmtx_std, _Copy_Construct,
_Param_Construct, ObjVector<…>::{push_back,resize,operator=}, _M_fill_insert_aux) — 0.324→0.393
on judged at ~5% pool cost; they collide trivially cross-compiler and have no porting value.
Explicitly DO NOT add a VT min-size floor (proven inverted: size≥128 → 0.182).

### T3 — BSim deployment + runner hardening (sonnet)
Scout-bsim-deploy proved: both patches apply cleanly to fork master (`git apply --check` = 0,
topk first); the jar-swap takes ~10s; BSim already completed in 152s on the seeded run; the
actual killer of run 1 was the 2h42m `decomp_correlate` stage. The ghidriff flag
`--no-decomp-correlate` exists (ghidriff commit 29d7f94) but is NOT in
`run_ghidriff_xenon.sh` — adding it is the single change that makes the next run ~15-20 min.
Apply patches anyway (cheap, correctness-positive for near-dup families, adds the Msg.info
observability that detects cross-compiler IDF truncation). Update the runner header docs:
GHIDRA_INSTALL_DIR must point at the fork dist (12.2 gzf format), recommended invocation with
`RB3_XENON_BSIM=1`.

### T4 — Eval/oracle hygiene + direct rb3wii seeds (opus)
Forensics §1D: ≥7 of VT's 25 "wrong" are eval artifacts — 6 Wii↔Xbox platform-class renames
(WiiMovie↔DxMovie, Band*↔Ham*, *Wii↔Ng*/*Xbox) the name-join can't bridge, plus 1
`normalize_demangled` arity bug (Wii `Fff` parsed as 2 params vs MSVC `MMM`=3;
QuatKeys::SetFrame sim=1.0 judged disagree). Fixing the eval raises measured VT ~0.324→~0.51
and stops us mis-tuning against a broken meter. Also: per-category (band3/system/network)
stratification (scout 4 measured VT game-code ≈0-20% vs engine ≈40-54% — the aggregate is
misleading), `--exclude-match-types`, a `--min-vt-score` sweep mode consuming the §3 contract,
and a low-trust flag for BinDiff oracle pairs on ≤88-byte stub-shaped functions (forensics §1B:
BinDiff pairs identical `return Symbol(...)` shapes arbitrarily — it is the oracle that's wrong
on the 3 sub-mode-B SRH pairs). Seeds: ingest `rb3-xenon/unified_id_rb3wii.json` at sim=1.0,
conf≥0.95 (41 new direct band3 anchors with both addresses in-file; holdout exclusion + range
checks) and regenerate seeds.json. `normalize_demangled` lives in build_xenon_seeds.py and is
imported by the eval — that is why seeds and eval are ONE task (file disjointness).

### T5 — Vetted-identity export for the 328 game-code identities (sonnet)
Scout 4's cheap verification protocol, productized as a script: ACCEPT ExactInstructions (12),
FILTER VT by TU-cluster coherence (`xenon_spread/max(wii_spread,1) < 10` AND
`xenon_spread < 50000` → 56 coherent), cross-check against `unified_id_rb3wii.json` by ADDRESS
(no name normalization needed — keeps T5 independent of T4), REJECT string-hasher types pending
T1. Output a tiered `vetted_identities.json` consumable by rb3-xenon. This is the template the
post-run tier export will generalize.

---

## 5. Rejected / deferred scout recommendations (and why)

1. **REJECT — "rework bsim-topk-cap.patch to cap at findSimilarNodes" (scout 4).** The patch
   ALREADY implements top-K at `findSimilarNodes` (PriorityQueue + PAIR_RANK) — scout 3 read the
   patch and verified rank-1 preservation on 600-member near-dup families synthetically. Scout
   4 was describing the obsolete `bsim-perf-candidatecap` 500-empty-set prototype. Do NOT use
   that branch; do not rewrite the patch.
2. **REJECT — separate BSim calibration mini-run before the full run (scout 4).** With
   `--no-decomp-correlate` the full run is ~15-20 min and the eval stratifies per match-type;
   the next gated run IS the calibration. An extra human-gated run costs more than it saves.
   BSim output stays quarantined under its own `BSIM` type until that number exists.
3. **DEFER — relaxed BinDiff seed threshold sim≥0.95 (+~520 seeds at est. 70-80% precision)
   (scout 4 lever 4).** Directly dilutes the seed tier (currently ~90.6%) against the >0.85
   goal, and seeds propagate through VT/BSim. Revisit only after the high-precision tier holds.
4. **REJECT — VT min-size floor.** Measured inverted (forensics §2: size≥128 → 0.182; correct
   VT matches skew small).
5. **DEFER — tuning `--vt-ref-min-score` now.** Non-binding on the emitted set
   (`below min_score: 0`). Superseded by T2's score export + T4's offline sweep; pick the
   operating point AFTER the next run, offline.
6. **MERGED — `UniqueStringAnchorHasher` (scout 4 lever 2)** into T1; the gated SRH is the same
   correlator. A separate path-string content filter is left to T1's judgment (the 1:1 gate
   already handles cross-compiler path mismatch naturally: differing content simply never
   keys together).
7. **REJECT — second-iteration seeding of the 63 exact matches (scout 4 lever 5).** ~24
   projected matches; noise-level. BSim is the real second-iteration lever and is covered.
8. **DEFER — RTTI-derived seeds (292 named methods, scout 4 §6).** Plausible but needs a
   name→Wii-addr join of its own; not needed to prove the next run. Optional stretch inside T4
   ONLY if the rb3wii ingestion lands early.
9. **REJECT — standalone "extract string bytes from gzf" deliverable (scout 4).** T1's offline
   validation pass over the existing gzfs covers the need; the forensics scripts
   (`forensics/recompute_strkeys.py`, `string_global_uniqueness.py`) already exist as templates.

---

## 6. The gated re-run playbook (HUMAN ONLY — no agent starts this)

After all five tasks land:

```bash
cd /home/free/code/milohax/rb3
GHIDRA_INSTALL_DIR=/home/free/code/milohax/ghidra/build/ghidra \
  JAVA_HOME=/usr/lib/jvm/java-17-openjdk \
  RB3_XENON_BSIM=1 \
  ./tools/ghidra/run_ghidriff_xenon.sh          # now includes --no-decomp-correlate

build/SZBE69_B8/ghidra/ghidriff-venv/bin/python tools/ghidra/eval_xenon_matches.py \
  --run-dir build/SZBE69_B8/ghidra/ghidriff-xenon
# then sweep VT offline:  ... eval_xenon_matches.py --min-vt-score <sweep> (T2 scores + T4 mode)
# then vet:               python3 tools/ghidra/vet_xenon_identities.py ...
```

Hold constant vs baseline: seed set provenance documented in seeds stats.json; note BSim is now
ON (the baseline was OFF — per autopsy, BSim shifts every downstream pool, so compare per-type
precision, not raw counts). Expected: ~15-20 min total; BSim ~3-5 min.

Success criteria for the run: (a) StringsRefs/StrUnique judged precision ≥0.8 or their match
count collapses to a small unique-anchor set; (b) VT measured ≥0.45 under the fixed eval, with
an offline score threshold achieving ≥0.85 at nonzero yield; (c) first measured BSim precision;
(d) high-confidence tier (seeds + exacts + gated strings + Implied + VT≥threshold) ≥0.85 judged
with total matches > the current 1,313-strong trustworthy subset.

## For the next agent
- Implementers: read your task's `read_docs` BEFORE coding; write your results to
  `docs/decomp/xenon-hardening/task-<id>-impl.md` (self-contained, file:line anchors, measured
  numbers, "## For the next agent" section).
- Hard constraints repeated: NO full ghidriff runs, NO pyghidra-service restarts, offline
  verification only (read-only minutes-bounded gzf passes allowed), stage only your own files,
  ghidriff work on `rb3-improvements`, Ghidra patches on branch `bsim-xenon-patches` (never
  master).
