# T3 VERIFICATION — holdout growth + known-negatives + anti-leak plumbing

Verifier: Fable (adversarial). Date: 2026-06-11. Verdict: **CONFIRMED** (with two
cross-task follow-ups that are downstream of T3, not defects in it).

Implementer claims doc: `docs/decomp/xenon-hardening/round2/task-T3-impl.md`
Commit verified: rb3 `4daa00fa` (on `master`, the default branch — consistent with
all prior round-1/round-2 commits; 6 files, matches the claimed file list exactly:
`grow_xenon_holdout.py` NEW, `build_xenon_seeds.py` M, `eval_xenon_matches.py` M,
`test_eval_xenon_matches.py` M, `forensics/judge_verdicts.json` NEW, impl doc NEW).

## What I re-ran / re-derived (all offline, all reproduced)

| Claim | Check | Result |
|---|---|---|
| pytest 44/44 | `python3 tools/ghidra/test_eval_xenon_matches.py` | **OK, 44 tests** |
| XOR split membership | **Independent re-derivation** from `forensics/judge_verdicts.json` + `forensics/structured_pairs.json` (sort by int(xenon_addr,16), `Random(42).shuffle`, ceil(23/2)) | EXACT match: holdout {02,03,05,08,12,14,21,22,24,27,28,30}, reserved {01,06,07,09,10,11,15,17,18,19,20}, already-in-146 {04,23,25,26}, wrong {13,16,29} |
| Counts | live `xenon-seeds/` files | holdout **158** (146+12, grown at indices 146–157), reserved **11**, known-neg **3** |
| Idempotency | md5 of all 3 outputs before/after a real re-run of `grow_xenon_holdout.py` | identical; holdout md5 `d6e9b91a359dfcd5711db29a3b532b76` = doc's claim |
| Original 146 preserved | fresh `build_xenon_seeds.py --out-dir /tmp/...` (writes crossval-only 146) vs live `entries[:146]` | **equal** (same order, same objects) |
| Anti-leak (temp build) | temp seeds (1213) ∩ holdout(158) | **0**; reserved ∩ holdout = 0; known-neg ∩ holdout = 0; known-neg ∩ reserved = 0 |
| Union-merge survives seed rebuild | copied grown holdout into temp out-dir, re-ran `build_xenon_seeds.py` | holdout stays **158**, all 12 grown preserved (`holdout_grown_preserved: 12` in stats), first-146 unchanged, seeds∩holdout=0 |
| Eval no-regression | new eval code + `run3-archive` matches + 146-only holdout + `--no-known-negatives --credit-platform-alias --stratify` vs `run3-archive/eval_report.json` (sans `inputs`) | **byte-identical: True** (I ran the replay myself) |
| Exact-addr scoring | archived run-3 matches + grown holdout + run-3 `seeds.json` | **12/12** grown rows `score_mode:"exact"`, all `recovered_correct`, matched `wii_addr` == `wii_addr_bank8` 12/12 |
| Doc table row 1 | same eval | eligible 158, excluded_as_seeds 0, **102/18, precision 0.850** — exact match |
| Known-negatives recur | same eval | `recurred_exact: 3` of 3; the 3 listed pairs = pairs 13/16/29 (verified against `verdict-pair-16.md` etc.) |
| KN semantics | code review (`kn_by_p2` index, both endpoints must match) + tests `TestKnownNegatives` | exact-(p2,p1) flagged WRONG; same-p2/different-p1 not penalized |
| Seed-exclusion fires (leak nuance) | archived matches + **real** `seeds_accept_run3.json` | excluded_as_seeds **12** (the grown set), exact-mode rows remaining **0**, recovered 90/18 |
| run-3 artifacts intact | md5 | `run3-archive/vetted_identities.json` = `dbc440b6…` (T4's audit value); archived matches `045a0ae0…` ≠ live `b5382b7d…`; `seeds.json` md5 `94c645cf…` untouched (mtime Jun 10 18:31) |
| Verdict JSON vs in-repo verdict docs | pairs 16 (wrong, float/int sibling), 25/26 (correct) | consistent; summary 27/30=0.900, BSim 19/21=0.905, non-BSim 8/9 — matches PLAN STATUS |

## The timeline twist the impl doc could not know

T3's "T1 had NOT run" was true when T3 checked but **stale within minutes**. The
interleaving (commit/mtime evidence):

- 10:25 `seeds_accept_run3.json` written by T1's `build_accept_seeds.py` (commit
  `e1693918`) — built from the **pre-growth 146** holdout (drops exactly the 73).
