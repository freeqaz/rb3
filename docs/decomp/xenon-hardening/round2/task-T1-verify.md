# Task T1 — ADVERSARIAL VERIFICATION (Fable): two-pass VT rescue

Verifier: Fable. Date: 2026-06-11 (~11:00 UTC). Subject: T1 implementer claims in
`task-T1-impl.md` + `task-T1-twopass.md` (commits ghidriff `e52d935`, rb3
`e1693918`, `b1ced3c3`).

## VERDICT: CONFIRMED

Every load-bearing claim re-verified independently — commits, code diffs, archive
integrity, seed invariants, run-log provenance, matches.json contents, and the
full eval (raw + alias + sweep) reproduced to the digit. The REFUTED verdict on
the VT-rescue hypothesis stands. Two minor non-refuting caveats below.

## What I checked (all PASS)

### 1. Commits exist, on the right branches, diffs match claims
- ghidriff `e52d935` = HEAD of `rb3-improvements` (`git branch --contains` →
  rb3-improvements). Diff touches ONLY `ghidriff/__main__.py` (+30/−9) and
  `ghidriff/ghidra_diff_engine.py` (+32/−1); no test files. All six scout-§10
  items present plus the 5b guard fix
  (`ghidra_diff_engine.py:2172`: `and not write_matches` added to the
  dump_pdiff_to_path early-out) — I read the original guard in the diff context;
  without 5b the matches-only path would indeed `return` before the
  write_matches block. The implementer's "scout missed this" claim is correct
  and load-bearing.
- rb3 `e1693918` (runner knobs + `tools/ghidra/build_accept_seeds.py`) and
  `b1ced3c3` (docs) both on `master` (`git branch --contains`). Runner diff:
  `SEEDS="${RB3_XENON_SEEDS:-…/seeds.json}"` + `MATCHES_ONLY_FLAG` appended only
  when set; `OUT_DIR`/`PROJ_DIR` untouched (constraint 2 honored).
- `--help` shows `--matches-only` (ran `ghidriff-venv/bin/python -m ghidriff
  --help`). Default `store_true, default=False`; full-report path preserved
  under `if not getattr(args,'matches_only',False)` in `__main__.py`.

### 2. Run-3 archive intact
- `run3-archive/vetted_identities.json` md5 =
  `dbc440b6b2b67b964b208a7c17af625e` (exact match, verified post-run).
- `run3-archive/json/` holds both the 209MB full json and the run-3
  matches.json; `run3-archive/eval_report.json` loads and carries the run-3
  baselines I used below.

