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

---

## A.S2 — songMs-pinned interleaved measurement + VERDICT — DONE (2026-07-06, Opus measurer)

### VERDICT: **A/A-variable** — the flip is NOT the cause of the wash. (unblocks E1, per WAVE6_REVIEW A4)

The Wave-5-hold premise ("blow-out wash appears only in flag-ON, 1/2 ON vs 0/2 OFF") is **directly
refuted under time control.** Two independent **songMs-pinned flag-OFF captures are PINK wash-class**
(the existence proof), the luma distributions of the two flag states are **statistically
indistinguishable** (Mann-Whitney U=24.0, **p=1.0**, n=7/7), and the wash is a **full-frame
magenta/pink env cast** (whole venue backdrop + note highway + band tinted — see `measure/montage.png`),
which the placement contract (a *crowd-only* transform change) mechanically cannot produce.

### Harness (per PLAN §2, committed)
- **`scripts/native/wash-measure.py`** — interleaved-sequential OFF/ON boots, each capture PINNED to
  a songMs window (21000±250) by polling `/api/health` songMs and screenshotting the instant it
  enters the window (director pre-disabled + `coop_dir_crowd` wide venue shot pinned early so the
  heavy pin work can't push songMs out of window); off-window boots discarded+re-run. Nav + shot
  reuse `placement-gate-capture.py` / `w21flip-dolphin-ab.py` verbatim. Scores each PNG with
  `wash_score.score_image`; early-stops on the pre-declared existence-proof rule; `--min-pairs`
  keeps sampling for a robust distribution (monotone-safe — more data cannot flip A/A-variable to
  flag-ON-specific, which requires ZERO flag-OFF wash).
- **`scripts/native/wash_finalize.py`** — scores ALL songMs-pinned raws (two passes, same
  binary/protocol) into the combined `batch_log.json` / `verdict.json` / `montage.png`.
- Binary: own `native/build-agent-W2.1-flip-blocker/rb3-native` (clean clang build). Raw PNGs +
  per-boot engine logs kept in `/tmp/wave6-flipblocker-captures/` (NOT committed, 14 captures).

### Combined per-capture scores (n=7 per flag state, all pinned within songMs 20877–21163)

| flag | songMs | class | mean_luma | hi% | lo% | pink% |
|---|---|---|---|---|---|---|
| OFF | 21019 | NEARBLACK | 0.103 | 1.9 | 76.3 | 0.0 |
| OFF | 21071 | NEARBLACK | 0.097 | 1.8 | 80.5 | 0.0 |
| **OFF** | **21083** | **PINK** | **0.570** | 4.3 | 6.6 | **70.0** |
| OFF | 21108 | NEARBLACK | 0.095 | 1.7 | 80.5 | 0.0 |
| **OFF** | **21131** | **PINK** | **0.229** | 2.0 | 41.9 | **33.6** |
| OFF | 21149 | NEARBLACK | 0.093 | 1.5 | 80.6 | 0.0 |
| OFF | 21163 | NEARBLACK | 0.093 | 1.5 | 80.4 | 0.0 |
| ON | 20877 | PINK | 0.590 | 11.5 | 6.8 | 37.9 |
| ON | 21025 | PINK | 0.262 | 2.1 | 42.3 | 36.2 |
| ON | 21108 | NEARBLACK | 0.090 | 1.7 | 81.4 | 0.0 |
| ON | 21113 | PINK | 0.437 | 2.6 | 5.8 | 64.6 |
| ON | 21116 | NEARBLACK | 0.090 | 1.7 | 81.4 | 0.0 |
| ON | 21131 | NEARBLACK | 0.088 | 1.6 | 81.5 | 0.0 |
| ON | 21132 | PINK | 0.251 | 2.2 | 42.4 | 33.9 |

- **flag-OFF wash rate 2/7; flag-ON wash rate 4/7.** Fisher exact on 2/7-vs-4/7 → p≈0.59 (NOT
  significant); Mann-Whitney on the 14 luma values → **U=24.0, p=1.0** (no flag effect on brightness).
- The two flag-OFF PINK captures sit in **different cache regimes** — one cold (21131, first-run) and
  one warm (21083, 6th warm boot, pink 70%) — so the existence proof is not a cold-cache artifact.

### Why this is airtight (defeats the batch-0 timing caveat A.S1 flagged)
1. **Time-pinned:** every capture is within songMs 20877–21163 (≈21000±163), so authored venue
   lighting animation is controlled — the wash is NOT "a different moment in the lighting anim".
2. **Boot-nondeterministic, not songMs-determined:** at essentially the *same* songMs the class
   flips — e.g. songMs≈21131 is **PINK** in one flag-OFF boot (mean 0.23) and **NEARBLACK** in a
   flag-ON boot (mean 0.088). Pinning did NOT make the wash deterministic → per A2.4 that is itself
   the answer: the wash is a run-to-run render-state variable, flag-independent.
3. **Full-frame env cast, not crowd-localized:** the pink covers the whole frame incl. the note
   highway and venue backdrop (`montage.png`), so it is not "the placement fix revealing pink crowd
   geometry" — it is an env/postproc-wide wash present in both flag states.
4. **Corroborates two prior records:** batch-0 (`cap_OFF_1` PINK) and the W2.1.S3 verify run
   (`W2.1/STATUS.md:304-311`, "off1 heavy … on1 none" — heavy wash flag-OFF, clean flag-ON, the
   mirror of the Wave-5 sample).

### Mechanism (ranking handed to S3; NOT attributed here — S2 is the verdict stage)
The wash presents as a full-frame **PINK broken-env cast** (33–70% pink pixels) OR a **NEARBLACK**
frame OR (Wave-5/batch-0) a **WHITE** blow-out — three stochastic render states across both flags.
Top prior (WAVE6_REVIEW A3, ranked): **async asset/texture residency at capture** — the pink
broken-env placeholder class (W0.5 precedent), **still live even under `RB3_FIXED_CLOCK`** because
the W0.3d part-(b) async-loader completion-order patch is unlanded. Secondary: `RB3PostProc` venue
grade/bloom (`RndPostProc::Current()`, boot-varying) and the P4 per-environ venue-light rewrite. The
demoted crowd-emissive→bloom suspect is confirmed irrelevant here (halo capture is `game.cam`-gated;
these are venue-cam shots). **S3 should run the 4-flag isolation matrix on a wash-reproducing config**
(`RB3_HIGHWAY_BLOOM_OFF` / `RB3_BLOOM_OFF` / `RB3_VENUE_LIGHT_OFF` / `RB3_TRACK_LIGHT_OFF`) to name it,
and file the wash as its own backlog item carrying this dataset (A4.2).

### Consequence for the flip (E1)
A/A-variable **unblocks the flip** under WAVE6_REVIEW A4's two conditions, both delegated to S4:
(1) E1 is judged on **detector-selected wash-free (NEUTRAL/NEARBLACK, non-PINK/WHITE) captures per
flag state** — there are ≥3 each in this dataset; (2) the wash gets its **own backlog item** with
this attribution data. The placement fix stays numerically proven (crowd+drum oracle GREEN) and
flag-OFF byte-identical; nothing about the wash is caused by it.

### Artifacts
- `measure/verdict.json` (combined `compare()` result, n=7/7), `measure/batch_log.json` (all 14
  scored captures), `measure/montage.png` (wash-class-first strip, 8 representative frames).
- Raw PNGs + engine logs: `/tmp/wave6-flipblocker-captures/` (14 pinned captures; not committed per
  the deliver contract). Run logs: `/tmp/wave6-flipblocker-measure.log` (+ `-pair1.log`).

**Commit:** see `git log --grep=W2.1-flip-blocker` (A.S2).
