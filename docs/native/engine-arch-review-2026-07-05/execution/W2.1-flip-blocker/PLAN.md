# W2.1-flip-blocker — PLAN (Wave 6, Lane A lead)

**Author:** Opus planner (A.S1). **Status:** plan + detector delivered; S2/S3/S4 scoped below.
**Goal:** characterize the flag-ON "blow-out wash" that made the coordinator HOLD the
`RB3_PLACEMENT_CONTRACT` flip at the Wave-5 E1 gate, and decide numerically whether it is
**A/A-variable** (pre-existing, flag-independent → flip may proceed on wash-free captures) or
**flag-ON-specific** (caused by the flip → must be fixed before flip).

Grounded in `WAVE6_REVIEW.md` A1 (protocol power), A2 (detector well-posedness), A3 (suspect
demoted), A4 (A/A-variable ships with conditions). The Wave-5 draft's prime suspect
(crowd-emissive → bloom) is **mechanically impossible** for the A/B shot: the halo capture is
`game.cam`-gated (`Rnd_Wgpu_RB3.cpp:4403`) and the A/B pins a *venue* cam — so S3 starts from the
ranked alternatives, not that hypothesis.

---

## 0. Deliverables of this stage (A.S1) — DONE

1. **`scripts/native/wash_score.py`** — numeric wash detector (per A2): continuous mean-luma +
   BOTH tails (`hi_frac` blow-out, `lo_frac` near-black) + `pink_frac` hue channel that separates
   the *pink broken-env* class from the *white exposure/bloom* class. `classify()` buckets each
   image into `PINK | WHITE | NEARBLACK | NEUTRAL`; `compare()` runs the S2 decision (Mann-Whitney
   U on the luma distributions + the pre-declared rule). `--selftest` validates on synthetic images
   (8 checks / 14 assertions, all green). Import-friendly for the S2 harness.
2. **Batch-0 scoring** of the committed Wave-5 captures (below) — the free prior evidence per A1.3.
3. **This PLAN** — the S2 measurement protocol + the pre-declared decision rule.

---

## 1. Preliminary evidence (Batch 0 — the free prior, per WAVE6_REVIEW A1.3)

The detector was validated against the recorded Wave-5 luma values (95.2 / 23.7 / 202.5 / 122.6 →
detector mean×255 = 95.1 / 23.7 / 202.6 / 122.6, exact) then run over the four committed
`W2.1-flip/dolphin-ab/cap_*.png`. Raw JSON: `batch0_scores.json`, `batch0_compare.json`.

| capture | flag | mean_luma | hi% (>0.90) | lo% (<0.10) | pink% | **class** |
|---|---|---|---|---|---|---|
| cap_OFF_1 | OFF | 0.373 | 2.47 | 30.85 | **41.43** | **PINK** |
| cap_OFF_2 | OFF | 0.093 | 1.01 | **77.46** | 0.49 | NEARBLACK |
| cap_ON_1  | ON  | 0.794 | **77.14** | 3.41 | 1.79 | WHITE |
| cap_ON_2  | ON  | 0.481 | 1.10 | 3.82 | **75.49** | PINK |

**`compare()` verdict on batch 0: `A/A-variable`.** Reason: a **PINK wash-class capture is present
in flag-OFF (cap_OFF_1, pink 41.4%)** — an existence proof that the wash class occurs without the
flag. And the two flag states are not luma-separated (Mann-Whitney U=0, p=0.33 at n=2/2 — expected
noise at this N).

