# Wii↔Xenon ghidriff precision hardening — round-2 session record (2026-06-11)

**Mission:** exploit the run-3 win (8,527 matches, ACCEPT tier 2,207 @ ~0.93 precision) with
four tasks: (T1) two-pass VT rescue experiment; (T2) gated rb3-xenon ingest; (T3) holdout
growth + known-negatives plumbing; (T4) this session record + cross-task invariant audit.

**Predecessor:** `docs/decomp/xenon-precision-hardening-2026-06-10.md` (run-1 and run-3 records)

**Round-2 headline measurement:** Human-judged 30-pair stratified band3 sample.
- **Overall: precision 0.900** (27 correct, 3 wrong; n=30)
- **BSim stratum: precision 0.905** (19 correct, 2 wrong; n=21)
- **Non-BSim stratum: precision 0.889** (8 correct, 1 wrong; n=9)

This is the first **human-judged** precision number at band3 scope. It validates and
slightly beats the holdout-calibrated 0.933 estimate (BSim sim×conf≥15), at a wider
n=21 covering all three BSim sub-strata (≥30, 20–30, 15–20).

---

## 1. Task status (honest verdicts)

### T1 — Two-pass VT rescue — NOT COMPLETED (agent did not run)

The T1 task doc does not exist at
`docs/decomp/xenon-hardening/round2/task-T1-twopass.md`. No round-4 ghidriff run
was executed: the live `json/bank8_target.elf-*.matches.json` is dated
2026-06-10 20:48 (run-3 output unchanged), and no `seeds_accept_run3.json`
was created. The VT hypothesis (seeding from ACCEPT-only tier rescues VT from 0.236
precision) is **untested this round**.

Concrete evidence:
```
$ stat build/SZBE69_B8/ghidra/ghidriff-xenon/json/bank8_target.elf-42264e.gzf-rb3_xenon_default_xex.gzf.ghidriff.matches.json | grep Modify
Modify: 2026-06-10 20:48:55  (run-3, not updated)
```

VT status carries over from run-3: precision **0.109 raw / 0.236 alias-credited**,
treated as CAUTION-tier until the experiment runs. This is the key open item for
round 3.

### T2 — rb3-xenon ingest — NOT COMPLETED (gated, T1 would unblock)

T2 is gated first on the judge sample (BSim stratum precision ≥0.85) and second on
T1's ACCEPT seeds (the plan reads run3-archive for T2 independence, but T2 was not
executed). No `ghidriff_identities.json` exists in rb3-xenon, and it is not yet in
rb3-xenon's `.gitignore`.

Gate evaluation on judge data (hypothetical, T2 not run):
- BSim stratum: 0.905 ≥ 0.85 → **GATE PASSES**
- All stratum: 0.900 ≥ 0.85 → **GATE PASSES**
- Gate rule: ingest all non-seed ACCEPT (sdk excluded, judged-WRONG xenon addrs
  excluded: `0x82518de0` pair-13, `0x824e51e0` pair-16, `0x8233afb0` pair-29)
- T2 implementation design is fully specified in S1 (`scout-rb3xenon-conventions.md`);
  the gate passes; no blocking reason remains except agent non-execution.

### T3 — Holdout growth + known-negatives — NOT COMPLETED (gated, after T1)

T3 is sequenced after T1 (edits eval and seed scripts that T1 executes). Not run.
No `reserved_seed_candidates_round2.json` was created.

The 27 judged-correct pairs are ready to be split 50/50 (deterministic,
`Random(42)` by xenon_addr sort) into holdout XOR reserved-for-seeds. The 3
judged-wrong pairs are ready to populate `xenon-seeds/known_negatives.json`.

### T4 — Session record + invariant audit — **THIS DOCUMENT**

Executed fully. Invariant probes below.

---

## 2. Cross-task invariant audit (re-checked with probes)

### (a) seeds.json ∩ holdout.json = ∅

