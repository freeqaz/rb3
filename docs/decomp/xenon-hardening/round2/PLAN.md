# Round 2 PLAN — verification + exploitation of the run-3 win (2026-06-11)

Planner: Fable. Mission: (1) two-pass VT rescue, (2) gated rb3-xenon ingest of vetted
ACCEPT identities, (3) holdout growth from judged pairs. Judging of the 30-pair band3
evidence sample is already running script-side (NOT planned here); its verdicts gate
tasks T2–T4 via runner injection.

Read-first context:
- Round-1 session record: `docs/decomp/xenon-precision-hardening-2026-06-10.md`
  (esp. §7 RUN 3 RESULTS + Open follow-ups)
- Scouts: `scout-rb3xenon-conventions.md` (S1), `scout-evidence-substrate.md` (S2),
  `scout-report-cost.md` (S3) — all in this directory.

## State of the world (measured)

- Run 3: 8,527 matches; holdout recall 63.8% @ 0.833; ACCEPT tier 2,207 @ ~0.93
  (BSim sim×conf≥15 = 0.933 on holdout). VT collapsed: 0.109 raw / 0.236 alias-credited —
  hypothesis: VT propagated from the full 7,426-match seeded graph contaminated with
  ~0.5-precision sub-threshold BSim matches.
- Run-3 post-match report stage = 107 min pure waste for our pipeline (84.8 min O(n×m)
  dedup + 22 min esym; matches.json complete at minute 8.4). S3 specced a `--matches-only`
  early-exit (6 precise insertion points, §10 checklist).
- 30 self-contained evidence packs exist (`evidence/pair-01..30.md`); judging in flight.

## Planner-stage actions ALREADY DONE (this session, 2026-06-11)

1. **Run-3 artifacts archived** to
   `build/SZBE69_B8/ghidra/ghidriff-xenon/run3-archive/`
   (`json/` incl. the 209MB full json + 4.9MB matches.json, `ghidriff.log`,
   `eval_report.json`, `vetted_identities.json`, the run-3 `.md` report).
   md5 of vetted_identities.json verified identical (`dbc440b6…`).
   **Rationale:** the two-pass run overwrites `json/*.matches.json` (same p1/p2 names).
   Archiving now (planner) instead of inside T1 removes the T1↔T2 file race — T2 reads
   the immutable `run3-archive/` copies and is therefore independent of T1.
2. **ACCEPT-seed numbers grounded** (from live vetted_identities.json, identical to archive):
   ACCEPT = 2,207; null wii_addr = **0** (post-T5-fix held); ACCEPT∩holdout (by xenon addr)
   = **73** (47 BSIM + 24 ExactInstr + 2 Implied) — these MUST be dropped from the new
   seeds (anti-leak); expected ACCEPT-only seed count ≈ **2,134** before 1:1 dedup
   (exactly 1 duplicated p1 and 1 duplicated p2 exist → ~2,131–2,133 after dropping
   ambiguous pairs).
3. **Eval-tool mechanics verified** (`tools/ghidra/eval_xenon_matches.py`):
   reads ONLY `json/*.matches.json` (`find_matches_json`, line 716) — compatible with a
   matches-only run; takes `--seeds` (default = the OLD `xenon-seeds/seeds.json`) — the
   two-pass eval **must pass `--seeds <accept-seeds path>`** or the seed-exclusion math
   (holdout eligibility, new-coverage) is wrong. It also `import`s from
   `build_xenon_seeds.py` — which T3 edits — hence T3 is serialized after T1.
4. **build_xenon_seeds.py holdout mechanics verified**: its `--holdout` default is the
   rb3-xenon crossval source (`crossval_agree.json`, key `agree_fns`, line 82+727), NOT
   `xenon-seeds/holdout.json` (key `entries`) — so growing holdout.json does NOT
   automatically exclude new entries from future seed regeneration, and a re-run would
   CLOBBER grown holdout.json (line 776 writes only the source entries). T3 must fix both.

## Task DAG (4 tasks)

