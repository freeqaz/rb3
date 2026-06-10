# Wii↔Xenon ghidriff precision hardening — session record (2026-06-10)

**Mission:** harden the Wii(Bank8/MWCC)↔Xenon(MSVC, stripped 65k-func) ghidriff identity-porting
pipeline after the first full experiment measured OVERALL judged precision **0.440** (carried by
trivial exacts), VTCombinedReference **0.324**, StringsRefsHasher **0.000**, holdout recall
**20.6%**, and a run dominated by a 2h42m `decomp_correlate` stage.

**Outcome:** five tasks landed (3 CONFIRMED, 2 PARTIAL by adversarial verification; both partials'
rerun-blocking defects fixed during synthesis). The system **IS ready for the human-gated
validating re-run** (~15–20 min predicted vs hours), with a fixed eval meter that already —
provably, on the existing artifacts — reads VT at 0.486 and OVERALL at 0.500.

All work is documented in handoff docs under [`xenon-hardening/`](xenon-hardening/):

| Doc | Role |
|---|---|
| [PLAN.md](xenon-hardening/PLAN.md) | Planner strategy, task DAG, schema contract, rejected levers |
| [scout-code-autopsy.md](xenon-hardening/scout-code-autopsy.md), [scout-failure-forensics.md](xenon-hardening/scout-failure-forensics.md), [scout-bsim-deploy.md](xenon-hardening/scout-bsim-deploy.md), [scout-recall-levers.md](xenon-hardening/scout-recall-levers.md) | Pre-plan scouting (mechanisms, measured failure modes) |
| [task-T1-impl.md](xenon-hardening/task-T1-impl.md) / [task-T1-verify.md](xenon-hardening/task-T1-verify.md) | String-hasher 1:1-uniqueness gate — **CONFIRMED** |
| [task-T2-impl.md](xenon-hardening/task-T2-impl.md) / [task-T2-verify.md](xenon-hardening/task-T2-verify.md) | Score export + VT STL gate — **CONFIRMED** |
| [task-T3-impl.md](xenon-hardening/task-T3-impl.md) / [task-T3-verify.md](xenon-hardening/task-T3-verify.md) | BSim deploy + runner hardening — **PARTIAL → fixed in synthesis** |
| [task-T4-impl.md](xenon-hardening/task-T4-impl.md) / [task-T4-verify.md](xenon-hardening/task-T4-verify.md) | Eval/oracle hygiene + rb3wii seeds — **CONFIRMED** |
| [task-T5-impl.md](xenon-hardening/task-T5-impl.md) / [task-T5-verify.md](xenon-hardening/task-T5-verify.md) | Vetted-identity export — **PARTIAL (post-run tool; fixes specced)** |
| [SYNTHESIS.md](xenon-hardening/SYNTHESIS.md) | Agent handoff version of this record |
| `xenon-hardening/forensics/` | Reproducible offline evidence (gzf recomputes, gate replays, TU-cluster checks) |

Baseline artifacts: `build/SZBE69_B8/ghidra/ghidriff-xenon/{eval_report.json, json/*.matches.json,
ghidriff.log}` (run of 2026-06-10, BSim OFF, decomp_correlate ON, 2,645 matches).

---

## 1. What landed, by task (honest verdicts)

