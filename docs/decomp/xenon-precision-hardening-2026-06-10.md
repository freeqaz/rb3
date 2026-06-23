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

---

## 7. RUN 3 RESULTS (2026-06-10 evening — the validating re-run)

Run 3 executed with everything above live: fork dist + BSim jar swap, BSim ON, `--no-decomp-correlate`,
gated string hashers (T1), score export + STL gate (T2), 1,213 seeds (T4). Wall clock: matching ~8 min
(BSim stage 135.5s — the top-K cap works at 36k×65k scale; this stage previously stalled 70+ min),
post-match diff/report ~1h50m (scales with the 3.2× matched pool — the new bottleneck, optional output).

### Headline vs baseline (run 2)

| Metric | Run 2 | Run 3 |
|---|---|---|
| Total matches | 2,645 | **8,527** (3.2×) |
| Holdout recovery (recall) | 20.6% | **63.8%** (90/141 correct) |
| Holdout precision on recovered | 0.906 | 0.833 |
| New identities (no prior) | 1,032 | 5,997 |
| **Vetted ACCEPT tier** | n/a (~70 trustworthy) | **2,207 (997 new: 309 band3 / 438 system / 216 network)** |

### Per-correlator (run-3 log + eval)

- **BSim: 5,939 matches** (first cross-binary measurement). Holdout precision 0.800 raw.
  **sim×conf is the discriminative axis** (similarity alone is useless — flat ~0.15 even at 0.99):
  ≥10 → 0.887 (1,969 pop), **≥15 → 0.933 (922 pop)**, ≥20 → 0.964 (522 pop). Below 10 ≈ 0.5 → CAUTION.
- **VTCombinedReference: 1,093 accepted** (9,758 candidates; 227 STL-excluded by T2 gate; 541 min-len).
  Judged precision DROPPED (0.109 raw / 0.236 alias-credited) — VT propagated from the BSim-contaminated
  seed graph; its product score does not rescue it (0.25 @ ≥100). VT is now the weak link; treat as CAUTION.
- **Gated StringsRefsHasher: 191** (was 610 ungated); judged set is only the 3 known oracle-error pairs
  (mechanical 0.000, predicted in advance — NOT gate failure). True precision still unmeasured.
- ExactInstructions 61 @ 0.935-0.958; Implied 8 (2/2 judged); the 3 sub-mode-B addrs behaved as predicted.

### Oracle conflict (important for reading any future eval)

