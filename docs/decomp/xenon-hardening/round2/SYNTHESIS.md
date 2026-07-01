# ROUND 2 SYNTHESIS — Wii↔Xenon ghidriff hardening (2026-06-11, Fable)

Final synthesis over: `PLAN.md`, scouts (S1 `scout-rb3xenon-conventions.md`, S2
`scout-evidence-substrate.md`, S3 `scout-report-cost.md`), implementations
(`task-T1-impl.md` + `task-T1-twopass.md`, `task-T2-ingest.md`, `task-T3-impl.md`,
`task-T4-impl.md`), verifications (`task-T1-verify.md` CONFIRMED,
`task-T2-verify.md` CONFIRMED, `task-T3-verify.md` CONFIRMED, `task-T4-verify.md`
PARTIAL), the 30-pair judge verdicts (`forensics/judge_verdicts.json`), and live
artifacts re-probed during synthesis.

**One-line verdict:** Round 2 delivered the first human-judged precision number
(band3 ACCEPT = **0.900 overall / 0.905 BSim**, validating the 0.933 holdout
calibration), **REFUTED** the round's central hypothesis (ACCEPT-only seeding does
NOT rescue VT: 0.222 alias vs 0.236 baseline), shipped a **978-identity ingest into
rb3-xenon** (fn_resolver tier T4b), grew the **holdout 146→158** with the first
known-negatives file, and made future runs ~14× cheaper (`--matches-only`, ~8 min
vs ~115 min).

---

## (a) Measured sample precision — and what it does to the 0.93 calibration

Human-judged, 30-pair stratified band3 ACCEPT non-seed sample
(`forensics/sample_manifest.json`, evidence packs `evidence/pair-NN.md`,
verdicts `forensics/judge_verdicts.json`):

| Stratum | n | Correct | Wrong | Precision |
|---|---|---|---|---|
| BSIM sim×conf ≥ 30 | 6 | 6 | 0 | 1.000 |
| BSIM sim×conf 20–30 | 8 | 7 | 1 | 0.875 |
| BSIM sim×conf 15–20 | 7 | 6 | 1 | 0.857 |
| ExactInstructionsFunctionHasher | 5 | 5 | 0 | 1.000 |
| SwitchSigHasher | 3 | 2 | 1 | 0.667 |
| Implied Match | 1 | 1 | 0 | 1.000 |
| **BSim total** | **21** | **19** | **2** | **0.905** |
| **Non-BSim total** | **9** | **8** | **1** | **0.889** |
| **ALL** | **30** | **27** | **3** | **0.900** |

**Calibration verdict:** 0.905 (n=21, human) is consistent with the run-3 holdout
calibration 0.933 (n=45, BSim sim×conf≥15) within CI — **the calibration holds**;
this is its first human-grade confirmation. It also settles round-1's oracle
conflict: the dc3-BinDiff "high-conf disagree" oracle (0.19–0.32 at the same
thresholds) is **refuted as arbiter** — trust holdout + human judging. Practical
reading: expect roughly **1 in 10–11 band3 ACCEPTs to be wrong**; consumers
(fn_resolver T4b at conf 0.93) must treat these as probabilistic, not ground truth.

**The 3 wrong pairs and the pattern** (all became `known_negatives.json`):

- **pair-13** (BSIM 20–30, simconf 20.79, xenon `0x82518de0`):
  `_List_base<pair<Symbol,Symbol>>::clear` misattributed — node-size literal to
  `_MemOrPoolFreeSTL` is 16 on Wii vs 0x24=36 on Xenon → a *different template
  instantiation* inside a 30+ same-TU sibling cluster.
- **pair-16** (BSIM 15–20, simconf 16.66, xenon `0x824e51e0`): pair ctor
  `__ct<PCc,i>` claimed, but Xenon stores DataNode type tag **1 = kDataFloat**
  (`src/system/obj/Data.h:23`), not 6 = kDataInt (`Data.h:28`); true partner is the
  float sibling `__ct<PCc,f>` @ Wii `0x803e6740`. BSim similarity 1.0 on bodies
  differing by one immediate — it picks blind. (`verdict-pair-16.md`.)
- **pair-29** (SwitchSig, xenon `0x8233afb0`): claimed
  `ActiveScoreType__12MusicLibraryCFv`, but Xenon references
  `'unrecognized instrument type "%d"'` + guitar/vocals/real_* strings =
  `BandTrack::SetInstrument` (`src/system/bandobj/BandTrack.cpp:544`). Partition
  shape matched; string evidence refutes.