### 3. Seed file invariants (re-computed from disk, not trusted)
`build/SZBE69_B8/ghidra/xenon-seeds/seeds_accept_run3.json`:
- 2,130 pairs; unique p1 = unique p2 = 2,130 (strict 1:1 holds).
- All p1 `0x80…` (Wii Bank 8), all p2 `0x82…` (Xenon) — orientation correct.
- Archived ACCEPT tier = 2,207, 0 null wii_addr; ACCEPT ∩ original-146 holdout
  = 73 (matches the planner's ground-truth numbers). 2,207 − 73 − 4 (1:1 dedup)
  = 2,130 ✓.
- **seeds ∩ original-146 holdout = 0** (anti-leak held).
- **seeds ∩ T3's 12 grown holdout entries = 12** — the cross-task leak is real
  and exactly as T1 reported (grown entries identified by
  `source=='judged-round2-correct'`, addr field `addr` not `xenon_addr`).
- `git check-ignore` confirms the seeds JSON is gitignored; only the script is
  committed.

### 4. The run is genuinely ACCEPT-only-seeded, matches-only
From `ghidriff.log` (run-4 section, 2026-06-11):
- `10:27:38 seed_matches: …/seeds_accept_run3.json` + `Loaded 2130 seed match
  pairs` + `10:28:13 SeedMatch: pre-accepted 2130 seed pairs, skipped 0
  unresolvable`. (The brief said "≈2,207" — 2,130 is the correct post-anti-leak
  post-1:1 count; fully accounted.)
- `bsim: True` (BSim ON, the only changed variable is the seed set).
- `10:35:37 --matches-only: skipping post-match diff/report stage` →
  `Wrote …matches.json (size: 4234K)`. Matching wall-clock: seed-accept
  10:28:13 → write 10:35:37 = **7m24s ≈ 7.4 min** as claimed; well under the
  45-min kill gate.
- Per-correlator: BSim Match Set = 3,964; `VTCombinedReference: seeded session
  with 6353 accepted` → `9912 candidates -> accepted 1193`; Implied `9 new
  matches`. All match the doc's numbers.

### 5. Output artifacts
- `json/…matches.json` mtime 2026-06-11 10:35, 4,234,456 B, **7,555**
  function_matches; first-type counts: SeedMatch 2130 / BSIM 3964 / VT 1193 /
  StringsRefs 214 / ExactInstr 25 / StrUnique 15 / Implied 9 / ExactMnemonics 4
  / SwitchSig 1. VT entries: 1,193, **0 missing `scores.VTCombinedReference.product`**;
  addrs bare-hex (first 500 checked).
- Full `…ghidriff.json` (209MB) mtime **2026-06-10 20:48** — NOT rewritten;
  `.md` mtime 2026-06-10 20:48 — NOT rewritten. Early-exit behaved exactly as
  designed.
- Live `vetted_identities.json` ACCEPT tier = **2,246** (claimed +39 vs 2,207 ✓).

### 6. Eval reproduced to the digit (same flags as baseline)
Re-ran `eval_xenon_matches.py --run-dir … --seeds …/seeds_accept_run3.json`:
- **alias** (`--credit-platform-alias --stratify`): OVERALL 0.323 (406 judged),
  BSIM 0.279, **VT 54 judged, 12 correct → 0.222**, ExactInstr 0.967 (29/30),
  Implied 0.500, StringsRefs 0.000. Holdout: eligible 146 (excluded as seeds:
  12), recovered 85 correct / 18 wrong / 38 unmatched → **rate 0.603, precision
  0.825**. Per-stratum VT: band3 0.154, system 0.321, network 0.000, sdk 0.000.
- **raw** (no alias flag): **VT 5/54 → 0.093**; OVERALL 0.291.
- All identical to the doc's table. The holdout subset is leak-free: the 12
  excluded-as-seeds entries are exactly T3's 12 grown entries, so the eligible
  set == the original 146 — the 0.603/0.825 vs run-3's 0.638/0.833 comparison
  is apples-to-apples.
- **Baseline integrity**: `run3-archive/eval_report.json` confirms run-3 inputs
  (seeds.json, same holdout/bindiff/wii_map) and numbers: totals 8,527; holdout
  0.638/0.833; BSIM alias 0.319; **VT alias 0.236 (13/55)**; raw 0.109 is in the
  round-1 record §7 (line 273). Same eval flags both sides modulo the
  mandatory seeds-path difference. Comparison legit.

### 7. VT sweep reproduced; assigned range confirmed inert
- VT `scores.product` distribution in the new matches.json: **min 20.3 /
  median 85.4 / max 805.7** — the assigned `9.5:14:0.5` range is entirely below
  min and culls nothing, exactly as T1 reported.
- Re-ran `--sweep-vt-score 20:820:40`: table matches the doc row-for-row
  (none/20→0.222@54; 60→0.324@37; 100→0.556@9; 140→0.500@8; 180→0.400@5;
  220→0.667@3; 260→1.000@1; ≥300→0 judged). **No operating point ≥0.85 at
  meaningful yield** — the 1.000 at floor 260 is n=1. The "sample exhaustion,
  not signal" reading is sound.

### 8. Test-failure claim
Ran the three JVM-free test files in the ghidriff venv: **45 passed, 2 failed
in 0.15s** — the same two `test_score_export.py` replay tests. Verified
nature: `test_replay_existing_matches_json_has_no_scores_field` asserts the
LIVE run-dir matches.json has no `scores` field — false since run 3 (2026-06-10)
added score export; `e52d935` touches no test file and no score-export code.
Genuinely pre-existing stale-artifact failures, independent of T1's edits.

## Verdict on the hypothesis (concur)

REFUTED, and the conclusion is properly drawn: with a ~0.93-precision
2,130-pair seed graph, VT still lands at 0.093 raw / 0.222 alias (vs run-3
0.109/0.236 from the contaminated graph) — the seed-purity lever moved VT by
≈−0.014, i.e. nothing. VT's weakness is intrinsic to the MWCC→MSVC reference
graph, not seed contamination. Demote-to-CAUTION-permanently is the right call;
the round-3 "seeds+exacts only" fallback is correctly ruled out (0.22 ≪ 0.4
trigger).

## Non-refuting caveats (2)

1. **`build_accept_seeds.py` is no longer cleanly re-runnable**: it reads the
   LIVE `xenon-seeds/holdout.json` (line 46) and hard-asserts
   `EXPECT_HOLDOUT_DROPS = 73` (line 52/94). After T3 grew holdout 146→158, a
   re-run would see 85 drops and **fail the assert loudly** (safe-conservative,
   no silent divergence — but the impl doc's "regenerable via
   build_accept_seeds.py" now needs the original-146 holdout or an updated
   expectation). The on-disk seeds file was built against the original 146
   (proven by seeds∩orig-146 = 0 and count 2,130) — no defect in the artifact.
2. **`eval_report.json` is a last-writer-wins artifact**: my verification
   re-runs overwrote it; the final state on disk is the alias-credited +
   sweep version (matching the doc's description). Anyone re-running eval with
   different flags will change it — compare against `run3-archive/` copies only.

## For the next agent
- T4's invariant audit must account for the verified 12-pair
  seeds∩grown-holdout overlap (T3-introduced; neutralized in eval by
  seed-exclusion **only when** `--seeds seeds_accept_run3.json` is passed).
- Run-dir state as of this verification: matches.json = run-4 (ACCEPT seeds),
  vetted_identities.json = run-4 re-vet (ACCEPT 2,246), eval_report.json =
  alias+sweep on run-4. Run-3 originals all in `run3-archive/` (md5-verified).
- Reproduction commands are in `task-T1-twopass.md` "For the verifier" — they
  work as written; numbers reproduce exactly.