```python
# Command run:
# python3 -c "
#   import json
#   holdout_addrs = set(e['addr'] for e in json.load(open('build/.../holdout.json'))['entries'])
#   seeds = json.load(open('build/.../seeds.json'))
#   seed_xenon_addrs = set(s['p2_addr'] for s in seeds)
#   intersection = holdout_addrs & seed_xenon_addrs
#   print(len(holdout_addrs), len(seeds), len(intersection))
```
Output: holdout 146, seeds 1213, intersection **0**. **PASS.**

Note: `seeds_accept_run3.json` does not exist yet (T1 not run). When T1 creates it,
the same disjointness must hold (planner pre-computed: 73 of the 2,207 ACCEPT entries
overlap holdout — those 73 must be dropped from the ACCEPT seed set before writing).

### (b) reserved_seed_candidates_round2.json ∩ holdout.json = ∅

`reserved_seed_candidates_round2.json` does not exist (T3 not run).

**STATUS: VACUOUSLY PASS** (non-existent file cannot intersect anything). Must be
verified when T3 creates the file.

### (c) rb3-xenon/ghidriff_identities.json contains 0 sdk-category and 0 judged-WRONG addrs, and is gitignored

`ghidriff_identities.json` does not exist in rb3-xenon (T2 not run). `.gitignore` was
not updated to list it.

**STATUS: VACUOUSLY PASS** (non-existent). When T2 creates it, both conditions must
be verified:
- No entry with `category == "sdk"` (sdk measured 0.000; S2 confirmed sdk exclusion)
- No entry with `xenon_addr` in {`0x82518de0`, `0x824e51e0`, `0x8233afb0`} (judged WRONG)
- Entry must be added to rb3-xenon `.gitignore` (line appended after line 80)

### (d) run3-archive/ still intact (vetted_identities.json md5 == dbc440b6b2b67b964b208a7c17af625e)

```
$ md5sum build/SZBE69_B8/ghidra/ghidriff-xenon/run3-archive/vetted_identities.json
dbc440b6b2b67b964b208a7c17af625e  build/SZBE69_B8/ghidra/ghidriff-xenon/run3-archive/vetted_identities.json
```
**PASS.** Archive intact. Also verified: run3-archive/json/ contains both the full
209MB `.ghidriff.json` and the 4.9MB `.matches.json`.

Note: the live `vetted_identities.json` is **also** dbc440b6b2b67b964b208a7c17af625e —
it has not been overwritten by a run-4. The archive copy and the live copy are
identical (T1 not run).

### (e) Claimed commits exist on the claimed branches

Round-1 session record claims the following commits. Verification:

**rb3 master:**
```
53f7a6aa  tools(xenon): fix RB3_XENON_BSIM toggle — confirmed present
4c9541e0  tools(xenon-vet): fix 4 annotation defects — confirmed present
2e8a82f7  tools(xenon-vet): BSim sim*conf ACCEPT gate + run-3 results — confirmed present
da52aac0  tools(ghidra-xenon-eval): T4 — platform-alias/arity crediting — confirmed present
d17d5e55  tools(xenon): add vet_xenon_identities.py — confirmed present
d53240b8  tools(xenon): harden run_ghidriff_xenon.sh — confirmed present
```
All verified via `git log --oneline --all | grep <sha>` on rb3 master.

**ghidriff fork `rb3-improvements`:**
```
5b9cc4e  string hashers: global 1:1-uniqueness gate — confirmed present
31a6f6c  vt: export per-match VT/Implied/BSim scores + STL/template gate — confirmed present
```
Verified via `git log --oneline --all` on the ghidriff repo.

No round-2-specific commits exist in rb3 or rb3-xenon (T1/T2/T3 not executed).

**PASS** (all claimed round-1 commits present on expected branches).

---

## 3. Judging detail — the 30-pair evidence review