```
T1 (opus, ungated)  two-pass VT rescue: --matches-only + ACCEPT seeds + THE RUN + eval
T2 (sonnet, GATED)  rb3-xenon ingest of vetted ACCEPTs (reads run3-archive/ — independent of T1)
T3 (opus,  GATED, after T1)  holdout growth + known-negatives + anti-leak seed plumbing
T4 (sonnet, GATED, after T1+T2+T3)  round-2 session record + cross-task invariant audit
```

T1 is the ONLY task allowed to start ghidriff (constraint 2). T2 has no file overlap
with T1 and can run as soon as judge verdicts land. T3 must wait for T1 because it edits
`eval_xenon_matches.py` / `build_xenon_seeds.py` (T1 executes the former, which imports
the latter) and rewrites `xenon-seeds/holdout.json` (T1 reads it for anti-leak filtering).

## T1 — Two-pass VT rescue (the experiment)

Question: does seeding the whole cascade from ONLY the vetted ACCEPT tier (~0.93
precision, 2,207 pairs) rescue VTCombinedReference from 0.236 alias-credited?

Design decisions:
- **BSim stays ON** (default): the variable under test is the seed set, everything else
  matches run 3. BSim re-derives its own matches over the smaller residual pool.
- **Original holdout stays excluded from seeds** (73 ACCEPT entries dropped) so holdout
  recall/precision remain readable and leak-free.