- 10:27–10:35 T1's two-pass ghidriff run: log shows `Loaded 2130 seed match pairs
  from …seeds_accept_run3.json`, `SeedMatch: pre-accepted 2130, skipped 0`,
  `--matches-only: skipping post-match diff/report stage`. Live `matches.json`
  overwritten 10:35.
- 10:33 T3's grow wrote the grown holdout/known-negs/reserved.
- 10:35 T3 committed `4daa00fa`.
- 10:38 T1's eval wrote the live `eval_report.json` — its `inputs` confirm it
  consumed **T3's grown holdout + known_negatives.json** with
  `seeds_accept_run3.json`: excluded_as_seeds **12** (exactly the grown entries),
  eligible 146, recovered 85/18 = **0.825**, `recurred_exact: 0` (see below).

So T3's plumbing was consumed correctly by T1's real eval, and the grown-holdout
exclusion fired exactly as designed. T3's proxy table row 2 (synthetic 2203 seeds,
excluded 85 = 73+12) is mechanism-consistent with the real run (excluded 12,
because the real ACCEPT seed file already dropped the 73 crossval entries at build
time). The proxy numbers are superseded by the live eval and should not be quoted.

## Findings (protocol-invariant deviations — downstream of T3, must be fixed round-3)

1. **`seeds_accept_run3.json` ∩ grown-holdout = 12, NOT ∅** (the protocol
   invariant as literally written fails on disk). Root cause: T1 built it at 10:25
   from the 146 holdout, before T3's growth existed. Mitigations VERIFIED: (a) the
   eval excludes seed-p2 from eligibility — the 12 are `excluded_as_seeds` in both
   my replay and T1's live report, so no metric is contaminated; (b) any future
   `build_xenon_seeds.py` run excludes them (`--extra-holdout`, proven ∅ above).
   This is exactly the "leak nuance" the task spec itself anticipated.
2. **The 3 known-negative (judged-WRONG) pairs are seeded as GIVENS in
   `seeds_accept_run3.json`** — exact (p1,p2) present for all 3 (verified). T1's
   two-pass run was therefore seeded with 3 human-confirmed-wrong identities
   (3/2130 = 0.14%; negligible for the VT read, but wrong in principle). This also
   explains the live report's `recurred_exact: 0` — seeded matches are excluded
   from scoring, so the oracle never sees them.
3. **`build_accept_seeds.py` (T1's file) will CRASH on re-run**: it reads the live
   (now 158-entry) holdout but asserts `EXPECT_HOLDOUT_DROPS = 73`
   (tools/ghidra/build_accept_seeds.py:52,94) → "expected 73, got 85". Fail-safe
   direction (crashes rather than leaks), but it must learn: drops=85 AND exclude
   `known_negatives.json`.
4. Minor shape deviation: reserved entries carry `xenon_addr/wii_addr_bank8/
   wii_symbol/tu/stem/category/match_types` but no `source` field (the brief's
   "same fields" reading); provenance lives in the file-level note. Not load-bearing.

## For the next agent

- T3 is CONFIRMED. Trust: grown holdout (158), `known_negatives.json` (3),
  `reserved_seed_candidates_round2.json` (11, unconsumed), the eval's exact-addr +
  known-negatives modes (default-off, byte-identical replay proven), and the
  `build_xenon_seeds.py` anti-leak fixes.
- Round-3 MUST: rebuild ACCEPT seeds AFTER the growth — fix
  `build_accept_seeds.py` to (a) expect 85 holdout drops (or derive it), (b)
  exclude `known_negatives.json` pairs. Until then, `seeds_accept_run3.json`
  contains 12 holdout addrs + 3 known-wrong pairs (eval-mitigated, file-level real).
- The true two-pass numbers are in the LIVE `eval_report.json` (10:38): holdout
  recovery 85/103 eligible-scored, precision 0.825; **VTCombinedReference judged
  precision 0.222** (54 judged, 12 correct) — the VT-rescue hypothesis looks
  REFUTED at first read, but that judgment belongs to T1's verifier.
- T3's impl-doc two-pass table (both rows) is superseded by the live eval; the
  146-row legacy numbers and all invariants remain valid.

## Exact commands used (reproducible)

```bash
python3 tools/ghidra/test_eval_xenon_matches.py                     # 44/44 OK
python3 tools/ghidra/grow_xenon_holdout.py --dry-run                # split summary
python3 tools/ghidra/grow_xenon_holdout.py                          # idempotency (md5 stable)
python3 tools/ghidra/build_xenon_seeds.py --out-dir /tmp/claude/xenon-seeds-verify
ARCH=build/SZBE69_B8/ghidra/ghidriff-xenon/run3-archive/json/bank8_target.elf-42264e.gzf-rb3_xenon_default_xex.gzf.ghidriff.matches.json
python3 tools/ghidra/eval_xenon_matches.py --matches $ARCH \
  --seeds build/SZBE69_B8/ghidra/xenon-seeds/seeds.json \
  --holdout /tmp/claude/holdout146.json \
  --no-known-negatives --credit-platform-alias --stratify   # byte-identical to run3-archive/eval_report.json
# grown-holdout evals: same but --holdout build/.../xenon-seeds/holdout.json,
# --seeds {seeds.json | seeds_accept_run3.json}
```