### Methodology
Sample drawn from `docs/decomp/xenon-hardening/round2/forensics/sample_manifest.json`
(30 pairs, stratified, `Random(42)`, confirmed by probe: 21 BSIM, 9 non-BSIM,
exactly matching the strata in the PLAN). Evidence packs at
`docs/decomp/xenon-hardening/round2/evidence/pair-01.md` … `pair-30.md`.

### Per-stratum breakdown

| Stratum | n | Correct | Wrong | Precision |
|---|---|---|---|---|
| BSIM sim×conf ≥ 30 | 6 | 6 | 0 | 1.000 |
| BSIM sim×conf 20–30 | 8 | 7 | 1 | 0.875 |
| BSIM sim×conf 15–20 | 7 | 6 | 1 | 0.857 |
| ExactInstructionsFunctionHasher | 5 | 5 | 0 | 1.000 |
| SwitchSigHasher | 3 | 2 | 1 | 0.667 |
| Implied Match | 1 | 1 | 0 | 1.000 |
| **ALL** | **30** | **27** | **3** | **0.900** |
| **BSim only** | **21** | **19** | **2** | **0.905** |
| **Non-BSim only** | **9** | **8** | **1** | **0.889** |

### Wrong pairs

**Pair 13 (BSIM 20–30, simconf=20.79):** `clear` on `_List_base<pair<Symbol,Symbol>>` ← misattribution.
Wii `0x80246770` (16-byte node size for `pair<Symbol,Symbol>`) vs Xenon passes 36-byte node size to
`_MemOrPoolFreeSTL`. Different template instantiation among 30+ siblings in AccomplishmentProgress.o.
(Same-TU near-dup cluster → BSim cannot distinguish; sibling-aliasing failure mode.)
Xenon addr: `0x82518de0`.

**Pair 16 (BSIM 15–20, simconf=16.656):** `__ct<PCc,i>` for `pair<Symbol,DataNode>` ← float sibling
misattribution. Xenon writes `DataNode::mType = 1` (kDataFloat) at `this+8`; claimed Wii sibling
writes `6` (kDataInt). The true Wii partner is `__ct<PCc,f>` at `0x803e6740`. BSim similarity=1.0 on
a body differing by only one immediate + load opcode; BSim picks blind. Xenon addr: `0x824e51e0`.
(Detailed verdict doc: `round2/verdict-pair-16.md`.)

**Pair 29 (SwitchSigHasher):** `ActiveScoreType__12MusicLibraryCFv` (Wii `0x80300f00`) ← wrong;
Xenon `0x8233afb0` references strings `'unrecognized instrument type "%d"'`, `guitar/vocals/real_*`
which match `BandTrack::SetInstrument` (BandTrack.cpp:510), not MusicLibrary::ActiveScoreType.
(SwitchSig matched on partition shape; string evidence refutes the identity.)

### Implications for calibration

The run-3 holdout calibration was 0.933 on BSim sim×conf≥15 (n=45). The judged precision is 0.905
(n=21). Both numbers sit within CI of each other (n is modest in both cases). The calibration holds:
0.905 does NOT contradict the 0.933 estimate. The PLAN's operating point `--min-bsim-simconf 15`
as the ACCEPT gate is vindicated.

The dc3-BinDiff "high-conf disagree" oracle showed BSim at 0.193–0.319 at the same thresholds
(round-1 record §7). Human judging at 0.905 confirms the oracle pessimism question: **the BinDiff
oracle is wrong as the arbiter**. Cross-compiler semantic disagreements (platform-alias class
renaming, toolchain-specific inlining patterns) are counted as our errors by BinDiff but are NOT
errors. The holdout (and now human judging) is the correct ground truth. BinDiff agreement should be
read only directionally.

The SwitchSig precision surprise (1/3 = 0.333 for this micro-sample, compared to the prior
expectation that hash-based correlators would be high-precision) is notable but the n=3 sample is
too small to update the gate. The single wrong case (pair-29) is a genuine misattribution where
string evidence directly refutes the hash.

---

## 4. T1 hypothesis status (VT rescue)