**Two distinct phenomena confirmed (A2's hypothesis):**
- **PINK (broken-env / missing-texture class):** appears in BOTH flag states — cap_OFF_1 (41%) and
  cap_ON_2 (75%). Flag-independent.
- **WHITE (exposure/bloom haze):** appears only in cap_ON_1 (hi 77%) in this batch — but n=1, and
  the mirror record below shows it flag-OFF too.
- **NEARBLACK:** cap_OFF_2 (lo 77%) — the "anomalous dark tail" A1 flagged.

**Corroborating prior record (WAVE6_REVIEW A1 citation).** `W2.1/STATUS.md` §"Gameplay visual A/B"
(lines ~304–311, the S3-verify run) recorded the wash as **"off1 heavy / off2 moderate; on1 none /
on2 moderate"** — i.e. **heavy wash in flag-OFF and a clean flag-ON**, the *mirror* of the Wave-5
package sample. `W2.1-flip/STATUS.md:152-159` records the same luma spread and concludes the wash
"swings … across boots **regardless of flag state**". The asymmetry that motivated the Wave-5 hold
is therefore already contradicted by two independent prior experiments *plus* batch 0.

**Caveat that S2 must close.** Batch 0 was captured on wall-clock settle sleeps, so each frame is at
a *different, unrecorded songMs* (`w21flip-dolphin-ab.py:~108-138`). Authored venue lighting moves
luma across songMs, so batch 0 cannot separate "wash" from "different moment in the lighting
animation". S2 re-measures with songMs-pinned captures to confirm the pink wash survives time
control. Batch 0 is strong *preliminary* evidence, not the verdict.

---

## 2. S2 — measurement protocol (Opus; the verdict stage)

**Design: interleaved sequential, songMs-pinned, continuous-scored (WAVE6_REVIEW A1+A2).**

### 2.1 Harness
Extend the proven nav (`placement-gate-capture.py` / `w21flip-dolphin-ab.py` `nav_to_gameplay`,
verbatim) into a new `scripts/native/wash-measure.py`. Per boot:
1. Boot `native/build-agent-W2.1-flip-blocker/rb3-native` headless (`RB3_HTTP=1
   RB3_FIXED_CLOCK=1`), flag state per the interleave schedule (see 2.2).
2. Nav splash → main_hub → song_select → part/diff → gameplay; autohit so the song plays.
3. **Pin the capture to a songMs window, do NOT sleep wall-clock:** poll `/api/health`
   (`k.health(port)[1]` = songMs) every ~30 ms; the moment `SONGMS_LO ≤ songMs ≤ SONGMS_HI`,
   `rb3_director_disable 1` + `rb3_force_shot <wide venue shot>` (same candidate list as the S1/S2
   harness) and screenshot via `/api/screenshot`. **Target window: songMs 21000 ± 250** (matches
   the Wave-4/5 captures; A2.1). Record the actual songMs achieved with each capture.
   - If a boot overshoots the window before the shot pins (load-latency jitter), it is **discarded
     and re-run**, not scored at the wrong songMs — an off-window frame is exactly the confound A2
     removes.
4. Score the PNG with `wash_score.score_image()`; append `{flag, songMs, metrics}` to the batch log
   and write the PNG to `W2.1-flip-blocker/measure/<flag>_<n>_<songMs>.png`.

### 2.2 Interleave + early-stop (A1.2)
- Capture in **alternating OFF/ON pairs**: OFF,ON,OFF,ON,… (never all-OFF-then-all-ON — that would
  confound flag with drift/thermal/asset-warmup).
- **flag-OFF arm** = no env (current committed default). **flag-ON arm** = `RB3_PLACEMENT_CONTRACT=1`.
  (These are the *pre-flip* semantics; A5 notes they invert post-flip — not this stage's concern.)
- After each pair, run `wash_score.compare(off_scores, on_scores)` and apply the decision rule (2.3).
- **Early-stop** when the rule fires a decision; **cap N=16 per state** (32 boots) if it never does.

### 2.3 Decision rule (PRE-DECLARED — numeric, no post-hoc)
Encoded in `wash_score.compare()`; verdict ∈ {`A/A-variable`, `flag-ON-specific`, `inconclusive`}:

1. **`A/A-variable`** — a **wash-class capture (`class ∈ {PINK, WHITE}`) appears in ANY flag-OFF
   boot**. This is an existence proof: the wash occurs without the flag. Decided immediately (the
   early-stop existence-proof branch). *Batch 0 already satisfies this, but S2 must reproduce it
   with a songMs-pinned flag-OFF wash to defeat the timing caveat.*
2. **`flag-ON-specific`** — NO flag-OFF boot is wash-class, AND all wash mass is in flag-ON, AND the
   luma distributions differ by **Mann-Whitney U two-sided p < 0.05**. Only then is the flip
   implicated.
3. **`inconclusive`** — neither; keep sampling to N=16/state, then report `inconclusive` (defaults
   to *do not flip* — treat like flag-ON-specific for safety, escalate to coordinator).

**Wash-class thresholds** (in `wash_score.py`, tuned off the batch-0 separation, wide margins):
`pink_frac ≥ 15%` → PINK; `hi_frac ≥ 25%` or `mean ≥ 0.65` → WHITE; `lo_frac ≥ 65%` or
`mean ≤ 0.12` → NEARBLACK. NEARBLACK is reported but is **not** a "wash" for rule 1 (a dark venue
shot is legitimately near-black; only PINK/WHITE are anomalies that break the E1 sign-off).

### 2.4 S2 outputs (checkpoint before returning)
`measure/batch_log.json` (every capture's flag/songMs/metrics), `measure/verdict.json`
(`compare()` result at stop), the pinned PNGs, and a STATUS.md §S2 with the verdict + the
per-capture table. Write `/tmp/wave6-checkpoints/A-S2.json` before returning.

---

## 3. S3 — mechanism attribution (Opus; conditional)

**If S2 = flag-ON-specific:** root-cause and fix. Prime suspect is DEMOTED (A3 — halo capture is
`game.cam`-gated, unreachable for the venue-cam crowd draws). Start from the ranked alternatives via
a **4-flag isolation matrix** (A3 "cheap attribution beats statistics") — on a wash-reproducing
config, re-capture + `wash_score` under each of, singly:
`RB3_HIGHWAY_BLOOM_OFF=1` · `RB3_BLOOM_OFF=1` (postproc term) · `RB3_VENUE_LIGHT_OFF=1` ·
`RB3_TRACK_LIGHT_OFF=1`. Whichever flag drops the wash names the mechanism in ~8 captures. Ranked
priors: (1) capture-timing/songMs drift [should already be controlled by 2.3]; (2) async/texture
residency at capture — the W0.3d part-(b) async-completion-order patch is **still unlanded**, live
even under `RB3_FIXED_CLOCK`; (3) `RB3PostProc` venue grade/bloom (`RndPostProc::Current()`,
boot-varying); (4) P4 per-environ venue-light SceneUniforms rewrite. Any fix: **placement oracle
stays GREEN (vertex-invariance untouched), flag-OFF byte-identical, fail-red demonstrated**, and the
gem/now-bar halo regression gate = `RB3_HIGHWAY_BLOOM_BLEND=0 vs default` pixel-diff (A8).
**File fence:** a name-based render-policy fix would land in `rb3_render_hook.cpp` — coordinate with
the coordinator (that file is also Lane C's forbidden zone).

**If S2 = A/A-variable (expected):** S3 is a **wash-rate note, no code** — file the wash as its own
backlog item carrying S2's attribution data (flag matrix + songMs-pinned scores) so it isn't
orphaned by the flip (A4.2). Still run the isolation matrix once on a wash frame to *name* the
mechanism for that backlog item (cheap, high-value).

---

## 4. S4 — E1 sign-off package (Sonnet)

Refresh the Dolphin A/B package with **detector-selected wash-free captures per flag state** (A4.1)
so the confound cannot re-enter the human sign-off: for each flag state, present ≥2 captures the
detector scored `NEUTRAL`, plus the full per-capture score table (so the coordinator sees which
frames were wash-affected and why they were excluded). Include: the S2 verdict + `verdict.json`, the
crowd+drum placement oracle GREEN companion (unchanged), and the wash-backlog item. **Pre-flip
checklist additions (A5):** run one `RB3_PLACEMENT_CONTRACT=1 drawlog-golden.py --canonical-order
--fixed-clock` and record the measured count as the re-golden target (expected 792, but *measure*
it); note that the flip inverts every "no env"-OFF-arm harness (post-flip OFF = `_OFF=1`) and the
`classification.json` rows go stale. Coordinator does E1 sign-off + flip + re-golden.

---

## 5. Exit criteria

- **A.S1 (this):** detector `--selftest` green + batch-0 scored + protocol/decision-rule written +
  committed. ✅
- **S2:** verdict ∈ {A/A-variable, flag-ON-specific, inconclusive} with checkpointed per-capture
  scores and the Mann-Whitney result; the verdict is reached under songMs-pinned captures.
- **S3:** (A/A-variable) mechanism named + backlog item filed; (flag-ON-specific) fix landed
  behind a flag with fail-red + oracle-GREEN + halo-present gate.
- **S4:** package with wash-free selected captures → coordinator E1.

## 6. Files this stage touches (fence)
- `scripts/native/wash_score.py` (new).
- `docs/native/engine-arch-review-2026-07-05/execution/W2.1-flip-blocker/**` (PLAN.md, STATUS.md,
  batch0_scores.json, batch0_compare.json).
Nothing else. No engine edits, no default flip, no classification.json edit this stage.
