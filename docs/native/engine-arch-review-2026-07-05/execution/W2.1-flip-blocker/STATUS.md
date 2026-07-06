# W2.1-flip-blocker — STATUS

## A.S1 — Plan + detector + evidence-first scoring — DONE (2026-07-06, Opus planner)

**Delivered:**
- `scripts/native/wash_score.py` — numeric wash detector per WAVE6_REVIEW A2: continuous mean-luma
  + both tails (`hi_frac` blow-out >0.90, `lo_frac` near-black <0.10) + `pink_frac` hue channel
  separating the PINK broken-env class from the WHITE exposure/bloom class. `classify()` →
  `PINK|WHITE|NEARBLACK|NEUTRAL`; `compare()` runs the S2 decision (Mann-Whitney U on luma +
  pre-declared rule). Import-friendly for the S2 harness.
- `--selftest`: **PASSED** (8 synthetic scenes / 14 assertions green — gray→NEUTRAL, white→WHITE,
  black→NEARBLACK, magenta→PINK, dark-magenta value-gated OUT, split-image both tails,
  compare→A/A-variable on OFF-wash, compare→flag-ON-specific on separated all-ON wash).
- Detector validated against recorded Wave-5 luma (95.2/23.7/202.5/122.6 → 95.1/23.7/202.6/122.6, exact).
- `PLAN.md` — S2 songMs-pinned interleaved-sequential protocol + pre-declared numeric decision rule
  + S3 isolation-matrix + S4 package brief.

**Batch-0 result (free prior, the four committed `W2.1-flip/dolphin-ab/cap_*.png`):**
`batch0_scores.json` + `batch0_compare.json`.

| capture | flag | mean | hi% | lo% | pink% | class |
|---|---|---|---|---|---|---|
| cap_OFF_1 | OFF | 0.373 | 2.47 | 30.85 | **41.43** | **PINK** |
| cap_OFF_2 | OFF | 0.093 | 1.01 | 77.46 | 0.49 | NEARBLACK |
| cap_ON_1  | ON  | 0.794 | 77.14 | 3.41 | 1.79 | WHITE |
| cap_ON_2  | ON  | 0.481 | 1.10 | 3.82 | **75.49** | PINK |

**`compare()` verdict: `A/A-variable`** — a PINK wash-class capture is present in **flag-OFF**
(cap_OFF_1, pink 41%), an existence proof that the wash occurs without the flag. The PINK
(broken-env) class appears in BOTH flag states (OFF_1 41%, ON_2 75%); WHITE (bloom) in ON_1 only in
this batch; the mirror record `W2.1/STATUS.md:304-311` ("off1 heavy / off2 moderate; on1 none")
already showed heavy wash flag-OFF + clean flag-ON. The Wave-5-hold asymmetry premise is contradicted
by batch 0 + two prior experiments.

**Caveat for S2:** batch 0 was NOT songMs-pinned (wall-clock settle sleeps), so it cannot fully
separate wash from lighting-animation drift. S2 must reproduce the flag-OFF wash under a
songMs-pinned capture (window 21000±250) to make the verdict airtight. Batch 0 = strong preliminary
evidence, not the final verdict.

**Commit:** see `git log --grep=W2.1-flip-blocker`.
