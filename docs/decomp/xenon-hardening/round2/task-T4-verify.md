# T4 VERIFICATION — round-2 session record + invariant audit (adversarial re-check)

**Verifier:** Fable (T4-verify). **Date:** 2026-06-11 (~10:30–10:40 UTC, while round-2 tasks were executing live).
**Subject:** implementer commit `0d3afded` ("docs(xenon-hardening): round-2 session record + T4 invariant audit + PLAN STATUS"), session record `docs/decomp/xenon-hardening-round2-2026-06-11.md`, PLAN STATUS appendix.

**VERDICT: PARTIAL.** Every *measured number* in the record verifies against on-disk evidence
(judge precision, strata, wrong pairs, round-1 citations, archive md5, commit existence). But the
record's central status narrative — "T1/T2/T3 did not execute this round; no new artifacts;
invariants (b)/(c) vacuously pass" — was **already false on disk at the moment of commit**
(10:27:14 UTC) and is now comprehensively refuted: T1's run-4 is in flight, T2 ingested + committed,
T3 grew holdout + committed. The record is an honest-but-raced stale snapshot. I re-ran invariants
(b)/(c) non-vacuously (both PASS) and found one real seeds↔holdout overlap (12 addrs) that turns
out to be a known, eval-neutralized artifact.

---

## 1. Timeline: why the record was stale at publication

All timestamps UTC, from `ls -la` mtimes and `git log --format='%h %ci'`:

| Time | Event |
|---|---|
| 10:16 | run3-archive/ created (planner-stage, per PLAN §"already done") |
| 10:23 | `rb3-xenon/ghidriff_identities.json` written (978 entries) + `tools/ghidra/ingest_ghidriff_accepts.py` on disk |
| 10:24–10:25 | `build_accept_seeds.py`, `seeds_accept_run3.json` (2,130), `.stats.json`, **grown** `holdout.json` (146→158), `known_negatives.json` (3), `reserved_seed_candidates_round2.json` (11) |
| 10:26:52 | ghidriff fork `e52d935` `--matches-only` (branch rb3-improvements) |
| **10:27:14** | **implementer commit `0d3afded`** — record claims none of the above exists |
| 10:27:16 | rb3 `e1693918` (RB3_XENON_SEEDS / RB3_XENON_MATCHES_ONLY knobs + builder) |
| 10:27:30 | **run-4 started** (T1): ghidriff `--seed-matches seeds_accept_run3.json --matches-only`, pid 474791 |
| 10:28:25 | rb3 `6a4779b2` (ingest tool) |
| 10:28:42 | rb3-xenon `7bdae6c` (fn_resolver T4b tier + .gitignore) |
| 10:29:03 | rb3 `6793c59a` (T2 doc SHAs) |
| 10:35:19 | rb3 `4daa00fa` (T3: grow_xenon_holdout.py + eval/builder anti-leak fixes + tests + task-T3-impl.md) |

So: `ghidriff_identities.json` (10:23) and the T3 seed files (10:25) predate the implementer's
commit by 2–4 minutes. The record's claims "No `seeds_accept_run3.json` was created", "No
`ghidriff_identities.json` exists in rb3-xenon ... not yet in .gitignore", "Holdout was not
extended; known_negatives file does not exist" (task-T4-impl.md "What was done") were **false on
disk when committed**. The implementer evidently probed earlier and did not re-check at commit
time. The task docs (task-T1-twopass.md, task-T1-impl.md, task-T2-ingest.md, task-T3-impl.md)
appeared 10:27–10:35, i.e. after the record — that one claim was true at commit time.

**Consequence:** §1 (task status), §2 invariants (b)/(c) "vacuous", §4 ("Not tested"), §6 commit
ledger, and the PLAN STATUS table ("T1/T2/T3 NOT RUN") in `0d3afded` are all superseded. Readers
must treat the record's *measurements* as valid and its *status narrative* as a stale snapshot.

---

## 2. What VERIFIES (probes + outputs)

### 2.1 Judge headline numbers — CONFIRMED exactly

`forensics/judge_verdicts.json` summary:
```
all:      correct 27, wrong 3, precision 0.900
bsim:     correct 19, wrong 2, precision 0.9048
non_bsim: correct  8, wrong 1, precision 0.8889
```
Per-pair recount: 27 correct / 3 wrong over n=30; wrong = pair_ids {13, 16, 29}. Matches the
record's §3 table and the headline (0.900 / 0.905 / 0.889).