**Failure mode is uniform: same-TU sibling aliasing** — near-identical template/
sibling bodies separated only by a type-tag immediate or node-size literal. Not
fixable from the correlator side; fixable at vet time with a cheap immediate/
literal-diff check on small bodies (round-3 candidate).

Caveats: the sample is **band3-only** (system 438 / network 216 ACCEPTs are
extrapolated, unmeasured at human quality); SwitchSig's 2/3 is a micro-sample
signal, not a measurement.

## (b) Two-pass VT rescue — REFUTED (the round's central hypothesis)

Question: did run-3's VT collapse (0.109 raw / 0.236 alias) come from propagating
over a seeded graph contaminated with ~0.3-precision sub-threshold BSim matches?
Experiment: re-run matching seeded ONLY by the vetted ACCEPT tier.

Run 4 (T1, the round's single permitted ghidriff run): 2,130 ACCEPT-only seeds
(2,207 − 73 holdout-overlap − 4 strict-1:1 dedup; `seeds_accept_run3.json`, built
by `tools/ghidra/build_accept_seeds.py`), BSim ON, `--matches-only` (new fork flag,
ghidriff `e52d935`), matching wall-clock **7.4 min**, report stage skipped
(~107 min saved), 45-min kill never tripped. Eval with identical flags to the run-3
baseline (`--credit-platform-alias --stratify`) plus the mandatory
`--seeds seeds_accept_run3.json`:

| Metric (same eval flags) | Run 3 (1,213 mixed seeds) | Run 4 (2,130 ACCEPT seeds) | Δ |
|---|---|---|---|
| **VT judged precision (alias)** | 0.236 (13/55) | **0.222 (12/54)** | −0.014 |
| **VT judged precision (raw)** | 0.109 | **0.093** | −0.016 |
| VT accepted | 1,093 | 1,193 | +100 |
| BSim judged (alias) | 0.319 | 0.279 | −0.040 |
| Holdout recall / precision | 0.638 / 0.833 | 0.603 / 0.825 | meter stable |
| Total matches | 8,527 | 7,555 | −972 (smaller BSim residual pool) |

VT `scores.product` sweep across its real range (20–820): no operating point
reaches ≥0.85 at meaningful yield — precision "rises" only as the judged sample
exhausts (floor 260 → 1.000 on n=1). Per-stratum VT: band3 0.154, system 0.321,
network/sdk 0.000. Both T1 doc and independent verification (`task-T1-verify.md`,
eval reproduced to the digit) agree.

**Conclusion: VT's weakness is intrinsic to the MWCC→MSVC reference graph, not
seed contamination. Demote VTCombinedReference to CAUTION-tier feeder permanently;
stop investing in VT as an ACCEPT source.** The PLAN's mid-band fallback
(seeds+exacts-only re-seed) is correctly NOT indicated (0.22 ≪ the 0.4 trigger).
Round-1 open follow-up #1 is resolved in the negative.

Byproducts independent of the negative verdict: `--matches-only` works (future
seed-iteration runs ≈8 min); run-4's re-vetted ACCEPT tier grew to **2,246** (+39)
at comparable holdout precision; ExactInstructions held at 0.967 (29/30) — the
reliable non-BSim ACCEPT feeder.

## (c) What landed in rb3-xenon (T2 — gate PASSED, full ingest)

Gate: BSim judged 19/21 = **0.9048 ≥ 0.85 → FULL ingest**. From the immutable
`run3-archive/vetted_identities.json` (md5 `dbc440b6…`):

```
8,527 → 2,207 ACCEPT → −1,210 SeedMatch-only (already in target_symbol_map)
      → −9 sdk (measured 0.000) → −7 null wii_symbol → −3 judged-WRONG = 978
```

- `/home/free/code/milohax/rb3-xenon/ghidriff_identities.json` — **978 entries**
  (913 BSIM simconf≥15 / 54 ExactInstr / 8 Implied / 3 SwitchSig; system 438 /
  band3 306 / network 216 / Bink-None 14 / main 4). Gitignored
  (rb3-xenon `.gitignore:55`), regenerable:
  `python3 tools/ghidra/ingest_ghidriff_accepts.py --gate full`.
- `tools/fn_resolver.py` tier **T4b `ghidriff_wii_b8`** between `fuzzy_pairs` and
  `bindiff_dc3`; confidences ExactInstr/Implied 0.94, BSIM≥15 0.93, SwitchSig 0.90.
- Field name **`wii_addr_bank8`** (Bank-5 fail-fast guard); verification proved
  **978/978** addresses agree with the Bank-8 CW map (`band_r_wii.map`) — genuinely
  Bank 8, unlike `unified_id_rb3wii.json` (Bank 5, untouched).
- Non-clobber proven: `scripts/target_symbol_map.json` byte-identical
  (md5 `4a6b2f826e855c8845c3d9f078729859`); resolver change purely additive
  (old-vs-new diff).
- Commits: rb3 `6a4779b2` + `6793c59a` (master), rb3-xenon `7bdae6c` (main).
- Known wrinkles: one duplicate `wii_addr_bank8` (`Init__11TrackWidgetFv` from two
  xenon addrs — at most one is right; ~0.93-tier noise); **85 of the 978 xenon
  addrs overlap the 158-entry holdout** — harmless today (eval never reads
  rb3-xenon files) but any future seed-builder consuming `ghidriff_identities.json`
  MUST exclude holdout addrs first.

## (d) Holdout growth + anti-leak split (T3)

From the 27 judged-correct: 4 were already in the crossval-146 holdout (pairs
04/23/25/26, dropped from the split); the remaining 23 went through the
deterministic XOR split (sort by xenon_addr, `Random(42)`, ceil(n/2)):

- **Holdout 146 → 158** (+12: pairs 02 03 05 08 12 14 21 22 24 27 28 30), each
  carrying `wii_addr_bank8` → scored by **exact address** (stronger than TU-stem).
- **Reserved-for-future-seeds: 11** (pairs 01 06 07 09 10 11 15 17 18 19 20),
  `reserved_seed_candidates_round2.json`, disjoint from holdout, unconsumed this
  round.
- **Known-negatives: 3** (pairs 13/16/29) → `xenon-seeds/known_negatives.json`,
  with a new eval oracle (exact `(p2,p1)` recurrence counts WRONG; same-p2/
  different-p1 not penalized).
- Plumbing fixed (both planner-verified defects): `build_xenon_seeds.py` now
  union-merges holdout.json (no clobber of grown entries) and excludes
  `--extra-holdout` addrs from seeds; `eval_xenon_matches.py` gained exact-addr
  scoring + known-negatives, all default-off, **byte-identical replay** vs
  `run3-archive/eval_report.json` proven. Tests 44/44. Commit rb3 `4daa00fa`.

**The cross-task race (the one real protocol deviation, fully accounted):**
T1 built `seeds_accept_run3.json` at 10:25 against the original 146-entry holdout;
T3 grew the holdout at 10:33. Result on disk: **seeds ∩ grown-holdout = 12 addrs**,
and the **3 known-negative pairs are seeded as givens** (3/2,130 = 0.14%). Both are
eval-neutralized — seeded addrs are excluded from holdout eligibility
(`eval_xenon_matches.py:443`; run-4 report shows `eligible 146, excluded_as_seeds
12`) and seeded matches are never scored (hence `recurred_exact: 0`) — **iff eval
is invoked with `--seeds seeds_accept_run3.json`**. The file-level invariant is
violated until the seeds are rebuilt post-growth.

## Refuted / superseded / unverified claims (read before trusting any single doc)

1. **REFUTED: the VT-rescue hypothesis** (this round's reason for being) — §(b).
2. **REFUTED: T4's session-record status narrative.** The round-2 record
   (`docs/decomp/xenon-hardening-round2-2026-06-11.md`, commit `0d3afded`) claims
   "T1/T2/T3 did not execute; invariants (b)/(c) vacuous" — false on disk at its
   own 10:27:14 commit (artifacts predate it by 2–4 min; all three tasks committed
   within the following 8 min). Its *measurements* (judge numbers, strata, wrong
   pairs, md5s, commit existence) all verify; its *status sections* (§1, §2b/c, §4,
   §6, PLAN STATUS) are superseded by the verifier addendum + this synthesis.
3. **SUPERSEDED: T3's two-pass proxy table** (synthetic 2,203-seed run) — quote the
   live run-4 eval instead (holdout 85/18 = 0.825; VT 0.222 alias / 0.093 raw).
4. **UNVERIFIED: system/network ACCEPT precision at human quality** — the T2 gate
   extrapolates band3 (0.905) to 654 system+network entries; holdout 0.933 is the
   only cross-stratum number.
5. **WEAK SIGNAL: SwitchSig 2/3** — n=3; pair-29 is a clean string-evidence
   refutation; audit before any future reliance (the 3 ingested SwitchSig entries
   ride on the all-stratum gate, not their own).
6. **Fragile artifacts:** `eval_report.json` is last-writer-wins (current on-disk
   copy is a raw-mode rerun; compare only against `run3-archive/` copies);
   `build_accept_seeds.py` crashes on re-run (hard-asserts
   `EXPECT_HOLDOUT_DROPS = 73` at lines 52/94 vs now-85 — fail-safe but blocks
   regeneration); T2's task-doc caveat-5 explanation is wrong (SwitchSig pool =
   3 kept + 1 WRONG + 1 sdk `__wpadCertWork`, not a null_sym).
7. **Archive integrity: VERIFIED repeatedly** — `run3-archive/vetted_identities.json`
   md5 `dbc440b6b2b67b964b208a7c17af625e`; run-3 matches.json archived
   (`045a0ae0…`) before run-4 overwrote the live copy.

## For the next agent (round-3 priorities)

1. **Rebuild ACCEPT seeds post-growth** before ANY future seeded run: fix
   `tools/ghidra/build_accept_seeds.py` to (a) derive holdout drops from the live
   158-entry holdout (85, not the hardcoded 73), (b) exclude
   `known_negatives.json` pairs. Verify `seeds ∩ holdout = ∅` at the file level.
2. **Stop investing in VT.** It is a CAUTION feeder, permanently. Do not re-run
   seed-purity variants; the lever is exhausted (T1 + T1-verify concur).
3. **System/network 30-pair judged sample** (same evidence-pack pipeline,
   `forensics/build_evidence_packs.py`) — closes the band3-extrapolation caveat
   gating the other 654 ingested identities.
4. **Cheap precision win:** add a sibling-aliasing vet check (immediate/literal
   diff on small near-identical same-TU bodies) — would have caught all 3 wrong
   pairs. Also: SwitchSig audit (n≥10), judge the `TrackWidget::Init` dup pair.
5. **Refresh the ingest from run-4's vetted tier** (2,246 ACCEPTs, +39) once seeds
   are rebuilt — re-run `ingest_ghidriff_accepts.py` (it reads run3-archive today;
   point it at a run-4 archive deliberately, never the mutable live dir). Give the
   14 Bink `None`-category entries a `lib` category.
6. **Upstream ghidriff PR series:** `--matches-only` (e52d935) + the O(n×m) dedup
   hash-join fix (84.8 min → seconds) on top of the round-1 commits.
7. Eval invocation discipline: **always pass the seeds file that seeded the run**
   (`--seeds seeds_accept_run3.json` for run-4 artifacts) — the default seeds.json
   silently inflates recall via the 12 leaked holdout entries.

## Commit ledger (round 2)

| Repo / branch | Commit | What |
|---|---|---|
| rb3 master | `9be35fbd` | S2 evidence substrate (30 packs, manifest, forensics) |
| ghidriff rb3-improvements | `e52d935` | `--matches-only` early-exit (+ dump_pdiff guard fix) |
| rb3 master | `e1693918` | runner knobs RB3_XENON_SEEDS/MATCHES_ONLY + build_accept_seeds.py |
| rb3 master | `0d3afded` | T4 session record + PLAN STATUS (status sections superseded) |
| rb3 master | `6a4779b2`, `6793c59a` | T2 ingest tool + doc |
| rb3-xenon main | `7bdae6c` | fn_resolver T4b tier + gitignore |
| rb3 master | `4daa00fa` | T3 holdout growth + eval/seed anti-leak plumbing + tests |
| rb3 master | `b1ced3c3` | T1 docs (twopass results + impl) |
| rb3 master | `6e9aaec5` | T4-verify audit + record/PLAN addenda |
| rb3 master | `fa3b3e82` | T1-verify doc |
| rb3 master | (this commit) | SYNTHESIS.md + remaining round-2 handoff docs + round-1 record §ROUND 2 |