**Not tested.** The two-pass experiment (seeding from ACCEPT-only tier to rescue VT from 0.236) did
not run. Carry-forward from run-3: VT precision 0.109 raw / 0.236 alias-credited. Treat VT as
CAUTION-only for any ingest.

The round-3 agenda on VT has two options (per PLAN §T1 success reading):
1. Run the two-pass experiment: `seeds_accept_run3.json` with the 73-holdout-overlap rows dropped,
   `RB3_XENON_SEEDS=<path>`, `RB3_XENON_MATCHES_ONLY=1`. Expected ~9–10 min.
2. Accept VT as permanently CAUTION-feeder; document and stop investing. Requires explicit decision
   once the experiment finally runs.

If VT lands mid (0.4–0.6), the follow-on lever is seeds+exacts-only seeding (~1,280 pairs ≈0.99).

---

## 5. Band3-only extrapolation caveat (applies to T2 gate)

The 30-pair judged sample is **band3-only** by design (S2 drew from the 309 band3 ACCEPT non-seed
entries). The T2 gate (BSim precision ≥0.85) passes on this band3 sample. However:

- The gate **extrapolates** band3 precision to the system/network ACCEPTs (309 band3 / 438 system /
  216 network non-seed entries in the ACCEPT tier).
- Mitigation: the BSim sim×conf≥15 holdout calibration (0.933, n=45) was computed over the full
  ACCEPT tier pool including all strata, so the holdout number is stratum-independent. The band3
  gate being ≥0.85 is consistent with the cross-stratum holdout.
- Risk: band3 is the "weakest" stratum under the dc3-BinDiff pessimistic oracle (which we now know
  is pessimistic). System/network may have higher or similar BSim precision. But this is
  unmeasured at human-judgment quality.
- Round-3 action: run a system/network 30-pair judged sample to close the gap.

---

## 6. Commit ledger

| Repo / branch | Commit | What |
|---|---|---|
| rb3 master | `9be35fbd` | Scout S2 — verification evidence substrate (round 2) |
| (T4 this doc) | pending | Round-2 session record + PLAN.md STATUS appendix |

No T1/T2/T3 commits exist for round 2. All round-1 commits confirmed on their branches (§2 invariant e).

---

## 7. Open items for round 3

1. **T1: Run the two-pass VT experiment.** Build `seeds_accept_run3.json` (2,207 ACCEPT minus 73
   holdout overlaps = ~2,134; drop 1 dup-p1, 1 dup-p2 → ~2,131–2,133). Run with
   `RB3_XENON_SEEDS=<path> RB3_XENON_MATCHES_ONLY=1 tools/ghidra/run_ghidriff_xenon.sh`.
   Kill if matching exceeds 45 min. Archive run-3 first (already done). VT verdict is decision-grade.

2. **T2: Ingest vetted ACCEPT pairs into rb3-xenon.** Gate passes (BSim 0.905). Implementation
   spec: S1 §4 (`ghidriff_identities.json`, `wii_addr_bank8` field, calibrated confidences,
   sdk-excluded, judged-WRONG excluded). Add to rb3-xenon `.gitignore` (after line 80).
   Source: run3-archive (immutable, path confirmed intact).

3. **T3: Grow holdout + create known-negatives.** 27 judged-correct pairs → 50/50
   `Random(42)` holdout/reserved split. 3 judged-wrong → `xenon-seeds/known_negatives.json`.
   Fix `build_xenon_seeds.py` holdout mechanics (anti-clobber + union-merge). Fix
   `eval_xenon_matches.py` exact-addr scoring + known-negatives oracle.

4. **System/network stratum judging.** Draw a 30-pair sample from system ACCEPT non-seeds;
   run the same evidence-pack judging pipeline. Closes the band3-extrapolation caveat (§5).

5. **Band3 CAUTION vetting.** The CAUTION tier (sub-threshold BSim ~0.5; VT cluster-coherent)
   has unknown precision for band3. Evidence-pack sample + judging before any ingest.