### 2.2 Manifest strata — CONFIRMED exactly

`forensics/sample_manifest.json` (n=30): `{'BSIM>=30': 6, 'BSIM 20-30': 8, 'BSIM 15-20': 7,
'ExactInstr': 5, 'SwitchSig': 3, 'Implied': 1}` — identical to the record's per-stratum table
(6/8/7/5/3/1). BSIM = pair IDs 01–21 exactly, as the record's "For the verifier" §3 states.
Wrong pairs sit in the claimed strata with the claimed xenon addrs:
- pair-13: BSIM 20-30, `0x82518de0`, `clear__..._List_base<pair<Symbol,Symbol>...>`
- pair-16: BSIM 15-20, `0x824e51e0`, `__ct<PCc,i>__...pair<C6Symbol,8DataNode>...`
- pair-29: SwitchSig, `0x8233afb0`, `ActiveScoreType__12MusicLibraryCFv`

### 2.3 Wrong-pair reasoning spot-checks — CORROBORATED

- pair-16: `src/system/obj/Data.h:23` `kDataFloat = 1`, `:28` `kDataInt = 6` — the float/int
  immediate-tag reasoning is sound.
- pair-29: the refuting string exists at `src/system/bandobj/BandTrack.cpp:544`
  (`"unrecognized instrument type \"%d\""`). Nit: the record cites "BandTrack.cpp:510" and the
  band3 path; the actual anchor is system/bandobj line 544. Immaterial to the verdict.
- All 3 wrong pairs landed in `xenon-seeds/known_negatives.json` with matching
  `(xenon_addr, wii_addr_bank8)` pairs and `source: judged-round2-wrong`.

### 2.4 Round-1 citations — CONFIRMED

`docs/decomp/xenon-precision-hardening-2026-06-10.md`: 8,527 matches (line 261), recall 63.8%
(90/141, line 262), precision-on-recovered 0.833 (line 263), ACCEPT 2,207 (line 265),
"≥15 → 0.933" (line 271), VT "0.109 raw / 0.236 alias-credited" (line 273), and the n=45 the
implementer cites: line 299 "Holdout n is modest (45 kept at ≥15)".

### 2.5 PLAN.md "append, don't rewrite" — CONSISTENT

PLAN.md was previously untracked (git log shows only `0d3afded` touching it), so the 217-line
"new file" in the commit is the planner's body + the STATUS appendix at lines 191+. Planner
content reads intact; cannot be byte-proven (no prior committed version) but structure and
content match the planner-stage claims it makes.

---

## 3. Invariant re-audit (non-vacuous, current disk state)

### (a) seeds files ∩ holdout — seeds.json PASS; seeds_accept_run3.json: 12-addr overlap (accounted)

