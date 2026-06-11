# Task T3 — Holdout growth from judged verdicts + known-negatives + anti-leak seed plumbing

Status: **DONE** (offline; all verification green). 2026-06-11.
Author: T3 (opus). Handoff for T4 (session record) + the verifier.

## TL;DR

Grew the Wii<->Xenon eval holdout from the 30-pair round-2 human judge (27 correct,
3 wrong) under strict anti-leak (XOR) discipline, recorded the 3 judged-WRONG pairs as
the first known-negatives, and fixed the two planner-verified plumbing defects in
`build_xenon_seeds.py` plus the two eval enhancements in `eval_xenon_matches.py`. All
new behaviour is default-off / provably no-op on legacy inputs — proven byte-identical
against the run-3 archive report.

- Holdout: **146 → 158** (+12 round-2 judged-correct, exact-addr-scored).
- Reserved-for-seeds pool: **11** (the other half of the split; NOT ingested this round).
- Known-negatives: **3** (judged-WRONG pairs 13/16/29).
- 4 judged-correct pairs (04/23/25/26) were **already in the crossval 146 holdout** →
  excluded from the split entirely (can't be re-added or reserved).

## Dependency note (T1 was MISSING — adapted)

T1 had **not run** when T3 executed: the live `json/*.matches.json` is byte-identical to
`run3-archive/` (md5 `045a0ae0…`), and `seeds_accept_run3.json` does not exist. T3's
deliverables (grow script, eval/seeds plumbing, the 3 round-2 JSON files, tests) are
independent of T1's run and are complete + verified. The one thing that genuinely needs
T1's output — the **two-pass eval delta** — is reported below against the run-3 (=live)
matches with a synthetic ACCEPT-seeds proxy; re-run trivially once T1 lands
`seeds_accept_run3.json` (command given).

## Inputs (ground truth used)

- Judge verdicts (27 correct / 3 wrong / 0 uncertain; precision 0.900): injected by the
  runner, persisted to `docs/decomp/xenon-hardening/round2/forensics/judge_verdicts.json`
  (the on-disk file wins over the in-script `INJECTED_VERDICTS`).
- Pair addrs/symbols/match types: `forensics/structured_pairs.json` (Xenon+Bank-8 addrs,
  match_type) joined to `vetted_identities.json` ACCEPT entries by Xenon addr for
  `tu`/`category`/`match_types`/`wii_addr` (Bank-8).
- Original holdout: `build/SZBE69_B8/ghidra/xenon-seeds/holdout.json` (146 crossval
  entries; `addr` = `0x`+UPPERCASE hex, `stem` = TU minus `.o`).

## (1) The XOR split — exact membership

Rule: judged-CORRECT pairs whose Xenon addr is **not already in the crossval 146**, sorted
by numeric `xenon_addr`, `random.Random(42).shuffle`, first `ceil(n/2)` → holdout, rest →
reserved. n = 23 eligible (27 correct − 4 already-in-holdout) → 12 holdout / 11 reserved.

**Holdout (12)** pair_ids: `02 03 05 08 12 14 21 22 24 27 28 30`
**Reserved-for-seeds (11)** pair_ids: `01 06 07 09 10 11 15 17 18 19 20`
**Known-negatives (3)** pair_ids: `13 16 29`
**Already-in-original-holdout (4, dropped from split)** pair_ids: `04 23 25 26`

(See `grow_xenon_holdout.py --dry-run` for the live summary; the pair_id→Wii-symbol map is
in `structured_pairs.json`.)

Holdout entry shape (the 12 grown entries appended after the 146):
```json
{"addr": "0x8252C728", "stem": "MusicLibrary", "wii_addr_bank8": "0x80300e10",
 "wii_symbol": "DifficultySortPart__12MusicLibraryCFv", "source": "judged-round2-correct"}
```
Known-negative entry shape:
```json
{"xenon_addr": "0x82518DE0", "wii_addr_bank8": "0x80246770",
 "wii_symbol": "clear__Q211stlpmtx_std120_List_base<...>Fv",
 "match_types": ["BSIM"], "source": "judged-round2-wrong"}
```

## (2) `tools/ghidra/grow_xenon_holdout.py` (NEW)

Single source of truth for the growth. Deterministic + idempotent. Key functions:
- `build_rows(...)` (line ~157): does the split; the anti-leak filter
  `orig_holdout_addrs` drops judged-correct pairs already in the crossval holdout
  (`already_in_holdout`), asserts holdout ∩ reserved = ∅.
- `merge_holdout(existing, new_rows)` (line ~250): UNION-MERGE by Xenon addr — original
  146 preserved byte-equivalent, grown entries appended/replaced-in-place (never
  duplicated → idempotent). Updates the top-level `note`.
- `main()`: writes `holdout.json` (union-merged), `reserved_seed_candidates_round2.json`,
  `known_negatives.json`. Asserts reserved ∩ crossval = ∅ and new-holdout ∩ crossval = ∅.

Idempotency proven: holdout.json md5 `d6e9b91a359dfcd5711db29a3b532b76` identical across
3 consecutive runs.

## (3) `tools/ghidra/build_xenon_seeds.py` (FIXED — both planner-verified defects)

- New `_read_holdout_entries(path)` / `_holdout_p2_addrs(entries)` helpers (line ~568)
  accept BOTH shapes: crossval `agree_fns` (the `--holdout` source) and grown `entries`
  (the eval holdout.json). Bare list tolerated.
- New CLI: `--extra-holdout` (default = `xenon-seeds/holdout.json`) whose p2 addrs are
  **also excluded from seeds**; `--no-extra-holdout` to disable. `--reserved`
  (default = `reserved_seed_candidates_round2.json`) for the disjointness assertion;
  `--no-reserved` to skip. Removed the duplicate `DEFAULT_OUT_DIR`.
- Holdout-exclusion block (line ~792): unions crossval-p2 ∪ extra-holdout-p2 before
  dropping seeds. **Reserved invariant**: asserts the reserved pool does NOT overlap the
  holdout exclusion set (future seeds must not be dropped) — i.e. reserved addrs stay
  eligible as seeds, AND are disjoint from holdout.
- Seed invariant (line ~847): `assert not seed_p2_set & all_holdout_p2` — no seed leaks
  into either holdout.
- holdout.json write (line ~868): **UNION-MERGE instead of clobber** — re-emits the
  crossval 146 and PRESERVES every existing non-crossval (= grown round-2) entry. Stats
  `holdout_written_entries` / `holdout_grown_preserved` added.

## (4) `tools/ghidra/eval_xenon_matches.py` (EXTENDED — both behaviours, default-off)

(a) **Exact-addr holdout scoring** — metric (a), line ~510. When a holdout entry carries
`wii_addr_bank8`, correctness = the matched Wii `p1` equals that exact Bank-8 address
(stronger than TU-stem: a stem-correct match to a *different* Game.o function is correctly
WRONG). Always scoreable. Tagged `"score_mode": "exact"` on the row. The original 146 (no
field) keep the stem path and emit **no** `score_mode` key → byte-identical.

(b) **Known-negatives oracle** — new `known_negatives` param + metric (b'), line ~681.
A scored match recurring an EXACT `(p2,p1)` judged-WRONG pair counts WRONG in
`precision_by_match_type` (fed via `judged`) and is listed under
`lists.known_negative_recurrences` + a `known_negatives` report section. The SAME p2
matched to a DIFFERENT p1 is **not** penalized (may be the true identity). CLI:
`--known-negatives PATH` (auto-loads `known_negatives.json` if present),
`--no-known-negatives` to disable. Absent file/empty list → no section, no list key →
byte-identical.

8 new tests in `test_eval_xenon_matches.py` (`TestExactAddrHoldout` ×4,
`TestKnownNegatives` ×3, `TestNoRegressionReplay` ×1).

## Verification (all offline, all green)

### pytest — 44/44
```
python3 tools/ghidra/test_eval_xenon_matches.py   # Ran 44 tests ... OK
```

### Invariant asserts (in-script + measured)
- holdout-new ∩ reserved = ∅ ✓; holdout-new ∩ crossval-146 = ∅ ✓; reserved ∩ crossval-146
  = ∅ ✓; reserved ∩ known-neg = ∅ ✓; holdout-new ∩ known-neg = ∅ ✓.
- Original 146 preserved **byte-equivalent** (prefix-equal, same order) ✓.
- Grow idempotent (md5 identical across 3 runs) ✓.

### Eval no-regression (byte-identical replay on archived inputs)
New eval code, run-3 archived matches, **original 146-only holdout**,
`--no-known-negatives --credit-platform-alias --stratify`, vs
`run3-archive/eval_report.json` (minus `inputs` paths):
```
byte-identical (sans inputs): True
```
(Command: `eval_xenon_matches.py --matches <run3-archive matches> --seeds <seeds.json>
--holdout /tmp/.../holdout.orig.json --no-known-negatives --credit-platform-alias --stratify`.)

### temp-dir seed build (spec step 5)
`build_xenon_seeds.py --out-dir /tmp/claude/xenon-seeds-test` (extra-holdout = the REAL
grown holdout):
- temp seeds (1213) ∩ grown-holdout p2 = **∅** ✓
- reserved ∩ grown-holdout = ∅; reserved ∩ temp-seed-p2 = 0 (reserved NOT excluded) ✓
- temp holdout.json ⊇ crossval-146 ✓
Pre-seeding the temp dir with the grown holdout then re-building → union-merge keeps
**146+12 = 158**, original 146 byte-equiv, all 12 grown preserved as
`judged-round2-correct`, temp-seeds ∩ temp-holdout = ∅ ✓.

### Two-pass eval (adapted — T1 missing)
Run-3 (=live) matches + grown 158 holdout + known-negatives:
| seeds | eligible | excluded_as_seeds | recovered_correct | recovered_wrong | precision_on_recovered |
|---|---|---|---|---|---|
| run-3 `seeds.json` (1213) | 158 | 0 | 102 | 18 | 0.850 |
| ACCEPT-seeds (synthetic, 2203, proxy for T1's `seeds_accept_run3.json`) | 73 | 85 | 24 | 13 | 0.649 |

- All **12 exact-mode holdout rows** scored `recovered_correct` with matched `wii_addr` ==
  `wii_addr_bank8` (e.g. `0x8252c728`→`0x80300e10`).
- **3/3 known-negative pairs recur** in run-3 matches (the vetting wrongly accepted them) →
  all counted WRONG.

### Leak-nuance verified (the critical one)
The 12 grown-holdout pairs ARE ACCEPT seeds in a T1-style run. With ACCEPT seeds,
`excluded_as_seeds` rose **0 → 85**, decomposing **exactly** as **73 crossval + 12 grown**
(73 = the planner's `ACCEPT∩holdout`). All 12 grown exact-mode rows **disappeared from
eligibility** (excluded as seeds → not scored → their trivially-1.0 seeded "recovery" is
NOT counted). They become *real* holdout only for future runs that exclude them from seeds
— which the `build_xenon_seeds.py --extra-holdout` fix now guarantees (temp seeds ∩
grown-holdout = ∅, proven above).

To reproduce the true two-pass delta once T1 lands `seeds_accept_run3.json`:
```bash
python3 tools/ghidra/eval_xenon_matches.py \
  --run-dir build/SZBE69_B8/ghidra/ghidriff-xenon \
  --seeds build/SZBE69_B8/ghidra/xenon-seeds/seeds_accept_run3.json \
  --credit-platform-alias --stratify
```

## Files

Modified (tracked): `tools/ghidra/build_xenon_seeds.py`,
`tools/ghidra/eval_xenon_matches.py`, `tools/ghidra/test_eval_xenon_matches.py`.
New (tracked): `tools/ghidra/grow_xenon_holdout.py`,
`docs/decomp/xenon-hardening/round2/forensics/judge_verdicts.json`, this doc.
New (gitignored, under build/): `xenon-seeds/{holdout.json (158), known_negatives.json (3),
reserved_seed_candidates_round2.json (11)}` — regenerable via `grow_xenon_holdout.py`.

Real `seeds.json` / `seeds_detail.json` **NOT regenerated** (T1 had not consumed them; all
seed-build verification used `--out-dir /tmp/...`). They remain the run-3 artifacts
(mtime 18:31, md5 `94c645cf…`). Regenerate them safely any time via
`python3 tools/ghidra/build_xenon_seeds.py` — the union-merge preserves the grown holdout.

## Caveats

1. **Two-pass delta is a proxy** until T1 produces `seeds_accept_run3.json`. The synthetic
   ACCEPT-seeds run (2203 pairs) exercises the same seed-exclusion code path and matches
   the planner's 73-crossval number exactly, so the mechanism is verified; only the
   *absolute* recall/precision numbers will shift with T1's exact seed set.
2. The grow split's "already-in-holdout" filter depends on the pre-existing holdout.json.
   In a fresh out-dir (no 146) all 27 correct flow through the split (14/13) — correct for
   that input state, but the canonical artifact is the real out-dir (12/11/4).
3. Sample is **band3 ACCEPT non-seed only** (S2's scope) — the 12 new holdout entries do
   not represent system/network/sdk strata. T4 should state this.
4. Reserved pool is **not ingested** anywhere this round (no seeds file consumes it); it is
   the future-seed candidate set. The 11 entries are recorded for a round-3 seed build.

## For the verifier

- Re-run everything: `python3 tools/ghidra/test_eval_xenon_matches.py` (44 OK);
  `python3 tools/ghidra/grow_xenon_holdout.py --dry-run` (summary, writes nothing);
  the byte-identical replay command above.
- The 3 known-negatives are pairs 13/16/29; the judge's evidence for each is in
  `judge_verdicts.json` `per_pair[].evidence`.
- Anti-leak crux: confirm `build_xenon_seeds.py --out-dir /tmp/x` then
  `set(seeds p2) & set(holdout addrs) == ∅`. Confirm holdout.json union-merge keeps the
  grown 12 across a `build_xenon_seeds.py` run (temp2 test, reproducible).

## For the next agent (T4)

- Holdout is now **158** (146 crossval + 12 round-2 exact-scored); known-negatives = **3**;
  reserved = **11** (unconsumed). Invariants for the cross-task audit:
  seeds∩holdout=∅ (every seeds file), reserved∩holdout=∅, reserved NOT excluded from seeds.
- The judge precision over the band3 ACCEPT sample is **27/30 = 0.900** (bsim 19/21 = 0.905,
  non-bsim 8/9 = 0.889). This is the number the T2 gate keyed on.
- Sample bias to report: band3 ACCEPT non-seed only; no system/network/sdk coverage.