- **Matching-only run** (S3's flag): expected ~9–10 min; hard kill at 45 min of matching.
  The report stage is pure waste (S3 §1) and skipping it mutates no state the next run
  needs (S3 §6).
- Runner gets `RB3_XENON_SEEDS` (seeds path override) + `RB3_XENON_MATCHES_ONLY` env
  knobs; defaults unchanged (full report, canonical seeds) so nothing regresses for
  other users.

Success reading (alias-credited, `--credit-platform-alias`):
- VT judged precision ≥0.5 = hypothesis confirmed directionally; an offline
  `--sweep-vt-score` operating point with judged precision ≥0.85 at nonzero yield = VT
  becomes an ACCEPT-tier feeder (run-3 Open follow-up #1 resolved).
- VT stays ≲0.3 = hypothesis refuted → VT is demoted permanently to CAUTION-feeder;
  document and stop investing in VT.
Either way the answer is decision-grade because the meter (eval + holdout) is unchanged.

## T2 — rb3-xenon ingest (GATED on judging)

Implements S1's design verbatim (it is complete): new gitignored
`rb3-xenon/ghidriff_identities.json` with `wii_addr_bank8` field naming (Bank-5/Bank-8
fail-fast), `wii_symbol` (CW-mangled) + `wii_symbol_demangled` (from matches.json
`p1_name`, joined bare-hex), fn_resolver tier T4b between `fuzzy_pairs` and
`bindiff_dc3`, calibrated confidences (BSIM≥15→0.93, ExactInstr→0.94,
Implied/SwitchSig→0.90, SymbolsHash→0.95).

The GATE (runner injects measured judge precision over the 21 BSIM-stratum pairs):
- **≥0.85** → ingest all non-seed ACCEPT (sdk excluded, judged-WRONG excluded): ~985.
- **0.70–0.85** → only BSim simconf≥20 + ExactInstr/SwitchSig/Implied/SymbolsHash.
- **<0.70** → NO ingest; write blocker doc with the measured number and per-stratum
  breakdown.
- Always: `category != "sdk"`; judged-WRONG xenon addrs excluded regardless of tier;
  SeedMatch-only entries omitted (already in target_symbol_map via T4 round 1).

Sources are the **immutable `run3-archive/` copies** — T2 never reads the live run dir
(T1 overwrites it).

## T3 — Holdout growth + known-negatives (GATED, after T1)

Judged-CORRECT pairs are human-grade ground truth. Anti-leak discipline: each pair goes
to holdout XOR reserved-for-future-seeds — deterministic 50/50 split (sort by
xenon_addr, `Random(42)` shuffle, first half → holdout). Judged-WRONG pairs become the
first known-negatives file.

Mechanics this task must fix (verified, see planner-stage §4):
- `xenon-seeds/holdout.json` grows additively (original 146 kept; new entries carry
  `wii_addr_bank8`/`wii_symbol` so eval can score them by EXACT address, stronger than
  the TU-stem heuristic).
- `build_xenon_seeds.py` must (a) also exclude grown-holdout addrs from seeds and
  (b) stop clobbering grown entries when it rewrites holdout.json (union-merge).
- `eval_xenon_matches.py` gains exact-addr holdout verdicts (when the field is present)
  + a known-negatives oracle (exact (p1,p2) recurrence of a judged-WRONG pair counts
  wrong; a different p1 for that p2 does NOT).
- Reserved-for-seeds list saved as `xenon-seeds/reserved_seed_candidates_round2.json`,
  asserted disjoint from holdout; NOT ingested into seeds this round.

## T4 — Session record + invariant audit (GATED, after all)

Collate T1–T3 docs + judge verdicts into a round-2 session record (sibling of the
round-1 record), with honest verdicts per task and re-checked invariants:
seeds∩holdout=∅ (every seeds file), reserved∩holdout=∅, ingest excluded sdk + WRONG,
archive intact, commits present on the right branches.

## Rejected alternatives (and why)

1. **Planning a judging task** — explicitly out of scope; already running script-side.
2. **VT-only second pass (BSim OFF)** — changes two variables at once; BSim ON keeps the
   run-3 environment so the seed-set effect is isolated. Also BSim ACCEPTs are the
   pipeline's main yield; a BSim-off run would produce no exploitable artifact.
3. **Fixing the O(n×m) dedup loop algorithmically** (hash-join would cut 84.8 min →
   seconds) — more invasive in upstream-shared code; `--matches-only` sidesteps it
   entirely for our pipeline and is additive/default-off. The algorithmic fix remains a
   good future upstream PR, noted for the ghidriff PR series.
4. **Extending `unified_id_rb3wii.json` for the ingest** — Bank-5 address space; S1
   showed silent corruption risk in `fn_resolver._t6_rb3wii` consumers. New file +
   `wii_addr_bank8` field name instead.
5. **Ingesting CAUTION tier / sdk entries** — sdk measured 0.000; CAUTION ~0.5. Round-3
   material at best (vet band3 CAUTION with the evidence-pack method first).
6. **Promoting ALL judged-correct pairs to holdout** — would leave no clean
   reserved-for-seeds pool; a pair used as a seed and as holdout simultaneously
   invalidates recall numbers (leak). 50/50 XOR split instead.
7. **Re-running vet/judging on system/network strata this round** — S2's sample is
   band3-only by design; whole-tier extrapolation is flagged in T4's record instead of
   pretending coverage we don't have.
8. **Fresh ghidriff project / changed paths** — hours of re-analysis; constraint 2
   forbids it; the analyzed project at `ghidriff-xenon/proj` is reused as-is.

## Numbers the tasks should treat as ground truth

| Quantity | Value | Source |
|---|---|---|
| ACCEPT tier | 2,207 (0 null wii_addr) | run3-archive/vetted_identities.json |
| ACCEPT∩holdout (drop from seeds) | 73 (47 BSIM / 24 ExactInstr / 2 Implied) | planner check |
| Expected ACCEPT-only seeds | ~2,134 (−1 dup p1, −1 dup p2 → ~2,131–2,133 1:1) | planner check |
| Run-3 VT judged precision | 0.109 raw / 0.236 alias | round-1 record §7 |
| BSim sim×conf≥15 holdout precision | 0.933 (n=45 kept) | round-1 record §7 |
| Matching time (run 3) | 8.4 min; report stage 107 min | scout-report-cost.md §1 |
| band3 ACCEPT non-seed population | 309 (280 BSIM/25 Exact/3 Switch/1 Implied) | S2 manifest |
| Original holdout | 146 entries, Xenon addr + TU stem only | xenon-seeds/holdout.json |

## Risks

- **T1 run risk:** seeded SeedMatch acceptance of 2,134 pairs shrinks the residual pool;
  BSim/VT behavior at this seeding level is untested. Mitigation: matching-only (cheap),
  45-min kill, archived baseline for full rollback of comparisons.
- **~7% of ACCEPT seeds are wrong** (0.93 precision) — the experiment measures whether
  VT tolerates that. If VT lands mid (0.4–0.6), the next lever is seeding from
  seeds+exacts only (~1,280 pairs ≈0.99) — noted in T1 doc template as round-3 option.
- **Judge sample is band3-only** (n=30, 21 BSIM): the T2 gate extrapolates band3 BSIM
  precision to system/network ACCEPTs. Mitigated by the BSim simconf≥15 holdout number
  (0.933) being stratum-independent; T4 must state the extrapolation explicitly.
- **Concurrent agents may move branches** — every task: `git branch --show-current`
  immediately before each commit; if unexpected, commit anyway and report the branch.

---

## STATUS (appended 2026-06-11 by T4 audit)

**Round-2 execution summary:**

| Task | Status | Notes |
|---|---|---|
| T1 (two-pass VT rescue) | NOT RUN | No run-4; matches.json still 2026-06-10 20:48; VT hypothesis untested |
| T2 (rb3-xenon ingest) | NOT RUN | Gate passes (BSim 0.905 ≥ 0.85); ghidriff_identities.json not created |
| T3 (holdout growth) | NOT RUN | reserved_seed_candidates_round2.json not created |
| T4 (session record) | COMPLETE | Session record at docs/decomp/xenon-hardening-round2-2026-06-11.md |

**Headline measurement from injected judge verdicts (30-pair stratified band3 sample):**
- Overall precision: **0.900** (27/30)
- BSim stratum: **0.905** (19/21) — validates holdout calibration 0.933; gate ≥0.85 PASSES
- Non-BSim stratum: **0.889** (8/9) — ExactInstr 5/5, SwitchSig 2/3, Implied 1/1
- Wrong pairs: 13 (BSIM, sibling aliasing), 16 (BSIM, float/int sibling), 29 (SwitchSig, string refutes)

**Key invariant checks (T4 probes):**
- seeds.json (1213) ∩ holdout.json (146) = **0** (PASS)
- run3-archive vetted_identities.json md5 = **dbc440b6b2b67b964b208a7c17af625e** (PASS)
- Round-1 commits (53f7a6aa, 4c9541e0, 2e8a82f7, 5b9cc4e, 31a6f6c, etc.) on expected branches (PASS)

**Round-3 priority actions:**
1. Run T1 (VT experiment) — 9–10 min with `--matches-only`; key open question
2. Run T2 (rb3-xenon ingest) — gate already passes on judge data
3. Run T3 (holdout growth, known-negatives) — 27 correct + 3 wrong pairs ready
4. System/network stratum judged sample — closes band3-extrapolation caveat

### STATUS ADDENDUM (T4 verifier, 2026-06-11 ~10:40 UTC)

The table above was a stale snapshot: T1/T2/T3 executed concurrently around the T4
record's commit (10:27:14). Verified state:

| Task | Actual status | Evidence |
|---|---|---|
| T1 | run-4 IN FLIGHT (started 10:27:30, `--matches-only`, 2,130 ACCEPT seeds loaded) | log `SeedMatch: pre-accepted 2130`; ps cmdline; rb3 `e1693918`; ghidriff `e52d935` |
| T2 | COMPLETE — 978 entries ingested, 0 sdk, 0 judged-wrong, gitignored | rb3 `6a4779b2`/`6793c59a`; rb3-xenon `7bdae6c`; task-T2-ingest.md |
| T3 | COMPLETE — holdout 146→158, known_negatives (3), reserved (11) | rb3 `4daa00fa`; task-T3-impl.md |
| T4 | record committed (`0d3afded`) but its task-status sections are superseded | task-T4-verify.md |

Judge numbers + invariants (a,d,e) in the record verified correct; (b)/(c) re-checked
non-vacuously: PASS. One accounted artifact: `seeds_accept_run3.json` ∩ grown holdout
= 12 addrs (seeds built against the original 146 before T3 grew it) — neutralized by
eval's seed-exclusion (eval_xenon_matches.py:443) **iff** eval gets
`--seeds seeds_accept_run3.json`. Full audit: `round2/task-T4-verify.md`.