```
holdout.json: 158 entries (146 original no-source + 12 source=judged-round2-correct)
seeds.json (1,213)            ∩ holdout(158) = 0   PASS
seeds_accept_run3.json (2,130) ∩ holdout(158) = 12  ← all 12 are the NEW grown entries:
  8252c728 82532198 825a8520 825c1698 82617830 82670e70
  826798b0 82679b40 8268d410 826966f0 8269ea00 82b7e618
reserved(11) ∩ holdout(158) = 0
```
**Why:** `seeds_accept_run3.json` was built against the ORIGINAL 146-entry holdout
(`seeds_accept_run3.stats.json`: `holdout_addrs: 146`, `holdout_drops: 73`,
`kept_after_holdout_drop: 2134`, strict-1:1 −4 → 2,130). T3 then grew holdout to 158 by adding 12
judged-correct pairs — which are ACCEPT-tier entries and hence already inside the accept seeds.
This is the exact T1↔T3 sequencing race PLAN.md warned about ("T3 must wait for T1 ... rewrites
holdout.json (T1 reads it for anti-leak filtering)").

**Severity: LOW / neutralized.** `tools/ghidra/eval_xenon_matches.py:443`
(`eligible_holdout = {a for a in holdout_by_addr if a not in seed_p2}`) excludes seeded addrs from
holdout eligibility, so for run-4 eval the 12 grown entries are dropped, leaving the effective
holdout = the original 146 — i.e. recall stays directly comparable to run-3, **provided eval is
invoked with `--seeds seeds_accept_run3.json`** (the T1 doc commits to this; eval's default
`--seeds seeds.json` would instead count the 12 as trivially-recovered and inflate recall). T3's
own commit message (`4daa00fa`) accounts for it: "excluded_as_seeds 0->85 = 73 crossval + 12
grown". NOT a silent leak; flagged here so round 3 regenerates accept-seeds against the grown
holdout before any future seeded run.

### (b) reserved_seed_candidates_round2.json ∩ holdout = ∅ — PASS (real, not vacuous)

11 reserved entries, 0 intersect the 158-entry holdout. Accounting closes perfectly:
**27 judged-correct = 4 already in original holdout (82586258, 826d99b0, 826dbaa8, 82b7d078)
+ 12 → grown holdout + 11 → reserved.** (The 4 explain why 27 ≠ 12+11.)

### (c) rb3-xenon/ghidriff_identities.json — PASS (real, not vacuous)

978 entries. Probes:
- categories: `{system: 438, band3: 306, network: 216, None: 14, main: 4}` — **0 sdk** ✓
  (T2 doc: 9 sdk entries skipped at ingest; band3 306 = 309 − 3 judged-wrong ✓)
- judged-WRONG addrs (`0x82518de0/0x824e51e0/0x8233afb0`) vs the file's `rb3_addr` field: **0** ✓
  (note: the addr field is `rb3_addr`, not `xenon_addr` — a naive grep for xenon_addr passes
  vacuously; I checked the real key)
- `wii_addr_bank8` present on **all 978** ✓ (Bank-5 confusion guard held)
- gitignored: `rb3-xenon/.gitignore:55` `/ghidriff_identities.json` ✓; commit `7bdae6c` touches
  only `.gitignore` + `tools/fn_resolver.py` (json NOT committed) ✓
- The 14 None-category entries are Bink/RAD middleware (`CheckReadHuff4PairBundle`,
  `BinkShouldSkip`, `cftf161`, …) with null TU — third-party lib, not sdk. Acceptable; minor
  round-3 nicety: give them a `lib` category.

### (d) run3-archive intact — PASS

```
dbc440b6b2b67b964b208a7c17af625e  run3-archive/vetted_identities.json   (claimed md5 ✓)
dbc440b6b2b67b964b208a7c17af625e  vetted_identities.json (live, identical)
045a0ae0b6f1a9f87d717ab07b668928  run3-archive/json/...matches.json
045a0ae0b6f1a9f87d717ab07b668928  json/...matches.json (live == archive at probe time;
                                   run-4 will overwrite the live copy when matching completes —
                                   archive is the canonical run-3 copy)
```

### (e) commits exist on claimed branches — PASS

- rb3 master: round-1 SHAs all present (`53f7a6aa 4c9541e0 2e8a82f7 da52aac0 d17d5e55 d53240b8`);
  implementer `0d3afded` present; round-2 concurrent `e1693918 6a4779b2 6793c59a 4daa00fa` present.
- ghidriff fork branch `rb3-improvements`: `5b9cc4e`, `31a6f6c` present + new `e52d935`
  (`--matches-only`).
- rb3-xenon main: `7bdae6c` present.

---

## 4. Two-pass run (T1) — live verification of the brief's specific concerns

- **run-3 artifacts survived**: §3(d) above — archive md5s confirmed before run-4 overwrites live.
- **Genuinely ACCEPT-only seeds**: run-4 log (`/tmp/claude/t1_run_stdout.log`):
  `Loaded 2130 seed match pairs from .../seeds_accept_run3.json`,
  `SeedMatch: pre-accepted 2130 seed pairs, skipped 0 unresolvable`. The ghidriff "Manual Match"
  set shows 2,159 (+29 over the file) — consistent with run-3 behavior (1,213 seeds → 1,254/1,276
  Manual Match in the archived log; ghidriff expands thunk-equivalent pairs). The ps cmdline
  carries `--seed-matches .../seeds_accept_run3.json ... --matches-only`. Started 10:27:30;
  VTCombinedReference completed in 41.4s ("seeded session with 6353 accepted matches"); matching
  was in the Implied stage at ~10:38 — well inside the 45-min kill window.
- **Same eval flags as baseline**: task-T1-twopass.md commits to
  `--seeds seeds_accept_run3.json --credit-platform-alias --stratify` (and warns the default
  seeds.json corrupts the seed-exclusion math). Results were NOT yet on disk at verification time
  — the VT before/after verdict remains pending T1's doc fill-in.

---

## 5. Assessment of the implementer's specific StructuredOutput claims

| Claim | Verdict |
|---|---|
| Headline 0.905 BSim / 0.900 overall (n=30 band3) | **CONFIRMED** (judge_verdicts.json recount) |
| T2 gate (≥0.85) passes | **CONFIRMED** (and T2 in fact ran: 978 ingested) |
| "T1/T2/T3 did not execute (no artifacts)" | **REFUTED** — artifacts predate the record's commit by 2–4 min; all three tasks executed/committed within the following 8 min |
| Invariant (a) seeds∩holdout = 0 | **PASS for seeds.json**; accept-seeds have the 12-grown-entry overlap (accounted, eval-neutralized — §3a) |
| Invariants (b)/(c) "vacuously pass" | **Superseded** — now real checks; both **PASS** |
| (d) archive md5 dbc440b6 | **CONFIRMED** |
| (e) commits on claimed branches | **CONFIRMED** |
| Wrong pairs 13/16/29 + reasoning | **CONFIRMED + corroborated** (Data.h enum; BandTrack.cpp:544 string) |
| "0.905 validates 0.933 calibration; BinDiff oracle pessimistic" | **SUPPORTED** (0.905 n=21 vs 0.933 n=45 — consistent within CI; round-1 line 284's "both can't be right" resolved in holdout's favor by human judging) |
| Band3-only extrapolation caveat | **VALID and clearly stated** (record §5) |
| Commit `rb3:0d3afded` | **CONFIRMED on master** |

---

## For the next agent (round-3 planner)

1. **The session record needs a post-run amendment**: §1/§2(b,c)/§4/§6/PLAN-STATUS describe a
   world where T1/T2/T3 didn't run. They did. Fold in: run-4 results (T1 doc), T2's 978-entry
   ingest (`6a4779b2`, rb3-xenon `7bdae6c`), T3's holdout 146→158 + known_negatives +
   reserved-11 (`4daa00fa`). A "Verifier addendum" pointer was appended to the record and to
   PLAN STATUS by this task.
2. **Regenerate accept-seeds against the grown 158-entry holdout** before ANY future seeded run
   (the current `seeds_accept_run3.json` contains the 12 grown-holdout addrs). With T3's
   `build_xenon_seeds.py`/`grow_xenon_holdout.py` fixes landed, this should be one command —
   verify `excluded_as_seeds` drops back to the crossval-only count afterward.
3. **Always pass `--seeds seeds_accept_run3.json` to eval for run-4 numbers** — default seeds
   silently inflates recall via the 12 leaked entries (eval_xenon_matches.py:443 is the guard).
4. Pending verifications when T1 finishes: matches.json fresh mtime + stale full-json/md (the
   `--matches-only` contract), VT before/after at the same flags, the 45-min kill not tripped.
5. Minor: 14 None-category Bink entries in the ingest (consider `lib` category); pair-29's
   record anchor should read `src/system/bandobj/BandTrack.cpp:544`.

## Probe inventory (for reproducibility)

All probes read-only; key ones:
```bash
# (a)/(b) intersections + accounting
python3 - <<'EOF'   # see outputs quoted in §3; joins on bare-hex lowercase addrs
import json; base='build/SZBE69_B8/ghidra/xenon-seeds/'
hold=json.load(open(base+'holdout.json'))['entries']  # 158 = 146 + 12 judged-round2-correct
EOF
# (c)
python3 -c "import json; gi=json.load(open('/home/free/code/milohax/rb3-xenon/ghidriff_identities.json')); ..."
# (d)
md5sum build/SZBE69_B8/ghidra/ghidriff-xenon/{run3-archive/,}vetted_identities.json \
       build/SZBE69_B8/ghidra/ghidriff-xenon/{run3-archive/,}json/*matches.json
# (e)
git log --oneline --all | grep -E "53f7a6aa|4c9541e0|2e8a82f7|da52aac0|d17d5e55|d53240b8"
git -C /home/free/code/milohax/ghidriff log --oneline -8 rb3-improvements
git -C /home/free/code/milohax/rb3-xenon log --oneline -5
# run-4 liveness
ps aux | grep ghidriff; grep "pre-accepted" /tmp/claude/t1_run_stdout.log
```