The dc3-BinDiff "high-conf disagree" oracle reads BSim at 0.193-0.319 while the clean holdout reads
0.800-0.964 at the same thresholds. The bindiff oracle counts every semantic-vs-structural disagreement
as OUR error; with platform-alias crediting 22 verdicts already flipped. Trust the holdout; use bindiff
agreement directionally only. (Both can't be right: at sim×conf≥15 holdout=0.933 vs dc3=0.512.)

### Vetted export (the deliverable)

`vet_xenon_identities.py` (post-T5-fix `4c9541e0` + new `--min-bsim-simconf` gate) over run 3 with
`--min-bsim-simconf 15`:

```
ACCEPT 2,207   (seeds 1,210 + BSim simconf≥15 922 + ExactInstr 61 + Implied 8 + SwitchSig 5 + SymbolsHash 1)
CAUTION 5,881  (sub-threshold BSim ~0.5, VT cluster-coherent, etc.)
FILTERED_VT 233 / REJECT 206
```

Output: `build/SZBE69_B8/ghidra/ghidriff-xenon/vetted_identities.json` (run-2 artifacts archived in
`run2-baseline-archive/`). Expected ACCEPT-tier precision ≈ 0.93-0.96 (seeds ≈1.0, BSim block 0.933±CI,
exacts 0.94+). Holdout n is modest (45 kept at ≥15) — CI is wide; the point estimate clears 0.85.

### Verdict

The plan's central bet — a score-thresholded ≥0.85-precision high-confidence tier — is **CONFIRMED on
the holdout oracle**, delivered by BSim sim×conf (not VT, which the plan expected to be the workhorse).
Cross-compiler porting Wii→Xenon is now a working lever: ~1,000 new vetted identities per run, recall
3.1× the baseline.

### Open follow-ups

1. VT precision collapse under a BSim-seeded graph: either feed VT only ACCEPT-tier seeds (two-pass run)
   or accept VT as a CAUTION-tier feeder. 2. band3 stratum is BSim's weakest (0.193 on the pessimistic
   oracle; holdout band3 subset too small to read) — vet band3 CAUTION entries before use. 3. Holdout n
   should grow (cheap: promote vetted ACCEPT identities to next run's holdout). 4. sdk stratum is 0.000 —
   exclude sdk from ACCEPT consumers (already only 12 entries). 5. rb3wii cross-check shows possible
   Bank5-vs-Bank8 mangling-spelling false-contradictions (e.g. GetLocalParticipants) — annotation only.

---

## ROUND 2 RESULTS (2026-06-11)

Full synthesis: `docs/decomp/xenon-hardening/round2/SYNTHESIS.md` (read it first; this
section is the digest). Round-2 docs: `docs/decomp/xenon-hardening/round2/` (PLAN, 3 scouts,
T1–T4 impl + adversarial verify docs, 30 evidence packs + judge verdicts under `forensics/`).
Note: the standalone round-2 record `docs/decomp/xenon-hardening-round2-2026-06-11.md`
(commit `0d3afded`) has CORRECT measurements but a STALE status narrative ("T1/T2/T3 did not
execute") — it raced the concurrent tasks by minutes; trust its verifier addendum + the
synthesis, not its §1/§2b,c/§4/§6.

### 1. First human-judged precision (30-pair stratified band3 ACCEPT sample)

**Overall 0.900 (27/30) · BSim 0.905 (19/21) · non-BSim 0.889 (8/9).** Per stratum:
BSIM≥30 6/6, BSIM 20–30 7/8, BSIM 15–20 6/7, ExactInstr 5/5, SwitchSig 2/3, Implied 1/1.
**The 0.933 holdout calibration HOLDS** (0.905 n=21 vs 0.933 n=45, within CI) — and the
dc3-BinDiff pessimistic oracle (0.19–0.32 at the same thresholds) is refuted as arbiter.
The 3 wrong pairs (13/16/29) share ONE failure mode — **same-TU sibling aliasing**: template/
sibling bodies identical except a type-tag immediate or node-size literal (pair-16: Xenon
writes kDataFloat=1, claimed Wii sibling writes kDataInt=6; true partner `__ct<PCc,f>`), or
a hash-shape match refuted by strings (pair-29 → `BandTrack::SetInstrument`,
`src/system/bandobj/BandTrack.cpp:544`). Caveat: band3-only; system/network unmeasured at
human quality.

### 2. Two-pass VT rescue — hypothesis REFUTED (run 4)

Seeding the cascade from ONLY the vetted ACCEPT tier (2,130 1:1 anti-leak seeds ≈0.93) did
NOT rescue VTCombinedReference. Same eval flags as run 3 (`--credit-platform-alias
--stratify`, plus the mandatory `--seeds seeds_accept_run3.json`):
**VT 0.236→0.222 alias, 0.109→0.093 raw** (marginally worse); holdout meter stable
(0.603/0.825 vs 0.638/0.833). Product-score sweep over VT's real range (20–820): no
operating point ≥0.85 at meaningful yield (the 1.000 at floor 260 is n=1 — sample
exhaustion, not signal). **VT's weakness is intrinsic to the MWCC→MSVC reference graph, not
seed contamination. VT is demoted to CAUTION-tier feeder PERMANENTLY** (resolves run-3 open
follow-up #1 in the negative; the mid-band seeds+exacts fallback is not indicated, 0.22≪0.4).
Byproducts: `--matches-only` fork flag (ghidriff `e52d935`) cuts a run from ~115 min to
**~8 min**; run-4 re-vet grew ACCEPT to 2,246 (+39); ExactInstr held 0.967.

### 3. rb3-xenon ingest LANDED (gate 0.905 ≥ 0.85 → full)

`rb3-xenon/ghidriff_identities.json` — **978 entries** (913 BSIM simconf≥15 / 54 ExactInstr /
8 Implied / 3 SwitchSig; system 438 / band3 306 / network 216 / Bink 14 / main 4), from the
immutable run3-archive; excludes 1,210 SeedMatch-only + 9 sdk + 7 null-symbol + 3
judged-WRONG. fn_resolver tier **T4b `ghidriff_wii_b8`** (between fuzzy_pairs and
bindiff_dc3; conf 0.94/0.93/0.90). Verified 978/978 `wii_addr_bank8` agree with the Bank-8
CW map; target_symbol_map untouched. Commits: rb3 `6a4779b2`/`6793c59a`, rb3-xenon `7bdae6c`.

### 4. Holdout grown 146→158 + first known-negatives

27 judged-correct → 4 already-in-holdout + deterministic XOR split: **12 → holdout**
(exact-Bank-8-addr scored) / **11 → reserved** (`reserved_seed_candidates_round2.json`,
unconsumed); **3 judged-wrong → `known_negatives.json`** with a new eval oracle.
`build_xenon_seeds.py` union-merge + extra-holdout exclusion and eval exact-addr/known-neg
modes are default-off and byte-identical-replay proven vs run3-archive (44/44 tests). rb3
`4daa00fa`.

### 5. Known deviations (all accounted, none metric-contaminating)

- **T1↔T3 race:** `seeds_accept_run3.json` was built against the pre-growth 146 holdout →
  on disk it contains 12 grown-holdout addrs AND the 3 known-negative pairs as givens.
  Eval-neutralized (run-4 report: `eligible 146, excluded_as_seeds 12`; seeded matches never
  scored) **iff** `--seeds seeds_accept_run3.json` is passed. Round 3 must rebuild seeds
  post-growth — `build_accept_seeds.py` currently hard-asserts 73 holdout drops (now 85) and
  will crash, fail-safe.
- 85 of the 978 ingested xenon addrs overlap the 158 holdout — harmless now; exclude holdout
  addrs if `ghidriff_identities.json` ever feeds a seed builder.
- `eval_report.json` is last-writer-wins; compare against `run3-archive/` copies only.
- Run-3 archive intact throughout: `run3-archive/vetted_identities.json` md5
  `dbc440b6b2b67b964b208a7c17af625e`.

### Round-3 priorities

1. Rebuild ACCEPT seeds against the grown holdout (fix `build_accept_seeds.py`: derive drops,
   exclude known-negatives). 2. System/network judged sample (closes the band3 extrapolation
   gating 654 ingested identities). 3. Sibling-aliasing vet check (immediate/literal diff on
   near-identical same-TU bodies — would have caught all 3 wrong pairs); SwitchSig audit n≥10.
4. Refresh ingest from run-4's 2,246 ACCEPTs once seeds are rebuilt. 5. Upstream ghidriff PRs:
   `--matches-only` + O(n×m) dedup hash-join. 6. NO further VT investment.

## ROUND 3 RESULTS (2026-06-23)

Full synthesis: `docs/decomp/xenon-hardening/round3/SYNTHESIS.md`. Round 3 did **not** run
ghidriff — it CONSUMED the 978 ingested ACCEPTs. Two deliverables, both adversarially
verified by a final synthesis pass (Opus; Fable unavailable).

### 1. band3 porting-worklist (task #1) — VERIFIED, could not refute

The 232 net-new band3 identities (RB3 game code DC3 cannot provide, across 93 TUs) shipped
as an additive porting worklist + per-fn identity oracle in rb3-xenon: tracked generator
`tools/gen_band3_port_worklist.py` + CW demangler + tracked `docs/plans/band3-port-worklist.md`
(TU-ranked, 47-row HIGH+BSim≥30 first-targets subset) + gitignored `band3_port_worklist.json`
feed. Commit **rb3-xenon `f064c2d`** (branch `main`; additive — 4 files, +1362 lines; **`target_symbol_map.json` / fn_resolver / report.json / build UNTOUCHED**, confirmed by `git show --name-only`). rb3-side handoff doc + re-runner: **rb3 `854489dc`** (branch `xenon-round3-recon`).

Independent re-derivation (all reproduced EXACTLY): 978 total → 762 net-new → band3 232/93 TUs;
certainty 20 high / 27 BSim≥30 / 92 BSim20-30 / 93 BSim15-20. Adversarial checks, **all passed**:
(a) genuinely net-new — band3 ∩ NOT `target_symbol_map` = 232, **0 already in map**; (b) genuinely
band3 — all 232 source category band3; (c) confidence labels — **0/232 mismatches** vs the
re-derived certainty rule; (d) nothing injected into the map; (e) **232/232 `wii_symbol`s resolve
in the CW map to the claimed Bank-8 VA** (independently re-resolved against the correct VA column);
(f) all 93 `src_path`s exist + faithful to the CW obj-path column; (g) 20 HIGH demangles sane, the
3 operator/template fallbacks carry the raw CW name. NOTE: production map grew 13,023→13,199 since
recon (concurrent porters) — the 216-in-map / 762-net-new split is computed against the live map; band3=232 is stable.

### 2. system/network judged sample (task #2) — MEASURED, clears the handoff bar

Round-3 priority #2 CLOSED. A 30-pair stratified sample (15 system + 15 network, all four strata;
`Random(42)`, one batched read-only fork-Ghidra pass ~3 min) was human-judged:
**overall 0.967 (29/30)** — *above* band3's 0.900. **system 0.933 (14/15) · network 1.000 (15/15).**
Per stratum: **high 3/3 · BSim≥30 8/8 · BSim20-30 10/10 · BSim15-20 8/9 (0.889).** The
**HIGH+BSim≥30 slice = 11/11 (1.000).** The lone miss (pair-15, system, BSim 15.142) is round-2's
exact failure mode — **same-TU sibling aliasing**: BSim collapsed two 20-byte `mImp->virtual()`
thunks differing ONLY in the vtable-slot immediate (`TrackWidget::Init` slot 0x44 vs the true
partner `TrackWidget::Empty` slot 0xc). Independently re-confirmed from
`build/SZBE69_B8/asm/system/track/TrackWidget.s` (Init=`lwz r12,0x44(r12)`, Empty=`lwz r12,0xc(r12)`).
This closes the band3-only extrapolation gating the 654 ingested non-band3 identities — they are
validated SAFE to hand off (above 0.85), with the BSim 15–20 tail as the residual confirm-on-consume risk.

**Handoff decision (0.967 ≥ 0.85):** recommend a system/network worklist on the band3 model
(additive, gitignored feed + tracked TU-ranked markdown, NOT a map injection), all four strata
surfaced with BSim 15–20 flagged confirm-on-consume; HIGH+BSim≥30 (system 22+57, network 0+32)
is the safe core. Second-priority behind band3 (DC3 partially covers shared engine + Quazal).