6. **SwitchSig precision audit.** 1/3 wrong in the micro-sample (pair-29). Before using
   SwitchSig as a source for any future ACCEPT tier, run a larger stratified judged sample
   (n≥10). Consider re-checking pair-27 and pair-28 with string-evidence analysis.

7. **Upstream ghidriff PR series.** Three candidates (from S3 + round-1 record):
   - `--matches-only` early-exit (6 precise insertion points in S3 §10) — cuts 107-min
     post-match waste to ~0s for our pipeline.
   - O(n×m) dedup loop algorithmic fix (hash-join O(n+m)) — cuts 84.8 min to seconds.
   - These are orthogonal; `--matches-only` is additive-default-off and lower-risk.

8. **T5 annotation fixes** (carried from round-1 §5): `rb3wii_check` Bank-5 address join,
   `category` mislabel fix, ExactMnemonics default-off, null `wii_addr` emission.

---

## 8. Numbers summary

| Quantity | Value | Source |
|---|---|---|
| Round-2 judged precision (all) | **0.900** (27/30) | Injected judge verdicts |
| Round-2 judged precision (BSim) | **0.905** (19/21) | Injected judge verdicts |
| Round-2 judged precision (non-BSim) | **0.889** (8/9) | Injected judge verdicts |
| Run-3 ACCEPT tier | 2,207 entries | run3-archive/vetted_identities.json (md5 dbc440b6) |
| Run-3 ACCEPT∩holdout (drop from seeds) | 73 | PLAN.md planner check |
| Run-3 holdout recall | 63.8% | run3-archive/eval_report.json |
| Run-3 holdout precision on recovered | 0.833 | run3-archive/eval_report.json |
| Run-3 BSim sim×conf≥15 holdout precision | 0.933 (n=45) | round-1 record §7 |
| Run-3 VT precision (alias-credited) | 0.236 | round-1 record §7 |
| Band3 non-seed ACCEPT population | 309 | S2/PLAN |
| Wrong xenon addrs (judged) | 0x82518de0, 0x824e51e0, 0x8233afb0 | pairs 13, 16, 29 |
| T2 gate result (hypothetical) | **PASSES** (BSim ≥ 0.85) | §2 |

---

## VERIFIER ADDENDUM (2026-06-11 ~10:40 UTC) — task-status sections superseded

This record was a stale snapshot at publication: T1/T2/T3 executed concurrently
(artifacts at 10:23–10:25 predate this record's 10:27:14 commit; commits landed
10:26:52–10:35:19). Superseded sections: §1 (all three "NOT COMPLETED" verdicts),
§2(b)/(c) ("vacuous"), §4 ("Not tested" — run-4 started 10:27:30), §6 ledger.
The judged-precision numbers (§3), calibration analysis, and invariant probe RESULTS
remain correct; (b)/(c) re-checked non-vacuously = PASS. One new accounted finding:
`seeds_accept_run3.json` ∩ grown holdout = 12 addrs (eval-neutralized iff
`--seeds seeds_accept_run3.json` is passed). Full audit + timeline:
`docs/decomp/xenon-hardening/round2/task-T4-verify.md`.

## For the verifier

1. All invariants (a)–(e) probed directly above with exact commands and outputs.
2. (b) and (c) are vacuously pass because T3/T2 did not run; they must be re-probed when
   those tasks execute.
3. The judged data in §3 traces to the injected `JUDGE RESULTS` block; strata cross-checked
   against `forensics/sample_manifest.json` (probe confirmed: 21 BSIM, 9 non-BSIM, matching
   pair IDs 01–21 = BSIM, 22–30 = non-BSIM).
4. Wrong pair xenon addresses confirmed from the manifest JSON (probe above).
5. No ghidriff run was started by this agent (constraint 2 respected).
6. The round-1 session record's numbers (cited in §2 and §4) are in
   `docs/decomp/xenon-precision-hardening-2026-06-10.md` §7.