### T1 — String-hasher global 1:1-uniqueness gate — CONFIRMED
ghidriff `5b9cc4e` (branch `rb3-improvements`): `gate_string_keys` in `ghidriff/correlators.py`
keeps a string-multiset key only if it has ≥2 unique strings OR its single string is referenced by
exactly one function in the program (the other side enforced by the cascade's one_to_one). Also:
MIN_STRING_LEN=5 actually enforced, `ref_count` removed from StrUnique's cross-compiler-incomparable
key, `ONE_TO_MANY=True→False` (attribute consumed nowhere), dead `strings_in_func` deleted.

**Proven offline (verifier reproduced byte-identically via independent read-only gzf re-run):**
- All 25 judged-wrong pairs killed (23 SRH + 2 StrUnique). The brief's "26" includes the 3
  sub-mode-B pairs that are **BinDiff-oracle errors** (forensics §1B) — those survive by design.
- Whole population: SRH 610 → 283 survivors (155 unique-single + 128 multi-string); StrUnique
  45 → 10. 19/19 unit tests against the real module.

**Not provable offline:** recall on TRUE string matches (zero judged-correct SRH pairs exist on
this run). Known side effects: same-ISA reuse of ghidriff inherits lower string-correlator recall;
the 327 released pairs flow to later one_to_many stages of unknown precision.

### T2 — Per-match score export + VT STL/template gate — CONFIRMED
ghidriff `31a6f6c`: a `pair_scores` side-channel (keyed `(str_src,str_dst)`, bare-hex addresses)
carries VT `{similarity, confidence, product}`, Implied `{ratio}`, BSim `{similarity, confidence}`
into an **optional `scores` field** per `function_matches` entry (PLAN §3 contract; `{type:count}`
matches-dict shape untouched — all readers audited). VT accept loop now rejects STL/template
internals via owner-anchored `DEFAULT_VT_EXCLUDE_PATTERNS` (CLI-overridable; `--vt-ref-min-score`
semantics unchanged; **no min-size floor** — measured inverted).

**Proven offline:** 18/18 new + 10/10 pre-existing tests; replay over the REAL 722-match VT pool
excludes exactly 19 STL internals (5 of them judged-wrong) and keeps real container-param
functions; verifier independently reproduced the replay and probed all 41,232 Wii names for gate
collateral (3 boilerplate `resize` bodies — negligible). This converts every future threshold
decision into an offline sweep instead of a 2h re-run.

### T3 — BSim patches + runner hardening — PARTIAL, rerun-blocker FIXED in synthesis
Ghidra fork branch `bsim-xenon-patches` (`1220f13915` top-K cap, `eebbd8ba3a` parallel
aggregation; payload verified byte-equivalent to the patch files despite necessary manual
application), jar-swapped into the fork dist and **verified reachable** from the runner's default
`GHIDRA_INSTALL_DIR` (symlink chain + `javap` of in-jar classes + `.orig` backup). rb3 `d53240b8`
added `--no-decomp-correlate` (the actual killer of run 1) and flipped the default install dir to
the fork (the gzfs are 12.2-format; `/opt` 12.1.2 cannot open them).

**Refuted by the verifier:** the BSim toggle. `${RB3_XENON_BSIM:+--bsim} ${RB3_XENON_BSIM:---no-bsim}`
made the documented invocation (`RB3_XENON_BSIM=1`) **crash argparse** (`--bsim 1` → exit 2) and
made "unset" run silently BSim-OFF — defeating the task's whole point (first measured BSim number).

**Fixed during synthesis (rb3 `53f7a6aa`):** real `BSIM_FLAG` conditional — unset or `=1` → `--bsim`
(default ON), `=0` → `--no-bsim`; comment aligned; JAVA_HOME marked as dead text (pyghidra resolves
the JDK via PATH; dist requires ≥21). Re-verified with the verifier's own recipe: `bash -n`, the
3-env dry-run table, and replaying the exact CMD through ghidriff's real argparse
(`PARSE OK; bsim=True; decomp_correlate=False` / `bsim=False` with `=0`).

### T4 — Eval/oracle hygiene + direct rb3wii seeds — CONFIRMED
rb3 `da52aac0`. All new eval behaviour is **default-OFF and byte-identical** to the stored
eval_report.json when off (verifier replayed). New: `--credit-platform-alias` (Wii↔Xbox class
twins: WiiMovie↔DxMovie etc. + the `Fff`↔`MMM` arity-normalization bug), `--exclude-match-types`,
`--min-vt-score`/`--sweep-vt-score` (consume T2's optional scores; verified no-ops while scores
absent), `--low-trust-stub` (BinDiff-arbitrary ≤88-byte stub shapes), `--stratify`
(band3/system/network).

**Proven offline:** with `--credit-platform-alias`, VT judged precision is **0.486** (18/37) and
OVERALL **0.500** on the EXISTING run — exactly 6 high-conf flips exist, so the plan's "~0.51" was
slightly optimistic and 0.486 is the honest number. Seeds: +24 net (1,189 → 1,213; 15 band3 +
9 network), not the planned +41 — the 41 ignored 16 holdout collisions that must be excluded.
**Critical catch:** `unified_id_rb3wii.json`'s `wii_addr` is **Bank 5**, not Bank 8
(0x8013cd10 = "SetInCoda" there but = RebuildBeats in the Bank 8 map); T4 re-resolves every
wii_name → Bank 8 address via a 1:1-unique normalize-join, so the seeds are immune to the bank
drift (verifier independently confirmed, 5/5 spot checks). 36/36 tests.

### T5 — Vetted-identity export (`tools/ghidra/vet_xenon_identities.py`) — PARTIAL
rb3 `d17d5e55`. The **tiering core is sound and confirmed**: four tiers over all 2,645 matches
(ACCEPT 1,268 / FILTERED_VT 280 / CAUTION 442 / REJECT 655), TU-cluster coherence for VT, selftest
13/13, and every scout-4 calibration number reproduced exactly (band3 new_coverage:
12 ExactInstructions ACCEPT, 55 FILTERED_VT, 74 CAUTION, 180 REJECT; AccomplishmentManager.o →
CAUTION at 2.78MB xenon spread; BandScreen.o → FILTERED_VT at 936B). CLI is post-T2-ready
(`--min-vt-score`, `--accept-types`, `--tier-config`).

**Refuted by the verifier (annotations, not tiering):**
1. The rb3wii cross-check compares Bank 8 p1 addresses against Bank 5 `wii_addr` — the
   `confirmed/contradicted` labels are **anti-informative** (28/637 "contradicted" are exact
   mangled-name AGREEMENTS, incl. 20/53 at conf≥0.95). Same Bank-5 landmine T4 caught; T5 didn't.
2. `_categorize_full` mislabels **79%** of entries as `band3` (checks nonexistent `system_wii\`
   prefixes; true split 456 band3 / 1,931 system / 149 network).
3. `ExactMnemonicsFunctionHasher` (unmeasured, 4 matches) silently in default ACCEPT; 65 entries
   export `wii_addr: null`.

Vetting runs **after** the gated re-run, so this does not block the rerun — but **no consumer may
act on `rb3wii_check` or `category` until the fixes in
[task-T5-verify.md §For-the-next-agent](xenon-hardening/task-T5-verify.md) land** (Bank5-ELF
mangled-name join; reuse the eval's `categorize_tu`; drop ExactMnemonics from default accept; emit
`wii_addr` unconditionally; regenerate `vetted_identities.json`).

### Synthesis-stage actions (this session)
- **rb3 master fast-forwarded** `d935f117..9658d75e` (branch `xenon-hardening-t1` → master, the
  T1-verify required followup); working tree returned to master with concurrent agents' unstaged
  edits intact.
- **Runner BSim toggle fixed + verified** (rb3 `53f7a6aa`, see T3 above).
- This record + [SYNTHESIS.md](xenon-hardening/SYNTHESIS.md) + the untracked handoff docs committed.

---

## 2. Commit ledger

| Repo / branch | Commit | What |
|---|---|---|
| ghidriff `rb3-improvements` | `31a6f6c` | T2: score export + VT STL gate |
| ghidriff `rb3-improvements` | `5b9cc4e` | T1: string-hasher 1:1-uniqueness gate |
| ghidra `bsim-xenon-patches` | `1220f13915`, `eebbd8ba3a` | T3: BSim top-K cap + parallel aggregation (jar-swapped into fork dist; **disk-only swap** — a dist rebuild reverts it, rollback = `cp "$JAR.orig" "$JAR"`) |
| rb3 master | `d53240b8` | T3: runner — fork GHIDRA_INSTALL_DIR default, `--no-decomp-correlate` |
| rb3 master | `d17d5e55` | T5: `vet_xenon_identities.py` |
| rb3 master | `da52aac0` | T4: eval flags + seed ingestion (seeds dir itself is a gitignored build artifact) |
| rb3 master | `10b98155`, `9658d75e` | T1 impl doc + forensics; T1 verify (landed via branch ff) |
| rb3 master | `20402177`, `d935f117` | T3/T2 impl handoff docs |
| rb3 master | `53f7a6aa` | Synthesis: RB3_XENON_BSIM toggle fix |

ghidriff is editable-installed in the run venv (verified: `__editable__…pth` →
`/home/free/code/milohax/ghidriff`), so the rerun picks up T1+T2 automatically.

---

## 3. The gated re-run (HUMAN ONLY)

Preconditions: machine otherwise free (the run starts its own large JVM); do NOT restart the
pyghidra service on 8001; if the Ghidra fork dist was rebuilt since 2026-06-10, re-apply the
BSim jar swap first (task-T3-impl.md).

```bash
cd /home/free/code/milohax/rb3

# 1. The run. Defaults are now correct: fork GHIDRA_INSTALL_DIR, BSim ON,
#    --no-decomp-correlate, 1,213-pair seeds. (RB3_XENON_BSIM=0 would disable BSim.)
./tools/ghidra/run_ghidriff_xenon.sh

# 2. Score it under the FIXED meter (alias/arity crediting + per-category stratification):
build/SZBE69_B8/ghidra/ghidriff-venv/bin/python tools/ghidra/eval_xenon_matches.py \
  --run-dir build/SZBE69_B8/ghidra/ghidriff-xenon \
  --credit-platform-alias --stratify

# 3. Sweep the VT operating point OFFLINE against the exported scores (T2):
build/SZBE69_B8/ghidra/ghidriff-venv/bin/python tools/ghidra/eval_xenon_matches.py \
  --run-dir build/SZBE69_B8/ghidra/ghidriff-xenon \
  --credit-platform-alias --sweep-vt-score 9.5:14:0.5

# 4. Tier export (AFTER applying the T5 fixes in task-T5-verify.md):
python3 tools/ghidra/vet_xenon_identities.py \
  --run-dir build/SZBE69_B8/ghidra/ghidriff-xenon \
  --min-vt-score <threshold chosen in step 3>
```

Post-run sanity (from T2-verify): every `VTCombinedReference` entry should carry
`scores.…​.product`, every `Implied Match` entry a `ratio`; addresses in matches.json are BARE hex
(no `0x`). Compare per-match-type precision vs baseline, NOT raw counts (BSim ON shifts every
downstream pool).

---

## 4. Expected outcome — predicted vs proven

Baseline (measured 2026-06-10 run): OVERALL 0.440 · VT 0.324 (12/37 judged; 722 matches) ·
StringsRefs 0.000 (0/26; 610) · StrUnique 0.000 (0/2; 45) · ExactInstructions 0.935 (29/31; 63) ·
Implied 0.750 (3/4) · holdout recall 20.6% (90.6% precision on recovered) · runtime hours
(decomp_correlate 2h42m).

**PROVEN already, offline, on the existing artifacts:**
- Eval fix alone: VT 0.324 → **0.486**, OVERALL 0.440 → **0.500** (exact replay, 6 flips).
- T1 gate: 25/25 judged-wrong string matches killed; population SRH 610→283, STU 45→10;
  3 oracle-error survivors kept.
- T2 STL gate: 19/722 VT matches removed, 5 of them judged-wrong.

**PREDICTED for the re-run (not proven until it happens):**
- Runtime ~15–20 min (decomp_correlate removed; BSim ~3–5 min — it already completed in 152s
  seeded).
- OVERALL judged ≥ ~0.5; VT measured ≥0.45 under the fixed meter (forensics projects ~0.52 with
  the STL gate, unverifiable offline).
- A VT score threshold found offline with judged precision ≥0.85 at nonzero yield, making the
  high-confidence tier (seeds + exacts + gated strings + Implied + VT≥threshold) ≥0.85 judged —
  the PLAN's success criterion. This is the plan's central bet, not a measurement.
- String survivors: SRH yield collapses toward a unique-anchor set (~283 scale); true precision
  est. 0.8–0.9 (population shape, unmeasured). **The judged SRH set may mechanically re-read
  0.000** if it consists only of the 3 sub-mode-B oracle-error pairs (xenon 0x827ffbf8,
  0x827d2588, 0x827f7cc0) — that is the oracle artifact, not gate failure.
- First measured BSim precision (scouts' prior: ~20–50% cross-compiler); quarantined under its own
  type either way.
- Recall: modest gain from +24 seeds and BSim; no quantified prediction — 64,742 functions
  unmatched at baseline and the recall levers (relaxed seeds, RTTI seeds) were deliberately
  deferred until the precision tier holds.

---

## 5. Risks / open items

1. **T5 annotations are anti-informative until fixed** — `rb3wii_check` labels true agreements
   "contradicted" (Bank5-vs-Bank8 address spaces) and `category` mislabels 79% of entries. Do not
   triage on them; the tiering and calibration themselves are sound. Fix recipes + measured flip
   counts: task-T5-verify.md.
2. **Sub-mode-B scoring decision needed before reading the next eval** (or SRH re-reports 0.000 on
   a 3-pair judged set). Options: human re-judgement or an oracle exception for the 3 xenon addrs.
3. **T1 true-match recall unmeasured**; same-ISA ghidriff reuse inherits lower string-correlator
   recall; 327 released pairs flow to one_to_many stages of unknown precision.
4. **BSim jar swap is disk-only** — any `gradle` dist rebuild silently reverts the patches.
5. **The 24 new rb3wii seeds are stub-sized (4–40B)** — exactly the BinDiff-arbitrary shape class
   from forensics §1B; their "~90% precision" is extrapolation. Watch these anchors.
6. **`--low-trust-stub` shrinks the precision denominator** — never quote its numbers against the
   0.32 experiment bar.
7. The ≥0.85 tier target is a projection contingent on the offline VT sweep finding a usable
   operating point.

## 6. Next steps

1. Apply the four T5 fixes (task-T5-verify.md), regenerate `vetted_identities.json`.
2. Decide sub-mode-B oracle handling (risk 2).
3. HUMAN: run §3. Then the offline sweep → pick the VT threshold → export the high-confidence tier.
4. Optional pre-run tweak: `--skip-correlators StrUniqueFuncRefsHasher` (post-gate it is a near-
   duplicate of gated SRH — 10 survivors, 6 shared). Not required.
5. After the run: re-baseline `test_replay_stl_exclusion_over_real_vt_pool` if the VT pool changed
   (T2-verify note); ingest results into the divergence/identity tooling in rb3-xenon.
